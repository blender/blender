##
##  Blender API mid level layer   01/2002 // strubi@blender.nl
##
##  $Id$
##

"""The Blender Object module

  This module provides **Object** manipulation routines.

  Example::
  
    from Blender import Object
    ob = Object.get('Plane')
    actobj = Object.getSelected()[0] # get active Object
    print actobj.loc                 # print position
    ob.makeParent([actobj])          # make ob the parent of actobj
"""

#import _Blender.Object as _Object

import shadow
reload(shadow)   # XXX

class _C:
	pass

InstanceType = type(_C())	
del _C  # don't export this


def _Empty_nodata(obj):
	return None

class Object(shadow.hasIPO):
	"""Blender Object

    A Blender Object (note the capital O) is the instance of a 3D structure,
    or rather, the Object that is (normally) visible in your Blender Scene.

    An instance of a Blender Object object is created by::

      from Blender import Object
      ob = Object.New(type)      # type must be a valid type string,
                                 # see Object.Types

    ...

  Attributes

    Note that it is in general not recommended to access the Object's
    attributes directly. Please rather use the get-/set- functions instead.

     loc    -- position vector (LocX, LocY, LocZ)

     dloc   -- delta position vector (dLocX, dLocY, dLocZ)

     rot    -- euler rotation vector (RotX, RotY, RotZ). 
             Warning: this may change in future.

     drot   -- delta rotation euler vector (dRotX, dRotY, dRotZ)
             Warning: this may change in future.

     size   -- scale vector (SizeX, SizeY, SizeZ)

     dsize  -- delta scale vector (dSizeX, dSizeY, dSizeZ)

     layer  -- layer bitvector (20 bit), defining what layers the object is 
             visible in
			 

   The following items are listed here only for compatibility to older
   scripts and are READ-ONLY! **USE the get- functions instead!**
	 
     data    -- reference to the data object (e.g. Mesh, Camera, Lamp, etc.)

     parent  -- reference to the parent object, if existing, 'None' otherwise.

     track   -- reference to the tracked object, if existing, 'None' otherwise.

   This bit mask can be read and written:	

     colbits -- the Material usage mask. A set bit #n means: 
                The Material #n in the *Object's* material list is used.
                Otherwise, the Material #n of the Objects *Data* material list
                is displayed.
"""

	def __init__(self, object = None):
		"""Returns an empty shadow Object"""
		self._object = object

	def __repr__(self):
		return "[Object \"%s\"]" % self.name

	def link(self, data):
		"""Links Object 'self' with data 'data'. The data type must match
the Object's type, so you cannot link a Lamp to a mesh type Object.
'data' can also be an Ipo object (IpoBlock)
"""
		#from _Blender import Types
		# special case for NMesh:
		if type(data) == Types.NMeshType:
			return self._object.link(data)
		elif type(data) == InstanceType:
			if data.__class__.__name__ == "rawMesh":
				data.update() # update mesh
			elif data.__class__.__name__ == "IpoBlock":
				self.setIpo(data)

		return shadow._link(self, data)	

	def copy(self):	
		"""Returns a copy of 'self'.
This is a true, linked copy, i.e. the copy shares the same data as the
original. The returned object is *free*, meaning, not linked to any scene."""
		return Object(self._object.copy())

	#def clone(self):
		#"""Makes a clone of the specified object in the current scene and
