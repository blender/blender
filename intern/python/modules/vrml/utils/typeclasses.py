'''
Classes of Types

Often you want to be able to say:
	if type(obj) in MutableTypes:
		yada
		
This module is intended to make that easier.
Just import and use :)
'''
import types

MutableTypes = [ types.ListType, types.DictType, types.InstanceType ]
MutableSequenceTypes = [ types.ListType ]
SequenceTypes = [ types.ListType, types.StringType, types.TupleType ]
NumericTypes = [ types.IntType, types.FloatType, types.LongType, types.ComplexType ]
MappingTypes = [ types.DictType ]

def regarray():
	if globals().has_key('array'):
		return 1
	try:
		import array
		SequenceTypes.append( array.ArrayType )
		MutableTypes.append( array.ArrayType )
		MutableSequenceTypes.append( array.ArrayType )
		return 1
	except ImportError:
		return 0

def regnumpy():
	'''
	Call if you want to register numpy arrays
	according to their types.
	'''
	if globals().has_key('Numeric'):
		return 1
	try:
#		import Numeric
		SequenceTypes.append( Numeric.ArrayType )
		MutableTypes.append( Numeric.ArrayType )
		MutableSequenceTypes.append( Numeric.ArrayType )
		return 1
	except ImportError:
		return 0

# for now, I'm going to always register these, if the module becomes part of the base distribution
# it might be better to leave it out so numpy isn't always getting loaded...
regarray()
regnumpy()
