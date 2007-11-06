# Blender.Scene module and the Scene PyType object

"""
The Blender.Scene submodule.

B{New}:
	- L{Scene.clearScriptLinks<Scene.Scene.clearScriptLinks>} accepts a parameter now.
	- acess methods L{Scene.getLayers<Scene.Scene.getLayers>}, L{Scene.setLayers<Scene.Scene.setLayers>} via lists to complement the layers and
		Layers Scene attributes which use bitmasks. 

Scene
=====

This module provides access to B{Scenes} in Blender.

Example::
	import Blender
	from Blender import Scene, Object, Camera
	#
	camdata = Camera.New('persp')           # create new camera data
	camdata.name = 'newCam'
	camdata.lens = 16.0
	scene = Scene.New('NewScene')           # create a new scene
	scene.objects.new(camdata,'Camera')     # add a new object to the scene with newly-created data
	scene.makeCurrent()                     # make this the current scene

@warn: B{scene.objects.new} is the preferred way to add new objects to a scene.
	The older way is to create an object with B{Object.New()}, link the
	data to the new object, then link the object to the scene.  This way is
	not recommended since a forgotten step or run-time error in the script can 
	cause bad things to be done to Blender's database.

	If you use this older method, it's recommended to always perform the
	operations in this order.  This is because if
	there is no object data linked to an object B{ob}, B{scene.link(ob)} will
	automatically create the missing data.  This is OK on its own, but I{if
	after that} object B{ob} is linked to obdata, the automatically created one
	will be discarded -- as expected -- but will stay in Blender's memory
	space until the program is exited, since Blender doesn't really get rid of
	most kinds of data.  So first linking ObData to object, then object to
	scene is a tiny tiny bit faster than the other way around and also saves
	some realtime memory (if many objects are created from scripts, the
	savings become important).
"""

def New (name = 'Scene'):
	"""
	Create a new Scene in Blender.
	@type name: string
	@param name: The Scene name.
	@rtype: Blender Scene
	@return: The created Scene.
	"""

def Get (name = None):
	"""
	Get the Scene(s) from Blender.
	@type name: string
	@param name: The name of a Scene.
	@rtype: Blender Scene or a list of Blender Scenes
	@return: It depends on the I{name} parameter:
			- (name): The Scene with the given I{name};
			- ():     A list with all Scenes currently in Blender.
	"""

def GetCurrent():
	"""
	Get the currently active Scene in Blender.
	@rtype: Blender Scene
	@return: The currently active Scene.
	"""

def Unlink(scene):
	"""
	Unlink (delete) a Scene from Blender.
	@type scene: Blender Scene
	@param scene: The Scene to be unlinked.
	"""
	
