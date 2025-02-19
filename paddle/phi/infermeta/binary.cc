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

#include "paddle/phi/infermeta/binary.h"

#include <algorithm>
#include <vector>
#include "paddle/phi/common/data_type.h"
#include "paddle/phi/core/ddim.h"
#include "paddle/phi/kernels/funcs/common_shape.h"

namespace phi {
namespace detail {

static void BinarySameInputDimsCheck(const MetaTensor& x,
                                     const MetaTensor& y,
                                     MetaConfig config) {
  auto input_dim = x.dims();
  auto other_dim = y.dims();
  PADDLE_ENFORCE_EQ(input_dim.size(),
                    other_dim.size(),
                    phi::errors::PreconditionNotMet(
                        "Input(Input) and Input(Other) must have the same "
                        "dimension size."));
  int n = input_dim.size();
  bool is_runtime = config.is_runtime;
  for (int i = 0; i < n; i++) {
    if (is_runtime) {
      PADDLE_ENFORCE_EQ(input_dim[i],
                        other_dim[i],
                        phi::errors::PreconditionNotMet(
                            "The value at dim %d of Input(Input) is not "
                            "equal to the Input(Other): %ld != %ld.",
                            i,
                            input_dim[i],
                            other_dim[i]));
    } else {
      if (!(input_dim[i] < 0 || other_dim[i] < 0)) {
        PADDLE_ENFORCE_EQ(input_dim[i],
                          other_dim[i],
                          phi::errors::PreconditionNotMet(
                              "The value at dim %d of Input(Input) is not "
                              "equal to the Input(Other): %ld != %ld.",
                              i,
                              input_dim[i],
                              other_dim[i]));
      }
    }
  }
}

}  // namespace detail

void AllValueCompareInferMeta(const MetaTensor& x,
                              const MetaTensor& y,
                              MetaTensor* out,
                              MetaConfig config) {
  detail::BinarySameInputDimsCheck(x, y, config);

  out->set_dims(phi::make_ddim({1}));
  out->set_dtype(DataType::BOOL);
}

void Atan2InferMeta(const MetaTensor& x, const MetaTensor& y, MetaTensor* out) {
  out->share_meta(x);
}

void BCELossInferMeta(const MetaTensor& input,
                      const MetaTensor& label,
                      MetaTensor* out,
                      MetaConfig config) {
  auto input_dims = input.dims();
  auto label_dims = label.dims();

  int rank = input_dims.size();
  PADDLE_ENFORCE_EQ(rank,
                    label_dims.size(),
                    phi::errors::InvalidArgument(
                        "Input(X) and Input(Label) shall have the same rank."
                        "But received: the rank of Input(X) is [%d], "
                        "the rank of Input(Label) is [%d].",
                        rank,
                        label_dims.size()));

  bool check = true;
  if ((!config.is_runtime) &&
      (phi::product(input_dims) <= 0 || phi::product(label_dims) <= 0)) {
    check = false;
  }

  if (check) {
    PADDLE_ENFORCE_EQ(input_dims,
                      label_dims,
                      phi::errors::InvalidArgument(
                          "Input(X) and Input(Label) shall have the same "
                          "shape. But received: the shape of Input(X) is "
                          "[%s], the shape of Input(Label) is [%s].",
                          input_dims,
                          label_dims));
  }

  out->set_dims(input_dims);
  out->set_dtype(input.dtype());
  out->share_lod(input);
}

void BincountInferMeta(const MetaTensor& x,
                       const paddle::optional<const MetaTensor&> weights,
                       int minlength,
                       MetaTensor* out) {
  auto input_dim = x.dims();

  PADDLE_ENFORCE_GE(minlength,
                    0,
                    phi::errors::InvalidArgument(
                        "The minlength should be greater than or equal to 0."
                        "But received minlength is %d",
                        minlength));

  PADDLE_ENFORCE_EQ(
      input_dim.size(),
      1,
      phi::errors::InvalidArgument("The 'shape' of Input(X) must be 1-D tensor."
                                   "But the dimension of Input(X) is [%d]",
                                   input_dim.size()));

  if (weights.is_initialized()) {
    auto weights_dim = weights->dims();
    PADDLE_ENFORCE_EQ(weights_dim.size(),
                      1,
                      phi::errors::InvalidArgument(
                          "The 'shape' of Input(Weights) must be 1-D tensor."
                          "But the dimension of Input(Weights) is [%d]",
                          weights_dim.size()));

    PADDLE_ENFORCE_EQ(
        weights_dim[0],
        input_dim[0],
        phi::errors::InvalidArgument(
            "The 'shape' of Input(Weights) must be equal to the 'shape' of "
            "Input(X)."
            "But received: the 'shape' of Input(Weights) is [%s],"
            "the 'shape' of Input(X) is [%s]",
            weights_dim,
            input_dim));
  }
  out->set_dims(phi::make_ddim({-1}));
  if (weights.is_initialized()) {
    out->set_dtype(weights->dtype());
  } else {
    out->set_dtype(x.dtype());
  }

  out->share_lod(x);
}

void CholeskySolveInferMeta(const MetaTensor& x,
                            const MetaTensor& y,
                            bool upper,
                            MetaTensor* out) {
  auto x_dims = x.dims();
  auto y_dims = y.dims();

  auto x_dims_n = x_dims.size();
  auto y_dims_n = y_dims.size();

  PADDLE_ENFORCE_GE(x_dims_n,
                    2,
                    phi::errors::InvalidArgument(
                        "the rank of input Y must greater or equal to 2"));
  PADDLE_ENFORCE_GE(y_dims_n,
                    2,
                    phi::errors::InvalidArgument(
                        "the rank of input X must greater or equal to 2"));
  PADDLE_ENFORCE_EQ(
      y_dims[y_dims_n - 1],
      y_dims[y_dims_n - 2],
      phi::errors::InvalidArgument("input Matrix Y should be square matrix,"
                                   "But Got last shape of %ld x %ld",
                                   y_dims[y_dims_n - 1],
                                   y_dims[y_dims_n - 2]));
  PADDLE_ENFORCE_EQ(
      x_dims[x_dims_n - 2],
      y_dims[y_dims_n - 2],
      phi::errors::InvalidArgument("the first dim of Matrix X must be equal to "
                                   "the fisrt dim of Matrix Y,"
                                   "But Got %ld and %ld",
                                   x_dims[x_dims_n - 2],
                                   y_dims[y_dims_n - 2]));

  std::vector<int64_t> x_dims_vec = phi::vectorize(x_dims);
  std::vector<int64_t> y_dims_vec = phi::vectorize(y_dims);

  std::vector<int64_t> x_dims_vec_cut(x_dims_vec.begin(), x_dims_vec.end() - 2);
  std::vector<int64_t> y_dims_vec_cut(y_dims_vec.begin(), y_dims_vec.end() - 2);

  std::vector<int64_t> expand_batch_portion =
      funcs::MatrixGetBroadcastBatchPortion(x_dims_vec_cut, y_dims_vec_cut);

  std::vector<int64_t> x_broadcast_dims({expand_batch_portion});
  x_broadcast_dims.insert(x_broadcast_dims.end(),
                          {x_dims_vec[x_dims_n - 2], x_dims_vec[x_dims_n - 1]});

  // dim of 'out' is the same with 'X' after broadcast
  out->set_dims(phi::make_ddim(x_broadcast_dims));
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
  out->share_lod(x);
}

void CompareInferMeta(const MetaTensor& x,
                      const MetaTensor& y,
                      int axis,
                      MetaTensor* out) {
  auto dim_x = x.dims();
  auto dim_y = y.dims();

  if (dim_x == dim_y) {
    out->share_meta(x);
  } else {
    int max_dim = std::max(dim_x.size(), dim_y.size());
    int axis = std::abs(dim_x.size() - dim_y.size());
    std::vector<int> x_dims_array(max_dim);
    std::vector<int> y_dims_array(max_dim);
    std::vector<int> out_dims_array(max_dim);
    funcs::GetBroadcastDimsArrays(dim_x,
                                  dim_y,
                                  x_dims_array.data(),
                                  y_dims_array.data(),
                                  out_dims_array.data(),
                                  max_dim,
                                  axis);

    out->set_dims(make_ddim(out_dims_array));
    out->share_lod(x);
  }

  out->set_dtype(DataType::BOOL);
}

void CompareAllInferMeta(const MetaTensor& x,
                         const MetaTensor& y,
                         MetaTensor* out) {
  auto dim_x = x.dims();
  auto dim_y = y.dims();
  PADDLE_ENFORCE_GE(
      dim_x.size(),
      dim_y.size(),
      errors::InvalidArgument(
          "The size of dim_y should not be greater than dim_x's."));
  out->share_lod(x);
  out->set_dims(make_ddim({1}));
  out->set_dtype(DataType::BOOL);
}

void CrossInferMeta(const MetaTensor& x,
                    const MetaTensor& y,
                    int axis,
                    MetaTensor* out) {
  auto x_dim = x.dims();
  auto y_dim = y.dims();
  auto dim = axis;

  bool dims_match = phi::funcs::CheckDims(x_dim, y_dim);
  PADDLE_ENFORCE_EQ(
      dims_match,
      true,
      phi::errors::InvalidArgument("The 'shape' of Input(X) should be equal to "
                                   "the 'shape' of Input(Y). But received "
                                   "Input(X).dimensions = [%s], "
                                   "Input(Y).dimensions = [%s]",
                                   x_dim,
                                   y_dim));

  if (dim != DDim::kMaxRank) {
    PADDLE_ENFORCE_EQ(
        dim < x_dim.size() && dim >= (0 - x_dim.size()),
        true,
        phi::errors::OutOfRange(
            "Attr(dim) is out of range, It's expected "
            "to be in range of [-%d, %d]. But received Attr(dim) = %d.",
            x_dim.size(),
            x_dim.size() - 1,
            dim));
    if (dim < 0) {
      dim += x_dim.size();
    }
    PADDLE_ENFORCE_EQ(x_dim[dim] == 3 && y_dim[dim] == 3,
                      true,
                      phi::errors::InvalidArgument(
                          "Input(X/Y).dims()[dim] should be equal to 3."
                          "But received Input(X/Y).dims()[dim] = %d.",
                          x_dim[dim]));
  }
  out->set_dims(x_dim);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
  out->share_lod(x);
}

void DistInferMeta(const MetaTensor& x,
                   const MetaTensor& y,
                   float p,
                   MetaTensor* out) {
  auto x_dims = x.dims();
  auto y_dims = y.dims();

  PADDLE_ENFORCE_NE(phi::product(x_dims),
                    0,
                    phi::errors::InvalidArgument(
                        "The Input(X) has not been initialized properly. The "
                        "shape of Input(X) = [%s].",
                        x_dims));
  PADDLE_ENFORCE_NE(phi::product(y_dims),
                    0,
                    phi::errors::InvalidArgument(
                        "The Input(Y) has not been initialized properly. The "
                        "shape of Input(Y) = [%s].",
                        y_dims));
  out->set_dims({1});
  out->set_dtype(x.dtype());
}

void DotInferMeta(const MetaTensor& x, const MetaTensor& y, MetaTensor* out) {
  auto x_dims = x.dims();
  auto x_rank = static_cast<size_t>(x_dims.size());
  PADDLE_ENFORCE_EQ(true,
                    1 == x_rank || 2 == x_rank,
                    phi::errors::PreconditionNotMet(
                        "ShapeError: The dimensions of input tensor X (%s) "
                        "should be 1 or 2",
                        x_dims.to_str()));

  auto y_dims = y.dims();
  PADDLE_ENFORCE_EQ(
      true,
      x_rank == static_cast<size_t>(y_dims.size()),
      phi::errors::PreconditionNotMet(
          "ShapeError: The shape of input tensor Y: %s should match with "
          "input tenosr X: %s",
          y_dims.to_str(),
          x_dims.to_str()));
  bool shape_match = true;
  for (size_t i = 0; i < x_rank; ++i) {
    if (x_dims[i] != y_dims[i]) {
      shape_match = false;
      break;
    }
  }

  PADDLE_ENFORCE_EQ(true,
                    shape_match,
                    phi::errors::PreconditionNotMet(
                        "ShapeError: The shape of input tensor X: %s should "
                        "be exactly the same "
                        "with input tensor Y: %s",
                        x_dims.to_str(),
                        y_dims.to_str()));

  x_dims[x_dims.size() - 1] = 1;
  out->set_dims(x_dims);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
}

void ElementwiseInferMeta(const MetaTensor& x,
                          const MetaTensor& y,
                          MetaTensor* out) {
  return ElementwiseRawInferMeta(x, y, -1, std::move(out));
}

void ElementwiseRawInferMeta(const MetaTensor& x,
                             const MetaTensor& y,
                             int axis,
                             MetaTensor* out) {
  if (x.dims() != y.dims()) {
    auto x_dims = x.dims();
    auto y_dims = y.dims();
    int max_dim = std::max(x_dims.size(), y_dims.size());
    if (x_dims.size() == y_dims.size()) {
      PADDLE_ENFORCE_EQ((axis == -1) || (axis == 0),
                        true,
                        phi::errors::InvalidArgument(
                            "axis should be -1 or 0 while the dimension of "
                            "tensor X (%s) is equal to the dimension of "
                            "tensor Y (%s), but received axis: %s",
                            x_dims.size(),
                            y_dims.size(),
                            axis));
    }
    PADDLE_ENFORCE_EQ((axis >= (-1 * max_dim)) && (axis < max_dim),
                      true,
                      phi::errors::InvalidArgument(
                          "The axis range must be [%s, %s), but axis is %s. "
                          "Please set the axis again.",
                          -1 * max_dim,
                          max_dim,
                          axis));
    axis = (axis < 0 ? (std::abs(x_dims.size() - y_dims.size()) + axis + 1)
                     : axis);
    std::vector<int> x_dims_array(max_dim);
    std::vector<int> y_dims_array(max_dim);
    std::vector<int> out_dims_array(max_dim);
    funcs::GetBroadcastDimsArrays(x_dims,
                                  y_dims,
                                  x_dims_array.data(),
                                  y_dims_array.data(),
                                  out_dims_array.data(),
                                  max_dim,
                                  axis);
    auto out_dims = phi::make_ddim(out_dims_array);
    out->set_dims(out_dims);
  } else {
    out->set_dims(x.dims());
  }

  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
  out->share_lod(x);
}

void GatherNdInferMeta(const MetaTensor& x,
                       const MetaTensor& index,
                       MetaTensor* out) {
  auto x_dims = x.dims();
  auto x_dims_size = x_dims.size();
  auto index_dims = index.dims();
  auto index_dims_size = index_dims.size();

  PADDLE_ENFORCE_LE(
      index_dims[index_dims_size - 1],
      x_dims_size,
      phi::errors::InvalidArgument(
          "Input(Index).shape[-1] should be no greater than Input(X).rank"));
  PADDLE_ENFORCE_GE(index_dims_size,
                    1UL,
                    phi::errors::InvalidArgument(
                        "The rank of Input(Index) should be greater than 1"));

  std::vector<int64_t> result_dims;
  // The result dims is
  //   Index.shape[:-1] + X.shape[Index.shape[-1]:]
  for (int i = 0; i < index_dims_size - 1; ++i) {
    result_dims.emplace_back(index_dims[i]);
  }
  for (int i = index_dims[index_dims_size - 1]; i < x_dims_size; ++i) {
    result_dims.emplace_back(x_dims[i]);
  }

  out->set_dims(phi::make_ddim(result_dims));
  out->share_lod(x);
  out->set_dtype(x.dtype());
}

void GatherTreeMeta(const MetaTensor& ids,
                    const MetaTensor& parents,
                    MetaTensor* out) {
  auto ids_dims = ids.dims();
  auto parents_dims = parents.dims();
  PADDLE_ENFORCE_EQ(ids_dims == parents_dims,
                    true,
                    phi::errors::InvalidArgument(
                        "The shape of Input(Parents) must be same with the "
                        "shape of Input(Ids)."));
  out->set_dims(ids_dims);
}

void HuberLossInferMeta(const MetaTensor& input,
                        const MetaTensor& label,
                        float delta,
                        MetaTensor* out,
                        MetaTensor* residual,
                        MetaConfig config) {
  auto input_dims = input.dims();
  auto label_dims = label.dims();

  PADDLE_ENFORCE_EQ(input_dims.size(),
                    label_dims.size(),
                    phi::errors::InvalidArgument(
                        "Input(input) rank and Input(label) rank should be "
                        "same, but received input rank(%d) != label rank(%d)",
                        input_dims.size(),
                        label_dims.size()));

  bool contain_unknown_dim = phi::contain_unknown_dim(input_dims) ||
                             phi::contain_unknown_dim(label_dims);
  if (config.is_runtime || !contain_unknown_dim) {
    PADDLE_ENFORCE_EQ(
        input_dims,
        label_dims,
        phi::errors::InvalidArgument(
            "The Input(input) and Input(label) should have the same "
            "shape, but received input shape [%s] != label shape [%s]",
            input_dims,
            label_dims));
  }

  auto out_dims = label_dims;
  residual->set_dims(out_dims);
  out->set_dims(out_dims);
  out->share_lod(input);
}

void IndexSampleInferMeta(const MetaTensor& x,
                          const MetaTensor& y,
                          MetaTensor* out,
                          MetaConfig config) {
  auto input_dims = x.dims();
  PADDLE_ENFORCE_EQ(input_dims.size(),
                    2,
                    errors::InvalidArgument(
                        "Inputs(X) shape of IndexSample op should be 2-D, but "
                        "got X's shape = [%s], please check X shape.",
                        input_dims));

  auto index_dims = y.dims();
  PADDLE_ENFORCE_EQ(
      index_dims.size(),
      2,
      errors::InvalidArgument(
          "Inputs(Index) shape of IndexSample op should be 2-D, but "
          "got Index's shape [%s] , please check index shape.",
          input_dims));
  if (config.is_runtime) {
    PADDLE_ENFORCE_EQ(input_dims[0],
                      index_dims[0],
                      errors::InvalidArgument(
                          "Inputs(X)'s value of dimension 0 must same with "
                          "Inputs(Index)'s value of dimension 0, but "
                          "got %d of Inputs(X), and got %d of Inputs(Index), "
                          "please check Inputs shape.",
                          input_dims[0],
                          index_dims[0]));
  }
  out->set_dtype(x.dtype());
  out->set_dims(index_dims);
  out->share_lod(y);
}

void LogLossInferMeta(const MetaTensor& input,
                      const MetaTensor& label,
                      float epsilon,
                      MetaTensor* out,
                      MetaConfig config) {
  auto pred_dims = input.dims();
  auto label_dims = label.dims();

  if (config.is_runtime ||
      (phi::product(pred_dims) > 0 && phi::product(label_dims) > 0)) {
    PADDLE_ENFORCE_EQ(
        pred_dims,
        label_dims,
        phi::errors::InvalidArgument(
            "The dimensions of Input(Predicted) must be equal to the"
            "dimensions of Input(Labels), but received dimensions of "
            "Input(Predicted)"
            "is [%s], received dimensions of Input(Labels) is [%s].",
            pred_dims,
            label_dims));
  }
  PADDLE_ENFORCE_EQ(pred_dims.size(),
                    2,
                    phi::errors::InvalidArgument(
                        "The dimensions of Input(Predicted) must be 2,"
                        "But received dimensions of Input(Predicted)"
                        "is [%d]",
                        pred_dims.size()));
  if (config.is_runtime) {
    PADDLE_ENFORCE_EQ(pred_dims[1],
                      1,
                      phi::errors::InvalidArgument(
                          "Each row of Input(Predicted) contains a real value, "
                          "so the 2nd dimension of Input(X) must be 1,"
                          "But got [%d]",
                          pred_dims[1]));
  }
  out->set_dims({pred_dims[0], 1});
  out->set_dtype(input.dtype());
  out->share_lod(input);
}

void MatmulInferMeta(const MetaTensor& x,
                     const MetaTensor& y,
                     bool trans_x,
                     bool trans_y,
                     MetaTensor* out) {
  std::vector<int64_t> dims_x = phi::vectorize(x.dims());
  std::vector<int64_t> dims_y = phi::vectorize(y.dims());
  auto ndims_x = dims_x.size();
  auto ndims_y = dims_y.size();
  PADDLE_ENFORCE_GT(ndims_x,
                    0UL,
                    phi::errors::InvalidArgument(
                        "The Input(x) dims size must be greater than 0,"
                        " but reviced dims size is 0. "));
  PADDLE_ENFORCE_GT(ndims_y,
                    0UL,
                    phi::errors::InvalidArgument(
                        "The Input(y) dims size must be greater than 0,"
                        " but reviced dims size is 0. "));

  bool x_broadcasted = false, y_broadcasted = false;
  if (ndims_x == 1) {
    dims_x.insert(dims_x.begin(), 1);
    ndims_x = 2;
    x_broadcasted = true;
  }

  if (ndims_y == 1) {
    dims_y.push_back(1);
    ndims_y = 2;
    y_broadcasted = true;
  }

  size_t M, N;
  if (trans_x) {
    M = dims_x[ndims_x - 1];
  } else {
    M = dims_x[ndims_x - 2];
  }
  if (trans_y) {
    N = dims_y[ndims_y - 2];
  } else {
    N = dims_y[ndims_y - 1];
  }

  std::vector<int64_t> new_dims;
  if (ndims_x > ndims_y) {
    new_dims.assign(dims_x.begin(), dims_x.end() - 2);
  } else if (ndims_x < ndims_y) {
    new_dims.assign(dims_y.begin(), dims_y.end() - 2);
  } else {
    new_dims.reserve(ndims_x);
    for (size_t i = 0; i < ndims_x - 2; ++i) {
      new_dims.push_back(std::max(dims_x[i], dims_y[i]));
    }
  }
  if (!x_broadcasted) {
    new_dims.push_back(M);
  }
  if (!y_broadcasted) {
    new_dims.push_back(N);
  }
  if (x_broadcasted && y_broadcasted) {
    new_dims.push_back(1);
  }

  auto ddim_out = phi::make_ddim(new_dims);

  out->set_dims(ddim_out);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
}

void MvInferMeta(const MetaTensor& x, const MetaTensor& vec, MetaTensor* out) {
  auto dim_x = x.dims();
  auto dim_vec = vec.dims();
  PADDLE_ENFORCE_EQ(
      dim_x.size(),
      2,
      phi::errors::InvalidArgument("The rank of input X should be 2, but is %d",
                                   dim_x.size()));
  PADDLE_ENFORCE_EQ(
      dim_vec.size(),
      1,
      phi::errors::InvalidArgument(
          "The rank of input Vec should be 1, but is %d", dim_vec.size()));
  PADDLE_ENFORCE_EQ(dim_x[1],
                    dim_vec[0],
                    phi::errors::InvalidArgument(
                        "X's second dimension is expected to be equal to "
                        "Vec's first dimension"
                        "but recieved X'shape = [%s], Vec's shape = [%s]",
                        dim_x,
                        dim_vec));

  auto dim_out = phi::make_ddim({dim_x[0]});

  out->set_dims(dim_out);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());
  out->share_lod(x);
}

