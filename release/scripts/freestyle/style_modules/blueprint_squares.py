#  Filename : blueprint_squares.py
#  Author   : Emmanuel Turquin
#  Date     : 04/08/2005
#  Purpose  : Produces a blueprint using square contour strokes
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

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ContourUP1D())
bpred = SameShapeIdBP1D()
Operators.select(upred)
Operators.bidirectionalChain(ChainPredicateIterator(upred,bpred), NotUP1D(upred))
Operators.select(pyHigherLengthUP1D(200))
shaders_list = [
		ConstantThicknessShader(8),
                pyBluePrintSquaresShader(2, 20),
		pyPerlinNoise1DShader(0.07, 10, 8),
		TextureAssignerShader(4),
		IncreasingColorShader(0.6, 0.3, 0.3, 0.7, 0.6, 0.3, 0.3, 0.3),
		ConstantThicknessShader(4)
               ]
Operators.create(TrueUP1D(), shaders_list)
