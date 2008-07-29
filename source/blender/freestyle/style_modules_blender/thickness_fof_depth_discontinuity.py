#
#  Filename : thickness_fof_depth_discontinuity.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Assigns to strokes a thickness that depends on the depth discontinuity
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

class pyDepthDiscontinuityThicknessShader(StrokeShader):
	def __init__(self, min, max):
		StrokeShader.__init__(self)
		self.__min = float(min)
		self.__max = float(max)
		self.__func = ZDiscontinuityF0D()
	def getName(self):
		return "pyDepthDiscontinuityThicknessShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		z_min=0.0
		z_max=1.0
		a = (self.__max - self.__min)/(z_max-z_min)
		b = (self.__min*z_max-self.__max*z_min)/(z_max-z_min)
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			z = self.__func(it.castToInterface0DIterator())
			thickness = a*z+b
			it.getObject().attribute().setThickness(thickness, thickness)
			it.increment()

Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = 	[
		SamplingShader(1),
		ConstantThicknessShader(3), 
		ConstantColorShader(0.0,0.0,0.0),
		pyDepthDiscontinuityThicknessShader(0.8, 6)
		]
Operators.create(TrueUP1D(), shaders_list)