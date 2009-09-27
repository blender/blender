from freestyle_init import *
from Functions1D import *
from random import *

class pyZBP1D(BinaryPredicate1D):
	def getName(self):
		return "pyZBP1D"

	def __call__(self, i1, i2):
		func = GetZF1D()
		return (func(i1) > func(i2))

class pyZDiscontinuityBP1D(BinaryPredicate1D):
	def __init__(self, iType = IntegrationType.MEAN):
		BinaryPredicate1D.__init__(self)
		self._GetZDiscontinuity = ZDiscontinuityF1D(iType)

	def getName(self):
		return "pyZDiscontinuityBP1D"

	def __call__(self, i1, i2):
		return (self._GetZDiscontinuity(i1) > self._GetZDiscontinuity(i2))

class pyLengthBP1D(BinaryPredicate1D):
	def getName(self):
		return "LengthBP1D"

	def __call__(self, i1, i2):
		return (i1.getLength2D() > i2.getLength2D())

class pySilhouetteFirstBP1D(BinaryPredicate1D):
	def getName(self):
		return "SilhouetteFirstBP1D"

	def __call__(self, inter1, inter2):
		bpred = SameShapeIdBP1D()
		if (bpred(inter1, inter2) != 1):
			return 0
		if (inter1.getNature() & Nature.SILHOUETTE):
			return (inter2.getNature() & Nature.SILHOUETTE)
		return (inter1.getNature() == inter2.getNature())

class pyNatureBP1D(BinaryPredicate1D):
	def getName(self):
		return "NatureBP1D"

	def __call__(self, inter1, inter2):
		return (inter1.getNature() & inter2.getNature())

class pyViewMapGradientNormBP1D(BinaryPredicate1D):
	def __init__(self,l, sampling=2.0):
		BinaryPredicate1D.__init__(self)
		self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)
	def getName(self):
		return "pyViewMapGradientNormBP1D"
	def __call__(self, i1,i2):
		print("compare gradient")
		return (self._GetGradient(i1) > self._GetGradient(i2))

class pyShuffleBP1D(BinaryPredicate1D):
	def __init__(self):
		BinaryPredicate1D.__init__(self)
		seed(1)
	def getName(self):
		return "pyNearAndContourFirstBP1D"

	def __call__(self, inter1, inter2):
		r1 = uniform(0,1)
		r2 = uniform(0,1)
		return (r1<r2)
