class inplace:
	def __add__( self, num ):
		self.base = self.base + num
		return self.base
	def __sub__( self, num ):
		self.base = self.base - num
		return self.base
	def __init__(self, base ):
		self.base = base
	def __repr__(self ):
		return repr( self.base)
	def __str__(self ):
		return str( self.base)
	__radd__ = __add__
	def __mul__(self, num ):
		return self.base * num
	def __div__(self, num ):
		return self.base / num
	def __mod__(self, num ):
		return self.base % num
	def __neg__(self ):
		return - abs( self.base)
	def __pos__(self ):
		return abs( self.base)
	def __abs__(self ):
		return abs( self.base )
	def __inv__(self ):
		return -self.base
	def __lshift__(self, num ):
		return self.base << num
	def __rshift__(self, num ):
		return self.base >> num
	def __and__(self, num ):
		return self.base and num
	def __or__(self, num ):
		return self.base or num
	def value( self ):
		return self.base