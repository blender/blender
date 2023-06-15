# SPDX-FileCopyrightText: 2008-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : sketchy_topology_broken.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The topology of the strokes is, first, built
#             independently from the 3D topology of objects,
#             and, second, so as to chain several times the same ViewEdge.

from freestyle.chainingiterators import pySketchyChainingIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SmoothingShader,
    SpatialNoiseShader,
    pyBackboneStretcherNoCuspShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
# Chain 3 times each ViewEdge independently from the
# initial objects topology
Operators.bidirectional_chain(pySketchyChainingIterator(3))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(6, 120, 2, True, True),
    IncreasingThicknessShader(4, 10),
    SmoothingShader(100, 0.1, 0, 0.2, 0, 0, 0, 1),
    pyBackboneStretcherNoCuspShader(20),
    IncreasingColorShader(0.2, 0.2, 0.2, 1, 0.5, 0.5, 0.5, 1),
]
Operators.create(TrueUP1D(), shaders_list)
