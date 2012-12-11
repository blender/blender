#
#  Filename : sketchy_multiple_parameterization.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Builds sketchy strokes whose topology relies on a 
#             parameterization that covers the complete lines (visible+invisible)
#             whereas only the visible portions are actually drawn
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
from shaders import *


Operators.select(QuantitativeInvisibilityUP1D(0))
## 0: don't restrict to selection
Operators.bidirectionalChain(pySketchyChainSilhouetteIterator(3,0))
shaders_list = 	[
		SamplingShader(2),
		SpatialNoiseShader(15, 120, 2, 1, 1), 
		IncreasingThicknessShader(5, 30), 
		SmoothingShader(100, 0.05, 0, 0.2, 0, 0, 0, 1),
		IncreasingColorShader(0,0.2,0,1,0.2,0.7,0.2,1),
		TextureAssignerShader(6),
		pyHLRShader()
		]
Operators.create(TrueUP1D(), shaders_list)
