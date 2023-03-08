# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : apriori_density.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws lines having a high a prior density

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    TrueUP1D,
    pyHighViewMapDensityUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
)
from freestyle.types import Operators


Operators.select(AndUP1D(QuantitativeInvisibilityUP1D(0), pyHighViewMapDensityUP1D(0.1, 5)))
bpred = TrueBP1D()
upred = AndUP1D(QuantitativeInvisibilityUP1D(0), pyHighViewMapDensityUP1D(0.0007, 5))
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    ConstantThicknessShader(2),
    ConstantColorShader(0.0, 0.0, 0.0, 1.0)
]
Operators.create(TrueUP1D(), shaders_list)
