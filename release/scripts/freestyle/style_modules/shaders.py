from freestyle_init import *
from PredicatesU0D import *
from PredicatesB1D import *
from PredicatesU1D import *
from logical_operators import *
from ChainingIterators import *
from random import *
from math import *

## thickness modifiers
######################

class pyDepthDiscontinuityThicknessShader(StrokeShader):
	def __init__(self, min, max):
		StrokeShader.__init__(self)
		self.__min = float(min)
		self.__max = float(max)
		self.__func = ZDiscontinuityF0D()
	def getName(self):
		return "pyDepthDiscontinuityThicknessShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		z_min=0.0
		z_max=1.0
		a = (self.__max - self.__min)/(z_max-z_min)
		b = (self.__min*z_max-self.__max*z_min)/(z_max-z_min)
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			z = self.__func(it.castToInterface0DIterator())
			thickness = a*z+b
			it.getObject().attribute().setThickness(thickness, thickness)
			it.increment()

class pyConstantThicknessShader(StrokeShader):
	def __init__(self, thickness):
		StrokeShader.__init__(self)
		self._thickness = thickness

	def getName(self):
		return "pyConstantThicknessShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			t = self._thickness/2.0
			att.setThickness(t, t)
			it.increment()

class pyFXSThicknessShader(StrokeShader):
	def __init__(self, thickness):
		StrokeShader.__init__(self)
		self._thickness = thickness

	def getName(self):
		return "pyFXSThicknessShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			t = self._thickness/2.0
			att.setThickness(t, t)
			it.increment()

class pyFXSVaryingThicknessWithDensityShader(StrokeShader):
	def __init__(self, wsize, threshold_min, threshold_max, thicknessMin, thicknessMax):
		StrokeShader.__init__(self)
		self.wsize= wsize
		self.threshold_min= threshold_min
		self.threshold_max= threshold_max
		self._thicknessMin = thicknessMin
		self._thicknessMax = thicknessMax

	def getName(self):
		return "pyVaryingThicknessWithDensityShader"
	def shade(self, stroke):
		n = stroke.strokeVerticesSize()
		i = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		func = DensityF0D(self.wsize)
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			toto = it.castToInterface0DIterator()
			c= func(toto)
			if (c < self.threshold_min ):
				c = self.threshold_min
			if (c > self.threshold_max ):
				c = self.threshold_max
##			t = (c - self.threshold_min)/(self.threshold_max - self.threshold_min)*(self._thicknessMax-self._thicknessMin) + self._thicknessMin
			t = (self.threshold_max - c  )/(self.threshold_max - self.threshold_min)*(self._thicknessMax-self._thicknessMin) + self._thicknessMin
			att.setThickness(t/2.0, t/2.0)
			i = i+1
			it.increment()
class pyIncreasingThicknessShader(StrokeShader):
	def __init__(self, thicknessMin, thicknessMax):
		StrokeShader.__init__(self)
		self._thicknessMin = thicknessMin
		self._thicknessMax = thicknessMax

	def getName(self):
		return "pyIncreasingThicknessShader"
	def shade(self, stroke):
		n = stroke.strokeVerticesSize()
		i = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			c = float(i)/float(n)
			if(i < float(n)/2.0):
				t = (1.0 - c)*self._thicknessMin + c * self._thicknessMax
			else:
				t = (1.0 - c)*self._thicknessMax + c * self._thicknessMin
			att.setThickness(t/2.0, t/2.0)
			i = i+1
			it.increment()

class pyConstrainedIncreasingThicknessShader(StrokeShader):
	def __init__(self, thicknessMin, thicknessMax, ratio):
		StrokeShader.__init__(self)
		self._thicknessMin = thicknessMin
		self._thicknessMax = thicknessMax
		self._ratio = ratio

	def getName(self):
		return "pyConstrainedIncreasingThicknessShader"
	def shade(self, stroke):
		slength = stroke.getLength2D()
		tmp = self._ratio*slength
		maxT = 0.0
		if(tmp < self._thicknessMax):
			maxT = tmp
		else:
			maxT = self._thicknessMax
		n = stroke.strokeVerticesSize()
		i = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			c = float(i)/float(n)
			if(i < float(n)/2.0):
				t = (1.0 - c)*self._thicknessMin + c * maxT
			else:
				t = (1.0 - c)*maxT + c * self._thicknessMin
			att.setThickness(t/2.0, t/2.0)
			if(i == n-1):
				att.setThickness(self._thicknessMin/2.0, self._thicknessMin/2.0)
			i = i+1
			it.increment()

class pyDecreasingThicknessShader(StrokeShader):
	def __init__(self, thicknessMax, thicknessMin):
		StrokeShader.__init__(self)
		self._thicknessMin = thicknessMin
		self._thicknessMax = thicknessMax

	def getName(self):
		return "pyDecreasingThicknessShader"
	def shade(self, stroke):
		l = stroke.getLength2D()
		tMax = self._thicknessMax
		if(self._thicknessMax > 0.33*l):
			tMax = 0.33*l
		tMin = self._thicknessMin
		if(self._thicknessMin > 0.1*l):
			tMin = 0.1*l
		n = stroke.strokeVerticesSize()
		i = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			c = float(i)/float(n)
			t = (1.0 - c)*tMax +c*tMin
			att.setThickness(t/2.0, t/2.0)
			i = i+1
			it.increment()

def smoothC( a, exp ):
	c = pow(float(a),exp)*pow(2.0,exp)
	return c

