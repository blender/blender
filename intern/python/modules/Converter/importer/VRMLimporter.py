# VRML import prototype
#
# strubi@blender.nl
#

"""VRML import module

  This is a prototype for VRML97 file import

  Supported:

  - Object hierarchies, transform collapsing (optional)
  
  - Meshes (IndexedFaceSet, no Basic primitives yet)

  - Materials

  - Textures (jpg, tga), conversion option from alien formats

"""

import Blender.sys as os                # Blender os emulation
from beta import Scenegraph 

Transform = Scenegraph.Transform

import beta.Objects

_b = beta.Objects

#from Blender import Mesh
Color = _b.Color
DEFAULTFLAGS = _b.DEFAULTFLAGS
FACEFLAGS = _b.FACEFLAGS
shadowNMesh = _b.shadowNMesh

quat = Scenegraph.quat                # quaternion math
vect = quat.vect                      # vector math module
from vrml import loader

#### GLOBALS 

OB = Scenegraph.Object.Types  # CONST values
LA = Scenegraph.Lamp.Types

g_level = 1
g_supported_fileformats = ["jpg", "jpeg", "tga"]

#### OPTIONS

OPTIONS = {'cylres' : 16,        # resolution of cylinder
           'flipnormals' : 0,    # flip normals (force)
		   'mat_as_vcol' : 0,    # material as vertex color - warning, this increases mem usage drastically on big files
		   'notextures' : 0,     # no textures - saves some memory
		   'collapseDEFs' : 0,   # collapse DEF nodes
		   'collapseTF' : 0,     # collapse Transforms (as far as possible,
		                         # i.e. currently to Object transform level)
		  }

#### CONSTANTS

LAYER_EMPTY = (1 << 2)
LAYER_LAMP = (1 << 4)
LAYER_CAMERA = 1 + (1 << 4)

CREASE_ANGLE_THRESHOLD = 0.45 # radians

PARSE_TIME = (loader.parser.IMPORT_PARSE_TIME )
PROCESS_TIME = (1.0 - PARSE_TIME )
PROGRESS_DEPTH = loader.parser.PROGRESS_DEPTH
VERBOSE_DEPTH = PROGRESS_DEPTH

#### DEBUG

def warn(text):
	print "###", text

def debug2(text):
	print (g_level - 1) * 4 * " " + text

def verbose(text):
	print text

def quiet(text):
	pass

debug = quiet

#### ERROR message filtering:

g_error = {} # dictionary for non-fatal errors to mark whether an error
             # was already reported

def clrError():
	global g_error
	g_error['toomanyfaces'] = 0

def isError(name):
	return g_error[name]

def setError(name):
	global g_error
	g_error[name] = 1

#### ERROR handling

class baseError:
	def __init__(self, value):
		self.value = value
	def __str__(self):
		return `self.value`

class MeshError(baseError):
	pass

UnfinishedError = loader.parser.UnfinishedError

##########################################################
# HELPER ROUTINES

def assignImage(f, img):
	f.image = img

def assignUV(f, uv):
	if len(uv) != len(f.v):
		uv = uv[:len(f.v)]
		#raise MeshError, "Number of UV coordinates does not match number of vertices in face"
	f.uv = []
	for u in uv:
		f.uv.append((u[0], u[1])) # make sure it's a tuple


#### VRML STUFF

# this is used for transform collapsing
class TransformStack:
	def __init__(self):
		self.stack = [Transform()]
	def push(self, t):
		self.stack.append(t)
	def pop(self):
		return self.stack.pop()
	def last(self):
		return self.stack[-1]

def fromVRMLTransform(tfnode):
	t = Transform()
	s = tfnode.scale
	t.scale = (s[0], s[1], s[2])
	r = tfnode.rotation
	if r[0] == 0.0 and r[1] == 0.0 and r[2] == 0.0:
		rotaxis = (0.0, 0.0, 1.0)
		ang = 0.0
	else:
		rotaxis = vect.norm3(r[:3])
		ang = r[3]

	#t.rotation = (rotaxis, ang)
	t.calcRotfromAxis((rotaxis, ang))
	tr = tfnode.translation
	t.translation = (tr[0], tr[1], tr[2])
	# XXX more to come..
	return t


