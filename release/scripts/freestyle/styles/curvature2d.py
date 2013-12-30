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
    StrokeTextureShader,
    py2DCurvatureColorShader,
    )
from freestyle.types import Operators, Stroke


Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    StrokeTextureShader("smoothAlpha.bmp", Stroke.OPAQUE_MEDIUM, False),
    ConstantThicknessShader(5),
    py2DCurvatureColorShader()
    ]
Operators.create(TrueUP1D(), shaders_list)
