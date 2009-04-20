# Blender.Mathutils module and its subtypes

"""
The Blender.Mathutils submodule.

Mathutils
=========

This module provides access to matrices, eulers, quaternions and vectors.

Example::
  import Blender
  from Blender import Mathutils
  from Blender.Mathutils import *

  vec = Vector([1,2,3])
  mat = RotationMatrix(90, 4, 'x')
  matT = TranslationMatrix(vec)

  matTotal = mat * matT
  matTotal.invert()

  mat3 = matTotal.rotationPart
  quat1 = mat.toQuat()
  quat2 = mat3.toQuat()

  angle = DifferenceQuats(quat1, quat2)
  print angle  
"""

def Rand (low=0.0, high = 1.0):
  """
  Return a random number within a range.
  low and high represent are optional parameters which represent the range
  from which the random number must return its result.
  @type low: float
  @param low: The lower range.
  @type high: float
  @param high: The upper range.
  """

def Intersect(vec1, vec2, vec3, ray, orig, clip=1):
  """
  Return the intersection between a ray and a triangle, if possible, return None otherwise.
  @type vec1: Vector object.
  @param vec1: A 3d vector, one corner of the triangle.
  @type vec2: Vector object.
  @param vec2: A 3d vector, one corner of the triangle.
  @type vec3: Vector object.
  @param vec3: A 3d vector, one corner of the triangle.
  @type ray: Vector object.
  @param ray: A 3d vector, the orientation of the ray. the length of the ray is not used, only the direction.
  @type orig: Vector object.
  @param orig: A 3d vector, the origin of the ray.
  @type clip: integer
  @param clip: if 0, don't restrict the intersection to the area of the triangle, use the infinite plane defined by the triangle.
  @rtype: Vector object
  @return: The intersection between a ray and a triangle, if possible, None otherwise.
  """

def TriangleArea(vec1, vec2, vec3):
  """
  Return the area size of the 2D or 3D triangle defined.
  @type vec1: Vector object.
  @param vec1: A 2d or 3d vector, one corner of the triangle.
  @type vec2: Vector object.
  @param vec2: A 2d or 3d vector, one corner of the triangle.
  @type vec3: Vector object.
  @param vec3: A 2d or 3d vector, one corner of the triangle.
  @rtype: float
  @return: The area size of the 2D or 3D triangle defined.
  """

def TriangleNormal(vec1, vec2, vec3):
  """
  Return the normal of the 3D triangle defined.
  @type vec1: Vector object.
  @param vec1: A 3d vector, one corner of the triangle.
  @type vec2: Vector object.
  @param vec2: A 3d vector, one corner of the triangle.
  @type vec3: Vector object.
  @param vec3: A 3d vector, one corner of the triangle.
  @rtype: float
  @return: The normal of the 3D triangle defined.
  """

def QuadNormal(vec1, vec2, vec3, vec4):
  """
  Return the normal of the 3D quad defined.
  @type vec1: Vector object.
  @param vec1: A 3d vector, the first vertex of the quad.
  @type vec2: Vector object.
  @param vec2: A 3d vector, the second vertex of the quad.
  @type vec3: Vector object.
  @param vec3: A 3d vector, the third vertex of the quad.
  @type vec4: Vector object.
  @param vec4: A 3d vector, the fourth vertex of the quad.
  @rtype: float
  @return: The normal of the 3D quad defined.
  """

def LineIntersect(vec1, vec2, vec3, vec4):
  """
  Return a tuple with the points on each line respectively closest to the other
  (when both lines intersect, both vector hold the same value).
  The lines are evaluated as infinite lines in space, the values returned may not be between the 2 points given for each line.
  @type vec1: Vector object.
  @param vec1: A 3d vector, one point on the first line.
  @type vec2: Vector object.
  @param vec2: A 3d vector, another point on the first line.
  @type vec3: Vector object.
  @param vec3: A 3d vector, one point on the second line.
  @type vec4: Vector object.
  @param vec4: A 3d vector, another point on the second line.
  @rtype: (Vector object, Vector object)
  @return: A tuple with the points on each line respectively closest to the other.
  """

