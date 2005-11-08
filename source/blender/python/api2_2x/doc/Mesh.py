# Blender.Mesh module and the Mesh PyType object

"""
The Blender.Mesh submodule.

B{New}:
 - L{transform()<Mesh.transform>}: apply transform matrix to mesh vertices
 - L{getFromObject()<Mesh.getFromObject>}: get mesh data from other
   geometry objects
 - L{findEdges()<Mesh.findEdges>}: determine if and where edges exist in the
   mesh's edge list
 - delete methods for L{verts<MVertSeq.delete>}, L{edges<MEdgeSeq.delete>}
   and L{faces<MFaceSeq.delete>}
 - new experimental mesh tools:
   L{mesh.fill()<Mesh.Mesh.fill>},
   L{mesh.flipNormals()<Mesh.Mesh.flipNormals>},
   L{mesh.recalcNormals()<Mesh.Mesh.recalcNormals>},
   L{mesh.remDoubles()<Mesh.Mesh.remDoubles>},
   L{mesh.smooth()<Mesh.Mesh.smooth>},
   L{mesh.subdivide()<Mesh.Mesh.subdivide>},
   L{mesh.toSphere()<Mesh.Mesh.toSphere>},
   L{mesh.quadToTriangle()<Mesh.Mesh.quadToTriangle>},
   L{mesh.triangleToQuad()<Mesh.Mesh.triangleToQuad>}
 - methods for accessing and modifying vertex groups
 - and if you're never used Mesh before, everything!

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

@type Modes: readonly dictionary
@type FaceFlags: readonly dictionary
@type FaceModes: readonly dictionary
@type FaceTranspModes: readonly dictionary
@var Modes: The available mesh modes.
    - NOVNORMALSFLIP - no flipping of vertex normals during render.
    - TWOSIDED - double sided mesh.
    - AUTOSMOOTH - turn auto smoothing of faces "on".
    - SUBSURF - turn Catmull-Clark subdivision of surfaces "on".
    - OPTIMAL - optimal drawing of edges when "SubSurf" is "on".
@var FaceFlags: The available *texture face* (uv face select mode) selection
  flags.  Note: these refer to TexFace faces, available if nmesh.hasFaceUV()
  returns true.
    - SELECT - selected.
    - HIDE - hidden.
    - ACTIVE - the active face.
@var FaceModes: The available *texture face* modes. Note: these are only
  meaningful if nmesh.hasFaceUV() returns true, since in Blender this info is
  stored at the TexFace (TexFace button in Edit Mesh buttons) structure.
    - ALL - set all modes at once.
    - BILLBOARD - always orient after camera.
    - HALO - halo face, always point to camera.
    - DYNAMIC - respond to collisions.
    - INVISIBLE - invisible face.
    - LIGHT - dynamic lighting.
    - OBCOL - use object color instead of vertex colors.
    - SHADOW - shadow type.
    - SHAREDVERT - apparently unused in Blender.
    - SHAREDCOL - shared vertex colors (per vertex).
    - TEX - has texture image.
    - TILES - uses tiled image.
    - TWOSIDE - two-sided face.
@var FaceTranspModes: The available face transparency modes. Note: these are
  enumerated values (enums), they can't be combined (and'ed, or'ed, etc) like a bit vector.
    - SOLID - draw solid.
    - ADD - add to background (halo).
    - ALPHA - draw with transparency.
    - SUB - subtract from background.
@var EdgeFlags: The available edge flags.
    - SELECT - selected.
    - EDGEDRAW - edge is drawn out of edition mode.
    - SEAM - edge is a seam for LSCM UV unwrapping
    - FGON - edge is part of a F-Gon.
@type AssignModes: readonly dictionary.
@var AssignModes: The available vertex group assignment modes, used by 
  L{mesh.assignVertsToGroup()<Mesh.Mesh.assignVertsToGroup>}.
	- ADD: if the vertex in the list is not assigned to the group
	already, this creates a new association between this vertex and the
	group with the weight specified, otherwise the weight given is added to
	the current weight of an existing association between the vertex and
	group.
	- SUBTRACT: will attempt to subtract the weight passed from a vertex
	already associated with a group, else it does nothing.\n
	- REPLACE: attempts to replace a weight with the new weight value
	for an already associated vertex/group, else it does nothing. 
"""

