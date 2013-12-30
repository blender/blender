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

#  Filename : sketchy_topology_preserved.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The topology of the strokes is built
#             so as to chain several times the same ViewEdge.
#             The topology of the objects is preserved

from freestyle.chainingiterators import pySketchyChainSilhouetteIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    )
from freestyle.shaders import (
    ConstantColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SmoothingShader,
    SpatialNoiseShader,
    TextureAssignerShader,
    )
from freestyle.types import Operators


upred = QuantitativeInvisibilityUP1D(0)
Operators.select(upred)
Operators.bidirectional_chain(pySketchyChainSilhouetteIterator(3, True))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(20, 220, 2, True, True),
    IncreasingThicknessShader(4, 8),
    SmoothingShader(300, 0.05, 0, 0.2, 0, 0, 0, 0.5),
    ConstantColorShader(0.6, 0.2, 0.0),
    TextureAssignerShader(4),
    ]
Operators.create(TrueUP1D(), shaders_list)
