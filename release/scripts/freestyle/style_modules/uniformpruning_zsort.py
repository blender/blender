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

from Freestyle import ChainSilhouetteIterator, ConstantColorShader, ConstantThicknessShader, IntegrationType, \
    Operators, QuantitativeInvisibilityUP1D, SamplingShader, Stroke, StrokeTextureShader
from PredicatesB1D import pyZBP1D
from PredicatesU1D import pyDensityUP1D

Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator())
#Operators.sequential_split(pyVertexNatureUP0D(Nature.VIEW_VERTEX), 2)
Operators.sort(pyZBP1D())
shaders_list = [
    StrokeTextureShader("smoothAlpha.bmp", Stroke.OPAQUE_MEDIUM, False),
    ConstantThicknessShader(3),
    SamplingShader(5.0),
    ConstantColorShader(0, 0, 0, 1),
    ]
Operators.create(pyDensityUP1D(2, 0.05, IntegrationType.MEAN, 4), shaders_list)
#Operators.create(pyDensityFunctorUP1D(8, 0.03, pyGetInverseProjectedZF1D(), 0, 1, IntegrationType.MEAN), shaders_list)
