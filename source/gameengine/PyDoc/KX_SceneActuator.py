# $Id$
# Documentation for KX_SceneActuator
from SCA_IActuator import *

class KX_SceneActuator(SCA_IActuator):
	"""
	Scene Actuator logic brick.
	
	@warning: Scene actuators that use a scene name will be ignored if at game start, the
	          named scene doesn't exist or is empty
		  
		  This will generate a warning in the console:
		  
		  C{ERROR: GameObject I{OBName} has a SceneActuator I{ActuatorName} (SetScene) without scene}
	"""
	def setUseRestart(flag):
		"""
		Set flag to True to restart the scene.
		
		@type flag: boolean
		"""
	def setScene(scene):
		"""
		Sets the name of the scene to change to/overlay/underlay/remove/suspend/resume.
		
		@type scene: string
		"""
	def setCamera(camera):
		"""
		Sets the name of the camera to change to.
		
		@type camera: string
		"""
	def getUseRestart():
		"""
		Returns True if the scene will be restarted.
		
		@rtype: boolean
		"""
	def getScene():
		"""
		Returns the name of the scene to change to/overlay/underlay/remove/suspend/resume.
		
		@rtype: string
		"""
	def getCamera():
		"""
		Returns the name of the camera to change to.
		
		@rtype: string
		"""