class pyNonLinearVaryingThicknessShader(StrokeShader):
	def __init__(self, thicknessExtremity, thicknessMiddle, exponent):
		StrokeShader.__init__(self)
		self._thicknessMin = thicknessMiddle
		self._thicknessMax = thicknessExtremity
		self._exponent = exponent

	def getName(self):
		return "pyNonLinearVaryingThicknessShader"
	def shade(self, stroke):
		n = stroke.strokeVerticesSize()
		i = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			if(i < float(n)/2.0):
				c = float(i)/float(n)
			else:
				c = float(n-i)/float(n)
			c = smoothC(c, self._exponent)
			t = (1.0 - c)*self._thicknessMax + c * self._thicknessMin
			att.setThickness(t/2.0, t/2.0)
			i = i+1
			it.increment()

## Spherical linear interpolation (cos)
class pySLERPThicknessShader(StrokeShader):
	def __init__(self, thicknessMin, thicknessMax, omega=1.2):
		StrokeShader.__init__(self)
		self._thicknessMin = thicknessMin
		self._thicknessMax = thicknessMax
		self._omega = omega

	def getName(self):
		return "pySLERPThicknessShader"
	def shade(self, stroke):
		slength = stroke.getLength2D()
		tmp = 0.33*slength
		maxT = self._thicknessMax
		if(tmp < self._thicknessMax):
			maxT = tmp
		
		n = stroke.strokeVerticesSize()
		i = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			c = float(i)/float(n)
			if(i < float(n)/2.0):
				t = sin((1-c)*self._omega)/sinh(self._omega)*self._thicknessMin + sin(c*self._omega)/sinh(self._omega) * maxT
			else:
				t = sin((1-c)*self._omega)/sinh(self._omega)*maxT + sin(c*self._omega)/sinh(self._omega) * self._thicknessMin
			att.setThickness(t/2.0, t/2.0)
			i = i+1
			it.increment()

class pyTVertexThickenerShader(StrokeShader): ## FIXME
	def __init__(self, a=1.5, n=3):
		StrokeShader.__init__(self)
		self._a = a
		self._n = n

	def getName(self):
		return "pyTVertexThickenerShader"

	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		predTVertex = pyVertexNatureUP0D(Nature.T_VERTEX)
		while it.isEnd() == 0:
			if(predTVertex(it) == 1):
				it2 = StrokeVertexIterator(it)
				it2.increment()
				if not(it.isBegin() or it2.isEnd()):
					it.increment()
					continue
				n = self._n
				a = self._a
				if(it.isBegin()):
					it3 = StrokeVertexIterator(it)
					count = 0
					while (it3.isEnd() == 0 and count < n):
						att = it3.getObject().attribute()
						tr = att.getThicknessR();
						tl = att.getThicknessL();
						r = (a-1.0)/float(n-1)*(float(n)/float(count+1) - 1) + 1
						#r = (1.0-a)/float(n-1)*count + a
						att.setThickness(r*tr, r*tl)	
						it3.increment()
						count = count + 1
				if(it2.isEnd()):
					it4 = StrokeVertexIterator(it)
					count = 0
					while (it4.isBegin() == 0 and count < n):
						att = it4.getObject().attribute()
						tr = att.getThicknessR();
						tl = att.getThicknessL();
						r = (a-1.0)/float(n-1)*(float(n)/float(count+1) - 1) + 1
						#r = (1.0-a)/float(n-1)*count + a
						att.setThickness(r*tr, r*tl)	
						it4.decrement()
						count = count + 1
					if ((it4.isBegin() == 1)):
						att = it4.getObject().attribute()
						tr = att.getThicknessR();
						tl = att.getThicknessL();
						r = (a-1.0)/float(n-1)*(float(n)/float(count+1) - 1) + 1
						#r = (1.0-a)/float(n-1)*count + a
						att.setThickness(r*tr, r*tl)	
			it.increment()

class pyImportance2DThicknessShader(StrokeShader):
	def __init__(self, x, y, w, kmin, kmax):
		StrokeShader.__init__(self)
		self._x = x
		self._y = y
		self._w = float(w)
		self._kmin = float(kmin)
		self._kmax = float(kmax)

	def getName(self):
		return "pyImportanceThicknessShader"
	def shade(self, stroke):
		origin = Vector([self._x, self._y])
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			v = it.getObject()
			p = Vector([v.getProjectedX(), v.getProjectedY()])
			d = (p-origin).length
			if(d>self._w):
				k = self._kmin
			else:
				k = (self._kmax*(self._w-d) + self._kmin*d)/self._w
			att = v.attribute()
			tr = att.getThicknessR()
			tl = att.getThicknessL()
			att.setThickness(k*tr/2.0, k*tl/2.0)
			it.increment()

class pyImportance3DThicknessShader(StrokeShader):
	def __init__(self, x, y, z, w, kmin, kmax):
		StrokeShader.__init__(self)
		self._x = x
		self._y = y
		self._z = z
		self._w = float(w)
		self._kmin = float(kmin)
		self._kmax = float(kmax)

	def getName(self):
		return "pyImportance3DThicknessShader"
	def shade(self, stroke):
		origin = Vector([self._x, self._y, self._z])
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			v = it.getObject()
			p = Vector([v.getX(), v.getY(), v.getZ()])
			d = (p-origin).length
			if(d>self._w):
				k = self._kmin
			else:
				k = (self._kmax*(self._w-d) + self._kmin*d)/self._w
			att = v.attribute()
			tr = att.getThicknessR()
			tl = att.getThicknessL()
			att.setThickness(k*tr/2.0, k*tl/2.0)
			it.increment()

