'''
Field coercian routines.

To replace the field coercian routines, you must edit
basenodes.py and node.py to import some other coercian
routines.  Basenodes.py is for use by the parser, node
is used by each node as it checks the validity of its
attributes.
'''

import types, sys, string
from utils import typeclasses, collapse

class FieldCoercian:
	'''
	A Field Coercian class allows for creating new behaviours
	when dealing with the conversion of fields to-and-from
	particular field types.  This allows the programmer to
	use alternate representations of fields (such as matrix arrays)
	'''
	def SFString( self, someobj, targetType=types.StringType, targetName='SFString', convertfunc=str ):
		'''
		Allowable types:
			simple string -> unchanged
			instance ( an IS ) -> unchanged
			sequence of length == 1 where first element is a string -> returns first element
			sequence of length > 1 where all elements are strings -> returns string.join( someobj, '')
		'''
		t = type(someobj)
		if t is targetType:
			return someobj
		if t in typeclasses.SequenceTypes:
			if len( someobj) == 1 and type( someobj[0] ) is targetType:
				return someobj[0] # 
			elif len(someobj) > 1:
				try:
					return string.join( someobj, '')
				except:
					pass # is not a sequence of strings...
		### if we get here, then an incorrect value was passed
		raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )
		
	def MFString( self, someobj, targetType=types.StringType, targetName='SFString', convertfunc=str ):
		'''
		Allowable Types:
			simple string -> wrapped in a list
			instance (an IS ) -> unchanged
			sequence of strings (of any length) -> equivalent list returned
		'''
		t = type(someobj)
		if t is targetType: # a bare string...
			return [someobj]
		elif t in typeclasses.SequenceTypes: # is a sequence
			if not filter( lambda x, t=targetType: x is not t, map( type, someobj) ): # are all strings...
				if t is not types.ListType:
					return list( someobj )
				else:
					return someobj
		### if we get here, then an incorrect value was passed
		raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )

	def SFBool( self, someobj, targetType=types.IntType, targetName='SFBool', convertfunc=int):
		'''
		Allowable Types:
			instance (an IS) -> unchanged
			Any object which is testable for truth/falsehood -> 1 or 0 respectively
		SFBool should always succeed
		'''
		if (type(someobj) in typeclasses.SequenceTypes):
			try:
				if hasattr( someobj[0], '__gi__'):
					return someobj[0]
				else:
					someobj = someobj[0]
			except IndexError: # is a null MFNode
				pass
		if someobj:
			return 1
		else:
			return 0
			
	def SFNode( self, someobj, targetType=types.InstanceType, targetName='SFNode', convertfunc=None):
		'''
		Allowable Types:
			instance of a Node -> unchanged
			instance (an IS or USE) -> unchanged
			sequence of length == 1 where first element is as above -> return first element
		'''
		if hasattr( someobj, '__gi__'): # about the only test I have without requiring that elements inherit from Node
			return someobj
		elif (type(someobj) in typeclasses.SequenceTypes):
			try:
				if hasattr( someobj[0], '__gi__'):
					return someobj[0]
			except IndexError: # is a null MFNode
				pass
		raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )
		
	def MFNode( self, someobj, targetType=types.InstanceType, targetName='MFNode', convertfunc=None):
		'''
		Allowable Types:
			instance (an IS) -> unchanged
			instance of a Node -> wrapped with a list
			sequence where all elements are nodes -> returned as list of same
		'''
		if hasattr( someobj, '__gi__') and someobj.__gi__ != "IS":
			# is this a bare SFNode? wrap with a list and return
			return [someobj]
		elif hasattr( someobj, "__gi__"): # is this an IS node
			return someobj
		elif type(someobj) in typeclasses.SequenceTypes:
			try:
				map( getattr, someobj, ['__gi__']*len(someobj) )
				# is this an IS node wrapped in a list?
				if len(someobj) == 1 and someobj[0].__gi__ == "IS":
					return someobj[0]
				# okay, assume is really nodes...
				if type(someobj) is types.ListType:
					return someobj
				else:
					return list(someobj)
			except AttributeError: # something isn't a node
				pass
		raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )

	def SFNumber( self, someobj, targetType, targetName, convertfunc=int ):
		'''
		Allowable Types:
			bare number -> numerically coerced to correct type
			instance ( an IS ) -> unchanged
			sequence of length == 1 where first element is a string -> returns first element
		'''
		t = type(someobj)
		if t is targetType or t is types.InstanceType:
			return someobj
		elif t in typeclasses.NumericTypes:
			return convertfunc( someobj)
		elif t in typeclasses.SequenceTypes:
			if len( someobj) == 1 and type( someobj[0] ):
				return convertfunc( someobj[0] ) # 
		### if we get here, then an incorrect value was passed
		raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )
	def MFInt32 ( self, someobject ):
		''' Convert value into a MFInt32 field value (preferably an array, otherwise a list of integers) '''
		t = type(someobject)
		value = None
		if t in typeclasses.SequenceTypes: # is a sequence
			try:
				value = map( int, someobject)
			except:
				try:
					value = map( int, collapse.collapse2_safe( someobject) )
				except:
					pass
		elif t in typeclasses.NumericTypes or t is types.StringType:
			value = [int(someobject)]
		if value is None:
			### if we get here, then an incorrect value was passed
			raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )
		return value
	SFImage = MFInt32
	def MFFloat( self, someobject ):
		''' Convert value into a MFFloat field value (preferably an array, otherwise a list of integers) '''
		t = type(someobject)
		value = None
		if t in typeclasses.SequenceTypes: # is a sequence
			try:
				value = map( float, someobject)
			except:
				try:
					value = map( float, collapse.collapse2_safe( someobject))
				except:
					pass
		elif t in typeclasses.NumericTypes or t is types.StringType:
			value = [float(someobj)]
		if value is None:
			### if we get here, then an incorrect value was passed
			raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )
		return value
	def SFVec3f (self,  value):
		''' Create a new SFVec3f value from value '''
		t = type(value)
		try:
			value = x,y,z = map (float, value)
		except ValueError:
			try:
				value =  (x,y,z) = map( float, value[0] )
			except (IndexError, ValueError):
				raise ValueError (''' Invalid value for field type SFVec3f: %s'''%(value))
		return value
	def SFRotation(self,  value):
		''' Create a new SFRotation value from value '''
		t = type(value)
		try:
			value = x,y,z, a = map (float, value)
		except ValueError:
			try:
				value =  (x,y,z, a) = map( float, value[0] )
			except (IndexError, ValueError):
				raise ValueError (''' Invalid value for field type SFRotation: %s'''%(value))
		# get the normalized vector for x,y,z
