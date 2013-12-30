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

#  Filename : suggestive.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the suggestive contours.
#             ***** The suggestive contours must be enabled
#             in the options dialog *****

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    pyNatureUP1D,
    )
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
    )
from freestyle.types import Nature, Operators


upred = AndUP1D(pyNatureUP1D(Nature.SUGGESTIVE_CONTOUR), QuantitativeInvisibilityUP1D(0))
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    IncreasingThicknessShader(1, 3),
    ConstantColorShader(0.2, 0.2, 0.2, 1),
    ]
Operators.create(TrueUP1D(), shaders_list)
