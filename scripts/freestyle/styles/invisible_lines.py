# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : invisible_lines.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws all lines whose Quantitative Invisibility
#             is different from 0

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
)
from freestyle.types import Operators


upred = NotUP1D(QuantitativeInvisibilityUP1D(0))
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    SamplingShader(5.0),
    ConstantThicknessShader(3.0),
    ConstantColorShader(0.7, 0.7, 0.7),
]
Operators.create(TrueUP1D(), shaders_list)
