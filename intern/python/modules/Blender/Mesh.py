"""The Blender Mesh module

  This module provides routines for more extensive mesh manipulation.
  Later, this Mesh type will also allow interactive access (like in
  EditMode).
  In the Publisher, Ngons will also be supported (and converted to
  triangles on mesh.update(). The following code demonstrates
  creation of an Ngon.

  Example::

	from Blender import Mesh, Object, Scene

	m = Mesh.New()  # new empty mesh
	vlist = []
	vlist.append(m.addVert((-0.0, -1.0, 0.0)))
	vlist.append(m.addVert((1.0, 0.0, 0.0)))
	vlist.append(m.addVert((1.0, 1.0, 0.0)))
	vlist.append(m.addVert((0.0, 3.0, 0.0)))
	vlist.append(m.addVert((-1.0, 2.0, 0.0)))
	vlist.append(m.addVert((-3.0, 1.0, 0.0)))
	vlist.append(m.addVert((-3.0, 3.0, 0.0)))
	vlist.append(m.addVert((-4.0, 3.0, 0.0)))
	vlist.append(m.addVert((-4.0, 0.0, 0.0)))

	f = m.addFace(vlist)

	# do some calculations: top project vertex coordinates to
	# UV coordinates and normalize them to the square [0.0, 1.0]*[0.0, 1.0]

	uvlist = map(lambda x: (x.co[0], x.co[1]), vlist)
	maxx = max(map(lambda x: x[0], uvlist))
	maxy = max(map(lambda x: x[1], uvlist))
	minx = min(map(lambda x: x[0], uvlist))
	miny = min(map(lambda x: x[1], uvlist))

	len = max((maxx - minx), (maxy - miny))
	offx = -minx / len
	offy = -miny / len

	f.uv = map(lambda x: (x[0]/len + offx, x[1]/len + offy), uvlist)  # assign UV coordinates by 'top' projection

	m.update()  # update and triangulate mesh

	ob = Object.New('Mesh')    # create new Object
	ob.link(m)                 # link mesh data
	sc = Scene.getCurrent()    # get current Scene
	sc.link(ob)                # link Object to scene
"""

from Blender.Types import NMFaceType
import Blender.Material as Material

#from _Blender import NMesh as _NMesh

FACEFLAGS = _NMesh.Const
DEFAULTFLAGS = FACEFLAGS.LIGHT + FACEFLAGS.DYNAMIC

import shadow

def makeFace(f):
	face = _NMesh.Face()
	for v in f:
		face.v.append(v)
		face.uv.append((v.uvco[0], v.uvco[1]))
	return face

def toTriangles(ngon):
	#from utils import tesselation
	# This should be a Publisher only feature...once the tesselation
	# is improved. The GLU tesselator of Mesa < 4.0 is crappy...
	if len(ngon.uv) == len(ngon.v):
		i = 0
		for v in ngon.v:
			v.uvco = ngon.uv[i]
			i += 1

	return tesselation.NgonAsTriangles(ngon, makeFace) # return triangles

def Color(r, g, b, a = 1.0):
	return _NMesh.Col(255 * r, 255 * g, 255 * b, 255 * a)

class Vert:  #shadow NMVert class for the tesselator
	"""Vertex wrapper class
This class emulates a float coordinate vector triple
"""
	def __init__(self):
		self.vert = None
		self.uv = []
	def __len__(self):
		return 3
	def __setitem__(self, i, val):
		self.vert[i] = val
	def __getitem__(self, i):
		return self.vert.co[i]

class Face:
	"""Face wrapper class
This class emulates a list of vertex references
"""
	def __init__(self, vlist):
		self.v= vlist
		self.uv = []

	def __len__(self):
		return len(self.v)

	def __setitem__(self, i, val):
		self.v[i] = val

	def __getitem__(self, i):
		return self.v[i]

# override:

Vert = _NMesh.Vert
Face = _NMesh.Face

