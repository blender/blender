# $Id$
# Documentation for the polygon proxy class

class KX_PolyProxy:
	"""
	A polygon holds the index of the vertex forming the poylgon.

	Note: 
	The polygon attributes are read-only, you need to retrieve the vertex proxy if you want
	to change the vertex settings. 
	
	@ivar matname: The name of polygon material, empty if no material.
	@type matname: string
	@ivar material: The material of the polygon
	@type material: L{KX_PolygonMaterial} or KX_BlenderMaterial
	@ivar texture: The texture name of the polygon.
	@type texture: string
	@ivar matid: The material index of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type matid: integer
	@ivar v1: vertex index of the first vertex of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type v1: integer	
	@ivar v2: vertex index of the second vertex of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type v2: integer	
	@ivar v3: vertex index of the third vertex of the polygon, use this to retrieve vertex proxy from mesh proxy
	@type v3: integer	
	@ivar v4: vertex index of the fourth vertex of the polygon, 0 if polygon has only 3 vertex
	          use this to retrieve vertex proxy from mesh proxy
	@type v4: integer	
	@ivar visible: visible state of the polygon: 1=visible, 0=invisible
	@type visible: integer
	@ivar collide: collide state of the polygon: 1=receives collision, 0=collision free.
	@type collide: integer
	"""

	def getMaterialName(): 
		"""
		Returns the polygon material name with MA prefix
		
		@rtype: string
		@return: material name
		"""
	def getMaterial(): 
		"""
		Returns the polygon material
		
		@rtype: L{KX_PolygonMaterial} or KX_BlenderMaterial
		"""
	def getTextureName():
		"""
		Returns the polygon texture name
		
		@rtype: string
		@return: texture name
		"""
	def getMaterialIndex():
		"""
		Returns the material bucket index of the polygon. 
		This index and the ones returned by getVertexIndex() are needed to retrieve the vertex proxy from L{KX_MeshProxy}.
		
		@rtype: integer
		@return: the material index in the mesh
		"""
	def getNumVertex(): 
		"""
		Returns the number of vertex of the polygon.
		
		@rtype: integer
		@return: number of vertex, 3 or 4.
		"""
	def isVisible():
		"""
		Returns whether the polygon is visible or not
		
		@rtype: integer
		@return: 0=invisible, 1=visible
		"""
	def isCollider():
		"""
		Returns whether the polygon is receives collision or not
		
		@rtype: integer
		@return: 0=collision free, 1=receives collision
		"""
	def getVertexIndex(vertex):
		"""
		Returns the mesh vertex index of a polygon vertex
		This index and the one returned by getMaterialIndex() are needed to retrieve the vertex proxy from L{KX_MeshProxy}.
		
		@type vertex: integer
		@param vertex: index of the vertex in the polygon: 0->3
		@rtype: integer
		@return: mesh vertex index
		"""
	def getMesh():
		"""
		Returns a mesh proxy
		
		@rtype: L{KX_MeshProxy}
		@return: mesh proxy
		"""
