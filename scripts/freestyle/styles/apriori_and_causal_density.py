# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Selects the lines with high a priori density and
#             subjects them to the causal density so as to avoid
#             cluttering

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    pyDensityUP1D,
    pyHighViewMapDensityUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
)
from freestyle.types import IntegrationType, Operators

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), pyHighViewMapDensityUP1D(0.3, IntegrationType.LAST))
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    ConstantThicknessShader(2),
    ConstantColorShader(0, 0, 0, 1),
]
Operators.create(pyDensityUP1D(1, 0.1, IntegrationType.MEAN), shaders_list)
