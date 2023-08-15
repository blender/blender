# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : uniformpruning_zsort.py
#  Authors  : Fredo Durand, Stephane Grabli, Francois Sillion, Emmanuel Turquin
#  Date     : 08/04/2005

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    pyDensityUP1D,
    pyZBP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
)
from freestyle.types import IntegrationType, Operators, Stroke


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator())
# Operators.sequential_split(pyVertexNatureUP0D(Nature.VIEW_VERTEX), 2)
Operators.sort(pyZBP1D())
shaders_list = [
    ConstantThicknessShader(3),
    SamplingShader(5.0),
    ConstantColorShader(0, 0, 0, 1),
]
Operators.create(pyDensityUP1D(2, 0.05, IntegrationType.MEAN, 4), shaders_list)