##returns its reference"""
		#return Object(self._object.clone())

	def shareFrom(self, object):
		"""Link data of 'self' with data of 'object'. This works only if
'object' has the same type as 'self'."""
		return Object(self._object.shareFrom(object._object))

	def getMatrix(self):
		"""Returns the object matrix"""
		return self._object.getMatrix()

	def getInverseMatrix(self):
		"""Returns the object's inverse matrix"""
		return self._object.getInverseMatrix()

	def getData(self):
		"Returns the Datablock object containing the object's data, e.g. Mesh"
		t = self._object.getType()
		data = self._object.data
		try:
			return self._dataWrappers[t][1](data)
		except:
			raise TypeError, "getData() not yet supported for this object type"

	def getDeformData(self):
		"""Returns the Datablock object containing the object's deformed data.
Currently, this is only supported for a Mesh"""
		#import _Blender.NMesh as _NMesh
		t = self._object.getType()
		if t == self.Types['Mesh']:
			data = _NMesh.GetRawFromObject(self.name)
			return self._dataWrappers[t][1](data)
		else:
			raise TypeError, "getDeformData() not yet supported for this object type"
			
	def getType(self):
		"Returns type string of Object, which is one of Object.Types.keys()"
		t = self._object.getType()
		try:
			return self._dataWrappers[t][0]
		except:
			return "<unsupported>"

	def getParent(self):
		"Returns object's parent object"
		if self._object.parent:
			return Object(self._object.parent)
		return None		

	def getTracked(self):
		"Returns object's tracked object"
		if self._object.track:
			return Object(self._object.track)
		return None

# FUTURE FEATURE :-) :
# def getLocation():
#      """Returns the object's location (x, y, z). 
#By default, the location vector is always relative to the object's parent. 
#If the location of another coordinate system is wanted, specify 'origin' by
#the object whose coordinate system the location should be calculated in.

