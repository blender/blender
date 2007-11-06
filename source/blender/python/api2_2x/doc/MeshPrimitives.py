# Blender.Mesh.Primitives module

"""
The Blender.Mesh.Primitives submodule.

B{New}:

Mesh Primitive Data
===================

This submodule provides access Blender's mesh primitives.  Each module 
function returns a BPy_Mesh object which wraps the mesh data.  This data can
then be manipulated using the L{Mesh} API.

Example::

  from Blender import *

  me = Mesh.Primitives.Cube(2.0)   # create a new cube of size 2
  sc = Scene.GetCurrent()          # get current scene
  sc.objects.new(me,'Mesh')        # add a new mesh-type object to the scene
  Window.RedrawAll()               # update windows
"""

def Plane(size=2.0):
  """
  Construct a filled planar mesh with 4 vertices.  The default size
  creates a 2 by 2 Blender unit plane, identical to the Blender UI.
  @type size: float
  @param size: optional size of the plane.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Cube(size=2.0):
  """
  Construct a cube mesh.  The default size creates a cube with each face
  2 by 2 Blender units, identical to the Blender UI.
  @type size: float
  @param size: optional size of the cube.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Circle(verts=32,diameter=2.0):
  """
  Construct a circle mesh.  The defaults create a circle with a
  diameter of 2 Blender units, identical to the Blender UI.
  @type verts: int
  @param verts: optional number of vertices for the circle.  
  Value must be in the range [3,100].
  @type diameter: float
  @param diameter: optional diameter of the circle.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Cylinder(verts=32, diameter=2.0, length=2.0):
  """
  Construct a cylindrical mesh (ends filled).  The defaults create a
  cylinder with a diameter of 2 Blender units and length 2 units,
  identical to the Blender UI.
  @type verts: int
  @param verts: optional number of vertices in the cylinder's perimeter.
  Value must be in the range [3,100].
  @type diameter: float
  @param diameter: optional diameter of the cylinder.
  @type length: float
  @param length: optional length of the cylinder.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Tube(verts=32, diameter=2.0, length=2.0):
  """
  Construct a cylindrical mesh (ends not filled).  The defaults create a
  cylinder with a diameter of 2 Blender units and length 2 units, identical
  to the Blender UI.
  @type verts: int
  @param verts: optional number of vertices in the tube's perimeter.
  Value must be in the range [3,100].
  @type diameter: float
  @param diameter: optional diameter of the tube.
  @type length: float
  @param length: optional length of the tube.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Cone(verts=32, diameter=2.0, length=2.0):
  """
  Construct a conic mesh (ends filled).  The defaulte create a cone with a
  base diameter of 2 Blender units and length 2 units, identical to
  the Blender UI.
  @type verts: int
  @param verts: optional number of vertices in the cone's perimeter.
  Value must be in the range [3,100].
  @type diameter: float
  @param diameter: optional diameter of the cone.
  @type length: float
  @param length: optional length of the cone.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Grid(xres=32, yres=32, size=2.0):
  """
  Construct a grid mesh.  The defaults create a 32 by 32 mesh of size 2
  Blender units, identical to the Blender UI.
  @type xres: int
  @param xres: optional grid size in the x direction.
  Value must be in the range [2,100].
  @type yres: int
  @param yres: optional grid size in the y direction.
  Value must be in the range [2,100].
  @type size: float
  @param size: optional size of the grid.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def UVsphere(segments=32, rings=32, diameter=2.0):
  """
  Construct a UV sphere mesh.  The defaults create a 32 by 32 sphere with
  a diameter of 2 Blender units, identical to the Blender UI.
  @type segments: int
  @param segments: optional number of longitudinal divisions.
  Value must be in the range [3,100].
  @type rings: int
  @param rings: optional number of latitudinal divisions.
  Value must be in the range [3,100].
  @type diameter: float
  @param diameter: optional diameter of the sphere.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Icosphere(subdivisions=2, diameter=2.0):
  """
  Construct a Icosphere mesh.  The defaults create sphere with 2 subdivisions
  and diameter of 2 Blender units, identical to the Blender UI.
  @type subdivisions: int
  @param subdivisions: optional number of subdivisions.
  Value must be in the range [2,5].
  @type diameter: float
  @param diameter: optional diameter of the sphere.
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

def Monkey():
  """
  Construct a Suzanne mesh.  
  @rtype: L{BPy_Mesh<Mesh>}
  @return: returns a mesh object.
  """

