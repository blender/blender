# $Id$
# Documentation for Camera game objects.
from KX_GameObject import *

class KX_Camera(KX_GameObject):
	"""
	A Camera object.
	
	@cvar INSIDE: see sphereInsideFrustum and boxInsideFrustum
	@cvar INTERSECT: see sphereInsideFrustum and boxInsideFrustum
	@cvar OUTSIDE: see sphereInsideFrustum and boxInsideFrustum
	Attributes:
	@cvar lens: The camera's lens value. float
	@cvar near: The camera's near clip distance. float
	@cvar far: The camera's far clip distance. float
	@cvar frustum_culling: True if this camera is frustum culling. boolean
	@cvar projection_matrix: This camera's 4x4 projection matrix. [[float]]
	@cvar modelview_matrix: This camera's 4x4 model view matrix. [[float]] (read only)
	Regenerated every frame from the camera's position and orientation.
	@cvar camera_to_world: This camera's camera to world transform. [[float]] (read only)
	Regenerated every frame from the camera's position and orientation.
	@cvar world_to_camera: This camera's world to camera transform. [[float]] (read only)
	Regenerated every frame from the camera's position and orientation.
	This is camera_to_world inverted.
	"""
	
	def sphereInsideFrustum(centre, radius):
		"""
		Tests the given sphere against the view frustum.
		
		@return: INSIDE, OUTSIDE or INTERSECT
		@type centre: list
		@param centre: The centre of the sphere (in world coordinates.)
		@type radius: float
		@param radius: the radius of the sphere
		
		Example:
		import GameLogic
		co = GameLogic.getCurrentController()
		cam = co.GetOwner()
		
		# A sphere of radius 4.0 located at [x, y, z] = [1.0, 1.0, 1.0]
		if (cam.sphereInsideFrustum([1.0, 1.0, 1.0], 4) != cam.OUTSIDE):
			# Sphere is inside frustum !
			# Do something useful !
		else:
			# Sphere is outside frustum
		"""
	def boxInsideFrustum(box):
		"""
		Tests the given box against the view frustum.
		
		@return: INSIDE, OUTSIDE or INTERSECT
		@type box: list
		@param box: Eight (8) corner points of the box (in world coordinates.)
		
		Example:
		@verbatim{
		import GameLogic
		co = GameLogic.getCurrentController()
		cam = co.GetOwner()
		
		# Box to test...
		box = []
		box.append([-1.0, -1.0, -1.0])
		box.append([-1.0, -1.0,  1.0])
		box.append([-1.0,  1.0, -1.0])
		box.append([-1.0,  1.0,  1.0])
		box.append([ 1.0, -1.0, -1.0])
		box.append([ 1.0, -1.0,  1.0])
		box.append([ 1.0,  1.0, -1.0])
		box.append([ 1.0,  1.0,  1.0])
		
		if (cam.boxInsideFrustum(box) != cam.OUTSIDE):
			# Box is inside/intersects frustum !
			# Do something useful !
		else:
			# Box is outside the frustum !
		}
		"""
	def pointInsideFrustum(point):
		"""
		Tests the given point against the view frustum.
		
		@rtype: boolean
		@return: True if the given point is inside this camera's viewing frustum.
		@type point: [x, y, z]
		@param point: The point to test (in world coordinates.)
		
		Example:
		@verbatim{
		import GameLogic
		co = GameLogic.getCurrentController()
		cam = co.GetOwner()
		
		# Test point [0.0, 0.0, 0.0]"
		if (cam.pointInsideFrustum([0.0, 0.0, 0.0])):
		# Point is inside frustum !
		# Do something useful !
		else:
		# Box is outside the frustum !
		}
		"""
	def getCameraToWorld():
		"""
		Returns the camera-to-world transform.
		
		@rtype: matrix (4x4 list)
		@return: the camera-to-world transform matrix.
		"""
	def getWorldToCamera():
		"""
		Returns the world-to-camera transform.
		
		This returns the inverse matrix of getCameraToWorld().
		
		@rtype: matrix (4x4 list)
		@return: the world-to-camera transform matrix.
		"""
	def getProjectionMatrix():
		"""
		Returns the camera's projection matrix.
		
		@rtype: matrix (4x4 list)
		@return: the camera's projection matrix.
		"""
	def setProjectionMatrix(matrix):
		"""
		Sets the camera's projection matrix.
		
		You should use normalised device coordinates for the clipping planes:
		left = -1.0, right = 1.0, top = 1.0, bottom = -1.0, near = cam.near, far = cam.far
		
		@type matrix: 4x4 matrix.
		@param matrix: The new projection matrix for this camera.
		
		Example:
		@verbatim{
		import GameLogic
		
		# Generate an identiy matrix.
		def Identity():
			return [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]
		
		# Generate a perspective projection matrix
		def Perspective(cam):
			return [[cam.near, 0.0     ,  0.0                                  ,  0.0                                      ],
			        [0.0     , cam.near,  0.0                                  ,  0.0                                      ],
				[0.0     , 0.0     , -(cam.far+cam.near)/(cam.far-cam.near), -2.0*cam.far*cam.near/(cam.far - cam.near)],
				[0.0     , 0.0     , -1.0                                  ,  0.0                                      ]]
		
		# Generate an orthographic projection matrix
		# You will need to scale the camera
		def Orthographic(cam):
			return [[1.0/cam.scaling[0], 0.0               ,  0.0                   ,  0.0                                  ],
				[0.0               , 1.0/cam.scaling[1],  0.0                   ,  0.0                                  ],
				[0.0               , 0.0               , -2.0/(cam.far-cam.near), -(cam.far+cam.near)/(cam.far-cam.near)],
				[0.0               , 0.0               ,  0.0                   ,  1.0                                  ]]
		
		# Generate an isometric projection matrix
		def Isometric():
			return [[0.866, 0.0  , 0.866, 0.0],
			        [0.25 , 0.866,-0.25 , 0.0],
				[0.0  , 0.0  ,-1.0  , 0.0],
				[0.0  , 0.0  , 0.0  , 1.0]]
			m = Identity()
			m[0][0] = m[0][2] = m[1][1] = 0.8660254037844386
			m[1][0] = 0.25
			m[1][2] = -0.25
			m[3][3] = 1.0
			return m
		
		co = GameLogic.getCurrentController()
		cam = co.getOwner()
		cam.setProjectionMatrix(Perspective()))
		"""

