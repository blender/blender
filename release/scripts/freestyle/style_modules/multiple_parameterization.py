#
#  Filename : multiple_parameterization.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The thickness and the color of the strokes vary continuously 
#             independently from occlusions although only
#             visible lines are actually drawn. This is equivalent
#             to assigning the thickness using a parameterization covering
#             the complete silhouette (visible+invisible) and drawing
#             the strokes using a second parameterization that only
#             covers the visible portions.
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
## Chain following the same nature, but without the restriction
## of staying inside the selection (0).
Operators.bidirectionalChain(ChainSilhouetteIterator(0))
shaders_list = 	[
		SamplingShader(20),
		IncreasingThicknessShader(1.5, 30), 
		ConstantColorShader(0.0,0.0,0.0),
		IncreasingColorShader(1,0,0,1,0,1,0,1),
		TextureAssignerShader(-1),
		pyHLRShader() ## this shader draws only visible portions
		]
Operators.create(TrueUP1D(), shaders_list)
