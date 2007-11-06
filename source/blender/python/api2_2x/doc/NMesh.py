# Blender.NMesh module and the NMesh PyType object

"""
The Blender.NMesh submodule.

B{Deprecated}:
This module is now maintained but not actively developed.

Access to data such as properties, library, UVLayers and ColorLayers is not available
further more, a mesh modified with NMesh will destroy inactive UV and Color layers
so writing tools that use NMesh is discouraged.

Use L{Mesh} instead.

Mesh Data
=========

This module provides access to B{Mesh Data} objects in Blender.

Example::

  import Blender
  from Blender import NMesh, Material, Window

  editmode = Window.EditMode()    # are we in edit mode?  If so ...
  if editmode: Window.EditMode(0) # leave edit mode before getting the mesh

  me = NMesh.GetRaw("Plane")       # get the mesh data called "Plane"

  if not me.materials:             # if there are no materials ...
    newmat = Material.New()        # create one ...
    me.materials.append(newmat)    # and append it to the mesh's list of mats

  print me.materials               # print the list of materials
  mat = me.materials[0]            # grab the first material in the list
  mat.R = 1.0                      # redefine its red component
  for v in me.verts:               # loop the list of vertices
    v.co[0] *= 2.5                 # multiply the coordinates
    v.co[1] *= 5.0
    v.co[2] *= 2.5
  me.update()                      # update the real mesh in Blender

  if editmode: Window.EditMode(1)  # optional, just being nice

@type Modes: readonly dictionary
@type FaceFlags: readonly dictionary
@type FaceModes: readonly dictionary
@type FaceTranspModes: readonly dictionary
@var Modes: The available mesh modes.
    - NOVNORMALSFLIP - no flipping of vertex normals during render.
    - TWOSIDED - double sided mesh.
    - AUTOSMOOTH - turn auto smoothing of faces "on".
@var FaceFlags: The available *texture face* (uv face select mode) selection
  flags.  Note: these refer to TexFace faces, available if nmesh.hasFaceUV()
  returns true.
    - SELECT - selected (deprecated after 2.43 release, use face.sel).
    - HIDE - hidden (deprecated after 2.43 release, use face.sel).
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
  enumerated values (enums), they can't be combined (ANDed, ORed, etc) like a bit vector.
    - SOLID - draw solid.
    - ADD - add to background (halo).
    - ALPHA - draw with transparency.
    - SUB - subtract from background.
@var EdgeFlags: The available edge flags.
    - SELECT - selected.
    - EDGEDRAW - edge is drawn out of edition mode.
    - SEAM - edge is a seam for UV unwrapping
    - FGON - edge is part of a F-Gon.
"""

def Col(col = [255, 255, 255, 255]):
  """
  Get a new mesh rgba color.
  @type col: list
  @param col: A list [red, green, blue, alpha] of integer values in [0, 255].
  @rtype: NMCol
  @return:  A new NMCol (mesh rgba color) object.
  """

def Vert(x = 0, y = 0, z = 0):
  """
  Get a new vertex object.
  @type x: float
  @type y: float
  @type z: float
  @param x: The x coordinate of the vertex.
  @param y: The y coordinate of the vertex.
  @param z: The z coordinate of the vertex.
  @rtype: NMVert
  @return: A new NMVert object.
  """

def Face(vertexList = None):
  """
  Get a new face object.
  @type vertexList: list
  @param vertexList: A list of B{up to 4} NMVerts (mesh vertex
      objects).
  @rtype: NMFace
  @return: A new NMFace object.
  """

def New(name = 'Mesh'):
  """
  Create a new mesh object.
  @type name: string
  @param name: An optional name for the created mesh.
  rtype: NMesh
  @return: A new (B{empty}) NMesh object.
  """

def GetRaw(name = None):
  """
  Get the mesh data object called I{name} from Blender.
  @type name: string
  @param name: The name of the mesh data object.
  @rtype: NMesh
  @return: It depends on the 'name' parameter:
      - (name) - The NMesh wrapper of the mesh called I{name},
        None if not found.
      - () - A new (empty) NMesh object.
  """