from IDProp import IDGroup, IDArray
class Scene:
	"""
	The Scene object
	================
		This object gives access to Scene data in Blender.
	@type Layers: integer (bitmask)
	@ivar Layers: The Scene layers (check also the easier to use
		L{layers}).  This value is a bitmask with at least
		one position set for the 20 possible layers starting from the low order
		bit.  The easiest way to deal with these values in in hexadecimal 
		notation.
		Example::
			scene.Layers = 0x04 # sets layer 3 ( bit pattern 0100 )
			scene.Layers |= 0x01
			print scene.Layers # will print: 5 ( meaning bit pattern 0101)
		After setting the Layers value, the interface (at least the 3d View and
		the Buttons window) needs to be redrawn to show the changes.
	@type layers: list of integers
	@ivar layers: The Scene layers (check also L{Layers}).
		This attribute accepts and returns a list of integer values in the
		range [1, 20].
		Example::
			scene.layers = [3] # set layer 3
			scene.layers = scene.layers.append(1)
			print scene.layers # will print: [1, 3]
	@type objects: sequence of objects
	@ivar objects: The scene's objects. The sequence supports the methods .link(ob), .unlink(ob), and .new(obdata), and can be iterated over.
	@type cursor: Vector (wrapped)
	@ivar cursor: the 3d cursor location for this scene.
	@type camera: Camera or None
	@ivar camera: The active camera for this scene (can be set)
	@type world: World or None
	@ivar world: The world that this scene uses (if any)
	@type timeline: Timeline
	@ivar timeline: The L{timeline<TimeLine.TimeLine>} for this scene, named markers are stored here. (read only)
	@type render: RenderData
	@ivar render: The scenes L{render<Render.RenderData>} settings. (read only)
	@type radiosity: RenderData
	@ivar radiosity: The scenes L{radiosity<Radio>} settings. (read only)
	"""

	def getName():
		"""
		Get the name of this Scene.
		@rtype: string
		"""

	def setName(name):
		"""
		Set the name of this Scene.
		@type name: string
		@param name: The new name.
		"""

	def getLayers():
		"""
		Get the layers set for this Scene.
		@rtype: list of integers
		@return: a list where each number means the layer with that number is set.
		"""

	def setLayers(layers):
		"""
		Set the visible layers for this scene.
		@type layers: list of integers
		@param layers: a list of integers in the range [1, 20], where each available
			index makes the layer with that number visible.
		@note: if this Scene is the current one, the 3D View layers are also
			updated, but the screen needs to be redrawn (at least 3D Views and
			Buttons windows) for the changes to be seen.
		"""

	def copy(duplicate_objects = 1):
		"""
		Make a copy of this Scene.
		@type duplicate_objects: int
		@param duplicate_objects: Defines how the Scene children are duplicated:
				- 0: Link Objects;
				- 1: Link Object Data;
				- 2: Full copy.
		@rtype: Scene
		@return: The copied Blender Scene.
		"""

	def makeCurrent():
		"""
		Make this Scene the currently active one in Blender.
		"""

	def update(full = 0):
		"""
		Update this Scene in Blender.
		@type full: int
		@param full: A bool to control the level of updating:
				- 0: sort the base list of objects.
				- 1: sort and also regroup, do ipos, keys, script links, etc.
		@warn: When in doubt, try with I{full = 0} first, since it is faster.
				The "full" update is a recent addition to this method.
		"""

	def getRenderingContext():
		"""
		Get the rendering context for this scene, see L{Render}.
		@rtype: RenderData
		@return: the render data object for this scene.
		"""

	def getRadiosityContext():
		"""
		Get the radiosity context for this scene, see L{Radio}.
		@rtype: Blender Radiosity
		@return: the radiosity object for this scene.
		@note: only the current scene can return a radiosity context.
		"""

	def getChildren():
		"""
		Get all objects linked to this Scene. (B{deprecated}).  B{Note}: new scripts
		should use the L{objects} attribute instead. In cases where a list is
		required use list(scn.objects).
		@rtype: list of Blender Objects
		@return: A list with all Blender Objects linked to this Scene.
		@note: L{Object.Get} will return all objects currently in Blender, which
			means all objects from all available scenes.  In most cases (exporter
			scripts, for example), it's probably better to use this
			scene.GetChildren instead, since it will only access objects from this
			particular scene.
		@warn: Depricated! use scene.objects instead.
		"""

	def getActiveObject():
		"""
		Get this scene's active object.
		@note: the active object, if selected, can also be retrieved with
			L{Object.GetSelected} -- it is the first item in the returned
			list.  But even when no object is selected in Blender, there can be
			an active one (if the user enters editmode, for example, this is the
			object that should become available for edition).  So what makes this
			scene method different from C{Object.GetSelected()[0]} is that it can
			return the active object even when no objects are selected.
		@rtype: Blender Object or None
		@return: the active object or None if not available.
		@warn: Depricated! use scene.objects.active instead.
		"""

	def getCurrentCamera():
		"""
		Get the currently active Camera for this Scene.
		@note: The active camera can be any object type, not just a camera object.
		@rtype: Blender Object
		@return: The currently active Camera object.
		"""

	def setCurrentCamera(camera):
		"""
		Set the currently active Camera in this Scene.
		@type camera: Blender Camera
		@param camera: The new active Camera.
		"""

	def link(object):
		"""
		Link an Object to this Scene.
		@type object: Blender Object
		@param object: A Blender Object.
		"""

	def unlink(object):
		"""
		Unlink an Object from this Scene.
		@type object: Blender Object
		@param object: A Blender Object.
		@rtype: boolean
		@return: true if object was found in the scene.
		"""

	def getScriptLinks (event):
		"""
		Get a list with this Scene's script links of type 'event'.
		@type event: string
		@param event: "FrameChanged", "OnLoad", "OnSave", "Redraw" or "Render".
		@rtype: list
		@return: a list with Blender L{Text} names (the script links of the given
				'event' type) or None if there are no script links at all.
		"""

	def clearScriptLinks (links = None):
		"""
		Delete script links from this Scene.  If no list is specified, all
		script links are deleted.
		@type links: list of strings
		@param links: None (default) or a list of Blender L{Text} names.
		"""

	def addScriptLink (text, event):
		"""
		Add a new script link to this Scene.
		
		Using OpenGL functions within a scene ScriptLink will draw graphics over the 3D view.
		There is an issue with the zoom of the floating panels also scaling graphics drawn by your scriptlink.
		This makes matching OpenGL graphics to mouse location impossible.
		Make sure that you use floating point for operations that you would usually use int functions for: glRasterPos2f rather then glRasterPos2i.
		
		The following example shows how you can use the OpenGL model view matrix to obtain the scale value.
		
		Example::
			from Blender import BGL
			view_matrix = BGL.Buffer(BGL.GL_FLOAT, 16)
			BGL.glGetFloatv(BGL.GL_MODELVIEW_MATRIX, view_matrix)
			gl_scale = 1/viewMatrix[0]
			
			# Now that we have the scale we can draw to the correct scale.
			BGL.glRect2f(10*gl_scale, 10*gl_scale, 110*gl_scale, 110*gl_scale)
			
			
		@type text: string
		@param text: the name of an existing Blender L{Text}.
		@type event: string
		@param event: "FrameChanged", "OnLoad", "OnSave", "Redraw" or "Render".
		"""

	def play (mode = 0, win = '<VIEW3D>'):
		"""
		Play a realtime animation.  This is the "Play Back Animation" function in
		Blender, different from playing a sequence of rendered images (for that
		check L{Render.RenderData.play}).
		@type mode: int
		@param mode: controls playing:
				- 0: keep playing in the biggest 'win' window;
				- 1: keep playing in all 'win', VIEW3D and SEQ windows;
				- 2: play once in the biggest VIEW3D;
				- 3: play once in all 'win', VIEW3D and SEQ windows.
		@type win: int
		@param win: window type, see L{Window.Types}.  Only some of them are
			meaningful here: VIEW3D, SEQ, IPO, ACTION, NLA, SOUND.  But the others
			are also accepted, since this function can be used simply as an
			interruptible timer.  If 'win' is not visible or invalid, VIEW3D is
			tried, then any bigger visible window.
		@rtype: bool
		@return: 0 on normal exit or 1 when play back is canceled by user input.
		"""

