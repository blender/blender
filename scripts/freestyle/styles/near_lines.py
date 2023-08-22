# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : near_lines.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the lines that are "closer" than a threshold
#             (between 0 and 1)

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyZSmallerUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
)
from freestyle.types import IntegrationType, Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), pyZSmallerUP1D(0.5, IntegrationType.MEAN))
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    ConstantThicknessShader(5),
    ConstantColorShader(0.0, 0.0, 0.0),
]
Operators.create(TrueUP1D(), shaders_list)
