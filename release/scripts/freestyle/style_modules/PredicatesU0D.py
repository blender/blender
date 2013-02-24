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

from Freestyle import Curvature2DAngleF0D, Nature, QuantitativeInvisibilityF0D, UnaryPredicate0D
from Functions0D import pyCurvilinearLengthF0D

class pyHigherCurvature2DAngleUP0D(UnaryPredicate0D):
	def __init__(self,a):
		UnaryPredicate0D.__init__(self)
		self._a = a
	def __call__(self, inter):
		func = Curvature2DAngleF0D()
		a = func(inter)
		return (a > self._a)

class pyUEqualsUP0D(UnaryPredicate0D):
	def __init__(self,u, w):
		UnaryPredicate0D.__init__(self)
		self._u = u
		self._w = w
	def __call__(self, inter):
		func = pyCurvilinearLengthF0D()
		u = func(inter)
		return (u > (self._u-self._w)) and (u < (self._u+self._w))

class pyVertexNatureUP0D(UnaryPredicate0D):
	def __init__(self,nature):
		UnaryPredicate0D.__init__(self)
		self._nature = nature
	def __call__(self, inter):
		v = inter.object
		return (v.nature & self._nature) != 0

## check whether an Interface0DIterator
## is a TVertex and is the one that is 
## hidden (inferred from the context)
class pyBackTVertexUP0D(UnaryPredicate0D):
	def __init__(self):
		UnaryPredicate0D.__init__(self)
		self._getQI = QuantitativeInvisibilityF0D()
	def __call__(self, iter):
		if (iter.object.nature & Nature.T_VERTEX) == 0:
			return 0
		if iter.is_end:
			return 0
		if self._getQI(iter) != 0:
			return 1
		return 0

class pyParameterUP0DGoodOne(UnaryPredicate0D):
	def __init__(self,pmin,pmax):
		UnaryPredicate0D.__init__(self)
		self._m = pmin
		self._M = pmax
		#self.getCurvilinearAbscissa = GetCurvilinearAbscissaF0D()
	def __call__(self, inter):
		#s = self.getCurvilinearAbscissa(inter)
		u = inter.u
		#print(u)
		return ((u>=self._m) and (u<=self._M))

class pyParameterUP0D(UnaryPredicate0D):
	def __init__(self,pmin,pmax):
		UnaryPredicate0D.__init__(self)
		self._m = pmin
		self._M = pmax
		#self.getCurvilinearAbscissa = GetCurvilinearAbscissaF0D()
	def __call__(self, inter):
		func = Curvature2DAngleF0D()
		c = func(inter)
		b1 = (c>0.1)
		#s = self.getCurvilinearAbscissa(inter)
		u = inter.u
		#print(u)
		b = ((u>=self._m) and (u<=self._M))
		return b and b1