### TODO: enable material later on
#class dummyMaterial:
	#def setMode(self, *args):
		#pass
	
def fromVRMLMaterial(mat):
	name = mat.DEF
	from Blender import Material
	m = Material.New(name)

	m.rgbCol = mat.diffuseColor
	m.alpha = 1.0 - mat.transparency
	m.emit = vect.len3(mat.emissiveColor)
	if m.Emit > 0.01:
		if vect.cross(mat.diffuseColor, mat.emissiveColor) > 0.01 * m.Emit:
			m.rgbCol = mat.emissiveColor

	m.ref = 1.0
	m.spec = mat.shininess
	m.specCol = mat.specularColor
	m.amb = mat.ambientIntensity
	return m

# override:
#def fromVRMLMaterial(mat):
#	return dummyMaterial()

def buildVRMLTextureMatrix(tr):
	from math import sin, cos
	newMat = vect.Matrix
	newVec = vect.Vector
	# rotmatrix
	s = tr.scale
	t = tr.translation
	c = tr.center

	phi = tr.rotation

	SR = newMat()
	C = newMat()
	C[2] = newVec(c[0], c[1], 1.0)

	if abs(phi) > 0.00001:
		SR[0] = newVec(s[0] * cos(phi), s[1] * sin(phi), 0.0)
		SR[1] = newVec(-s[0] * sin(phi), s[1] * cos(phi), 0.0)
	else:
		SR[0] = newVec(s[0], 0.0, 0.0)
		SR[1] = newVec(0.0, s[1], 0.0)

	SR = C * SR * C.inverse()  # rotate & scale about rotation center

	T = newMat()
	T[2] = newVec(t[0], t[1], 1.0)
	return SR * T # texture transform matrix

def imageConvert(fromfile, tofile):
	"""This should convert from a image file to another file, type is determined
automatically (on extension). It's currently just a stub - users can override
this function to implement their own converters"""
	return 0 # we just fail in general

def addImage(path, filename):
	"returns a possibly existing image which is imported by Blender"
	from Blender import Image
	img = None
	try:
		r = filename.rindex('.')
	except:
		return None

	naked = filename[:r]
	ext = filename[r+1:].lower()

	if path:
		name = os.sep.join([path, filename])
		file = os.sep.join([path, naked])
	else:
		name = filename
		file = naked

	if not ext in g_supported_fileformats:
		tgafile = file + '.tga'
		jpgfile = file + '.jpg'
		for f in tgafile, jpgfile: # look for jpg, tga
			try:
				img = Image.Load(f)
				if img:
					verbose("couldn't load %s (unsupported).\nFound %s instead" % (name, f))
					return img
			except IOError, msg:
				pass
		try:
			imgfile = open(name, "rb")
			imgfile.close()
		except IOError, msg:
			warn("Image %s not found" % name)
			return None

		verbose("Format unsupported, trying to convert to %s" % tgafile)
		if not imageConvert(name, tgafile):
			warn("image conversion failed")
			return None
		else:
			return Image.Load(tgafile)
		return None # failed
	try:
		img = Image.Load(name)
	except IOError, msg:
		warn("Image %s not found" % name)
	return img
	# ok, is supported

def callMethod(_class, method, vnode, newnode, warn = 1):
	meth = None
	try:
		meth = getattr(_class, method)
	except AttributeError:
		if warn:
			unknownType(method)
		return None, None
	if meth:
		return meth(vnode, parent = newnode)

def unknownType(type):
	warn("unsupported:" + repr(type))

def getChildren(vnode):		
	try:
		children = vnode.children
	except:
		children = None
	return children

def getNodeType(vnode):
	return vnode.__gi__

GroupingNodeTypes = ["Group", "Collision", "Anchor", "Billboard", "Inline",
                     "LOD", "Switch", "Transform"]

################################################################################
#
#### PROCESSING CLASSES


