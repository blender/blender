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

#  Filename : Functions1D.py
#  Authors  : Fredo Durand, Stephane Grabli, Francois Sillion, Emmanuel Turquin 
#  Date     : 08/04/2005
#  Purpose  : Functions (functors) to be used for 1D elements

from Freestyle import GetProjectedZF1D, IntegrationType, UnaryFunction1DDouble, integrate
from Functions0D import pyDensityAnisotropyF0D, pyViewMapGradientNormF0D
import string 

class pyGetInverseProjectedZF1D(UnaryFunction1DDouble):
	def __call__(self, inter):
		func = GetProjectedZF1D()
		z = func(inter)
		return (1.0 - z)

class pyGetSquareInverseProjectedZF1D(UnaryFunction1DDouble):
	def __call__(self, inter):
		func = GetProjectedZF1D()
		z = func(inter)
		return (1.0 - z*z)

class pyDensityAnisotropyF1D(UnaryFunction1DDouble):
	def __init__(self,level,  integrationType=IntegrationType.MEAN, sampling=2.0):
		UnaryFunction1DDouble.__init__(self, integrationType)
		self._func = pyDensityAnisotropyF0D(level)
		self._integration = integrationType
		self._sampling = sampling
	def __call__(self, inter):
		v = integrate(self._func, inter.pointsBegin(self._sampling), inter.pointsEnd(self._sampling), self._integration)
		return v

class pyViewMapGradientNormF1D(UnaryFunction1DDouble):
	def __init__(self,l, integrationType, sampling=2.0):
		UnaryFunction1DDouble.__init__(self, integrationType)
		self._func = pyViewMapGradientNormF0D(l)
		self._integration = integrationType
		self._sampling = sampling
	def __call__(self, inter):
		v = integrate(self._func, inter.pointsBegin(self._sampling), inter.pointsEnd(self._sampling), self._integration)
		return v
