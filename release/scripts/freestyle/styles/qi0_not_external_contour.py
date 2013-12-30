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

#  Filename : qi0_not_external_contour.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the visible lines (chaining follows same nature lines)
#             that do not belong to the external contour of the scene

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    )
from freestyle.shaders import (
    BackboneStretcherShader,
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SpatialNoiseShader,
    TextureAssignerShader,
    )
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(4, 150, 2, True, True),
    IncreasingThicknessShader(2, 5),
    BackboneStretcherShader(20),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
    TextureAssignerShader(4),
    ]
Operators.create(TrueUP1D(), shaders_list)