class NullProcessor:
	def __init__(self, tstack = TransformStack()):
		self.stack = tstack
		self.walker = None
		self.mesh = None
		self.ObjectNode = Scenegraph.NodefromData # may be altered...
		self.MaterialCache = {}
		self.ImageCache = {}

# This is currently not used XXX
class DEFcollapser(NullProcessor):
	"""This is for collapsing DEF Transform nodes into a single object"""
	def __init__(self):
		self.collapsedNodes = []

	def Transform(self, curnode, parent, **kw):
		name = curnode.DEF
		if not name: # node is a DEF node
			return None, None

		return children, None
		
		
class Processor(NullProcessor):
	"""The processor class defines the handler for a VRML Scenegraph node.
Definition of a handler method simply happens by use of the VRML Scenegraph
entity name.

A handler usually creates a new Scenegraph node in the target scenegraph, 
converting the data from the given VRML node.

A handler takes the arguments:

	curnode: the currently visited VRML node
	parent:  the previously generated target scenegraph parent node
	**kw: additional keywords
	
It MUST return: (children, newBnode) where:
	children: the children of the current VRML node. These will be further
	          processed by the processor. If this is not wanted (because they
			  might have been processed by the handler), None must be returned.
	newBnode: the newly created target node	or None.
	"""

	def _handleProto(self, curnode, parent, **kw):
		p = curnode.PROTO
		if not p.sceneGraph:
			print curnode.__gi__, "unsupported"
			return None, None

	def _dummy(self, curnode, parent, **kw):
		print curnode.sceneGraph
		return None, None

	#def __getattr__(self, name):
		#"""If method is not statically defined, look up prototypes"""
		#return self._handleProto

	def _currentTransform(self):
		return self.stack.last()
		
	def _parent(self, curnode, parent, trans):
		name = curnode.DEF
		children = getChildren(curnode)
		debug("children: %s" % children)
		objects = []
		transforms = []
		groups = []
		isempty = 0
		for c in children:
			type = getNodeType(c)
			if type == 'Transform':
				transforms.append(c)
			elif type in GroupingNodeTypes:
				groups.append(c)
			#else:
			elif hasattr(self, type):
				objects.append(c)
		if transforms or groups or len(objects) != 1:
			# it's an empty
			if not name:
				name = 'EMPTY'
			Bnode = self.ObjectNode(None, OB.EMPTY, name) # empty Blender Object node
			if options['layers']:
				Bnode.object.Layer = LAYER_EMPTY
			Bnode.transform = trans
			Bnode.update()
			isempty = 1
			parent.insert(Bnode)
		else: # don't insert extra empty if only one object has children
			Bnode = parent

		for node in objects:
			c, new = self.walker.walk(node, Bnode)
			if not isempty: # only apply transform if no extra transform empty in hierarchy
				new.transform = trans
			Bnode.insert(new)
		for node in transforms:
			self.walker.walk(node, Bnode)
		for node in groups:	
			self.walker.walk(node, Bnode)

		return None, None

	def sceneGraph(self, curnode, parent, **kw):
		parent.type = 'ROOT'
		return curnode.children, None

	def Transform(self, curnode, parent, **kw):
		# we support 'center' and 'scaleOrientation' by inserting
		# another Empty in between the Transforms

		t = fromVRMLTransform(curnode)
		cur = self._currentTransform()

		chainable = 0

		if OPTIONS['collapseTF']:
			try:
				cur = cur * t # chain transforms
			except:
				cur = self._currentTransform()
				chainable = 1

		self.stack.push(cur)

		# here comes the tricky hacky transformation conversion

		# TODO: SR not supported yet

		if chainable == 1: # collapse, but not chainable
			# insert extra transform:
			Bnode = self.ObjectNode(None, OB.EMPTY, 'Transform') # Empty
			Bnode.transform = cur
			parent.insert(Bnode)
			parent = Bnode

		c = curnode.center
		if c != [0.0, 0.0, 0.0]:
			chainable = 1
			trans = Transform()
			trans.translation = (-c[0], -c[1], -c[2])
			tr = t.translation
			t.translation = (tr[0] + c[0], tr[1] + c[1], tr[2] + c[2])

			Bnode = self.ObjectNode(None, OB.EMPTY, 'C') # Empty
			Bnode.transform = t
			parent.insert(Bnode)
			parent = Bnode
		else:
			trans = t

		if chainable == 2: # collapse and is chainable
			# don't parent, insert into root node:
			for c in getChildren(curnode):
				dummy, node = self.walker.walk(c, parent) # skip transform node, insert into parent
				if node: # a valid Blender node
					node.transform = cur
		else:
			self._parent(curnode, parent, trans)


		self.stack.pop()
		return None, None

	def Switch(self, curnode, parent, **kw):
		return None, None

	def Group(self, curnode, parent, **kw):
		if OPTIONS['collapseTF']: 
			cur = self._currentTransform()
			# don't parent, insert into root node:
			children = getChildren(curnode)
			for c in children:
				dummy, node = self.walker.walk(c, parent) # skip transform node, insert into parent
				if node: # a valid Blender node
					node.transform = cur
		else:	
			t = Transform()
			self._parent(curnode, parent, t)
		return None, None

	def Collision(self, curnode, parent, **kw):
		return self.Group(curnode, parent)

