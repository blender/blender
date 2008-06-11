# Blender.Object module and the Object PyType object

"""
The Blender.Object submodule

B{New}:
	- Addition of attributes for particle deflection, softbodies, and
		rigidbodies.
	- Objects now increment the Blender user count when they are created and
		decremented it when they are destroyed.  This means Python scripts can
		keep the object "alive" if it is deleted in the Blender GUI.
	- L{Object.getData} now accepts two optional bool keyword argument to
			define (1) if the user wants the data object or just its name
			and (2) if a mesh object should use NMesh or Mesh.
	- L{Object.clearScriptLinks} accepts a parameter now.
	- Object attributes: renamed Layer to L{Layers<Object.Object.Layers>} and
		added the easier L{layers<Object.Object.layers>}.  The old form "Layer"
		will continue to work.


Object
======

This module provides access to the B{Objects} in Blender.

Example::

	import Blender
	scn = Blender.Scene.GetCurrent()      # get the current scene
	cam = Blender.Camera.New('ortho')     # make ortho camera data object
	ob = scn.objects.new(cam)             # make a new object in this scene using the camera data
	ob.setLocation (0.0, -5.0, 1.0)       # position the object in the scene

	Blender.Redraw()                      # redraw the scene to show the updates.

@type DrawModes: readonly dictionary
@var DrawModes: Constant dict used for with L{Object.drawMode} bitfield
	attribute.  Values can be ORed together.  Individual bits can also
	be set/cleared with boolean attributes.
		- AXIS: Enable display of active object's center and axis.
		- TEXSPACE: Enable display of active object's texture space.
		- NAME: Enable display of active object's name.
		- WIRE: Enable the active object's wireframe over solid drawing.
		- XRAY: Enable drawing the active object in front of others.
		- TRANSP: Enable transparent materials for the active object (mesh only).

@type DrawTypes: readonly dictionary
@var DrawTypes: Constant dict used for with L{Object.drawType} attribute.
	Only one type can be selected at a time.
		- BOUNDBOX: Only draw object with bounding box
		- WIRE: Draw object in wireframe
		- SOLID: Draw object in solid
		- SHADED: Draw object with shaded or textured

@type ParentTypes: readonly dictionary
@var ParentTypes: Constant dict used for with L{Object.parentType} attribute.
		- OBJECT: Object parent type.
		- CURVE: Curve deform parent type.
		- LATTICE: Lattice deform parent type. Note: This is the same as ARMATURE, 2.43 was released with LATTICE as an invalid value.
		- ARMATURE: Armature deform parent type.
		- VERT1: 1 mesh vert parent type.
		- VERT3: 1 mesh verts parent type.
		- BONE: Armature bone parent type.
			

@type ProtectFlags: readonly dictionary
@var ProtectFlags: Constant dict used for with L{Object.protectFlags} attribute.
	Values can be ORed together.  
		- LOCX, LOCY, LOCZ: lock x, y or z location individually
		- ROTX, ROTY, ROTZ: lock x, y or z rotation individually
		- SCALEX, SCALEY, SCALEZ: lock x, y or z scale individually
		- LOC, ROT, SCALE: lock all 3 attributes for location, rotation or scale

@type PITypes: readonly dictionary
@var PITypes: Constant dict used for with L{Object.piType} attribute.
	Only one type can be selected at a time.
		- NONE: No force influence on particles
		- FORCE: Object center attracts or repels particles ("Spherical")
		- VORTEX: Particles swirl around Z-axis of the object
		- WIND: Constant force applied in direction of object Z axis
		- GUIDE: Use a Curve Path to guide particles

@type RBFlags: readonly dictionary
@var RBFlags: Constant dict used for with L{Object.rbFlags} attribute.
	Values can be ORed together.  
		- SECTOR: All game elements should be in the Sector boundbox
		- PROP: An Object fixed within a sector
		- BOUNDS: Specify a bounds object for physics
		- ACTOR: Enables objects that are evaluated by the engine
		- DYNAMIC: Enables motion defined by laws of physics (requires ACTOR)
		- GHOST: Enable objects that don't restitute collisions (requires ACTOR)
		- MAINACTOR: Enables MainActor (requires ACTOR)
		- RIGIDBODY: Enable rolling physics (requires ACTOR, DYNAMIC)
		- COLLISION_RESPONSE: Disable auto (de)activation (requires ACTOR, DYNAMIC)
		- USEFH: Use Fh settings in Materials (requires ACTOR, DYNAMIC)
		- ROTFH: Use face normal to rotate Object (requires ACTOR, DYNAMIC)
		- ANISOTROPIC: Enable anisotropic friction (requires ACTOR, DYNAMIC)
		- CHILD: reserved

@type IpoKeyTypes: readonly dictionary
@var IpoKeyTypes: Constant dict used for with L{Object.insertIpoKey} attribute.
	Values can be ORed together.
		- LOC
		- ROT
		- SIZE
		- LOCROT
		- LOCROTSIZE
		- LAYER
		- PI_STRENGTH
		- PI_FALLOFF
		- PI_SURFACEDAMP
		- PI_RANDOMDAMP
		- PI_PERM

@type RBShapes: readonly dictionary
@var RBShapes: Constant dict used for with L{Object.rbShapeBoundType}
	attribute.  Only one type can be selected at a time.  Values are
	BOX, SPHERE, CYLINDER, CONE, and POLYHEDERON

"""

def New (type, name='type'):
	"""
	Creates a new Object.  Deprecated; instead use Scene.objects.new().
	@type type: string
	@param type: The Object type: 'Armature', 'Camera', 'Curve', 'Lamp', 'Lattice',
		'Mball', 'Mesh', 'Surf' or 'Empty'.
	@type name: string
	@param name: The name of the object. By default, the name will be the same
		as the object type.
		If the name is already in use, this new object will have a number at the end of the name.
	@return: The created Object.

	I{B{Example:}}

	The example below creates a new Lamp object and puts it at the default
	location (0, 0, 0) in the current scene::
		import Blender

		object = Blender.Object.New('Lamp')
		lamp = Blender.Lamp.New('Spot')
		object.link(lamp)
		sce = Blender.Scene.GetCurrent()
		sce.link(object)

		Blender.Redraw()
	@Note: if an object is created but is not linked to object data, and the
	object is not linked to a scene, it will be deleted when the Python
	object is deallocated.  This is because Blender does not allow objects
	to exist without object data unless they are Empty objects.  Scene.link()
	will automatically create object data for an object if it has none.
	"""

def Get (name = None):
	"""
	Get the Object from Blender.
	@type name: string
	@param name: The name of the requested Object.
	@return: It depends on the 'name' parameter:
			- (name): The Object with the given name;
			- ():     A list with all Objects in the current scene.

	I{B{Example 1:}}

	The example below works on the default scene. The script returns the plane object and prints the location of the plane::
		import Blender

		object = Blender.Object.Get ('plane')
		print object.getLocation()

	I{B{Example 2:}}

	The example below works on the default scene. The script returns all objects
	in the scene and prints the list of object names::
		import Blender

		objects = Blender.Object.Get ()
		print objects
	@note: Get will return objects from all scenes.
			Most user tools should only operate on objects from the current scene - Blender.Scene.GetCurrent().getChildren()
	"""

