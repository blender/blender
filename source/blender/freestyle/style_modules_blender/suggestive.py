#
#  Filename : suggestive.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws the suggestive contours.
#             ***** The suggestive contours must be enabled
#             in the options dialog *****
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
from PredicatesB1D import *
from PredicatesU1D import *
from shaders import *

upred = AndUP1D(pyNatureUP1D(Nature.SUGGESTIVE_CONTOUR), QuantitativeInvisibilityUP1D(0))
Operators.select(upred)
Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = 	[
		IncreasingThicknessShader(1, 3), 
		ConstantColorShader(0.2,0.2,0.2, 1)
		]
Operators.create(TrueUP1D(), shaders_list)
