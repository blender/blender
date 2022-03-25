# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : occluded_by_specific_object.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws only the lines that are occluded by a given object

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyIsInOccludersListUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
)
from freestyle.types import Id, Operators


# the id of the occluder (use SHIFT+click on the ViewMap to
# retrieve ids)
id = Id(3, 0)
upred = AndUP1D(NotUP1D(QuantitativeInvisibilityUP1D(0)), pyIsInOccludersListUP1D(id))
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    SamplingShader(5),
    ConstantThicknessShader(3),
    ConstantColorShader(0.3, 0.3, 0.3, 1),
]
Operators.create(TrueUP1D(), shaders_list)
