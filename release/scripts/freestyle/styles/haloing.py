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

#  Filename : haloing.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : This style module selects the lines that 
#             are connected (in the image) to a specific 
#             object and trims them in order to produce
#             a haloing effect around the target shape

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyIsOccludedByUP1D,
    )
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    TipRemoverShader,
    pyTVertexRemoverShader,
    )
from freestyle.types import Id, Operators


# id corresponds to the id of the target object 
# (accessed by SHIFT+click)
id = Id(3,0)
upred = AndUP1D(QuantitativeInvisibilityUP1D(0), pyIsOccludedByUP1D(id))
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    IncreasingThicknessShader(3, 5), 
    IncreasingColorShader(1,0,0, 1,0,1,0,1),
    SamplingShader(1.0),
    pyTVertexRemoverShader(),
    TipRemoverShader(3.0),
    ]
Operators.create(TrueUP1D(), shaders_list)
