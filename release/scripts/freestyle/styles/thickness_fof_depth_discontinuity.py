# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : thickness_fof_depth_discontinuity.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Assigns to strokes a thickness that depends on the depth discontinuity

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
    pyDepthDiscontinuityThicknessShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    SamplingShader(1),
    ConstantThicknessShader(3),
    ConstantColorShader(0.0, 0.0, 0.0),
    pyDepthDiscontinuityThicknessShader(0.8, 6),
]
Operators.create(TrueUP1D(), shaders_list)
