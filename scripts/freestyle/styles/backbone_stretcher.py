# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : backbone_stretcher.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Stretches the geometry of visible lines

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    BackboneStretcherShader,
    ConstantColorShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    ConstantColorShader(0.5, 0.5, 0.5),
    BackboneStretcherShader(20),
]
Operators.create(TrueUP1D(), shaders_list)