def GetNames():
  """
  Get a list with the names of all available meshes in Blender.
  @rtype: list of strings
  @return: a list of mesh names.
  @note: to get actual mesh data, pass a mesh name to L{GetRaw}.
  """

def GetRawFromObject(name):
  """
  Get the raw mesh data object from the Object in Blender called I{name}.\n
  Note: The mesh coordinates are in local space, not the world space of its Object.\n
  For world space vertex coordinates, each vertex location must be multiplied by the object's 4x4 matrix.
  This function support all the geometry based objects: Mesh, Text, Surface, Curve, Meta.
  @type name: string
  @param name: The name of an Object.
  @rtype: NMesh
  @return: The NMesh wrapper of the mesh data from the Object called I{name}.
  @note: For "subsurfed" meshes, it's the B{display} level of subdivision that
      matters, the rendering one is only processed at the rendering pre-stage
      and is not available for scripts.  This is not a problem at all, since
      you can get and set the subdivision levels via scripting, too (see
      L{NMesh.NMesh.getSubDivLevels}, L{NMesh.NMesh.setSubDivLevels}).
  @note: Meshes extracted from curve based objects (Font/2D filled curves)
      contain both the filled surfaces and the outlines of the shapes.
  @warn: This function gets I{deformed} mesh data, already modified for
      displaying (think "display list").  It also doesn't let you overwrite the
      original mesh in Blender, so if you try to update it, a new mesh will
      be created.
  @warn: For Meta Object's, this function will only return a NMesh with some geometry
      when called on the base element (the one with the shortest name).
  """

def PutRaw(nmesh, name = None, recalc_normals = 1, store_edges = 0):
  """
  Put a BPython NMesh object as a mesh data object in Blender.
  @note: if there is already a mesh with the given 'name', its contents are
  freed and the new data is put in it.  Also, if this mesh is not linked to any
  object, a new object for it is created.  Reminder: in Blender an object is
  composed of the base object and linked object data (mesh, metaball, camera,
  etc. etc).
  @type nmesh: NMesh
  @type name: string
  @type recalc_normals: int
  @type store_edges: int
  @param name: The name of the mesh data object in Blender which will receive
     this nmesh data.  It can be an existing mesh data object or a new one.
  @param recalc_normals: If non-zero, the vertex normals for the mesh will
     be recalculated.
  @param store_edges: deprecated, edges are always stored now.
  @rtype: None or Object
  @return: It depends on the 'name' parameter:
      - I{name} refers to an existing mesh data obj already linked to an
           object: return None.
      - I{name} refers to a new mesh data obj or an unlinked (no users) one:
           return the created Blender Object wrapper.
  """

class NMCol:
  """
  The NMCol object
  ================
    This object is a list of ints: [r, g, b, a] representing an
    rgba color.
  @ivar r: The Red component in [0, 255].
  @ivar g: The Green component in [0, 255].
  @ivar b: The Blue component in [0, 255].
  @ivar a: The Alpha (transparency) component in [0, 255].
  """

class NMVert:
  """
  The NMVert object
  =================
    This object holds mesh vertex data.
  @type co: 3D Vector object. (WRAPPED DATA)
  @ivar co: The vertex coordinates (x, y, z).
  @type no: 3D Vector object. (unit length) (WRAPPED DATA)
  @ivar no: The vertex normal vector (x, y, z).
  @type uvco: 3D Vector object. (WRAPPED DATA)
  @ivar uvco: The vertex texture "sticky" coordinates. The Z value of the Vector is ignored.
  @type index: int
  @ivar index: The vertex index, if owned by a mesh.
  @type sel: int
  @ivar sel: The selection state (selected:1, unselected:0) of this vertex.\n
  Note: An NMesh will return the selection state of the mesh when EditMod was last exited. A python script operating in EditMode must exit edit mode, before getting the current selection state of the mesh.
  @warn:  There are two kinds of uv texture coordinates in Blender: per vertex
     ("sticky") and per face vertex (uv in L{NMFace}).  In the first, there's
     only one uv pair of coordinates for each vertex in the mesh.  In the
     second, for each face it belongs to, a vertex can have different uv
     coordinates.  This makes the per face option more flexible, since two
     adjacent faces won't have to be mapped to a continuous region in an image:
     each face can be independently mapped to any part of its texture.
  """

