# $Id$
# Documentation for KX_Scene.py

from PyObjectPlus import *

class KX_Scene(PyObjectPlus):
	"""
	Scene.
	
	The activity culling stuff is supposed to disable logic bricks when their owner gets too far
	from the active camera.  It was taken from some code lurking at the back of KX_Scene - who knows 
	what it does!
	
	Example::
		import GameLogic
		
		# get the scene
		scene = GameLogic.getCurrentScene()
		
		# print all the objects in the scene
		for obj in scene.objects:
			print obj.name
		
		# get an object named 'Cube'
		obj = scene.objects["OBCube"]
		
		# get the first object in the scene.
		obj = scene.objects[0]
	
	Example::
		# Get the depth of an object in the camera view.
		import GameLogic
		
		obj = GameLogic.getCurrentController().getOwner()
		cam = GameLogic.getCurrentScene().active_camera
		
		# Depth is negative and decreasing further from the camera
		depth = obj.position[0]*cam.world_to_camera[2][0] + obj.position[1]*cam.world_to_camera[2][1] + obj.position[2]*cam.world_to_camera[2][2] + cam.world_to_camera[2][3]
	
	@bug: All attributes are read only at the moment.
		
	@ivar name: The scene's name
	@type name: string
	@ivar objects: A list of objects in the scene.
	@type objects: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_GameObject.KX_GameObject>}
	@ivar objects_inactive: A list of objects on background layers (used for the addObject actuator).
	@type objects_inactive: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_GameObject.KX_GameObject>}
	@ivar lights: A list of lights in the scene.
	@type lights: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_LightObject.KX_LightObject>}
	@ivar cameras: A list of cameras in the scene.
	@type cameras: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_Camera.KX_Camera>}
	@ivar active_camera: The current active camera
	@type active_camera: L{KX_Camera}
	@ivar suspended: True if the scene is suspended.
	@type suspended: boolean
	@ivar activity_culling: True if the scene is activity culling
	@type activity_culling: boolean
	@ivar activity_culling_radius: The distance outside which to do activity culling.  Measured in manhattan distance.
	@type activity_culling_radius: float
	@group Deprecated: getLightList, getObjectList, getName
	"""
	
	def getLightList():
		"""
		DEPRECATED: use the 'lights' property.
		Returns the list of lights in the scene.
		
		@rtype: list [L{KX_LightObject}]
		"""
	def getObjectList():
		"""
		DEPRECATED: use the 'objects' property.
		Returns the list of objects in the scene.
		
		@rtype: list [L{KX_GameObject}]
		"""
	def getName():
		"""
		DEPRECATED: use the 'name' property.
		Returns the name of the scene.
		
		@rtype: string
		"""

	def addObject(object, other, time=0):
		"""
		Adds an object to the scene like the Add Object Actuator would, and returns the created object.
		
		@param object: The object to add
		@type object: L{KX_GameObject} or string
		@param other: The object's center to use when adding the object
		@type other: L{KX_GameObject} or string
		@param time: The lifetime of the added object, in frames. A time of 0 means the object will last forever.
		@type time: int
		
		@rtype: L{KX_GameObject}
		"""