class pyZDependingThicknessShader(StrokeShader):
	def __init__(self, min, max):
		StrokeShader.__init__(self)
		self.__min = min
		self.__max = max
		self.__func = GetProjectedZF0D()
	def getName(self):
		return "pyZDependingThicknessShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		z_min = 1
		z_max = 0
		while it.isEnd() == 0:
			z = self.__func(it.castToInterface0DIterator())
			if z < z_min:
				z_min = z
			if z > z_max:
				z_max = z
			it.increment()
		z_diff = 1 / (z_max - z_min)
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			z = (self.__func(it.castToInterface0DIterator()) - z_min) * z_diff
			thickness = (1 - z) * self.__max + z * self.__min
			it.getObject().attribute().setThickness(thickness, thickness)
			it.increment()


## color modifiers
##################

class pyConstantColorShader(StrokeShader):
	def __init__(self,r,g,b, a = 1):
		StrokeShader.__init__(self)
		self._r = r
		self._g = g
		self._b = b
		self._a = a
	def getName(self):
		return "pyConstantColorShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			att.setColor(self._r, self._g, self._b)
			att.setAlpha(self._a)
			it.increment()

#c1->c2
class pyIncreasingColorShader(StrokeShader):
	def __init__(self,r1,g1,b1,a1, r2,g2,b2,a2):
		StrokeShader.__init__(self)
		self._c1 = [r1,g1,b1,a1]
		self._c2 = [r2,g2,b2,a2]
	def getName(self):
		return "pyIncreasingColorShader"
	def shade(self, stroke):
		n = stroke.strokeVerticesSize() - 1
		inc = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			c = float(inc)/float(n)

			att.setColor(  (1-c)*self._c1[0] + c*self._c2[0], 
				      (1-c)*self._c1[1] + c*self._c2[1],
			                    (1-c)*self._c1[2] + c*self._c2[2],)
			att.setAlpha((1-c)*self._c1[3] + c*self._c2[3],)
			inc = inc+1
			it.increment()

# c1->c2->c1
class pyInterpolateColorShader(StrokeShader):
	def __init__(self,r1,g1,b1,a1, r2,g2,b2,a2):
		StrokeShader.__init__(self)
		self._c1 = [r1,g1,b1,a1]
		self._c2 = [r2,g2,b2,a2]
	def getName(self):
		return "pyInterpolateColorShader"
	def shade(self, stroke):
		n = stroke.strokeVerticesSize() - 1
		inc = 0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			u = float(inc)/float(n)
			c = 1-2*(fabs(u-0.5))
			att.setColor(  (1-c)*self._c1[0] + c*self._c2[0], 
				      (1-c)*self._c1[1] + c*self._c2[1],
			                    (1-c)*self._c1[2] + c*self._c2[2],)
			att.setAlpha((1-c)*self._c1[3] + c*self._c2[3],)
			inc = inc+1
			it.increment()

class pyMaterialColorShader(StrokeShader):
	def __init__(self, threshold=50):
		StrokeShader.__init__(self)
		self._threshold = threshold

	def getName(self):
		return "pyMaterialColorShader"

	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		func = MaterialF0D()
		xn = 0.312713
		yn = 0.329016
		Yn = 1.0
		un = 4.* xn/ ( -2.*xn + 12.*yn + 3. )
		vn= 9.* yn/ ( -2.*xn + 12.*yn +3. )	
		while it.isEnd() == 0:
			toto = it.castToInterface0DIterator()
			mat = func(toto)
			
			r = mat.diffuseR()
			g = mat.diffuseG()
			b = mat.diffuseB()

			X = 0.412453*r + 0.35758 *g + 0.180423*b
			Y = 0.212671*r + 0.71516 *g + 0.072169*b
			Z = 0.019334*r + 0.119193*g + 0.950227*b

			if((X == 0) and (Y == 0) and (Z == 0)):
				X = 0.01
				Y = 0.01
				Z = 0.01
			u = 4.*X / (X + 15.*Y + 3.*Z)
			v = 9.*Y / (X + 15.*Y + 3.*Z)
			
			L= 116. * pow((Y/Yn),(1./3.)) -16
			U = 13. * L * (u - un)
			V = 13. * L * (v - vn)
			
			if (L > self._threshold):
				L = L/1.3
				U = U+10
			else:
				L = L +2.5*(100-L)/5.
				U = U/3.0
				V = V/3.0				
			u = U / (13. * L) + un
			v = V / (13. * L) + vn
			
			Y = Yn * pow( ((L+16.)/116.), 3.)
			X = -9. * Y * u / ((u - 4.)* v - u * v)
			Z = (9. * Y - 15*v*Y - v*X) /( 3. * v)
			
			r = 3.240479 * X - 1.53715 * Y - 0.498535 * Z
			g = -0.969256 * X + 1.875991 * Y + 0.041556 * Z
			b = 0.055648 * X - 0.204043 * Y + 1.057311 * Z

			r = max(0,r)
			g = max(0,g)
			b = max(0,b)

			att = it.getObject().attribute()
			att.setColor(r, g, b)
			it.increment()

class pyRandomColorShader(StrokeShader):
	def getName(self):
		return "pyRandomColorShader"
	def __init__(self, s=1):
		StrokeShader.__init__(self)
		seed(s)
	def shade(self, stroke):
		## pick a random color
		c0 = float(uniform(15,75))/100.0
		c1 = float(uniform(15,75))/100.0
		c2 = float(uniform(15,75))/100.0
		print(c0, c1, c2)
		it = stroke.strokeVerticesBegin()
		while(it.isEnd() == 0):
			it.getObject().attribute().setColor(c0,c1,c2)
			it.increment()

class py2DCurvatureColorShader(StrokeShader):
	def getName(self):
		return "py2DCurvatureColorShader"

	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		func = Curvature2DAngleF0D()
		while it.isEnd() == 0:
			toto = it.castToInterface0DIterator()
			sv = it.getObject()
			att = sv.attribute()
			c = func(toto)
			if (c<0):
				print("negative 2D curvature")
			color = 10.0 * c/3.1415
			print(color)
			att.setColor(color,color,color);
			it.increment()

