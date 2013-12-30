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

#  Filename : sequentialsplit_sketchy.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Use the sequential split with two different
#             predicates to specify respectively the starting and
#             the stopping extremities for strokes 

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyBackTVertexUP0D,
    pyVertexNatureUP0D,
    )
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
    SpatialNoiseShader,
    TextureAssignerShader,
    )
from freestyle.types import Nature, Operators


upred = QuantitativeInvisibilityUP1D(0)
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
## starting and stopping predicates:
start = pyVertexNatureUP0D(Nature.NON_T_VERTEX)
stop = pyBackTVertexUP0D()
Operators.sequential_split(start, stop, 10)
shaders_list = [
    SpatialNoiseShader(7, 120, 2, True, True),
    IncreasingThicknessShader(5, 8),
    ConstantColorShader(0.2, 0.2, 0.2, 1),
    TextureAssignerShader(4),
    ]
Operators.create(TrueUP1D(), shaders_list)