void SegmentPoolInferMeta(const MetaTensor& x,
                          const MetaTensor& segment_ids,
                          const std::string& pooltype,
                          MetaTensor* out,
                          MetaTensor* summed_ids,
                          MetaConfig config) {
  auto dims = x.dims();
  dims[0] = -1;
  out->set_dims(dims);
  out->set_dtype(x.dtype());
  out->set_layout(x.layout());

  if (pooltype == "MEAN") {
    summed_ids->set_dims({-1, 1});
    summed_ids->set_dtype(x.dtype());
    summed_ids->set_layout(x.layout());
  }
}

void SigmoidCrossEntropyWithLogitsInferMeta(const MetaTensor& x,
                                            const MetaTensor& label,
                                            bool normalize,
                                            int ignore_index,
                                            MetaTensor* out,
                                            MetaConfig config) {
  auto x_dims = x.dims();
  auto labels_dims = label.dims();
  int rank = x_dims.size();
  PADDLE_ENFORCE_EQ(rank,
                    labels_dims.size(),
                    phi::errors::InvalidArgument(
                        "Input(X) and Input(Label) shall have the same rank."
                        "But received: the rank of Input(X) is [%d], "
                        "the rank of Input(Label) is [%d].",
                        rank,
                        labels_dims.size()));

  bool check = true;
  if ((!config.is_runtime) &&
      (phi::product(x_dims) <= 0 || phi::product(labels_dims) <= 0)) {
    check = false;
  }

  if (check) {
    PADDLE_ENFORCE_EQ(
        phi::slice_ddim(x_dims, 0, rank),
        phi::slice_ddim(labels_dims, 0, rank),
        phi::errors::InvalidArgument(
            "Input(X) and Input(Label) shall have the same shape "
            "except the last dimension. But received: the shape of "
            "Input(X) is [%s], the shape of Input(Label) is [%s].",
            x_dims,
            labels_dims));
  }

  out->set_dims(x_dims);
  out->set_dtype(x.dtype());
  out->share_lod(x);
}

