# Tencent is pleased to support the open source community by making ncnn available.
#
# Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
# in compliance with the License. You may obtain a copy of the License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import torch
import torch.nn as nn
import torch.nn.functional as F
from packaging import version

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

        self.pool_0 = nn.AdaptiveAvgPool2d(output_size=(6,5))
        self.pool_1 = nn.AdaptiveAvgPool2d(output_size=1)
        self.pool_2 = nn.AdaptiveAvgPool2d(output_size=(None,3))
        self.pool_3 = nn.AdaptiveAvgPool2d(output_size=(3,None))

    def forward(self, x):
        out0 = self.pool_0(x)
        out1 = self.pool_1(x)
        if version.parse(torch.__version__) < version.parse('1.10'):
            return out0, out1

        out2 = self.pool_2(x)
        out3 = self.pool_3(x)
        return out0, out1, out2, out3

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 128, 18, 15)

    a = net(x)

    # export onnx
    torch.onnx.export(net, (x,), "test_nn_AdaptiveAvgPool2d.onnx")

    # onnx to pnnx
    import os
    os.system("../../src/pnnx test_nn_AdaptiveAvgPool2d.onnx inputshape=[1,128,18,15]")

    # pnnx inference
    import test_nn_AdaptiveAvgPool2d_pnnx
    b = test_nn_AdaptiveAvgPool2d_pnnx.test_inference()

    for a0, b0 in zip(a, b):
        if not torch.allclose(a0, b0, 1e-4, 1e-4):
            return False
    return True

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