class pyTimeColorShader(StrokeShader):
	def __init__(self, step=0.01):
		StrokeShader.__init__(self)
		self._t = 0
		self._step = step
	def shade(self, stroke):
		c = self._t*1.0
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			att = it.getObject().attribute()
			att.setColor(c,c,c)
			it.increment()
		self._t = self._t+self._step

## geometry modifiers

class pySamplingShader(StrokeShader):
	def __init__(self, sampling):
		StrokeShader.__init__(self)
		self._sampling = sampling
	def getName(self):
		return "pySamplingShader"
	def shade(self, stroke):
		stroke.Resample(float(self._sampling)) 

class pyBackboneStretcherShader(StrokeShader):
	def __init__(self, l):
		StrokeShader.__init__(self)
		self._l = l
	def getName(self):
		return "pyBackboneStretcherShader"
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
		vn_1 = itn_1.getObject()
		vn = itn.getObject()
		p0 = Vector([v0.getProjectedX(), v0.getProjectedY()])
		pn = Vector([vn.getProjectedX(), vn.getProjectedY()])
		p1 = Vector([v1.getProjectedX(), v1.getProjectedY()])
		pn_1 = Vector([vn_1.getProjectedX(), vn_1.getProjectedY()])
		d1 = p0-p1
		d1.normalize()
		dn = pn-pn_1
		dn.normalize()
		newFirst = p0+d1*float(self._l)
		newLast = pn+dn*float(self._l)
		v0.setPoint(newFirst)
		vn.setPoint(newLast)
		stroke.UpdateLength()
		
class pyLengthDependingBackboneStretcherShader(StrokeShader):
	def __init__(self, l):
		StrokeShader.__init__(self)
		self._l = l
	def getName(self):
		return "pyBackboneStretcherShader"
	def shade(self, stroke):
		l = stroke.getLength2D()
		stretch = self._l*l 
		it0 = stroke.strokeVerticesBegin()
		it1 = StrokeVertexIterator(it0)
		it1.increment()
		itn = stroke.strokeVerticesEnd()
		itn.decrement()
		itn_1 = StrokeVertexIterator(itn)
		itn_1.decrement()
		v0 = it0.getObject()
		v1 = it1.getObject()
		vn_1 = itn_1.getObject()
		vn = itn.getObject()
		p0 = Vector([v0.getProjectedX(), v0.getProjectedY()])
		pn = Vector([vn.getProjectedX(), vn.getProjectedY()])
		p1 = Vector([v1.getProjectedX(), v1.getProjectedY()])
		pn_1 = Vector([vn_1.getProjectedX(), vn_1.getProjectedY()])
		d1 = p0-p1
		d1.normalize()
		dn = pn-pn_1
		dn.normalize()
		newFirst = p0+d1*float(stretch)
		newLast = pn+dn*float(stretch)
		v0.setPoint(newFirst)
		vn.setPoint(newLast)
		stroke.UpdateLength()


## Shader to replace a stroke by its corresponding tangent
class pyGuidingLineShader(StrokeShader):
	def getName(self):
		return "pyGuidingLineShader"
	## shading method
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin() 	## get the first vertex
		itlast = stroke.strokeVerticesEnd() 	## 
		itlast.decrement()			## get the last one
		t = itlast.getObject().getPoint() - it.getObject().getPoint()	## tangent direction
		itmiddle = StrokeVertexIterator(it)	## 
		while(itmiddle.getObject().u()<0.5):	## look for the stroke middle vertex
			itmiddle.increment()		##
		it = StrokeVertexIterator(itmiddle)	
		it.increment()
		while(it.isEnd() == 0):			## position all the vertices along the tangent for the right part
			it.getObject().setPoint(itmiddle.getObject().getPoint() \
			+t*(it.getObject().u()-itmiddle.getObject().u()))
			it.increment()
		it = StrokeVertexIterator(itmiddle)
		it.decrement()
		while(it.isBegin() == 0):		## position all the vertices along the tangent for the left part
			it.getObject().setPoint(itmiddle.getObject().getPoint() \
			-t*(itmiddle.getObject().u()-it.getObject().u()))
			it.decrement()
		it.getObject().setPoint(itmiddle.getObject().getPoint()-t*(itmiddle.getObject().u())) ## first vertex
		stroke.UpdateLength()


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
		vn_1 = itn_1.getObject()
		vn = itn.getObject()
		if((vn.getNature() & Nature.CUSP == 0) and (vn_1.getNature() & Nature.CUSP == 0)):
			pn = vn.getPoint()
			pn_1 = vn_1.getPoint()
			dn = pn-pn_1
			dn.normalize()
			newLast = pn+dn*float(self._l)	
			vn.setPoint(newLast)
		stroke.UpdateLength()

normalInfo=Normal2DF0D()
curvatureInfo=Curvature2DAngleF0D()

def edgestopping(x, sigma): 
	return exp(- x*x/(2*sigma*sigma))

class pyDiffusion2Shader(StrokeShader):
	def __init__(self, lambda1, nbIter):
		StrokeShader.__init__(self)
		self._lambda = lambda1
		self._nbIter = nbIter
		self._normalInfo = Normal2DF0D()
		self._curvatureInfo = Curvature2DAngleF0D()
	def getName(self):
		return "pyDiffusionShader"
	def shade(self, stroke):
		for i in range (1, self._nbIter):
			it = stroke.strokeVerticesBegin()
			while it.isEnd() == 0:
				v=it.getObject()
				p1 = v.getPoint()
				p2 = self._normalInfo(it.castToInterface0DIterator())*self._lambda*self._curvatureInfo(it.castToInterface0DIterator())
				v.setPoint(p1+p2)
				it.increment()
		stroke.UpdateLength()

