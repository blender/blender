from Blender import Scene
import Blender.NMesh as _NMesh
import Blender.Material as Material


defaultUV = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]

FACEFLAGS = _NMesh.Const
DEFAULTFLAGS = FACEFLAGS.LIGHT + FACEFLAGS.DYNAMIC

curface = None
tessfaces = None

def error():
	pass
def beginPolygon():
	global curface
	global tessfaces
	curface = _NMesh.Face()
def endPolygon():
	global curface
	global tessfaces
	tessfaces.append(curface)
def addVertex(v):
	global curface
	curface.v.append(v)
	curface.uv.append((v.uvco[0], v.uvco[1]))

class Face:
	def __init__(self, vlist):
		self.v= vlist
		self.uv = []
		self.mode = 0

class shadow:
	def __setattr__(self, name, val):
		setattr(self.data, name, val)
	def __getattr__(self, name):
		return getattr(self.data, name)
	def __repr__(self):
		return repr(self.data)

##########################################
# replacement xMesh (NMesh shadow class)

class shadowNVert:  #shadow NMVert class for the tesselator
	def __init__(self):
		self.vert = None
		self.uv = []
	def __len__(self):
		return 3
	def __getitem__(self, i):
		return self.vert.co[i]

def Color(r, g, b, a = 1.0):
	return _NMesh.Col(255 * r, 255 * g, 255 * b, 255 * a)

class shadowNMesh:
	def __init__(self, name = None, default_flags = None):
		self.scene = Scene.getCurrent()
		self.data = _NMesh.GetRaw()
		self.name = name
		if default_flags:
			flags = default_flags
		else:
			flags = DEFAULTFLAGS
		self.flags = flags
		self.smooth = 0
		self.faces = []
	#	#try:
	#		import tess
	#		self.tess = tess.Tess(256, beginPolygon, endPolygon, error, addVertex)
	#	except:
	#		#print "couldn't import tesselator"
	#		self.tess = None
		self.tess = None
		self.curface = None
		self.tessfaces = []
		self.recalc_normals = 1

	def __del__(self):
		del self.data

	def __getattr__(self, name):
		if name == 'vertices':
			return self.data.verts
		else:
			return getattr(self.data, name)

	def __repr__(self):
		return "Mesh: %d faces, %d vertices" % (len(self.faces), len(self.verts))
	def toNMFaces(self, ngon):
		# This should be a Publisher only feature...once the tesselation
		# is improved. The GLU tesselator of Mesa < 4.0 is crappy...
		if not self.tess:
			return [] # no faces converted
		import tess
		i = 0
		global tessfaces
		tessfaces = []
		tess.beginPolygon(self.tess)
		for v in ngon.v:
			if len(ngon.uv) == len(ngon.v):
				v.uvco = ngon.uv[i]
			tess.vertex(self.tess, (v.co[0], v.co[1], v.co[2]), v)
			i += 1
		tess.endPolygon(self.tess)
		return tessfaces

	def hasFaceUV(self, true):
		self.data.hasFaceUV(true)

	def addVert(self, v):
		vert = _NMesh.Vert(v[0], v[1], v[2]) 
		self.data.verts.append(vert)
		return vert

	def addFace(self, vlist, flags = None, makedefaultUV = 0):
		n = len(vlist)
		if n > 4:
			face = Face(vlist)
		else:
			face = _NMesh.Face()
			for v in vlist:
				face.v.append(v)
			if makedefaultUV:
				face.uv = defaultUV[:n]
		self.faces.append(face)	
		# turn on default flags:
		if not flags:
			face.mode = self.flags
		else:
			face.mode = flags
		return face

	def write(self):
		from Blender import Object
		# new API style: 
		self.update()
		ob = Object.New(Object.Types.MESH)  # create object 
		ob.link(self.data)                  # link mesh data to it
		self.scene.link(ob)
		return ob

	def update(self):
		from Blender.Types import NMFaceType
		smooth = self.smooth
		for f in self.faces:
			if type(f) == NMFaceType:
				f.smooth = smooth
				self.data.faces.append(f)
				f.materialIndex = 0
			else: #it's a NGON (shadow face)
				faces = self.toNMFaces(f)
				for nf in faces:
					nf.smooth = smooth
					nf.materialIndex = 0
					self.data.faces.append(nf)
					
		if not self.name:
			self.name = "Mesh"

	def assignMaterial(self, material):
		self.data.materials = [material._object]

Mesh = shadowNMesh
Vert = shadowNVert

