# $Id$
# Documentation for KX_MeshProxy

class KX_MeshProxy:
	"""
	A mesh object.
	
	You can only change the vertex properties of a mesh object, not the mesh topology.
	"""
	
	def GetNumMaterials():
		"""
		Gets the number of materials associated with this object.
		
		@rtype integer
		"""
	
	def GetMaterialName(matid):
		"""
		Gets the name of the specified material.
		
		@type matid: integer
		@param matid: the specified material.
		@rtype: string
		@return: the attached material name.
		"""
	def GetTextureName(matid):
		"""
		Gets the name of the specified material's texture.
		
		@type matid: integer
		@param matid: the specified material
		@rtype: string
		@return: the attached material's texture name.
		"""
	def GetVertexArrayLength(matid):
		"""
		Gets the length of the vertex array associated with the specified material.
		
		There is one vertex array for each material.
		
		@type matid: integer
		@param matid: the specified material
		@rtype: integer
		@return: the number of verticies in the vertex array.
		"""
	def GetVertex(matid, index):
		"""
		Gets the specified vertex from the mesh object.
		
		@type matid: integer
		@param matid: the specified material
		@type index: integer
		@param index: the index into the vertex array.
		@rtype KX_VertexProxy
		@return a vertex object.
		"""

