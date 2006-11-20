class IDProperty:
	"""
	The IDProperty Type
	===================
	@ivar name: The name of the property
	@type name: string
	@ivar type: The property type (is read-only)
	@type type: int
	@ivar data: The property's data.  This data can be of several forms, depending on the 
	ID property type:
	
		1. For arrays, data implements the [] and allows editing of the array.
		2. For groups, data allows iteration through the group, and access using the [] 
		operator (but note that you can access a group that way through the parent IDProperty too).
		See L{IDGroup<IDGroup>}.
		3. For strings/ints/floats, data just holds the value and can be freely modified.
	"""
	
class IDGroup:
	"""
	The IDGroup Type
	================
	This type supports both iteration and the []
	operator to get child ID properties.
	
	You can also add new properties using the [] operator.
	For example:
	
	group['a float!'] = 0.0
	group['an int!'] = 0
	group['a string!'] = "hi!"
	group['an array!'] = [0, 0, 1.0, 0]
	
	group['a subgroup!] = {"float": 0.0, "an int": 1.0, "an array": [1, 2], \
	  "another subgroup": {"a": 0.0, "str": "bleh"}}
	 
	Note that for arrays, the array type defaults to int unless a float is found
	while scanning the template list; if any floats are found, then the whole
	array is float.
	"""
	
	def newProperty(type, name, array_type="Float", val=""):
		"""
		This function creates a new child ID property in the group.
		@type type: an int or a string
		@param type: The ID property type.  Can be:
			"String" or Blender.IDPropTypes['String']
			"Int" or Blender.IDPropTypes['Int']
			"Float" or Blender.IDPropTypes['Float']
			"Array" or Blender.IDPropTypes['Array']
			"Group" or Blender.IDPropTypes['Group']
		"""
	
	def deleteProperty(prop):
		"""
		deletes a property, takes either a name or a reference
		as an argument.
		"""
		
class IDArray:
	"""
	The IDArray Type
	================
	
	@ivar type: returns the type of the array, can be either IDP_Int or IDP_Float
	"""
	
	def __getitem__(self):
		pass
	
	def __setitem__(self):
		pass
	
	def __len__(self):
		pass
	