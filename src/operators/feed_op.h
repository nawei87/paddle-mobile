/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

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

#include <string>
#include "framework/operator.h"
#include "operators/op_param.h"

namespace paddle_mobile {
namespace operators {

template <typename DeviceType, typename T>
class FeedOp : public framework::OperatorBase<DeviceType> {
 public:
  FeedOp(const std::string &type, const VariableNameMap &inputs,
         const VariableNameMap &outputs, const framework::AttributeMap attrs,
         std::shared_ptr<framework::Scope> scope)
      : framework::OperatorBase<DeviceType>(type, inputs, outputs, attrs,
                                            scope),
        param_(inputs, outputs, attrs, scope.get()) {}

  void InferShape() const {
    auto out_dims = param_.Out()->dims();
    out_dims[0] = param_.BatchSize();
    param_.Out()->Resize(out_dims);
  }

#ifdef PADDLE_MOBILE_FPGA

  void Init() {
    Tensor *output = param_.Out();
    fpga::format_fp16_ofm(output);
  }

  void RunImpl() const {
    auto input = (Tensor *)const_cast<LoDTensor *>(param_.InputX());  // NOLINT
    fpga::format_image(input);
    auto input_ptr = input->data<float>();
    Tensor *output = param_.Out();
    auto output_ptr = output->data<float>();

    fpga::BypassArgs args = {fpga::DATA_TYPE_FP32};

    args.input_data_type = fpga::DATA_TYPE_FP32;
    args.output_data_type = fpga::DATA_TYPE_FP16;
    args.input_layout_type = fpga::LAYOUT_CHW;
    args.output_layout_type = fpga::LAYOUT_HWC;
    args.image.address = (void *)input_ptr;  // NOLINT
    args.image.channels = (uint32_t)input->dims()[1];
    args.image.height = (uint32_t)input->dims()[2];
    args.image.width = (uint32_t)input->dims()[3];
    args.image.pad_height = 0;
    args.image.pad_width = 0;
    args.output.address = output_ptr;
    args.output.scale_address = output->scale;
    fpga::PerformBypass(args);
  }

#else
  void Init() {}
  void RunImpl() const {
    param_.Out()->ShareDataWith(*param_.InputX());
    param_.Out()->set_lod(param_.InputX()->lod());
  }
#endif

 protected:
  FeedParam<DeviceType> param_;
};

}  // namespace operators
}  // namespace paddle_mobile
