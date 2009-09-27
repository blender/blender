from freestyle_init import *
from Functions1D import *

count = 0
class pyNFirstUP1D(UnaryPredicate1D):
	def __init__(self, n):
		UnaryPredicate1D.__init__(self)
		self.__n = n
	def __call__(self, inter):
		global count
		count = count + 1
		if count <= self.__n:
			return 1
		return 0

class pyHigherLengthUP1D(UnaryPredicate1D):
	def __init__(self,l):
		UnaryPredicate1D.__init__(self)
		self._l = l
			
	def getName(self):
		return "HigherLengthUP1D"

	def __call__(self, inter):
		return (inter.getLength2D() > self._l)

class pyNatureUP1D(UnaryPredicate1D):
	def __init__(self,nature):
		UnaryPredicate1D.__init__(self)
		self._nature = nature
		self._getNature = CurveNatureF1D()
			
	def getName(self):
		return "pyNatureUP1D"

	def __call__(self, inter):
		if(self._getNature(inter) & self._nature):
			return 1
		return 0

class pyHigherNumberOfTurnsUP1D(UnaryPredicate1D):
	def __init__(self,n,a):
		UnaryPredicate1D.__init__(self)
		self._n = n
		self._a = a

	def getName(self):
		return "HigherNumberOfTurnsUP1D"

	def __call__(self, inter):
		count = 0
		func = Curvature2DAngleF0D()
		it = inter.verticesBegin()
		while(it.isEnd() == 0):
			if(func(it) > self._a):
				count = count+1
			if(count > self._n):
				return 1
			it.increment()
		return 0

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
		if(self._func(inter) < self._threshold):
			return 1
		return 0

class pyLowSteerableViewMapDensityUP1D(UnaryPredicate1D):
	def __init__(self,threshold, level,integration = IntegrationType.MEAN):
		UnaryPredicate1D.__init__(self)
		self._threshold = threshold
		self._level = level
		self._integration = integration
			
	def getName(self):
		return "pyLowSteerableViewMapDensityUP1D"

	def __call__(self, inter):
		func = GetSteerableViewMapDensityF1D(self._level, self._integration)
		v = func(inter)
		print(v)
		if(v < self._threshold):
			return 1
		return 0

class pyLowDirectionalViewMapDensityUP1D(UnaryPredicate1D):
	def __init__(self,threshold, orientation, level,integration = IntegrationType.MEAN):
		UnaryPredicate1D.__init__(self)
		self._threshold = threshold
		self._orientation = orientation
		self._level = level
		self._integration = integration
			
	def getName(self):
		return "pyLowDirectionalViewMapDensityUP1D"

	def __call__(self, inter):
		func = GetDirectionalViewMapDensityF1D(self._orientation, self._level, self._integration)
		v = func(inter)
		#print(v)
		if(v < self._threshold):
			return 1
		return 0

class pyHighSteerableViewMapDensityUP1D(UnaryPredicate1D):
	def __init__(self,threshold, level,integration = IntegrationType.MEAN):
		UnaryPredicate1D.__init__(self)
		self._threshold = threshold
		self._level = level
		self._integration = integration
		self._func = GetSteerableViewMapDensityF1D(self._level, self._integration)	
	def getName(self):
		return "pyHighSteerableViewMapDensityUP1D"

	def __call__(self, inter):
		
		v = self._func(inter)
		if(v > self._threshold):
			return 1
		return 0

class pyHighDirectionalViewMapDensityUP1D(UnaryPredicate1D):
	def __init__(self,threshold, orientation, level,integration = IntegrationType.MEAN, sampling=2.0):
		UnaryPredicate1D.__init__(self)
		self._threshold = threshold
		self._orientation = orientation
		self._level = level
		self._integration = integration
		self._sampling = sampling		
	def getName(self):
		return "pyLowDirectionalViewMapDensityUP1D"

	def __call__(self, inter):
		func = GetDirectionalViewMapDensityF1D(self._orientation, self._level, self._integration, self._sampling)
		v = func(inter)
		if(v > self._threshold):
			return 1
		return 0

