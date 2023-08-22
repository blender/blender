# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : sketchy_multiple_parameterization.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Builds sketchy strokes whose topology relies on a
#             parameterization that covers the complete lines (visible+invisible)
#             whereas only the visible portions are actually drawn

from freestyle.chainingiterators import pySketchyChainSilhouetteIterator
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
    pyHLRShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(pySketchyChainSilhouetteIterator(3, False))
shaders_list = [
    SamplingShader(2),
    SpatialNoiseShader(15, 120, 2, True, True),
    IncreasingThicknessShader(5, 30),
    SmoothingShader(100, 0.05, 0, 0.2, 0, 0, 0, 1),
    IncreasingColorShader(0, 0.2, 0, 1, 0.2, 0.7, 0.2, 1),
    pyHLRShader(),
]
Operators.create(TrueUP1D(), shaders_list)