class pyTipRemoverShader(StrokeShader):
	def __init__(self, l):
		StrokeShader.__init__(self)
		self._l = l
	def getName(self):
		return "pyTipRemoverShader"
	def shade(self, stroke):
		originalSize = stroke.strokeVerticesSize()
		if(originalSize<4):
			return
		verticesToRemove = []
		oldAttributes = []
		it = stroke.strokeVerticesBegin()
		while(it.isEnd() == 0):
			v = it.getObject()
			if((v.curvilinearAbscissa() < self._l) or (v.strokeLength()-v.curvilinearAbscissa() < self._l)):
				verticesToRemove.append(v)
			oldAttributes.append(StrokeAttribute(v.attribute()))
			it.increment()
		if(originalSize-len(verticesToRemove) < 2):
			return
		for sv in verticesToRemove:
			stroke.RemoveVertex(sv)
		stroke.Resample(originalSize)
		if(stroke.strokeVerticesSize() != originalSize):
			print("pyTipRemover: Warning: resampling problem")
		it = stroke.strokeVerticesBegin()
		for a in oldAttributes:
			if(it.isEnd() == 1):
				break
			v = it.getObject()
			v.setAttribute(a)
			it.increment()
		stroke.UpdateLength()

class pyTVertexRemoverShader(StrokeShader):
	def getName(self):
		return "pyTVertexRemoverShader"
	def shade(self, stroke):
		if(stroke.strokeVerticesSize() <= 3 ):
			return
		predTVertex = pyVertexNatureUP0D(Nature.T_VERTEX)
		it = stroke.strokeVerticesBegin()
		itlast = stroke.strokeVerticesEnd()
		itlast.decrement()
		if(predTVertex(it) == 1):
			stroke.RemoveVertex(it.getObject())
		if(predTVertex(itlast) == 1):
			stroke.RemoveVertex(itlast.getObject())
		stroke.UpdateLength()

class pyExtremitiesOrientationShader(StrokeShader):
	def __init__(self, x1,y1,x2=0,y2=0):
		StrokeShader.__init__(self)
		self._v1 = Vector([x1,y1])
		self._v2 = Vector([x2,y2])
	def getName(self):
		return "pyExtremitiesOrientationShader"
	def shade(self, stroke):
		print(self._v1.x,self._v1.y)
		stroke.setBeginningOrientation(self._v1.x,self._v1.y)
		stroke.setEndingOrientation(self._v2.x,self._v2.y)

def getFEdge(it1, it2):
	return it1.getFEdge(it2)

class pyHLRShader(StrokeShader):
	def getName(self):
		return "pyHLRShader"
	def shade(self, stroke):
		originalSize = stroke.strokeVerticesSize()
		if(originalSize<4):
			return
		it = stroke.strokeVerticesBegin()
		invisible = 0
		it2 = StrokeVertexIterator(it)
		it2.increment()
		fe = getFEdge(it.getObject(), it2.getObject())
		if(fe.viewedge().qi() != 0):
			invisible = 1
		while(it2.isEnd() == 0):
			v = it.getObject()
			vnext = it2.getObject()
			if(v.getNature() & Nature.VIEW_VERTEX):
				#if(v.getNature() & Nature.T_VERTEX):
				fe = getFEdge(v,vnext)
				qi = fe.viewedge().qi()
				if(qi != 0):
					invisible = 1
				else:
					invisible = 0
			if(invisible == 1):		
				v.attribute().setVisible(0)
			it.increment()
			it2.increment()

class pyTVertexOrientationShader(StrokeShader):
	def __init__(self):
		StrokeShader.__init__(self)
		self._Get2dDirection = Orientation2DF1D()
	def getName(self):
		return "pyTVertexOrientationShader"
	## finds the TVertex orientation from the TVertex and 
	## the previous or next edge
	def findOrientation(self, tv, ve):
		mateVE = tv.mate(ve)
		if((ve.qi() != 0) or (mateVE.qi() != 0)):
			ait = AdjacencyIterator(tv,1,0)
			winner = None
			incoming = 1
			while(ait.isEnd() == 0):
				ave = ait.getObject()
				if((ave.getId() != ve.getId()) and (ave.getId() != mateVE.getId())):
					winner = ait.getObject()
					if(ait.isIncoming() == 0):
						incoming = 0
						break
				ait.increment()
			if(winner != None):
				if(incoming != 0):
					direction = self._Get2dDirection(winner.fedgeB())
				else:
					direction = self._Get2dDirection(winner.fedgeA())
				return direction
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it2 = StrokeVertexIterator(it)
		it2.increment()
		## case where the first vertex is a TVertex
		v = it.getObject() 
		if(v.getNature() & Nature.T_VERTEX):
			tv = v.castToTVertex()
			ve = getFEdge(v, it2.getObject()).viewedge()
			if(tv != None):			
				dir = self.findOrientation(tv, ve)
				#print(dir.x, dir.y)
				v.attribute().setAttributeVec2f("orientation", dir)
		while(it2.isEnd() == 0):
			vprevious = it.getObject()
			v = it2.getObject()
			if(v.getNature() & Nature.T_VERTEX):
				tv = v.castToTVertex()
				ve = getFEdge(vprevious, v).viewedge()
				if(tv != None):	
					dir = self.findOrientation(tv, ve)
					#print(dir.x, dir.y)
					v.attribute().setAttributeVec2f("orientation", dir)			
			it.increment()
			it2.increment()
		## case where the last vertex is a TVertex
		v = it.getObject() 
		if(v.getNature() & Nature.T_VERTEX):
			itPrevious = StrokeVertexIterator(it)
			itPrevious.decrement()
			tv = v.castToTVertex()
			ve = getFEdge(itPrevious.getObject(), v).viewedge()
			if(tv != None):			
				dir = self.findOrientation(tv, ve)
				#print(dir.x, dir.y)
				v.attribute().setAttributeVec2f("orientation", dir)

