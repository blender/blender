#
#  Filename : qi0_not_external_contour.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the visible lines (chaining follows same nature lines)
#             that do not belong to the external contour of the scene
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

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select(upred)
Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = 	[
		SamplingShader(4),
		SpatialNoiseShader(4, 150, 2, True, True), 
		IncreasingThicknessShader(2, 5), 
		BackboneStretcherShader(20),
		IncreasingColorShader(1,0,0,1,0,1,0,1),
		TextureAssignerShader(4)
		]
Operators.create(TrueUP1D(), shaders_list)