import id_generics
Scene.__doc__ += id_generics.attributes


class SceneObjects:
	"""
	The SceneObjects (Scene ObjectSeq) object
	=========================================
		This object gives access to the Objects in a Scene in Blender.

		Example::
			from Blender import Scene
			scn = Scene.GetCurrent()
			
			scn.objects.selected = [] # select none
			scn.objects.selected = scn.objects # select all
			scn.objects.context = scn.objects # select all and move into the scenes display layer
			
			# get a list of mesh objects
			obs = [ob for ob in scn.objects if ob.type == 'Mesh']
			
			# Select only these mesh objects
			scn.objects.selected = obs
			
			# print all object names
			for ob in scn.objects: print ob.name
			
			# make a list of objects that you can add and remove to
			# will not affect the current scene
			scene_obs = list(scn.objects)

	@ivar selected: an iterator over all the selected objects in a scene.
	@type selected: sequence of L{Object}
	@ivar context: an iterator over all the visible selected objects in a scene.
	@type context: sequence of L{Object}
	@ivar active: the active object in the scene.
	@type active: L{Object}
	@ivar camera: the active camera in the scene.
	@type camera: L{Object}
	"""

	def new(data):
		"""
		Adds a new object to the scene.  Data is either object data such as a
		L{Mesh} or L{Curve}, or the string "Empty" for an Empty object.  The
		type of the object is determined by the type of the data.
		@type data: string or object data
		@param data: the object data for the new object
		@return: the new object.
		@rtype: L{Object}
		"""

	def link(object):
		"""
		Adds an existing object to the scene.  If the object is already linked
		to the scene, no action is taken and no exception is raised.
		@type object: L{Object}
		@param object: the object
		@rtype: None
		"""

	def unlink(object):
		"""
		Removes an object from the scene.  If the object is not linked
		to the scene, no action is taken and no exception is raised.
		@type object: L{Object}
		@param object: the object
		@rtype: None
		"""

