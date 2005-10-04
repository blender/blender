# Blender.Mesh module and the Mesh PyType object

"""
The Blender.Mesh submodule.

Mesh Data
=========

This module provides access to B{Mesh Data} objects in Blender.  It differs
from the NMesh module by allowing direct access to the actual Blender data, 
so that changes are done immediately without need to update or put the data
back into the original mesh.  The result is faster operations with less memory
usage.

Example::

  import Blender
  from Blender import Mesh, Material, Window

  editmode = Window.EditMode()    # are we in edit mode?  If so ...
  if editmode: Window.EditMode(0) # leave edit mode before getting the mesh

  me = Mesh.Get("Plane")          # get the mesh data called "Plane"

  if not me.materials:             # if there are no materials ...
    newmat = Material.New()        # create one ...
    me.materials=[newmat]          # and set the mesh's list of mats

  print me.materials               # print the list of materials
  mat = me.materials[0]            # grab the first material in the list
  mat.R = 1.0                      # redefine its red component
  for v in me.verts:               # loop the list of vertices
    v.co[0] *= 2.5                 # multiply the coordinates
    v.co[1] *= 5.0
    v.co[2] *= 2.5

  if editmode: Window.EditMode(1)  # optional, just being nice
"""

def Get(name=None):
  """
  Get the mesh data object called I{name} from Blender.
  @type name: string
  @param name: The name of the mesh data object.
  @rtype: Mesh
  @return: If a name is given, it returns either the requested mesh or None.
    If no parameter is given, it returns all the meshs in the current scene.
  """

def New(name='Mesh'):
  """
  Create a new mesh data object called I{name}.
  @type name: string
  @param name: The name of the mesh data object.
  @rtype: Mesh
  @return: a new Blender mesh.
  """

class MCol:
  """
  The MCol object
  ===============
    This object is four ints representing an RGBA color.
  @ivar r: The Red component in [0, 255].
  @type r: int
  @ivar g: The Green component in [0, 255].
  @type g: int
  @ivar b: The Blue component in [0, 255].
  @type b: int
  @ivar a: The Alpha (transparency) component in [0, 255].
  @type a: int
  """

class MVert:
  """
  The MVert object
  ================
    This object holds mesh vertex data.
  @ivar co: The vertex coordinates (x, y, z).
  @type co: vector
  @ivar no: The vertex's unit normal vector (x, y, z).  Read-only.  B{Note}:
    if vertex coordinates are changed, it may be necessary to use
    L{Mesh.calcNormals()} to update the vertex normals.
  @type no: vector
  @ivar uvco: The vertex texture "sticky" coordinates (x, y), if present. 
    Use L{Mesh.vertexUV} to test for presence before trying to access;
    otherwise an exception will may be thrown.
    (Sticky coordinates can be set when the object is in the Edit mode;
    from the Editing Panel (F9), look under the "Mesh" properties for the 
    "Sticky" button).  
  @type uvco: vector
  @ivar index: The vertex's index within the mesh.  Read-only.
  @type index: int
  @ivar sel: The vertex's selection state (selected=1).
   B{Note}: a Mesh will return the selection state of the mesh when EditMode 
   was last exited. A Python script operating in EditMode must exit EditMode 
   before getting the current selection state of the mesh.
  @type sel: int
  @warn:  There are two kinds of UV texture coordinates in Blender: per vertex
     ("sticky") and per face vertex (UV in L{MFace}).  In the first, there's
     only one UV pair of coordinates for each vertex in the mesh.  In the
     second, for each face it belongs to, a vertex can have different UV
     coordinates.  This makes the per face option more flexible, since two
     adjacent faces won't have to be mapped to a continuous region in an image:
     each face can be independently mapped to any part of its texture.
  """

class MVertSeq:
  """
  The MVertSeq object
  ===================
    This object provides sequence and iterator access to the mesh's vertices.
  """

  def extend(coords):
    """
    Append one or more vertices to the mesh.  Unlike L{MEdgeSeq.extend()} and
    L{MFaceSeq.extend()} no attempt is made to check for duplicate vertices in
    the parameter list, or for vertices already in the mesh.

    Example::
      import Blender
      from Blender import Mesh
      from Blender.Mathutils import Vector

      me = Mesh.Get("Plane")          # get the mesh data called "Plane"
      me.verts.extend(1,1,1)          # add one vertex
      l=[(.1,.1,.1),Vector([2,2,.5])]
      me.verts.extend(l)              # add multiple vertices

    @type coords: tuple(s) of floats or vectors
    @param coords: coords can be
       - a tuple of three floats,
       - a 3D vector, or
       - a sequence (list or tuple) of either of the above.
    """

