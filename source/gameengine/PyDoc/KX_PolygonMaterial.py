# $Id$

class KX_PolygonMaterial:
	"""
	This is the interface to materials in Blender.
	
	Materials define the render state to be applied to mesh objects.
	
	@bug: All attributes are read only.
	
	@ivar texture: Texture name
	@type texture: string
	
	@ivar gl_texture: OpenGL texture handle (eg for glBindTexture(GL_TEXTURE_2D, gl_texture)
	@type gl_texture: integer
	
	@ivar material: Material name
	@type material: string
	
	@ivar tface: Texture face properties
	@type tface: CObject
	
	@ivar tile: Texture is tiling
	@type tile: boolean
	@ivar tilexrep: Number of tile repetitions in x direction.
	@type tilexrep: integer
	@ivar tileyrep: Number of tile repetitions in y direction.
	@type tileyrep: integer
	
	@ivar drawingmode: Drawing mode for the material.
		- 2  (drawingmode & 4)     Textured
		- 4  (drawingmode & 16)    Light
		- 14 (drawingmode & 16384) 3d Polygon Text
	@type drawingmode: bitfield
	
	@ivar transparent: This material is transparent.  All meshes with this
		material will be rendered after non transparent meshes from back
		to front.
	@type transparent: boolean
	
	@ivar zsort: Transparent polygons in meshes with this material will be sorted back to
		front before rendering.
		Non-Transparent polygons will be sorted front to back before rendering.
	@type zsort: boolean
	
	@ivar lightlayer: Light layers this material affects.
	@type lightlayer: bitfield.
	
	@ivar triangle: Mesh data with this material is triangles.
	@type triangle: boolean
	
	@ivar diffuse: The diffuse colour of the material.  black = [0.0, 0.0, 0.0, 1.0] white = [1.0, 1.0, 1.0, 1.0]
	@type diffuse: list [r, g, b, a]
	@ivar specular: The specular colour of the material. black = [0.0, 0.0, 0.0, 1.0] white = [1.0, 1.0, 1.0, 1.0]
	@type specular: list [r, g, b, a] 
	@ivar shininess: The shininess (specular exponent) of the material. 0.0 <= shininess <= 128.0
	@type shininess: float
	@ivar specularity: The amount of specular of the material. 0.0 <= specularity <= 1.0
	@type specularity: float
	"""
	def updateTexture(tface, rasty):
		"""
		Updates a realtime animation.
		
		@param tface: Texture face (eg mat.tface)
		@type tface: CObject
		@param rasty: Rasterizer
		@type rasty: CObject
		"""
	def setTexture(tface):
		"""
		Sets texture render state.
		
		Example::
			mat.setTexture(mat.tface)
		
		@param tface: Texture face
		@type tface: CObject
		"""
	def activate(rasty, cachingInfo):
		"""
		Sets material parameters for this object for rendering.
		
		Material Parameters set:
			1. Texture
			2. Backface culling
			3. Line drawing
			4. Specular Colour
			5. Shininess
			6. Diffuse Colour
			7. Polygon Offset.
		
		@param rasty: Rasterizer instance.
		@type rasty: CObject
		@param cachingInfo: Material cache instance.
		@type cachingInfo: CObject
		"""
	def setCustomMaterial(material):
		"""
		Sets the material state setup object.
		
		Example::
			class PyMaterial:
				def __init__(self):
					self.pass_no = 0
				
				def activate(self, rasty, cachingInfo, material):
					# Activate the material here.
					#
					# The activate method will be called until it returns False.
					# Every time the activate method returns True the mesh will
					# be rendered.
					#
					# rasty is a CObject for passing to material.updateTexture() 
					#       and material.activate()
					# cachingInfo is a CObject for passing to material.activate()
					# material is the KX_PolygonMaterial instance this material
					#          was added to
					
					# default material properties:
					if self.pass_no == 0:
						material.activate(rasty, cachingInfo)
						self.pass_no = 1
						# Return True to do this pass
						return True
					
					self.pass_no = 0
					return False
			
			# Create a new Python Material and pass it to the renderer.
			mat.setCustomMaterial(PyMaterial())
		
		@param material: The material object.
		@type material: instance
		"""
		
