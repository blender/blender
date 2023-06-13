# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : blueprint_squares.py
#  Author   : Emmanuel Turquin
#  Date     : 04/08/2005
#  Purpose  : Produces a blueprint using square contour strokes

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
    pyBluePrintSquaresShader,
    pyPerlinNoise1DShader,
)
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
bpred = SameShapeIdBP1D()
Operators.select(upred)
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
Operators.select(pyHigherLengthUP1D(200))
shaders_list = [
    ConstantThicknessShader(8),
    pyBluePrintSquaresShader(2, 20),
    pyPerlinNoise1DShader(0.07, 10, 8),
    IncreasingColorShader(0.6, 0.3, 0.3, 0.7, 0.6, 0.3, 0.3, 0.3),
    ConstantThicknessShader(4),
]
Operators.create(TrueUP1D(), shaders_list)
