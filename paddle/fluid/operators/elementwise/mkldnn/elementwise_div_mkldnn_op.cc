/* Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/fluid/operators/elementwise/mkldnn/elementwise_mkldnn_op.h"

namespace paddle {
namespace framework {
class ExecutionContext;
}  // namespace framework
namespace platform {
class CPUDeviceContext;
}  // namespace platform
}  // namespace paddle

namespace paddle {
namespace operators {
template <typename T>
class EltwiseDivMKLDNNGradKernel : public ElemwiseGradKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& ctx) const override {
    ElemwiseGradKernel<T>::Compute(ctx);

    auto& dev_ctx =
        ctx.template device_context<platform::MKLDNNDeviceContext>();
    const auto& mkldnn_engine = dev_ctx.GetEngine();

    auto* y = ctx.Input<framework::Tensor>("Y");
    auto* out = ctx.Input<framework::Tensor>("Out");
    auto* dout = ctx.Input<framework::Tensor>(framework::GradVarName("Out"));
    auto* dx = ctx.Output<framework::Tensor>(framework::GradVarName("X"));
    auto* dy = ctx.Output<framework::Tensor>(framework::GradVarName("Y"));
    int axis = ctx.Attr<int>("axis");

    auto& astream = platform::MKLDNNDeviceContext::tls().get_stream();

    if (dx) {
      // dx = dout / y

      platform::BinaryMKLDNNHandler<T> handler(
          dnnl::algorithm::binary_div, axis, mkldnn_engine, ctx.GetPlace(),
          dout, y, dx, 1.0f, 1.0f, 1.0f);

      const auto src_dout_memory = handler.AcquireSrcMemory(dout);
      const auto src_y_memory = handler.AcquireSecondSrcMemory(y);
      const auto dst_dx_memory = handler.AcquireDstMemory(dx);

      const auto binary_prim = handler.AcquireForwardPrimitive();

      const std::unordered_map<int, dnnl::memory> args = {
          {DNNL_ARG_SRC_0, *src_dout_memory},
          {DNNL_ARG_SRC_1, *src_y_memory},
          {DNNL_ARG_DST, *dst_dx_memory}};

      binary_prim->execute(astream, args);
      astream.wait();

      dx->set_layout(framework::DataLayout::kMKLDNN);
      dx->set_format(platform::GetMKLDNNFormat(*dst_dx_memory));
    }

    if (dy) {
      // dy = -dout * out / y

      platform::BinaryMKLDNNHandler<T> y_handler(
          dnnl::algorithm::binary_div, axis, mkldnn_engine, ctx.GetPlace(), y,
          y, nullptr, 1.0f, 1.0f, 1.0f);

      const auto y_memory = y_handler.AcquireSrcMemory(y);

      dnnl::post_ops po;
      po.append_binary(dnnl::algorithm::binary_div, y_memory->get_desc());

      platform::BinaryMKLDNNHandler<T> handler(
          dnnl::algorithm::binary_mul, axis, mkldnn_engine, ctx.GetPlace(),
          dout, out, nullptr, -1.0f, 1.0f, 1.0f, po);

      const auto src_dout_memory = handler.AcquireSrcMemory(dout);
      const auto src_out_memory = handler.AcquireSecondSrcMemory(out);

      // If broadcasting is in use then let's write to temporary
      // buffer allocated by oneDNN
      const auto dst_dy_memory = (dout->dims() == dy->dims())
                                     ? handler.AcquireDstMemory(dy)
                                     : handler.AcquireDstMemory();

      const auto binary_prim = handler.AcquireForwardPrimitive();

      const std::unordered_map<int, dnnl::memory> args = {
          {DNNL_ARG_SRC_0, *src_dout_memory},
          {DNNL_ARG_SRC_1, *src_out_memory},
          {DNNL_ARG_DST, *dst_dy_memory},
          {DNNL_ARG_ATTR_MULTIPLE_POST_OP(0) | DNNL_ARG_SRC_1, *y_memory}};

      binary_prim->execute(astream, args);
      astream.wait();

      dy->set_layout(framework::DataLayout::kMKLDNN);

      // Reduction is needed for broadcasting scenario
      if (dout->dims() != dy->dims()) {
        platform::ReductionMKLDNNHandler<T> handler_sum(
            dnnl::algorithm::reduction_sum, 0.0f, 0.0f, mkldnn_engine,
            ctx.GetPlace(), dout, dy, CalculateBroadcastedDims(dout, dy));
        auto dy_memory_p = handler_sum.AcquireDstMemory(dy);
        auto reduction_p = handler_sum.AcquireForwardPrimitive();

        // As source we use mem object with results from binary operation
        reduction_p->execute(astream, {{DNNL_ARG_SRC, *dst_dy_memory},
                                       {DNNL_ARG_DST, *dy_memory_p}});
        astream.wait();
        dy->set_format(
            platform::GetMKLDNNFormat(dy_memory_p->get_desc().reshape(
                phi::vectorize<int64_t>(dy->dims()))));

      } else {
        dy->set_format(platform::GetMKLDNNFormat(*dst_dy_memory));
      }
    }
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;

// TODO(piotrekobi) add int8, uint8 support
REGISTER_OP_KERNEL(elementwise_div, MKLDNN, paddle::platform::CPUPlace,
                   ops::EltwiseMKLDNNKernel<float, dnnl::algorithm::binary_div>,
                   ops::EltwiseMKLDNNKernel<paddle::platform::bfloat16,
                                            dnnl::algorithm::binary_div>)

REGISTER_OP_KERNEL(elementwise_div_grad, MKLDNN, paddle::platform::CPUPlace,
                   ops::EltwiseDivMKLDNNGradKernel<paddle::platform::bfloat16>,
                   ops::EltwiseDivMKLDNNGradKernel<float>)