void TriangularSolveInferMeta(const MetaTensor& x,
                              const MetaTensor& y,
                              bool upper,
                              bool transpose,
                              bool unitriangular,
                              MetaTensor* out) {
  auto x_dims = x.dims();
  auto y_dims = y.dims();

  auto x_dims_n = x_dims.size();
  auto y_dims_n = y_dims.size();

  PADDLE_ENFORCE_GE(x_dims_n,
                    2,
                    phi::errors::InvalidArgument(
                        "The input tensor X's dimensions of TriangularSolveOp "
                        "should be >= 2. But received X's "
                        "dimensions = %d, X's shape = [%s]",
                        x_dims.size(),
                        x_dims));

  PADDLE_ENFORCE_GE(y_dims_n,
                    2,
                    phi::errors::InvalidArgument(
                        "The input tensor Y's dimensions of TriangularSolveOp "
                        "should be >=2. But received Y's "
                        "dimensions = %d, Y's shape = [%s]",
                        y_dims.size(),
                        y_dims));

  PADDLE_ENFORCE_EQ(x_dims[x_dims_n - 2],
                    x_dims[x_dims_n - 1],
                    phi::errors::InvalidArgument(
                        "The inner-most 2 dimensions of Input(X) all should "
                        "be square matrices "
                        "But received X's shape[-2] = %d and shape[-1] = %d.",
                        x_dims[x_dims_n - 2],
                        x_dims[x_dims_n - 1]));

  std::vector<int64_t> x_dims_vec = phi::vectorize(x_dims);
  std::vector<int64_t> y_dims_vec = phi::vectorize(y_dims);

  std::vector<int64_t> x_dims_vec_cut(x_dims_vec.begin(), x_dims_vec.end() - 2);
  std::vector<int64_t> y_dims_vec_cut(y_dims_vec.begin(), y_dims_vec.end() - 2);

  std::vector<int64_t> expand_batch_portion =
      funcs::MatrixGetBroadcastBatchPortion(x_dims_vec_cut, y_dims_vec_cut);

  std::vector<int64_t> y_broadcast_dims({expand_batch_portion});
  y_broadcast_dims.insert(y_broadcast_dims.end(),
                          {y_dims_vec[y_dims_n - 2], y_dims_vec[y_dims_n - 1]});

  // dim of 'out' is the same with 'Y' after broadcast
  out->set_dims(phi::make_ddim(y_broadcast_dims));
  out->set_dtype(y.dtype());
  out->set_layout(y.layout());
  out->share_lod(y);
}

}  // namespace phi
