# $Id: KX_ParentActuator.py 2615 2004-06-02 12:43:27Z kester $
# Documentation for KX_ParentActuator
from SCA_IActuator import *

class KX_ParentActuator(SCA_IActuator):
	"""
	The parent actuator can set or remove an objects parent object.
	"""
	def setObject(object):
		"""
		Sets the object to set as parent.
		
		Object can be either a L{KX_GameObject} or the name of the object.
		
		@type object: L{KX_GameObject}, string or None
		"""
	def getObject(name_only = 1):
		"""
		Returns the name of the object to change to.
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""
