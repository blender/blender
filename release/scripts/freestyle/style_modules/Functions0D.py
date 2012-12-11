from freestyle_init import *

class CurveMaterialF0D(UnaryFunction0DMaterial):
	# A replacement of the built-in MaterialF0D for stroke creation.
	# MaterialF0D does not work with Curves and Strokes.
	def getName(self):
		return "CurveMaterialF0D"
	def __call__(self, inter):
		cp = inter.getObject()
		assert(isinstance(cp, CurvePoint))
		fe = cp.A().getFEdge(cp.B())
		assert(fe is not None)
		return fe.material() if fe.isSmooth() else fe.bMaterial()

class pyInverseCurvature2DAngleF0D(UnaryFunction0DDouble):
	def getName(self):
		return "InverseCurvature2DAngleF0D"

	def __call__(self, inter):
		func = Curvature2DAngleF0D()
		c = func(inter)
		return (3.1415 - c)

class pyCurvilinearLengthF0D(UnaryFunction0DDouble):
	def getName(self):
		return "CurvilinearLengthF0D"

	def __call__(self, inter):
		i0d = inter.getObject()
		s = i0d.getExactTypeName()
		if (string.find(s, "CurvePoint") == -1):
			print("CurvilinearLengthF0D: not implemented yet for", s)
			return -1
		cp = castToCurvePoint(i0d)
		return cp.t2d()

## estimate anisotropy of density
class pyDensityAnisotropyF0D(UnaryFunction0DDouble):
	def __init__(self,level):
		UnaryFunction0DDouble.__init__(self)
		self.IsoDensity = ReadCompleteViewMapPixelF0D(level)
		self.d0Density = ReadSteerableViewMapPixelF0D(0, level)
		self.d1Density = ReadSteerableViewMapPixelF0D(1, level)
		self.d2Density = ReadSteerableViewMapPixelF0D(2, level)
		self.d3Density = ReadSteerableViewMapPixelF0D(3, level)
	def getName(self):
		return "pyDensityAnisotropyF0D"
	def __call__(self, inter):
		c_iso = self.IsoDensity(inter) 
		c_0 = self.d0Density(inter) 
		c_1 = self.d1Density(inter) 
		c_2 = self.d2Density(inter) 
		c_3 = self.d3Density(inter) 
		cMax = max( max(c_0,c_1), max(c_2,c_3))
		cMin = min( min(c_0,c_1), min(c_2,c_3))
		if ( c_iso == 0 ):
			v = 0
		else:
			v = (cMax-cMin)/c_iso
		return (v)

## Returns the gradient vector for a pixel 
## 	l
##	  the level at which one wants to compute the gradient
class pyViewMapGradientVectorF0D(UnaryFunction0DVec2f):
	def __init__(self, l):
		UnaryFunction0DVec2f.__init__(self)
		self._l = l
		self._step = pow(2,self._l)
	def getName(self):
		return "pyViewMapGradientVectorF0D"
	def __call__(self, iter):
		p = iter.getObject().getPoint2D()
		gx = ReadCompleteViewMapPixelCF(self._l, int(p.x()+self._step), int(p.y()))- ReadCompleteViewMapPixelCF(self._l, int(p.x()), int(p.y()))
		gy = ReadCompleteViewMapPixelCF(self._l, int(p.x()), int(p.y()+self._step))- ReadCompleteViewMapPixelCF(self._l, int(p.x()), int(p.y()))
		return Vector([gx, gy])

class pyViewMapGradientNormF0D(UnaryFunction0DDouble):
	def __init__(self, l):
		UnaryFunction0DDouble.__init__(self)
		self._l = l
		self._step = pow(2,self._l)
	def getName(self):
		return "pyViewMapGradientNormF0D"
	def __call__(self, iter):
		p = iter.getObject().getPoint2D()
		gx = ReadCompleteViewMapPixelCF(self._l, int(p.x()+self._step), int(p.y()))- ReadCompleteViewMapPixelCF(self._l, int(p.x()), int(p.y()))
		gy = ReadCompleteViewMapPixelCF(self._l, int(p.x()), int(p.y()+self._step))- ReadCompleteViewMapPixelCF(self._l, int(p.x()), int(p.y()))
		grad = Vector([gx, gy])
		return grad.length


