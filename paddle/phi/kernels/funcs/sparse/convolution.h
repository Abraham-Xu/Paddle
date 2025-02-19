/* Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

#include "paddle/phi/core/ddim.h"
#include "paddle/phi/kernels/funcs/blas/blas.h"

namespace phi {
namespace funcs {
namespace sparse {

struct Dims4D {
  int dims[4];
  Dims4D(const int batch, const int x, const int y, const int z) {
    dims[0] = batch;
    dims[1] = z;
    dims[2] = y;
    dims[3] = x;
  }
  HOSTDEVICE const int& operator[](int i) const { return dims[i]; }
};

// Judge whether the current position x is in (lower, upper)
inline HOSTDEVICE bool Check(const int& x,
                             const int& kx,
                             const int& pad,
                             const int& stride,
                             const int dilation,
                             const int kdim,
                             const int xdim) {
  const int lower = x - dilation * kx + pad;
  const int uper = x + (kdim - kx - 1) * dilation - pad;
  return (lower >= 0 && lower % stride == 0 && uper < xdim);
}

// Check whether the current position(x, y, z) is legal:
// Judge the minimum and maximum values at each latitude
inline HOSTDEVICE bool Check(const Dims4D& dims,
                             const Dims4D& kernel_dims,
                             const Dims4D& paddings,
                             const Dims4D& dilations,
                             const Dims4D& strides,
                             const int x,
                             const int y,
                             const int z,
                             const int kx,
                             const int ky,
                             const int kz) {
  bool x_valid = Check(
      x, kx, paddings[3], strides[3], dilations[3], kernel_dims[3], dims[3]);
  bool y_valid = Check(
      y, ky, paddings[2], strides[2], dilations[2], kernel_dims[2], dims[2]);
  bool z_valid = Check(
      z, kz, paddings[1], strides[1], dilations[1], kernel_dims[1], dims[1]);
  return (x_valid && y_valid && z_valid);
}

template <typename Dim>
inline HOSTDEVICE int PointToIndex(const int& batch,
                                   const int& x,
                                   const int& y,
                                   const int& z,
                                   const Dim& dims) {
  return batch * dims[1] * dims[2] * dims[3] + z * dims[2] * dims[3] +
         y * dims[3] + x;
}

// TODO(zhangkaihuo): use division and multiply to optimize
// modulo operation
template <typename Dim>
inline HOSTDEVICE void IndexToPoint(
    const int index, const Dim& dims, int* batch, int* x, int* y, int* z) {
  int n = index;
  *x = n % dims[3];
  n /= dims[3];
  *y = n % dims[2];
  n /= dims[2];
  *z = n % dims[1];
  n /= dims[1];
  *batch = n;
}

inline void GetOutShape(const DDim& x_dims,
                        const DDim& kernel_dims,
                        const std::vector<int>& paddings,
                        const std::vector<int>& dilations,
                        const std::vector<int>& strides,
                        DDim* out_dims) {
  PADDLE_ENFORCE_EQ(
      x_dims.size(),
      5,
      phi::errors::InvalidArgument("the shape of x should be (N, D, H, W, C)"));
  PADDLE_ENFORCE_EQ(kernel_dims.size(),
                    5,
                    phi::errors::InvalidArgument(
                        "the shape of kernel should be (D, H, W, C, OC)"));

  // infer out shape
  (*out_dims)[0] = x_dims[0];
  (*out_dims)[4] = kernel_dims[4];
  for (int i = 1; i < 4; i++) {
    (*out_dims)[i] = (x_dims[i] + 2 * paddings[i - 1] -
                      dilations[i - 1] * (kernel_dims[i - 1] - 1) - 1) /
                         strides[i - 1] +
                     1;
  }
}

inline void ResetSubmKernelSizeAndStrides(const DDim& kernel_dims,
                                          std::vector<int>* paddings,
                                          std::vector<int>* strides) {
  for (uint64_t i = 0; i < paddings->size(); i++) {
    (*paddings)[i] = kernel_dims[i] / 2;
    (*strides)[i] = 1;
  }
}

template <typename T, typename Context>
inline void SubmPreProcess(const Context& dev_ctx,
                           const SparseCooTensor& x,
                           const DenseTensor& kernel,
                           const SparseCooTensor& out_grad,
                           const int in_channels,
                           const int out_channels,
                           const int half_kernel_size,
                           DenseTensor* kernel_grad,
                           DenseTensor* x_grad) {
  auto blas = phi::funcs::GetBlas<Context, T>(dev_ctx);
  T* d_kernel_ptr = kernel_grad->data<T>();
  blas.GEMM(CblasTrans,
            CblasNoTrans,
            x.non_zero_elements().dims()[1],
            out_grad.non_zero_elements().dims()[1],
            x.non_zero_elements().dims()[0],
            static_cast<T>(1),
            x.non_zero_elements().data<T>(),
            out_grad.non_zero_elements().data<T>(),
            static_cast<T>(0),
            d_kernel_ptr + half_kernel_size * in_channels * out_channels);

  // call gemm: d_x = out_grad * transpose(kernel)
  // (n, out_channels) * (out_channels, in_channels)
  T* x_grad_ptr = x_grad->data<T>();
  blas.GEMM(CblasNoTrans,
            CblasTrans,
            out_grad.non_zero_elements().dims()[0],
            in_channels,
            out_grad.non_zero_elements().dims()[1],
            static_cast<T>(1),
            out_grad.non_zero_elements().data<T>(),
            kernel.data<T>() + half_kernel_size * in_channels * out_channels,
            static_cast<T>(0),
            x_grad_ptr);
}

}  // namespace sparse
}  // namespace funcs
}  // namespace phi
