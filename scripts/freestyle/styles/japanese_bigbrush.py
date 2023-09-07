# SPDX-FileCopyrightText: 2008-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Simulates a big brush fr oriental painting

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.functions import pyInverseCurvature2DAngleF0D
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    pyDensityUP1D,
    pyHigherLengthUP1D,
    pyHigherNumberOfTurnsUP1D,
    pyLengthBP1D,
    pyParameterUP0D,
)
from freestyle.shaders import (
    BezierCurveShader,
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
    TipRemoverShader,
    pyNonLinearVaryingThicknessShader,
    pySamplingShader,
)
from freestyle.types import IntegrationType, Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
# Splits strokes at points of highest 2D curvature
# when there are too many abrupt turns in it
func = pyInverseCurvature2DAngleF0D()
Operators.recursive_split(func, pyParameterUP0D(0.2, 0.8), NotUP1D(pyHigherNumberOfTurnsUP1D(3, 0.5)), 2)
# Keeps only long enough strokes
Operators.select(pyHigherLengthUP1D(100))
# Sorts so as to draw the longest strokes first
# (this will be done using the causal density)
Operators.sort(pyLengthBP1D())
shaders_list = [
    pySamplingShader(10),
    BezierCurveShader(30),
    SamplingShader(50),
    ConstantThicknessShader(10),
    pyNonLinearVaryingThicknessShader(4, 25, 0.6),
    ConstantColorShader(0.2, 0.2, 0.2, 1.0),
    TipRemoverShader(10),
]
# Use the causal density to avoid cluttering
Operators.create(pyDensityUP1D(8, 0.4, IntegrationType.MEAN), shaders_list)