def CopyVec(vector):
  """
  Create a copy of the Vector object.
  @attention: B{DEPRECATED} use vector.copy() instead.
  @type vector: Vector object.
  @param vector: A 2d,3d or 4d vector to be copied.
  @rtype: Vector object.
  @return: A new vector object which is a copy of the one passed in.
  """

def CrossVecs(vec1, vec2):
  """
  Return the cross product of two vectors.
  @type vec1: Vector object.
  @param vec1: A 3d vector.
  @type vec2: Vector object.
  @param vec2: A 3d vector.
  @rtype: Vector object.
  @return: A new vector representing the cross product of
  the two vectors.
  """

def DotVecs(vec1, vec2):
  """
  Return the dot product of two vectors.
  @type vec1: Vector object.
  @param vec1: A 2d,3d or 4d vector.
  @type vec2: Vector object.
  @param vec2: A 2d,3d or 4d vector.
  @rtype: float
  @return: Return the scalar product of vector muliplication.
  """

def AngleBetweenVecs(vec1, vec2):
  """
  Return the angle between two vectors. Zero length vectors raise an error.
  @type vec1: Vector object.
  @param vec1: A 2d or 3d vector.
  @type vec2: Vector object.
  @param vec2: A 2d or 3d vector.
  @rtype: float
  @return: The angle between the vectors in degrees.
  @raise AttributeError: When there is a zero-length vector as an argument.
  """

def MidpointVecs(vec1, vec2):
  """
  Return a vector to the midpoint between two vectors.
  @type vec1: Vector object.
  @param vec1: A 2d,3d or 4d vector.
  @type vec2: Vector object.
  @param vec2: A 2d,3d or 4d vector.
  @rtype: Vector object
  @return: The vector to the midpoint.
  """

def VecMultMat(vec, mat):
  """
  Multiply a vector and matrix (pre-multiply)
  Vector size and matrix column size must equal.
  @type vec: Vector object.
  @param vec: A 2d,3d or 4d vector.
  @type mat: Matrix object.
  @param mat: A 2d,3d or 4d matrix.
  @rtype: Vector object
  @return: The row vector that results from the muliplication.
  @attention: B{DEPRECATED} You should now multiply vector * matrix direcly
  Example::
      result = myVector * myMatrix
  """

def ProjectVecs(vec1, vec2):
  """  
  Return the projection of vec1 onto vec2.
  @type vec1: Vector object.
  @param vec1: A 2d,3d or 4d vector.
  @type vec2: Vector object.
  @param vec2: A 2d,3d or 4d vector.
  @rtype: Vector object
  @return: The parallel projection vector.
  """

def RotationMatrix(angle, matSize, axisFlag, axis):
  """  
  Create a matrix representing a rotation.
  @type angle: float
  @param angle: The angle of rotation desired.
  @type matSize: int
  @param matSize: The size of the rotation matrix to construct.
  Can be 2d, 3d, or 4d.
  @type axisFlag: string (optional)
  @param axisFlag: Possible values:
       - "x - x-axis rotation"
       - "y - y-axis rotation"
       - "z - z-axis rotation"
       - "r - arbitrary rotation around vector"
  @type axis: Vector object. (optional)
  @param axis: The arbitrary axis of rotation used with "R"
  @rtype: Matrix object.
  @return: A new rotation matrix.
  """

def TranslationMatrix(vector):
  """  
  Create a matrix representing a translation
  @type vector: Vector object
  @param vector: The translation vector
  @rtype: Matrix object.
  @return: An identity matrix with a translation.
  """

