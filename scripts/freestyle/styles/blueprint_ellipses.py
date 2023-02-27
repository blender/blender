# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : blueprint_ellipses.py
#  Author   : Emmanuel Turquin
#  Date     : 04/08/2005
#  Purpose  : Produces a blueprint using elliptic contour strokes

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    SameShapeIdBP1D,
    TrueUP1D,
    pyHigherLengthUP1D,
)
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
    pyBluePrintEllipsesShader,
    pyPerlinNoise1DShader,
)
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
bpred = SameShapeIdBP1D()
Operators.select(upred)
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
Operators.select(pyHigherLengthUP1D(200))
shaders_list = [
    ConstantThicknessShader(5),
    pyBluePrintEllipsesShader(3),
    pyPerlinNoise1DShader(0.1, 10, 8),
    IncreasingColorShader(0.6, 0.3, 0.3, 0.7, 0.3, 0.3, 0.3, 0.1),
]
Operators.create(TrueUP1D(), shaders_list)
