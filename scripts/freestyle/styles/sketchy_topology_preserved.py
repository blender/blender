# SPDX-FileCopyrightText: 2008-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : sketchy_topology_preserved.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The topology of the strokes is built
#             so as to chain several times the same ViewEdge.
#             The topology of the objects is preserved

from freestyle.chainingiterators import pySketchyChainSilhouetteIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SmoothingShader,
    SpatialNoiseShader,
)
from freestyle.types import Operators


upred = QuantitativeInvisibilityUP1D(0)
Operators.select(upred)
Operators.bidirectional_chain(pySketchyChainSilhouetteIterator(3, True))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(20, 220, 2, True, True),
    IncreasingThicknessShader(4, 8),
    SmoothingShader(300, 0.05, 0, 0.2, 0, 0, 0, 0.5),
    ConstantColorShader(0.6, 0.2, 0.0),
]
Operators.create(TrueUP1D(), shaders_list)
