# $Id$
# Documentation for KX_VisibilityActuator
from SCA_IActuator import *

class KX_VisibilityActuator(SCA_IActuator):
	"""
	Visibility Actuator.
	@ivar visibility: whether the actuator makes its parent object visible or invisible
	@type visibility: boolean
	@ivar recursion: whether the visibility/invisibility should be propagated to all children of the object
	@type recursion: boolean
	"""
	def set(visible):
		"""
		DEPRECATED: Use the visibility property instead.
		Sets whether the actuator makes its parent object visible or invisible.

		@param visible: - True: Makes its parent visible.
		                - False: Makes its parent invisible.
		"""
