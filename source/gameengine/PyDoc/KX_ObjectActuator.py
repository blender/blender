# $Id$
# Documentation for KX_ObjectActuator
from SCA_IActuator import *

class KX_ObjectActuator(SCA_IActuator):
	"""
	The object actuator ("Motion Actuator") applies force, torque, displacement, angular displacement,
	velocity, or angular velocity to an object.
	Servo control allows to regulate force to achieve a certain speed target.
	"""
	def getForce():
		"""
		Returns the force applied by the actuator.
		
		@rtype: list [fx, fy, fz, local]
		@return: A four item list, containing the vector force, and a flag specifying whether the force is local.
		"""
	def setForce(fx, fy, fz, local):
		"""
		Sets the force applied by the actuator.
		
		@type fx: float
		@param fx: the x component of the force.
		@type fy: float
		@param fy: the z component of the force.
		@type fz: float
		@param fz: the z component of the force.
		@type local: boolean
		@param local: - False: the force is applied in world coordinates.
		              - True: the force is applied in local coordinates.
		"""
	def getTorque():
		"""
		Returns the torque applied by the actuator.
		
		@rtype: list [S{Tau}x, S{Tau}y, S{Tau}z, local]
		@return: A four item list, containing the vector torque, and a flag specifying whether
		         the torque is applied in local coordinates (True) or world coordinates (False)
		"""
	def setTorque(tx, ty, tz, local):
		"""
		Sets the torque applied by the actuator.
		
		@type tx: float
		@param tx: the x component of the torque.
		@type ty: float
		@param ty: the z component of the torque.
		@type tz: float
		@param tz: the z component of the torque.
		@type local: boolean
		@param local: - False: the torque is applied in world coordinates.
		              - True: the torque is applied in local coordinates.
		"""
	def getDLoc():
		"""
		Returns the displacement vector applied by the actuator.
		
		@rtype: list [dx, dy, dz, local]
		@return: A four item list, containing the vector displacement, and whether
		         the displacement is applied in local coordinates (True) or world
			 coordinates (False)
		"""
	def setDLoc(dx, dy, dz, local):
		"""
		Sets the displacement vector applied by the actuator.
		
		Since the displacement is applied every frame, you must adjust the displacement
		based on the frame rate, or you game experience will depend on the player's computer
		speed.
		
		@type dx: float
		@param dx: the x component of the displacement vector.
		@type dy: float
		@param dy: the z component of the displacement vector.
		@type dz: float
		@param dz: the z component of the displacement vector.
		@type local: boolean
		@param local: - False: the displacement vector is applied in world coordinates.
		              - True: the displacement vector is applied in local coordinates.
		"""
	def getDRot():
		"""
		Returns the angular displacement vector applied by the actuator.
		
		@rtype: list [dx, dy, dz, local]
		@return: A four item list, containing the angular displacement vector, and whether
		         the displacement is applied in local coordinates (True) or world
			 coordinates (False)
		"""
	def setDRot(dx, dy, dz, local):
		"""
		Sets the angular displacement vector applied by the actuator.
		
		Since the displacement is applied every frame, you must adjust the displacement
		based on the frame rate, or you game experience will depend on the player's computer
		speed.
		
		@type dx: float
		@param dx: the x component of the angular displacement vector.
		@type dy: float
		@param dy: the z component of the angular displacement vector.
		@type dz: float
		@param dz: the z component of the angular displacement vector.
		@type local: boolean
		@param local: - False: the angular displacement vector is applied in world coordinates.
		              - True: the angular displacement vector is applied in local coordinates.
		"""
	def getLinearVelocity():
		"""
		Returns the linear velocity applied by the actuator.
		For the servo control actuator, this is the target speed.
		
		@rtype: list [vx, vy, vz, local]
		@return: A four item list, containing the vector velocity, and whether the velocity is applied in local coordinates (True) or world coordinates (False)
		"""
	def setLinearVelocity(vx, vy, vz, local):
		"""
		Sets the linear velocity applied by the actuator.
		For the servo control actuator, sets the target speed.
		
		@type vx: float
		@param vx: the x component of the velocity vector.
		@type vy: float
		@param vy: the z component of the velocity vector.
		@type vz: float
		@param vz: the z component of the velocity vector.
		@type local: boolean
		@param local: - False: the velocity vector is in world coordinates.
		              - True: the velocity vector is in local coordinates.
		"""
	def getAngularVelocity():
		"""
		Returns the angular velocity applied by the actuator.
		
		@rtype: list [S{omega}x, S{omega}y, S{omega}z, local]
		@return: A four item list, containing the vector velocity, and whether
		         the velocity is applied in local coordinates (True) or world
			 coordinates (False)
		"""
	def setAngularVelocity(wx, wy, wz, local):
		"""
		Sets the angular velocity applied by the actuator.
		
		@type wx: float
		@param wx: the x component of the velocity vector.
		@type wy: float
		@param wy: the z component of the velocity vector.
		@type wz: float
		@param wz: the z component of the velocity vector.
		@type local: boolean
		@param local: - False: the velocity vector is applied in world coordinates.
		              - True: the velocity vector is applied in local coordinates.
		"""
	def getDamping():
		"""
		Returns the damping parameter of the servo controller.
		
		@rtype: integer
		@return: the time constant of the servo controller in frame unit.
		"""
	def setDamping(damp):
		"""
		Sets the damping parameter of the servo controller.
		
		@type damp: integer
		@param damp: the damping parameter in frame unit.
		"""
	def getForceLimitX():
		"""
		Returns the min/max force limit along the X axis used by the servo controller.
		
		@rtype: list [min, max, enabled]
		@return: A three item list, containing the min and max limits of the force as float
		         and whether the limits are active(true) or inactive(true)
		"""
	def setForceLimitX(min, max, enable):
		"""
		Sets the min/max force limit along the X axis and activates or deactivates the limits in the servo controller.
		
		@type min: float
		@param min: the minimum value of the force along the X axis.
		@type max: float
		@param max: the maximum value of the force along the X axis.
		@type enable: boolean
		@param enable: - True: the force will be limited to the min/max
		               - False: the force will not be limited		               
		"""
	def getForceLimitY():
		"""
		Returns the min/max force limit along the Y axis used by the servo controller.
		
		@rtype: list [min, max, enabled]
		@return: A three item list, containing the min and max limits of the force as float
		         and whether the limits are active(true) or inactive(true)
		"""
	def setForceLimitY(min, max, enable):
		"""
		Sets the min/max force limit along the Y axis and activates or deactivates the limits in the servo controller.
		
		@type min: float
		@param min: the minimum value of the force along the Y axis.
		@type max: float
		@param max: the maximum value of the force along the Y axis.
		@type enable: boolean
		@param enable: - True: the force will be limited to the min/max
		               - False: the force will not be limited		               
		"""
	def getForceLimitZ():
		"""
		Returns the min/max force limit along the Z axis used by the servo controller.
		
		@rtype: list [min, max, enabled]
		@return: A three item list, containing the min and max limits of the force as float
		         and whether the limits are active(true) or inactive(true)
		"""
	def setForceLimitZ(min, max, enable):
		"""
		Sets the min/max force limit along the Z axis and activates or deactivates the limits in the servo controller.
		
		@type min: float
		@param min: the minimum value of the force along the Z axis.
		@type max: float
		@param max: the maximum value of the force along the Z axis.
		@type enable: boolean
		@param enable: - True: the force will be limited to the min/max
		               - False: the force will not be limited		               
		"""
	def getPID():
		"""
		Returns the PID coefficient of the servo controller.
		
		@rtype: list [P, I, D]
		@return: A three item list, containing the PID coefficient as floats:
		         P : proportional coefficient
		         I : Integral coefficient
		         D : Derivate coefficient
		"""
	def setPID(P, I, D):
		"""
		Sets the PID coefficients of the servo controller.
		
		@type P: flat
		@param P: proportional coefficient
		@type I: float
		@param I: Integral coefficient
		@type D: float
		@param D: Derivate coefficient
		"""


