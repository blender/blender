# Blender.Mathutils module and its subtypes



class Vector:
  """
  
  @attention: Vector data can be wrapped or non-wrapped. When a object is wrapped it
  means that the object will give you direct access to the data inside of blender. Modification
  of this object will directly change the data inside of blender. To copy a wrapped object
  you need to use the object's constructor. If you copy and object by assignment you will not get
  a second copy but a second reference to the same data. Only certain functions will return 
  wrapped data. This will be indicated in the method description.
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

class Euler:
  """
  The Euler object
  ================
    This object gives access to Eulers in Blender.
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

class Quaternion:
  """
  The Quaternion object
  =====================
    This object gives access to Quaternions in Blender.
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

class Matrix:
  """
  The Matrix Object
  =================
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
