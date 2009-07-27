#
#  Filename : anisotropic_diffusion.py
#  Author   : Fredo Durand
#  Date     : 12/08/2004
#  Purpose  : Smoothes lines using an anisotropic diffusion scheme
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
from PredicatesU0D import *
from math import *

## thickness modifiers

normalInfo=Normal2DF0D()
curvatureInfo=Curvature2DAngleF0D()

def edgestopping(x, sigma): 
	return exp(- x*x/(2*sigma*sigma))

class pyDiffusion2Shader(StrokeShader):
	def __init__(self, lambda1, nbIter):
		StrokeShader.__init__(self)
		self._lambda = lambda1
		self._nbIter = nbIter
	def getName(self):
		return "pyDiffusionShader"
	def shade(self, stroke):
		for i in range (1, self._nbIter):
			it = stroke.strokeVerticesBegin()
			while it.isEnd() == 0:
				v=it.getObject()
				p1 = v.getPoint()
				p2 = normalInfo(it.castToInterface0DIterator())*self._lambda*curvatureInfo(it.castToInterface0DIterator())
				v.setPoint(p1+p2)
				it.increment()

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), ExternalContourUP1D())
Operators.select( upred )
bpred = TrueBP1D();
Operators.bidirectionalChain(ChainPredicateIterator(upred, bpred), NotUP1D(upred) )
shaders_list = 	[
		ConstantThicknessShader(4),
		StrokeTextureShader("smoothAlpha.bmp", Stroke.OPAQUE_MEDIUM, 0),
		SamplingShader(2),
		pyDiffusion2Shader(-0.03, 30), 
		IncreasingColorShader(1.0,0.0,0.0,1, 0, 1, 0, 1)
		]
Operators.create(TrueUP1D(), shaders_list)



