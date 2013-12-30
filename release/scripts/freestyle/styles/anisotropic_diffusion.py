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

#  Filename : anisotropic_diffusion.py
#  Author   : Fredo Durand
#  Date     : 12/08/2004
#  Purpose  : Smoothes lines using an anisotropic diffusion scheme

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    ExternalContourUP1D,
    NotUP1D,
    Operators,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    TrueUP1D,
    )
from freestyle.shaders import (
    ConstantThicknessShader,
    IncreasingColorShader,
    SamplingShader,
    StrokeTextureShader,
    pyDiffusion2Shader,
    )
from freestyle.types import Operators, Stroke


# pyDiffusion2Shader parameters
offset = 0.25
nbIter = 30

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(upred))
shaders_list = [
    ConstantThicknessShader(4),
    StrokeTextureShader("smoothAlpha.bmp", Stroke.OPAQUE_MEDIUM, False),
    SamplingShader(2),
    pyDiffusion2Shader(offset, nbIter),
    IncreasingColorShader(1, 0, 0, 1, 0, 1, 0, 1),
    ]
Operators.create(TrueUP1D(), shaders_list)
