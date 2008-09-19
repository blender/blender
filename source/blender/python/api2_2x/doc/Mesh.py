# Blender.Mesh module and the Mesh PyType object

"""
The Blender.Mesh submodule.

B{New}:

Mesh Data
=========

This module provides access to B{Mesh Data} objects in Blender.  It differs
from the NMesh module by allowing direct access to the actual Blender data, 
so that changes are done immediately without need to update or put the data
back into the original mesh.  The result is faster operations with less memory
usage.  The example below creates a simple pyramid, and sets some of the
face's attributes (the vertex color):

Example::
	from Blender import *
	import bpy

	editmode = Window.EditMode()    # are we in edit mode?  If so ...
	if editmode: Window.EditMode(0) # leave edit mode before getting the mesh

	# define vertices and faces for a pyramid
	coords=[ [-1,-1,-1], [1,-1,-1], [1,1,-1], [-1,1,-1], [0,0,1] ]	
	faces= [ [3,2,1,0], [0,1,4], [1,2,4], [2,3,4], [3,0,4] ]

	me = bpy.data.meshes.new('myMesh')          # create a new mesh

	me.verts.extend(coords)          # add vertices to mesh
	me.faces.extend(faces)           # add faces to the mesh (also adds edges)

	me.vertexColors = 1              # enable vertex colors 
	me.faces[1].col[0].r = 255       # make each vertex a different color
	me.faces[1].col[1].g = 255
	me.faces[1].col[2].b = 255

	scn = bpy.data.scenes.active     # link object to current scene
	ob = scn.objects.new(me, 'myObj')

	if editmode: Window.EditMode(1)  # optional, just being nice

Vertices, edges and faces are added to a mesh using the .extend() methods.
For best speed and efficiency, gather all vertices, edges or faces into a
list and call .extend() once as in the above example.  Similarly, deleting
from the mesh is done with the .delete() methods and are most efficient when
done once.

@type Modes: readonly dictionary
@type FaceFlags: readonly dictionary
@type FaceModes: readonly dictionary
@type FaceTranspModes: readonly dictionary
@var Modes: The available mesh modes.
		- NOVNORMALSFLIP - no flipping of vertex normals during render.
		- TWOSIDED - double sided mesh.
		- AUTOSMOOTH - turn auto smoothing of faces "on".
		- note: SUBSURF and OPTIMAL have been removed, use Modifiers to apply subsurf.
@var FaceFlags: The available *texture face* (uv face select mode) selection
	flags.  Note: these refer to TexFace faces, available if mesh.faceUV
	returns true.
		- SELECT - selected (deprecated in versions after 2.43, use face.sel).
		- HIDE - hidden  (deprecated in versions after 2.43, use face.hide).
		- ACTIVE - the active face, read only - Use L{mesh.activeFace<Mesh.Mesh.activeFace>} to set.
@var FaceModes: The available *texture face* modes. Note: these are only
	meaningful if mesh.faceUV returns true, since in Blender this info is
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
		- SELECT - selected (B{deprecated}).  Use edge.sel attribute instead.
		- EDGEDRAW - edge is drawn out of edition mode.
		- EDGERENDER - edge is drawn out of edition mode.
		- SEAM - edge is a seam for UV unwrapping
		- FGON - edge is part of a F-Gon.
		- LOOSE - Edge is not a part of a face (only set on leaving editmode)
		- SHARP - Edge will be rendered sharp when used with the "Edge Split" modifier.
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
@type SelectModes: readonly dictionary.
@var SelectModes: The available edit select modes.
	- VERTEX: vertex select mode.
	- EDGE: edge select mode.
	- FACE: face select mode.
"""

AssignModes = {'REPLACE':1}

def Get(name=None):
	"""
	Get the mesh data object called I{name} from Blender.
	@type name: string
	@param name: The name of the mesh data object.
	@rtype: Mesh
	@return: If a name is given, it returns either the requested mesh or None.
		If no parameter is given, it returns all the meshes in the current scene.
	"""

