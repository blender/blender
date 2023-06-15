# SPDX-FileCopyrightText: 2008-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : external_contour.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the external contour of the scene

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
)
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
shaders_list = [
    ConstantThicknessShader(3),
    ConstantColorShader(0.0, 0.0, 0.0, 1),
]
Operators.create(TrueUP1D(), shaders_list)
