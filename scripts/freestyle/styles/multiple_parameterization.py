# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The thickness and the color of the strokes vary continuously
#             independently from occlusions although only
#             visible lines are actually drawn. This is equivalent
#             to assigning the thickness using a parameterization covering
#             the complete silhouette (visible+invisible) and drawing
#             the strokes using a second parameterization that only
#             covers the visible portions.

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    pyHLRShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
# Chain following the same nature, but without the restriction
# of staying inside the selection (False).
Operators.bidirectional_chain(ChainSilhouetteIterator(False))
shaders_list = [
    SamplingShader(20),
    IncreasingThicknessShader(1.5, 30),
    ConstantColorShader(0.0, 0.0, 0.0),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
    pyHLRShader(),  # this shader draws only visible portions
]
Operators.create(TrueUP1D(), shaders_list)
