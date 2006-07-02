# Blender.Geometry module and its subtypes

"""
The Blender.Geometry submodule.

Geometry
========

This new module provides access to a geometry function.
"""

def PolyFill(polylines):
  """
  Takes a list of polylines and calculates triangles that would fill in the polylines.
  Multiple lines can be used to make holes inside a polyline, or fill in 2 seperate lines at once.
  @type polylines: List of lists containing vectors, each representing a closed polyline.
  @rtype: list
  @return: a list if tuples each a tuple of 3 ints representing a triangle indexing the points given.
  @note: 2D Vectors will have an assumed Z axis of zero, 4D Vectors W axis is ignored.
  @note: The order of points in a polyline effect the direction returned triangles face, reverse the order of a polyline to flip the normal of returned faces.

  I{B{Example:}}

  The example below creates 2 polylines and fills them in with faces, then makes a mesh in the current scene::
    import Blender
    Vector= Blender.Mathutils.Vector

    # Outline of 5 points
    polyline1= [Vector(-2.0, 1.0, 1.0), Vector(-1.0, 2.0, 1.0), Vector(1.0, 2.0, 1.0), Vector(1.0, -1.0, 1.0), Vector(-1.0, -1.0, 1.0)]
    polyline2= [Vector(-1, 1, 1.0), Vector(0, 1, 1.0), Vector(0, 0, 1.0), Vector(-1.0, 0.0, 1.0)]
    fill= Blender.Geometry.PolyFill([polyline1, polyline2])

    # Make a new mesh and add the truangles into it
    me= Blender.Mesh.New()
    me.verts.extend(polyline1)
    me.verts.extend(polyline2)
    me.faces.extend(fill) # Add the faces, they reference the verts in polyline 1 and 2

    ob= Blender.Object.New('Mesh')
    ob.link(me)
    scn = Blender.Scene.GetCurrent()
    scn.link(ob)

    Blender.Redraw()
  """
