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

def Rand (high = 1, low = 0):
  """
  Return a random number within a range.
  High and low represent the range from which the random
  number must return its result.
  @type high: float
  @param high: The upper range.
  @type low: float
  @param low: The lower range.
  """

def Vector (list = None):
  """
  Create a new Vector object from a list.
  @type list: PyList of float or int
  @param list: The list of values for the Vector object.
  Must be 2, 3, or 4 values.
  @rtype: Vector object.
  @return: It depends wheter a parameter was passed:
      - (list): Vector object initialized with the given values;
      - ():     An empty 3 dimensional vector.
  """

def CopyVec(vector):
  """
  Create a copy of the Vector object.
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
  Return the angle between two vectors.
  @type vec1: Vector object.
  @param vec1: A 2d or 3d vector.
  @type vec2: Vector object.
  @param vec2: A 2d or 3d vector.
  @rtype: float
  @return: The angle between the vectors in degrees.
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

def Matrix(list1 = None, list2 = None, list3 = None, list4 = None):
  """  
  Create a new matrix object from intialized values.
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
  """

def Quaternion(list = None, angle = None):
  """  
  Create a new matrix object from intialized values.
  @type list: PyList of int/float
  @param list: A 3d or 4d list to intialize quaternion.
  4d if intializing, 3d if will be used as an axis of rotation.
  @type angle: float (optional)
  @param angle: An arbitrary rotation amount around 'list'.
  List is used as an axis of rotation in this case.
  @rtype: New quaternion object.
  @return: It depends wheter a parameter was passed:
      - (list/angle): Quaternion object initialized with the given values;
      - ():     An identity 4 dimensional quaternion.
  """

def CopyQuat(quaternion):
  """
  Create a copy of the Quaternion object.
  @type quaternion: Quaternion object.
  @param quaternion: Quaternion to be copied.
  @rtype: Quaternion object.
  @return: A new quaternion object which is a copy of the one passed in.
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

def Euler(list = None):
  """
  Create a new euler object.
  @type list: PyList of float/int
  @param list: 3d list to initalize euler
  @rtype: Euler object
  @return: Euler representing heading, pitch, bank.
  Values are in degrees.
  """

def CopyEuler(euler):
  """
  Create a new euler object.
  @type euler: Euler object
  @param euler: The euler to copy
  @rtype: Euler object
  @return: A copy of the euler object passed in.
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
  @cvar x: The x value.
  @cvar y: The y value.
  @cvar z: The z value (if any).
  @cvar w: The w value (if any).
  @cvar length: The magnitude of the vector.
  """

  def zero():
    """
    Set all values to zero.
    """

  def normalize():
    """
    Normalize the vector.
    """

  def negate():
    """
    Set all values to their negative.
    """

  def resize2D():
    """
    Resize the vector to 2d.
    """

  def resize3D():
    """
    Resize the vector to 3d.
    """

  def resize4D():
    """
    Resize the vector to 4d.
    """

class Euler:
  """
  The Euler object
  ================
    This object gives access to Eulers in Blender.
  @cvar x: The heading value in degrees.
  @cvar y: The pitch value in degrees.
  @cvar z: The roll value in degrees.
  """

  def zero():
    """
    Set all values to zero.
    """

  def unique():
    """
    Calculate a unique rotation for this euler.
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
  @cvar w: The w value.
  @cvar x: The x value.
  @cvar y: The y value.
  @cvar z: The z value.
  @cvar magnitude: The magnitude of the quaternion.
  @cvar axis: Vector representing the axis of rotation.
  @cvar angle: A scalar representing the amount of rotation
  in degrees.
  """

  def identity():
    """
    Set the quaternion to the identity quaternion.
    """

  def negate():
    """
    Set the quaternion to it's negative.
    """

  def conjugate():
    """
    Set the quaternion to it's conjugate.
    """

  def inverse():
    """
    Set the quaternion to it's inverse
    """

  def normalize():
    """
    Normalize the quaternion.
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
  @cvar rowsize: The row size of the matrix.
  @cvar colsize: The column size of the matrix.
  """

  def zero():
    """
    Set all matrix values to 0.
    """

  def identity():
    """
    Set the matrix to the identity matrix.
    """

  def transpose():
    """
    Set the matrix to it's transpose.
    """

  def determinant():
    """
    Return a the determinant of a matrix.
    @rtype: int
    @return: Return a the determinant of a matrix.

    """

  def inverse():
    """
    Set the matrix to it's inverse.
    """

  def rotationPart():
    """
    Return the 3d rotation matrix.
    @rtype: Matrix object.
    @return: Return the 3d rotation matrix.

    """

  def translationPart():
    """
    Return a the translation part of a 4 row matrix.
    @rtype: Vector object.
    @return: Return a the translation of a matrix.

    """

  def resize4x4():
    """
    Resize the matrix to by 4x4
    """
  
  def toEuler():
    """
    Return a euler representing the rotation matrix.
    @rtype: Euler object
    @return: Return a euler representing the rotation matrix.

    """

  def toQuat():
    """
    Return a quaternion representation the rotation matrix
    @rtype: Quaternion object
    @return: Quaternion representation the rotation matrix

    """

