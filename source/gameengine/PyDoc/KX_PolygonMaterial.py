# $Id$

class KX_PolygonMaterial:
	"""
	This is the interface to materials in the game engine.
	
	Materials define the render state to be applied to mesh objects.
	
	This example requires:
		- PyOpenGL http://pyopengl.sourceforge.net/
		- GLEWPy http://glewpy.sourceforge.net/
	Example::
		
		import GameLogic
		import OpenGL
		from OpenGL.GL import *
		from OpenGL.GLU import *
		import glew
		from glew import *
		
		glewInit()
		
		vertex_shader = \"\"\"
		
		void main(void)
		{
			gl_Position = ftransform();
		}
		\"\"\"
		
		fragment_shader =\"\"\"
		
		void main(void)
		{
			gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
		}
		\"\"\"
		
		class MyMaterial:
			def __init__(self):
				self.pass_no = 0
				# Create a shader
				self.m_program = glCreateProgramObjectARB()
				# Compile the vertex shader
				self.shader(GL_VERTEX_SHADER_ARB, (vertex_shader))
				# Compile the fragment shader
				self.shader(GL_FRAGMENT_SHADER_ARB, (fragment_shader))
				# Link the shaders together
				self.link()
				
			def PrintInfoLog(self, tag, object):
				\"\"\"
				PrintInfoLog prints the GLSL compiler log
				\"\"\"
				print "Tag: 	def PrintGLError(self, tag = ""):
				
			def PrintGLError(self, tag = ""):
				\"\"\"
				Prints the current GL error status
				\"\"\"
				if len(tag):
					print tag
				err = glGetError()
				if err != GL_NO_ERROR:
					print "GL Error: %s\\n"%(gluErrorString(err))
		
			def shader(self, type, shaders):
				\"\"\"
				shader compiles a GLSL shader and attaches it to the current
				program.
				
				type should be either GL_VERTEX_SHADER_ARB or GL_FRAGMENT_SHADER_ARB
				shaders should be a sequence of shader source to compile.
				\"\"\"
				# Create a shader object
				shader_object = glCreateShaderObjectARB(type)
		
				# Add the source code
				glShaderSourceARB(shader_object, len(shaders), shaders)
				
				# Compile the shader
				glCompileShaderARB(shader_object)
				
				# Print the compiler log
				self.PrintInfoLog("vertex shader", shader_object)
				
				# Check if compiled, and attach if it did
				compiled = glGetObjectParameterivARB(shader_object, GL_OBJECT_COMPILE_STATUS_ARB)
				if compiled:
					glAttachObjectARB(self.m_program, shader_object)
					
				# Delete the object (glAttachObjectARB makes a copy)
				glDeleteObjectARB(shader_object)
				
				# print the gl error log
				self.PrintGLError()
				
			def link(self):
				\"\"\"
				Links the shaders together.
				\"\"\"
				# clear error indicator		
				glGetError()
				
				glLinkProgramARB(self.m_program)
		
				self.PrintInfoLog("link", self.m_program)
			
				linked = glGetObjectParameterivARB(self.m_program, GL_OBJECT_LINK_STATUS_ARB)
				if not linked:
					print "Shader failed to link"
					return
		
				glValidateProgramARB(self.m_program)
				valid = glGetObjectParameterivARB(self.m_program, GL_OBJECT_VALIDATE_STATUS_ARB)
				if not valid:
					print "Shader failed to validate"
					return
				
			def Activate(self, rasty, cachingInfo, mat):
				self.pass_no+=1
				if (self.pass_no == 1):
					glDisable(GL_COLOR_MATERIAL)
					glUseProgramObjectARB(self.m_program)
					return True
				
				glEnable(GL_COLOR_MATERIAL)
				glUseProgramObjectARB(0)
				self.pass_no = 0	
				return False
		
		obj = GameLogic.getCurrentController().getOwner()
		
		mesh = obj.getMesh(0)
		
		for mat in mesh.materials:
			mat.setCustomMaterial(MyMaterial())
			print mat.texture
	
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
		
