from freestyle_init import *
from Functions0D import *

class pyHigherCurvature2DAngleUP0D(UnaryPredicate0D):
	def __init__(self,a):
		UnaryPredicate0D.__init__(self)
		self._a = a
			
	def getName(self):
		return "HigherCurvature2DAngleUP0D"

	def __call__(self, inter):
		func = Curvature2DAngleF0D()
		a = func(inter)
		return ( a > self._a)

class pyUEqualsUP0D(UnaryPredicate0D):
	def __init__(self,u, w):
		UnaryPredicate0D.__init__(self)
		self._u = u
		self._w = w
			
	def getName(self):
		return "UEqualsUP0D"

	def __call__(self, inter):
		func = pyCurvilinearLengthF0D()
		u = func(inter)
		return ( ( u > (self._u-self._w) ) and ( u < (self._u+self._w) ) )

class pyVertexNatureUP0D(UnaryPredicate0D):
	def __init__(self,nature):
		UnaryPredicate0D.__init__(self)
		self._nature = nature
			
	def getName(self):
		return "pyVertexNatureUP0D"

	def __call__(self, inter):
		v = inter.getObject()
		nat = v.getNature()
		if(nat & self._nature):
			return 1;
		return 0

## check whether an Interface0DIterator
## is a TVertex and is the one that is 
## hidden (inferred from the context)
class pyBackTVertexUP0D(UnaryPredicate0D):
	def __init__(self):
		UnaryPredicate0D.__init__(self)
		self._getQI = QuantitativeInvisibilityF0D()
	def getName(self):
		return "pyBackTVertexUP0D"
	def __call__(self, iter):
		v = iter.getObject()
		nat = v.getNature()
		if(nat & Nature.T_VERTEX == 0):
			return 0
		next = iter
		if(next.isEnd()):
			return 0
		if(self._getQI(next) != 0):
			return 1
		return 0

class pyParameterUP0DGoodOne(UnaryPredicate0D):
	def __init__(self,pmin,pmax):
		UnaryPredicate0D.__init__(self)
		self._m = pmin
		self._M = pmax
		#self.getCurvilinearAbscissa = GetCurvilinearAbscissaF0D()

	def getName(self):
		return "pyCurvilinearAbscissaHigherThanUP0D"

	def __call__(self, inter):
		#s = self.getCurvilinearAbscissa(inter)
		u = inter.u()
		#print(u)
		return ((u>=self._m) and (u<=self._M))

class pyParameterUP0D(UnaryPredicate0D):
	def __init__(self,pmin,pmax):
		UnaryPredicate0D.__init__(self)
		self._m = pmin
		self._M = pmax
		#self.getCurvilinearAbscissa = GetCurvilinearAbscissaF0D()

	def getName(self):
		return "pyCurvilinearAbscissaHigherThanUP0D"

	def __call__(self, inter):
		func = Curvature2DAngleF0D()
		c = func(inter)
		b1 = (c>0.1)
		#s = self.getCurvilinearAbscissa(inter)
		u = inter.u()
		#print(u)
		b = ((u>=self._m) and (u<=self._M))
		return b and b1