def GetSelected ():
	"""
	Get the user selection. If no objects are selected, an empty list will be returned.
	
	@return: A list of all selected Objects in the current scene.

	I{B{Example:}}

	The example below works on the default scene. Select one or more objects and
	the script will print the selected objects::
		import Blender

		objects = Blender.Object.GetSelected()
		print objects
	@note: The active object will always be the first object in the list (if selected).
	@note: The user selection is made up of selected objects from Blender's current scene only.
	@note: The user selection is limited to objects on visible layers;
			if the user's last active 3d view is in localview then the selection will be limited to the objects in that localview.
	"""


def Duplicate (mesh=0, surface=0, curve=0, text=0, metaball=0, armature=0, lamp=0, material=0, texture=0, ipo=0):
	"""
	Duplicate selected objects on visible layers from Blenders current scene,
	de-selecting the currently visible, selected objects and making a copy where all new objects are selected.
	By default no data linked to the object is duplicated; use the keyword arguments to change this.
	L{Object.GetSelected()<GetSelected>} will return the list of objects resulting from duplication.
	
	B{Note}: This command will raise an error if used from the command line (background mode) because it uses the 3D view context.
	
	@type mesh: bool
	@param mesh: When non-zero, mesh object data will be duplicated with the objects.
	@type surface: bool
	@param surface: When non-zero, surface object data will be duplicated with the objects.
	@type curve: bool
	@param curve: When non-zero, curve object data will be duplicated with the objects.
	@type text: bool
	@param text: When non-zero, text object data will be duplicated with the objects.
	@type metaball: bool
	@param metaball: When non-zero, metaball object data will be duplicated with the objects.
	@type armature: bool
	@param armature: When non-zero, armature object data will be duplicated with the objects.
	@type lamp: bool
	@param lamp: When non-zero, lamp object data will be duplicated with the objects.
	@type material: bool
	@param material: When non-zero, materials used by the object or its object data will be duplicated with the objects.
	@type texture: bool
	@param texture: When non-zero, texture data used by the object's materials will be duplicated with the objects.
	@type ipo: bool
	@param ipo: When non-zero, Ipo data linked to the object will be duplicated with the objects.

	I{B{Example:}}

	The example below creates duplicates the active object 10 times
	and moves each object 1.0 on the X axis::
		import Blender

		scn = Scene.GetCurrent()
		ob_act = scn.objects.active
		
		# Unselect all
		scn.objects.selected = []
		ob_act.sel = 1
		
		for x in xrange(10):
				Blender.Object.Duplicate() # Duplicate linked
				ob_act = scn.objects.active
				ob_act.LocX += 1
		Blender.Redraw()
	"""

