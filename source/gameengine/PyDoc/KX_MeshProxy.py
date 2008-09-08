# $Id$
# Documentation for KX_MeshProxy

class KX_MeshProxy:
	"""
	A mesh object.
	
	You can only change the vertex properties of a mesh object, not the mesh topology.
	
	To use mesh objects effectively, you should know a bit about how the game engine handles them.
		1. Mesh Objects are converted from Blender at scene load.
		2. The Converter groups polygons by Material.  This means they can be sent to the
		   renderer efficiently.  A material holds:
			1. The texture.
			2. The Blender material.
			3. The Tile properties
			4. The face properties - (From the "Texture Face" panel)
			5. Transparency & z sorting
			6. Light layer
			7. Polygon shape (triangle/quad)
			8. Game Object
		3. Verticies will be split by face if necessary.  Verticies can only be shared between
		   faces if:
			1. They are at the same position
			2. UV coordinates are the same
			3. Their normals are the same (both polygons are "Set Smooth")
			4. They are the same colour
		   For example: a cube has 24 verticies: 6 faces with 4 verticies per face.
		   
	The correct method of iterating over every L{KX_VertexProxy} in a game object::
		import GameLogic
		
		co = GameLogic.getcurrentController()
		obj = co.getOwner()
		
		m_i = 0
		mesh = obj.getMesh(m_i) # There can be more than one mesh...
		while mesh != None:
			for mat in range(mesh.getNumMaterials()):
				for v_index in range(mesh.getVertexArrayLength(mat)):
					vertex = mesh.getVertex(mat, v_index)
					# Do something with vertex here...
					# ... eg: colour the vertex red.
					vertex.colour = [1.0, 0.0, 0.0, 1.0]
			m_i += 1
			mesh = obj.getMesh(m_i)
	
			
	"""
	
	def getNumMaterials():
		"""
		Gets the number of materials associated with this object.
		
		@rtype: integer
		"""
	
	def getMaterialName(matid):
		"""
		Gets the name of the specified material.
		
		@type matid: integer
		@param matid: the specified material.
		@rtype: string
		@return: the attached material name.
		"""
	def getTextureName(matid):
		"""
		Gets the name of the specified material's texture.
		
		@type matid: integer
		@param matid: the specified material
		@rtype: string
		@return: the attached material's texture name.
		"""
	def getVertexArrayLength(matid):
		"""
		Gets the length of the vertex array associated with the specified material.
		
		There is one vertex array for each material.
		
		@type matid: integer
		@param matid: the specified material
		@rtype: integer
		@return: the number of verticies in the vertex array.
		"""
	def getVertex(matid, index):
		"""
		Gets the specified vertex from the mesh object.
		
		@type matid: integer
		@param matid: the specified material
		@type index: integer
		@param index: the index into the vertex array.
		@rtype: L{KX_VertexProxy}
		@return: a vertex object.
		"""
	def getNumPolygons():
		"""
		Returns the number of polygon in the mesh.
		
		@rtype: integer
		"""
	def getPolygon(index):
		"""
		Gets the specified polygon from the mesh.
		
		@type index: integer
		@param index: polygon number
		@rtype: L{KX_PolyProxy}
		@return: a polygon object.
		"""
	def reinstancePhysicsMesh():
		"""
		Updates the physics system with the changed mesh.
		
		A mesh must have only one material with collision flags, 
		and have all collision primitives in one vertex array (ie. < 65535 verts) and
		be either a polytope or polyheder mesh.  If you don't get a warning in the
		console when the collision type is polytope, the mesh is suitable for reinstance.
		
		@rtype: boolean
		@return: True if reinstance succeeded, False if it failed.
		"""