def ScaleMatrix(factor, matSize, axis):
  """  
  Create a matrix representing a scaling.
  @type factor: float
  @param factor: The factor of scaling to apply.
  @type matSize: int
  @param matSize: The size of the scale matrix to construct.
  Can be 2d, 3d, or 4d.
  @type axis: Vector object.  (optional)
  @param axis: Direction to influence scale.
  @rtype: Matrix object.
  @return: A new scale matrix.
  """

def OrthoProjectionMatrix(plane, matSize, axis):
  """  
  Create a matrix to represent an orthographic projection
  @type plane: string
  @param plane: Can be any of the following:
       - "x - x projection (2D)"
       - "y - y projection (2D)"
       - "xy - xy projection"
       - "xz - xz projection"
       - "yz - yz projection"
       - "r - arbitrary projection plane"
  @type matSize: int
  @param matSize: The size of the projection matrix to construct.
  Can be 2d, 3d, or 4d.
  @type axis: Vector object. (optional)
  @param axis: Arbitrary perpendicular plane vector.
  @rtype: Matrix object.
  @return: A new projeciton matrix.
  """

def ShearMatrix(plane, factor, matSize):
  """  
  Create a matrix to represent an orthographic projection
  @type plane: string
  @param plane: Can be any of the following:
       - "x - x shear (2D)"
       - "y - y shear (2D)"
       - "xy - xy shear"
       - "xz - xz shear"
       - "yz - yz shear"
  @type factor: float
  @param factor: The factor of shear to apply.
  @type matSize: int
  @param matSize: The size of the projection matrix to construct.
  Can be 2d, 3d, or 4d.
  @rtype: Matrix object.
  @return: A new shear matrix.
  """

def CopyMat(matrix):
  """
  Create a copy of the Matrix object.
  @type matrix: Matrix object.
  @param matrix: A 2d,3d or 4d matrix to be copied.
  @rtype: Matrix object.
  @return: A new matrix object which is a copy of the one passed in.
  @attention: B{DEPRECATED} Use the matrix copy funtion to make a copy.
  Example::
      newMat = myMat.copy()
  """

def MatMultVec(mat, vec):
  """
  Multiply a matrix and a vector (post-multiply)
  Vector size and matrix row size must equal.
  @type vec: Vector object.
  @param vec: A 2d,3d or 4d vector.
  @type mat: Matrix object.
  @param mat: A 2d,3d or 4d matrix.
  @rtype: Vector object
  @return: The column vector that results from the muliplication.
  @attention: B{DEPRECATED} You should use direct muliplication on the arguments
  Example::
      result = myMatrix * myVector
  """

def CopyQuat(quaternion):
  """
  Create a copy of the Quaternion object.
  @type quaternion: Quaternion object.
  @param quaternion: Quaternion to be copied.
  @rtype: Quaternion object.
  @return: A new quaternion object which is a copy of the one passed in.
  @attention: B{DEPRECATED} You should use the Quaterion() constructor directly
  to create copies of quaternions
  Example::
      newQuat = Quaternion(myQuat)
  """

def CrossQuats(quat1, quat2):
  """
  Return the cross product of two quaternions.
  @type quat1: Quaternion object.
  @param quat1: Quaternion.
  @type quat2: Quaternion object.
  @param quat2: Quaternion.
  @rtype: Quaternion object.
  @return: A new quaternion representing the cross product of
  the two quaternions.
  """

def DotQuats(quat1, quat2):
  """
  Return the dot product of two quaternions.
  @type quat1: Quaternion object.
  @param quat1: Quaternion.
  @type quat2: Quaternion object.
  @param quat2: Quaternion.
  @rtype: float
  @return: Return the scalar product of quaternion muliplication.
  """

def DifferenceQuats(quat1, quat2):
  """
  Returns a quaternion represting the rotational difference.
  @type quat1: Quaternion object.
  @param quat1: Quaternion.
  @type quat2: Quaternion object.
  @param quat2: Quaternion.
  @rtype: Quaternion object
  @return: Return a quaternion which which represents the rotational
  difference between the two quat rotations.
  """