from IDProp import IDGroup, IDArray
class Object:
	"""
	The Object object
	=================
		This object gives access to generic data from all objects in Blender. 

	B{Note}:
	When dealing with properties and functions such as LocX/RotY/getLocation(), getSize() and getEuler(),
	keep in mind that these transformation properties are relative to the object's parent (if any).

	To get these values in worldspace (taking into account vertex parents, constraints, etc.)
	pass the argument 'worldspace' to these functions.

	@ivar restrictDisplay: Don't display this object in the 3D view: disabled by default, use the outliner to toggle.
	@type restrictDisplay: bool
	@ivar restrictSelect: Don't select this object in the 3D view: disabled by default, use the outliner to toggle.
	@type restrictSelect: bool
	@ivar restrictRender: Don't render this object: disabled by default, use the outliner to toggle.
	@type restrictRender: bool
	@ivar LocX: The X location coordinate of the object.
	@type LocX: float
	@ivar LocY: The Y location coordinate of the object.
	@type LocY: float
	@ivar LocZ: The Z location coordinate of the object.
	@type LocZ: float
	@ivar loc: The (X,Y,Z) location coordinates of the object.
	@type loc: tuple of 3 floats
	@ivar dLocX: The delta X location coordinate of the object.
		This variable applies to IPO Objects only.
	@type dLocX: float
	@ivar dLocY: The delta Y location coordinate of the object.
		This variable applies to IPO Objects only.
	@type dLocY: float
	@ivar dLocZ: The delta Z location coordinate of the object.
		This variable applies to IPO Objects only.
	@type dLocZ: float
	@ivar dloc: The delta (X,Y,Z) location coordinates of the object (vector).
		This variable applies to IPO Objects only.
	@type dloc: tuple of 3 floats
	@ivar RotX: The X rotation angle (in radians) of the object.
	@type RotX: float
	@ivar RotY: The Y rotation angle (in radians) of the object.
	@type RotY: float
	@ivar RotZ: The Z rotation angle (in radians) of the object.
	@type RotZ: float
	@ivar rot: The (X,Y,Z) rotation angles (in radians) of the object.
	@type rot: euler (Py_WRAPPED)
	@ivar dRotX: The delta X rotation angle (in radians) of the object.
		This variable applies to IPO Objects only.
	@type dRotX: float
	@ivar dRotY: The delta Y rotation angle (in radians) of the object.
		This variable applies to IPO Objects only.
	@type dRotY: float
	@ivar dRotZ: The delta Z rotation angle (in radians) of the object.
		This variable applies to IPO Objects only.
	@type dRotZ: float
	@ivar drot: The delta (X,Y,Z) rotation angles (in radians) of the object.
		This variable applies to IPO Objects only.
	@type drot: tuple of 3 floats
	@ivar SizeX: The X size of the object.
	@type SizeX: float
	@ivar SizeY: The Y size of the object.
	@type SizeY: float
	@ivar SizeZ: The Z size of the object.
	@type SizeZ: float
	@ivar size: The (X,Y,Z) size of the object.
	@type size: tuple of 3 floats
	@ivar dSizeX: The delta X size of the object.
	@type dSizeX: float
	@ivar dSizeY: The delta Y size of the object.
	@type dSizeY: float
	@ivar dSizeZ: The delta Z size of the object.
	@type dSizeZ: float
	@ivar dsize: The delta (X,Y,Z) size of the object.
	@type dsize: tuple of 3 floats
	@ivar Layers: The object layers (also check the newer attribute
		L{layers<layers>}).  This value is a bitmask with at 
		least one position set for the 20 possible layers starting from the low
		order bit.  The easiest way to deal with these values in in hexadecimal
		notation.
		Example::
			ob.Layer = 0x04 # sets layer 3 ( bit pattern 0100 )
		After setting the Layer value, call Blender.Redraw( -1 ) to update
		the interface.
	@type Layers: integer (bitmask)
	@type layers: list of integers
	@ivar layers: The layers this object is visible in (also check the older
		attribute L{Layers<Layers>}).  This returns a list of
		integers in the range [1, 20], each number representing the respective
		layer.  Setting is done by passing a list of ints or an empty list for
		no layers.
		Example::
			ob.layers = []  # object won't be visible
			ob.layers = [1, 4] # object visible only in layers 1 and 4
			ls = o.layers
			ls.append([10])
			o.layers = ls
			print ob.layers # will print: [1, 4, 10]
		B{Note}: changes will only be visible after the screen (at least
		the 3d View and Buttons windows) is redrawn.
	@ivar parent: The parent object of the object (if defined). Read-only.
	@type parent: Object or None
	@ivar data: The Datablock object linked to this object.  Read-only.
	@type data: varies
	@ivar ipo: Contains the Ipo if one is assigned to the object, B{None}
		otherwise.  Setting to B{None} clears the current Ipo.
	@type ipo:  Ipo
	@ivar mat: The matrix of the object in world space (absolute, takes vertex parents, tracking
		and Ipos into account).  Read-only.
	@type mat: Matrix
	@ivar matrix: Same as L{mat}.  Read-only.
	@type matrix: Matrix
	@ivar matrixLocal: The matrix of the object relative to its parent; if there is no parent,
	returns the world matrix (L{matrixWorld<Object.Object.matrixWorld>}).
	@type matrixLocal: Matrix
	@ivar matrixParentInverse: The inverse if the parents local matrix, set when the objects parent is set (wrapped).
	@type matrixParentInverse: Matrix
	@ivar matrixOldWorld: Old-type worldspace matrix (prior to Blender 2.34).
		Read-only.
	@type matrixOldWorld: Matrix
	@ivar matrixWorld: Same as L{mat}. Read-only.
	@type matrixWorld: Matrix
	@ivar colbits: The Material usage mask. A set bit #n means: the Material
		#n in the Object's material list is used. Otherwise, the Material #n
		of the Objects Data material list is displayed.
		Example::
			object.colbits = (1<<0) + (1<<5) # use mesh materials 0 (1<<0) and 5 (1<<5)
			# use object materials for all others
	@ivar sel: The selection state of the object in the current scene. 
		True is selected, False is unselected. Setting makes the object active.
	@type sel: boolean
	@ivar effects: The list of particle effects associated with the object. (depricated, will always return an empty list)
		Read-only.
	@type effects: list of Effect objects
	@ivar parentbonename: The string name of the parent bone (if defined).
		This can be set to another bone in the armature if the object already has a bone parent.
	@type parentbonename: string or None
	@ivar parentVertexIndex: A list of vertex parent indicies, with a length of 0, 1 or 3. When there are 1 or 3 vertex parents, the indicies can be assigned to a sequence of the same length.
	@type parentVertexIndex: list
	@ivar protectFlags: The "transform locking" bitfield flags for the object.  
	See L{ProtectFlags} const dict for values.
	@type protectFlags: int
	@ivar DupGroup: The DupliGroup Animation Property. Assign a group to
		DupGroup to make this object an instance of that group.
		This does not enable or disable the DupliGroup option, for that use
		L{enableDupGroup}.
		The attribute returns None when this object does not have a dupliGroup, 
		and setting the attrbute to None deletes the object from the group.
	@type DupGroup: Group or None
	@ivar DupObjects: The Dupli object instances.  Read-only.
		Returns of list of tuples for object duplicated
		by dupliframe, dupliverts dupligroups and other animation properties.
		The first tuple item is the original object that is duplicated,
		the second is the 4x4 worldspace dupli-matrix.
		Example::
			import Blender
			from Blender import Object, Scene, Mathutils

			ob= Object.Get('Cube')
			dupe_obs= ob.DupObjects
			scn= Scene.GetCurrent()
			for dupe_ob, dupe_matrix in dupe_obs:
				print dupe_ob.name
				empty_ob = scn.objects.new('Empty')
				empty_ob.setMatrix(dupe_matrix)
			Blender.Redraw()
	@type DupObjects: list of tuples containing (object, matrix)
	@ivar enableNLAOverride: Whether the object uses NLA or active Action for animation. When True the NLA is used.
	@type enableNLAOverride: boolean
	@ivar enableDupVerts: The DupliVerts status of the object.
		Does not indicate that this object has any dupliVerts,
		(as returned by L{DupObjects}) just that dupliVerts are enabled.
	@type enableDupVerts: boolean
	@ivar enableDupFaces: The DupliFaces status of the object.
		Does not indicate that this object has any dupliFaces,
		(as returned by L{DupObjects}) just that dupliFaces are enabled.
	@type enableDupFaces: boolean
	@ivar enableDupFacesScale: The DupliFacesScale status of the object.
	@type enableDupFacesScale: boolean
	@ivar dupFacesScaleFac: Scale factor for dupliface instance, 1.0 by default.
	@type dupFacesScaleFac: float	
	@ivar enableDupFrames: The DupliFrames status of the object.
		Does not indicate that this object has any dupliFrames,
		(as returned by L{DupObjects}) just that dupliFrames are enabled.
	@type enableDupFrames: boolean
	@ivar enableDupGroup: The DupliGroup status of the object.
		Set True to make this object an instance of the object's L{DupGroup},
		and set L{DupGroup} to a group for this to take effect,
		Use L{DupObjects} to get the object data from this instance.
	@type enableDupGroup: boolean
	@ivar enableDupRot: The DupliRot status of the object.
		Use with L{enableDupVerts} to rotate each instance
		by the vertex normal.
	@type enableDupRot: boolean
	@ivar enableDupNoSpeed: The DupliNoSpeed status of the object.
		Use with L{enableDupFrames} to ignore dupliFrame speed. 
	@type enableDupNoSpeed: boolean
	@ivar DupSta: The DupliFrame starting frame. Use with L{enableDupFrames}.
		Value clamped to [1,32767].
	@type DupSta: int
	@ivar DupEnd: The DupliFrame end frame. Use with L{enableDupFrames}.
		Value clamped to [1,32767].
	@type DupEnd: int
	@ivar DupOn: The DupliFrames in succession between DupOff frames.
		Value is clamped to [1,1500].
		Use with L{enableDupFrames} and L{DupOff} > 0.
	@type DupOn: int
	@ivar DupOff: The DupliFrame removal of every Nth frame for this object.
		Use with L{enableDupFrames}.  Value is clamped to [0,1500].
	@type DupOff: int
	@ivar passIndex: Index # for the IndexOB render pass.
		Value is clamped to [0,1000].
	@type passIndex: int
	@ivar activeMaterial: The active material index for this object.

		The active index is used to select the material to edit in the material buttons,
		new data created will also use the active material.

		Value is clamped to [1,len(ob.materials)]. - [0,0] when there is no materials applied to the object.
	@type activeMaterial: int
	@ivar activeShape: The active shape key index for this object.

		The active index is used to select the material to edit in the material buttons,
		new data created will also use the active material.

		Value is clamped to [1,len(ob.data.key.blocks)]. - [0,0] when there are no keys.

	@type activeShape: int
	
	@ivar pinShape: If True, only the activeShape will be displayed.
	@type pinShape: bool
	@ivar drawSize: The size to display the Empty.
	Value clamped to [0.01,10.0].
	@type drawSize: float
	@ivar modifiers: The modifiers associated with the object.
		Example::
			# copy the active objects modifiers to all other visible selected objects
			from Blender import *
			scn = Scene.GetCurrent()
			ob_act = scn.objects.active
			for ob in scn.objects.context:
				# Cannot copy modifiers to an object of a different type
				if ob.type == ob_act.type:
					ob.modifiers = ob_act.modifiers
	@type modifiers: L{Modifier Sequence<Modifier.ModSeq>}
	@ivar constraints: a L{sequence<Constraint.Constraints>} of
		L{constraints<Constraint.Constraint>} for the object. Read-only.
	@type constraints: Constraint Sequence
	@ivar actionStrips: a L{sequence<NLA.ActionStrips>} of
		L{action strips<NLA.ActionStrip>} for the object.  Read-only.
	@type actionStrips: BPy_ActionStrips
	@ivar action: The action associated with this object (if defined).
	@type action: L{Action<NLA.Action>} or None
	@ivar oopsLoc: Object's (X,Y) OOPs location.  Returns None if object
		is not found in list.
	@type oopsLoc:  tuple of 2 floats
	@ivar oopsSel: Object OOPs selection flag.
	@type oopsSel: boolean
	@ivar game_properties: The object's properties.  Read-only.
	@type game_properties: list of Properties.
	@ivar timeOffset: The time offset of the object's animation.
		Value clamped to [-300000.0,300000.0].
	@type timeOffset: float
	@ivar track: The object's tracked object.  B{None} is returned if no
		object is tracked.  Also, assigning B{None} clear the tracked object. 
	@type track: Object or None
	@ivar type: The object's type.  Read-only.
	@type type: string
	@ivar boundingBox: The bounding box of this object.  Read-only.
	@type boundingBox: list of 8 3D vectors
	@ivar drawType: The object's drawing type.
		See L{DrawTypes} constant dict for values.
	@type drawType: int
	@ivar parentType: The object's parent type.  Read-only.
		See L{ParentTypes} constant dict for values.
	@type parentType: int
	@ivar axis: Enable display of active object's center and axis.
		Also see B{AXIS} bit in L{drawMode} attribute.
	@type axis: boolean
	@ivar texSpace: Enable display of active object's texture space.
		Also see B{TEXSPACE} bit in L{drawMode} attribute.
	@type texSpace: boolean
	@ivar nameMode: Enable display of active object's name.
		Also see B{NAME} bit in L{drawMode} attribute.
	@type nameMode: boolean
	@ivar wireMode: Enable the active object's wireframe over solid drawing.
		Also see B{WIRE} bit in L{drawMode} attribute.
	@type wireMode: boolean
	@ivar xRay: Enable drawing the active object in front of others.
		Also see B{XRAY} bit in L{drawMode} attribute.
	@type xRay: boolean
	@ivar transp: Enable transparent materials for the active object
		(mesh only).  Also see B{TRANSP} bit in L{drawMode} attribute.
	@type transp: boolean
	@ivar drawMode: The object's drawing mode bitfield.
		See L{DrawModes} constant dict for values.
	@type drawMode: int

	@ivar piType: Type of particle interaction. 
		See L{PITypes} constant dict for values.
	@type piType: int
	@ivar piFalloff: The particle interaction falloff power.
		Value clamped to [0.0,10.0].
	@type piFalloff: float
	@ivar piMaxDist: Max distance for the particle interaction field to work.
		Value clamped to [0.0,1000.0].
	@type piMaxDist: float
	@ivar piPermeability: Probability that a particle will pass through the
		mesh.  Value clamped to [0.0,1.0].
	@type piPermeability: float
	@ivar piRandomDamp: Random variation of particle interaction damping.
		Value clamped to [0.0,1.0].
	@type piRandomDamp: float
	@ivar piSoftbodyDamp: Damping factor for softbody deflection.
		Value clamped to [0.0,1.0].
	@type piSoftbodyDamp: float
	@ivar piSoftbodyIThick: Inner face thickness for softbody deflection.
		Value clamped to [0.001,1.0].
	@type piSoftbodyIThick: float
	@ivar piSoftbodyOThick: Outer face thickness for softbody deflection.
		Value clamped to [0.001,1.0].
	@type piSoftbodyOThick: float
	@ivar piStrength: Particle interaction force field strength.
		Value clamped to [0.0,1000.0].
	@type piStrength: float
	@ivar piSurfaceDamp: Amount of damping during particle collision.
		Value clamped to [0.0,1.0].
	@type piSurfaceDamp: float
	@ivar piUseMaxDist: Use a maximum distance for the field to work.
	@type piUseMaxDist: boolean

	@ivar isSoftBody: True if object is a soft body.  Read-only.
	@type isSoftBody: boolean
	@ivar SBDefaultGoal: Default softbody goal value, when no vertex group used.
		Value clamped to [0.0,1.0].
	@type SBDefaultGoal: float
	@ivar SBErrorLimit: Softbody Runge-Kutta ODE solver error limit (low values give more precision).
		Value clamped to [0.01,1.0].
	@type SBErrorLimit: float
	@ivar SBFriction: General media friction for softbody point movements.
		Value clamped to [0.0,10.0].
	@type SBFriction: float
	@ivar SBGoalFriction: Softbody goal (vertex target position) friction.
		Value clamped to [0.0,10.0].
	@type SBGoalFriction: float
	@ivar SBGoalSpring: Softbody goal (vertex target position) spring stiffness.
		Value clamped to [0.0,0.999].
	@type SBGoalSpring: float
	@ivar SBGrav: Apply gravitation to softbody point movement.
		Value clamped to [0.0,10.0].
	@type SBGrav: float
	@ivar SBInnerSpring: Softbody edge spring stiffness.
		Value clamped to [0.0,0.999].
	@type SBInnerSpring: float
	@ivar SBInnerSpringFrict: Softbody edge spring friction.
		Value clamped to [0.0,10.0].
	@type SBInnerSpringFrict: float
	@ivar SBMass: Softbody point mass (heavier is slower).
		Value clamped to [0.001,50.0].
	@type SBMass: float
	@ivar SBMaxGoal: Softbody goal maximum (vertex group weights scaled to
		match this range).  Value clamped to [0.0,1.0].
	@type SBMaxGoal: float
	@ivar SBMinGoal: Softbody goal minimum (vertex group weights scaled to
		match this range).  Value clamped to [0.0,1.0].
	@type SBMinGoal: float
	@ivar SBSpeed: Tweak timing for physics to control softbody frequency and
		speed.  Value clamped to [0.0,10.0].
	@type SBSpeed: float
	@ivar SBStiffQuads: Softbody adds diagonal springs on 4-gons enabled.
	@type SBStiffQuads: boolean
	@ivar SBUseEdges: Softbody use edges as springs enabled.
	@type SBUseEdges: boolean
	@ivar SBUseGoal: Softbody forces for vertices to stick to animated position enabled.
	@type SBUseGoal: boolean

	@ivar rbFlags: Rigid body bitfield.  See L{RBFlags} for valid values.
	@type rbFlags: int
	@ivar rbMass: Rigid body mass.  Must be a positive value.
	@type rbMass: float
	@ivar rbRadius: Rigid body bounding sphere size.  Must be a positive
		value.
	@type rbRadius: float
	@ivar rbShapeBoundType: Rigid body shape bound type.  See L{RBShapes}
		const dict for values.
	@type rbShapeBoundType: int
	@ivar trackAxis: Track axis. Return string 'X' | 'Y' | 'Z' | '-X' | '-Y' | '-Z' (readonly)
	@type trackAxis: string 
	@ivar upAxis: Up axis. Return string 'Y' | 'Y' | 'Z' (readonly)
	@type upAxis: string
	"""
	def getParticleSystems():
		"""
		Return a list of particle systems linked to this object (see Blender.Particle).
		"""
		
	def newParticleSystem():
		"""
		Link a new particle system (see Blender.Particle).
		"""
		
	def addVertexGroupsFromArmature(object):
		"""
		Add vertex groups from armature using the bone heat method
		This method can be only used with an Object of the type Mesh when NOT in edit mode.
		@type object: a bpy armature
		"""

	def buildParts():
		"""
		Recomputes the particle system. This method only applies to an Object of
		the type Effect. (depricated, does nothing now, use makeDisplayList instead to update the modifier stack)
		"""

	def insertShapeKey():
		"""
		Insert a Shape Key in the current object.  It applies to Objects of
		the type Mesh, Lattice, or Curve.
		"""

	def getPose():
		"""
		Gets the current Pose of the object.
		@rtype: Pose object
		@return: the current pose object
		"""

	def evaluatePose(framenumber):
		"""
		Evaluates the Pose based on its currently bound action at a certain frame.
		@type framenumber: Int
		@param framenumber: The frame number to evaluate to.
		"""

	def clearIpo():
		"""
		Unlinks the ipo from this object.
		@return: True if there was an ipo linked or False otherwise.
		"""

	def clrParent(mode = 0, fast = 0):
		"""
		Clears parent object.
		@type mode: Integer
		@type fast: Integer
		@param mode: A mode flag. If mode flag is 2, then the object transform will
			be kept. Any other value, or no value at all will update the object
			transform.
		@param fast: If the value is 0, the scene hierarchy will not be updated. Any
			other value, or no value at all will update the scene hierarchy.
		"""

	def getData(name_only=False, mesh=False):
		"""
		Returns the Datablock object (Mesh, Lamp, Camera, etc.) linked to this 
		Object.  If the keyword parameter B{name_only} is True, only the Datablock
		name is returned as a string.  It the object is of type Mesh, then the
		B{mesh} keyword can also be used; the data return is a Mesh object if
		True, otherwise it is an NMesh object (the default).
		The B{mesh} keyword is ignored for non-mesh objects.
		@type name_only: bool
		@param name_only: This is a keyword parameter.  If True (or nonzero),
		only the name of the data object is returned. 
		@type mesh: bool
		@param mesh: This is a keyword parameter.  If True (or nonzero), 
		a Mesh data object is returned.
		@rtype: specific Object type or string
		@return: Depends on the type of Datablock linked to the Object.  If
		B{name_only} is True, it returns a string.
		@note: Mesh is faster than NMesh because Mesh is a thin wrapper.
		@note: This function is different from L{NMesh.GetRaw} and L{Mesh.Get}
		because it keeps a link to the original mesh, which is needed if you are
		dealing with Mesh weight groups.
		@note: Make sure the object you are getting the data from isn't in
		EditMode before calling this function; otherwise you'll get the data
		before entering EditMode. See L{Window.EditMode}.
		"""

	def getParentBoneName():
		"""
		Returns None, or the 'sub-name' of the parent (eg. Bone name)
		@return: string
		"""

	def getDeltaLocation():
		"""
		Returns the object's delta location in a list (x, y, z)
		@rtype: A vector triple
		@return: (x, y, z)
		"""

	def getDrawMode():
		"""
		Returns the object draw mode.
		@rtype: Integer
		@return: a sum of the following:
				- 2  - axis
				- 4  - texspace
				- 8  - drawname
				- 16 - drawimage
				- 32 - drawwire
				- 64 - xray
		"""

	def getDrawType():
		"""
		Returns the object draw type
		@rtype: Integer
		@return: One of the following:
				- 1 - Bounding box
				- 2 - Wire
				- 3 - Solid
				- 4 - Shaded
				- 5 - Textured
		"""

	def getEuler(space):
		"""
		@type space: string
		@param space: The desired space for the size:
			- localspace: (default) relative to the object's parent;
			- worldspace: absolute, taking vertex parents, tracking and
					Ipo's into account;
		Returns the object's localspace rotation as Euler rotation vector (rotX, rotY, rotZ).  Angles are in radians.
		@rtype: Py_Euler
		@return: A python Euler. Data is wrapped when euler is present.
		"""

	def getInverseMatrix():
		"""
		Returns the object's inverse matrix.
		@rtype: Py_Matrix
		@return: A python matrix 4x4
		"""

	def getIpo():
		"""
		Returns the Ipo associated to this object or None if there's no linked ipo.
		@rtype: Ipo
		@return: the wrapped ipo or None.
		"""
	def isSelected():
		"""
		Returns the objects selection state in the current scene as a boolean value True or False.
		@rtype: Boolean
		@return: Selection state as True or False
		"""
	
	def getLocation(space):
		"""
		@type space: string
		@param space: The desired space for the location:
			- localspace: (default) relative to the object's parent;
			- worldspace: absolute, taking vertex parents, tracking and
				Ipo's into account;
		Returns the object's location (x, y, z).
		@return: (x, y, z)

		I{B{Example:}}

		The example below works on the default scene. It retrieves all objects in
		the scene and prints the name and location of each object::
			import Blender

			sce = Blender.Scene.GetCurrent()

			for ob in sce.objects:
				print obj.name
				print obj.loc
		@note: the worldspace location is the same as ob.matrixWorld[3][0:3]
		"""

	def getAction():
		"""
		Returns an action if one is associated with this object (only useful for armature types).
		@rtype: Py_Action
		@return: a python action.
		"""

	def getMaterials(what = 0):
		"""
		Returns a list of materials assigned to the object.
		@type what: int
		@param what: if nonzero, empty slots will be returned as None's instead
				of being ignored (default way). See L{NMesh.NMesh.getMaterials}.
		@rtype: list of Material Objects
		@return: list of Material Objects assigned to the object.
		"""

	def getMatrix(space = 'worldspace'):
		"""
		Returns the object matrix.
		@type space: string
		@param space: The desired matrix:
			- worldspace (default): absolute, taking vertex parents, tracking and
				Ipo's into account;
			- localspace: relative to the object's parent (returns worldspace 
				matrix if the object doesn't have a parent);
			- old_worldspace: old behavior, prior to Blender 2.34, where eventual 
				changes made by the script itself were not taken into account until 
				a redraw happened, either called by the script or upon its exit.
		Returns the object matrix.
		@rtype: Py_Matrix (WRAPPED DATA)
		@return: a python 4x4 matrix object. Data is wrapped for 'worldspace'
		"""

	def getName():
		"""
		Returns the name of the object
		@return: The name of the object

		I{B{Example:}}

		The example below works on the default scene. It retrieves all objects in
		the scene and prints the name of each object::
			import Blender

			sce= Blender.Scene.GetCurrent()

			for ob in sce.objects:
				print ob.getName()
		"""

	def getParent():
		"""
		Returns the object's parent object.
		@rtype: Object
		@return: The parent object of the object. If not available, None will be
		returned.
		"""

	def getSize(space):
		"""
		@type space: string
		@param space: The desired space for the size:
			- localspace: (default) relative to the object's parent;
			- worldspace: absolute, taking vertex parents, tracking and
				Ipo's into account;
		Returns the object's size.
		@return: (SizeX, SizeY, SizeZ)
		@note: the worldspace size will not return negative (flipped) scale values.
		"""

	def getParentBoneName():
		"""
		Returns the object's parent object's sub name, or None.
		For objects parented to bones, this is the name of the bone.
		@rtype: String
		@return: The parent object sub-name of the object.
			If not available, None will be returned.
		"""

	def getTimeOffset():
		"""
		Returns the time offset of the object's animation.
		@return: TimeOffset
		"""

	def getTracked():
		"""
		Returns the object's tracked object.
		@rtype: Object
		@return: The tracked object of the object. If not available, None will be
		returned.
		"""

	def getType():
		"""
		Returns the type of the object in 'Armature', 'Camera', 'Curve', 'Lamp', 'Lattice',
		'Mball', 'Mesh', 'Surf', 'Empty', 'Wave' (deprecated) or 'unknown' in exceptional cases.

		I{B{Example:}}

		The example below works on the default scene. It retrieves all objects in
		the scene and updates the location and rotation of the camera. When run,
		the camera will rotate 180 degrees and moved to the opposite side of the X
		axis. Note that the number 'pi' in the example is an approximation of the
		true number 'pi'.  A better, less error-prone value of pi is math.pi from the python math module.::
			import Blender

			sce = Blender.Scene.GetCurrent()

			for obj in sce.objects:
				if obj.type == 'Camera':
					obj.LocY = -obj.LocY
					obj.RotZ = 3.141592 - obj.RotZ

			Blender.Redraw()
		
		@return: The type of object.
		@rtype: String
		"""

	def insertIpoKey(keytype):
		"""
		Inserts keytype values in object ipo at curframe.
		@type keytype: int
		@param keytype: A constant from L{IpoKeyTypes<Object.IpoKeyTypes>}
		@return: None
		"""

	def link(datablock):
		"""
		Links Object with ObData datablock provided in the argument. The data must match the
		Object's type, so you cannot link a Lamp to a Mesh type object.
		@type datablock: Blender ObData datablock
		@param datablock: A Blender datablock matching the objects type.
		"""

	def makeParent(objects, noninverse = 0, fast = 0):
		"""
		Makes the object the parent of the objects provided in the argument which
		must be a list of valid Objects.
		@type objects: Sequence of Blender Object
		@param objects: The children of the parent
		@type noninverse: Integer
		@param noninverse:
				0 - make parent with inverse
				1 - make parent without inverse
		@type fast: Integer
		@param fast:
				0 - update scene hierarchy automatically
				1 - don't update scene hierarchy (faster). In this case, you must
				explicitely update the Scene hierarchy.
		@warn: objects must first be linked to a scene before they can become
			parents of other objects.  Calling this makeParent method for an
			unlinked object will result in an error.
		"""

	def join(objects):
		"""
		Uses the object as a base for all of the objects in the provided list to join into.
		
		@type objects: Sequence of Blender Object
		@param objects: A list of objects matching the object's type.
		@note: Objects in the list will not be removed from the scene.
			To avoid overlapping data you may want to remove them manually after joining.
		@note: Join modifies the base object's data in place so that
			other objects are joined into it. No new object or data is created.
		@note: Join will only work for object types Mesh, Armature, Curve and Surface;
			an excption will be raised if the object is not of these types.
		@note: Objects in the list will be ignored if they to not match the base object.
		@note: The base object must be in the current scene to be joined.
		@note: This function will not work in background mode (no user interface).
		@note: An error in the function input will raise a TypeError or AttributeError,
			otherwise an error in the data input will raise a RuntimeError.
			For situations where you don't have tight control on the data that is being joined,
			you should handle the RuntimeError error, letting the user know the data can't be joined.
		"""

	def makeParentDeform(objects, noninverse = 0, fast = 0):
		"""
		Makes the object the deformation parent of the objects provided in the argument
		which must be a list of valid Objects.
		The parent object must be a Curve or Armature.
		@type objects: Sequence of Blender Object
		@param objects: The children of the parent
		@type noninverse: Integer
		@param noninverse:
				0 - make parent with inverse
				1 - make parent without inverse
		@type fast: Integer
		@param fast:
				0 - update scene hierarchy automatically
				1 - don't update scene hierarchy (faster). In this case, you must
				explicitely update the Scene hierarchy.
		@warn: objects must first be linked to a scene before they can become
				parents of other objects.  Calling this makeParent method for an
				unlinked object will result in an error.
		@warn: child objects must be of mesh type to deform correctly. Other object
				types will fall back to normal parenting silently.
		"""

	def makeParentVertex(objects, indices, noninverse = 0, fast = 0):
		"""
		Makes the object the vertex parent of the objects provided in the argument
		which must be a list of valid Objects.
		The parent object must be a Mesh, Curve or Surface.
		@type objects: Sequence of Blender Object
		@param objects: The children of the parent
		@type indices: Tuple of Integers
		@param indices: The indices of the vertices you want to parent to (1 or 3 values)
		@type noninverse: Integer
		@param noninverse:
				0 - make parent with inverse
				1 - make parent without inverse
		@type fast: Integer
		@param fast:
				0 - update scene hierarchy automatically
				1 - don't update scene hierarchy (faster). In this case, you must
				explicitely update the Scene hierarchy.
		@warn: objects must first be linked to a scene before they can become
				parents of other objects.  Calling this makeParent method for an
				unlinked object will result in an error.
		"""
	def makeParentBone(objects, bonename, noninverse = 0, fast = 0):
		"""
		Makes one of the object's bones the parent of the objects provided in the argument
		which must be a list of valid objects.  The parent object must be an Armature.
		@type objects: Sequence of Blender Object
		@param objects: The children of the parent
		@type bonename: string
		@param bonename: a valid bone name from the armature
		@type noninverse: integer
		@param noninverse:
				0 - make parent with inverse
				1 - make parent without inverse
		@type fast: integer
		@param fast:
				0 - update scene hierarchy automatically
				1 - don't update scene hierarchy (faster). In this case, you must
				explicitly update the Scene hierarchy.
		@warn: Objects must first be linked to a scene before they can become
			parents of other objects.  Calling this method for an
			unlinked object will result in an exception.
		"""

	def setDeltaLocation(delta_location):
		"""
		Sets the object's delta location which must be a vector triple.
		@type delta_location: A vector triple
		@param delta_location: A vector triple (x, y, z) specifying the new
		location.
		"""

	def setDrawMode(drawmode):
		"""
		Sets the object's drawing mode. The drawing mode can be a mix of modes. To
		enable these, add up the values.
		@type drawmode: Integer
		@param drawmode: A sum of the following:
				- 2  - axis
				- 4  - texspace
				- 8  - drawname
				- 16 - drawimage
				- 32 - drawwire
				- 64 - xray
		"""

	def setDrawType(drawtype):
		"""
		Sets the object's drawing type.
		@type drawtype: Integer
		@param drawtype: One of the following:
				- 1 - Bounding box
				- 2 - Wire
				- 3 - Solid
				- 4 - Shaded
				- 5 - Textured
		"""

	def setEuler(euler):
		"""
		Sets the object's localspace rotation according to the specified Euler angles.
		@type euler: Py_Euler or a list of floats
		@param euler: a python Euler or x,y,z rotations as floats
		"""

	def setIpo(ipo):
		"""
		Links an ipo to this object.
		@type ipo: Blender Ipo
		@param ipo: an object type ipo.
		"""

	def setLocation(x, y, z):
		"""
		Sets the object's location relative to the parent object (if any).
		@type x: float
		@param x: The X coordinate of the new location.
		@type y: float
		@param y: The Y coordinate of the new location.
		@type z: float
		@param z: The Z coordinate of the new location.
		"""

	def setMaterials(materials):
		"""
		Sets the materials. The argument must be a list 16 items or less.  Each
		list element is either a Material or None.  Also see L{colbits}.
		@type materials: Materials list
		@param materials: A list of Blender material objects.
		@note: Materials are assigned to the object's data by default.  Unless
		you know the material is applied to the object or are changing the
		object's L{colbits}, you need to look at the object data's materials.
		"""

	def setMatrix(matrix):
		"""
		Sets the object's matrix and updates its transformation.  If the object
		has a parent, the matrix transform is relative to the parent.
		@type matrix: Py_Matrix 3x3 or 4x4
		@param matrix: a 3x3 or 4x4 Python matrix.  If a 3x3 matrix is given,
		it is extended to a 4x4 matrix.
		@Note: This method is "bad": when called it changes the location,
		rotation and size attributes of the object (since Blender uses these
		values to calculate the object's transformation matrix).  Ton is
		not happy having a method which "pretends" to do a matrix operation.
		In the future, this method may be replaced with other methods which
		make it easier for the user to determine the correct loc/rot/size values
		for necessary for the object.
		"""

	def setName(name):
		"""
		Sets the name of the object. A string longer than 20 characters will be shortened.
		@type name: String
		@param name: The new name for the object.
		"""

	def setSize(x, y, z):
		"""
		Sets the object's size, relative to the parent object (if any), clamped 
		@type x: float
		@param x: The X size multiplier.
		@type y: float
		@param y: The Y size multiplier.
		@type z: float
		@param z: The Z size multiplier.
		"""

	def setTimeOffset(timeOffset):
		"""
		Sets the time offset of the object's animation.
		@type timeOffset: float
		@param timeOffset: The new time offset for the object's animation.
		"""
	
	def shareFrom(object):
		"""
		Link data of a specified argument with this object. This works only
		if both objects are of the same type.
		@type object: Blender Object
		@param object: A Blender Object of the same type.
		@note: This function is faster than using L{getData()} and setData()
		because it skips making a Python object from the object's data.
		"""
	
	def select(boolean):
		"""
		Sets the object's selection state in the current scene.
		setting the selection will make this object the active object of this scene.
		@type boolean: Integer
		@param boolean:
				- 0  - unselected
				- 1  - selected
		"""
	
	def getBoundBox(worldspace=1):
		"""
		Returns the worldspace bounding box of this object.  This works for meshes (out of
		edit mode) and curves.
		@type worldspace: int
		@param worldspace: An optional argument. When zero, the bounding values will be localspace.
		@rtype: list of 8 (x,y,z) float coordinate vectors (WRAPPED DATA)
		@return: The coordinates of the 8 corners of the bounding box. Data is wrapped when
		bounding box is present.
		"""

	def makeDisplayList():
		"""
		Forces an update to the objects display data. If the object isn't modified,
		there's no need to recalculate this data.
		This method is here for the *few cases* where it is needed.

		Example::
			import Blender

			scn = Blender.Scene.GetCurrent()
			object = scn.objects.active
			object.modifiers.append(Blender.Modifier.Type.SUBSURF)
			object.makeDisplayList()
			Blender.Window.RedrawAll()

		If you try this example without the line to update the display list, the
		object will disappear from the screen until you press "SubSurf".
		@warn: If after running your script objects disappear from the screen or
			are not displayed correctly, try this method function.  But if the script
			works properly without it, there's no reason to use it.
		"""

	def getScriptLinks (event):
		"""
		Get a list with this Object's script links of type 'event'.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		@rtype: list
		@return: a list with Blender L{Text} names (the script links of the given
				'event' type) or None if there are no script links at all.
		"""

	def clearScriptLinks (links = None):
		"""
		Delete script links from this Object.  If no list is specified, all
		script links are deleted.
		@type links: list of strings
		@param links: None (default) or a list of Blender L{Text} names.
		"""

	def addScriptLink (text, event):
		"""
		Add a new script link to this Object.
		@type text: string
		@param text: the name of an existing Blender L{Text}.
		@type event: string
		@param event: "FrameChanged", "Redraw" or "Render".
		"""

	def makeTrack (tracked, fast = 0):
		"""
		Make this Object track another.
		@type tracked: Blender Object
		@param tracked: the object to be tracked.
		@type fast: int (bool)
		@param fast: if zero, the scene hierarchy is updated automatically.  If
			you set 'fast' to a nonzero value, don't forget to update the scene
			yourself (see L{Scene.Scene.update}).
		@note: you also need to clear the rotation (L{setEuler}) of this object 
			if it was not (0,0,0) already.
		"""

	def clearTrack (mode = 0, fast = 0):
		"""
		Make this Object not track another anymore.
		@type mode: int (bool)
		@param mode: if nonzero the matrix transformation used for tracking is kept.
		@type fast: int (bool)
		@param fast: if zero, the scene hierarchy is updated automatically.  If
			you set 'fast' to a nonzero value, don't forget to update the scene
			yourself (see L{Scene.Scene.update}).
		"""

	def getAllProperties ():
		"""
		Return a list of all game properties from this object.
		@rtype: PyList
		@return: List of Property objects.
		"""

	def getProperty (name):
		"""
		Return a game property from this object matching the name argument.
		@type name: string
		@param name: the name of the property to get.
		@rtype: Property object
		@return: The first property that matches name.
		"""

	def addProperty (name_or_property, data, type):
		"""
		Add or create a game property for an object.  If called with only a
		property object, the property is assigned to the object.  If called
		with a property name string and data object, a new property is
		created and added to the object.
		@type name_or_property: string or Property object
		@param name_or_property: the property name, or a property object.
		@type data: string, int or float
		@param data: Only valid when I{name_or_property} is a string. 
		Value depends on what is passed in:
			- string:  string type property
			- int:  integer type property
			- float:  float type property
		@type type: string (optional)
		@param type: Only valid when I{name_or_property} is a string.
		Can be the following:
			- 'BOOL'
			- 'INT'
			- 'FLOAT'
			- 'TIME'
			- 'STRING'
		@warn: If a type is not declared string data will
		become string type, int data will become int type
		and float data will become float type. Override type
		to declare bool type, and time type.
		@warn:  A property object can be added only once to an object;
		you must remove the property from an object to add it elsewhere.
		"""

	def removeProperty (property):
		"""
		Remove a game property from an object.
		@type property: Property object or string
		@param property: Property object or property name to be removed.
		"""

	def removeAllProperties():
		"""
		Removes all game properties from an object. 
		"""

	def copyAllPropertiesTo (object):
		"""
		Copies all game properties from one object to another.
		@type object: Object object
		@param object: Object that will receive the properties.
		"""

	def getPIStregth():
		"""
		Get the Object's Particle Interaction Strength.
		@rtype: float
		"""

	def setPIStrength(strength):
		"""
		Set the Object's Particle Interaction Strength.
		Values between -1000.0 to 1000.0
		@rtype: None
		@type strength: float
		@param strength: the Object's Particle Interaction New Strength.
		"""

	def getPIFalloff():
		"""
		Get the Object's Particle Interaction falloff.
		@rtype: float
		"""

	def setPIFalloff(falloff):
		"""
		Set the Object's Particle Interaction falloff.
		Values between 0 to 10.0
		@rtype: None
		@type falloff: float
		@param falloff: the Object's Particle Interaction New falloff.
		"""    
		
	def getPIMaxDist():
		"""
		Get the Object's Particle Interaction MaxDist.
		@rtype: float
		"""

	def setPIMaxDist(MaxDist):
		"""
		Set the Object's Particle Interaction MaxDist.
		Values between 0 to 1000.0
		@rtype: None
		@type MaxDist: float
		@param MaxDist: the Object's Particle Interaction New MaxDist.
		"""    
		
	def getPIType():
		"""
		Get the Object's Particle Interaction Type.
		@rtype: int
		"""

	def setPIType(type):
		"""
		Set the Object's Particle Interaction type.
		Use Module Constants
			- NONE
			- WIND
			- FORCE
			- VORTEX
			- MAGNET
		@rtype: None
		@type type: int
		@param type: the Object's Particle Interaction Type.
		"""   

	def getPIUseMaxDist():
		"""
		Get the Object's Particle Interaction if using MaxDist.
		@rtype: int
		"""

	def setPIUseMaxDist(status):
		"""
		Set the Object's Particle Interaction MaxDist.
		0 = Off, 1 = on
		@rtype: None
		@type status: int
		@param status: the new status
		""" 

	def getPIDeflection():
		"""
		Get the Object's Particle Interaction Deflection Setting.
		@rtype: int
		"""

	def setPIDeflection(status):
		"""
		Set the Object's Particle Interaction Deflection Setting.
		0 = Off, 1 = on
		@rtype: None
		@type status: int
		@param status: the new status
		""" 

	def getPIPermf():
		"""
		Get the Object's Particle Interaction Permeability.
		@rtype: float
		"""

	def setPIPerm(perm):
		"""
		Set the Object's Particle Interaction Permeability.
		Values between 0 to 10.0
		@rtype: None
		@type perm: float
		@param perm: the Object's Particle Interaction New Permeability.
		"""    

	def getPIRandomDamp():
		"""
		Get the Object's Particle Interaction RandomDamp.
		@rtype: float
		"""

	def setPIRandomDamp(damp):
		"""
		Set the Object's Particle Interaction RandomDamp.
		Values between 0 to 10.0
		@rtype: None
		@type damp: float
		@param damp: the Object's Particle Interaction New RandomDamp.
		"""    

	def getPISurfaceDamp():
		"""
		Get the Object's Particle Interaction SurfaceDamp.
		@rtype: float
		"""

	def setPISurfaceDamp(damp):
		"""
		Set the Object's Particle Interaction SurfaceDamp.
		Values between 0 to 10.0
		@rtype: None
		@type damp: float
		@param damp: the Object's Particle Interaction New SurfaceDamp.
		"""    

	def getSBMass():
		"""
		Get the Object's SoftBody Mass.
		@rtype: float
		"""

	def setSBMass(mass):
		"""
		Set the Object's SoftBody Mass.
		Values between 0 to 50.0
		@rtype: None
		@type mass: float
		@param mass: the Object's SoftBody New mass.
		"""  
	
	def getSBGravity():
		"""
		Get the Object's SoftBody Gravity.
		@rtype: float
		"""

	def setSBGravity(grav):
		"""
		Set the Object's SoftBody Gravity.
		Values between 0 to 10.0
		@rtype: None
		@type grav: float
		@param grav: the Object's SoftBody New Gravity.
		""" 
		
	def getSBFriction():
		"""
		Get the Object's SoftBody Friction.
		@rtype: float
		"""

	def setSBFriction(frict):
		"""
		Set the Object's SoftBody Friction.
		Values between 0 to 10.0
		@rtype: None
		@type frict: float
		@param frict: the Object's SoftBody New Friction.
		""" 

	def getSBErrorLimit():
		"""
		Get the Object's SoftBody ErrorLimit.
		@rtype: float
		"""

	def setSBErrorLimit(err):
		"""
		Set the Object's SoftBody ErrorLimit.
		Values between 0 to 1.0
		@rtype: None
		@type err: float
		@param err: the Object's SoftBody New ErrorLimit.
		""" 
		
	def getSBGoalSpring():
		"""
		Get the Object's SoftBody GoalSpring.
		@rtype: float
		"""

	def setSBGoalSpring(gs):
		"""
		Set the Object's SoftBody GoalSpring.
		Values between 0 to 0.999
		@rtype: None
		@type gs: float
		@param gs: the Object's SoftBody New GoalSpring.
		""" 
		
	def getSBGoalFriction():
		"""
		Get the Object's SoftBody GoalFriction.
		@rtype: float
		"""

	def setSBGoalFriction(gf):
		"""
		Set the Object's SoftBody GoalFriction.
		Values between 0 to 10.0
		@rtype: None
		@type gf: float
		@param gf: the Object's SoftBody New GoalFriction.
		""" 
		
	def getSBMinGoal():
		"""
		Get the Object's SoftBody MinGoal.
		@rtype: float
		"""

	def setSBMinGoal(mg):
		"""
		Set the Object's SoftBody MinGoal.
		Values between 0 to 1.0
		@rtype: None
		@type mg: float
		@param mg: the Object's SoftBody New MinGoal.
		""" 
		
	def getSBMaxGoal():
		"""
		Get the Object's SoftBody MaxGoal.
		@rtype: float
		"""

	def setSBMaxGoal(mg):
		"""
		Set the Object's SoftBody MaxGoal.
		Values between 0 to 1.0
		@rtype: None
		@type mg: float
		@param mg: the Object's SoftBody New MaxGoal.
		""" 
		
	def getSBInnerSpring():
		"""
		Get the Object's SoftBody InnerSpring.
		@rtype: float
		"""

	def setSBInnerSpring(sprr):
		"""
		Set the Object's SoftBody InnerSpring.
		Values between 0 to 0.999
		@rtype: None
		@type sprr: float
		@param sprr: the Object's SoftBody New InnerSpring.
		""" 
		
	def getSBInnerSpringFriction():
		"""
		Get the Object's SoftBody InnerSpringFriction.
		@rtype: float
		"""

	def setSBInnerSpringFriction(sprf):
		"""
		Set the Object's SoftBody InnerSpringFriction.
		Values between 0 to 10.0
		@rtype: None
		@type sprf: float
		@param sprf: the Object's SoftBody New InnerSpringFriction.
		""" 
		
	def getSBDefaultGoal():
		"""
		Get the Object's SoftBody DefaultGoal.
		@rtype: float
		"""

	def setSBDefaultGoal(goal):
		"""
		Set the Object's SoftBody DefaultGoal.
		Values between 0 to 1.0
		@rtype: None
		@type goal: float
		@param goal: the Object's SoftBody New DefaultGoal.
		"""   

	def isSB():
		"""
		Returns the Object's SoftBody enabled state.
		@rtype: boolean
		"""

	def getSBPostDef():
		"""
		get SoftBodies PostDef option
		@rtype: int
		"""

	def setSBPostDef(switch):
		"""
		Enable / Disable SoftBodies PostDef option
		1: on
		0: off
		@rtype: None
		@type switch: int
		@param switch: the Object's SoftBody New PostDef Value.
		""" 

	def getSBUseGoal():
		"""
		get SoftBodies UseGoal option
		@rtype: int
		"""

	def setSBUseGoal(switch):
		"""
		Enable / Disable SoftBodies UseGoal option
		1: on
		0: off
		@rtype: None
		@type switch: int
		@param switch: the Object's SoftBody New UseGoal Value.
		""" 
	def getSBUseEdges():
		"""
		get SoftBodies UseEdges option
		@rtype: int
		"""

	def setSBUseEdges(switch):
		"""
		Enable / Disable SoftBodies UseEdges option
		1: on
		0: off
		@rtype: None
		@type switch: int
		@param switch: the Object's SoftBody New UseEdges Value.
		""" 
		
	def getSBStiffQuads():
		"""
		get SoftBodies StiffQuads option
		@rtype: int
		"""

	def setSBStiffQuads(switch):
		"""
		Enable / Disable SoftBodies StiffQuads option
		1: on
		0: off
		@rtype: None
		@type switch: int
		@param switch: the Object's SoftBody New StiffQuads Value.
		"""     


class Property:
	"""
	The Property object
	===================
		This property gives access to object property data in Blender, used by the game engine.
		@ivar name: The property name.
		@ivar data: Data for this property. Depends on property type.
		@ivar type: The property type.
		@warn:  Comparisons between properties will only be true when
		both the name and data pairs are the same.
	"""

	def getName ():
		"""
		Get the name of this property.
		@rtype: string
		@return: The property name.
		"""

	def setName (name):
		"""
		Set the name of this property.
		@type name: string
		@param name: The new name of the property
		"""

	def getData():
		"""
		Get the data for this property.
		@rtype: string, int, or float
		"""

	def setData(data):
		"""
		Set the data for this property.
		@type data: string, int, or float
		@param data: The data to set for this property.
		@warn:  See object.setProperty().  Changing data
		which is of a different type then the property is 
		set to (i.e. setting an int value to a float type'
		property) will change the type of the property 
		automatically.
		"""

	def getType ():
		"""
		Get the type for this property.
		@rtype: string
		"""

import id_generics
Object.__doc__ += id_generics.attributes
