# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the visible lines (chaining follows same nature lines)
#             that do not belong to the external contour of the scene

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    BackboneStretcherShader,
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SpatialNoiseShader,
)
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(4, 150, 2, True, True),
    IncreasingThicknessShader(2, 5),
    BackboneStretcherShader(20),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
