<<<<<<< .working
# $Id$
# Documentation for KX_StateActuator
from SCA_IActuator import *

class KX_StateActuator(SCA_IActuator):
	"""
	State actuator changes the state mask of parent object.
	
	Property:
	
	@ivar operation: type of bit operation to be applied on object state mask.
	                 You can use one of the following constant:
	                 KX_STATE_OP_CPY (0) : Copy state mask
	                 KX_STATE_OP_SET (1) : Add bits to state mask
	                 KX_STATE_OP_CLR (2) : Substract bits to state mask
	                 KX_STATE_OP_NEG (3) : Invert bits to state mask
	@type operation: integer
	
	@ivar mask: value that defines the bits that will be modified by the operation.
	            The bits that are 1 in the mask will be updated in the object state,
		        the bits that are 0 are will be left unmodified expect for the Copy operation
		        which copies the mask to the object state
	@type mask: integer
	"""
	def setOperation(op):
		"""
		DEPRECATED: Use the operation property instead.
		Set the type of bit operation to be applied on object state mask.
		Use setMask() to specify the bits that will be modified.
		
		@param op: bit operation (0=Copy, 1=Add, 2=Substract, 3=Invert)
		@type op: integer
		"""
	def setMask(mask):
		"""
		DEPRECATED: Use the mask property instead.
		Set the value that defines the bits that will be modified by the operation.
		The bits that are 1 in the value will be updated in the object state,
		the bits that are 0 are will be left unmodified expect for the Copy operation
		which copies the value to the object state.
		
		@param mask: bits that will be modified
		@type mask: integer
		"""
=======
# $Id$
# Documentation for KX_StateActuator
from SCA_IActuator import *

class KX_StateActuator(SCA_IActuator):
	"""
	State actuator changes the state mask of parent object.
	"""
	def setOperation(op):
		"""
		Set the type of bit operation to be applied on object state mask.
		Use setMask() to specify the bits that will be modified.
		
		@param op: bit operation (0=Copy, 1=Add, 2=Substract, 3=Invert)
		@type op: integer
		"""
	def setMask(mask):
		"""
		Set the value that defines the bits that will be modified by the operation.
		The bits that are 1 in the value will be updated in the object state,
		the bits that are 0 are will be left unmodified expect for the Copy operation
		which copies the value to the object state.
		
		@param mask: bits that will be modified
		@type mask: integer
		"""
>>>>>>> .merge-right.r19804