#	def LOD(self, curnode, parent, **kw):
#		c, node = self.walker.walk(curnode.level[0], parent)
#		parent.insert(node)
#		return None, None

	def Appearance(self, curnode, parent, **kw):
		# material colors:
		mat = curnode.material
		self.curColor = mat.diffuseColor
			
		name = mat.DEF
		if name:  
			if self.MaterialCache.has_key(name):
				self.curmaterial = self.MaterialCache[name]
			else:	
				m = fromVRMLMaterial(mat)
				self.MaterialCache[name] = m
				self.curmaterial = m
		else:
			if curnode.DEF:
				name = curnode.DEF
				if self.MaterialCache.has_key(name):
					self.curmaterial = self.MaterialCache[name]
				else:	
					m = fromVRMLMaterial(mat)
					self.MaterialCache[name] = m
					self.curmaterial = m
			else:
				self.curmaterial = fromVRMLMaterial(mat)

		try:	
			name = curnode.texture.url[0]
		except:
			name = None
		if name:	
			if self.ImageCache.has_key(name):
				self.curImage = self.ImageCache[name]
			else:	
				self.ImageCache[name] = self.curImage = addImage(self.curpath, name)
		else:
			self.curImage = None

		tr = curnode.textureTransform
		if tr:
			self.curtexmatrix = buildVRMLTextureMatrix(tr)
		else:
			self.curtexmatrix = None
		return None, None

	def Shape(self, curnode, parent, **kw):
		name = curnode.DEF
		debug(name)
		#self.mesh = Mesh.rawMesh()
		self.mesh = shadowNMesh()
		self.mesh.name = name
		
		# don't mess with the order of these..
		if curnode.appearance:
			self.walker.preprocess(curnode.appearance, self.walker.preprocessor)
		else:
			# no appearance, get colors from shape (vertex colors)
			self.curColor = None
			self.curImage = None
		self.walker.preprocess(curnode.geometry, self.walker.preprocessor)

		if hasattr(self, 'curmaterial'):
			self.mesh.assignMaterial(self.curmaterial)

		meshobj = self.mesh.write()  # write mesh
		del self.mesh
		bnode = Scenegraph.ObjectNode(meshobj, OB.MESH, name) 
		if name:
			curnode.setTargetnode(bnode) # mark as already processed
		return None, bnode

	def Box(self, curnode, parent, **kw):
		col = apply(Color, self.curColor)
	
		faces = []
		x, y, z = curnode.size
		x *= 0.5; y *= 0.5; z *= 0.5
		name = curnode.DEF
		m = self.mesh
		v0 = m.addVert((-x, -y, -z))
		v1 = m.addVert(( x, -y, -z))
		v2 = m.addVert(( x,  y, -z))
		v3 = m.addVert((-x,  y, -z))
		v4 = m.addVert((-x, -y,  z))
		v5 = m.addVert(( x, -y,  z))
		v6 = m.addVert(( x,  y,  z))
		v7 = m.addVert((-x,  y,  z))

		flags = DEFAULTFLAGS
		if not self.curImage:
			uvflag = 1
		else:
			uvflag = 0

		m.addFace([v3, v2, v1, v0], flags, uvflag)
		m.addFace([v0, v1, v5, v4], flags, uvflag)
		m.addFace([v1, v2, v6, v5], flags, uvflag)
		m.addFace([v2, v3, v7, v6], flags, uvflag)
		m.addFace([v3, v0, v4, v7], flags, uvflag)
		m.addFace([v4, v5, v6, v7], flags, uvflag)

		for f in m.faces:
			f.col = [col, col, col, col]
		return None, None
	
	def Viewpoint(self, curnode, parent, **kw):
		t = Transform()
		r = curnode.orientation
		name = 'View_' + curnode.description
		t.calcRotfromAxis((r[:3], r[3]))
		t.translation = curnode.position
		Bnode = self.ObjectNode(None, OB.CAMERA, name) # Empty
		Bnode.object.Layer = LAYER_CAMERA
		Bnode.transform = t
		return None, Bnode

	def DirectionalLight(self, curnode, parent, **kw):
		loc = (0.0, 10.0, 0.0)
		l = self._lamp(curnode, loc)
		l.object.data.type = LA.SUN
		return None, l

	def PointLight(self, curnode, parent, **kw):
		l = self._lamp(curnode, curnode.location)
		l.object.data.type = LA.LOCAL
		return None, l

	def _lamp(self, curnode, location):
		t = Transform()
		name = curnode.DEF
		energy = curnode.intensity
		t.translation = location
		Bnode = self.ObjectNode(None, OB.LAMP, "Lamp")
		Bnode.object.data.energy = energy * 5.0
		if options['layers']:
			Bnode.object.Layer = LAYER_LAMP
		Bnode.transform = t
		return Bnode

	def IndexedFaceSet(self, curnode, **kw):
		matxvec = vect.matxvec
		mesh = self.mesh
		debug("IFS, read mesh")

		texcoo = curnode.texCoord
		uvflag = 0

		if curnode.color:
			colors = curnode.color.color
			if curnode.colorIndex: # we have color indices
				colindex = curnode.colorIndex
			else:
				colindex = curnode.coordIndex
			if not texcoo:	
				uvflag = 1
		else:
			colors = None

		faceflags = DEFAULTFLAGS

		if not texcoo and OPTIONS['mat_as_vcol'] and self.curColor:
			uvflag = 1
			col = apply(Color, self.curColor)
		elif self.curImage:
			faceflags += FACEFLAGS.TEX

