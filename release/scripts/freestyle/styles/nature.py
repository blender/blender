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

#  Filename : nature.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Uses the NatureUP1D predicate to select the lines
#             of a given type (among Nature.SILHOUETTE, Nature.CREASE, Nature.SUGGESTIVE_CONTOURS,
#             Nature.BORDERS).
#             The suggestive contours must have been enabled in the 
#             options dialog to appear in the View Map.

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.predicates import (
    NotUP1D,
    TrueUP1D,
    pyNatureUP1D,
    )
from freestyle.shaders import (
    IncreasingColorShader,
    IncreasingThicknessShader,
    )
from freestyle.types import Operators, Nature


Operators.select(pyNatureUP1D(Nature.SILHOUETTE))
Operators.bidirectional_chain(ChainSilhouetteIterator(), NotUP1D(pyNatureUP1D(Nature.SILHOUETTE)))
shaders_list = [
    IncreasingThicknessShader(3, 10),
    IncreasingColorShader(0.0, 0.0, 0.0, 1, 0.8, 0, 0, 1),
    ]
Operators.create(TrueUP1D(), shaders_list)
