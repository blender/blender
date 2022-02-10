# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : cartoon.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws colored lines. The color is automatically
#             inferred from each object's material in a cartoon-like
#             fashion.

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    BezierCurveShader,
    ConstantThicknessShader,
    pyMaterialColorShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    BezierCurveShader(3),
    ConstantThicknessShader(4),
    pyMaterialColorShader(0.8),
]
Operators.create(TrueUP1D(), shaders_list)
