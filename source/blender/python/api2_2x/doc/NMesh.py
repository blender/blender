# Blender.NMesh module and the NMesh PyType object

"""
The Blender.NMesh submodule.

Mesh Data
=========

This module provides access to B{Mesh Data} objects in Blender.

Example::

  import Blender
  from Blender import NMesh, Material
  #
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

@type Modes: readonly dictionary
@type FaceFlags: readonly dictionary
@type FaceModes: readonly dictionary
@type FaceTranspModes: readonly dictionary
@var Modes: The available mesh modes.
    - NOVNORMALSFLIP - no flipping of vertex normals during render.
    - TWOSIDED - double sided mesh.
    - AUTOSMOOTH - turn auto smoothing of faces "on".
    - SUBSURF - turn Catmull-Clark subdivision of surfaces "on".
@var FaceFlags: The available face selection flags.
    - SELECT - selected.
    - HIDE - hidden.
    - ACTIVE - the active face.
@var FaceModes: The available face modes.
    - ALL - set all modes at once.
    - BILLBOARD - always orient after camera.
    - HALO - halo face, always point to camera.
    - DINAMYC - respond to collisions.
    - INVISIBLE - invisible face.
    - LIGHT - dinamyc lighting.
    - OBCOL - use object colour instead of vertex colours.
    - SHADOW - shadow type.
    - SHAREDVERT - apparently unused in Blender.
    - SHAREDCOL - shared vertex colours (per vertex).
    - TEX - has texture image.
    - TILES - uses tiled image.
    - TWOSIDE - two-sided face.
@var FaceTranspModes: The available face transparency modes. Note: these are
  ENUMS, they can't be combined (and'ed, or'ed, etc) like a bit vector.
    - SOLID - draw solid.
    - ADD - add to background (halo).
    - ALPHA - draw with transparency.
    - SUB - subtract from background.
"""