#If world coordinates are wanted, set 'relative' = "World"."""

	def getLocation(self, relative = None):
		"""Returns the object's location (x, y, z). For the moment,
'relative' has no effect."""
		l = self._object.loc
		return (l[0], l[1], l[2])

	def setLocation(self, location, relative = None):
		"""Sets the object's location. 'location' must be a vector triple.
See 'getLocation()' about relative coordinate systems."""
		l = self._object.loc   # make sure this is copied
		l[0], l[1], l[2] = location

	def getDeltaLocation(self):
		"""Returns the object's delta location (x, y, z)"""
		l = self._object.dloc
		return (l[0], l[1], l[2])

	def setDeltaLocation(self, delta_location):
		"""Sets the object's delta location which must be a vector triple"""
		l = self._object.dloc   # make sure this is copied
		l[0], l[1], l[2] = delta_location

	def getEuler(self):
		"""Returns the object's rotation as Euler rotation vector 
(rotX, rotY, rotZ)"""
		e = self._object.rot
		return (e[0], e[1], e[2])

	def setEuler(self, euler = (0.0, 0.0, 0.0)):
		"""Sets the object's rotation according to the specified Euler angles.
'euler' must be a vector triple"""
		e = self._object.rot
		e[0], e[1], e[2] = euler

	def makeParent(self, objlist, mode = 0, fast = 0):
		"""Makes 'self' the parent of the objects in 'objlist' which must be
a list of valid Objects.
If specified:

  mode     -- 0: make parent with inverse

              1: without inverse

  fast     -- 0: update scene hierarchy automatically

              1: don't update scene hierarchy (faster). In this case, you
                 must explicitely update the Scene hierarchy, see:
                 'Blender.Scene.getCurrent().update()'"""
		list = map(lambda x: x._object, objlist)
		return Object(self._object.makeParent(list, mode, fast))

	def clrParent(self, mode = 0, fast = 0):
		"""Clears parent object.
If specified:

  mode     -- 2: keep object transform

  fast > 0 -- don't update scene hierarchy (faster)"""
		return Object(self._object.clrParent(mode, fast))

	def getMaterials(self):
		"""Returns list of materials assigned to the object"""
		from Blender import Material
		return shadow._List(self._object.getMaterials(), Material.Material)

	def setMaterials(self, materials = []):
		"""Sets materials. 'materials' must be a list of valid material objects"""
		o = self._object
		old_mask = o.colbits
		o.colbits = -1  # set material->object linking
		o.setMaterials(map(lambda x: x._object, materials))
		o.colbits = old_mask

	def materialUsage(self, flag):
		"""Determines the way the material is used and returns status. 

'flag' = 'Data'   : Materials assigned to the object's data are shown. (default)

'flag' = 'Object' : Materials assigned to the object are shown. 

The second case is desired when the object's data wants to be shared among
objects, but not the Materials assigned to their data. See also 'colbits'
attribute for more (and no future compatible) control."""
		if flag == "Object":
			self._object.colbits = -1
		elif flag == "Data":
			self._object.colbits = 0
			return self._object.colbits
		else:
			raise TypeError, "unknown mode %s" % flag

	_getters = {}	 

	from Blender import Mesh, Camera, Lamp

	t = _Object.Types
	Types = {"Camera"   : t.CAMERA,
			 "Empty"    : t.EMPTY,
			 "Lamp"     : t.LAMP,
			 "Mesh"     : t.MESH,
			} 

	# create lookup table for data wrappers
	_dataWrappers = range(max(Types.values()) + 1)
	_dataWrappers[t.MESH] = ("Mesh", Mesh.rawMesh)
	_dataWrappers[t.CAMERA] = ("Camera", Camera.Camera)
	_dataWrappers[t.LAMP] = ("Lamp", Lamp.Lamp)
	_dataWrappers[t.EMPTY] = ("Empty", _Empty_nodata)

	t = _Object.DrawTypes
	DrawTypes = {"Bounds"     : t.BOUNDBOX,
			     "Wire"       : t.WIRE,
			     "Solid"      : t.SOLID,
			     "Shaded"     : t.SHADED,
				} 

	t = _Object.DrawModes
	DrawModes = {"axis"       : t.AXIS,
			     "boundbox"   : t.BOUNDBOX,
			     "texspace"   : t.TEXSPACE,
			     "name"       : t.NAME,
				} 
 

	del t
	del Mesh, Camera, Lamp

	def getDrawMode(self):
		"""Returns the Object draw modes as a list of strings"""
		return shadow._getModeBits(self.DrawModes, self._object.drawMode)

	def setDrawMode(self, *args):
		"""Sets the Object's drawing modes as a list of strings"""
		self._object.drawMode = shadow._setModeBits(self.DrawModes, args)

	def getDrawType(self):
		"""Returns the Object draw type"""
		for k in self.DrawTypes.keys():
			if self.DrawTypes[k] == self.drawType:
				return k

	def setDrawType(self, name):
		"""Sets the Object draw type. 'name' must be one of:

* 'Bounds' : Draw bounding box only

* 'Wire'   : Draw in wireframe mode

* 'Solid'  : Draw solid

* 'Shaded' : Draw solid, shaded and textures
"""
		try:
			self._object.drawType = self.DrawTypes[name]
		except:
			raise TypeError, "type must be one of %s" % self.DrawTypes.keys()


##################
# MODULE FUNCTIONS

def New(objtype, name = None):
	"""Creates a new, empty object and returns it. 
'objtype' is a string and must be one of::

  Camera
  Empty
  Mesh
  Lamp

More object types will be supported in future.

Example::

  ob = Object.New('Camera')
"""

	if type(objtype) == type(0):
		obj =  Object(_Object.New(objtype))  # emulate old syntax
	else:
		t = Object.Types[objtype]
		obj =  Object(_Object.New(t))
	return obj

def get(name = None):
	"""If 'name' given, the Object 'name' is returned if existing, 'None' otherwise.
If no name is given, a list of all Objects is returned"""
	if name:
		ob = _Object.get(name)
		if ob:
			return Object(ob)
		else:
			return None
	else:
		return shadow._List(_Object.get(), Object)

Get = get  # emulation

def getSelected():
	"""Returns a list of selected Objects in the active layer(s).
The active object is the first in the list, if visible"""
	return shadow._List(_Object.getSelected(), Object)

GetSelected = getSelected  # emulation

Types = _Object.Types  # for compatibility