class MEdge:
  """
  The MEdge object
  ================
    This object holds mesh edge data.
  @ivar v1: The first vertex of the edge.
  @type v1: MVert
  @ivar v2: The second vertex of the edge.
  @type v2: MVert
  @ivar crease: The crease value of the edge. It is in the range [0,255].
  @type crease: int
  @ivar flag: The bitfield describing edge properties. See L{NMesh.EdgeFlags}.
  @type flag: int
  @ivar index: The edge's index within the mesh.  Read-only.
  @type index: int
  """

  def __iter__():
    """
    Iterator for MEdge.  It iterates over the MVerts of the edge, returning
    v1 then v2.
    @return: one of the edge's vertices
    @rtype: MVert
    """

class MEdgeSeq:
  """
  The MEdgeSeq object
  ===================
    This object provides sequence and iterator access to the mesh's edges.
  """

  def extend(vertseq):
    """
    Add one or more edges to the mesh.  Edges which already exist in the 
    mesh are ignored.  If three or four verts are specified in any tuple,
    an edge is also created between the first and last vertices (this is
    useful when adding faces).

    Example::
      import Blender
      from Blender import Mesh

      me = Mesh.Get("Plane")          # get the mesh data called "Plane"
      v = me.verts                    # get vertices
      if len(v) >= 6:                 # if there are enough vertices...
        me.edges.extend(v[0],v[1])    #   add a single edge
        l=[(v[1],v[2],v[3]),(v[0],v[2],v[4],v[5])]
        me.edges.extend(l)            #   add multiple edges

    @type vertseq: tuple(s) of MVerts
    @param vertseq: either two to four MVerts, or sequence (list or tuple) 
    of tuples each containing two to four MVerts.
    """

class MFace:
  """
  The MFace object
  ================
  This object holds mesh face data.

  Example::
   import Blender
   from Blender import Mesh, Window

   in_emode = Window.EditMode()
   if in_emode: Window.EditMode(0)

   me = Mesh.Get("Mesh")
   faces = me.faces

   ## Example for editmode faces selection:
   selected_faces = []
   for f in faces:
     if f.sel:
       selected_faces.append(f)
   # ... unselect selected and select all the others:
   for f in faces:
     f.sel = 1 - f.sel # 1 becomes 0, 0 becomes 1

   ## Example for UV textured faces selection:
   selected_faces = []
   SEL = NMesh.FaceFlags['SELECT']
   # get selected faces:
   for f in faces:
     if f.flag & SEL:
       selected_faces.append(f)
   # ... unselect selected and select all the others:
   for f in faces:
     if f.flag & SEL:
       f.flag &= ~SEL # unselect these
     else:
       f.flag |= SEL # and select these

   if in_emode: Window.EditMode(1)
   Blender.Redraw()

  @ivar verts: The face's vertices.  Each face has 3 or 4 vertices.
  @type verts: list of MVerts
  @ivar v: Same as L{verts}.  This attribute is only for compatibility with
      NMesh scripts and will probably be deprecated in the future.
  @ivar sel: The face's B{edit mode} selection state (selected=1).
      This is not the same as the selection state of
      the textured faces (see L{flag}).
  @type sel: int
  @ivar hide: The face's B{edit mode} visibility state (hidden=1).
      This is not the same as the visibility state of
      the textured faces (see L{flag}).
  @type hide: int
  @ivar smooth: If set, the vertex normals are averaged to make this
     face look smooth.  (This is the same as choosing "Set Smooth" in the 
     Editing Panel (F9) under "Link and Material" properties).
  @type smooth: int
  @ivar col: The face's vertex colors, if defined.  Each vertex has its own
     color.
     Will throw an exception if the mesh does not have UV faces or vertex
     colors; use L{Mesh.faceUV} and L{Mesh.vertexColors} to test.  B{Note}:
     if a mesh has i{both} UV faces and vertex colors, the colors stored in
     the UV faces will be used here. 
  @type col: list of MCols
  @ivar mat: The face's index into the mesh's materials
      list.  It is in the range [0,15].
  @type mat: int
  @ivar image: The Image used as a texture for this face.
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type image: Image
  @ivar mode: The texture mode bitfield (see L{NMesh.FaceModes}).
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type mode: int
  @ivar index: The face's index within the mesh.  Read-only.
  @type index: int

  @ivar flag: The face's B{texture mode} flags; indicates the selection, 
      active , and visibility states of a textured face (see
      L{NMesh.FaceFlags} for values).
      This is not the same as the selection or visibility states of
      the faces in edit mode (see L{sel} and L{hide}).
      To set the active face, use
      the L{Mesh.activeFace} attribute instead.
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.

  @ivar transp: Transparency mode.  It is one of the values in 
      L{NMesh.FaceTranspModes}).
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type transp: int

  @ivar uv: The face's UV coordinates.  Each vertex has its own UV coordinate.
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type uv: list of vectors
  @ivar no: The face's normal vector (x, y, z).  Read-only.
  @type no: vector
  @note: there are regular faces and textured faces in Blender, both currently
    with their own selection and visibility states, due to a mix of old and new
    code.  To (un)select or (un)hide regular faces (visible in EditMode), use
    L{MFace.sel} and L{MFace.hide} attributes.  For textured faces (UV Face 
    Select and Paint modes in Blender) use the L{MFace.flag} attribute.
    Check the example above and note L{Window.EditMode}.
  @note: Assigning UV textures to mesh faces in Blender works like this:
    1. Select your mesh.
    2. Enter face select mode (press f) and select at least some face(s).
    3. In the UV/Image Editor window, load / select an image.
    4. Play in both windows (better split the screen to see both at the same
       time) until the UV coordinates are where you want them.  Hint: in the
       3D window, the 'u' key opens a menu of default UV choices and the 'r'
       key lets you rotate the UV coords.
    5. Leave face select mode (press f).
  """

  def __iter__():
    """
    Iterator for MVert.  It iterates over the MVerts of the face, returning
    v1, v2, v3 (and optionally v4);
    @return: one of the face's vertices
    @rtype: MVert
    """