# MAKE VERTICES

		coo = curnode.coord
		ncoo = len(coo.point)

		if curnode.normal: # normals defined
			normals = curnode.normal.vector
			if curnode.normalPerVertex and len(coo.point) == len(normals):
				self.mesh.recalc_normals = 0
				normindex = curnode.normalIndex
				i = 0
				for v in coo.point:
					newv = mesh.addVert(v)
					n = newv.no
					n[0], n[1], n[2] = normals[normindex[i]]
					i += 1
			else:
				for v in coo.point:
					mesh.addVert(v)
		else:		
			for v in coo.point:
				mesh.addVert(v)
			if curnode.creaseAngle < CREASE_ANGLE_THRESHOLD:
				self.mesh.smooth = 1

		nvertices = len(mesh.vertices)
		if nvertices != ncoo:
			print "todo: %d, done: %d" % (ncoo, nvertices)
			raise RuntimeError, "FATAL: could not create all vertices"

# MAKE FACES		

		index = curnode.coordIndex
		vlist = []

		flip = OPTIONS['flipnormals']
		facecount = 0
		vertcount = 0

		cols = []
		if curnode.colorPerVertex:    # per vertex colors
			for i in index:
				if i == -1:
					if flip or (curnode.ccw == 0 and not flip): # counterclockwise face def
						vlist.reverse()
					f = mesh.addFace(vlist, faceflags, uvflag)
					if uvflag or colors:
						f.col = cols
						cols = []
					vlist = []
				else:
					if colors:
						col = apply(Color, colors[colindex[vertcount]])
						cols.append(col)
						vertcount += 1
					v = mesh.vertices[i]
					vlist.append(v) 
		else:                         # per face colors
			for i in index:
				if i == -1:
					if flip or (curnode.ccw == 0 and not flip): # counterclockwise face def
						vlist.reverse()
					f = mesh.addFace(vlist, faceflags, uvflag)
					facecount += 1

					if colors:
						col = apply(Color, colors[colindex[facecount]])
						cols = len(f.v) * [col]

					if uvflag or colors:
						f.col = cols
					vlist = []
				else:
					v = mesh.vertices[i]
					vlist.append(v) 

