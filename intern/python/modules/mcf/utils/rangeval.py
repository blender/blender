''' Classes which match ranges, sets, or anything at all. '''
import dummy # provides storage functions as well as a few others

class BetwVal(dummy.Dummy):
	'''
	Matches any object greater than smaller and less than larger
	'''
	def __init__(self, first, second):
		if first <= second:
			dummy.Dummy.__init__(self, [first, second])
		else:
			dummy.Dummy.__init__(self, [second, first])
	def __getinitargs__(self):
		return (self._base[0], self._base[1])
	def __cmp__(self, object):
		'''The Guts of the Class, allows standard comparison operators'''
		if self._base[0]<=object:
			if self._base[1] >=object:
				return 0
			else: return 1
		else: return -1
	def __repr__(self):
		return '%s(%s,%s)'% (self.__class__.__name__,`self._base[0]`,`self._base[1]`)

class WInVal(dummy.Dummy):
	'''
	Matches any value in the sequential object used as initialiser
	Doesn't gracefully handle situations where not found, as it just
	returns a -1
	'''
	def __init__(self,seq):
		self._base = seq
	def __cmp__(self, object):
		''' Standard comparison operators '''
		for x in self._base:
			if x == object:
				return 0
		return -1
	def __repr__(self):
		return '%s(%s)'% (self.__class__.__name__,`self._base`)

class ExceptVal(WInVal):
	'''
	A negative Version of WInVal
	'''
	def __cmp__(self, object):
		for x in self._base:
			if x == object:
				return -1
		return 0

class AnyVal:
	'''
	Matches anything at all
	'''
	def __init__(self):
		pass
	def __getinitargs__(self):
		return ()
	def __cmp__(self, object):
		return 0
	def __repr__(self):
		return 'AnyVal()'