class pyHighViewMapDensityUP1D(UnaryPredicate1D):
	def __init__(self,threshold, level,integration = IntegrationType.MEAN, sampling=2.0):
		UnaryPredicate1D.__init__(self)
		self._threshold = threshold
		self._level = level
		self._integration = integration
		self._sampling = sampling
		self._func = GetCompleteViewMapDensityF1D(self._level, self._integration, self._sampling) # 2.0 is the smpling
			
	def getName(self):
		return "pyHighViewMapDensityUP1D"

	def __call__(self, inter):
		#print("toto")
		#print(func.getName())
		#print(inter.getExactTypeName())
		v= self._func(inter)
		if(v > self._threshold):
			return 1
		return 0

class pyDensityFunctorUP1D(UnaryPredicate1D):
	def __init__(self,wsize,threshold, functor, funcmin=0.0, funcmax=1.0, integration = IntegrationType.MEAN):
		UnaryPredicate1D.__init__(self)
		self._wsize = wsize
		self._threshold = float(threshold)
		self._functor = functor
		self._funcmin = float(funcmin)
		self._funcmax = float(funcmax)
		self._integration = integration
			
	def getName(self):
		return "pyDensityFunctorUP1D"

	def __call__(self, inter):
		func = DensityF1D(self._wsize, self._integration)
		res = self._functor(inter)
		k = (res-self._funcmin)/(self._funcmax-self._funcmin)
		if(func(inter) < self._threshold*k):
			return 1
		return 0

class pyZSmallerUP1D(UnaryPredicate1D):
	def __init__(self,z, integration=IntegrationType.MEAN):
		UnaryPredicate1D.__init__(self)
		self._z = z
		self._integration = integration
	def getName(self):
		return "pyZSmallerUP1D"

	def __call__(self, inter):
		func = GetProjectedZF1D(self._integration)
		if(func(inter) < self._z):
			return 1
		return 0

class pyIsOccludedByUP1D(UnaryPredicate1D):
	def __init__(self,id):
		UnaryPredicate1D.__init__(self)
		self._id = id
	def getName(self):
		return "pyIsOccludedByUP1D"
	def __call__(self, inter):
		func = GetShapeF1D()
		shapes = func(inter)
		for s in shapes:
			if(s.getId() == self._id):
				return 0
		it = inter.verticesBegin()
		itlast = inter.verticesEnd()
		itlast.decrement()
		v =  it.getObject()
		vlast = itlast.getObject()
		tvertex = v.viewvertex()
		if type(tvertex) is TVertex:
			print("TVertex: [ ", tvertex.getId().getFirst(), ",",  tvertex.getId().getSecond()," ]")
			eit = tvertex.edgesBegin()
			while(eit.isEnd() == 0):
				ve, incoming = eit.getObject()
				if(ve.getId() == self._id):
					return 1
				print("-------", ve.getId().getFirst(), "-", ve.getId().getSecond())
				eit.increment()
		tvertex = vlast.viewvertex()
		if type(tvertex) is TVertex:
			print("TVertex: [ ", tvertex.getId().getFirst(), ",",  tvertex.getId().getSecond()," ]")
			eit = tvertex.edgesBegin()
			while(eit.isEnd() == 0):
				ve, incoming = eit.getObject()
				if(ve.getId() == self._id):
					return 1
				print("-------", ve.getId().getFirst(), "-", ve.getId().getSecond())
				eit.increment()
		return 0

class pyIsInOccludersListUP1D(UnaryPredicate1D):
	def __init__(self,id):
		UnaryPredicate1D.__init__(self)
		self._id = id
	def getName(self):
		return "pyIsInOccludersListUP1D"
	def __call__(self, inter):
		func = GetOccludersF1D()
		occluders = func(inter)
		for a in occluders:
			if(a.getId() == self._id):
				return 1
		return 0

class pyIsOccludedByItselfUP1D(UnaryPredicate1D):
	def __init__(self):
		UnaryPredicate1D.__init__(self)
		self.__func1 = GetOccludersF1D()
		self.__func2 = GetShapeF1D()
	def getName(self):
		return "pyIsOccludedByItselfUP1D"
	def __call__(self, inter):
		lst1 = self.__func1(inter)
		lst2 = self.__func2(inter)
		for vs1 in lst1:
			for vs2 in lst2:
				if vs1.getId() == vs2.getId():
					return 1
		return 0