def New(name='Mesh'):
	"""
	Create a new mesh data object called I{name}.
	@type name: string
	@param name: The name of the mesh data object.
	@rtype: Mesh
	@return: a new Blender mesh.
	@note: if the mesh is not linked to an object, its datablock will be deleted
	when the object is deallocated.
	"""

def Mode(mode=0):
	"""
	Get and/or set the selection modes for mesh editing.  These are the modes
	visible in the 3D window when a mesh is in Edit Mode.
	@type mode: int
	@param mode: The desired selection mode.  See L{SelectModes} for values.
	Modes can be combined.  If omitted, the selection mode is not changed.
	@rtype: int
	@return: the current selection mode.
	@note: The selection mode is an attribute of the current scene.  If the
	scene is changed, the selection mode may not be the same.
	"""

def Unlink(name):
	"""
	Delete an unused mesh from Blender's database.  The mesh must not have
	any users (i.e., it must not be linked to any object).  
	@type name: string
	@param name: The name of the mesh data object.  
	@rtype: None
	@note: This function may be a temporary solution; it may be replaced
	in the future by a more general unlink function for many datablock types.
	Hopefully this will be decided prior to the 2.42 release of Blender.
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
	@type co: vector (WRAPPED DATA)
	@ivar no: The vertex's unit normal vector (x, y, z).
		B{Note}: if vertex coordinates are changed, it may be necessary to use
		L{Mesh.calcNormals()} to update the vertex normals.
		B{Note}: Vertex normals can be set, but are not wrapped so modifying a normal
		vector will not effect the verts normal. The result is only visible
		when faces have the smooth option enabled.
		Example::
			# This won't work.
			for v in me.verts:
				v.no.x= 0
				v.no.y= 0
				v.no.z= 1
			# This will work
			no= Blender.Mathutils.Vector(0,0,1)
			for v in me.verts:
				v.no= no
	@type no: vector
	@ivar uvco: The vertex texture "sticky" coordinates (x, y),
		B{Note}: These are not seen in the UV editor and they are not a part of UV a UVLayer. Use face UV's for that.
		if present. Available for MVerts only. 
		Use L{Mesh.vertexUV} to test for presence before trying to access;
		otherwise an exception will may be thrown.
		(Sticky coordinates can be set when the object is in the Edit mode;
		from the Editing Panel (F9), look under the "Mesh" properties for the 
		"Sticky" button).  
	@type uvco: vector (WRAPPED DATA)
	@ivar index: The vertex's index within the mesh (MVerts only). Read-only.
	@type index: int
	@ivar sel: The vertex's selection state (selected=1).
		B{Note}: a Mesh will return the selection state of the mesh when EditMode 
		was last exited. A Python script operating in EditMode must exit EditMode 
		before getting the current selection state of the mesh.
	@type sel: int
	@ivar hide: The face's B{edit mode} visibility state (hidden=1).
	@type hide: int
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
		Append zero or more vertices to the mesh.  Unlike L{MEdgeSeq.extend()} and
		L{MFaceSeq.extend()} no attempt is made to check for duplicate vertices in
		the parameter list, or for vertices already in the mesh.
		@note: Since Blender 2.44 all new verts are selected.

		Example::
			import Blender
			from Blender import Mesh
			from Blender.Mathutils import Vector

			me = Mesh.Get("Plane")          # get the mesh data called "Plane"
			me.verts.extend(1,1,1)          # add one vertex
			l=[(.1,.1,.1),Vector([2,2,.5])]
			me.verts.extend(l)              # add multiple vertices

		@type coords: sequences(s) of floats or vectors
		@param coords: coords can be
			- a sequence of three floats,
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
	@ivar length: The length of the edge, same as (ed.v1.co-ed.v2.co).length where "ed" is an MEdge.
	@type length: float
	@ivar crease: The crease value of the edge. It is in the range [0,255].
	@type crease: int
	@ivar flag: The bitfield describing edge properties. See L{EdgeFlags}.
		Example::
			# This script counts fgon and non fgon edges
			from Blender import Scene, Mesh
			scn= Scene.GetCurrent() # Current scene, important to be scene aware
			ob= scn.objects.active # last selected object
			me= ob.getData(mesh=1) # thin wrapper doesn't copy mesh data like nmesh
		
			total_fgon_eds= total_nor_eds= 0
			
			# Look through the edges and find any fgon edges, then print the findings to the console
			for ed in me.edges: # all meshes have edge data now
				if ed.flag & Mesh.EdgeFlags.FGON:
					total_fgon_eds+=1
				else:
					total_nor_eds+=1
			
			print 'Blender has', total_fgon_eds, 'fgon edges and', total_nor_eds, 'non fgon edges'
	@type flag: int
	@ivar index: The edge's index within the mesh.  Read-only.
	@type index: int
	@ivar sel: The edge's B{edit mode} selection state (selected=1).  B{Note}:
	changing the select state of an edge changes the select state of the edge's
	vertices.
	@type sel: int
	@ivar key: The edge's vert indices in an ordered tuple, which can be used
	as a dictionary key. Read-only.
	This is the same as (min(ed.v1.index, ed.v2.index), max(ed.v1.index, ed.v2.index))
	@type key: tuple
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
		Add zero or more edges to the mesh.  Edges which already exist in the 
		mesh or with both vertices the same are ignored.  If three or four verts
		are specified in any sequence, an edge is also created between the first
		and last vertices (this is useful when adding faces).  
		@note: Since Blender 2.44 all new edges are selected.

		Example::
			import Blender
			from Blender import Mesh

			me = Mesh.Get("Plane")          # get the mesh data called "Plane"
			v = me.verts                    # get vertices
			if len(v) >= 6:                 # if there are enough vertices...
				me.edges.extend(v[0],v[1])    #   add a single edge
				l=[(v[1],v[2],v[3]),[0,2,4,5]]
				me.edges.extend(l)            #   add multiple edges

		@type vertseq: sequence(s) of ints or MVerts
		@param vertseq: either two to four ints or MVerts, or sequence
		(list or tuple) of sequences each containing two to four ints or MVerts.
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
			f.sel = not f.sel # 1 becomes 0, 0 becomes 1

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
		This is not the same as the selection state of the textured faces
		(see L{flag}). B{Note}: changing the select state of a face changes
		the select state of the face's vertices.
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
		Will throw an exception if L{Mesh.vertexColors} is False.

		Example::
			# This example uses vertex normals to apply normal colors to each face.
			import bpy
			from Blender import Window
			scn= bpy.scenes.active	# Current scene, important to be scene aware
			ob= scn.objects.active	# last selected object
			me= ob.getData(mesh=1)	# thin wrapper doesn't copy mesh data like nmesh
			me.vertexColors= True	# Enable face, vertex colors
			for f in me.faces:
				for i, v in enumerate(f):
					no= v.no
					col= f.col[i]
					col.r= int((no.x+1)*128)
					col.g= int((no.y+1)*128)
					col.b= int((no.z+1)*128)
			Window.RedrawAll()
	@type col: tuple of MCols
	@ivar mat: The face's index into the mesh's materials
			list.  It is in the range [0,15].
	@type mat: int
	@ivar image: The Image used as a texture for this face.
			Setting this attribute will create UV faces if they do not exist.
			Getting this attribute throw an exception if the mesh does not have 
			UV faces; use L{Mesh.faceUV} to test.  
			Assigning an image will automatically set the TEX attribute of the
			L{mode} bitfield.  Use "del f.image" or "f.image = None" to clear the
			image assigned to the face.
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
			Setting this attribute will create UV faces if they do not exist.
			Getting this attribute throw an exception if the mesh does not have 
			UV faces; use L{Mesh.faceUV} to test.  
	@type uv: tuple of vectors (WRAPPED DATA)
	@ivar uvSel: The face's UV coordinates selection state; a 1 indicates the
			vertex is selected.  Each vertex has its own UV coordinate select state
			(this is not the same as the vertex's edit mode selection state).
			Setting this attribute will create UV faces if they do not exist.
			Getting this attribute throw an exception if the mesh does not have 
			UV faces; use L{Mesh.faceUV} to test.  
	@type uvSel: tuple of ints
	@ivar no: The face's normal vector (x, y, z).  Read-only.
	@type no: vector
	@ivar cent: The center of the face. Read-only.
	@type cent: vector
	@ivar area: The area of the face. Read-only.
	@type area: float
	@ivar edge_keys: A tuple, each item a key that can reference an edge by its
	ordered indices. Read-only.  This is useful for building connectivity data.
	Example::
			from Blender import Mesh
			me = Mesh.Get('Cube')
			# a dictionary where the edge is the key, and a list of faces that use it are the value
			edge_faces = dict([(ed.key, []) for ed in me.edges])

			# Add the faces to the dict
			for f in me.faces:
				for key in f.edge_keys:
					edge_faces[key].append(f) # add this face to the edge as a user

			# Print the edges and the number of face users
			for key, face_users in edge_faces.iteritems():
				print 'Edge:', key, 'uses:', len(face_users),'faces'

	@type edge_keys: tuple
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

	def __len__():
		"""
		len for MVert.  It returns the number of vertices in the face.
		@rtype: int
		"""

class MFaceSeq:
	"""
	The MFaceSeq object
	===================
		This object provides sequence and iterator access to the mesh's faces.
	"""

	def extend(vertseq,ignoreDups=True,indexList=True):
		"""
		Add zero or more faces and edges to the mesh.  Faces which already exist
		in the mesh, or faces which contain the same vertex multiple times are
		ignored.  Sequences of two vertices are accepted, but no face will be
		created.
		@note: Since Blender 2.44 all new faces are selected.

		Example::
			import Blender
			from Blender import Mesh

			me = Mesh.Get("Plane")          # get the mesh data called "Plane"
			v = me.verts                    # get vertices
			if len(v) >= 6:                 # if there are enough vertices...
				me.faces.extend(v[1],v[2],v[3]) #   add a single edge
				l=[(v[0],v[1]),[0,2,4,5]]
				me.faces.extend(l)            #   add another face

		@type vertseq: sequence(s) of MVerts
		@param vertseq: either two to four ints or MVerts, or sequence (list or
		tuple) of sequences each containing two to four ints or MVerts.
		@type ignoreDups: boolean
		@param ignoreDups: keyword parameter (default is False).  If supplied and
		True, do not check the input list or mesh for duplicate faces.  This can
		speed up scripts but can prossibly produce undesirable effects.  Only
		use if you know what you're doing.
		@type indexList: boolean
		@param indexList: keyword parameter (default is False).  If supplied and
		True, the method will return a list representing the new index for each
		face in the input list.  If faces are removed as duplicates, None is
		inserted in place of the index.
		@type smooth: boolean
		@param smooth: keyword parameter (default is False).  If supplied new faces will have smooth enabled.
		@warning: Faces using the first vertex at the 3rd or 4th location in the
		face's vertex list will have their order rotated so that the zero index
		on in the first or second location in the face. When creating face data
		with UVs or vertex colors, you may need to work around this, either by
		checking for zero indices yourself or by adding a dummy first vertex to
		the mesh that can be removed when your script has finished.
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

	def sort():
		"""
		Sorts the faces using exactly the same syntax as pythons own list sorting function.

		Example::
			import Blender
			from Blender import Mesh
			me = Mesh.Get('mymesh')
			
			me.faces.sort(key=lambda f: f.area)
			
			me.faces.sort(key=lambda f: f.cent)
		
		@note: Internally faces only refer to their index, so after sorting, faces you alredy have will not have their index changed to match the new sorted order. 
		"""
		
	def selected():
		"""
		Get selected faces.
		@return: a list of the indices for all faces selected in edit mode.
		@rtype: list of ints
		"""