# TEXTURE COORDINATES

		if not texcoo:
			return None, None

		self.curmaterial.setMode("traceable", "shadow", "texFace")
		m = self.curtexmatrix
		if m: # texture transform exists:
			for uv in texcoo.point:
				v = (uv[0], uv[1], 1.0)
				v1 = matxvec(m, v)
				uv[0], uv[1] = v1[0], v1[1]
				
		UVindex = curnode.texCoordIndex
		if not UVindex: 
			UVindex = curnode.coordIndex
		# go assign UVs
		self.mesh.hasFaceUV(1)
		j = 0
		uv = []
		for i in UVindex:
			if i == -1: # flush
				if not curnode.ccw:
					uv.reverse()
				assignUV(f, uv)
				assignImage(f, self.curImage)
				uv = []
				j +=1
			else:
				f = mesh.faces[j]
				uv.append(texcoo.point[i])
		return None, None

class PostProcessor(NullProcessor):
	def Shape(self, curnode, **kw):
		pass
		return None, None
	def Transform(self, curnode, **kw):
		return None, None

class Walker:
	"""The node visitor (walker) class for VRML nodes"""
	def __init__(self, pre, post = NullProcessor(), progress = None):
		self.scene = Scenegraph.BScene()
		self.preprocessor = pre
		self.postprocessor = post
		pre.walker = self # processor knows about walker
		post.walker = self 
		self.nodes = 1
		self.depth = 0
		self.progress = progress
		self.processednodes = 0

	def walk(self, vnode, parent):
		"""Essential walker routine. It walks along the scenegraph nodes and
processes them according to its pre/post processor methods.

The preprocessor methods return the children of the node remaining
to be processed or None. Also, a new created target node is returned.
If the target node is == None, the current node will be skipped in the
target scenegraph generation. If it is a valid node, the walker routine
inserts it into the 'parent' node of the target scenegraph, which
must be a valid root node on first call, leading us to the example usage:

	p = Processor()
	w = Walker(p, PostProcessor())
	root = Scenegraph.RootNode()
	w.walk(SG, root) # SG is a VRML scenegraph
	"""
		global g_level  #XXX
		self.depth += 1  
		g_level = self.depth
		if self.depth < PROGRESS_DEPTH:
			self.processednodes += 1
			if self.progress:
				ret = self.progress(PARSE_TIME + PROCESS_TIME * float(self.processednodes) / self.nodes)
				if not ret:
					progress(1.0)
					raise UnfinishedError, "User cancelled conversion"

		# if vnode has already been processed, call Linker method, Processor method otherwise
		id = vnode.DEF # get name
		if not id:
			id = 'Object'

		processed = vnode.getTargetnode()
		if processed: # has been processed ?
			debug("linked obj: %s" % id)
			children, bnode = self.link(processed, parent)		
		else:
			children, bnode = self.preprocess(vnode, parent)
			
		if not bnode:
			bnode = parent # pass on
		else:
			parent.insert(bnode) # insert into SG

		if children:
			for c in children:
				self.walk(c, bnode)
		if not processed:
			self.postprocess(vnode, bnode)

		self.depth -= 1 

		return children, bnode

	def link(self, bnode, parent):
		"""Link already processed data"""
		# link data:
		new = bnode.clone()
		if not new:
			raise RuntimeError, "couldn't clone object"
		return None, new 

	def preprocess(self, vnode, newnode = None):
		"""Processes a VRML node 'vnode' and returns a custom node. The processor must
be specified in 'p'.		
Optionally, a custom parent node (previously created) is passed as 'newnode'."""

		pre = "pre"

		nodetype = vnode.__gi__

		debug(pre + "process:" + repr(nodetype) + " " + vnode.DEF)
		return callMethod(self.preprocessor, nodetype, vnode, newnode)

	def postprocess(self, vnode, newnode = None):
		"""Postprocessing of a VRML node, see Walker.preprocess()"""

		nodetype = vnode.__gi__
		pre = "post"

		debug(pre + "process:" + repr(nodetype) + " " + vnode.DEF)
		return callMethod(self.postprocessor, nodetype, vnode, newnode, 0)