def Slerp(quat1, quat2, factor):
  """
  Returns the interpolation of two quaternions.
  @type quat1: Quaternion object.
  @param quat1: Quaternion.
  @type quat2: Quaternion object.
  @param quat2: Quaternion.
  @type factor: float
  @param factor: The interpolation value
  @rtype: Quaternion object
  @return: The interpolated rotation.
  """

def CopyEuler(euler):
  """
  Create a new euler object.
  @type euler: Euler object
  @param euler: The euler to copy
  @rtype: Euler object
  @return: A copy of the euler object passed in.
  @attention: B{DEPRECATED} You should use the Euler constructor directly
  to make copies of Euler objects
  Example::
      newEuler = Euler(myEuler)
  """

def RotateEuler(euler, angle, axis):
  """
  Roatate a euler by an amount in degrees around an axis.
  @type euler: Euler object
  @param euler: Euler to rotate.
  @type angle: float
  @param angle: The amount of rotation in degrees
  @type axis: string
  @param axis: axis to rotate around:
       - "x"
       - "y"
       - "z"
  """

class Vector:
  """
  The Vector object
  =================
    This object gives access to Vectors in Blender.
  @ivar x: The x value.
  @ivar y: The y value.
  @ivar z: The z value (if any).
  @ivar w: The w value (if any).
  @ivar length: The magnitude of the vector.
  @ivar magnitude: This is a synonym for length.
  @ivar wrapped: Whether or not this item is wrapped data
  @note: Comparison operators can be done on Vector classes:
      - >, >=, <, <= test the vector magnitude
      - ==, != test vector values e.g. 1,2,3 != 1,2,4 even if they are the same length
  @note: Math can be performed on Vector classes
      - vec + vec
      - vec - vec
      - vec * float/int
      - vec * matrix
      - vec * vec
      - vec * quat
      - -vec
  @note: You can access a vector object like a sequence
      - x = vector[0]
  @attention: Vector data can be wrapped or non-wrapped. When a object is wrapped it
  means that the object will give you direct access to the data inside of blender. Modification
  of this object will directly change the data inside of blender. To copy a wrapped object
  you need to use the object's constructor. If you copy and object by assignment you will not get
  a second copy but a second reference to the same data. Only certain functions will return 
  wrapped data. This will be indicated in the method description.
  Example::
      wrappedObject = Object.getAttribute() #this is wrapped data
      print wrappedObject.wrapped #prints 'True'
      copyOfObject = Object(wrappedObject) #creates a copy of the object
      secondPointer = wrappedObject #creates a second pointer to the same data
      print wrappedObject.attribute #prints '5'
      secondPointer.attribute = 10
      print wrappedObject.attribute #prints '10'
      print copyOfObject.attribute #prints '5'
  """

  def __init__(list = None):
    """
    Create a new 2d, 3d, or 4d Vector object from a list of floating point numbers.
    @note: that python uses higher precission floating point numbers, so values assigned to a vector may have some rounding error.
    

    Example::
      v = Vector(1,0,0)
      v = Vector(myVec)
      v = Vector(list)
    @type list: PyList of float or int
    @param list: The list of values for the Vector object. Can be a sequence or raw numbers.
    Must be 2, 3, or 4 values. The list is mapped to the parameters as [x,y,z,w].
    @rtype: Vector object.
    @return: It depends wheter a parameter was passed:
        - (list): Vector object initialized with the given values;
        - ():     An empty 3 dimensional vector.
    """

  def copy():
    """
    Returns a copy of this vector
    @return: a copy of itself
    """

  def zero():
    """
    Set all values to zero.
    @return: an instance of itself
    """

  def normalize():
    """
    Normalize the vector, making the length of the vector always 1.0
    @note: Normalize works for vectors of all sizes, however 4D Vectors w axis is left untouched.
    @note: Normalizing a vector where all values are zero results in all axis having a nan value (not a number).
    @return: an instance of itself
    """

  def negate():
    """
    Set all values to their negative.
    @return: an instance of its self
    """

  def resize2D():
    """
    Resize the vector to 2d.
    @return: an instance of itself
    """

  def resize3D():
    """
    Resize the vector to 3d. New axis will be 0.0.
    @return: an instance of itself
    """

  def resize4D():
    """
    Resize the vector to 4d. New axis will be 0.0.
    The last component will be 1.0, to make multiplying 3d vectors by 4x4 matrices easier.
    @return: an instance of itself
    """

  def toTrackQuat(track, up):
    """
    Return a quaternion rotation from the vector and the track and up axis.
    @type track: String.
    @param track: Possible values:
		   - "x - x-axis up"
		   - "y - y-axis up"
		   - "z - z-axis up"
		   - "-x - negative x-axis up"
		   - "-y - negative y-axis up"
		   - "-z - negative z-axis up"
    @type up: String.
    @param up: Possible values:
		   - "x - x-axis up"
		   - "y - y-axis up"
		   - "z - z-axis up"
    @rtype: Quaternion
    @return: Return a quaternion rotation from the vector and the track and up axis.
    """

  def reflect(mirror):
    """
    Return the reflection vector from the mirror vector argument.
    @type mirror: Vector object
    @param mirror: This vector could be a normal from the reflecting surface.
    @rtype: Vector object matching the size of this vector.
    @return: The reflected vector.
    """

