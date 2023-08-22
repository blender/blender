# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : ignore_small_occlusions.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The strokes are drawn through small occlusions

from freestyle.chainingiterators import pyFillOcclusionsAbsoluteChainingIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
# Operators.bidirectional_chain(pyFillOcclusionsChainingIterator(0.1))
Operators.bidirectional_chain(pyFillOcclusionsAbsoluteChainingIterator(12))
shaders_list = [
    SamplingShader(5.0),
    ConstantThicknessShader(3),
    ConstantColorShader(0.0, 0.0, 0.0),
]
Operators.create(TrueUP1D(), shaders_list)
