# SPDX-FileCopyrightText: 2008-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : split_at_highest_2d_curvature.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the visible lines (chaining follows same nature lines)
#             (most basic style module)

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.functions import pyInverseCurvature2DAngleF0D
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyHigherLengthUP1D,
    pyParameterUP0D,
)
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
func = pyInverseCurvature2DAngleF0D()
Operators.recursive_split(func, pyParameterUP0D(0.4, 0.6), NotUP1D(pyHigherLengthUP1D(100)), 2)
shaders_list = [
    ConstantThicknessShader(10),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