class pySinusDisplacementShader(StrokeShader):
	def __init__(self, f, a):
		StrokeShader.__init__(self)
		self._f = f
		self._a = a
		self._getNormal = Normal2DF0D()

	def getName(self):
		return "pySinusDisplacementShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			v = it.getObject()
			#print(self._getNormal.getName())
			n = self._getNormal(it.castToInterface0DIterator())
			p = v.getPoint()
			u = v.u()
			a = self._a*(1-2*(fabs(u-0.5)))
			n = n*a*cos(self._f*u*6.28)
			#print(n.x, n.y)
			v.setPoint(p+n)
			#v.setPoint(v.getPoint()+n*a*cos(f*v.u()))
			it.increment()
		stroke.UpdateLength()

class pyPerlinNoise1DShader(StrokeShader):
	def __init__(self, freq = 10, amp = 10, oct = 4, seed = -1):
		StrokeShader.__init__(self)
		self.__noise = Noise(seed)
		self.__freq = freq
		self.__amp = amp
		self.__oct = oct
	def getName(self):
		return "pyPerlinNoise1DShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			v = it.getObject()
			i = v.getProjectedX() + v.getProjectedY()
			nres = self.__noise.turbulence1(i, self.__freq, self.__amp, self.__oct)
			v.setPoint(v.getProjectedX() + nres, v.getProjectedY() + nres)
			it.increment()
		stroke.UpdateLength()

class pyPerlinNoise2DShader(StrokeShader):
	def __init__(self, freq = 10, amp = 10, oct = 4, seed = -1):
		StrokeShader.__init__(self)
		self.__noise = Noise(seed)
		self.__freq = freq
		self.__amp = amp
		self.__oct = oct
	def getName(self):
		return "pyPerlinNoise2DShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			v = it.getObject()
			vec = Vector([v.getProjectedX(), v.getProjectedY()])
			nres = self.__noise.turbulence2(vec, self.__freq, self.__amp, self.__oct)
			v.setPoint(v.getProjectedX() + nres, v.getProjectedY() + nres)
			it.increment()
		stroke.UpdateLength()

class pyBluePrintCirclesShader(StrokeShader):
	def __init__(self, turns = 1, random_radius = 3, random_center = 5):
		StrokeShader.__init__(self)
		self.__turns = turns
		self.__random_center = random_center
		self.__random_radius = random_radius
	def getName(self):
		return "pyBluePrintCirclesShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		if it.isEnd():
			return
		p_min = it.getObject().getPoint()
		p_max = it.getObject().getPoint()
		while it.isEnd() == 0:
			p = it.getObject().getPoint()
			if (p.x < p_min.x):
				p_min.x = p.x
			if (p.x > p_max.x):
				p_max.x = p.x
			if (p.y < p_min.y):
				p_min.y = p.y
			if (p.y > p_max.y):
				p_max.y = p.y
			it.increment()
		stroke.Resample(32 * self.__turns)
		sv_nb = stroke.strokeVerticesSize()
#		print("min  :", p_min.x, p_min.y) # DEBUG
#		print("mean :", p_sum.x, p_sum.y) # DEBUG
#		print("max  :", p_max.x, p_max.y) # DEBUG
#		print("----------------------") # DEBUG
#######################################################	
		sv_nb = sv_nb // self.__turns
		center = (p_min + p_max) / 2
		radius = (center.x - p_min.x + center.y - p_min.y) / 2
		p_new = Vector([0, 0])
#######################################################
		R = self.__random_radius
		C = self.__random_center
		i = 0
		it = stroke.strokeVerticesBegin()
		for j in range(self.__turns):
			prev_radius = radius
			prev_center = center
			radius = radius + randint(-R, R)
			center = center + Vector([randint(-C, C), randint(-C, C)])
			while i < sv_nb and it.isEnd() == 0:
				t = float(i) / float(sv_nb - 1)
				r = prev_radius + (radius - prev_radius) * t
				c = prev_center + (center - prev_center) * t
				p_new.x = c.x + r * cos(2 * pi * t)
				p_new.y = c.y + r * sin(2 * pi * t)
				it.getObject().setPoint(p_new)
				i = i + 1
				it.increment()
			i = 1
		verticesToRemove = []
		while it.isEnd() == 0:
			verticesToRemove.append(it.getObject())
			it.increment()
		for sv in verticesToRemove:
			stroke.RemoveVertex(sv)
		stroke.UpdateLength()

class pyBluePrintEllipsesShader(StrokeShader):
	def __init__(self, turns = 1, random_radius = 3, random_center = 5):
		StrokeShader.__init__(self)
		self.__turns = turns
		self.__random_center = random_center
		self.__random_radius = random_radius
	def getName(self):
		return "pyBluePrintEllipsesShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		if it.isEnd():
			return
		p_min = it.getObject().getPoint()
		p_max = it.getObject().getPoint()
		while it.isEnd() == 0:
			p = it.getObject().getPoint()
			if (p.x < p_min.x):
				p_min.x = p.x
			if (p.x > p_max.x):
				p_max.x = p.x
			if (p.y < p_min.y):
				p_min.y = p.y
			if (p.y > p_max.y):
				p_max.y = p.y
			it.increment()
		stroke.Resample(32 * self.__turns)
		sv_nb = stroke.strokeVerticesSize()
		sv_nb = sv_nb // self.__turns
		center = (p_min + p_max) / 2
		radius = center - p_min
		p_new = Vector([0, 0])
