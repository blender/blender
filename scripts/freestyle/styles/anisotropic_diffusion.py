# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : anisotropic_diffusion.py
#  Author   : Fredo Durand
#  Date     : 12/08/2004
#  Purpose  : Smooth lines using an anisotropic diffusion scheme

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
    SamplingShader,
    pyDiffusion2Shader,
)
from freestyle.types import Operators, Stroke


# pyDiffusion2Shader parameters
offset = 0.25
nbIter = 30

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
shaders_list = [
    ConstantThicknessShader(4),
    SamplingShader(2),
    pyDiffusion2Shader(offset, nbIter),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