testfile2 = '/home/strubi/exotic/wrl/BrownTrout1.wrl'
testfile = '/home/strubi/exotic/wrl/examples/VRML_Model_HSL.wrl'

def fix_VRMLaxes(root, scale):
	from Blender import Object, Scene
	q = quat.fromRotAxis((1.0, 0.0, 0.0), 1.57079)
	empty = Object.New(OB.EMPTY)
	empty.layer = LAYER_EMPTY
	Scene.getCurrent().link(empty)
	node = Scenegraph.ObjectNode(empty, None, "VRMLscene")
	node.transform.rotation = q
	if scale:
		node.transform.scale = (0.01, 0.01, 0.01)
	for c in root.children:
		node.insert(c)
	node.update()
	root.children = [node]

#################################################################
# these are the routines that must be provided for the importer
# interface in blender

def checkmagic(name):
	"check for file magic"
	f = open(name, "r")
	magic = loader.getFileType(f)
	f.close()
	if magic == 'vrml':
		return 1
	elif magic == 'gzip':
		verbose("gzipped file detected")
		try:
			import gzip
		except ImportError, value:
			warn("Importing gzip module: %s" % value)
			return 0

		f = gzip.open(name, 'rb')
		header = f.readline()
		f.close()
		if header[:10] == "#VRML V2.0":
			return 1
		else:
			return 0
	print "unknown file"
	return 0

g_infotxt = ""

def progress(done):
	from Blender import Window
	ret = Window.draw_progressbar(done, g_infotxt)
	return ret

class Counter:
	def __init__(self):
		self._count = 0
		self.depth = 0
	def count(self, node):
		if self.depth >= PROGRESS_DEPTH:
			return 0

		self.depth += 1
		self._count += 1
		if not getChildren(node):
			self.depth -= 1
			return 0
		else:
			for c in node.children:
				self.count(c)
		self.depth -= 1
		return self._count

################################################################################
# MAIN ROUTINE

def importfile(name):

	global g_infotxt
	global options
	global DEFAULTFLAGS

	from Blender import Get # XXX 
	options = Get('vrmloptions')
	DEFAULTFLAGS = FACEFLAGS.LIGHT + FACEFLAGS.DYNAMIC
	if options['twoside']:
		print "TWOSIDE"
		DEFAULTFLAGS |= FACEFLAGS.TWOSIDE
	clrError()
	g_infotxt = "load & parse file..."
	progress(0.0)
	root = Scenegraph.RootNode()
	try:
		l = loader.Loader(name, progress)
		SG = l.load()
		p = Processor()
		w = Walker(p, PostProcessor(), progress)
		g_infotxt = "convert data..."
		p.curpath = os.path.dirname(name)
		print "counting nodes...",
		c = Counter()
		nodes = c.count(SG)
		print "done."
		w.nodes = nodes # let walker know about number of nodes parsed # XXX
		w.walk(SG, root)
	except UnfinishedError, msg:
		print msg

	progress(1.0)
	fix_VRMLaxes(root, options['autoscale']) # rotate coordinate system: in VRML, y is up!
	root.update() # update baselist for proper display
	return root
