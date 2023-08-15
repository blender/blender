# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : external_contour_sketchy.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the external contour of the scene using a sketchy
#             chaining iterator (in particular each ViewEdge can be drawn
#             several times

from freestyle.chainingiterators import pySketchyChainingIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SmoothingShader,
    SpatialNoiseShader,
)
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
Operators.bidirectional_chain(pySketchyChainingIterator(), NotUP1D(upred))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(10, 150, 2, True, True),
    IncreasingThicknessShader(4, 10),
    SmoothingShader(400, 0.1, 0, 0.2, 0, 0, 0, 1),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
