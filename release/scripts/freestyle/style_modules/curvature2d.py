#
#  Filename : curvature2d.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The stroke points are colored in gray levels and depending
#             on the 2d curvature value
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
from shaders import *

class py2DCurvatureColorShader(StrokeShader):
	def getName(self):
		return "py2DCurvatureColorShader"

	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		func = Curvature2DAngleF0D()
		while it.isEnd() == 0:
			it0D = it.castToInterface0DIterator()
			sv = it.getObject()
			att = sv.attribute()
			c = func(it0D)
			if (c<0):
 				print("negative 2D curvature")
			color = 10.0 * c/3.1415
			att.setColor(color,color,color);
			it.increment()

Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectionalChain(ChainSilhouetteIterator(), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = 	[
		StrokeTextureShader("smoothAlpha.bmp", Stroke.OPAQUE_MEDIUM, 0),
		ConstantThicknessShader(5),
		py2DCurvatureColorShader()
		]
Operators.create(TrueUP1D(), shaders_list)
