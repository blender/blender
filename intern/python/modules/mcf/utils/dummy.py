'''
Dummy Class, intended as an abstract class for the creation
of base/builtin classes with slightly altered functionality
uses _base as the name of an instance of the base datatype,
mapping all special functions to that name.

>>> from mcf.utils import dummy

>>> j = dummy.Dummy({})

>>> j['this'] = 23

>>> j

{'this': 23}

>>> class example(dummy.Dummy):

... 	def __repr__(self):

... 		return '<example: %s>'%`self._base`

>>> k = example([])

>>> k # uses the __repr__ function

<example: []>

>>> k.append # finds the attribute of the _base

<built-in method append of list object at 501830>

'''
import types, copy

class Dummy:
	'Abstract class for slightly altering functionality of objects (including builtins)'
	def __init__(self, val=None):
		'Initialisation, should be overridden'
		if val and type(val)== types.InstanceType and hasattr(val, '_base'):
			# Dict is used because subclasses often want to override
			# the setattr function
			self.__dict__['_base']=copy.copy(val.__dict__['_base'])
		else:
			self.__dict__['_base'] = val
	def __repr__(self):
		'Return a string representation'
		return repr(self._base)
	def __str__(self):
		'Convert to a string'
		return str(self._base)
	def __cmp__(self,other):
		'Compare to other value'
		# altered 98.03.17 from if...elif...else statement
		return cmp(self._base, other)
	def __getitem__(self, key):
		'Get an item by index'
		return self._base[key]
	def __setitem__(self, key, val):
		'Set an item by index'
		self._base[key]=val
	def __len__(self):
		'return the length of the self'
		return len(self._base)
	def __delitem__(self, key):
		'remove an item by index'
		del(self._base[key])
	def __getslice__(self, i, j):
		'retrieve a slice by indexes'
		return self._base[i:j]
	def __setslice__(self, i, j, val):
		'set a slice by indexes to values'
		self._base[i:j]=val
	def __delslice__(self, i, j):
		'remove a slice by indexes'
		del(self._base[i:j])
	def __nonzero__(self):
		if self._base:
			return 1
		else:
			return 0
	def __getattr__(self, attr):
		'find an attribute when normal lookup fails, will raise a KeyError if missing _base attribute'
		try:
			return getattr( self.__dict__['_base'], attr)
		except (AttributeError, KeyError):
			try:
				return self.__dict__['_base'][attr]
			except (KeyError,TypeError):
				pass
		raise AttributeError, attr