class NMEdge:
  """
  The NMEdge object
  =================
    This object holds mesh edge data.
  @type v1: NMVert
  @ivar v1: The first vertex of the edge.
  @type v2: NMVert
  @ivar v2: The second vertex of the edge.
  @type crease: int
  @ivar crease: The crease value of the edge. It is in the range [0,255].
  @type flag: int
  @ivar flag: The bitmask describing edge properties. See L{NMesh.EdgeFlags<EdgeFlags>}.
  """

class NMFace:
  """
  The NMFace object
  =================
  This object holds mesh face data.

  Example::
   import Blender
   from Blender import NMesh, Window

   in_emode = Window.EditMode()
   if in_emode: Window.EditMode(0)

   me = NMesh.GetRaw("Mesh")
   faces = me.faces

   ## Example for editmode faces selection:
   selected_faces = []
   for f in faces:
     if f.sel:
       selected_faces.append(f)
   # ... unselect selected and select all the others:
   for f in faces:
     f.sel = 1 - f.sel # 1 becomes 0, 0 becomes 1

   ## Example for uv textured faces selection:
   selected_faces = []
   SEL = NMesh.FaceFlags['SELECT']
   # get selected faces:
   for f in faces:
     if f.flag & SEL:
       selected_faces.append(f)
   # ... unselect selected and select all the others:
   for f in faces:
     if f.flag & SEL:
       f.flag &=~SEL # unselect these
     else: f.flag |= SEL # and select these

   me.update()
   if in_emode: Window.EditMode(1)
   Blender.Redraw()

  @type v: list
  @ivar v: The list of face vertices (B{up to 4}).
  @type sel: bool
  @ivar sel: The selection state (1: selected, 0: unselected) of this NMesh's
      faces *in edit mode*.  This is not the same as the selection state of
      the textured faces (see L{flag}).
  @type hide: bool
  @ivar hide: The visibility state (1: hidden, 0: visible) of this NMesh's
      faces *in edit mode*.  This is not the same as the visibility state of
      the textured faces (see L{flag}).
  @ivar col: The list of vertex colors.
  @ivar mat: Same as I{materialIndex} below.
  @ivar materialIndex: The index of this face's material in its NMesh materials
      list.
  @ivar smooth: If non-zero, the vertex normals are averaged to make this
     face look smooth.
  @ivar image: The Image used as a texture for this face.
  @ivar mode: The display mode (see L{Mesh.FaceModes<FaceModes>})
  @ivar flag: Bit vector specifying selection / visibility flags for uv
     textured faces (visible in Face Select mode, see
     L{NMesh.FaceFlags<FaceFlags>}).
  @ivar transp: Transparency mode bit vector
     (see L{NMesh.FaceTranspModes<FaceTranspModes>}).
  @ivar uv: List of per-face UV coordinates: [(u0, v0), (u1, v1), ...].
  @ivar normal: (or just B{no}) The normal vector for this face: [x,y,z].
  @note: there are normal faces and textured faces in Blender, both currently
    with their own selection and visibility states, due to a mix of old and new
    code.  To (un)select or (un)hide normal faces (visible in editmode), use
    L{sel} and L{hide} variables.  For textured faces (Face Select
    mode in Blender) use the old L{flag} bitflag.  Also check the
    example above and note L{Window.EditMode}.
  @note: Assigning uv textures to mesh faces in Blender works like this:
    1. Select your mesh.
    2. Enter face select mode (press f) and select at least some face(s).
    3. In the UV/Image Editor window, load / select an image.
    4. Play in both windows (better split the screen to see both at the same
       time) until the uv coordinates are where you want them.  Hint: in the
       3d window, the 'u' key opens a menu of default uv choices and the 'r'
       key lets you rotate the uv coordinates.
    5. Leave face select mode (press f).
  """

  def append(vertex):
    """
    Append a vertex to this face's vertex list.
    @type vertex: NMVert
    @param vertex: An NMVert object.
    """

