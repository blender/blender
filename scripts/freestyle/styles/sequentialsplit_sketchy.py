# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : sequentialsplit_sketchy.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Use the sequential split with two different
#             predicates to specify respectively the starting and
#             the stopping extremities for strokes

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyBackTVertexUP0D,
    pyVertexNatureUP0D,
)
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
    SpatialNoiseShader,
)
from freestyle.types import Nature, Operators


upred = QuantitativeInvisibilityUP1D(0)
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
# starting and stopping predicates:
start = pyVertexNatureUP0D(Nature.NON_T_VERTEX)
stop = pyBackTVertexUP0D()
Operators.sequential_split(start, stop, 10)
shaders_list = [
    SpatialNoiseShader(7, 120, 2, True, True),
    IncreasingThicknessShader(5, 8),
    ConstantColorShader(0.2, 0.2, 0.2, 1),
]
Operators.create(TrueUP1D(), shaders_list)