#######################################################
		R = self.__random_radius
		C = self.__random_center
		i = 0
		it = stroke.strokeVerticesBegin()
		for j in range(self.__turns):
			prev_radius = radius
			prev_center = center
			radius = radius + Vector([randint(-R, R), randint(-R, R)])
			center = center + Vector([randint(-C, C), randint(-C, C)])
			while i < sv_nb and it.isEnd() == 0:
				t = float(i) / float(sv_nb - 1)
				r = prev_radius + (radius - prev_radius) * t
				c = prev_center + (center - prev_center) * t
				p_new.x = c.x + r.x * cos(2 * pi * t)
				p_new.y = c.y + r.y * sin(2 * pi * t)
				it.getObject().setPoint(p_new)
				i = i + 1
				it.increment()
			i = 1
		verticesToRemove = []
		while it.isEnd() == 0:
			verticesToRemove.append(it.getObject())
			it.increment()
		for sv in verticesToRemove:
			stroke.RemoveVertex(sv)
		stroke.UpdateLength()


class pyBluePrintSquaresShader(StrokeShader):
	def __init__(self, turns = 1, bb_len = 10, bb_rand = 0):
		StrokeShader.__init__(self)
		self.__turns = turns
		self.__bb_len = bb_len
		self.__bb_rand = bb_rand

	def getName(self):
		return "pyBluePrintSquaresShader"

	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		if it.isEnd():
			return
		p_min = it.getObject().getPoint()
		p_max = it.getObject().getPoint()
		while it.isEnd() == 0:
			p = it.getObject().getPoint()
			if (p.x < p_min.x):
				p_min.x = p.x
			if (p.x > p_max.x):
				p_max.x = p.x
			if (p.y < p_min.y):
				p_min.y = p.y
			if (p.y > p_max.y):
				p_max.y = p.y
			it.increment()
		stroke.Resample(32 * self.__turns)
		sv_nb = stroke.strokeVerticesSize()
#######################################################	
		sv_nb = sv_nb // self.__turns
		first = sv_nb // 4
		second = 2 * first
		third = 3 * first
		fourth = sv_nb
		p_first = Vector([p_min.x - self.__bb_len, p_min.y])
		p_first_end = Vector([p_max.x + self.__bb_len, p_min.y])
		p_second = Vector([p_max.x, p_min.y - self.__bb_len])
		p_second_end = Vector([p_max.x, p_max.y + self.__bb_len])
		p_third = Vector([p_max.x + self.__bb_len, p_max.y])
		p_third_end = Vector([p_min.x - self.__bb_len, p_max.y])
		p_fourth = Vector([p_min.x, p_max.y + self.__bb_len])
		p_fourth_end = Vector([p_min.x, p_min.y - self.__bb_len])
#######################################################
		R = self.__bb_rand
		r = self.__bb_rand // 2
		it = stroke.strokeVerticesBegin()
		visible = 1
		for j in range(self.__turns):
			p_first = p_first + Vector([randint(-R, R), randint(-r, r)])
			p_first_end = p_first_end + Vector([randint(-R, R), randint(-r, r)])
			p_second = p_second + Vector([randint(-r, r), randint(-R, R)])
			p_second_end = p_second_end + Vector([randint(-r, r), randint(-R, R)])
			p_third = p_third + Vector([randint(-R, R), randint(-r, r)])
			p_third_end = p_third_end + Vector([randint(-R, R), randint(-r, r)])
			p_fourth = p_fourth + Vector([randint(-r, r), randint(-R, R)])
			p_fourth_end = p_fourth_end + Vector([randint(-r, r), randint(-R, R)])
			vec_first = p_first_end - p_first
			vec_second = p_second_end - p_second
			vec_third = p_third_end - p_third
			vec_fourth = p_fourth_end - p_fourth
			i = 0
			while i < sv_nb and it.isEnd() == 0:
				if i < first:
					p_new = p_first + vec_first * float(i)/float(first - 1)
					if i == first - 1:
						visible = 0
				elif i < second:
					p_new = p_second + vec_second * float(i - first)/float(second - first - 1)
					if i == second - 1:
						visible = 0
				elif i < third:
					p_new = p_third + vec_third * float(i - second)/float(third - second - 1)
					if i == third - 1:
						visible = 0
				else:
					p_new = p_fourth + vec_fourth * float(i - third)/float(fourth - third - 1)
					if i == fourth - 1:
						visible = 0
				if it.getObject() == None:
					i = i + 1
					it.increment()
					if visible == 0:
						visible = 1
					continue
				it.getObject().setPoint(p_new)
				it.getObject().attribute().setVisible(visible)
				if visible == 0:
					visible = 1
				i = i + 1
				it.increment()
		verticesToRemove = []
		while it.isEnd() == 0:
			verticesToRemove.append(it.getObject())
			it.increment()
		for sv in verticesToRemove:
			stroke.RemoveVertex(sv)
		stroke.UpdateLength()