class MFaceSeq:
  """
  The MFaceSeq object
  ===================
    This object provides sequence and iterator access to the mesh's faces.
  """

  def extend(vertseq):
    """
    Add one or more faces to the mesh.  Faces which already exist in the 
    mesh are ignored.  Tuples of two vertices are accepted, but no face
    will be created.

    Example::
      import Blender
      from Blender import Mesh

      me = Mesh.Get("Plane")          # get the mesh data called "Plane"
      v = me.verts                    # get vertices
      if len(v) >= 6:                 # if there are enough vertices...
        me.face.extend(v[1],v[2],v[3]) #   add a single edge
        l=[(v[0],v[1]),(v[0],v[2],v[4],v[5])]
        me.face.extend(l)            #   add another face

    @type vertseq: tuple(s) of MVerts
    @param vertseq: either two to four MVerts, or sequence (list or tuple) 
    of tuples each containing two to four MVerts.
    """


class Mesh:
  """
  The Mesh Data object
  ====================
    This object gives access to mesh data in Blender.
  @note: the verts, edges and faces attributes are implemented as sequences.
  The operator[] and len() are defined for these sequences.  You cannot
  assign to an item in the sequence, but you can assign to most of the
  attributes of individual items.
  @ivar edges: The mesh's edges.
  @type edges: sequence of MEdges
  @ivar faces: The mesh's faces.
  @type faces: sequence of MFaces
  @ivar verts: The mesh's vertices.
  @type verts: sequence of MVerts

  @ivar materials: The mesh's materials.  Each mesh can reference up to
    16 materials.  Empty slots in the mesh's list are represented by B{None}.
  @type materials: list of Materials
  @ivar degr: The max angle for auto smoothing in [1,80].  
  @type degr: int
  @ivar maxSmoothAngle: Same as L{degr}.  This attribute is only for
    compatibility with NMesh scripts and will probably be deprecated in 
    the future.
  @ivar mode: The mesh's mode bitfield.  See L{NMesh.Modes}.
  @type mode: int

  @ivar name: The Mesh name.  It's common to use this field to store extra
     data about the mesh (to be exported to another program, for example).
  @type name: str
  @ivar subDivLevels: The [display, rendering] subdivision levels in [1, 6].
  @type subDivLevels: list of 2 ints
  @ivar users: The number of Objects using (linked to) this mesh.
  @type users: int

  @ivar faceUV: The mesh contains UV-mapped textured faces.  Read-only.
  @type faceUV: bool
  @ivar vertexColors: The mesh contains vertex colors.  Read-only.
  @type vertexColors: bool
  @ivar vertexUV: The mesh contains "sticky" per-vertex UV coordinates.  Read-only.
  @type vertexUV: bool
  @ivar activeFace: Index of the mesh's active face in UV Face Select and
    Paint modes.  Only one face can be active at a time.  Note that this is
    independent of the selected faces in Face Select and Edit modes.
    Will throw an exception if the mesh does not have UV faces; use
    L{faceUV} to test.
  @type activeFace: int
  """

  def calcNormals():
    """
    Recalculates the vertex normals using face data.
    """

  def vertexShade(object):
    """
    Colors vertices based on the current lighting setup, like when there
    are no vertex colors and no textured faces and a user enters Vertex Paint
    Mode in Blender (only lamps in visible layers account).  An exception is
    thrown if called while in EditMode.
    @type object: Object
    @param object: The Blender Object linked to the mesh.
    """