class Euler:
  """
  The Euler object
  ================
    This object gives access to Eulers in Blender.
  @ivar x: The heading value in degrees.
  @ivar y: The pitch value in degrees.
  @ivar z: The roll value in degrees.
  @ivar wrapped: Whether or not this object is wrapping data directly
  @note: You can access a euler object like a sequence
      - x = euler[0]
  @note: Comparison operators can be done:
      - ==, != test numeric values within epsilon
  @attention: Euler data can be wrapped or non-wrapped. When a object is wrapped it
  means that the object will give you direct access to the data inside of blender. Modification
  of this object will directly change the data inside of blender. To copy a wrapped object
  you need to use the object's constructor. If you copy and object by assignment you will not get
  a second copy but a second reference to the same data. Only certain functions will return 
  wrapped data. This will be indicated in the method description.
  Example::
      wrappedObject = Object.getAttribute() #this is wrapped data
      print wrappedObject.wrapped #prints 'True'
      copyOfObject = Object(wrappedObject) #creates a copy of the object
      secondPointer = wrappedObject #creates a second pointer to the same data
      print wrappedObject.attribute #prints '5'
      secondPointer.attribute = 10
      print wrappedObject.attribute #prints '10'
      print copyOfObject.attribute #prints '5'
  """

  def __init__(list = None):
    """
    Create a new euler object.

    Example::
      euler = Euler(45,0,0)
      euler = Euler(myEuler)
      euler = Euler(sequence)
    @type list: PyList of float/int
    @param list: 3d list to initialize euler
    @rtype: Euler object
    @return: Euler representing heading, pitch, bank.
    @note: Values are in degrees.
    """

  def zero():
    """
    Set all values to zero.
    @return: an instance of itself
    """

  def copy():
    """
    @return: a copy of this euler.
    """

  def unique():
    """
    Calculate a unique rotation for this euler. Avoids gimble lock.
    @return: an instance of itself
    """

  def toMatrix():
    """
    Return a matrix representation of the euler.
    @rtype: Matrix object
    @return: A roation matrix representation of the euler.
    """

  def toQuat():
    """
    Return a quaternion representation of the euler.
    @rtype: Quaternion object
    @return: Quaternion representation of the euler.
    """

