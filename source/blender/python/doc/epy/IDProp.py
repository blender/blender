class IDGroup:
	"""
	The IDGroup Type
	================
	This type supports both iteration and the []
	operator to get child ID properties.
	
	You can also add new properties using the [] operator.
	For example::
	
		group['a float!'] = 0.0
		group['an int!'] = 0
		group['a string!'] = "hi!"
		group['an array!'] = [0, 0, 1.0, 0]
		
		group['a subgroup!] = {"float": 0.0, "an int": 1.0, "an array": [1, 2],
		  "another subgroup": {"a": 0.0, "str": "bleh"}}
	
	Note that for arrays, the array type defaults to int unless a float is found
	while scanning the template list; if any floats are found, then the whole
	array is float.  Note that double-precision floating point numbers are used for
	python-created float ID properties and arrays (though the internal C api does 
	support single-precision floats, and the python code will read them).
	
	You can also delete properties with the del operator.  For example:
	
	del group['property']
	
	To get the type of a property, use the type() operator, for example::
	
		if type(group['bleh']) == str: pass
	
	To tell if the property is a group or array type, import the Blender.Types module and test
	against IDGroupType and IDArrayType, like so::
	
		from Blender.Types import IDGroupType, IDArrayType.
	
		if type(group['bleghr']) == IDGroupType:
			(do something)
	
	@ivar name: The name of the property
	@type name: string
	"""
	
	def pop(item):
		"""
		Pop an item from the group property.
		@type item: string
		@param item: The item name.
		@rtype: can be dict, list, int, float or string.
		@return: The removed property.	
		"""
		
	def update(updatedict):
		"""
		Updates items in the dict, similar to normal python
		dictionary method .update().
		@type updatedict: dict
		@param updatedict: A dict of simple types to derive updated/new IDProperties from.
		@rtype: None
		@return: None
		"""
	
	def keys():
		"""
		Returns a list of the keys in this property group.
		@rtype: list of strings.
		@return: a list of the keys in this property group.	
		"""
	
	def values():
		"""
		Returns a list of the values in this property group.
		
		Note that unless a value is itself a property group or an array, you 
		cannot change it by changing the values in this list, you must change them
		in the parent property group.
		
		For example,
		
		group['some_property'] = new_value
		
		. . .is correct, while,
		
		values = group.values()
		values[0] = new_value
		
		. . .is wrong.
		
		@rtype: list of strings.
		@return: a list of the values in this property group.
		"""
	
	def iteritems():
		"""
		Implements the python dictionary iteritmes method.
		
		For example::

			for k, v in group.iteritems():
				print "Property name: " + k
				print "Property value: " + str(v)
			
		@rtype: an iterator that spits out items of the form [key, value]
		@return: an iterator.	
		"""
	
	def convert_to_pyobject():
		"""
		Converts the entire property group to a purely python form.
		
		@rtype: dict
		@return: A python dictionary representing the property group
		"""
		
class IDArray:
	"""
	The IDArray Type
	================
	
	@ivar type: returns the type of the array, can be either IDP_Int or IDP_Float
	"""
	
	def __getitem__(index):
		pass
	
	def __setitem__(index, value):
		pass
	
	def __len__():
		pass
	