class rawMesh:
	"""Wrapper for raw Mesh data"""
	def __init__(self, object = None):
		if object:
			self._object = object
		else:	
			self._object = _NMesh.GetRaw()

		self.flags = DEFAULTFLAGS
		self.smooth = 0
		self.recalc_normals = 1
		self.faces = self._object.faces[:]

	def __getattr__(self, name):
		if name == 'vertices':
			return self._object.verts
		elif name == 'has_col':
			return self._object.hasVertexColours()
		elif name == 'has_uv':
			return self._object.hasFaceUV()
		else:
			return getattr(self._object, name)

	def __repr__(self):
		return "Mesh: %d faces, %d vertices" % (len(self.faces), len(self.verts))

	def hasFaceUV(self, true = None):
		"""Sets the per-face UV texture flag, if 'true' specified (either
		0 or 1). Returns the texture flag in any case."""
		if true == None:
			return self._object.hasFaceUV()
		return self._object.hasFaceUV(true)

	def hasVertexUV(self, true = None):
		"""Sets the per-vertex UV texture flag, if 'true' specified (either
		0 or 1). Returns the texture flag in any case."""
		if true == None:
			return self._object.hasVertexUV()
		return self._object.hasVertexUV(true)

	def hasVertexColours(self, true = None):
		"""Sets the per-face UV texture flag, if 'true' specified (either
		0 or 1). Returns the texture flag in any case."""
		if true == None:
			return self._object.hasVertexColours()
		return self._object.hasVertexColours(true)

	def addVert(self, v):
		"""Adds a vertex to the mesh and returns a reference to it. 'v' can
be a float triple or any data type emulating a sequence, containing the
coordinates of the vertex. Note that the returned value references an
*owned* vertex"""
		vert = _NMesh.Vert(v[0], v[1], v[2]) 
		self._object.verts.append(vert)
		return vert

	def addFace(self, vlist, flags = None, makedefaultUV = 0):
		"""Adds a face to the mesh and returns a reference to it. 'vlist' 
must be a list of vertex references returned by addVert(). 
Note that the returned value references an *owned* face"""
		if type(vlist) == NMFaceType:
			face = vlist
		else:	
			n = len(vlist)
			face = _NMesh.Face(vlist)
			if makedefaultUV:
				face.uv = defaultUV[:n]

		self.faces.append(face)	
		# turn on default flags:
		if not flags:
			face.mode = self.flags
		else:
			face.mode = flags
		return face

	def update(self):
		"""Updates the mesh datablock in Blender"""
		o = self._object
		o = self._object
		o.faces = []
		smooth = self.smooth
		for f in self.faces:
			if len(f) > 4: #it's a NGON 
				faces = toTriangles(f)
				for nf in faces:
					nf.smooth = smooth
					o.faces.append(nf)
			else:
				o.faces.append(f)
		o.update()

	def link(self, material):
		"""Link material 'material' with the mesh. Note that a mesh can
currently have up to 16 materials, which are referenced by 
Face().materialIndex"""
		mats = self._object.materials
		if material in mats:
			print "material already assigned to mesh"
			return
		mats.append(material._object)

	def unlink(self, material):
		"""Unlink (remove) material 'material' from the mesh. Note
that the material indices per face need to be updated."""
		self._object.materials.remove(material._object)

	def setMaterials(self, materials = []):
		"""Sets materials. 'materials' must be a list of valid material objects
Note that a mesh can currently have up to 16 materials, which are referenced 
by Face().materialIndex"""

		self._object.materials = (map(lambda x: x._object, materials))

	def getMaterials(self, materials = []):
		"""Returns materials assigned to the mesh"""
		return shadow._List(self._object.materials, Material.Material)

def New():
	return rawMesh()

def get(name = None):
	"""If 'name' given, the Mesh 'name' is returned if existing, 'None' otherwise."""
	if name:
		ob = _NMesh.GetRaw(name)
		if ob:
			return rawMesh(ob)
		else:
			return None
	else:
		raise SystemError, "get() for Meshes is not yet supported"