class Quaternion:
  """
  The Quaternion object
  =====================
    This object gives access to Quaternions in Blender.
  @ivar w: The w value.
  @ivar x: The x value.
  @ivar y: The y value.
  @ivar z: The z value.
  @ivar wrapped: Wether or not this object wraps data directly
  @ivar magnitude: The magnitude of the quaternion.
  @ivar axis: Vector representing the axis of rotation.
  @ivar angle: A scalar representing the amount of rotation
  in degrees.
  @note: Comparison operators can be done:
      - ==, != test numeric values within epsilon
  @note: Math can be performed on Quaternion classes
      - quat + quat
      - quat - quat 
      - quat * float/int
      - quat * vec
      - quat * quat
  @note: You can access a quaternion object like a sequence
      - x = quat[0]
  @attention: Quaternion data can be wrapped or non-wrapped. When a object is wrapped it
  means that the object will give you direct access to the data inside of blender. Modification
  of this object will directly change the data inside of blender. To copy a wrapped object
  you need to use the object's constructor. If you copy and object by assignment you will not get
  a second copy but a second reference to the same data. Only certain functions will return 
  wrapped data. This will be indicated in the method description.
  Example::
      wrappedObject = Object.getAttribute() #this is wrapped data
      print wrappedObject.wrapped #prints 'True'
      copyOfObject = Object(wrappedObject) #creates a copy of the object
      secondPointer = wrappedObject #creates a second pointer to the same data
      print wrappedObject.attribute #prints '5'
      secondPointer.attribute = 10
      print wrappedObject.attribute #prints '10'
      print copyOfObject.attribute #prints '5'
  """

  def __init__(list, angle = None):
    """  
    Create a new quaternion object from initialized values.

    Example::
      quat = Quaternion(1,2,3,4)
      quat = Quaternion(axis, angle)
    quat = Quaternion()
    quat = Quaternion(180, list)

    @type list: PyList of int/float
    @param list: A 3d or 4d list to initialize quaternion.
        4d if intializing [w,x,y,z], 3d if used as an axis of rotation.
    @type angle: float (optional)
    @param angle: An arbitrary rotation amount around 'list'.
        List is used as an axis of rotation in this case.
    @rtype: New quaternion object.
    @return: It depends wheter a parameter was passed:
        - (list/angle): Quaternion object initialized with the given values;
        - ():     An identity 4 dimensional quaternion.
    """

  def identity():
    """
    Set the quaternion to the identity quaternion.
    @return: an instance of itself
    """

  def copy():
    """
    make a copy of the quaternion.
    @return: a copy of itself
    """

  def negate():
    """
    Set the quaternion to its negative.
    @return: an instance of itself
    """

  def conjugate():
    """
    Set the quaternion to its conjugate.
    @return: an instance of itself
    """

  def inverse():
    """
    Set the quaternion to its inverse
    @return: an instance of itself
    """

  def normalize():
    """
    Normalize the quaternion.
    @return: an instance of itself
    """

  def toEuler():
    """
    Return Euler representation of the quaternion.
    @rtype: Euler object
    @return: Euler representation of the quaternion.
    """
  
  def toMatrix():
    """
    Return a matrix representation of the quaternion.
    @rtype: Matrix object
    @return: A rotation matrix representation of the quaternion.
    """

