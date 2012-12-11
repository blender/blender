#
#  Filename : contour.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Draws each object's visible contour
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

Operators.select(AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D() ) )
bpred = SameShapeIdBP1D();
upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
Operators.bidirectionalChain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list =	 [
		ConstantThicknessShader(5.0),
		IncreasingColorShader(0.8,0,0,1,0.1,0,0,1)
		]
Operators.create(TrueUP1D(), shaders_list)
