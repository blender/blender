'''
NameSpace v0.04:

A "NameSpace" is an object wrapper around a _base dictionary
which allows chaining searches for an 'attribute' within that
dictionary, or any other namespace which is defined as part
of the search path (depending on the downcascade variable, is
either the hier-parents or the hier-children).

You can assign attributes to the namespace normally, and read
them normally. (setattr, getattr, a.this = that, a.this)

I use namespaces for writing parsing systems, where I want to
differentiate between sources (have multiple sources that I can
swap into or out of the namespace), but want to be able to get
at them through a single interface.  There is a test function
which gives you an idea how to use the system.

In general, call NameSpace(someobj), where someobj is a dictionary,
a module, or another NameSpace, and it will return a NameSpace which
wraps up the keys of someobj.  To add a namespace to the NameSpace,
just call the append (or hier_addchild) method of the parent namespace
with the child as argument.

### NOTE: if you pass a module (or anything else with a dict attribute),
names which start with '__' will be removed.  You can avoid this by
pre-copying the dict of the object and passing it as the arg to the
__init__ method.

### NOTE: to properly pickle and/or copy module-based namespaces you
will likely want to do: from mcf.utils import extpkl, copy_extend

### Changes:
	97.05.04 -- Altered to use standard hierobj interface, cleaned up
	interface by removing the "addparent" function, which is reachable
	by simply appending to the __parent__ attribute, though normally
	you would want to use the hier_addchild or append functions, since
	they let both objects know about the addition (and therefor the 
	relationship will be restored if the objects are stored and unstored)
	
	97.06.26 -- Altered the getattr function to reduce the number of
	situations in which infinite lookup loops could be created
	(unfortunately, the cost is rather high).  Made the downcascade
	variable harden (resolve) at init, instead of checking for every
	lookup. (see next note)
	
	97.08.29 -- Discovered some _very_ weird behaviour when storing
	namespaces in mcf.store dbases.  Resolved it by storing the 
	__namespace_cascade__ attribute as a normal attribute instead of
	using the __unstore__ mechanism... There was really no need to
	use the __unstore__, but figuring out how a functions saying
	self.__dict__['__namespace_cascade__'] = something
	print `self.__dict__['__namespace_cascade__']` can print nothing
	is a bit beyond me. (without causing an exception, mind you)

	97.11.15 Found yet more errors, decided to make two different 
	classes of namespace.  Those based on modules now act similar
	to dummy objects, that is, they let you modify the original
	instead of keeping a copy of the original and modifying that.
	
	98.03.15 -- Eliminated custom pickling methods as they are no longer
	needed for use with Python 1.5final
	
	98.03.15 -- Fixed bug in items, values, etceteras with module-type
	base objects.
'''
import copy, types, string
import hierobj

class NameSpace(hierobj.Hierobj):
	'''
	An hierarchic NameSpace, allows specification of upward or downward
	chaining search for resolving names
	'''
	def __init__(self, val = None, parents=None, downcascade=1,children=[]):
		'''
		A NameSpace can be initialised with a dictionary, a dummied
		dictionary, another namespace, or something which has a __dict__
		attribute.
		Note that downcascade is hardened (resolved) at init, not at
		lookup time.
		'''
		hierobj.Hierobj.__init__(self, parents, children)
		self.__dict__['__downcascade__'] = downcascade # boolean
		if val is None:
			self.__dict__['_base'] = {}
		else:
			if type( val ) == types.StringType:
				# this is a reference to a module which has been pickled
				val = __import__( val, {},{}, string.split( val, '.') )
			try:
				# See if val's a dummy-style object which has a _base
				self.__dict__['_base']=copy.copy(val._base)
			except (AttributeError,KeyError):
				# not a dummy-style object... see if it has a dict attribute...
				try:
					if type(val) != types.ModuleType:
						val = copy.copy(val.__dict__)
				except (AttributeError, KeyError):
					pass
				# whatever val is now, it's going to become our _base...
				self.__dict__['_base']=val
		# harden (resolve) the reference to downcascade to speed attribute lookups
		if downcascade: self.__dict__['__namespace_cascade__'] = self.__childlist__
		else: self.__dict__['__namespace_cascade__'] = self.__parent__
	def __setattr__(self, var, val):
		'''
		An attempt to set an attribute should place the attribute in the _base
		dictionary through a setitem call.
		'''
		# Note that we use standard attribute access to allow ObStore loading if the
		# ._base isn't yet available.
		try:
			self._base[var] = val
		except TypeError:
			setattr(self._base, var, val)
	def __getattr__(self,var):
##		print '__getattr__', var
		return self.__safe_getattr__(var, {}) # the {} is a stopdict

	def __safe_getattr__(self, var,stopdict):
		'''
		We have a lot to do in this function, if the attribute is an unloaded
		but stored attribute, we need to load it.  If it's not in the stored
		attributes, then we need to load the _base, then see if it's in the 
		_base.
		If it's not found by then, then we need to check our resource namespaces
		and see if it's in them.
		'''
		# we don't have a __storedattr__ or it doesn't have this key...
		if var != '_base':
			try:
				return self._base[var]
			except (KeyError,TypeError), x:
				try:
					return getattr(self._base, var)
				except AttributeError:
					pass
		try: # with pickle, it tries to get the __setstate__ before restoration is complete
			for cas in self.__dict__['__namespace_cascade__']:
				try:
					stopdict[id(cas)] # if succeeds, we've already tried this child
					# no need to do anything, if none of the children succeeds we will
					# raise an AttributeError
				except KeyError:
					stopdict[id(cas)] = None
					return cas.__safe_getattr__(var,stopdict)
		except (KeyError,AttributeError):
			pass
		raise AttributeError, var
	def items(self):
		try:
			return self._base.items()
		except AttributeError:
			pass
		try:
			return self._base.__dict__.items()
		except AttributeError:
			pass
	def keys(self):
		try:
			return self._base.keys()
		except AttributeError:
			pass
		try:
			return self._base.__dict__.keys()
		except AttributeError:
			pass
	def has_key( self, key ):
		try:
			return self._base.has_key( key)
		except AttributeError:
			pass
		try:
			return self._base.__dict__.has_key( key)
		except AttributeError:
			pass
	def values(self):
		try:
			return self._base.values()
		except AttributeError:
			pass
		try:
			return self._base.__dict__.values()
		except AttributeError:
			pass

	def __getinitargs__(self):
		if type( self._base ) is types.ModuleType:
			base = self._base.__name__
		else:
			base = self._base
		return (base, self.__parent__, self.__downcascade__, self.__childlist__)
	def __getstate__(self):
		return None
	def __setstate__(self,*args):
		pass
	def __deepcopy__(self, memo=None):
		d = id(self)
		if memo is None:
			memo = {}
		elif memo.has_key(d):
			return memo[d]
		if type(self._base) == types.ModuleType:
			rest = tuple(map( copy.deepcopy, (self.__parent__, self.__downcascade__, self.__childlist__) ))
			new = apply(self.__class__, (self._base,)+rest )
		else:
			new = tuple(map( copy.deepcopy, (self._base, self.__parent__, self.__downcascade__, self.__childlist__) ))
		return new
##	def __del__( self, id=id ):
##		print 'del namespace', id( self )
			

def test():
	import string
	a = NameSpace(string)
	del(string)
	a.append(NameSpace({'a':23,'b':42}))
	import math
	a.append(NameSpace(math))
	print 'The returned object should allow access to the attributes of the string,\nand math modules, and two simple variables "a" and "b" (== 23 and42 respectively)'
	return a


