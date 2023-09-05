# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Removes strokes extremities

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
    TipRemoverShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    SamplingShader(5),
    ConstantThicknessShader(3),
    ConstantColorShader(0, 0, 0),
    TipRemoverShader(20),
]
Operators.create(TrueUP1D(), shaders_list)
