# SPDX-FileCopyrightText: 2008-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : suggestive.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the suggestive contours.
#             ***** The suggestive contours must be enabled
#             in the options dialog *****

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyNatureUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
)
from freestyle.types import Nature, Operators


upred = AndUP1D(pyNatureUP1D(Nature.SUGGESTIVE_CONTOUR), QuantitativeInvisibilityUP1D(0))
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    IncreasingThicknessShader(1, 3),
    ConstantColorShader(0.2, 0.2, 0.2, 1),
]
Operators.create(TrueUP1D(), shaders_list)