from IDProp import IDGroup, IDArray
class NMesh:
  """
  The NMesh Data object
  =====================
    This object gives access to mesh data in Blender.  We refer to mesh as the
    object in Blender and NMesh as its Python counterpart.
  @ivar properties: Returns an L{IDGroup<IDProp.IDGroup>} reference to this 
  object's ID Properties.
  @type properties: L{IDGroup<IDProp.IDGroup>}
  @ivar name: The NMesh name.  It's common to use this field to store extra
     data about the mesh (to be exported to another program, for example).
  @ivar materials: The list of materials used by this NMesh.  See
     L{getMaterials} for important details.
  @ivar verts: The list of NMesh vertices (NMVerts).
  @ivar users: The number of Objects using (linked to) this mesh.
  @ivar faces: The list of NMesh faces (NMFaces).
  @ivar edges: A list of L{NMEdge} edges.
  @ivar mode:  The mode flags for this mesh.  See L{setMode}.
  @ivar subDivLevels: The [display, rendering] subdivision levels in [1, 6].
  @ivar maxSmoothAngle: The max angle for auto smoothing.  See L{setMode}.
  @cvar key: The L{Key.Key} object attached to this mesh, if any.
  """

  def addEdge(v1, v2):
    """
    Create an edge between two vertices.
    If an edge already exists between those vertices, it is returned.
    Created edge is automatically added to edges list.
    You can only call this method if mesh has edge data.
    @note: In Blender only zero or one edge can link two vertices.
    @type v1: NMVert
    @param v1: the first vertex of the edge.
    @type v2: NMVert
    @param v2: the second vertex of the edge.
    @rtype: NMEdge
    @return: The created or already existing edge.
    """

  def findEdge(v1, v2):
    """
    Try to find an edge between two vertices.
    If no edge exists between v1 and v2, None is returned.
    You can only call this method if mesh has edge data.
    @type v1: NMVert
    @param v1: the first vertex of the edge.
    @type v2: NMVert
    @param v2: the second vertex of the edge.
    @rtype: NMEdge
    @return: The found edge. None if no edge was found.
    """

  def removeEdge(v1, v2):
    """
    Remove an edge between two vertices.
    All faces using this edge are removed from faces list.
    You can only call this method if mesh has edge data.
    @type v1: NMVert
    @param v1: the first vertex of the edge.
    @type v2: NMVert
    @param v2: the second vertex of the edge.
    """

  def addFace(face):
    """
    Add a face to face list and add to edge list (if edge data exists) necessary edges.
    @type face: NMFace
    @param face: the face to add to the mesh.
    @rtype: list of NMEdge
    @return: If mesh has edge data, return the list of face edges.
    """

  def removeFace(face):
    """
    Remove a face for face list and remove edges no more used by any other face (if edge data exists).
    @type face: NMFace
    @param face: the face to add to the mesh.
    """

  def addMaterial(material):
    """
    Add a new material to this NMesh's list of materials.  This method is the
    slower but safer way to add materials, since it checks if the argument
    given is really a material, imposes a limit of 16 materials and only adds
    the material if it wasn't already in the list.
    @type material: Blender Material
    @param material: A Blender Material.
    """

  def getMaterials(what = -1):
    """
    Get this NMesh's list of materials.
    @type what: int
    @param what: determines the list's contents:
        - -1: return the current NMesh's list;
        -  0: retrieve a fresh list from the Blender mesh -- eventual
              modifications made by the script not included, unless
              L{update} is called before this method;
        -  1: like 0, but empty slots are not ignored, they are returned as
              None's.
    @note: what >= 0 also updates nmesh.materials attribute.
    @rtype: list of materials
    @return: the requested list of materials.
    @note: if a user goes to the material buttons window and removes some
        mesh's link to a material, that material slot becomes empty.
        Previously such materials were ignored.
    @note: L{Object.colbits<Object.Object.colbits>} needs to be set correctly
        for each object in order for these materials to be used instead of
        the object's materials.
    """

  def setMaterials(matlist):
    """
    Set this NMesh's list of materials.  This method checks the consistency of
    the passed list: must only have materials or None's and can't contain more
    than 16 entries.
    @type matlist: list of materials
    @param matlist: a list with materials, None's also accepted (they become
        empty material slots in Blender.
    @note: L{Object.colbits<Object.Object.colbits>} needs to be set correctly
        for each object in order for these materials to be used instead of
        the object's materials.
    """

  def hasVertexColours(flag = None):
    """
    Get (and optionally set) if this NMesh has vertex colors.
    @type flag: int
    @param flag: If given and non-zero, the "vertex color" flag for this NMesh
        is turned I{on}.
    @rtype: bool
    @return: The current value of the "vertex color" flag.
    @warn: If a mesh has both vertex colors and textured faces, this function
       will return False.  This is due to the way Blender deals internally with
       the vertex colors array (if there are textured faces, it is copied to
       the textured face structure and the original array is freed/deleted).
       If you want to know if a mesh has both textured faces and vertex
       colors, set *in Blender* the "VCol Paint" flag for each material that
       covers an area that was also vertex painted and then check in your
       Python script if that material flag is set.  Of course also tell others
       who use your script to do the same.  The "VCol Paint" material mode flag
       is the way to tell Blender itself to render with vertex colors, too, so
       it's a natural solution.
    """

  def hasFaceUV(flag = None):
    """
    Get (and optionally set) if this NMesh has UV-mapped textured faces.
    @type flag: int
    @param flag: If given and non-zero, the "textured faces" flag for this
        NMesh is turned I{on}.
    @rtype: bool
    @return: The current value of the "textured faces" flag.
    """

  def hasVertexUV(flag = None):
    """
    Get (and optionally set) the "sticky" flag that controls if a mesh has
    per vertex UV coordinates.
    @type flag: int
    @param flag: If given and non-zero, the "sticky" flag for this NMesh is
        turned I{on}.
    @rtype: bool
    @return: The current value of the "sticky" flag.
    """

  def getActiveFace():
    """
    Get the index of the active face.
    @rtype: int
    @return: The index of the active face.
    """

  def getSelectedFaces(flag = None):
    """
    Get list of selected faces.
    @type flag: int
    @param flag: If given and non-zero, the list will have indices instead of
        the NMFace objects themselves.
    @rtype: list
    @return:  It depends on the I{flag} parameter:
        - if None or zero: List of NMFace objects.
        - if non-zero: List of indices to NMFace objects.
    @warn: this method exists to speed up retrieving of selected faces from
       the actual mesh in Blender.  So, if you make changes to the nmesh, you
       need to L{update} it before using this method.
    """

  def getVertexInfluences(index):
    """
    Get influences of bones in a specific vertex.
    @type index: int
    @param index: The index of a vertex.
    @rtype: list of lists
    @return: List of pairs (name, weight), where name is the bone name (string)
        and its weight is a float value.
    """

  def getKey():
    """
    Get the Key object representing the Vertex Keys (absolute or
    relative) assigned to this mesh.
    @rtype: L{Key.Key} object or None
    """

  def insertKey(frame = None, type = 'relative'):
    """
    Insert a mesh key at the given frame.  Remember to L{update} the nmesh
    before doing this, or changes in the vertices won't be updated in the
    Blender mesh.
    @type frame: int
    @type type: string
    @param frame: The Scene frame where the mesh key should be inserted.  If
        None, the current frame is used.
    @param type: The mesh key type: 'relative' or 'absolute'.  This is only
        relevant on the first call to insertKey for each nmesh (and after all
        keys were removed with L{removeAllKeys}, of course).
    @warn: This and L{removeAllKeys} were included in this release only to
        make accessing vertex keys possible, but may not be a proper solution
        and may be substituted by something better later.  For example, it
        seems that 'frame' should be kept in the range [1, 100]
        (the curves can be manually tweaked in the Ipo Curve Editor window in
        Blender itself later).
    """

  def removeAllKeys():
    """
    Remove all mesh keys stored in this mesh.
    @rtype: bool
    @return: True if successful or False if this NMesh wasn't linked to a real
       Blender Mesh yet (or was, but the Mesh had no keys).
    @warn: Currently the mesh keys from meshes that are grabbed with
       NMesh.GetRaw() or .GetRawFromObject() are preserved, so if you want to
       clear them or don't want them at all, remember to call this method.  Of
       course NMeshes created with NMesh.New() don't have mesh keys until you
       add them.
    """

  def update(recalc_normals = 0, store_edges = 0, vertex_shade = 0):
    """
    Update the mesh in Blender.  The changes made are put back to the mesh in
    Blender, if available, or put in a newly created mesh if this NMesh wasn't
    already linked to one.
    @type recalc_normals: int (bool)
    @param recalc_normals: if nonzero the vertex normals are recalculated.
    @type store_edges: int (bool)
    @param store_edges: deprecated, edges are always stored now.
    @type vertex_shade: int (bool)
    @param vertex_shade: if nonzero vertices are colored based on the
        current lighting setup, like when there are no vertex colors and no
        textured faces and a user enters Vertex Paint Mode in Blender (only
        lamps in visible layers account).  To use this functionality, be out of
        edit mode or else an error will be returned.
    @warn: edit mesh and normal mesh are two different structures in Blender,
        synchronized upon leaving or entering edit mode.  Always remember to
        leave edit mode (L{Window.EditMode}) before calling this update
        method, or your changes will be lost.  Even better: for the same reason
        programmers should leave EditMode B{before} getting a mesh, or changes
        made to the editmesh in Blender may not be visible to your script
        (check the example at the top of NMesh module doc).
    @warn: unlike the L{PutRaw} function, this method doesn't check validity of
        vertex, face and material lists, because it is meant to be as fast as
        possible (and already performs many tasks).  So programmers should make
        sure they only feed proper data to the nmesh -- a good general
        recommendation, of course.  It's also trivial to write code to check
        all data before updating, for example by comparing each item's type
        with the actual L{Types}, if you need to.
    @note: this method also redraws the 3d view and -- if 'vertex_shade' is
        nonzero -- the edit buttons window.
    @note: if your mesh disappears after it's updated, try
        L{Object.Object.makeDisplayList}.  'Subsurf' meshes (see L{getMode},
        L{setMode}) need their display lists updated, too.
    """

  def transform(matrix, recalc_normals = False):
    """
    Transforms the mesh by the specified 4x4 matrix, as returned by
    L{Object.Object.getMatrix}, though this will work with any invertible 4x4
    matrix type.  Ideal usage for this is exporting to an external file where
    global vertex locations are required for each object.
    Sometimes external renderers or file formats do not use vertex normals.
    In this case, you can skip transforming the vertex normals by leaving
    the optional parameter recalc_normals as False or 0 ( the default value ).
    
    Example::
     # This script outputs deformed meshes worldspace vertex locations
     # for a selected object
     import Blender
     from Blender import NMesh, Object
     
     ob = Object.GetSelected()[0] # Get the first selected object
     me = NMesh.GetRawFromObject(ob.name) # Get the objects deformed mesh data
     me.transform(ob.matrix)
   
     for v in me.verts:
       print 'worldspace vert', v.co
    
    @type matrix: Py_Matrix
    @param matrix: 4x4 Matrix which can contain location, scale and rotation. 
    @type recalc_normals: int (bool)
    @param recalc_normals: if True or 1, transform normals as well as vertex coordinates.
    @warn: if you call this method and later L{update} the mesh, the new
        vertex positions will be passed back to Blender, but the object
        matrix of each object linked to this mesh won't be automatically
        updated.  You need to set the object transformations (rotation,
        translation and scaling) to identities, then, or the mesh data will
        be changed ("transformed twice").
    """

  def getMode():
    """
    Get this mesh's mode flags.
    @rtype: int
    @return: ORed value.  See L{Modes}.
    """

  def setMode(m=None, m1=None, m2=None):
    """
    Set the mode flags for this mesh.  Given mode strings turn the mode "on".
    Modes not passed in are turned "off", so setMode() (without arguments)
    unsets all mode flags.
    @type m: string or int (bitflag)
    @param m: mode string or int.  An int (see L{Modes}) or from none to 3
      strings can be given:
       - "NoVNormalsFlip"
       - "TwoSided"
       - "AutoSmooth"
    """

  def addVertGroup(group):
    """
    Add a named and empty vertex (deform) group to the object this nmesh is
    linked to. If this nmesh was newly created or accessed with GetRaw, it must
    first be linked to an object (with object.link or NMesh.PutRaw) so the
    method knows which object to update.\n
    This is because vertex groups in Blender are stored in I{the object} --
    not in the mesh, which may be linked to more than one object. For this
    reason, it's better to use "mesh = object.getData()" than
    "mesh = NMesh.GetRaw(meshName)" to access an existing mesh.
    @type group: string
    @param group: the name for the new group.
    """

  def removeVertGroup(group):
    """
    Remove a named vertex (deform) group from the object linked to this nmesh.
    All vertices assigned to the group will be removed (just from the group,
    not deleted from the mesh), if any. If this nmesh was newly created, it
    must first be linked to an object (read the comment in L{addVertGroup} for
    more info).
    @type group: string
    @param group: the name of a vertex group.
    """

  def assignVertsToGroup(group, vertList, weight, assignmode = 'replace'):
    """
    Adds an array (a python list) of vertex points to a named vertex group
    associated with a mesh. The vertex list is a list of vertex indices from
    the mesh. You should assign vertex points to groups only when the mesh has
    all its vertex points added to it and is already linked to an object.

    I{B{Example:}}
    The example here adds a new set of vertex indices to a sphere primitive::
     import Blender
     sphere = Blender.Object.Get('Sphere')
     mesh = sphere.getData()
     mesh.addVertGroup('firstGroup')
     vertList = []
     for x in range(300):
         if x % 3 == 0:
             vertList.append(x)
     mesh.assignVertsToGroup('firstGroup', vertList, 0.5, 'add')

    @type group: string
    @param group: the name of the group.
    @type vertList: list of ints
    @param vertList: a list of vertex indices.
    @type weight: float
    @param weight: the deform weight for (which means: the amount of influence
        the group has over) the given vertices. It should be in the range
        [0.0, 1.0]. If weight <= 0, the given vertices are removed from the
        group.  If weight > 1, it is clamped.
    @type assignmode: string
    @param assignmode: Three choices:
        - 'add'
        - 'subtract'
        - 'replace'\n
	
        'B{add}': if the vertex in the list is not assigned to the group
        already, this creates a new association between this vertex and the
        group with the weight specified, otherwise the weight given is added to
        the current weight of an existing association between the vertex and
        group.\n
        'B{subtract}' will attempt to subtract the weight passed from a vertex
        already associated with a group, else it does nothing.\n
        'B{replace}' attempts to replace a weight with the new weight value
        for an already associated vertex/group, else it does nothing. 
       """

  def removeVertsFromGroup(group, vertList = None):
    """
    Remove a list of vertices from the given group.  If this nmesh was newly
    created, it must first be linked to an object (check L{addVertGroup}).
    @type group: string
    @param group: the name of a vertex group
    @type vertList: list of ints
    @param vertList: a list of vertex indices to be removed from the given
        'group'.  If None, all vertices are removed -- the group is emptied.
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

  def getMaxSmoothAngle():
    """
    Get the max angle for auto smoothing.
    Note: This will only affect smoothing generated at render time.
    Smoothing can also be set per face which is visible in Blenders 3D View.
    @return: The value in degrees.
    """

  def setMaxSmoothAngle(angle):
    """
    Set the max angle for auto smoothing.
    @type angle: int
    @param angle: The new value in degrees -- it's clamped to [1, 80].
    """

  def getSubDivLevels():
    """
    Get the mesh subdivision levels for realtime display and rendering.
    @return: list of ints: [display, render].
    """

  def setSubDivLevels(subdiv):
    """
    Set the mesh subdivision levels for realtime display and rendering.
    @type subdiv: list of 2 ints
    @param subdiv: new subdiv levels: [display, render].  Both are clamped to
        lie in the range [1, 6].
    """