def Col(col = [255, 255, 255, 255]):
  """
  Get a new mesh rgba color.
  @type col: list
  @param col: A list [red, green, blue, alpha] of int values in [0, 255].
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

def GetRawFromObject(name):
  """
  Get the raw mesh data object from the Object in Blender called I{name}.
  @type name: string
  @param name: The name of an Object of type "Mesh".
  @rtype: NMesh
  @return: The NMesh wrapper of the mesh data from the Object called I{name}.
  @warn: This function gets I{deformed} mesh data, already modified for
      rendering (think "display list").  It also doesn't let you overwrite the
      original mesh in Blender, so if you try to update it, a new mesh will
      be created.
  """

def PutRaw(nmesh, name = None, recalculate_normals = 1):
  """
  Put an NMesh object back in Blender.
  @type nmesh: NMesh
  @type name: string
  @type recalculate_normals: int
  @param name: The name of the mesh data object in Blender which will receive
     this nmesh data.  It can be an existing mesh data object or a new one.
  @param recalculate_normals: If non-zero, the vertex normals for the mesh will
     be recalculated.
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
    rgba colour.
  @cvar r: The Red component in [0, 255].
  @cvar g: The Green component in [0, 255].
  @cvar b: The Blue component in [0, 255].
  @cvar a: The Alpha (transparency) component in [0, 255].
  """

class NMVert:
  """
  The NMVert object
  =================
    This object holds mesh vertex data.
  @type co: list of three floats
  @cvar co: The vertex coordinates (x, y, z).
  @type no: list of three floats
  @cvar no: The vertex normal vector (x, y, z).
  @type uvco: list of two floats
  @cvar uvco: The vertex texture "sticky" coordinates.
  @type index: int
  @cvar index: The vertex index, if owned by a mesh.
  @warn:  There are two kinds of uv texture coordinates in Blender: per vertex
     ("sticky") and per face vertex (uv in L{NMFace}).  In the first, there's
     only one uv pair of coordinates for each vertex in the mesh.  In the
     second, for each face it belongs to, a vertex can have different uv
     coordinates.  This makes the per face option more flexible, since two
     adjacent faces won't have to be mapped to a continuous region in an image:
     each face can be independently mapped to any part of its texture.
  """

class NMFace:
  """
  The NMFace object
  =================
    This object holds mesh face data.
  @type v: list
  @cvar v: The list of face vertices (B{up to 4}).
  @cvar col: The list of vertex colours.
  @cvar mat: Same as I{materialIndex} below.
  @cvar materialIndex: The index of this face's material in its NMesh materials
      list.
  @cvar smooth: If non-zero, the vertex normals are averaged to make this
     face look smooth.
  @cvar image: The Image used as a texture for this face.
  @cvar mode: The display mode (see L{Mesh.FaceModes<FaceModes>})
  @cvar flag: Bit vector specifying selection flags
     (see L{NMesh.FaceFlags<FaceFlags>}).
  @cvar transp: Transparency mode bit vector
     (see L{NMesh.FaceTranspModes<FaceTranspModes>}).
  @cvar uv: List of per-face UV coordinates: [(u0, v0), (u1, v1), ...].
  @cvar normal: (or just B{no}) The normal vector for this face: [x,y,z].
  @warn: Assigning uv textures to mesh faces in Blender works like this:
    1. Select your mesh.
    2. Enter face select mode (press f) and select at least some face(s).
    3. In the UV/Image Editor window, load / select an image.
    4. Play in both windows (better split the screen to see both at the same
       time) until the uv coordinates are where you want them.  Hint: in the
       3d window, the 'u' key opens a menu of default uv choices and the 'r'
       key lets you rotate the uv coords.
    5. Leave face select mode (press f).
  """

  def append(vertex):
    """
    Append a vertex to this face's vertex list.
    @type vertex: NMVert
    @param vertex: An NMVert object.
    """

class NMesh:
  """
  The NMesh Data object
  =====================
    This object gives access to mesh data in Blender.  We refer to mesh as the
    object in Blender and NMesh as its Python counterpart.
  @cvar name: The NMesh name.  It's common to use this field to store extra
     data about the mesh (to be exported to another program, for example).
  @cvar materials: The list of materials used by this NMesh.
  @cvar verts: The list of NMesh vertices (NMVerts).
  @cvar users: The number of Objects using (linked to) this mesh.
  @cvar faces: The list of NMesh faces (NMFaces).
  @cvar mode:  The mode flags for this mesh.  See L{setMode}.
  @cvar subDivLevels: The [display, rendering] subdivision levels in [1, 6].
  @cvar maxSmoothAngle: The max angle for auto smoothing.  See L{setMode}.
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

  def hasVertexColours(flag = None):
    """
    Get (and optionally set) if this NMesh has vertex colours.
    @type flag: int
    @param flag: If given and non-zero, the "vertex colour" flag for this NMesh
        is turned I{on}.
    @rtype: bool
    @return: The current value of the "vertex colour" flag.
    @warn: If a mesh has both vertex colours and textured faces, this function
       will return False.  This is due to the way Blender deals internally with
       the vertex colours array (if there are textured faces, it is copied to
       the textured face structure and the original array is freed/deleted).
       If you want to know if a mesh has both textured faces and vertex
       colours, set *in Blender* the "VCol Paint" flag for each material that
       covers an area that was also vertex painted and then check in your
       Python script if that material flag is set.  Of course also tell others
       who use your script to do the same.  The "VCol Paint" material mode flag
       is the way to tell Blender itself to render with vertex colours, too, so
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
    """

  def getVertexInfluences(index):
    """
    Get influences of bones in a specific vertex.
    @type index: int
    @param index: The index of a vertex.
    @rtype: list
    @return: List of pairs (name, weight), where name is the bone name (string)
        and its weight is a float value.
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
    @return: True if succesful or False if this NMesh wasn't linked to a real
       Blender Mesh yet (or was, but the Mesh had no keys).
    @warn: Currently the mesh keys from meshs that are grabbed with
       NMesh.GetRaw() or .GetRawFromObject() are preserved, so if you want to
       clear them or don't want them at all, remember to call this method.  Of
       course NMeshes created with NMesh.New() don't have mesh keys until you
       add them.
    """

  def update(recalc_normals = 0):
    """
    Update the mesh in Blender.  The changes made are put back to the mesh in
    Blender, if available, or put in a newly created mesh object if this NMesh
    wasn't linked to one, yet.
    @type recalc_normals: int
    @param recalc_normals: If given and equal to 1, the vertex normals are
        recalculated.
    """

  def getMode():
    """
    Get this mesh's mode flags.
    @rtype: int
    @return: Or'ed value.  See L{Modes}.
    """

  def setMode(m = None, m1=None, m2=None, m3=None):
    """
    Set the mode flags for this mesh.  Given mode strings turn the mode "on".
    Modes not passed in are turned "off", so setMode() (without arguments)
    unsets all mode flags.
    @type m: string
    @param m: mode string.  From none to 4 can be given:
       - "NoVNormalsFlip"
       - "TwoSided"
       - "AutoSmooth"
       - "SubSurf"
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
        - 'substract'
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

  def getMaxSmoothAngle():
    """
    Get the max angle for auto smoothing.
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
