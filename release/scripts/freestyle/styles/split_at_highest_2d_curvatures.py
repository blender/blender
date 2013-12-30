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

#  Filename : split_at_highest_2d_curvature.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the visible lines (chaining follows same nature lines)
#             (most basic style module)

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.functions import pyInverseCurvature2DAngleF0D
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyHigherLengthUP1D,
    pyParameterUP0D,
    )
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
    TextureAssignerShader,
    )
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
func = pyInverseCurvature2DAngleF0D()
Operators.recursive_split(func, pyParameterUP0D(0.4, 0.6), NotUP1D(pyHigherLengthUP1D(100)), 2)
shaders_list = [
    ConstantThicknessShader(10),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
    TextureAssignerShader(3),
    ]
Operators.create(TrueUP1D(), shaders_list)
