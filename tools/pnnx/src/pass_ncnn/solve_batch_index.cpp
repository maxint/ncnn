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

#include "solve_batch_index.h"

#include <algorithm>

namespace pnnx {

namespace ncnn {

static bool is_known_operator_with_batch_index_0(const Operator* op)
{
    static const char* operator_with_batch_index_0[] = {
        "F.adaptive_avg_pool1d",
        "F.adaptive_avg_pool2d",
        "F.adaptive_avg_pool3d",
        "F.adaptive_max_pool1d",
        "F.adaptive_max_pool2d",
        "F.adaptive_max_pool3d",
        "F.affine_grid",
        "F.avg_pool1d",
        "F.avg_pool2d",
        "F.avg_pool3d",
        "F.batch_norm",
        "F.conv_transpose1d",
        "F.conv_transpose2d",
        "F.conv_transpose3d",
        "F.conv1d",
        "F.conv2d",
        "F.conv3d",
        "F.embedding",
        "F.fold",
        "F.grid_sample",
        "F.group_norm",
        "F.instance_norm",
        "F.interpolate",
        "F.layer_norm",
        "F.linear",
        "F.local_response_norm",
        "F.lp_pool1d",
        "F.lp_pool2d",
        "F.max_pool1d",
        "F.max_pool2d",
        "F.max_pool3d",
        "F.pixel_shuffle",
        "F.pixel_unshuffle",
        "F.prelu",
        "F.rms_norm",
        "F.scaled_dot_product_attention",
        "F.unfold",
        "F.upsample_bilinear",
        "F.upsample_nearest",
        "F.upsample",

        "nn.AdaptiveAvgPool1d",
        "nn.AdaptiveAvgPool2d",
        "nn.AdaptiveAvgPool3d",
        "nn.AdaptiveMaxPool1d",
        "nn.AdaptiveMaxPool2d",
        "nn.AdaptiveMaxPool3d",
        "nn.AvgPool1d",
        "nn.AvgPool2d",
        "nn.AvgPool3d",
        "nn.BatchNorm1d",
        "nn.BatchNorm2d",
        "nn.BatchNorm3d",
        "nn.ChannelShuffle",
        "nn.ConstantPad1d",
        "nn.ConstantPad2d",
        "nn.ConstantPad3d",
        "nn.Conv1d",
        "nn.Conv2d",
        "nn.Conv3d",
        "nn.ConvTranspose1d",
        "nn.ConvTranspose2d",
        "nn.ConvTranspose3d",
        "nn.Embedding",
        "nn.Fold",
        "nn.GroupNorm",
        "nn.InstanceNorm1d",
        "nn.InstanceNorm2d",
        "nn.InstanceNorm3d",
        "nn.LocalResponseNorm",
        "nn.LayerNorm",
        "nn.LPPool1d",
        "nn.LPPool2d",
        "nn.MaxPool1d",
        "nn.MaxPool2d",
        "nn.MaxPool3d",
        "nn.PixelShuffle",
        "nn.PixelUnshuffle",
        "nn.PReLU",
        "nn.ReflectionPad1d",
        "nn.ReflectionPad2d",
        "nn.ReplicationPad1d",
        "nn.ReplicationPad2d",
        "nn.ReplicationPad3d",
        "nn.RMSNorm",
        "nn.Softmax2d",
        "nn.Unfold",
        "nn.Upsample",
        "nn.UpsamplingBilinear2d",
        "nn.UpsamplingNearest2d",
        "nn.ZeroPad2d",
    };

    const size_t operator_with_batch_index_0_count = sizeof(operator_with_batch_index_0) / sizeof(const char*);
    for (size_t i = 0; i < operator_with_batch_index_0_count; i++)
    {
        if (op->type == operator_with_batch_index_0[i])
            return true;
    }

    return false;
}

static bool is_known_operator_with_batch_first_param(const Operator* op)
{
    return op->type == "nn.RNN" || op->type == "nn.LSTM" || op->type == "nn.GRU" || op->type == "nn.MultiheadAttention";
}

static void solve_batch_index_backward(Operand* operand);
static void solve_batch_index_forward(Operand* operand)
{
    if (operand->params.find("__batch_index") == operand->params.end())
        return;

    int batch_index = operand->params["__batch_index"].i;
    if (batch_index == 233)
        return;

    for (Operator* op : operand->consumers)
    {
        if (is_known_operator_with_batch_index_0(op))
            continue;

        if (is_known_operator_with_batch_first_param(op))
            continue;

        const int input_rank0 = op->inputs.empty() ? 0 : (int)op->inputs[0]->shape.size();

        if (op->type == "Tensor.permute")
        {
            const std::vector<int>& dims = op->params.at("dims").ai;

            int batch_index_permuted = -1;
            for (int i = 0; i < (int)dims.size(); i++)
            {
                int dim = dims[i];
                if (dim < 0)
                    dim += input_rank0;

                if (dim >= 0 && dim == batch_index)
                {
                    batch_index_permuted = i;
                    break;
                }
            }

            for (Operand* r : op->outputs)
            {
                if (r->params.find("__batch_index") != r->params.end())
                    continue;

                r->params["__batch_index"] = batch_index_permuted;

                solve_batch_index_forward(r);
                solve_batch_index_backward(r);
            }
        }
        else if (op->type == "torch.transpose")
        {
            int dim0 = op->params.at("dim0").i;
            int dim1 = op->params.at("dim1").i;
            if (dim0 < 0)
                dim0 += input_rank0;
            if (dim1 < 0)
                dim1 += input_rank0;

            int batch_index_transposed = batch_index;
            if (dim0 >= 0 && dim0 == batch_index)
            {
                batch_index_transposed = dim1;
            }
            else if (dim1 >= 0 && dim1 == batch_index)
            {
                batch_index_transposed = dim0;
            }

            for (Operand* r : op->outputs)
            {
                if (r->params.find("__batch_index") != r->params.end())
                    continue;

                r->params["__batch_index"] = batch_index_transposed;

                solve_batch_index_forward(r);
                solve_batch_index_backward(r);
            }
        }
        else if (op->type == "Tensor.reshape" || op->type == "Tensor.view")
        {
            if (op->params.find("shape") == op->params.end())
            {
                continue;
            }

            const std::vector<int>& shape = op->params.at("shape").ai;

            if (shape[batch_index] == 1)
            {
                for (Operand* r : op->outputs)
                {
                    if (r->params.find("__batch_index") != r->params.end())
                        continue;

                    r->params["__batch_index"] = batch_index;

                    solve_batch_index_forward(r);
                    solve_batch_index_backward(r);
                }
            }
            else
            {
                // give up reshape across batch index
            }
        }
        else if (op->type == "Tensor.slice" || op->type == "Tensor.select")
        {
            Operand* r = op->outputs[0];
            if (r->params.find("__batch_index") == r->params.end())
            {
                r->params["__batch_index"] = batch_index;

                solve_batch_index_forward(r);
                solve_batch_index_backward(r);
            }
        }
        else if (op->type == "pnnx.SliceIndexes")
        {
            // pass
        }
        else if (op->type == "torch.squeeze")
        {
            int dim = op->params.at("dim").i;
            if (dim < 0)
                dim += input_rank0;

            int batch_index_squeezed = batch_index;
            if (dim >= 0 && dim < batch_index)
            {
                batch_index_squeezed = batch_index - 1;
            }
            if (dim >= 0 && dim == batch_index)
            {
                batch_index_squeezed = 233;
            }

            Operand* r = op->outputs[0];
            if (r->params.find("__batch_index") == r->params.end())
            {
                r->params["__batch_index"] = batch_index_squeezed;

                solve_batch_index_forward(r);
                solve_batch_index_backward(r);
            }
        }
        else if (op->type == "torch.unsqueeze")
        {
            int dim = op->params.at("dim").i;
            if (dim < 0)
                dim += input_rank0;

            int batch_index_unsqueezed = batch_index;
            if (dim >= 0 && dim <= batch_index)
            {
                batch_index_unsqueezed = batch_index + 1;
            }

            Operand* r = op->outputs[0];
            if (r->params.find("__batch_index") == r->params.end())
            {
                r->params["__batch_index"] = batch_index_unsqueezed;

                solve_batch_index_forward(r);
                solve_batch_index_backward(r);
            }
        }
        else
        {
            for (Operand* r : op->outputs)
            {
                if (r->params.find("__batch_index") != r->params.end())
                    continue;

                r->params["__batch_index"] = batch_index;

                solve_batch_index_forward(r);
                solve_batch_index_backward(r);
            }
        }
    }
}

static void solve_batch_index_backward(Operand* operand)
{
    if (operand->params.find("__batch_index") == operand->params.end())
        return;

    int batch_index = operand->params["__batch_index"].i;
    if (batch_index == 233)
        return;

    Operator* op = operand->producer;
    if (is_known_operator_with_batch_index_0(op))
        return;

    if (is_known_operator_with_batch_first_param(op))
        return;

    const int input_rank0 = op->inputs.empty() ? 0 : (int)op->inputs[0]->shape.size();

    if (op->type == "Tensor.permute")
    {
        const std::vector<int>& dims = op->params.at("dims").ai;

        int batch_index_permuted = dims[batch_index];
        if (batch_index_permuted < 0)
            batch_index_permuted += input_rank0;

        for (Operand* r : op->inputs)
        {
            if (r->params.find("__batch_index") != r->params.end())
                continue;

            r->params["__batch_index"] = batch_index_permuted;

            solve_batch_index_backward(r);
            solve_batch_index_forward(r);
        }
    }
    else if (op->type == "torch.transpose")
    {
        int dim0 = op->params.at("dim0").i;
        int dim1 = op->params.at("dim1").i;
        if (dim0 < 0)
            dim0 += input_rank0;
        if (dim1 < 0)
            dim1 += input_rank0;

        int batch_index_transposed = batch_index;
        if (dim0 >= 0 && dim0 == batch_index)
        {
            batch_index_transposed = dim1;
        }
        else if (dim1 >= 0 && dim1 == batch_index)
        {
            batch_index_transposed = dim0;
        }

        for (Operand* r : op->inputs)
        {
            if (r->params.find("__batch_index") != r->params.end())
                continue;

            r->params["__batch_index"] = batch_index_transposed;

            solve_batch_index_backward(r);
            solve_batch_index_forward(r);
        }
    }
    else if (op->type == "Tensor.reshape" || op->type == "Tensor.view")
    {
        if (op->params.find("shape") == op->params.end())
        {
            return;
        }

        const std::vector<int>& shape = op->params.at("shape").ai;

        if (shape[batch_index] == 1)
        {
            for (Operand* r : op->inputs)
            {
                if (r->params.find("__batch_index") != r->params.end())
                    continue;

                r->params["__batch_index"] = batch_index;

                solve_batch_index_backward(r);
                solve_batch_index_forward(r);
            }
        }
        else
        {
            // give up reshape across batch index
        }
    }
    else if (op->type == "Tensor.slice" || op->type == "Tensor.select")
    {
        Operand* r = op->inputs[0];
        if (r->params.find("__batch_index") == r->params.end())
        {
            r->params["__batch_index"] = batch_index;

            solve_batch_index_backward(r);
            solve_batch_index_forward(r);
        }
    }
    else if (op->type == "pnnx.SliceIndexes")
    {
        // pass
    }
    else if (op->type == "torch.squeeze")
    {
        int dim = op->params.at("dim").i;
        if (dim < 0)
            dim += input_rank0;

        int batch_index_unsqueezed = batch_index;
        if (dim >= 0 && dim <= batch_index)
        {
            batch_index_unsqueezed = batch_index + 1;
        }

        Operand* r = op->inputs[0];
        if (r->params.find("__batch_index") == r->params.end())
        {
            r->params["__batch_index"] = batch_index_unsqueezed;

            solve_batch_index_backward(r);
            solve_batch_index_forward(r);
        }
    }
    else if (op->type == "torch.unsqueeze")
    {
        int dim = op->params.at("dim").i;
        if (dim < 0)
            dim += input_rank0;

        int batch_index_squeezed = batch_index;
        if (dim >= 0 && dim <= batch_index)
        {
            batch_index_squeezed = batch_index - 1;
        }

        Operand* r = op->inputs[0];
        if (r->params.find("__batch_index") == r->params.end())
        {
            r->params["__batch_index"] = batch_index_squeezed;

            solve_batch_index_backward(r);
            solve_batch_index_forward(r);
        }
    }
    else
    {
        for (Operand* r : op->inputs)
        {
            if (r->params.find("__batch_index") != r->params.end())
                continue;

            r->params["__batch_index"] = batch_index;

            solve_batch_index_backward(r);
            solve_batch_index_forward(r);
        }
    }
}

void solve_batch_index(Graph& graph)
{
    // assign known operator
    for (Operator* op : graph.ops)
    {
        if (is_known_operator_with_batch_index_0(op))
        {
            if (op->type == std::string("F.grid_sample"))
            {
                op->inputs[1]->params["__batch_index"] = 0;
            }

            op->inputs[0]->params["__batch_index"] = 0;
            op->outputs[0]->params["__batch_index"] = 0;
        }

        if (is_known_operator_with_batch_first_param(op))
        {
            bool batch_first = false;
            if (op->params.find("batch_first") != op->params.end())
            {
                batch_first = op->params["batch_first"].b;
            }

            op->inputs[0]->params["__batch_index"] = batch_first ? 0 : 1;
            op->outputs[0]->params["__batch_index"] = batch_first ? 0 : 1;

            for (size_t j = 1; j < op->inputs.size(); j++)
            {
                if (op->type == "nn.MultiheadAttention")
                {
                    if (op->inputnames.size() == op->inputs.size() && op->inputnames[j] == "attn_mask")
                    {
                        // no batch for mha attn_mask
                        op->inputs[j]->params["__batch_index"] = 233;
                    }
                    else
                    {
                        op->inputs[j]->params["__batch_index"] = batch_first ? 0 : 1;
                    }

                    continue;
                }

                op->inputs[j]->params["__batch_index"] = 1;
            }

            for (size_t j = 1; j < op->outputs.size(); j++)
            {
                op->outputs[j]->params["__batch_index"] = 1;
            }
        }
    }

    // batch index propagate
    for (Operator* op : graph.ops)
    {
        for (Operand* r : op->inputs)
        {
            solve_batch_index_backward(r);
        }

        for (Operand* r : op->outputs)
        {
            solve_batch_index_forward(r);
        }
    }

    // always treat 1-dim tensor as no batch axis
    for (Operand* r : graph.operands)
    {
        if (r->shape.size() == 1)
        {
            fprintf(stderr, "force batch axis 233 for operand %s\n", r->name.c_str());
            r->params["__batch_index"] = 233;
        }
    }

    // fallback axis 233 for unknown
    for (Operand* r : graph.operands)
    {
        if (r->params.find("__batch_index") == r->params.end())
        {
            fprintf(stderr, "fallback batch axis 233 for operand %s\n", r->name.c_str());
            r->params["__batch_index"] = 233;
        }
    }
}

} // namespace ncnn

} // namespace pnnx
