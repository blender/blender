# $Id$
# Documentation for KX_Scene.py

class KX_Scene:
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
		for obj in scene.getObjectList():
			print obj.getName()
		
		# get an object named 'Cube'
		obj = scene.getObjectList()["OBCube"]
		
		# get the first object in the scene.
		obj = scene.getObjectList()[0]
	
	Example::
		# Get the depth of an object in the camera view.
		import GameLogic
		
		obj = GameLogic.getCurrentController().getOwner()
		cam = GameLogic.getCurrentScene().active_camera
		
		# Depth is negative and decreasing further from the camera
		depth = obj.position[0]*cam.world_to_camera[2][0] + obj.position[1]*cam.world_to_camera[2][1] + obj.position[2]*cam.world_to_camera[2][2] + cam.world_to_camera[2][3]
		
	@ivar name: The scene's name
	@type name: string
	@ivar active_camera: The current active camera
	@type active_camera: L{KX_Camera}
	@ivar suspended: True if the scene is suspended.
	@type suspended: boolean
	@ivar activity_culling: True if the scene is activity culling
	@type activity_culling: boolean
	@ivar activity_culling_radius: The distance outside which to do activity culling.  Measured in manhattan distance.
	@type activity_culling_radius: float
	"""
	
	def getLightList():
		"""
		Returns the list of lights in the scene.
		
		@rtype: list [L{KX_Light}]
		"""
	def getObjectList():
		"""
		Returns the list of objects in the scene.
		
		@rtype: list [L{KX_GameObject}]
		"""
	def getName():
		"""
		Returns the name of the scene.
		
		@rtype: string
		"""

