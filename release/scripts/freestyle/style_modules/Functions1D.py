from freestyle_init import *
from Functions0D import *
import string 

class pyGetInverseProjectedZF1D(UnaryFunction1DDouble):
	def getName(self):
		return "pyGetInverseProjectedZF1D"

	def __call__(self, inter):
		func = GetProjectedZF1D()
		z = func(inter)
		return (1.0 - z)

class pyGetSquareInverseProjectedZF1D(UnaryFunction1DDouble):
	def getName(self):
		return "pyGetInverseProjectedZF1D"

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
	def getName(self):
		return "pyDensityAnisotropyF1D"
	def __call__(self, inter):
		v = integrate(self._func, inter.pointsBegin(self._sampling), inter.pointsEnd(self._sampling), self._integration)
		return v

class pyViewMapGradientNormF1D(UnaryFunction1DDouble):
	def __init__(self,l, integrationType, sampling=2.0):
		UnaryFunction1DDouble.__init__(self, integrationType)
		self._func = pyViewMapGradientNormF0D(l)
		self._integration = integrationType
		self._sampling = sampling
	def getName(self):
		return "pyViewMapGradientNormF1D"
	def __call__(self, inter):
		v = integrate(self._func, inter.pointsBegin(self._sampling), inter.pointsEnd(self._sampling), self._integration)
		return v
