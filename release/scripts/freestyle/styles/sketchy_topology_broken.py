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

#  Filename : sketchy_topology_broken.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The topology of the strokes is, first, built
#             independantly from the 3D topology of objects, 
#             and, second, so as to chain several times the same ViewEdge.

from freestyle.chainingiterators import pySketchyChainingIterator
from freestyle.predicates import (
    QuantitativeInvisibilityUP1D,
    TrueUP1D,
    )
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
    SamplingShader,
    SmoothingShader,
    SpatialNoiseShader,
    TextureAssignerShader,
    pyBackboneStretcherNoCuspShader
    )
from freestyle.types import Operators


Operators.select(QuantitativeInvisibilityUP1D(0))
## Chain 3 times each ViewEdge indpendantly from the 
## initial objects topology
Operators.bidirectional_chain(pySketchyChainingIterator(3))
shaders_list = [
    SamplingShader(4),
    SpatialNoiseShader(6, 120, 2, True, True),
    IncreasingThicknessShader(4, 10),
    SmoothingShader(100, 0.1, 0, 0.2, 0, 0, 0, 1),
    pyBackboneStretcherNoCuspShader(20),
    #ConstantColorShader(0.0, 0.0, 0.0)
    IncreasingColorShader(0.2, 0.2, 0.2, 1, 0.5, 0.5, 0.5, 1),
    #IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
    TextureAssignerShader(4),
    ]
Operators.create(TrueUP1D(), shaders_list)