class pyBluePrintDirectedSquaresShader(StrokeShader):
	def __init__(self, turns = 1, bb_len = 10, mult = 1):
		StrokeShader.__init__(self)
		self.__mult = mult
		self.__turns = turns
		self.__bb_len = 1 + float(bb_len) / 100
	def getName(self):
		return "pyBluePrintDirectedSquaresShader"
	def shade(self, stroke):
		stroke.Resample(32 * self.__turns)
		p_mean = Vector([0, 0])
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			p = it.getObject().getPoint()
			p_mean = p_mean + p
			it.increment()
		sv_nb = stroke.strokeVerticesSize()
		p_mean = p_mean / sv_nb
		p_var_xx = 0
		p_var_yy = 0
		p_var_xy = 0
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			p = it.getObject().getPoint()
			p_var_xx = p_var_xx + pow(p.x - p_mean.x, 2)
			p_var_yy = p_var_yy + pow(p.y - p_mean.y, 2)
			p_var_xy = p_var_xy + (p.x - p_mean.x) * (p.y - p_mean.y)
			it.increment()
		p_var_xx = p_var_xx / sv_nb
		p_var_yy = p_var_yy / sv_nb
		p_var_xy = p_var_xy / sv_nb
##		print(p_var_xx, p_var_yy, p_var_xy)
		trace = p_var_xx + p_var_yy
		det = p_var_xx * p_var_yy - p_var_xy * p_var_xy
		sqrt_coeff = sqrt(trace * trace - 4 * det)
		lambda1 = (trace + sqrt_coeff) / 2
		lambda2 = (trace - sqrt_coeff) / 2
##		print(lambda1, lambda2)
		theta = atan(2 * p_var_xy / (p_var_xx - p_var_yy)) / 2
##		print(theta)
		if p_var_yy > p_var_xx:
			e1 = Vector([cos(theta + pi / 2), sin(theta + pi / 2)]) * sqrt(lambda1) * self.__mult
			e2 = Vector([cos(theta + pi), sin(theta + pi)]) *  sqrt(lambda2) * self.__mult
		else:
			e1 = Vector([cos(theta), sin(theta)]) * sqrt(lambda1) * self.__mult
			e2 = Vector([cos(theta + pi / 2), sin(theta + pi / 2)]) * sqrt(lambda2) * self.__mult
#######################################################	
		sv_nb = sv_nb // self.__turns
		first = sv_nb // 4
		second = 2 * first
		third = 3 * first
		fourth = sv_nb
		bb_len1 = self.__bb_len
		bb_len2 = 1 + (bb_len1 - 1) * sqrt(lambda1 / lambda2)
		p_first = p_mean - e1 - e2 * bb_len2
		p_second = p_mean - e1 * bb_len1 + e2
		p_third = p_mean + e1 + e2 * bb_len2
		p_fourth = p_mean + e1 * bb_len1 - e2
		vec_first = e2 * bb_len2 * 2
		vec_second = e1 * bb_len1 * 2
		vec_third = vec_first * -1
		vec_fourth = vec_second * -1
#######################################################
		it = stroke.strokeVerticesBegin()
		visible = 1
		for j in range(self.__turns):
			i = 0
			while i < sv_nb:
				if i < first:
					p_new = p_first + vec_first * float(i)/float(first - 1)
					if i == first - 1:
						visible = 0
				elif i < second:
					p_new = p_second + vec_second * float(i - first)/float(second - first - 1)
					if i == second - 1:
						visible = 0
				elif i < third:
					p_new = p_third + vec_third * float(i - second)/float(third - second - 1)
					if i == third - 1:
						visible = 0
				else:
					p_new = p_fourth + vec_fourth * float(i - third)/float(fourth - third - 1)
					if i == fourth - 1:
						visible = 0
				it.getObject().setPoint(p_new)
				it.getObject().attribute().setVisible(visible)
				if visible == 0:
					visible = 1
				i = i + 1
				it.increment()
		verticesToRemove = []
		while it.isEnd() == 0:
			verticesToRemove.append(it.getObject())
			it.increment()
		for sv in verticesToRemove:
			stroke.RemoveVertex(sv)
		stroke.UpdateLength()

class pyModulateAlphaShader(StrokeShader):
	def __init__(self, min = 0, max = 1):
		StrokeShader.__init__(self)
		self.__min = min
		self.__max = max
	def getName(self):
		return "pyModulateAlphaShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		while it.isEnd() == 0:
			alpha = it.getObject().attribute().getAlpha()
			p = it.getObject().getPoint()
			alpha = alpha * p.y / 400
			if alpha < self.__min:
				alpha = self.__min
			elif alpha > self.__max:
				alpha = self.__max
			it.getObject().attribute().setAlpha(alpha)
			it.increment()


## various
class pyDummyShader(StrokeShader):
	def getName(self):
		return "pyDummyShader"
	def shade(self, stroke):
		it = stroke.strokeVerticesBegin()
		it_end = stroke.strokeVerticesEnd()
		while it.isEnd() == 0:
			toto = it.castToInterface0DIterator()
			att = it.getObject().attribute()
			att.setColor(0.3, 0.4, 0.4)
			att.setThickness(0, 5)
			it.increment()

class pyDebugShader(StrokeShader):
	def getName(self):
		return "pyDebugShader"

	def shade(self, stroke):
		fe = GetSelectedFEdgeCF()
		id1=fe.vertexA().getId()
		id2=fe.vertexB().getId()
		#print(id1.getFirst(), id1.getSecond())
		#print(id2.getFirst(), id2.getSecond())
		it = stroke.strokeVerticesBegin()
		found = 0
		foundfirst = 0
		foundsecond = 0
		while it.isEnd() == 0:
			cp = it.getObject()
			if((cp.A().getId() == id1) or (cp.B().getId() == id1)):
				foundfirst = 1
			if((cp.A().getId() == id2) or (cp.B().getId() == id2)):
				foundsecond = 1
			if((foundfirst != 0) and (foundsecond != 0)):
				found = 1
				break
			it.increment()
		if(found != 0):
			print("The selected Stroke id is: ", stroke.getId().getFirst(), stroke.getId().getSecond())
