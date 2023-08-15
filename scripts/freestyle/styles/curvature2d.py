# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : curvature2d.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The stroke points are colored in gray levels and depending
#             on the 2d curvature value

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantThicknessShader,
    py2DCurvatureColorShader,
)
from freestyle.types import Operators, Stroke


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    ConstantThicknessShader(5),
    py2DCurvatureColorShader()
]
Operators.create(TrueUP1D(), shaders_list)
