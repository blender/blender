#
# Blender mid level modules
# author: strubi@blender.nl
#
# 

"""Shadow class module

  These classes shadow the internal Blender objects
  
  There is no need for you to use the shadow module really - it is
  just there for documentation. Blender object classes with a common
  subset of function members derive from these sub classes.
"""


def _List(list, Wrapper):
	"""This function returns list of wrappers, taking a list of raw objects
and the wrapper method"""	
	return map(Wrapper, list)

def _getModeBits(dict, attr):
	list = []
	for k in dict.keys():
		i = dict[k]
		if attr & i:
			list.append(k)
	return list		

def _setModeBits(dict, args):
		flags = 0
		try:
			for a in args:
				flags |= dict[a]
		except:
			raise TypeError, "mode must be one of %s" % dict.keys()
		return flags


def _link(self, data):
	"""Links Object 'self' with data 'data'. The data type must match
the Object's type, so you cannot link a Lamp to a mesh type Object"""
	try:
		self._object.link(data._object)
	except:
		print "Users:", self._object.users

class shadow: 
	"""This is the shadow base class"""
	_getters   = {}
	_setters   = {} 
	_emulation = {}

	def __init__(self, object):
		self._object = object

	def __getattr__(self, a):
		try:
			return getattr(self._object, a)
		except:	
			if self._emulation.has_key(a):
				return getattr(self._object, self._emulation[a])
			elif self._getters.has_key(a):
				return self._getters[a](self)
			else:
				raise AttributeError, a

	def __setattr__(self, a, val):
		if a == "_object":
			self.__dict__['_object'] = val
			return

		try:
			setattr(self.__dict__['_object'], a, val)
		except:
			if self._emulation.has_key(a):
				setattr(self.__dict__['_object'], self._emulation[a], val)
			elif self._setters.has_key(a):
				self._setters[a](self, val)
			else:
				raise AttributeError, a
	link = _link

	def rename(self, name):
		"""Tries to set the name of the object to 'name'. If the name already
exists, a unique name is created by appending a version number (e.g. '.001')
to 'name'. The effective name is returned."""
		self._object.name = name
		return self._object.name

def _getattrEx(self, a):
	if self._emulation.has_key(a):
		return getattr(self._object, self._emulation[a])
	elif self._getters.has_key(a):
		return self._getters[a](self)
	else:
		return getattr(self._object, a)

class shadowEx: 
	"""This is the shadow base class with a minor change; check for 
emulation attributes happens before access to the raw object's attributes"""
	_getters   = {}
	_setters   = {} 
	_emulation = {}

	def __del__(self):
		self.__dict__.clear()

	def __init__(self, object):
		self._object = object

	def __getattr__(self, a):
		return _getattrEx(self, a)

	def __setattr__(self, a, val):
		if a == "_object":
			self.__dict__['_object'] = val
			return

		if self._emulation.has_key(a):
			setattr(self.__dict__['_object'], self._emulation[a], val)
		elif self._setters.has_key(a):
			self._setters[a](self, val)
		else:
			setattr(self.__dict__['_object'], a, val)

	def __repr__(self):
		return repr(self._object)

	def rename(self, name):
		"""Tries to set the name of the object to 'name'. If the name already
exists, a unique name is created by appending a version number (e.g. '.001')
to 'name'. The effective name is returned."""
		self._object.name = name
		return self._object.name

	link = _link

class hasIPO(shadowEx):
	"""Object class which has Ipo curves assigned"""

	def getIpo(self):
		"Returns the Ipo assigned to 'self'"
		import Ipo
		return Ipo.IpoBlock(self._object.ipo)

	def setIpo(self, ipo):
		"Assigns the IpoBlock 'ipo' to 'self'"
		return self._object.assignIpo(ipo._object)

	def __getattr__(self, a):
		if a == "ipo":
			print "ipo member access deprecated, use self.getIpo() instead!"
			return self.getIpo()
		else:
			return _getattrEx(self, a)

class hasModes(shadowEx):
	"""Object class which has different Modes"""
	def getMode(self):
		"""Returns a list of the modes which are set for 'self'"""
		list = []
		for k in self.Modes.keys():
			i = self.Modes[k]
			if self._object.mode & i:
				list.append(k)
		return list		

	def setMode(self, *args):
		"""Set the mode of 'self'. This function takes a variable number
of string arguments of the types listed in self.Modes"""
 		flags = 0
		try:
			for a in args:
				flags |= self.Modes[a]
		except:
			raise TypeError, "mode must be one of" % self.Modes.keys()
		self._object.mode = flags

class dict:
	"""readonly dictionary shadow"""
	_emulation = {}

	def __init__(self, dict):
		self._dict = dict

	def __getitem__(self, key):
		try:
			return self._dict[key]
		except:
			key = _emulation[key]
			return self._dict[key]

	def __repr__(self):
		return repr(self._dict)
