
"""This is a basic scenegraph module for Blender
It contains low level API calls..."""

# (c) 2001, Martin Strubel // onk@section5.de

from util import quat #quaternions

from Blender import Object, Lamp, Scene


TOLERANCE = 0.01

def uniform_scale(vec):
	v0 = vec[0]
	d = abs(vec[1] - v0)
	if d > TOLERANCE:
		return 0
	d = abs(vec[2] - v0) 
	if d > TOLERANCE:
		return 0
	return v0

class Transform:
	"""An abstract transform, containing translation, rotation and scale information"""
	def __init__(self):
		self.scale = (1.0, 1.0, 1.0)
		self.translation = (0.0, 0.0, 0.0)
		self.rotation = quat.Quat()
		self.scaleOrientation = quat.Quat() # axis, angle
		self.parent = None
	def __mul__(self, other):
		s = uniform_scale(self.scale)
		if not s:
			raise RuntimeError, "non uniform scale, can't multiply"
		t = Transform()
		sc = other.scale
		t.scale = (s * sc[0], s * sc[1], s * sc[2])
		t.rotation = self.rotation * other.rotation
		tr = s * apply(quat.Vector, other.translation) 
		t.translation = self.rotation.asMatrix() * tr + self.translation
		return t
	def getLoc(self):
		t = self.translation
		return (t[0], t[1], t[2]) # make sure it's a tuple..silly blender
	def calcRotfromAxis(self, axisrotation):
		self.rotation = apply(quat.fromRotAxis,axisrotation)
	def getRot(self):
		return self.rotation.asEuler()
	def getSize(self):
		s = self.scale
		return (s[0], s[1], s[2])
	def __repr__(self):
		return "Transform: rot: %s loc:%s" % (self.getRot(), self.getLoc())
	def copy(self):
		"returns copy of self"
		t = Transform()
		t.scale = self.scale
		t.translation = self.translation
		t.rotation = self.rotation
		t.scaleOrientation  = self.scaleOrientation
		return t

class BID:
	"Blender named Object ID"
	def __init__(self, name):
		self.name = name
		self.data = None

class BScene:
	def __init__(self, name = None):
		from Blender import Scene
		self.dict = {'Image': {}, 'Object':{}, 'Mesh' : {}}
		self.name = name
	def __getitem__(self, name):
		return self.dict[name]
	def __setitem__(self, name, val):
		self.dict[name] = val
	def has_key(self, name):
		if self.dict.has_key(name):
			return 1
		else:
			return 0
	def getnewID(self, templ):
		n = 0
		name = templ
		while self.dict.has_key(name):
			n += 1
			name = "%s.%03d" % (templ, n)
		return name	
			
class BSGNode:
	"Blender Scenegraph node"
	isRoot = 0
	def __init__(self, object = None, type = "", name = ""):
		self.type = type
		self.name = name
		self.children = []
		self.level = 0
		self.object = object
	def addChildren(self, children):
		self.children += children
	def traverse(self, visitor):
		ret = visitor()
		for c in self.children:
			c.traverse(visitor)
		return ret
	def setDepth(self, level):
		self.level = level
		for c in self.children:
			c.setDepth(level + 1)
	def update(self):
		ob.name = self.name
	def __repr__(self):
		l = self.level
		children = ""
		pre = l * ' '
		return "\n%s%s [%s] ->%s" % (pre, self.name, self.type, self.children)

class ObjectNode(BSGNode):
	def __init__(self, object = None, type = "", name = ""):
		self.transform = Transform()
		self.scene = Scene.getCurrent()
		BSGNode.__init__(self, object, type, name)
	def makeParent(self, child):
		self.child = parent
		child.parent = self
	def clone(self):
		ob = self.object
		newob = ob.copy()
		self.scene.link(newob)
		new = ObjectNode(newob)
		new.transform = self.transform.copy()
		return new
	def insert(self, child):
		self.children.append(child)
		child.level = self.level + 1
		ob = child.object
		self.object.makeParent([ob], 1, 1)
		# first parent, THEN set local transform
		child.update()
	def applyTransform(self, tf):
		self.transform = tf * self.transform
	def update(self):
		ob = self.object
		t = self.transform
		ob.loc = t.getLoc()
		ob.size = t.getSize()
		ob.rot = t.getRot()
		ob.name = self.name

def NodefromData(ob, type, name):
	new = ObjectNode(None, type, name)
	if ob:
		obj = ob
	else:
		obj = Object.New(type)
		Scene.getCurrent().link(obj)
		if not obj:
			raise RuntimeError, "FATAL: could not create object"
	new.object= obj
	new.object.name = name
	#new.fromData(ob)
	return new

class RootNode(ObjectNode):
	"""stupid simple scenegraph prototype"""
	level = 0
	isRoot = 1
	type = 'Root'
	name = 'ROOT'

	def __init__(self, object = None, type = "", name = ""):
		from Blender import Scene
		self.transform = Transform()
		BSGNode.__init__(self, object, type, name)
		self.scene = Scene.getCurrent()
	def insert(self, child):
		child.update()
		self.children.append(child)
	def update(self):
		self.scene.update()
