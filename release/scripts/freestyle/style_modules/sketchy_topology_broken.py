#
#  Filename : sketchy_topology_broken.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : The topology of the strokes is, first, built
#             independantly from the 3D topology of objects, 
#             and, second, so as to chain several times the same ViewEdge.
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

## Backbone stretcher that leaves cusps intact to avoid cracks
class pyBackboneStretcherNoCuspShader(StrokeShader):
	def __init__(self, l):
		StrokeShader.__init__(self)
		self._l = l
	def getName(self):
		return "pyBackboneStretcherNoCuspShader"
	def shade(self, stroke):
		it0 = stroke.strokeVerticesBegin()
		it1 = StrokeVertexIterator(it0)
		it1.increment()
		itn = stroke.strokeVerticesEnd()
		itn.decrement()
		itn_1 = StrokeVertexIterator(itn)
		itn_1.decrement()
		v0 = it0.getObject()
		v1 = it1.getObject()
		if((v0.getNature() & Nature.CUSP == 0) and (v1.getNature() & Nature.CUSP == 0)):
			p0 = v0.getPoint()
			p1 = v1.getPoint()
			d1 = p0-p1
			d1.normalize()
			newFirst = p0+d1*float(self._l)
			v0.setPoint(newFirst)
		else:
			print("got a v0 cusp")
		vn_1 = itn_1.getObject()
		vn = itn.getObject()
		if((vn.getNature() & Nature.CUSP == 0) and (vn_1.getNature() & Nature.CUSP == 0)):
			pn = vn.getPoint()
			pn_1 = vn_1.getPoint()
			dn = pn-pn_1
			dn.normalize()
			newLast = pn+dn*float(self._l)	
			vn.setPoint(newLast)
		else:
			print("got a vn cusp")


Operators.select(QuantitativeInvisibilityUP1D(0))
## Chain 3 times each ViewEdge indpendantly from the 
## initial objects topology
Operators.bidirectionalChain(pySketchyChainingIterator(3))
shaders_list = 	[
		SamplingShader(4),
		SpatialNoiseShader(6, 120, 2, 1, 1), 
		IncreasingThicknessShader(4, 10), 
		SmoothingShader(100, 0.1, 0, 0.2, 0, 0, 0, 1),
		pyBackboneStretcherNoCuspShader(20),
		#ConstantColorShader(0.0,0.0,0.0)
		IncreasingColorShader(0.2,0.2,0.2,1,0.5,0.5,0.5,1),
		#IncreasingColorShader(1,0,0,1,0,1,0,1),
		TextureAssignerShader(4)
		]
Operators.create(TrueUP1D(), shaders_list)
