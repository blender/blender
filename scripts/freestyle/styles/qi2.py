# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws lines hidden by two surfaces.
#             *** Quantitative Invisibility must have been
#             enabled in the options dialog to use this style module ****

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(2))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(2)))
shaders_list = [
    SamplingShader(10),
    ConstantThicknessShader(1.5),
    ConstantColorShader(0.7, 0.7, 0.7, 1),
]
Operators.create(TrueUP1D(), shaders_list)
