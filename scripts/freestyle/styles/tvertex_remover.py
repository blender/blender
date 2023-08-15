# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Filename : tvertex_remover.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Removes TVertices

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
)
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    pyTVertexRemoverShader,
)
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    IncreasingThicknessShader(3, 5),
    ConstantColorShader(0.2, 0.2, 0.2, 1),
    SamplingShader(10.0),
    pyTVertexRemoverShader(),
]
Operators.create(TrueUP1D(), shaders_list)
