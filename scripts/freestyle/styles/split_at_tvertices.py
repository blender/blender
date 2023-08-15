# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : split_at_tvertices.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws strokes that starts and stops at Tvertices (visible or not)

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyVertexNatureUP0D,
)
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
)
from freestyle.types import Nature, Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
start = pyVertexNatureUP0D(Nature.T_VERTEX)
# use the same predicate to decide where to start and where to stop
# the strokes:
Operators.sequential_split(start, start, 10)
shaders_list = [
    ConstantThicknessShader(5),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
