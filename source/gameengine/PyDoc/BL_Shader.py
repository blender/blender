
from PyObjectPlus import *

class BL_Shader(PyObjectPlus):
	"""
	BL_Shader GLSL shaders.
	
	TODO - Description
	"""
	
	def setUniformfv(name, fList):
		"""
		Set a uniform with a list of float values
		
		@param name: the uniform name
		@type name: string
		
		@param fList: a list (2, 3 or 4 elements) of float values
		@type fList: list[float]
		"""

	def delSource():
		"""
		TODO - Description

		"""
	def getFragmentProg():
		"""
		Returns the fragment program.
		
		@rtype: string
		@return: The fragment program.
		"""
	def getVertexProg():
		"""
		Get the vertex program.
		
		@rtype: string
		@return: The vertex program.
		"""
	def isValid():
		"""
		Check if the shader is valid.

		@rtype: bool
		@return: True if the shader is valid
		"""
	def setAttrib(enum):
		"""
		Set attribute location. (The parameter is ignored a.t.m. and the value of "tangent" is always used.)
		
		@param enum: attribute location value
		@type enum: integer
		"""
	def setNumberOfPasses( max_pass ):
		"""
		Set the maximum number of passes. Not used a.t.m.
		
		@param max_pass: the maximum number of passes
		@type max_pass: integer
		"""
	def setSampler(name, index):
		"""
		Set uniform texture sample index.
		
		@param name: Uniform name
		@type name: string

		@param index: Texture sample index.
		@type index: integer
		"""
	def setSource(vertexProgram, fragmentProgram):
		"""
		Set the vertex and fragment programs
		
		@param vertexProgram: Vertex program
		@type vertexProgram: string

		@param fragmentProgram: Fragment program
		@type fragmentProgram: string
		"""
	def setUniform1f(name, fx):
		"""
		Set a uniform with 1 float value.
		
		@param name: the uniform name
		@type name: string
		
		@param fx: Uniform value
		@type fx: float
		"""
	def setUniform1i(name, ix):
		"""
		Set a uniform with an integer value.
		
		@param name: the uniform name
		@type name: string

		@param ix: the uniform value
		@type ix: integer
		"""
	def setUniform2f(name, fx, fy):
		"""
		Set a uniform with 2 float values
		
		@param name: the uniform name
		@type name: string

		@param fx: first float value
		@type fx: float
		
		@param fy: second float value
		@type fy: float
		"""
	def setUniform2i(name, ix, iy):
		"""
		Set a uniform with 2 integer values
		
		@param name: the uniform name
		@type name: string

		@param ix: first integer value
		@type ix: integer
		
		@param iy: second integer value
		@type iy: integer
		"""
	def setUniform3f(name, fx,fy,fz):
		"""
		Set a uniform with 3 float values.
		
		@param name: the uniform name
		@type name: string

		@param fx: first float value
		@type fx: float
		
		@param fy: second float value
		@type fy: float

		@param fz: third float value
		@type fz: float
		"""
	def setUniform3i(name, ix,iy,iz):
		"""
		Set a uniform with 3 integer values
		
		@param name: the uniform name
		@type name: string

		@param ix: first integer value
		@type ix: integer
		
		@param iy: second integer value
		@type iy: integer
		
		@param iz: third integer value
		@type iz: integer
		"""
	def setUniform4f(name, fx,fy,fz,fw):
		"""
		Set a uniform with 4 float values.
		
		@param name: the uniform name
		@type name: string

		@param fx: first float value
		@type fx: float
		
		@param fy: second float value
		@type fy: float

		@param fz: third float value
		@type fz: float

		@param fw: fourth float value
		@type fw: float
		"""
	def setUniform4i(name, ix,iy,iz, iw):
		"""
		Set a uniform with 4 integer values
		
		@param name: the uniform name
		@type name: string

		@param ix: first integer value
		@type ix: integer
		
		@param iy: second integer value
		@type iy: integer
		
		@param iz: third integer value
		@type iz: integer
		
		@param iw: fourth integer value
		@type iw: integer
		"""
	def setUniformDef(name, type):
		"""
		Define a new uniform
		
		@param name: the uniform name
		@type name: string

		@param type: uniform type
		@type type: UNI_NONE, UNI_INT, UNI_FLOAT, UNI_INT2, UNI_FLOAT2,	UNI_INT3, UNI_FLOAT3, UNI_INT4,	UNI_FLOAT4,	UNI_MAT3, UNI_MAT4,	UNI_MAX
		"""
	def setUniformMatrix3(name, mat, transpose):
		"""
		Set a uniform with a 3x3 matrix value
		
		@param name: the uniform name
		@type name: string

		@param mat: A 3x3 matrix [[f,f,f], [f,f,f], [f,f,f]]
		@type mat: 3x3 matrix
		
		@param transpose: set to True to transpose the matrix
		@type transpose: bool
		"""
	def setUniformMatrix4(name, mat, transpose):
		"""
		Set a uniform with a 4x4 matrix value
		
		@param name: the uniform name
		@type name: string

		@param mat: A 4x4 matrix [[f,f,f,f], [f,f,f,f], [f,f,f,f], [f,f,f,f]]
		@type mat: 4x4 matrix
		
		@param transpose: set to True to transpose the matrix
		@type transpose: bool
		"""
	def setUniformiv(name, iList):
		"""
		Set a uniform with a list of integer values
		
		@param name: the uniform name
		@type name: string
		
		@param iList: a list (2, 3 or 4 elements) of integer values
		@type iList: list[integer]
		"""
	def validate():
		"""
		Validate the shader object.
		
		"""
