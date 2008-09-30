#
#  Filename : sketchy_topology_preserved.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The topology of the strokes is built
#             so as to chain several times the same ViewEdge.
#             The topology of the objects is preserved
#
#############################################################################  
#
#  Copyright (C) : Please refer to the COPYRIGHT file distributed 
#  with this source distribution. 
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
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
#############################################################################

from freestyle_init import *
from logical_operators import *
from ChainingIterators import *
from PredicatesU1D import *
from shaders import *

upred = QuantitativeInvisibilityUP1D(0)
Operators.select(upred)
Operators.bidirectionalChain(pySketchyChainSilhouetteIterator(3,1))
shaders_list = 	[
		SamplingShader(4),
		SpatialNoiseShader(20, 220, 2, 1, 1), 
		IncreasingThicknessShader(4, 8), 
		SmoothingShader(300, 0.05, 0, 0.2, 0, 0, 0, 0.5),
		ConstantColorShader(0.6,0.2,0.0),
		TextureAssignerShader(4),
		]

Operators.create(TrueUP1D(), shaders_list)

