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

#  Filename : blueprint_circles.py
#  Author   : Emmanuel Turquin
#  Date     : 04/08/2005
#  Purpose  : Produces a blueprint using circular contour strokes

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ContourUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    SameShapeIdBP1D,
    TrueUP1D,
    pyHigherLengthUP1D,
    )
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
    TextureAssignerShader,
    pyBluePrintCirclesShader,
    pyPerlinNoise1DShader,
    )
from freestyle.types import Operators


upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
bpred = SameShapeIdBP1D()
Operators.select(upred)
Operators.bidirectional_chain(ChainPredicateIterator(upred,bpred), NotUP1D(upred))
Operators.select(pyHigherLengthUP1D(200))
shaders_list = [
    ConstantThicknessShader(5),
    pyBluePrintCirclesShader(3),
    pyPerlinNoise1DShader(0.1, 15, 8),
    TextureAssignerShader(4),
    IncreasingColorShader(0.8, 0.8, 0.3, 0.4, 0.3, 0.3, 0.3, 0.1),
    ]
Operators.create(TrueUP1D(), shaders_list)
