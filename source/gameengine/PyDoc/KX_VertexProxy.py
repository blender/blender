# $Id$
# Documentation for the vertex proxy class

class KX_VertexProxy:
	"""
	A vertex holds position, UV, colour and normal information.
	
	Note:
	The physics simulation is NOT currently updated - physics will not respond
	to changes in the vertex position.
	
	Attributes:
	@cvar XYZ: The position of the vertex. (list [x, y, z])
	@cvar UV: The texture coordinates of the vertex. (list [u, v])
	@cvar normal: The normal of the vertex (list [nx, ny, nz])
	@cvar colour: The colour of the vertex. (list [r, g, b, a]) Black = [0.0, 0.0, 0.0, 1.0], White = [1.0, 1.0, 1.0, 1.0]
	@cvar color: Synonym for colour.
	
	@cvar x: The x coordinate of the vertex. (float)
	@cvar y: The y coordinate of the vertex. (float)
	@cvar z: The z coordinate of the vertex. (float)
	
	@cvar u: The u texture coordinate of the vertex. (float)
	@cvar v: The v texture coordinate of the vertex. (float)
	
	@cvar r: The red component of the vertex colour.   (float) 0.0 <= r <= 1.0
	@cvar g: The green component of the vertex colour. (float) 0.0 <= g <= 1.0
	@cvar b: The blue component of the vertex colour.  (float) 0.0 <= b <= 1.0
	@cvar a: The alpha component of the vertex colour. (float) 0.0 <= a <= 1.0
	"""
	
	def getXYZ():
		"""
		Gets the position of this vertex.
		
		@rtype: list [x, y, z]
		@return: this vertexes position in local coordinates.
		"""
	def setXYZ(pos):
		"""
		Sets the position of this vertex.
		
		@type pos: list [x, y, z]
		@param: the new position for this vertex in local coordinates.
		"""
	def getUV():
		"""
		Gets the UV (texture) coordinates of this vertex.
		
		@rtype: list [u, v]
		@return: this vertexes UV (texture) coordinates.
		"""
	def setUV(uv):
		"""
		Sets the UV (texture) coordinates of this vertex.
		
		@type uv: list [u, v]
		"""
	def getRGBA():
		"""
		Gets the colour of this vertex.
		
		Example:
		# Big endian:
		col = v.getRGBA()
		red = (col & 0xff000000) >> 24
		green = (col & 0xff0000) >> 16
		blue = (col & 0xff00) >> 8
		alpha = (col & 0xff)
		
		# Little endian:
		col = v.getRGBA()
		alpha = (col & 0xff000000) >> 24
		blue = (col & 0xff0000) >> 16
		green = (col & 0xff00) >> 8
		red = (col & 0xff)
		
		@rtype: integer
		@return: packed colour. 4 byte integer with one byte per colour channel in RGBA format.
		"""
	def setRGBA(col):
		"""
		Sets the colour of this vertex.
		
		@type col: integer
		@param col: the new colour of this vertex in packed format.
		"""
	def getNormal():
		"""
		Gets the normal vector of this vertex.
		
		@rtype: list [nx, ny, nz]
		@return: normalised normal vector.
		"""
