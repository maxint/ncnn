// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "fuse_module_pass.h"

namespace pnnx {

class LocalResponseNorm : public FuseModulePass
{
public:
    const char* match_type_str() const
    {
        return "__torch__.torch.nn.modules.normalization.LocalResponseNorm";
    }

    const char* type_str() const
    {
        return "nn.LocalResponseNorm";
    }

    void write(Operator* op, const TorchGraphProxy& graph) const
    {
        const TorchNodeProxy* avg_pool = graph.find_node_by_kind("aten::avg_pool2d");
        const TorchNodeProxy* avg_pool3d = graph.find_node_by_kind("aten::avg_pool3d");

        if (avg_pool3d)
        {
            avg_pool = avg_pool3d;
        }

        const TorchNodeProxy* kernel_size = graph.find_producer_node_by_value(avg_pool->namedInput("kernel_size"));
        op->params["size"] = kernel_size->input(0);

        const TorchNodeProxy* pow = graph.find_node_by_kind("aten::pow");
        op->params["beta"] = pow->input(1);

        const TorchNodeProxy* add = graph.find_producer_node_by_value(pow->input(0));
        op->params["k"] = add->input(1);

        const TorchNodeProxy* mul = graph.find_producer_node_by_value(add->input(0));
        op->params["alpha"] = mul->input(1);
    }
};

REGISTER_GLOBAL_PNNX_FUSE_MODULE_PASS(LocalResponseNorm)

} // namespace pnnx