class pyIsOccludedByIdListUP1D(UnaryPredicate1D):
	def __init__(self, idlist):
		UnaryPredicate1D.__init__(self)
		self._idlist = idlist
		self.__func1 = GetOccludersF1D()
	def getName(self):
		return "pyIsOccludedByIdListUP1D"
	def __call__(self, inter):
		lst1 = self.__func1(inter)
		for vs1 in lst1:
			for id in self._idlist:
				if vs1.getId() == id:
					return 1
		return 0

class pyShapeIdListUP1D(UnaryPredicate1D):
	def __init__(self,idlist):
		UnaryPredicate1D.__init__(self)
		self._idlist = idlist
		self._funcs = []
		for id in idlist :
			self._funcs.append(ShapeUP1D(id.getFirst(), id.getSecond()))
		
	def getName(self):
		return "pyShapeIdUP1D"
	def __call__(self, inter):
		for func in self._funcs :
			if(func(inter) == 1) :
				return 1
		return 0

## deprecated
class pyShapeIdUP1D(UnaryPredicate1D):
	def __init__(self,id):
		UnaryPredicate1D.__init__(self)
		self._id = id
	def getName(self):
		return "pyShapeIdUP1D"
	def __call__(self, inter):
		func = GetShapeF1D()
		shapes = func(inter)
		for a in shapes:
			if(a.getId() == self._id):
				return 1
		return 0

class pyHighDensityAnisotropyUP1D(UnaryPredicate1D):
	def __init__(self,threshold, level, sampling=2.0):
		UnaryPredicate1D.__init__(self)
		self._l = threshold
		self.func = pyDensityAnisotropyF1D(level, IntegrationType.MEAN, sampling)
	def getName(self):
		return "pyHighDensityAnisotropyUP1D"
	def __call__(self, inter):
		return (self.func(inter) > self._l)

class pyHighViewMapGradientNormUP1D(UnaryPredicate1D):
	def __init__(self,threshold, l, sampling=2.0):
		UnaryPredicate1D.__init__(self)
		self._threshold = threshold
		self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)
	def getName(self):
		return "pyHighViewMapGradientNormUP1D"
	def __call__(self, inter):
		gn = self._GetGradient(inter)
		#print(gn)
		return (gn > self._threshold)

class pyDensityVariableSigmaUP1D(UnaryPredicate1D):
	def __init__(self,functor, sigmaMin,sigmaMax, lmin, lmax, tmin, tmax, integration = IntegrationType.MEAN, sampling=2.0):
		UnaryPredicate1D.__init__(self)
		self._functor = functor
		self._sigmaMin = float(sigmaMin)
		self._sigmaMax = float(sigmaMax)
		self._lmin = float(lmin)
		self._lmax = float(lmax)
		self._tmin = tmin
		self._tmax = tmax
		self._integration = integration
		self._sampling = sampling
			
	def getName(self):
		return "pyDensityUP1D"

	def __call__(self, inter):
		sigma = (self._sigmaMax-self._sigmaMin)/(self._lmax-self._lmin)*(self._functor(inter)-self._lmin) + self._sigmaMin
		t = (self._tmax-self._tmin)/(self._lmax-self._lmin)*(self._functor(inter)-self._lmin) + self._tmin
		if(sigma<self._sigmaMin):
			sigma = self._sigmaMin
		self._func = DensityF1D(sigma, self._integration, self._sampling)
		d = self._func(inter)
		if(d<t):
			return 1
		return 0

class pyClosedCurveUP1D(UnaryPredicate1D):
	def __call__(self, inter):
		it = inter.verticesBegin()
		itlast = inter.verticesEnd()
		itlast.decrement()	
		vlast = itlast.getObject()
		v = it.getObject()
		print(v.getId().getFirst(), v.getId().getSecond())
		print(vlast.getId().getFirst(), vlast.getId().getSecond())
		if(v.getId() == vlast.getId()):
			return 1
		return 0
