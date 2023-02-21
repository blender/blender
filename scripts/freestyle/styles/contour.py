# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : contour.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws each object's visible contour

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    SameShapeIdBP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
)
from freestyle.types import Operators


Operators.select(AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D()))
bpred = SameShapeIdBP1D()
upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    ConstantThicknessShader(5.0),
    IncreasingColorShader(0.8, 0, 0, 1, 0.1, 0, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