class Matrix:
  """
  The Matrix Object
  =================
    This object gives access to Matrices in Blender.
  @ivar rowSize: The row size of the matrix.
  @ivar colSize: The column size of the matrix.
  @ivar wrapped: Whether or not this object wrapps internal data
  @note: Math can be performed on Matrix classes
      - mat + mat 
      - mat - mat 
      - mat * float/int
      - mat * vec
      - mat * mat 
  @note: Comparison operators can be done:
      - ==, != test numeric values within epsilon
  @note: You can access a quaternion object like a 2d sequence
      - x = matrix[0][1]
      - vector = matrix[2]
  @attention: Quaternion data can be wrapped or non-wrapped. When a object is wrapped it
  means that the object will give you direct access to the data inside of blender. Modification
  of this object will directly change the data inside of blender. To copy a wrapped object
  you need to use the object's constructor. If you copy and object by assignment you will not get
  a second copy but a second reference to the same data. Only certain functions will return 
  wrapped data. This will be indicated in the method description.
  Example::
      wrappedObject = Object.getAttribute() #this is wrapped data
      print wrappedObject.wrapped #prints 'True'
      copyOfObject = Object(wrappedObject) #creates a copy of the object
      secondPointer = wrappedObject #creates a second pointer to the same data
      print wrappedObject.attribute #prints '5'
      secondPointer.attribute = 10
      print wrappedObject.attribute #prints '10'
      print copyOfObject.attribute #prints '5'
  """

  def __init__(list1 = None, list2 = None, list3 = None, list4 = None):
    """  
    Create a new matrix object from initialized values.

    Example::
      matrix = Matrix([1,1,1],[0,1,0],[1,0,0])
      matrix = Matrix(mat)
      matrix = Matrix(seq1, seq2, vector)

    @type list1: PyList of int/float
    @param list1: A 2d,3d or 4d list.
    @type list2: PyList of int/float
    @param list2: A 2d,3d or 4d list.
    @type list3: PyList of int/float
    @param list3: A 2d,3d or 4d list.
    @type list4: PyList of int/float
    @param list4: A 2d,3d or 4d list.
    @rtype: New matrix object.
    @return: It depends wheter a parameter was passed:
        - (list1, etc.): Matrix object initialized with the given values;
        - ():     An empty 3 dimensional matrix.
    """

  def zero():
    """
    Set all matrix values to 0.
    @return: an instance of itself
    """


  def copy():
    """
    Returns a copy of this matrix
    @return: a copy of itself
    """

  def identity():
    """
    Set the matrix to the identity matrix.
    An object with zero location and rotation, a scale of 1, will have an identity matrix.

    See U{http://en.wikipedia.org/wiki/Identity_matrix}
    @return: an instance of itself
    """

  def transpose():
    """
    Set the matrix to its transpose.
    
    See U{http://en.wikipedia.org/wiki/Transpose}
    @return: None
    """

  def determinant():
    """
    Return the determinant of a matrix.
    
    See U{http://en.wikipedia.org/wiki/Determinant}
    @rtype: float
    @return: Return a the determinant of a matrix.
    """

  def invert():
    """
    Set the matrix to its inverse.
    
    See U{http://en.wikipedia.org/wiki/Inverse_matrix}
    @return: an instance of itself.
    @raise ValueError: When matrix is singular.
    """

  def rotationPart():
    """
    Return the 3d submatrix corresponding to the linear term of the 
    embedded affine transformation in 3d. This matrix represents rotation
    and scale. Note that the (4,4) element of a matrix can be used for uniform
    scaling, too.
    @rtype: Matrix object.
    @return: Return the 3d matrix for rotation and scale.
    """

  def translationPart():
    """
    Return a the translation part of a 4 row matrix.
    @rtype: Vector object.
    @return: Return a the translation of a matrix.
    """

  def scalePart():
    """
    Return a the scale part of a 3x3 or 4x4 matrix.
    @note: This method does not return negative a scale on any axis because it is not possible to obtain this data from the matrix alone.
    @rtype: Vector object.
    @return: Return a the scale of a matrix.
    """

  def resize4x4():
    """
    Resize the matrix to by 4x4
    @return: an instance of itself.
    """
  
  def toEuler():
    """
    Return an Euler representation of the rotation matrix (3x3 or 4x4 matrix only).
    @rtype: Euler object
    @return: Euler representation of the rotation matrix.
    """

  def toQuat():
    """
    Return a quaternion representation of the rotation matrix
    @rtype: Quaternion object
    @return: Quaternion representation of the rotation matrix
    """

