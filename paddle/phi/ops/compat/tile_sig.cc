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

#include "paddle/phi/core/compat/op_utils.h"

namespace phi {

KernelSignature TileOpArgumentMapping(const ArgumentMappingContext& ctx) {
  if (ctx.HasInput("RepeatTimes")) {
    return KernelSignature("tile", {"X"}, {"RepeatTimes"}, {"Out"});
  } else if (ctx.InputSize("repeat_times_tensor") > 0) {
    return KernelSignature("tile", {"X"}, {"repeat_times_tensor"}, {"Out"});
  } else {
    return KernelSignature("tile", {"X"}, {"repeat_times"}, {"Out"});
  }
}

KernelSignature TileGradOpArgumentMapping(const ArgumentMappingContext& ctx) {
  if (ctx.HasInput("RepeatTimes")) {
    return KernelSignature("tile_grad",
                           {"X", GradVarName("Out")},
                           {"RepeatTimes"},
                           {GradVarName("X")});
  } else if (ctx.InputSize("repeat_times_tensor") > 0) {
    return KernelSignature("tile_grad",
                           {"X", GradVarName("Out")},
                           {"repeat_times_tensor"},
                           {GradVarName("X")});
  } else {
    return KernelSignature("tile_grad",
                           {"X", GradVarName("Out")},
                           {"repeat_times"},
                           {GradVarName("X")});
  }
}

}  // namespace phi

PD_REGISTER_ARG_MAPPING_FN(tile, phi::TileOpArgumentMapping);
PD_REGISTER_ARG_MAPPING_FN(tile_grad, phi::TileGradOpArgumentMapping);
