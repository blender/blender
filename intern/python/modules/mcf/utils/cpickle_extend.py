'''
Extend cpickle storage to include modules, and builtin functions/methods

To use, just import this module.
'''
import copy_reg

### OBJECTS WHICH ARE RESTORED THROUGH IMPORTS
# MODULES
def pickle_module(module):
	'''
	Store a module to a pickling stream, must be available for
	reimport during unpickling
	'''
	return unpickle_imported_code, ('import %s'%module.__name__, module.__name__)

# FUNCTIONS, METHODS (BUILTIN)
def pickle_imported_code(funcmeth):
	'''
	Store a reference to an imported element (such as a function/builtin function,
	Must be available for reimport during unpickling.
	'''
	module = _whichmodule(funcmeth)
	return unpickle_imported_code, ('from %s import %s'%(module.__name__,funcmeth.__name__),funcmeth.__name__)

import types, regex
import_filter = regex.compile('''\(from [A-Za-z0-9_\.]+ \)?import [A-Za-z0-9_\.]+''') # note the limitations on whitespace
getattr_filter = regex.compile('''[A-Za-z0-9_\.]+''') # note we allow you to use x.y.z here

# MODULES, AND FUNCTIONS
def unpickle_imported_code(impstr,impname):
	'''
	Attempt to load a reference to a module or other imported code (such as functions/builtin functions)
	'''
	if import_filter.match(impstr) != len(impstr) or getattr_filter.match(impname)!= len(impname):
		import sys
		sys.stderr.write('''Possible attempt to smuggle arbitrary code into pickle file (see module cpickle_extend).\nPassed code was %s\n%s\n'''%(impstr,impname))
		del(sys)
	else:
		ns = {}
		try:
			exec (impstr) in ns # could raise all sorts of errors, of course, and is still dangerous when you have no control over the modules on your system!  Do not allow for untrusted code!!!
			return eval(impname, ns)
		except:
			import sys
			sys.stderr.write('''Error unpickling module %s\n None returned, will likely raise errors.'''%impstr)
			return None

# Modules
copy_reg.pickle(type(regex),pickle_module,unpickle_imported_code)
# builtin functions/methods
copy_reg.pickle(type(regex.compile),pickle_imported_code, unpickle_imported_code)

del(regex) # to keep the namespace neat as possible

### INSTANCE METHODS
'''
The problem with instance methods is that they are almost always
stored inside a class somewhere.  We really need a new type: reference
that lets us just say "y.this"

We also need something that can reliably find burried functions :( not
likely to be easy or clean...

then filter for x is part of the set
'''
import new

def pickle_instance_method(imeth):
	'''
	Use the (rather surprisingly clean) internals of
	the method to store a reference to a method. Might
	be better to use a more general "get the attribute
	'x' of this object" system, but I haven't written that yet :)
	'''
	klass = imeth.im_class
	funcimp = _imp_meth(imeth)
	self = imeth.im_self # will be None for UnboundMethodType
	return unpickle_instance_method, (funcimp,self,klass)
def unpickle_instance_method(funcimp,self,klass):
	'''
	Attempt to restore a reference to an instance method,
	the instance has already been recreated by the system
	as self, so we just call new.instancemethod
	'''
	funcimp = apply(unpickle_imported_code, funcimp)
	return new.instancemethod(func,self,klass)

copy_reg.pickle(types.MethodType, pickle_instance_method, unpickle_instance_method)
copy_reg.pickle(types.UnboundMethodType, pickle_instance_method, unpickle_instance_method)

### Arrays
try:
	import array
	LittleEndian = array.array('i',[1]).tostring()[0] == '\001'
	def pickle_array(somearray):
		'''
		Store a standard array object, inefficient because of copying to string
		'''
		return unpickle_array, (somearray.typecode, somearray.tostring(), LittleEndian)
	def unpickle_array(typecode, stringrep, origendian):
		'''
		Restore a standard array object
		'''
		newarray = array.array(typecode)
		newarray.fromstring(stringrep)
		# floats are always big-endian, single byte elements don't need swapping
		if origendian != LittleEndian and typecode in ('I','i','h','H'):
			newarray.byteswap()
		return newarray
	copy_reg.pickle(array.ArrayType, pickle_array, unpickle_array)
except ImportError: # no arrays
	pass

### NUMPY Arrays
try:
	import Numeric
	LittleEndian = Numeric.array([1],'i').tostring()[0] == '\001'
	def pickle_numpyarray(somearray):
		'''
		Store a numpy array, inefficent, but should work with cPickle
		'''
		return unpickle_numpyarray, (somearray.typecode(), somearray.shape, somearray.tostring(), LittleEndian)
	def unpickle_numpyarray(typecode, shape, stringval, origendian):
		'''
		Restore a numpy array
		'''
		newarray = Numeric.fromstring(stringval, typecode)
		Numeric.reshape(newarray, shape)
		if origendian != LittleEndian and typecode in ('I','i','h','H'):
			# this doesn't seem to work correctly, what's byteswapped doing???
			return newarray.byteswapped()
		else:
			return newarray
	copy_reg.pickle(Numeric.ArrayType, pickle_numpyarray, unpickle_numpyarray)
except ImportError:
	pass

### UTILITY FUNCTIONS
classmap = {}
def _whichmodule(cls):
	"""Figure out the module in which an imported_code object occurs.
	Search sys.modules for the module.
	Cache in classmap.
	Return a module name.
	If the class cannot be found, return __main__.
	Copied here from the standard pickle distribution
	to prevent another import
	"""
	if classmap.has_key(cls):
		return classmap[cls]
	clsname = cls.__name__
	for name, module in sys.modules.items():
		if name != '__main__' and \
		   hasattr(module, clsname) and \
		   getattr(module, clsname) is cls:
			break
	else:
		name = '__main__'
	classmap[cls] = name
	return name

import os, string, sys

def _imp_meth(im):
	'''
	One-level deep recursion on finding methods, i.e. we can 
	find them only if the class is at the top level.
	'''
	fname = im.im_func.func_code.co_filename
	tail = os.path.splitext(os.path.split(fname)[1])[0]
	ourkeys = sys.modules.keys()
	possibles = filter(lambda x,tail=tail: x[-1] == tail, map(string.split, ourkeys, ['.']*len(ourkeys)))
	
	# now, iterate through possibles to find the correct class/function
	possibles = map(string.join, possibles, ['.']*len(possibles))
	imp_string = _search_modules(possibles, im.im_func)
	return imp_string
	
def _search_modules(possibles, im_func):
	for our_mod_name in possibles:
		our_mod = sys.modules[our_mod_name]
		if hasattr(our_mod, im_func.__name__) and getattr(our_mod, im_func.__name__).im_func is im_func:
			return 'from %s import %s'%(our_mod.__name__, im_func.__name__), im_func.__name__
		for key,val in our_mod.__dict__.items():
			if hasattr(val, im_func.__name__) and getattr(val, im_func.__name__).im_func is im_func:
				return 'from %s import %s'%(our_mod.__name__,key), '%s.%s'%(key,im_func.__name__)
	raise '''No import string calculable for %s'''%im_func