AssignModes = {'REPLACE':1}

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
  @ivar uvco: (MVerts only). The vertex texture "sticky" coordinates (x, y),
    if present. 
    Use L{Mesh.vertexUV} to test for presence before trying to access;
    otherwise an exception will may be thrown.
    (Sticky coordinates can be set when the object is in the Edit mode;
    from the Editing Panel (F9), look under the "Mesh" properties for the 
    "Sticky" button).  
  @type uvco: vector
  @ivar index: (MVerts only). The vertex's index within the mesh.  Read-only.
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

  def __init__(coord):
    """
    Create a new PVert object.  

    @note: PVert-type objects are designed to be used for creating and
    modifying a mesh's vertex list, but since they do not "wrap" any Blender
    data there are some differences.  The B{index} and B{uvco} attributes 
    are not defined for PVerts, and the B{no} attribute contains valid
    data only if the PVert was created from an MVert (using a slice
    operation on the mesh's vertex list.)  PVerts also cannot be used as an
    argument to any method which expects data wrapping a Blender mesh, such
    as L{MVertSeq.delete()}.

    Example::
      v = Blender.Mesh.MVert(1,0,0)
      v = Blender.Mesh.MVert(Blender.Mathutils.Vector([1,0,0]))

      m = Blender.Mesh.Get('Mesh')
      vlist = m.verts[:]   # slice operation also returns PVerts

    @type coord: three floats or a Vector object
    @param coord: the coordinate values for the new vertex
    @rtype: PVert
    @return: a new PVert object

    """