from IDProp import IDGroup, IDArray
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
		B{Note}: Making the material list shorter does not change the face's material indices.
		Take care when using the face's material indices to reference a material in this list.
		B{Note}: The list that's returned is I{not} linked to the original mesh.
		mesh.materials.append(material) won't do anything.
		Use mesh.materials += [material] instead.
	@type materials: list of L{Material}s
	@ivar degr: The max angle for auto smoothing in [1,80].  
	@type degr: int
	@ivar maxSmoothAngle: Same as L{degr}.  This attribute is only for
		compatibility with NMesh scripts and will probably be deprecated in 
		the future.
	@ivar mode: The mesh's mode bitfield.  See L{Modes}.
	@type mode: int
	@ivar sel: Sets selection status for all vertices, edges and faces in the
		mesh (write only).
	@type sel: boolean
	@ivar hide: Sets hidden status for all vertices, edges and faces in the
		mesh (write only).
	@type hide: boolean
	@ivar subDivLevels: The [display, rendering] subdivision levels in [1, 6].
	@type subDivLevels: list of 2 ints
	@ivar faceUV: The mesh contains UV-mapped textured faces.
	@type faceUV: bool
	@ivar vertexColors: The mesh contains vertex colors. Set True to add vertex colors.
	@type vertexColors: bool
	@ivar vertexUV: The mesh contains "sticky" per-vertex UV coordinates.
	@type vertexUV: bool
	@ivar activeFace: Index of the mesh's active face in UV Face Select and
		Paint modes.  Only one face can be active at a time.  Note that this is
		independent of the selected faces in Face Select and Edit modes.
		Will throw an exception if the mesh does not have UV faces; use
		L{faceUV} to test.
	@type activeFace: int
	@ivar activeGroup: The mesh's active vertex group.  The mesh must be
		linked to an object (read the comment in L{addVertGroup} for more info).
	@type activeGroup: string or None
	@ivar texMesh: The mesh's texMesh setting, used so coordinates from another
		mesh can be used for rendering textures.
	@type texMesh: Mesh or None
	@ivar key: The L{Key<Key.Key>} object containing the keyframes for this mesh, if any.
	@type key: Key or None
	@ivar activeUVLayer: The mesh's active UV/Image layer. None if there is no UV/Image layers.

		B{Note}: After setting this value, call L{update} so the result can be seen the the 3d view.
	@type activeUVLayer: string
	@ivar activeColorLayer: The mesh's active Vertex Color layer. None if there is no UV/Image layers.

		B{Note}: After setting this value, call L{update} so the result can be seen the the 3d view.
	@type activeColorLayer: string
	
	@ivar renderUVLayer: The mesh's rendered UV/Image layer. None if there is no UV/Image layers.
	@type renderUVLayer: string
	@ivar renderColorLayer: The mesh's rendered Vertex Color layer. None if there is no UV/Image layers.
	@type renderColorLayer: string
	
	@ivar multires: The mesh has multires data, set True to add multires data.
		Will throw an exception if the mesh has shape keys; use L{key} to test.
	@type multires: bool
	@ivar multiresLevelCount: The mesh has multires data. (read only)
	@type multiresLevelCount: int
	@ivar multiresDrawLevel: The multires level to display in the 3dview in [1 - multiresLevelCount].
	@type multiresDrawLevel: int
	@ivar multiresEdgeLevel: The multires level edge display in the 3dview [1 - multiresLevelCount].
	@type multiresEdgeLevel: int
	@ivar multiresPinLevel: The multires pin level, used for applying modifiers [1 - multiresLevelCount].
	@type multiresPinLevel: int
	@ivar multiresRenderLevel: The multires level to render [1 - multiresLevelCount].
	@type multiresRenderLevel: int
	
	
	"""

	def getFromObject(object, cage=0, render=0):
		"""
		Replace the mesh's existing data with the raw mesh data from a Blender
		Object.  This method supports all the geometry based objects (mesh, text,
		curve, surface, and meta).  If the object has modifiers, they will be
		applied before to the object before extracting the vertex data unless
		the B{cage} parameter is 1.
		@note: The mesh coordinates are in I{local space}, not the world space of
		its object.  For world space vertex coordinates, each vertex location must
		be multiplied by the object's 4x4 transform matrix (see L{transform}).
		@note: The objects materials will not be copied into the existing mesh,
		however the face material indices will match the material list of the original data.
		@type object: blender object or string
		@param object: The Blender object or its name, which contains the geometry data.
		@type cage: int
		@param cage: determines whether the original vertices or derived vertices
		@type render: int
		@param render: determines whether the render setting for modifiers will be used or not.
		(for objects with modifiers) are used.  The default is derived vertices.
		"""

	def calcNormals():
		"""
		Recalculates the vertex normals using face data.
		"""
	
	def pointInside(point, selected_only=False):
		"""
		@type point: vector
		@param point: Test if this point is inside the mesh
		@type selected_only: bool
		@param selected_only: if True or 1, only the selected faces are taken into account.
		Returns true if vector is inside the mesh.
		@note: Only returns a valid result for mesh data that has no holes.
		@note: Bubbles in the mesh work as expect.
		"""
	def getTangents():
		"""
		Calculates tangents for this mesh, returning a list of tuples,
		each with 3 or 4 tangent vectors, these are alligned with the meshes faces.
		
		Example::
			# Display the tangents as edges over a the active mesh object
			from Blender import *
			sce = Scene.GetCurrent()
			ob = sce.objects.active
			
			me = ob.getData(mesh=1)
			ts = me.getTangents()
			me_disp = Mesh.New()
			
			verts = []
			edges = []
			for i, f in enumerate(me.faces):
				ft = ts[i]
				for j, v in enumerate(f):
					tan = ft[j]
					print tan
					co = v.co
					
					verts.append(co)
					verts.append(co+tan)
					
					i = len(verts)
					edges.append((i-1, i-2))
			
			me_disp.verts.extend( verts )
			me_disp.edges.extend( edges )
			
			sce.objects.new( me_disp )
		
		@note: The tangents are computed using the active UV layer, if there are no UV layers, orco coords are used.
		"""
	
	
	def transform(matrix, recalc_normals = False, selected_only=False):
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
		@type selected_only: bool
		@param selected_only: if True or 1, only the selected verts will be transformed.
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

	def update(key=None):
		"""
		Update display lists after changes to mesh.  B{Note}: with changes taking
		place for using a directed acyclic graph (DAG) for scene and object
		updating, this method may be only temporary and may be removed in future
		releases.
		@type key: string
		@param key: Use this optional argument to write the current vertex
		locations to the a shape key. the name must match an existing shape key for this mesh
		See L{Mesh.Mesh.key} and L{Key.Key.blocks} to get a list of the named shape keys, setting the active keys is
		done from the object with L{Object.Object.pinShape}, L{Object.Object.activeShape}.
		
		
		
		@warn: Since Blender 2.42 this function has changed; now it won't recalculate
		vertex normals (seen when faces are smooth). See L{Mesh.calcNormals()}.
		"""

	def findEdges(edges):
		"""
		Quickly search for the location of an edges.  
		@type edges: sequence(s) of ints or MVerts
		@param edges: can be tuples of MVerts or integer indexes (B{note:} will
			not work with PVerts) or a sequence (list or tuple) containing two or
			more sequences.
		@rtype: int, None or list
		@return: if an edge is found, its index is returned; otherwise None is
		returned.  If a sequence of edges is passed, a list is returned.
		"""

	def addVertGroup(group):
		"""
		Add a named and empty vertex (deform) group to the object this mesh is
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

	def assignVertsToGroup(group, vertList, weight, assignmode):
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
		Remove a list of vertices from the given group.  If this mesh was newly
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
		@param weightsFlag: if 1, each item in the list returned contains a
			tuple pair (index, weight), the weight is a float between 0.0 and 1.0.
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

	def getUVLayerNames():
		"""
		Return a list of all UV layer names
		@rtype: list of strings
		@return: returns a list of strings representing all UV layers
		associated with the mesh's object
		"""

	def getColorLayerNames():
		"""
		Return a list of all color layer names
		@rtype: list of strings
		@return: returns a list of strings representing all color layers
		associated with the mesh's object
		"""

	def getVertexInfluences(index):
		"""
		Get the bone influences for a specific vertex.
		@type index: int
		@param index: The index of a vertex.
		@rtype: list of lists
		@return: List of pairs [name, weight], where name is the bone name (string)
				and weight is a float value.
		"""

	def removeAllKeys():
		"""
		Remove all mesh keys stored in this mesh.
		@rtype: bool
		@return: True if successful or False if the Mesh has no keys.
		"""

	def insertKey(frame = None, type = 'relative'):
		"""
		Insert a mesh key at the given frame. 
		@type frame: int
		@type type: string
		@param frame: The Scene frame where the mesh key should be inserted.  If
				None or the arg is not given, the current frame is used.
		@param type: The mesh key type: 'relative' or 'absolute'.  This is only
				relevant on meshes with no keys.
		@warn: This and L{removeAllKeys} were included in this release only to
				make accessing vertex keys possible, but may not be a proper solution
				and may be substituted by something better later.  For example, it
				seems that 'frame' should be kept in the range [1, 100]
				(the curves can be manually tweaked in the Ipo Curve Editor window in
				Blender itself later).
		@warn: Will throw an error if the mesh has multires. use L{multires} to check.
		"""

	def addUVLayer(name):
		"""
		Adds a new UV/Image layer to this mesh, it will always be the last layer but not made active.
		@type name: string
		@param name: The name of the new UV layer, 31 characters max.
		"""

	def addColorLayer(name):
		"""
		Adds a new Vertex Color layer to this mesh, it will always be the last layer but not made active.
		@type name: string
		@param name: The name of the new Color layer, 31 characters max.
		"""

	def addMultiresLevel(levels = 1, type = 'catmull-clark'):
		"""
		Adds multires levels to this mesh.
		@type levels: int
		@param levels: The number of levels to add
		@type type: string
		@param type: The type of multires level, 'catmull-clark' or 'simple'.
		"""

	def removeUVLayer(name):
		"""
		Removes the active UV/Image layer.
		@type name: string
		@param name: The name of the UV layer to remove.
		"""

	def removeColorLayer(name):
		"""
		Removes the active Vertex Color layer.
		@type name: string
		@param name: The name of the Color layer to remove.
		"""

	def renameUVLayer(name, newname):
		"""
		Renames the UV layer called name to newname.
		@type name: string
		@param name: The UV layer to rename.
		@type newname: string
		@param newname: The new name of the UV layer, will be made unique.
		"""

	def renameColorLayer(name, newname):
		"""
		Renames the color layer called name to newname.
		@type name: string
		@param name: The Color layer to rename.
		@type newname: string
		@param newname: The new name of the Color layer, will be made unique.
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

	def __copy__ ():
		"""
		Make a copy of this mesh
		@rtype: Mesh
		@return:  a copy of this mesh
		"""

import id_generics
Mesh.__doc__ += id_generics.attributes
