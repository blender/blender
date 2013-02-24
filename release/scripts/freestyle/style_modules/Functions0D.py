# ##### BEGIN GPL LICENSE BLOCK #####
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
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

from Freestyle import Curvature2DAngleF0D, CurvePoint, ReadCompleteViewMapPixelF0D, \
    ReadSteerableViewMapPixelF0D, UnaryFunction0DDouble, UnaryFunction0DMaterial, \
    UnaryFunction0DVec2f
from Freestyle import ContextFunctions as CF

import math
import mathutils

class CurveMaterialF0D(UnaryFunction0DMaterial):
	# A replacement of the built-in MaterialF0D for stroke creation.
	# MaterialF0D does not work with Curves and Strokes.
	def __call__(self, inter):
		cp = inter.object
		assert(isinstance(cp, CurvePoint))
		fe = cp.first_svertex.get_fedge(cp.second_svertex)
		assert(fe is not None)
		return fe.material if fe.is_smooth else fe.material_left

class pyInverseCurvature2DAngleF0D(UnaryFunction0DDouble):
	def __call__(self, inter):
		func = Curvature2DAngleF0D()
		c = func(inter)
		return (3.1415 - c)

class pyCurvilinearLengthF0D(UnaryFunction0DDouble):
	def __call__(self, inter):
		cp = inter.object		
		assert(isinstance(cp, CurvePoint))
		return cp.t2d

## estimate anisotropy of density
class pyDensityAnisotropyF0D(UnaryFunction0DDouble):
	def __init__(self,level):
		UnaryFunction0DDouble.__init__(self)
		self.IsoDensity = ReadCompleteViewMapPixelF0D(level)
		self.d0Density = ReadSteerableViewMapPixelF0D(0, level)
		self.d1Density = ReadSteerableViewMapPixelF0D(1, level)
		self.d2Density = ReadSteerableViewMapPixelF0D(2, level)
		self.d3Density = ReadSteerableViewMapPixelF0D(3, level)
	def __call__(self, inter):
		c_iso = self.IsoDensity(inter) 
		c_0 = self.d0Density(inter) 
		c_1 = self.d1Density(inter) 
		c_2 = self.d2Density(inter) 
		c_3 = self.d3Density(inter) 
		cMax = max(max(c_0,c_1), max(c_2,c_3))
		cMin = min(min(c_0,c_1), min(c_2,c_3))
		if c_iso == 0:
			v = 0
		else:
			v = (cMax-cMin)/c_iso
		return v

## Returns the gradient vector for a pixel 
## 	l
##	  the level at which one wants to compute the gradient
class pyViewMapGradientVectorF0D(UnaryFunction0DVec2f):
	def __init__(self, l):
		UnaryFunction0DVec2f.__init__(self)
		self._l = l
		self._step = math.pow(2,self._l)
	def __call__(self, iter):
		p = iter.object.point_2d
		gx = CF.read_complete_view_map_pixel(self._l, int(p.x+self._step), int(p.y)) - \
		    CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
		gy = CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y+self._step)) - \
		    CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
		return mathutils.Vector([gx, gy])

class pyViewMapGradientNormF0D(UnaryFunction0DDouble):
	def __init__(self, l):
		UnaryFunction0DDouble.__init__(self)
		self._l = l
		self._step = math.pow(2,self._l)
	def __call__(self, iter):
		p = iter.object.point_2d
		gx = CF.read_complete_view_map_pixel(self._l, int(p.x+self._step), int(p.y)) - \
		    CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
		gy = CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y+self._step)) - \
		    CF.read_complete_view_map_pixel(self._l, int(p.x), int(p.y))
		grad = mathutils.Vector([gx, gy])
		return grad.length
