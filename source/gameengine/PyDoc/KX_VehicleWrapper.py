from PyObjectPlus import *

class KX_VehicleWrapper(PyObjectPlus):
	"""
	KX_VehicleWrapper
	
	TODO - description
	"""
	
	def addWheel(wheel, attachPos, attachDir, axleDir, suspensionRestLength, wheelRadius, hasSteering):
		
		"""
		Add a wheel to the vehicle
		
		@param wheel: The object to use as a wheel.
		@type wheel: L{KX_GameObject<KX_GameObject.KX_GameObject>} or a KX_GameObject name
		@param attachPos: The position that this wheel will attach to.
		@type attachPos: vector of 3 floats
		@param attachDir: The direction this wheel points.
		@type attachDir: vector of 3 floats
		@param axleDir: The direction of this wheels axle.
		@type axleDir: vector of 3 floats
		@param suspensionRestLength: TODO - Description
		@type suspensionRestLength: float
		@param wheelRadius: The size of the wheel.
		@type wheelRadius: float
		"""

	def applyBraking(force, wheelIndex):
		"""
		Apply a braking force to the specified wheel
		
		@param force: the brake force
		@type force: float
		
		@param wheelIndex: index of the wheel where the force needs to be applied
		@type wheelIndex: integer
		"""
	def applyEngineForce(force, wheelIndex):
		"""
		Apply an engine force to the specified wheel
		
		@param force: the engine force
		@type force: float
		
		@param wheelIndex: index of the wheel where the force needs to be applied
		@type wheelIndex: integer
		"""
	def getConstraintId():
		"""
		Get the constraint ID
		
		@rtype: integer
		@return: the constraint id
		"""
	def getConstraintType():
		"""
		Returns the constraint type.
		
		@rtype: integer
		@return: constraint type
		"""
	def getNumWheels():
		"""
		Returns the number of wheels.
		
		@rtype: integer
		@return: the number of wheels for this vehicle
		"""
	def getWheelOrientationQuaternion(wheelIndex):
		"""
		Returns the wheel orientation as a quaternion.
		
		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		
		@rtype: TODO - type should be quat as per method name but from the code it looks like a matrix
		@return: TODO Description
		"""
	def getWheelPosition(wheelIndex):
		"""
		Returns the position of the specified wheel
		
		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		
		@rtype: list[x, y, z]
		@return: position vector
		"""
	def getWheelRotation(wheelIndex):
		"""
		Returns the rotation of the specified wheel
		
		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		
		@rtype: float
		@return: the wheel rotation
		"""
	def setRollInfluence(rollInfluece, wheelIndex):
		"""
		Set the specified wheel's roll influence.
		The higher the roll influence the more the vehicle will tend to roll over in corners.
		
		@param rollInfluece: the wheel roll influence
		@type rollInfluece: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSteeringValue(steering, wheelIndex):
		"""
		Set the specified wheel's steering
		
		@param steering: the wheel steering
		@type steering: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSuspensionCompression(compression, wheelIndex):
		"""
		Set the specified wheel's compression
		
		@param compression: the wheel compression
		@type compression: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSuspensionDamping(damping, wheelIndex):
		"""
		Set the specified wheel's damping
		
		@param damping: the wheel damping
		@type damping: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setSuspensionStiffness(stiffness, wheelIndex):
		"""
		Set the specified wheel's stiffness
		
		@param stiffness: the wheel stiffness
		@type stiffness: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
	def setTyreFriction(friction, wheelIndex):
		"""
		Set the specified wheel's tyre friction
		
		@param friction: the tyre friction
		@type friction: float

		@param wheelIndex: the wheel index
		@type wheelIndex: integer
		"""
