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

#  Filename : ignore_small_oclusions.py
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
#Operators.bidirectional_chain(pyFillOcclusionsChainingIterator(0.1))
Operators.bidirectional_chain(pyFillOcclusionsAbsoluteChainingIterator(12))
shaders_list = [
    SamplingShader(5.0),
    ConstantThicknessShader(3),
    ConstantColorShader(0.0, 0.0, 0.0),
    ]
Operators.create(TrueUP1D(), shaders_list)
