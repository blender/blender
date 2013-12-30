# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

#  Filename : japanese_bigbrush.py
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
    TextureAssignerShader,
    TipRemoverShader,
    pyNonLinearVaryingThicknessShader,
    pySamplingShader,
    )
from freestyle.types import IntegrationType, Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
## Splits strokes at points of highest 2D curavture 
## when there are too many abrupt turns in it
func = pyInverseCurvature2DAngleF0D()
Operators.recursive_split(func, pyParameterUP0D(0.2, 0.8), NotUP1D(pyHigherNumberOfTurnsUP1D(3, 0.5)), 2)
## Keeps only long enough strokes
Operators.select(pyHigherLengthUP1D(100))
## Sorts so as to draw the longest strokes first
## (this will be done using the causal density)
Operators.sort(pyLengthBP1D())
shaders_list = [
    pySamplingShader(10),
    BezierCurveShader(30),
    SamplingShader(50),
    ConstantThicknessShader(10),
    pyNonLinearVaryingThicknessShader(4, 25, 0.6),
    TextureAssignerShader(6),
    ConstantColorShader(0.2, 0.2, 0.2,1.0),
    TipRemoverShader(10),
    ]
## Use the causal density to avoid cluttering
Operators.create(pyDensityUP1D(8, 0.4, IntegrationType.MEAN), shaders_list)