class MVertSeq:
  """
  The MVertSeq object
  ===================
    This object provides sequence and iterator access to the mesh's vertices.
    Access and assignment of single items and slices are also supported.
    When a single item in the vertex list is accessed, the operator[] returns
    a MVert object which "wraps" the actual vertex in the mesh; changing any
    of the vertex's attributes will immediately change the data in the mesh.
    When a slice of the vertex list is accessed, however, the operator[]
    returns a list of PVert objects which are copies of the mesh's vertex
    data.  Changes to these objects have no effect on the mesh; they must be
    assigned back to the mesh's vertex list.

    Slice assignments cannot change the vertex list size.  The size of the
    list being assigned must be the same as the specified slice; otherwise an
    exception is thrown.

    Example::
      import Blender
      from Blender import Mesh

      me = Mesh.Get("Plane")          # get the mesh data called "Plane"
      vert = me.verts[0]              # vert accesses actual mesh data
      vert.co[0] += 2                 # change the vertex's X location
      pvert = me.verts[-2:]           # pvert is COPY of mesh's last two verts
      pvert[0].co[0] += 2             # change the vertex's X location
      pvert[1].co[0] += 2             # change the vertex's X location
      me.verts[-1] = pvert[1]         # put change to second vertex into mesh

    @note: The mesh can be "cleared" by assigning B{None} to the mesh's vertex
    list.  This does not delete the Blender mesh object, it only deletes all
    the memory allocated to the mesh.  The result is equivalent to calling 
    Mesh.New().  The intent is to allow users writing exporters to free memory
    after it is used in a quick and simple way.

    Example::
      import Blender
      from Blender import Mesh

      me = Mesh.Get("Plane")          # get the mesh data called "Plane"
      me.verts = None                 # delete all the mesh's attributes

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

  def delete(verts):
    """
    Deletes one or more vertices from the mesh.  Any edge or face which
    uses the specified vertices are also deleted.

    @type verts: multiple ints or MVerts
    @param verts: can be
       - a single MVert belonging to the mesh (B{note:} will not work with
         PVerts)
       - a single integer, specifying an index into the mesh's vertex list
       - a sequence (list or tuple) containing two or more of either of
         the above.
    """

  def selected():
    """
    Get selected vertices.
    @return: a list of the indices for all vertices selected in edit mode.
    @rtype: list of ints
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
  @ivar flag: The bitfield describing edge properties. See L{EdgeFlags}.
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

  def delete(edges):
    """
    Deletes one or more edges from the mesh.  In addition, also delete:
      - any faces which uses the specified edge(s)
      - any "orphan" vertices (belonging only to specified edge(s))

    @type edges: multiple ints or MEdges
    @param edges: can be
       - a single MEdge belonging to the mesh
       - a single integer, specifying an index into the mesh's edge list
       - a sequence (list or tuple) containing two or more of either of
         the above.
    """

  def selected():
    """
    Get selected edges.
    Selected edges are those for which both vertices are selected.
    @return: a list of the indices for all edges selected in edit mode.
    @rtype: list of ints
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
   SEL = Mesh.FaceFlags['SELECT']
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
  @ivar mode: The texture mode bitfield (see L{FaceModes}).
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type mode: int
  @ivar index: The face's index within the mesh.  Read-only.
  @type index: int

  @ivar flag: The face's B{texture mode} flags; indicates the selection, 
      active , and visibility states of a textured face (see
      L{FaceFlags} for values).
      This is not the same as the selection or visibility states of
      the faces in edit mode (see L{sel} and L{hide}).
      To set the active face, use
      the L{Mesh.activeFace} attribute instead.
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.

  @ivar transp: Transparency mode.  It is one of the values in 
      L{FaceTranspModes}).
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type transp: int

  @ivar uv: The face's UV coordinates.  Each vertex has its own UV coordinate.
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type uv: list of vectors
  @ivar uvSel: The face's UV coordinates seletion state; a 1 indicates the
      vertex is selected.  Each vertex has its own UV coordinate select state
      (this is not the same as the vertex's edit mode selection state).
      Will throw an exception if the mesh does not have UV faces; use
      L{Mesh.faceUV} to test.
  @type uvSel: list of ints
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

  def delete(deledges, faces):
    """
    Deletes one or more faces (and optionally the edges associated with
    the face(s)) from the mesh.  

    @type deledges: int
    @param deledges: controls whether just the faces (deledges=0)
    or the faces and edges (deledges=1) are deleted.  These correspond to the
    "Only Faces" and "Edges & Faces" options in the Edit Mode pop-up menu
    @type faces: multiple ints or MFaces
    @param faces: a sequence (list or tuple) containing one or more of:
       - an MEdge belonging to the mesh
       - a integer, specifying an index into the mesh's face list
    """

  def selected():
    """
    Get selected faces.
    mode.
    @return: a list of the indices for all faces selected in edit mode.
    @rtype: list of ints
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
    B{Note}: L{Object.colbits<Object.Object.colbits>} needs to be set correctly
    for each object in order for these materials to be used instead of
    the object's materials.
  @type materials: list of Materials
  @ivar degr: The max angle for auto smoothing in [1,80].  
  @type degr: int
  @ivar maxSmoothAngle: Same as L{degr}.  This attribute is only for
    compatibility with NMesh scripts and will probably be deprecated in 
    the future.
  @ivar mode: The mesh's mode bitfield.  See L{Modes}.
  @type mode: int

  @ivar name: The Mesh name.  It's common to use this field to store extra
     data about the mesh (to be exported to another program, for example).
  @type name: str
  @ivar subDivLevels: The [display, rendering] subdivision levels in [1, 6].
  @type subDivLevels: list of 2 ints
  @ivar users: The number of Objects using (linked to) this mesh.
  @type users: int

  @ivar faceUV: The mesh contains UV-mapped textured faces.  Enabling faceUV
    does not initialize the face colors like the Blender UI does; this must
    be done in the script.  B{Note}: if faceUV is set, L{vertexColors} cannot
    be set.  Furthermore, if vertexColors is already set when faceUV is set,
    vertexColors is cleared.  This is because the vertex color information
    is stored with UV faces, so enabling faceUV implies enabling vertexColors.
  @type faceUV: bool
  @ivar vertexColors: The mesh contains vertex colors.  See L{faceUV} for the
    use of vertex colors when UV-mapped texture faces are enabled.
  @type vertexColors: bool
  @ivar vertexUV: The mesh contains "sticky" per-vertex UV coordinates.
  @type vertexUV: bool
  @ivar activeFace: Index of the mesh's active face in UV Face Select and
    Paint modes.  Only one face can be active at a time.  Note that this is
    independent of the selected faces in Face Select and Edit modes.
    Will throw an exception if the mesh does not have UV faces; use
    L{faceUV} to test.
  @type activeFace: int
  """

  def getFromObject(name):
    """
    Replace the mesh's existing data with the raw mesh data from a Blender
    Object.  This method supports all the geometry based objects (mesh, text,
    curve, surface, and meta).  If the object has modifiers, they will be
    applied before to the object before extracting the vertex data.
    @note: The mesh coordinates are in i{local space}, not the world space of
    its object.  For world space vertex coordinates, each vertex location must
    be multiplied by the object's 4x4 transform matrix (see L{transform}).
    @type name: string
    @param name: name of the Blender object which contains the geometry data.
    """

  def calcNormals():
    """
    Recalculates the vertex normals using face data.
    """

  def transform(matrix, recalc_normals = False):
    """
    Transforms the mesh by the specified 4x4 matrix (such as returned by
    L{Object.Object.getMatrix}).  The matrix should be invertible.
    Ideal usage for this is exporting to an external file where
    global vertex locations are required for each object.
    Sometimes external renderers or file formats do not use vertex normals.
    In this case, you can skip transforming the vertex normals by leaving
    the optional parameter recalc_normals as False or 0 (the default value).

    Example::
     # This script outputs deformed meshes worldspace vertex locations
     # for a selected object without changing the object
     import Blender
     from Blender import Mesh, Object
     
     ob = Object.GetSelected()[0] # Get the first selected object
     me = Mesh.New()              # Create a new mesh
     me.getFromObject(ob.name)    # Get the object's mesh data
     verts = me.verts[:]          # Save a copy of the vertices
     me.transform(ob.matrix)      # Convert verts to world space
     for v in me.verts:
       print 'worldspace vert', v.co
     me.verts = verts             # Restore the original verts
    
    @type matrix: Py_Matrix
    @param matrix: 4x4 Matrix which can contain location, scale and rotation. 
    @type recalc_normals: int
    @param recalc_normals: if True or 1, also transform vertex normals.
    @warn: unlike L{NMesh.transform()<NMesh.NMesh.transform>}, this method
    I{will immediately modify the mesh data} when it is used.  If you
    transform the mesh using the object's matrix to get the vertices'
    world positions, the result will be a "double transform".  To avoid
    this you either need to set the object's matrix to the identity
    matrix, perform the inverse transform after outputting the transformed
    vertices, or make a copy of the vertices prior to using this method
    and restore them after outputting the transformed vertices (as shown
    in the example).
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

  def update():
    """
    Update display lists after changes to mesh.  B{Note}: with changes taking
    place for using a directed acyclic graph (DAG) for scene and object
    updating, this method may be only temporary and may be removed in future
    releases.
    """

  def findEdges(edges):
    """
    Quickly search for the location of an edge.  
    @type edges: tuple(s) of ints or MVerts
    @param edges: can be tuples of MVerts or integer indexes (B{note:} will
       not work with PVerts) or a sequence (list or tuple) containing two or
       more tuples.
    @rtype: int, None or list
    @return: if an edge is found, its index is returned; otherwise None is
    returned.  If a sequence of edges is passed, a list is returned.
    """

  def addVertGroup(group):
    """
    Add a named and empty vertex (deform) group to the object this nmesh is
    linked to.  The mesh must first be linked to an object (with object.link()
    or object.getData() ) so the method knows which object to update.  
    This is because vertex groups in Blender are stored in I{the object} --
    not in the mesh, which may be linked to more than one object. 
    @type group: string
    @param group: the name for the new group.
    """

  def removeVertGroup(group):
    """
    Remove a named vertex (deform) group from the object linked to this mesh.
    All vertices assigned to the group will be removed (just from the group,
    not deleted from the mesh), if any. If this mesh was newly created, it
    must first be linked to an object (read the comment in L{addVertGroup} for
    more info).
    @type group: string
    @param group: the name of a vertex group.
    """

  def assignVertsToGroup(group, vertList, weight, assignmode = AssignModes['REPLACE']):
    """
    Adds an array (a Python list) of vertex points to a named vertex group
    associated with a mesh. The vertex list is a list of vertex indices from
    the mesh. You should assign vertex points to groups only when the mesh has
    all its vertex points added to it and is already linked to an object.

    I{B{Example:}}
    The example here adds a new set of vertex indices to a sphere primitive::
     import Blender
     sphere = Blender.Object.Get('Sphere')
     replace = Blender.Mesh.AssignModes.REPLACE
     mesh = sphere.getData(mesh=True)
     mesh.addVertGroup('firstGroup')
     vertList = []
     for x in range(300):
         if x % 3 == 0:
             vertList.append(x)
     mesh.assignVertsToGroup('firstGroup', vertList, 0.5, replace)

    @type group: string
    @param group: the name of the group.
    @type vertList: list of ints
    @param vertList: a list of vertex indices.
    @type weight: float
    @param weight: the deform weight for (which means: the amount of influence
        the group has over) the given vertices. It should be in the range
        [0.0, 1.0]. If weight <= 0, the given vertices are removed from the
        group.  If weight > 1, it is clamped.
    @type assignmode: module constant
    @param assignmode: Three choices: REPLACE, ADD or SUBTRACT. 
        See L{AssignModes} for a complete description.
       """

  def removeVertsFromGroup(group, vertList = None):
    """
    Remove a list of vertices from the given group.  If this nmesh was newly
    created, it must first be linked to an object (check L{addVertGroup}).
    @type group: string
    @param group: the name of a vertex group
    @type vertList: list of ints
    @param vertList: a list of vertex indices to be removed from I{group}.
        If None, all vertices are removed -- the group is emptied.
    """

  def getVertsFromGroup(group, weightsFlag = 0, vertList = None):
    """
    Return a list of vertex indices associated with the passed group. This
    method can be used to test whether a vertex index is part of a group and
    if so, what its weight is. 

    I{B{Example:}}
    Append this to the example from L{assignVertsToGroup}::
     # ...
     print "Vertex indices from group %s :" % groupName
     print mesh.getVertsFromGroup('firstGroup')
     print "Again, with weights:"
     print mesh.getVertsFromGroup('firstGroup',1)
     print "Again, with weights and restricted to the given indices:"
     print mesh.getVertsFromGroup('firstGroup',1,[1,2,3,4,5,6])     

    @type group: string
    @param group: the group name.
    @type weightsFlag: bool
    @param weightsFlag: if 1, the weight is returned along with the index. 
    @type vertList: list of ints
    @param vertList: if given, only those vertex points that are both in the
        list and group passed in are returned.
    """

  def renameVertGroup(groupName, newName):
    """
    Renames a vertex group.
    @type groupName: string
    @param groupName: the vertex group name to be renamed.
    @type newName: string
    @param newName: the name to replace the old name.
    """

  def getVertGroupNames():
    """
    Return a list of all vertex group names.
    @rtype: list of strings
    @return: returns a list of strings representing all vertex group
    associated with the mesh's object
    """

  def smooth():
    """
    Flattens angle of selected faces. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    """

  def flipNormals():
    """
    Toggles the direction of selected face's normals. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    """

  def toSphere():
    """
    Moves selected vertices outward in a spherical shape. Experimental mesh
    tool.
    An exception is thrown if called while in EditMode.
    """

  def fill():
    """
    Scan fill a closed selected edge loop. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    """

  def triangleToQuad():
    """
    Convert selected triangles to quads. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    """

  def quadToTriangle(mode=0):
    """
    Convert selected quads to triangles. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    @type mode: int
    @param mode: specifies whether a to add the new edge between the
    closest (=0) or farthest(=1) vertices.
    """

  def subdivide(beauty=0):
    """
    Subdivide selected edges in a mesh. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    @type beauty: int
    @param beauty: specifies whether a "beauty" subdivide should be
    enabled (disabled is default).  Value must be in the range [0,1].
    """

  def remDoubles(limit):
    """
    Removes duplicates from selected vertices. Experimental mesh tool.
    An exception is thrown if called while in EditMode.
    @type limit: float
    @param limit: specifies the maximum distance considered for vertices
    to be "doubles".  Value is clamped to the range [0.0,1.0].
    @rtype: int
    @return: the number of vertices deleted
    """

  def recalcNormals(direction=0):
    """
    Recalculates inside or outside normals for selected faces. Experimental
    mesh tool.
    An exception is thrown if called while in EditMode.
    @type direction: int
    @param direction: specifies outward (0) or inward (1) normals.  Outward
    is the default.  Value must be in the range [0,1].
    """
