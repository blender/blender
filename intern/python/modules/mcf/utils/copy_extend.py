'''
Module to allow for "copying" Numeric arrays,
(and thereby also matrices and userarrays)
standard arrays, classes and modules
(last two are not actually copied, but hey :) ).

Could do approximately the same thing with
copy_reg, but would be inefficient because
of passing the data into and out of strings.

To use, just import this module.
'''
# altered 98.11.05, moved copy out of NUMPY test
import copy
try: # in case numpy not installed
	import Numeric
	def _numpyarray_copy(somearray, memo=None):
		'''
		Simple function for getting a copy of a NUMPY array
		'''
		if memo == None:
			memo = {} # yeah, I know, not _really_ necessary
		# see if already done this item, return the copy if we have...
		d = id(somearray)
		try:
			return memo[d]
		except KeyError:
			pass
		temp = Numeric.array(somearray, copy=1)
		memo[d] = temp
		return temp
	# now make it available to the copying functions
	copy._copy_dispatch[Numeric.ArrayType] = _numpyarray_copy
	copy._deepcopy_dispatch[Numeric.ArrayType] = _numpyarray_copy
except ImportError: # Numeric not installed...
	pass

try: # in case array not installed
	import array
	def _array_copy(somearray, memo = None):
		'''
		Simple function for getting a copy of a standard array.
		'''
		if memo == None:
			memo = {} # yeah, I know, not _really_ necessary
		# see if already done this item, return the copy if we have...
		d = id(somearray)
		try:
			return memo[d]
		except KeyError:
			pass
		newarray = somearay[:]
		memo[d] = newarray
		return newarray
		
	# now make it available to the copying functions
	copy._copy_dispatch[ array.ArrayType ] = _array_copy
	copy._deepcopy_dispatch[ array.ArrayType ] = _array_copy
except ImportError:
	pass

import types

def _module_copy(somemodule, memo = None):
	'''
	Modules we will always treat as themselves during copying???
	'''
	return somemodule

# now make it available to the copying functions
copy._copy_dispatch[ types.ModuleType ] = _module_copy
copy._deepcopy_dispatch[ types.ModuleType ] = _module_copy

def _class_copy(someclass, memo=None):
	'''
	Again, classes are considered immutable, they are
	just returned as themselves, not as new objects.
	'''
	return someclass

# now make it available to the copying functions
#copy._copy_dispatch[ types.ClassType ] = _class_copy
copy._deepcopy_dispatch[ types.ClassType ] = _class_copy
