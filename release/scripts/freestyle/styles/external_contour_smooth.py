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

#  Filename : external_contour_smooth.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws a smooth external contour

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    TrueUP1D,
    )
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SmoothingShader,
    )
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
shaders_list = [
    SamplingShader(2),
    IncreasingThicknessShader(4,20),
    IncreasingColorShader(1.0, 0.0, 0.5,1, 0.5,1, 0.3, 1),
    SmoothingShader(100, 0.05, 0, 0.2, 0, 0, 0, 1),
    ]
Operators.create(TrueUP1D(), shaders_list)
