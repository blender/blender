"""The Blender NMesh module

  This module provides access to the raw **Mesh** data block.

  Examples will not be given, as the life time of this module will be
  most probably limited. Use the 'Mesh' module instead.
"""  

import _Blender.NMesh as _NMesh
import shadow

class Mesh(shadow.shadow):
	"""The NMesh object

    This contains a copy of the raw mesh object data. 

  Attributes
  
  verts     -- A list of vertices of type 'Vert'

  faces     -- List of faces of type 'Face'
"""
	def update(self):
		"""updates the mesh object in Blender with the modified mesh data"""
		self._object.update()

class Vert:
	"""Vertex object
	
  Attributes

    co    -- The vertex coordinates (x, y, z)

    no    -- Vertex normal vector (nx, ny, nz)

    uvco  -- Vertex texture ("sticky") coordinates

    index -- The vertex index, if owned by a mesh
"""

class Face:
	"""Face object
	
  Attributes
   
   mode          -- Display mode, see NMesh.FaceModes

   flag          -- flag bit vector, specifying selection flags.
                    see NMesh.FaceFlags

   transp        -- transparency mode bit vector; see NMesh.FaceTranspModes

   v             -- List of Face vertices

   col           -- List of Vertex colours

   materialIndex -- Material index (referring to one of the Materials in
                    the Meshes material list, see Mesh documentation

   smooth        -- Flag whether smooth normals should be calculated (1 = yes)

   image         -- Reference to texture image object 

   uv            -- A list of per-face UV coordinates:
                    [(u0, v0), (u1, v1), (u2, v2), .. ]
"""

class Col:
	"""Colour object

    See NMesh module documentation for an example.

  Attributes

    r, g, b, a  -- The RGBA components of the colour
	               A component must lie in the range of [0, 255]
"""	
	

class FaceModes:
	"""Face mode bit flags

  BILLBOARD  -- always orient after camera

  DYNAMIC    -- respond to collisions

  INVISIBLE  -- invisible face

  HALO       -- halo face, always point to camera

  LIGHT      -- dynamic lighting

  OBCOL      -- use object colour instead of vertex colours

  SHADOW     -- shadow type

  SHAREDCOL  -- shared vertex colors (per vertex)

  TEX        -- has texture image

  TILES      -- uses tiled image 

  TWOSIDE    -- twosided face
"""  
	t = _NMesh.Const
	BILLBOARD  = t.BILLBOARD
	DYNAMIC    = t.DYNAMIC
	INVISIBLE  = t.INVISIBLE
	HALO       = t.HALO
	LIGHT      = t.LIGHT
	OBCOL      = t.OBCOL
	SHADOW     = t.SHADOW
	SHAREDCOL  = t.SHAREDCOL
	TEX        = t.TEX
	TILES      = t.TILES
	TWOSIDE    = t.TWOSIDE
	del t


class FaceTranspModes:
	"""Readonly dictionary

...containing Face transparency draw modes. They are of type 'enum', i.e.
can not be combined like a bit vector.

    SOLID  -- draw solid
	
    ADD    -- add to background(halo)

    ALPHA  -- draw with transparency

    SUB    -- subtract from background 
"""
	t = _NMesh.Const
	SOLID  = t.SOLID
	ADD    = t.ADD
	ALPHA  = t.ALPHA
	SUB    = t.SUB
	del t

class FaceFlags:
	"""Readonly dictionary

...containing Face flags bitvectors:

  SELECT -- selected

  HIDE   -- hidden

  ACTIVE -- the active face
"""
	t = _NMesh.Const
	SELECT  = t.SELECT
	HIDE    = t.HIDE
	ACTIVE  = t.ACTIVE
	del t


def New(name = None):
	"""Creates a new NMesh mesh object and returns it"""
	pass

def GetRaw(name = None):
	"""If 'name' specified, the Mesh object with 'name' is returned, 'None'
if not existant. Otherwise, a new empty Mesh is initialized and returned."""
	pass

def PutRaw(mesh, name = "Mesh"):
	"""Creates a Mesh Object instance in Blender, i.e. a Mesh Object in the
current Scene and returns a reference to it. If 'name' specified, the Mesh
'name' is overwritten. In this case, no Object reference is returned."""
	pass

def GetRawFromObject(name):
	"""This returns the mesh as used by the object, which
means it contains all deformations and modifications."""
	pass
	
# override all these functions again, because we only used them for
# documentation -- NMesh will be no longer supported in future

New              = _NMesh.New
GetRaw           = _NMesh.GetRaw
PutRaw           = _NMesh.PutRaw
GetRawFromObject = _NMesh.GetRawFromObject
Const            = _NMesh.Const
Vert             = _NMesh.Vert
Face             = _NMesh.Face
Col              = _NMesh.Col

def NMesh(data):
	return data
