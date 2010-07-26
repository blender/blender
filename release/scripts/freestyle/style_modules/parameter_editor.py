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

import Freestyle

from freestyle_init import *
from logical_operators import *
from ChainingIterators import *
from shaders import *

def process(layer_name, lineset_name):
    scene = Freestyle.getCurrentScene()
    layer = scene.render.layers[layer_name]
    lineset = layer.freestyle_settings.linesets[lineset_name]
    linestyle = lineset.linestyle

    color = linestyle.color

    upred = QuantitativeInvisibilityUP1D(0)
    Operators.select(upred)
    Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(upred))
    shaders_list = [
        SamplingShader(5.0),
        ConstantThicknessShader(linestyle.thickness),
        ConstantColorShader(color.r, color.g, color.b, linestyle.alpha)
        ]
    Operators.create(TrueUP1D(), shaders_list)
