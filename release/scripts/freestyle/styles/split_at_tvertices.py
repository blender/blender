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

#  Filename : split_at_tvertices.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws strokes that starts and stops at Tvertices (visible or not)

from freestyle import ChainSilhouetteIterator, ConstantThicknessShader, IncreasingColorShader, \
    Nature, Operators, QuantitativeInvisibilityUP1D, TextureAssignerShader, TrueUP1D
from PredicatesU0D import pyVertexNatureUP0D
from logical_operators import NotUP1D

Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
start = pyVertexNatureUP0D(Nature.T_VERTEX)
## use the same predicate to decide where to start and where to stop
## the strokes:
Operators.sequential_split(start, start, 10)
shaders_list = [
    ConstantThicknessShader(5),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
    TextureAssignerShader(3),
    ]
Operators.create(TrueUP1D(), shaders_list)
