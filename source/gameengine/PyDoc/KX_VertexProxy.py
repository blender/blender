# $Id$
# Documentation for the vertex proxy class

class KX_VertexProxy:
	"""
	A vertex holds position, UV, colour and normal information.
	
	Note:
	The physics simulation is NOT currently updated - physics will not respond
	to changes in the vertex position.
	
	@ivar XYZ: The position of the vertex.
	@type XYZ: list [x, y, z]
	@ivar UV: The texture coordinates of the vertex.
	@type UV: list [u, v]
	@ivar normal: The normal of the vertex 
	@type normal: list [nx, ny, nz]
	@ivar colour: The colour of the vertex. 
	              Black = [0.0, 0.0, 0.0, 1.0], White = [1.0, 1.0, 1.0, 1.0]
	@type colour: list [r, g, b, a]
	@ivar color: Synonym for colour.
	
	@group Position: x, y, z
	@ivar x: The x coordinate of the vertex.
	@type x: float
	@ivar y: The y coordinate of the vertex.
	@type y: float
	@ivar z: The z coordinate of the vertex.
	@type z: float
	
	@group Texture Coordinates: u, v
	@ivar u: The u texture coordinate of the vertex.
	@type u: float
	@ivar v: The v texture coordinate of the vertex.
	@type v: float
	
	@group Colour: r, g, b, a
	@ivar r: The red component of the vertex colour.   0.0 <= r <= 1.0
	@type r: float
	@ivar g: The green component of the vertex colour. 0.0 <= g <= 1.0
	@type g: float
	@ivar b: The blue component of the vertex colour.  0.0 <= b <= 1.0
	@type b: float
	@ivar a: The alpha component of the vertex colour. 0.0 <= a <= 1.0
	@type a: float
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
		@param pos: the new position for this vertex in local coordinates.
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
		
		The colour is represented as four bytes packed into an integer value.  The colour is 
		packed as RGBA.
		
		Since Python offers no way to get each byte without shifting, you must use the struct module to
		access colour in an machine independent way.
		
		Because of this, it is suggested you use the r, g, b and a attributes or the colour attribute instead.
		
		Example::
			import struct;
			col = struct.unpack('4B', struct.pack('I', v.getRGBA()))
			# col = (r, g, b, a)
			# black = (  0,   0,   0, 255)
			# white = (255, 255, 255, 255)
		
		@rtype: integer
		@return: packed colour. 4 byte integer with one byte per colour channel in RGBA format.
		"""
	def setRGBA(col):
		"""
		Sets the colour of this vertex.
		
		See getRGBA() for the format of col, and its relevant problems.  Use the r, g, b and a attributes
		or the colour attribute instead.
		
		setRGBA() also accepts a four component list as argument col.  The list represents the colour as [r, g, b, a]
		with black = [0.0, 0.0, 0.0, 1.0] and white = [1.0, 1.0, 1.0, 1.0]
		
		Example::
			v.setRGBA(0xff0000ff) # Red
			v.setRGBA(0xff00ff00) # Green on little endian, transparent purple on big endian
			v.setRGBA([1.0, 0.0, 0.0, 1.0]) # Red
			v.setRGBA([0.0, 1.0, 0.0, 1.0]) # Green on all platforms.
		
		@type col: integer or list [r, g, b, a]
		@param col: the new colour of this vertex in packed RGBA format.
		"""
	def getNormal():
		"""
		Gets the normal vector of this vertex.
		
		@rtype: list [nx, ny, nz]
		@return: normalised normal vector.
		"""
