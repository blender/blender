from freestyle_init import *
from logical_operators import *
from PredicatesU1D import *
from PredicatesU0D import *
from PredicatesB1D import *
from Functions0D import *
from Functions1D import *
from shaders import *

class pyDensityUP1D(UnaryPredicate1D):
	def __init__(self,wsize,threshold, integration = IntegrationType.MEAN, sampling=2.0):
		UnaryPredicate1D.__init__(self)
		self._wsize = wsize
		self._threshold = threshold
		self._integration = integration
		self._func = DensityF1D(self._wsize, self._integration, sampling)
			
	def getName(self):
		return "pyDensityUP1D"

	def __call__(self, inter):
		d = self._func(inter)
		print("For Chain ", inter.getId().getFirst(), inter.getId().getSecond(), "density is ", d)
		if(d < self._threshold):
			return 1
		return 0

Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectionalChain(ChainSilhouetteIterator())
#Operators.sequentialSplit(pyVertexNatureUP0D(Nature.VIEW_VERTEX), 2)
Operators.sort(pyZBP1D())
shaders_list = 	[
		StrokeTextureShader("smoothAlpha.bmp", Stroke.OPAQUE_MEDIUM, 0),
		ConstantThicknessShader(3), 
		SamplingShader(5.0),
		ConstantColorShader(0,0,0,1)
		]
Operators.create(pyDensityUP1D(2,0.05, IntegrationType.MEAN,4), shaders_list)
#Operators.create(pyDensityFunctorUP1D(8,0.03, pyGetInverseProjectedZF1D(), 0,1, IntegrationType.MEAN), shaders_list)

