#
#  Filename : occluded_by_specific_object.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws only the lines that are occluded by a given object
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
from PredicatesU1D import *
from shaders import *

## the id of the occluder (use SHIFT+click on the ViewMap to
## retrieve ids)
id = Id(3,0)
upred = AndUP1D(NotUP1D(QuantitativeInvisibilityUP1D(0)),
pyIsInOccludersListUP1D(id))
Operators.select(upred)
Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(upred))
shaders_list = 	[
		SamplingShader(5),
		ConstantThicknessShader(3), 
		ConstantColorShader(0.3,0.3,0.3,1)
		]
Operators.create(TrueUP1D(), shaders_list)