##		length = (x*x+y*y+z*z)**.5 or 0.0000
##		value = (x/length,y/length,z/length, a)
		return value
	def SFVec2f (self,  value):
		''' Create a new SFVec3f value from value '''
		t = type(value)
		try:
			value = x,y = map (float, value)
		except ValueError:
			try:
				value =  (x,y) = map( float, value[0] )
			except (IndexError, ValueError):
				raise ValueError (''' Invalid value for field type SFVec3f: %s'''%(value))
		return value
	def SFColor(self,  value):
		''' Create a new SFVec3f value from value '''
		t = type(value)
		try:
			r,g,b = map (float, value)
		except ValueError:
			try:
				r,g,b = map( float, value[0] )
			except (IndexError, ValueError):
				raise ValueError (''' Invalid value for field type SFColor: %s'''%(value))
		r = max( (0.0, min((r,1.0))) )
		g = max( (0.0, min((g,1.0))) )
		b = max( (0.0, min((b,1.0))) )
		return value
		
	def MFCompoundNumber( self, someobj, targetName='SFVec3f', convertfunc=float, type=type):
		'''
		Allowable Types:
			instance ( an IS ) -> unchanged
			# instance ( a matrix ) -> reshaped (eventually)
			list of lists, sub-sequences of proper length -> unchanged
			sequence of numeric types of proper length -> converted to list, diced
		'''
##		if targetName == 'SFColor':
##			import pdb
##			pdb.set_trace()
		converter = getattr( self, targetName )
		t = type( someobj)
		reporterror = 0
		if t is types.InstanceType:
			return someobj
		elif t in typeclasses.SequenceTypes:
			if not someobj:
				return []
			if type( someobj[0] ) is not types.StringType and type( someobj[0] ) in typeclasses.SequenceTypes:
				try:
					return map( converter, someobj )
				except ValueError:
					pass
			elif type( someobj[0] ) in typeclasses.NumericTypes or type( someobj[0] ) is types.StringType:
				# a single-level list?
				base = map( convertfunc, someobj )
				# if we get here, someobj is a list
				if targetName[-2:] == '2f': # vec2f
					tlen = 2
				elif targetName[-2:] == 'on': # rotation
					tlen = 4
				else:
					tlen = 3
				value = []
				while base:
					value.append( converter( base[:tlen]) )
					del base[:tlen]
				return value
		raise ValueError, """Attempted to set value for an %s field which is not compatible: %s"""%( targetName, `someobj` )
	def __call__( self, someobj, targetName):
		func, args = self.algomap[targetName]
##		try:
##			if targetName == 'SFInt32':
##				import pdb
##				pdb.set_trace()
		if hasattr( someobj, "__gi__") and someobj.__gi__ == "IS":
			return someobj
		else:
			return apply( func, (self, someobj)+args )
##		except TypeError:
##			print someobj, targetName
##			print func, args
##			raise

	algomap = { \
		'SFString': (SFString, (types.StringType, 'SFString', str)), \
		'MFString': (MFString, (types.StringType, 'MFString', str)), \
		'SFInt32': (SFNumber, (types.IntType, 'SFInt32', int)), \
		'SFFloat': (SFNumber, (types.FloatType, 'SFFloat', float)), \
		'SFTime': (SFNumber, (types.FloatType, 'SFFloat', float)), \
		'SFColor': (SFColor, ()), \
		'SFVec2f': (SFVec2f, ()), \
		'SFVec3f': (SFVec3f, ()), \
		'SFNode': (SFNode, (types.InstanceType, 'SFNode', None)), \
		'SFBool': (SFBool, (types.IntType, 'SFBool', int)), \
		'SFNode': (SFNode, (types.InstanceType, 'SFNode', None)), \
		'MFInt32': (MFInt32, ()), \
		'SFImage': (MFInt32, ()), \
		'MFTime': (MFFloat, ()), \
		'MFFloat': (MFFloat, ()), \
		'MFColor': (MFCompoundNumber, ('SFColor', float)), \
		'MFVec2f': (MFCompoundNumber, ('SFVec2f', float)), \
		'MFVec3f': (MFCompoundNumber, ('SFVec3f', float)), \
		'SFRotation': (SFRotation, ()), \
		'MFRotation': (MFCompoundNumber, ('SFRotation', float)), \
		'MFNode': (MFNode, (types.InstanceType, 'MFNode', None)) \
	}
	  
FIELDCOERCE = FieldCoercian ()
