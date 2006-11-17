class IDProperty:
	"""
	The IDProperty wrapper type
	===========================
	@ivar name: the name of the property
	@ivar type: the property type (is read-only)
	@ivar data: the property's data.  
	"""
	
class IDGroup:
	"""
	The IDGroup wrapper type
	========================
	This type supports both iteration and the []
	operator to get child ID properties.
	
	You can also add new properties using the [] operator.
	For example:
	
	group['a float!'] = 0.0
	group['an int!'] = 0
	group['a string!'] = "hi!"
	group['an array!'] = [0, 0, 1.0, 0] #note that any floats in the list
	                                   #makes the whole list a float array.
	group['a subgroup!] = {"float": 0.0, "an int": 1.0, "an array": [1, 2], \
	  "another subgroup": {"a": 0.0, "str": "bleh"}}
	 
	you also do del group['item']
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
	The IDArray wrapper type
	========================
	
	@ivar type: returns the type of the array, can be either IDP_Int or IDP_Float
	"""
	
	def __getitem__(self):
		pass
	
	def __setitem__(self):
		pass
	
	def __len__(self):
		pass
	