#  The Blender Edgetools is to bring CAD tools to Blender.
#  Copyright (C) 2012  Paul Marshall

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

bl_info = {
    "name": "EdgeTools",
    "author": "Paul Marshall",
    "version": (0, 9, 2),
    "blender": (2, 68, 0),
    "location": "View3D > Toolbar and View3D > Specials (W-key)",
    "warning": "",
    "description": "CAD style edge manipulation tools",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Modeling/EdgeTools",
    "category": "Mesh"}


import bpy
import bmesh
from bpy.types import (
        Operator,
        Menu,
        )
from math import acos, pi, radians, sqrt
from mathutils import Matrix, Vector
from mathutils.geometry import (
        distance_point_to_plane,
        interpolate_bezier,
        intersect_point_line,
        intersect_line_line,
        intersect_line_plane,
        )
from bpy.props import (
        BoolProperty,
        IntProperty,
        FloatProperty,
        EnumProperty,
       )

"""
Blender EdgeTools
This is a toolkit for edge manipulation based on mesh manipulation
abilities of several CAD/CAE packages, notably CATIA's Geometric Workbench
from which most of these tools have a functional basis.

The GUI and Blender add-on structure shamelessly coded in imitation of the
LoopTools addon.

Examples:
- "Ortho" inspired from CATIA's line creation tool which creates a line of a
   user specified length at a user specified angle to a curve at a chosen
   point.  The user then selects the plane the line is to be created in.
- "Shaft" is inspired from CATIA's tool of the same name.  However, instead
   of a curve around an axis, this will instead shaft a line, a point, or
   a fixed radius about the selected axis.
- "Slice" is from CATIA's ability to split a curve on a plane.  When
   completed this be a Python equivalent with all the same basic
   functionality, though it will sadly be a little clumsier to use due
   to Blender's selection limitations.

Notes:
- Fillet operator and related functions removed as they didn't work
- Buggy parts have been hidden behind ENABLE_DEBUG global (set it to True)
   Example: Shaft with more than two edges selected

Paul "BrikBot" Marshall
Created: January 28, 2012
Last Modified: October 6, 2012

Coded in IDLE, tested in Blender 2.6.
Search for "@todo" to quickly find sections that need work

Note: lijenstina - modified this script in preparation for merging
fixed the needless jumping to object mode for bmesh creation
causing the crash with the Slice > Rip operator
Removed the test operator since version 0.9.2
added general error handling
"""

# Enable debug
# Set to True to have the debug prints available
ENABLE_DEBUG = False


# Quick an dirty method for getting the sign of a number:
def sign(number):
    return (number > 0) - (number < 0)


# is_parallel
# Checks to see if two lines are parallel

def is_parallel(v1, v2, v3, v4):
    result = intersect_line_line(v1, v2, v3, v4)
    return result is None


# Handle error notifications
def error_handlers(self, op_name, error, reports="ERROR", func=False):
    if self and reports:
        self.report({'WARNING'}, reports + " (See Console for more info)")

    is_func = "Function" if func else "Operator"
    print("\n[Mesh EdgeTools]\n{}: {}\nError: {}\n".format(is_func, op_name, error))


def flip_edit_mode():
    bpy.ops.object.editmode_toggle()
    bpy.ops.object.editmode_toggle()


# check the appropriate selection condition
# to prevent crashes with the index out of range errors
# pass the bEdges and bVerts based selection tables here
# types: Edge, Vertex, All
def is_selected_enough(self, bEdges, bVerts, edges_n=1, verts_n=0, types="Edge"):
    check = False
    try:
        if bEdges and types == "Edge":
            check = (len(bEdges) >= edges_n)
        elif bVerts and types == "Vertex":
            check = (len(bVerts) >= verts_n)
        elif bEdges and bVerts and types == "All":
            check = (len(bEdges) >= edges_n and len(bVerts) >= verts_n)

        if check is False:
            strings = "%s Vertices and / or " % verts_n if verts_n != 0 else ""
            self.report({'WARNING'},
                        "Needs at least " + strings + "%s Edge(s) selected. "
                        "Operation Cancelled" % edges_n)
            flip_edit_mode()

        return check

    except Exception as e:
        error_handlers(self, "is_selected_enough", e,
                      "No appropriate selection. Operation Cancelled", func=True)
        return False

    return False


# is_axial
# This is for the special case where the edge is parallel to an axis.
# The projection onto the XY plane will fail so it will have to be handled differently

def is_axial(v1, v2, error=0.000002):
    vector = v2 - v1
    # Don't need to store, but is easier to read:
    vec0 = vector[0] > -error and vector[0] < error
    vec1 = vector[1] > -error and vector[1] < error
    vec2 = vector[2] > -error and vector[2] < error
    if (vec0 or vec1) and vec2:
        return 'Z'
    elif vec0 and vec1:
        return 'Y'
    return None


# is_same_co
# For some reason "Vector = Vector" does not seem to look at the actual coordinates

def is_same_co(v1, v2):
    if len(v1) != len(v2):
        return False
    else:
        for co1, co2 in zip(v1, v2):
            if co1 != co2:
                return False
    return True


def is_face_planar(face, error=0.0005):
    for v in face.verts:
        d = distance_point_to_plane(v.co, face.verts[0].co, face.normal)
        if ENABLE_DEBUG:
            print("Distance: " + str(d))
        if d < -error or d > error:
            return False
    return True


# other_joined_edges
# Starts with an edge. Then scans for linked, selected edges and builds a
# list with them in "order", starting at one end and moving towards the other

def order_joined_edges(edge, edges=[], direction=1):
    if len(edges) == 0:
        edges.append(edge)
        edges[0] = edge

    if ENABLE_DEBUG:
        print(edge, end=", ")
        print(edges, end=", ")
        print(direction, end="; ")

    # Robustness check: direction cannot be zero
    if direction == 0:
        direction = 1

    newList = []
    for e in edge.verts[0].link_edges:
        if e.select and edges.count(e) == 0:
            if direction > 0:
                edges.insert(0, e)
                newList.extend(order_joined_edges(e, edges, direction + 1))
                newList.extend(edges)
            else:
                edges.append(e)
                newList.extend(edges)
                newList.extend(order_joined_edges(e, edges, direction - 1))

    # This will only matter at the first level:
    direction = direction * -1

    for e in edge.verts[1].link_edges:
        if e.select and edges.count(e) == 0:
            if direction > 0:
                edges.insert(0, e)
                newList.extend(order_joined_edges(e, edges, direction + 2))
                newList.extend(edges)
            else:
                edges.append(e)
                newList.extend(edges)
                newList.extend(order_joined_edges(e, edges, direction))

    if ENABLE_DEBUG:
        print(newList, end=", ")
        print(direction)

    return newList


# --------------- GEOMETRY CALCULATION METHODS --------------

# distance_point_line
# I don't know why the mathutils.geometry API does not already have this, but
# it is trivial to code using the structures already in place. Instead of
# returning a float, I also want to know the direction vector defining the
# distance. Distance can be found with "Vector.length"

def distance_point_line(pt, line_p1, line_p2):
    int_co = intersect_point_line(pt, line_p1, line_p2)
    distance_vector = int_co[0] - pt
    return distance_vector


# interpolate_line_line
# This is an experiment into a cubic Hermite spline (c-spline) for connecting
# two edges with edges that obey the general equation.
# This will return a set of point coordinates (Vectors)
#
# A good, easy to read background on the mathematics can be found at:
# http://cubic.org/docs/hermite.htm
#
# Right now this is . . . less than functional :P
# @todo
#   - C-Spline and Bezier curves do not end on p2_co as they are supposed to.
#   - B-Spline just fails.  Epically.
#   - Add more methods as I come across them.  Who said flexibility was bad?

def interpolate_line_line(p1_co, p1_dir, p2_co, p2_dir, segments, tension=1,
                          typ='BEZIER', include_ends=False):
    pieces = []
    fraction = 1 / segments

    # Form: p1, tangent 1, p2, tangent 2
    if typ == 'HERMITE':
        poly = [[2, -3, 0, 1], [1, -2, 1, 0],
                [-2, 3, 0, 0], [1, -1, 0, 0]]
    elif typ == 'BEZIER':
        poly = [[-1, 3, -3, 1], [3, -6, 3, 0],
                [1, 0, 0, 0], [-3, 3, 0, 0]]
        p1_dir = p1_dir + p1_co
        p2_dir = -p2_dir + p2_co
    elif typ == 'BSPLINE':
        # Supposed poly matrix for a cubic b-spline:
        # poly = [[-1, 3, -3, 1], [3, -6, 3, 0],
        #         [-3, 0, 3, 0], [1, 4, 1, 0]]
        # My own invention to try to get something that somewhat acts right
        # This is semi-quadratic rather than fully cubic:
        poly = [[0, -1, 0, 1], [1, -2, 1, 0],
                [0, -1, 2, 0], [1, -1, 0, 0]]

    if include_ends:
        pieces.append(p1_co)

    # Generate each point:
    for i in range(segments - 1):
        t = fraction * (i + 1)
        if ENABLE_DEBUG:
            print(t)
        s = [t ** 3, t ** 2, t, 1]
        h00 = (poly[0][0] * s[0]) + (poly[0][1] * s[1]) + (poly[0][2] * s[2]) + (poly[0][3] * s[3])
        h01 = (poly[1][0] * s[0]) + (poly[1][1] * s[1]) + (poly[1][2] * s[2]) + (poly[1][3] * s[3])
        h10 = (poly[2][0] * s[0]) + (poly[2][1] * s[1]) + (poly[2][2] * s[2]) + (poly[2][3] * s[3])
        h11 = (poly[3][0] * s[0]) + (poly[3][1] * s[1]) + (poly[3][2] * s[2]) + (poly[3][3] * s[3])
        pieces.append((h00 * p1_co) + (h01 * p1_dir) + (h10 * p2_co) + (h11 * p2_dir))
    if include_ends:
        pieces.append(p2_co)

    # Return:
    if len(pieces) == 0:
        return None
    else:
        if ENABLE_DEBUG:
            print(pieces)
        return pieces


# intersect_line_face

# Calculates the coordinate of intersection of a line with a face.  It returns
# the coordinate if one exists, otherwise None.  It can only deal with tris or
# quads for a face. A quad does NOT have to be planar
"""
Quad math and theory:
A quad may not be planar. Therefore the treated definition of the surface is
that the surface is composed of all lines bridging two other lines defined by
the given four points. The lines do not "cross"

The two lines in 3-space can defined as:
┌  ┐         ┌   ┐     ┌   ┐  ┌  ┐         ┌   ┐     ┌   ┐
│x1│         │a11│     │b11│  │x2│         │a21│     │b21│
│y1│ = (1-t1)│a12│ + t1│b12│, │y2│ = (1-t2)│a22│ + t2│b22│
│z1│         │a13│     │b13│  │z2│         │a23│     │b23│
└  ┘         └   ┘     └   ┘  └  ┘         └   ┘     └   ┘
Therefore, the surface is the lines defined by every point alone the two
lines with a same "t" value (t1 = t2). This is basically R = V1 + tQ, where
Q = V2 - V1 therefore R = V1 + t(V2 - V1) -> R = (1 - t)V1 + tV2:
┌   ┐            ┌                  ┐      ┌                  ┐
│x12│            │(1-t)a11 + t * b11│      │(1-t)a21 + t * b21│
│y12│ = (1 - t12)│(1-t)a12 + t * b12│ + t12│(1-t)a22 + t * b22│
│z12│            │(1-t)a13 + t * b13│      │(1-t)a23 + t * b23│
└   ┘            └                  ┘      └                  ┘
Now, the equation of our line can be likewise defined:
┌  ┐   ┌   ┐     ┌   ┐
│x3│   │a31│     │b31│
│y3│ = │a32│ + t3│b32│
│z3│   │a33│     │b33│
└  ┘   └   ┘     └   ┘
Now we just have to find a valid solution for the two equations.  This should
be our point of intersection. Therefore, x12 = x3 -> x, y12 = y3 -> y,
z12 = z3 -> z.  Thus, to find that point we set the equation defining the
surface as equal to the equation for the line:
        ┌                  ┐      ┌                  ┐   ┌   ┐     ┌   ┐
        │(1-t)a11 + t * b11│      │(1-t)a21 + t * b21│   │a31│     │b31│
(1 - t12)│(1-t)a12 + t * b12│ + t12│(1-t)a22 + t * b22│ = │a32│ + t3│b32│
        │(1-t)a13 + t * b13│      │(1-t)a23 + t * b23│   │a33│     │b33│
        └                  ┘      └                  ┘   └   ┘     └   ┘
This leaves us with three equations, three unknowns.  Solving the system by
hand is practically impossible, but using Mathematica we are given an insane
series of three equations (not reproduced here for the sake of space: see
http://www.mediafire.com/file/cc6m6ba3sz2b96m/intersect_line_surface.nb and
http://www.mediafire.com/file/0egbr5ahg14talm/intersect_line_surface2.nb for
Mathematica computation).

Additionally, the resulting series of equations may result in a div by zero
exception if the line in question if parallel to one of the axis or if the
quad is planar and parallel to either the XY, XZ, or YZ planes. However, the
system is still solvable but must be dealt with a little differently to avaid
these special cases. Because the resulting equations are a little different,
we have to code them differently. 00Hence the special cases.

Tri math and theory:
A triangle must be planar (three points define a plane). So we just
have to make sure that the line intersects inside the triangle.

If the point is within the triangle, then the angle between the lines that
connect the point to the each individual point of the triangle will be
equal to 2 * PI. Otherwise, if the point is outside the triangle, then the
sum of the angles will be less.
"""
# @todo
# - Figure out how to deal with n-gons
# How the heck is a face with 8 verts definied mathematically?
# How do I then find the intersection point of a line with said vert?
# How do I know if that point is "inside" all the verts?
# I have no clue, and haven't been able to find anything on it so far
# Maybe if someone (actually reads this and) who knows could note?


def intersect_line_face(edge, face, is_infinite=False, error=0.000002):
    int_co = None

    # If we are dealing with a non-planar quad:
    if len(face.verts) == 4 and not is_face_planar(face):
        edgeA = face.edges[0]
        edgeB = None
        flipB = False

        for i in range(len(face.edges)):
            if face.edges[i].verts[0] not in edgeA.verts and \
               face.edges[i].verts[1] not in edgeA.verts:

                edgeB = face.edges[i]
                break

        # I haven't figured out a way to mix this in with the above. Doing so might remove a
        # few extra instructions from having to be executed saving a few clock cycles:
        for i in range(len(face.edges)):
            if face.edges[i] == edgeA or face.edges[i] == edgeB:
                continue
            if ((edgeA.verts[0] in face.edges[i].verts and
               edgeB.verts[1] in face.edges[i].verts) or
               (edgeA.verts[1] in face.edges[i].verts and edgeB.verts[0] in face.edges[i].verts)):

                flipB = True
                break

        # Define calculation coefficient constants:
        # "xx1" is the x coordinate, "xx2" is the y coordinate, and "xx3" is the z coordinate
        a11, a12, a13 = edgeA.verts[0].co[0], edgeA.verts[0].co[1], edgeA.verts[0].co[2]
        b11, b12, b13 = edgeA.verts[1].co[0], edgeA.verts[1].co[1], edgeA.verts[1].co[2]

        if flipB:
            a21, a22, a23 = edgeB.verts[1].co[0], edgeB.verts[1].co[1], edgeB.verts[1].co[2]
            b21, b22, b23 = edgeB.verts[0].co[0], edgeB.verts[0].co[1], edgeB.verts[0].co[2]
        else:
            a21, a22, a23 = edgeB.verts[0].co[0], edgeB.verts[0].co[1], edgeB.verts[0].co[2]
            b21, b22, b23 = edgeB.verts[1].co[0], edgeB.verts[1].co[1], edgeB.verts[1].co[2]
        a31, a32, a33 = edge.verts[0].co[0], edge.verts[0].co[1], edge.verts[0].co[2]
        b31, b32, b33 = edge.verts[1].co[0], edge.verts[1].co[1], edge.verts[1].co[2]

        # There are a bunch of duplicate "sub-calculations" inside the resulting
        # equations for t, t12, and t3.  Calculate them once and store them to
        # reduce computational time:
        m01 = a13 * a22 * a31
        m02 = a12 * a23 * a31
        m03 = a13 * a21 * a32
        m04 = a11 * a23 * a32
        m05 = a12 * a21 * a33
        m06 = a11 * a22 * a33
        m07 = a23 * a32 * b11
        m08 = a22 * a33 * b11
        m09 = a23 * a31 * b12
        m10 = a21 * a33 * b12
        m11 = a22 * a31 * b13
        m12 = a21 * a32 * b13
        m13 = a13 * a32 * b21
        m14 = a12 * a33 * b21
        m15 = a13 * a31 * b22
        m16 = a11 * a33 * b22
        m17 = a12 * a31 * b23
        m18 = a11 * a32 * b23
        m19 = a13 * a22 * b31
        m20 = a12 * a23 * b31
        m21 = a13 * a32 * b31
        m22 = a23 * a32 * b31
        m23 = a12 * a33 * b31
        m24 = a22 * a33 * b31
        m25 = a23 * b12 * b31
        m26 = a33 * b12 * b31
        m27 = a22 * b13 * b31
        m28 = a32 * b13 * b31
        m29 = a13 * b22 * b31
        m30 = a33 * b22 * b31
        m31 = a12 * b23 * b31
        m32 = a32 * b23 * b31
        m33 = a13 * a21 * b32
        m34 = a11 * a23 * b32
        m35 = a13 * a31 * b32
        m36 = a23 * a31 * b32
        m37 = a11 * a33 * b32
        m38 = a21 * a33 * b32
        m39 = a23 * b11 * b32
        m40 = a33 * b11 * b32
        m41 = a21 * b13 * b32
        m42 = a31 * b13 * b32
        m43 = a13 * b21 * b32
        m44 = a33 * b21 * b32
        m45 = a11 * b23 * b32
        m46 = a31 * b23 * b32
        m47 = a12 * a21 * b33
        m48 = a11 * a22 * b33
        m49 = a12 * a31 * b33
        m50 = a22 * a31 * b33
        m51 = a11 * a32 * b33
        m52 = a21 * a32 * b33
        m53 = a22 * b11 * b33
        m54 = a32 * b11 * b33
        m55 = a21 * b12 * b33
        m56 = a31 * b12 * b33
        m57 = a12 * b21 * b33
        m58 = a32 * b21 * b33
        m59 = a11 * b22 * b33
        m60 = a31 * b22 * b33
        m61 = a33 * b12 * b21
        m62 = a32 * b13 * b21
        m63 = a33 * b11 * b22
        m64 = a31 * b13 * b22
        m65 = a32 * b11 * b23
        m66 = a31 * b12 * b23
        m67 = b13 * b22 * b31
        m68 = b12 * b23 * b31
        m69 = b13 * b21 * b32
        m70 = b11 * b23 * b32
        m71 = b12 * b21 * b33
        m72 = b11 * b22 * b33
        n01 = m01 - m02 - m03 + m04 + m05 - m06
        n02 = -m07 + m08 + m09 - m10 - m11 + m12 + m13 - m14 - m15 + m16 + m17 - m18 - \
              m25 + m27 + m29 - m31 + m39 - m41 - m43 + m45 - m53 + m55 + m57 - m59
        n03 = -m19 + m20 + m33 - m34 - m47 + m48
        n04 = m21 - m22 - m23 + m24 - m35 + m36 + m37 - m38 + m49 - m50 - m51 + m52
        n05 = m26 - m28 - m30 + m32 - m40 + m42 + m44 - m46 + m54 - m56 - m58 + m60
        n06 = m61 - m62 - m63 + m64 + m65 - m66 - m67 + m68 + m69 - m70 - m71 + m72
        n07 = 2 * n01 + n02 + 2 * n03 + n04 + n05
        n08 = n01 + n02 + n03 + n06

        # Calculate t, t12, and t3:
        t = (n07 - sqrt(pow(-n07, 2) - 4 * (n01 + n03 + n04) * n08)) / (2 * n08)

        # t12 can be greatly simplified by defining it with t in it:
        # If block used to help prevent any div by zero error.
        t12 = 0

        if a31 == b31:
            # The line is parallel to the z-axis:
            if a32 == b32:
                t12 = ((a11 - a31) + (b11 - a11) * t) / ((a21 - a11) + (a11 - a21 - b11 + b21) * t)
            # The line is parallel to the y-axis:
            elif a33 == b33:
                t12 = ((a11 - a31) + (b11 - a11) * t) / ((a21 - a11) + (a11 - a21 - b11 + b21) * t)
            # The line is along the y/z-axis but is not parallel to either:
            else:
                t12 = -(-(a33 - b33) * (-a32 + a12 * (1 - t) + b12 * t) + (a32 - b32) *
                        (-a33 + a13 * (1 - t) + b13 * t)) / (-(a33 - b33) *
                        ((a22 - a12) * (1 - t) + (b22 - b12) * t) + (a32 - b32) *
                        ((a23 - a13) * (1 - t) + (b23 - b13) * t))
        elif a32 == b32:
            # The line is parallel to the x-axis:
            if a33 == b33:
                t12 = ((a12 - a32) + (b12 - a12) * t) / ((a22 - a12) + (a12 - a22 - b12 + b22) * t)
            # The line is along the x/z-axis but is not parallel to either:
            else:
                t12 = -(-(a33 - b33) * (-a31 + a11 * (1 - t) + b11 * t) + (a31 - b31) * (-a33 + a13 *
                      (1 - t) + b13 * t)) / (-(a33 - b33) * ((a21 - a11) * (1 - t) + (b21 - b11) * t) +
                      (a31 - b31) * ((a23 - a13) * (1 - t) + (b23 - b13) * t))
        # The line is along the x/y-axis but is not parallel to either:
        else:
            t12 = -(-(a32 - b32) * (-a31 + a11 * (1 - t) + b11 * t) + (a31 - b31) * (-a32 + a12 *
                  (1 - t) + b12 * t)) / (-(a32 - b32) * ((a21 - a11) * (1 - t) + (b21 - b11) * t) +
                  (a31 - b31) * ((a22 - a21) * (1 - t) + (b22 - b12) * t))

        # Likewise, t3 is greatly simplified by defining it in terms of t and t12:
        # If block used to prevent a div by zero error.
        t3 = 0
        if a31 != b31:
            t3 = (-a11 + a31 + (a11 - b11) * t + (a11 - a21) *
                t12 + (a21 - a11 + b11 - b21) * t * t12) / (a31 - b31)
        elif a32 != b32:
            t3 = (-a12 + a32 + (a12 - b12) * t + (a12 - a22) *
                t12 + (a22 - a12 + b12 - b22) * t * t12) / (a32 - b32)
        elif a33 != b33:
            t3 = (-a13 + a33 + (a13 - b13) * t + (a13 - a23) *
                t12 + (a23 - a13 + b13 - b23) * t * t12) / (a33 - b33)
        else:
            if ENABLE_DEBUG:
                print("The second edge is a zero-length edge")
            return None

        # Calculate the point of intersection:
        x = (1 - t3) * a31 + t3 * b31
        y = (1 - t3) * a32 + t3 * b32
        z = (1 - t3) * a33 + t3 * b33
        int_co = Vector((x, y, z))

        if ENABLE_DEBUG:
            print(int_co)

        # If the line does not intersect the quad, we return "None":
        if (t < -1 or t > 1 or t12 < -1 or t12 > 1) and not is_infinite:
            int_co = None

    elif len(face.verts) == 3:
        p1, p2, p3 = face.verts[0].co, face.verts[1].co, face.verts[2].co
        int_co = intersect_line_plane(edge.verts[0].co, edge.verts[1].co, p1, face.normal)

        # Only check if the triangle is not being treated as an infinite plane:
        # Math based from http://paulbourke.net/geometry/linefacet/
        if int_co is not None and not is_infinite:
            pA = p1 - int_co
            pB = p2 - int_co
            pC = p3 - int_co
            # These must be unit vectors, else we risk a domain error:
            pA.length = 1
            pB.length = 1
            pC.length = 1
            aAB = acos(pA.dot(pB))
            aBC = acos(pB.dot(pC))
            aCA = acos(pC.dot(pA))
            sumA = aAB + aBC + aCA

            # If the point is outside the triangle:
            if (sumA > (pi + error) and sumA < (pi - error)):
                int_co = None

    # This is the default case where we either have a planar quad or an n-gon
    else:
        int_co = intersect_line_plane(edge.verts[0].co, edge.verts[1].co,
                                      face.verts[0].co, face.normal)
    return int_co


# project_point_plane
# Projects a point onto a plane. Returns a tuple of the projection vector
# and the projected coordinate

def project_point_plane(pt, plane_co, plane_no):
    if ENABLE_DEBUG:
        print("project_point_plane was called")
    proj_co = intersect_line_plane(pt, pt + plane_no, plane_co, plane_no)
    proj_ve = proj_co - pt
    if ENABLE_DEBUG:
        print("project_point_plane: proj_co is {}\nproj_ve is {}".format(proj_co, proj_ve))
    return (proj_ve, proj_co)


# ------------ CHAMPHER HELPER METHODS -------------

def is_planar_edge(edge, error=0.000002):
    angle = edge.calc_face_angle()
    return ((angle < error and angle > -error) or
            (angle < (180 + error) and angle > (180 - error)))


# ------------- EDGE TOOL METHODS -------------------

# Extends an "edge" in two directions:
#   - Requires two vertices to be selected. They do not have to form an edge
#   - Extends "length" in both directions

class Extend(Operator):
    bl_idname = "mesh.edgetools_extend"
    bl_label = "Extend"
    bl_description = "Extend the selected edges of vertex pairs"
    bl_options = {'REGISTER', 'UNDO'}

    di1 = BoolProperty(
            name="Forwards",
            description="Extend the edge forwards",
            default=True
            )
    di2 = BoolProperty(
            name="Backwards",
            description="Extend the edge backwards",
            default=False
            )
    length = FloatProperty(
            name="Length",
            description="Length to extend the edge",
            min=0.0, max=1024.0,
            default=1.0
            )

    def draw(self, context):
        layout = self.layout

        row = layout.row(align=True)
        row.prop(self, "di1", toggle=True)
        row.prop(self, "di2", toggle=True)

        layout.prop(self, "length")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return(ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bEdges = bm.edges
            bVerts = bm.verts

            edges = [e for e in bEdges if e.select]
            verts = [v for v in bVerts if v.select]

            if not is_selected_enough(self, edges, 0, edges_n=1, verts_n=0, types="Edge"):
                return {'CANCELLED'}

            if len(edges) > 0:
                for e in edges:
                    vector = e.verts[0].co - e.verts[1].co
                    vector.length = self.length

                    if self.di1:
                        v = bVerts.new()
                        if (vector[0] + vector[1] + vector[2]) < 0:
                            v.co = e.verts[1].co - vector
                            newE = bEdges.new((e.verts[1], v))
                            bEdges.ensure_lookup_table()
                        else:
                            v.co = e.verts[0].co + vector
                            newE = bEdges.new((e.verts[0], v))
                            bEdges.ensure_lookup_table()
                    if self.di2:
                        v = bVerts.new()
                        if (vector[0] + vector[1] + vector[2]) < 0:
                            v.co = e.verts[0].co + vector
                            newE = bEdges.new((e.verts[0], v))
                            bEdges.ensure_lookup_table()
                        else:
                            v.co = e.verts[1].co - vector
                            newE = bEdges.new((e.verts[1], v))
                            bEdges.ensure_lookup_table()
            else:
                vector = verts[0].co - verts[1].co
                vector.length = self.length

                if self.di1:
                    v = bVerts.new()
                    if (vector[0] + vector[1] + vector[2]) < 0:
                        v.co = verts[1].co - vector
                        e = bEdges.new((verts[1], v))
                        bEdges.ensure_lookup_table()
                    else:
                        v.co = verts[0].co + vector
                        e = bEdges.new((verts[0], v))
                        bEdges.ensure_lookup_table()
                if self.di2:
                    v = bVerts.new()
                    if (vector[0] + vector[1] + vector[2]) < 0:
                        v.co = verts[0].co + vector
                        e = bEdges.new((verts[0], v))
                        bEdges.ensure_lookup_table()
                    else:
                        v.co = verts[1].co - vector
                        e = bEdges.new((verts[1], v))
                        bEdges.ensure_lookup_table()

            bmesh.update_edit_mesh(me)

        except Exception as e:
            error_handlers(self, "mesh.edgetools_extend", e,
                           reports="Extend Operator failed", func=False)
            return {'CANCELLED'}

        return {'FINISHED'}


# Creates a series of edges between two edges using spline interpolation.
# This basically just exposes existing functionality in addition to some
# other common methods: Hermite (c-spline), Bezier, and b-spline. These
# alternates I coded myself after some extensive research into spline theory
#
# @todo Figure out what's wrong with the Blender bezier interpolation

class Spline(Operator):
    bl_idname = "mesh.edgetools_spline"
    bl_label = "Spline"
    bl_description = "Create a spline interplopation between two edges"
    bl_options = {'REGISTER', 'UNDO'}

    alg = EnumProperty(
            name="Spline Algorithm",
            items=[('Blender', "Blender", "Interpolation provided through mathutils.geometry"),
                    ('Hermite', "C-Spline", "C-spline interpolation"),
                    ('Bezier', "Bezier", "Bezier interpolation"),
                    ('B-Spline', "B-Spline", "B-Spline interpolation")],
            default='Bezier'
            )
    segments = IntProperty(
            name="Segments",
            description="Number of segments to use in the interpolation",
            min=2, max=4096,
            soft_max=1024,
            default=32
            )
    flip1 = BoolProperty(
            name="Flip Edge",
            description="Flip the direction of the spline on Edge 1",
            default=False
            )
    flip2 = BoolProperty(
            name="Flip Edge",
            description="Flip the direction of the spline on Edge 2",
            default=False
            )
    ten1 = FloatProperty(
            name="Tension",
            description="Tension on Edge 1",
            min=-4096.0, max=4096.0,
            soft_min=-8.0, soft_max=8.0,
            default=1.0
            )
    ten2 = FloatProperty(
            name="Tension",
            description="Tension on Edge 2",
            min=-4096.0, max=4096.0,
            soft_min=-8.0, soft_max=8.0,
            default=1.0
            )

    def draw(self, context):
        layout = self.layout

        layout.prop(self, "alg")
        layout.prop(self, "segments")

        layout.label("Edge 1:")
        split = layout.split(percentage=0.8, align=True)
        split.prop(self, "ten1")
        split.prop(self, "flip1", text="", icon="ALIGN", toggle=True)

        layout.label("Edge 2:")
        split = layout.split(percentage=0.8, align=True)
        split.prop(self, "ten2")
        split.prop(self, "flip2", text="", icon="ALIGN", toggle=True)

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return(ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bEdges = bm.edges
            bVerts = bm.verts

            seg = self.segments
            edges = [e for e in bEdges if e.select]

            if not is_selected_enough(self, edges, 0, edges_n=2, verts_n=0, types="Edge"):
                return {'CANCELLED'}

            verts = [edges[v // 2].verts[v % 2] for v in range(4)]

            if self.flip1:
                v1 = verts[1]
                p1_co = verts[1].co
                p1_dir = verts[1].co - verts[0].co
            else:
                v1 = verts[0]
                p1_co = verts[0].co
                p1_dir = verts[0].co - verts[1].co
            if self.ten1 < 0:
                p1_dir = -1 * p1_dir
                p1_dir.length = -self.ten1
            else:
                p1_dir.length = self.ten1

            if self.flip2:
                v2 = verts[3]
                p2_co = verts[3].co
                p2_dir = verts[2].co - verts[3].co
            else:
                v2 = verts[2]
                p2_co = verts[2].co
                p2_dir = verts[3].co - verts[2].co
            if self.ten2 < 0:
                p2_dir = -1 * p2_dir
                p2_dir.length = -self.ten2
            else:
                p2_dir.length = self.ten2

            # Get the interploted coordinates:
            if self.alg == 'Blender':
                pieces = interpolate_bezier(
                                p1_co, p1_dir, p2_dir, p2_co, self.segments
                                )
            elif self.alg == 'Hermite':
                pieces = interpolate_line_line(
                                p1_co, p1_dir, p2_co, p2_dir, self.segments, 1, 'HERMITE'
                                )
            elif self.alg == 'Bezier':
                pieces = interpolate_line_line(
                                p1_co, p1_dir, p2_co, p2_dir, self.segments, 1, 'BEZIER'
                                )
            elif self.alg == 'B-Spline':
                pieces = interpolate_line_line(
                                p1_co, p1_dir, p2_co, p2_dir, self.segments, 1, 'BSPLINE'
                                )

            verts = []
            verts.append(v1)
            # Add vertices and set the points:
            for i in range(seg - 1):
                v = bVerts.new()
                v.co = pieces[i]
                bVerts.ensure_lookup_table()
                verts.append(v)
            verts.append(v2)
            # Connect vertices:
            for i in range(seg):
                e = bEdges.new((verts[i], verts[i + 1]))
                bEdges.ensure_lookup_table()

            bmesh.update_edit_mesh(me)

        except Exception as e:
            error_handlers(self, "mesh.edgetools_spline", e,
                           reports="Spline Operator failed", func=False)
            return {'CANCELLED'}

        return {'FINISHED'}


# Creates edges normal to planes defined between each of two edges and the
# normal or the plane defined by those two edges.
#   - Select two edges.  The must form a plane.
#   - On running the script, eight edges will be created.  Delete the
#     extras that you don't need.
#   - The length of those edges is defined by the variable "length"
#
# @todo Change method from a cross product to a rotation matrix to make the
#   angle part work.
#   --- todo completed 2/4/2012, but still needs work ---
# @todo Figure out a way to make +/- predictable
#   - Maybe use angle between edges and vector direction definition?
#   --- TODO COMPLETED ON 2/9/2012 ---

class Ortho(Operator):
    bl_idname = "mesh.edgetools_ortho"
    bl_label = "Angle Off Edge"
    bl_description = "Creates new edges within an angle from vertices of selected edges"
    bl_options = {'REGISTER', 'UNDO'}

    vert1 = BoolProperty(
            name="Vertice 1",
            description="Enable edge creation for Vertice 1",
            default=True
            )
    vert2 = BoolProperty(
            name="Vertice 2",
            description="Enable edge creation for Vertice 2",
            default=True
            )
    vert3 = BoolProperty(
            name="Vertice 3",
            description="Enable edge creation for Vertice 3",
            default=True
            )
    vert4 = BoolProperty(
            name="Vertice 4",
            description="Enable edge creation for Vertice 4",
            default=True
            )
    pos = BoolProperty(
            name="Positive",
            description="Enable creation of positive direction edges",
            default=True
            )
    neg = BoolProperty(
            name="Negative",
            description="Enable creation of negative direction edges",
            default=True
            )
    angle = FloatProperty(
            name="Angle",
            description="Define the angle off of the originating edge",
            min=0.0, max=180.0,
            default=90.0
            )
    length = FloatProperty(
            name="Length",
            description="Length of created edges",
            min=0.0, max=1024.0,
            default=1.0
            )
    # For when only one edge is selected (Possible feature to be testd):
    plane = EnumProperty(
            name="Plane",
            items=[("XY", "X-Y Plane", "Use the X-Y plane as the plane of creation"),
                   ("XZ", "X-Z Plane", "Use the X-Z plane as the plane of creation"),
                   ("YZ", "Y-Z Plane", "Use the Y-Z plane as the plane of creation")],
            default="XY"
            )

    def draw(self, context):
        layout = self.layout

        layout.label("Creation:")
        split = layout.split()
        col = split.column()

        col.prop(self, "vert1", toggle=True)
        col.prop(self, "vert2", toggle=True)

        col = split.column()
        col.prop(self, "vert3", toggle=True)
        col.prop(self, "vert4", toggle=True)

        layout.label("Direction:")
        row = layout.row(align=False)
        row.alignment = 'EXPAND'
        row.prop(self, "pos")
        row.prop(self, "neg")

        layout.separator()

        col = layout.column(align=True)
        col.prop(self, "angle")
        col.prop(self, "length")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return(ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bVerts = bm.verts
            bEdges = bm.edges
            edges = [e for e in bEdges if e.select]
            vectors = []

            if not is_selected_enough(self, edges, 0, edges_n=2, verts_n=0, types="Edge"):
                return {'CANCELLED'}

            verts = [edges[0].verts[0],
                     edges[0].verts[1],
                     edges[1].verts[0],
                     edges[1].verts[1]]

            cos = intersect_line_line(verts[0].co, verts[1].co, verts[2].co, verts[3].co)

            # If the two edges are parallel:
            if cos is None:
                self.report({'WARNING'},
                            "Selected lines are parallel: results may be unpredictable")
                vectors.append(verts[0].co - verts[1].co)
                vectors.append(verts[0].co - verts[2].co)
                vectors.append(vectors[0].cross(vectors[1]))
                vectors.append(vectors[2].cross(vectors[0]))
                vectors.append(-vectors[3])
            else:
                # Warn the user if they have not chosen two planar edges:
                if not is_same_co(cos[0], cos[1]):
                    self.report({'WARNING'},
                                "Selected lines are not planar: results may be unpredictable")

                # This makes the +/- behavior predictable:
                if (verts[0].co - cos[0]).length < (verts[1].co - cos[0]).length:
                    verts[0], verts[1] = verts[1], verts[0]
                if (verts[2].co - cos[0]).length < (verts[3].co - cos[0]).length:
                    verts[2], verts[3] = verts[3], verts[2]

                vectors.append(verts[0].co - verts[1].co)
                vectors.append(verts[2].co - verts[3].co)

                # Normal of the plane formed by vector1 and vector2:
                vectors.append(vectors[0].cross(vectors[1]))

                # Possible directions:
                vectors.append(vectors[2].cross(vectors[0]))
                vectors.append(vectors[1].cross(vectors[2]))

            # Set the length:
            vectors[3].length = self.length
            vectors[4].length = self.length

            # Perform any additional rotations:
            matrix = Matrix.Rotation(radians(90 + self.angle), 3, vectors[2])
            vectors.append(matrix * -vectors[3])    # vectors[5]
            matrix = Matrix.Rotation(radians(90 - self.angle), 3, vectors[2])
            vectors.append(matrix * vectors[4])     # vectors[6]
            vectors.append(matrix * vectors[3])     # vectors[7]
            matrix = Matrix.Rotation(radians(90 + self.angle), 3, vectors[2])
            vectors.append(matrix * -vectors[4])    # vectors[8]

            # Perform extrusions and displacements:
            # There will be a total of 8 extrusions.  One for each vert of each edge.
            # It looks like an extrusion will add the new vert to the end of the verts
            # list and leave the rest in the same location.
            # -- EDIT --
            # It looks like I might be able to do this within "bpy.data" with the ".add" function

            for v in range(len(verts)):
                vert = verts[v]
                if ((v == 0 and self.vert1) or (v == 1 and self.vert2) or
                   (v == 2 and self.vert3) or (v == 3 and self.vert4)):

                    if self.pos:
                        new = bVerts.new()
                        new.co = vert.co - vectors[5 + (v // 2) + ((v % 2) * 2)]
                        bVerts.ensure_lookup_table()
                        bEdges.new((vert, new))
                        bEdges.ensure_lookup_table()
                    if self.neg:
                        new = bVerts.new()
                        new.co = vert.co + vectors[5 + (v // 2) + ((v % 2) * 2)]
                        bVerts.ensure_lookup_table()
                        bEdges.new((vert, new))
                        bEdges.ensure_lookup_table()

            bmesh.update_edit_mesh(me)
        except Exception as e:
            error_handlers(self, "mesh.edgetools_ortho", e,
                           reports="Angle Off Edge Operator failed", func=False)
            return {'CANCELLED'}

        return {'FINISHED'}


# Usage:
# Select an edge and a point or an edge and specify the radius (default is 1 BU)
# You can select two edges but it might be unpredictable which edge it revolves
# around so you might have to play with the switch

class Shaft(Operator):
    bl_idname = "mesh.edgetools_shaft"
    bl_label = "Shaft"
    bl_description = "Create a shaft mesh around an axis"
    bl_options = {'REGISTER', 'UNDO'}

    # Selection defaults:
    shaftType = 0

    # For tracking if the user has changed selection:
    last_edge = IntProperty(
            name="Last Edge",
            description="Tracks if user has changed selected edges",
            min=0, max=1,
            default=0
            )
    last_flip = False

    edge = IntProperty(
            name="Edge",
            description="Edge to shaft around",
            min=0, max=1,
            default=0
            )
    flip = BoolProperty(
            name="Flip Second Edge",
            description="Flip the percieved direction of the second edge",
            default=False
            )
    radius = FloatProperty(
            name="Radius",
            description="Shaft Radius",
            min=0.0, max=1024.0,
            default=1.0
            )
    start = FloatProperty(
            name="Starting Angle",
            description="Angle to start the shaft at",
            min=-360.0, max=360.0,
            default=0.0
            )
    finish = FloatProperty(
            name="Ending Angle",
            description="Angle to end the shaft at",
            min=-360.0, max=360.0,
            default=360.0
            )
    segments = IntProperty(
            name="Shaft Segments",
            description="Number of segments to use in the shaft",
            min=1, max=4096,
            soft_max=512,
            default=32
            )

    def draw(self, context):
        layout = self.layout

        if self.shaftType == 0:
            layout.prop(self, "edge")
            layout.prop(self, "flip")
        elif self.shaftType == 3:
            layout.prop(self, "radius")

        layout.prop(self, "segments")
        layout.prop(self, "start")
        layout.prop(self, "finish")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return(ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        # Make sure these get reset each time we run:
        self.last_edge = 0
        self.edge = 0

        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bFaces = bm.faces
            bEdges = bm.edges
            bVerts = bm.verts

            active = None
            edges, verts = [], []

            # Pre-caclulated values:
            rotRange = [radians(self.start), radians(self.finish)]
            rads = radians((self.finish - self.start) / self.segments)

            numV = self.segments + 1
            numE = self.segments

            edges = [e for e in bEdges if e.select]

            # Robustness check: there should at least be one edge selected
            if not is_selected_enough(self, edges, 0, edges_n=1, verts_n=0, types="Edge"):
                return {'CANCELLED'}

            # If two edges are selected:
            if len(edges) == 2:
                # default:
                edge = [0, 1]
                vert = [0, 1]

                # By default, we want to shaft around the last selected edge (it
                # will be the active edge). We know we are using the default if
                # the user has not changed which edge is being shafted around (as
                # is tracked by self.last_edge). When they are not the same, then
                # the user has changed selection.
                # We then need to make sure that the active object really is an edge
                # (robustness check)
                # Finally, if the active edge is not the inital one, we flip them
                # and have the GUI reflect that
                if self.last_edge == self.edge:
                    if isinstance(bm.select_history.active, bmesh.types.BMEdge):
                        if bm.select_history.active != edges[edge[0]]:
                            self.last_edge, self.edge = edge[1], edge[1]
                            edge = [edge[1], edge[0]]
                    else:
                        flip_edit_mode()
                        self.report({'WARNING'},
                                    "Active geometry is not an edge. Operation Cancelled")
                        return {'CANCELLED'}
                elif self.edge == 1:
                    edge = [1, 0]

                verts.append(edges[edge[0]].verts[0])
                verts.append(edges[edge[0]].verts[1])

                if self.flip:
                    verts = [1, 0]

                verts.append(edges[edge[1]].verts[vert[0]])
                verts.append(edges[edge[1]].verts[vert[1]])

                self.shaftType = 0
            # If there is more than one edge selected:
            # There are some issues with it ATM, so don't expose is it to normal users
            # @todo Fix edge connection ordering issue
            elif ENABLE_DEBUG and len(edges) > 2:
                if isinstance(bm.select_history.active, bmesh.types.BMEdge):
                    active = bm.select_history.active
                    edges.remove(active)
                    # Get all the verts:
                    # edges = order_joined_edges(edges[0])
                    verts = []
                    for e in edges:
                        if verts.count(e.verts[0]) == 0:
                            verts.append(e.verts[0])
                        if verts.count(e.verts[1]) == 0:
                            verts.append(e.verts[1])
                else:
                    flip_edit_mode()
                    self.report({'WARNING'},
                                "Active geometry is not an edge. Operation Cancelled")
                    return {'CANCELLED'}
                self.shaftType = 1
            else:
                verts.append(edges[0].verts[0])
                verts.append(edges[0].verts[1])

                for v in bVerts:
                    if v.select and verts.count(v) == 0:
                        verts.append(v)
                    v.select = False
                if len(verts) == 2:
                    self.shaftType = 3
                else:
                    self.shaftType = 2

            # The vector denoting the axis of rotation:
            if self.shaftType == 1:
                axis = active.verts[1].co - active.verts[0].co
            else:
                axis = verts[1].co - verts[0].co

            # We will need a series of rotation matrices. We could use one which
            # would be faster but also might cause propagation of error
            # matrices = []
            # for i in range(numV):
            #    matrices.append(Matrix.Rotation((rads * i) + rotRange[0], 3, axis))
            matrices = [Matrix.Rotation((rads * i) + rotRange[0], 3, axis) for i in range(numV)]

            # New vertice coordinates:
            verts_out = []

            # If two edges were selected:
            #  - If the lines are not parallel, then it will create a cone-like shaft
            if self.shaftType == 0:
                for i in range(len(verts) - 2):
                    init_vec = distance_point_line(verts[i + 2].co, verts[0].co, verts[1].co)
                    co = init_vec + verts[i + 2].co
                    # These will be rotated about the orgin so will need to be shifted:
                    for j in range(numV):
                        verts_out.append(co - (matrices[j] * init_vec))
            elif self.shaftType == 1:
                for i in verts:
                    init_vec = distance_point_line(i.co, active.verts[0].co, active.verts[1].co)
                    co = init_vec + i.co
                    # These will be rotated about the orgin so will need to be shifted:
                    for j in range(numV):
                        verts_out.append(co - (matrices[j] * init_vec))
            # Else if a line and a point was selected:
            elif self.shaftType == 2:
                init_vec = distance_point_line(verts[2].co, verts[0].co, verts[1].co)
                # These will be rotated about the orgin so will need to be shifted:
                verts_out = [
                    (verts[i].co - (matrices[j] * init_vec)) for i in range(2) for j in range(numV)
                    ]
            else:
                # Else the above are not possible, so we will just use the edge:
                #  - The vector defined by the edge is the normal of the plane for the shaft
                #  - The shaft will have radius "radius"
                if is_axial(verts[0].co, verts[1].co) is None:
                    proj = (verts[1].co - verts[0].co)
                    proj[2] = 0
                    norm = proj.cross(verts[1].co - verts[0].co)
                    vec = norm.cross(verts[1].co - verts[0].co)
                    vec.length = self.radius
                elif is_axial(verts[0].co, verts[1].co) == 'Z':
                    vec = verts[0].co + Vector((0, 0, self.radius))
                else:
                    vec = verts[0].co + Vector((0, self.radius, 0))
                init_vec = distance_point_line(vec, verts[0].co, verts[1].co)
                # These will be rotated about the orgin so will need to be shifted:
                verts_out = [
                    (verts[i].co - (matrices[j] * init_vec)) for i in range(2) for j in range(numV)
                    ]

            # We should have the coordinates for a bunch of new verts
            # Now add the verts and build the edges and then the faces

            newVerts = []

            if self.shaftType == 1:
                # Vertices:
                for i in range(numV * len(verts)):
                    new = bVerts.new()
                    new.co = verts_out[i]
                    bVerts.ensure_lookup_table()
                    new.select = True
                    newVerts.append(new)
                # Edges:
                for i in range(numE):
                    for j in range(len(verts)):
                        e = bEdges.new((newVerts[i + (numV * j)], newVerts[i + (numV * j) + 1]))
                        bEdges.ensure_lookup_table()
                        e.select = True
                for i in range(numV):
                    for j in range(len(verts) - 1):
                        e = bEdges.new((newVerts[i + (numV * j)], newVerts[i + (numV * (j + 1))]))
                        bEdges.ensure_lookup_table()
                        e.select = True

                # Faces: There is a problem with this right now
                """
                for i in range(len(edges)):
                    for j in range(numE):
                        f = bFaces.new((newVerts[i], newVerts[i + 1],
                                       newVerts[i + (numV * j) + 1], newVerts[i + (numV * j)]))
                        f.normal_update()
                """
            else:
                # Vertices:
                for i in range(numV * 2):
                    new = bVerts.new()
                    new.co = verts_out[i]
                    new.select = True
                    bVerts.ensure_lookup_table()
                    newVerts.append(new)
                # Edges:
                for i in range(numE):
                    e = bEdges.new((newVerts[i], newVerts[i + 1]))
                    e.select = True
                    bEdges.ensure_lookup_table()
                    e = bEdges.new((newVerts[i + numV], newVerts[i + numV + 1]))
                    e.select = True
                    bEdges.ensure_lookup_table()
                for i in range(numV):
                    e = bEdges.new((newVerts[i], newVerts[i + numV]))
                    e.select = True
                    bEdges.ensure_lookup_table()
                # Faces:
                for i in range(numE):
                    f = bFaces.new((newVerts[i], newVerts[i + 1],
                                    newVerts[i + numV + 1], newVerts[i + numV]))
                    bFaces.ensure_lookup_table()
                    f.normal_update()

            bmesh.update_edit_mesh(me)

        except Exception as e:
            error_handlers(self, "mesh.edgetools_shaft", e,
                           reports="Shaft Operator failed", func=False)
            return {'CANCELLED'}

        return {'FINISHED'}


# "Slices" edges crossing a plane defined by a face

class Slice(Operator):
    bl_idname = "mesh.edgetools_slice"
    bl_label = "Slice"
    bl_description = "Cut edges at the plane defined by a selected face"
    bl_options = {'REGISTER', 'UNDO'}

    make_copy = BoolProperty(
            name="Make Copy",
            description="Make new vertices at intersection points instead of spliting the edge",
            default=False
            )
    rip = BoolProperty(
            name="Rip",
            description="Split into two edges that DO NOT share an intersection vertex",
            default=True
            )
    pos = BoolProperty(
            name="Positive",
            description="Remove the portion on the side of the face normal",
            default=False
            )
    neg = BoolProperty(
            name="Negative",
            description="Remove the portion on the side opposite of the face normal",
            default=False
            )

    def draw(self, context):
        layout = self.layout

        layout.prop(self, "make_copy")
        if not self.make_copy:
            layout.prop(self, "rip")
            layout.label("Remove Side:")
            layout.prop(self, "pos")
            layout.prop(self, "neg")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return(ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bVerts = bm.verts
            bEdges = bm.edges
            bFaces = bm.faces

            face, normal = None, None

            # Find the selected face. This will provide the plane to project onto:
            #  - First check to use the active face. Allows users to just
            #    select a bunch of faces with the last being the cutting plane
            #  - If that fails, then use the first found selected face in the BMesh face list
            if isinstance(bm.select_history.active, bmesh.types.BMFace):
                face = bm.select_history.active
                normal = bm.select_history.active.normal
                bm.select_history.active.select = False
            else:
                for f in bFaces:
                    if f.select:
                        face = f
                        normal = f.normal
                        f.select = False
                        break

            # If we don't find a selected face exit:
            if face is None:
                flip_edit_mode()
                self.report({'WARNING'},
                            "Please select a face as the cutting plane. Operation Cancelled")
                return {'CANCELLED'}

            # Warn the user if they are using an n-gon might lead to some odd results
            elif len(face.verts) > 4 and not is_face_planar(face):
                self.report({'WARNING'},
                            "Selected face is an N-gon.  Results may be unpredictable")

            if ENABLE_DEBUG:
                dbg = 0
                print("Number of Edges: ", len(bEdges))

            for e in bEdges:
                if ENABLE_DEBUG:
                    print("Looping through Edges - ", dbg)
                    dbg = dbg + 1

                # Get the end verts on the edge:
                v1 = e.verts[0]
                v2 = e.verts[1]

                # Make sure that verts are not a part of the cutting plane:
                if e.select and (v1 not in face.verts and v2 not in face.verts):
                    if len(face.verts) < 5:  # Not an n-gon
                        intersection = intersect_line_face(e, face, True)
                    else:
                        intersection = intersect_line_plane(v1.co, v2.co, face.verts[0].co, normal)

                    if ENABLE_DEBUG:
                        print("Intersection: ", intersection)

                    # If an intersection exists find the distance of each of the end
                    # points from the plane, with "positive" being in the direction
                    # of the cutting plane's normal. If the points are on opposite
                    # side of the plane, then it intersects and we need to cut it
                    if intersection is not None:
                        bVerts.ensure_lookup_table()
                        bEdges.ensure_lookup_table()
                        bFaces.ensure_lookup_table()

                        d1 = distance_point_to_plane(v1.co, face.verts[0].co, normal)
                        d2 = distance_point_to_plane(v2.co, face.verts[0].co, normal)
                        # If they have different signs, then the edge crosses the cutting plane:
                        if abs(d1 + d2) < abs(d1 - d2):
                            # Make the first vertex the positive one:
                            if d1 < d2:
                                v2, v1 = v1, v2

                            if self.make_copy:
                                new = bVerts.new()
                                new.co = intersection
                                new.select = True
                                bVerts.ensure_lookup_table()
                            elif self.rip:
                                if ENABLE_DEBUG:
                                    print("Branch rip engaged")
                                newV1 = bVerts.new()
                                newV1.co = intersection
                                bVerts.ensure_lookup_table()
                                if ENABLE_DEBUG:
                                    print("newV1 created", end='; ')

                                newV2 = bVerts.new()
                                newV2.co = intersection
                                bVerts.ensure_lookup_table()

                                if ENABLE_DEBUG:
                                    print("newV2 created", end='; ')

                                newE1 = bEdges.new((v1, newV1))
                                newE2 = bEdges.new((v2, newV2))
                                bEdges.ensure_lookup_table()

                                if ENABLE_DEBUG:
                                    print("new edges created", end='; ')

                                if e.is_valid:
                                    bEdges.remove(e)

                                bEdges.ensure_lookup_table()

                                if ENABLE_DEBUG:
                                    print("Old edge removed.\nWe're done with this edge")
                            else:
                                new = list(bmesh.utils.edge_split(e, v1, 0.5))
                                bEdges.ensure_lookup_table()
                                new[1].co = intersection
                                e.select = False
                                new[0].select = False
                                if self.pos:
                                    bEdges.remove(new[0])
                                if self.neg:
                                    bEdges.remove(e)
                                bEdges.ensure_lookup_table()

            if ENABLE_DEBUG:
                print("The Edge Loop has exited. Now to update the bmesh")
                dbg = 0

            bmesh.update_edit_mesh(me)

        except Exception as e:
            error_handlers(self, "mesh.edgetools_slice", e,
                           reports="Slice Operator failed", func=False)
            return {'CANCELLED'}

        return {'FINISHED'}


# This projects the selected edges onto the selected plane
# and/or both points on the selected edge

class Project(Operator):
    bl_idname = "mesh.edgetools_project"
    bl_label = "Project"
    bl_description = ("Projects the selected Vertices/Edges onto a selected plane\n"
                      "(Active is projected onto the rest)")
    bl_options = {'REGISTER', 'UNDO'}

    make_copy = BoolProperty(
            name="Make Copy",
            description="Make duplicates of the vertices instead of altering them",
            default=False
            )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "make_copy")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bFaces = bm.faces
            bVerts = bm.verts

            fVerts = []

            # Find the selected face.  This will provide the plane to project onto:
            # @todo Check first for an active face
            for f in bFaces:
                if f.select:
                    for v in f.verts:
                        fVerts.append(v)
                    normal = f.normal
                    f.select = False
                    break

            for v in bVerts:
                if v.select:
                    if v in fVerts:
                        v.select = False
                        continue
                    d = distance_point_to_plane(v.co, fVerts[0].co, normal)
                    if self.make_copy:
                        temp = v
                        v = bVerts.new()
                        v.co = temp.co
                        bVerts.ensure_lookup_table()
                    vector = normal
                    vector.length = abs(d)
                    v.co = v.co - (vector * sign(d))
                    v.select = False

            bmesh.update_edit_mesh(me)

        except Exception as e:
            error_handlers(self, "mesh.edgetools_project", e,
                           reports="Project Operator failed", func=False)

            return {'CANCELLED'}

        return {'FINISHED'}


# Project_End is for projecting/extending an edge to meet a plane
# This is used be selecting a face to define the plane then all the edges
# Then move the vertices in the edge that is closest to the
# plane to the coordinates of the intersection of the edge and the plane

class Project_End(Operator):
    bl_idname = "mesh.edgetools_project_end"
    bl_label = "Project (End Point)"
    bl_description = ("Projects the vertices of the selected\n"
                      "edges closest to a plane onto that plane")
    bl_options = {'REGISTER', 'UNDO'}

    make_copy = BoolProperty(
            name="Make Copy",
            description="Make a duplicate of the vertice instead of moving it",
            default=False
            )
    keep_length = BoolProperty(
            name="Keep Edge Length",
            description="Maintain edge lengths",
            default=False
            )
    use_force = BoolProperty(
            name="Use opposite vertices",
            description="Force the usage of the vertices at the other end of the edge",
            default=False
            )
    use_normal = BoolProperty(
            name="Project along normal",
            description="Use the plane's normal as the projection direction",
            default=False
            )

    def draw(self, context):
        layout = self.layout

        if not self.keep_length:
            layout.prop(self, "use_normal")
        layout.prop(self, "make_copy")
        layout.prop(self, "use_force")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return(ob and ob.type == 'MESH' and context.mode == 'EDIT_MESH')

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        try:
            me = context.object.data
            bm = bmesh.from_edit_mesh(me)
            bm.normal_update()

            bFaces = bm.faces
            bEdges = bm.edges
            bVerts = bm.verts

            fVerts = []

            # Find the selected face. This will provide the plane to project onto:
            for f in bFaces:
                if f.select:
                    for v in f.verts:
                        fVerts.append(v)
                    normal = f.normal
                    f.select = False
                    break

            for e in bEdges:
                if e.select:
                    v1 = e.verts[0]
                    v2 = e.verts[1]
                    if v1 in fVerts or v2 in fVerts:
                        e.select = False
                        continue
                    intersection = intersect_line_plane(v1.co, v2.co, fVerts[0].co, normal)
                    if intersection is not None:
                        # Use abs because we don't care what side of plane we're on:
                        d1 = distance_point_to_plane(v1.co, fVerts[0].co, normal)
                        d2 = distance_point_to_plane(v2.co, fVerts[0].co, normal)
                        # If d1 is closer than we use v1 as our vertice:
                        # "xor" with 'use_force':
                        if (abs(d1) < abs(d2)) is not self.use_force:
                            if self.make_copy:
                                v1 = bVerts.new()
                                v1.co = e.verts[0].co
                                bVerts.ensure_lookup_table()
                                bEdges.ensure_lookup_table()
                            if self.keep_length:
                                v1.co = intersection
                            elif self.use_normal:
                                vector = normal
                                vector.length = abs(d1)
                                v1.co = v1.co - (vector * sign(d1))
                            else:
                                v1.co = intersection
                        else:
                            if self.make_copy:
                                v2 = bVerts.new()
                                v2.co = e.verts[1].co
                                bVerts.ensure_lookup_table()
                                bEdges.ensure_lookup_table()
                            if self.keep_length:
                                v2.co = intersection
                            elif self.use_normal:
                                vector = normal
                                vector.length = abs(d2)
                                v2.co = v2.co - (vector * sign(d2))
                            else:
                                v2.co = intersection
                    e.select = False

            bmesh.update_edit_mesh(me)

        except Exception as e:
            error_handlers(self, "mesh.edgetools_project_end", e,
                           reports="Project (End Point) Operator failed", func=False)
            return {'CANCELLED'}

        return {'FINISHED'}


class VIEW3D_MT_edit_mesh_edgetools(Menu):
    bl_label = "Edge Tools"
    bl_description = "Various tools for manipulating edges"

    def draw(self, context):
        layout = self.layout

        layout.operator("mesh.edgetools_extend")
        layout.operator("mesh.edgetools_spline")
        layout.operator("mesh.edgetools_ortho")
        layout.operator("mesh.edgetools_shaft")
        layout.operator("mesh.edgetools_slice")
        layout.separator()

        layout.operator("mesh.edgetools_project")
        layout.operator("mesh.edgetools_project_end")


# define classes for registration
classes = (
    VIEW3D_MT_edit_mesh_edgetools,
    Extend,
    Spline,
    Ortho,
    Shaft,
    Slice,
    Project,
    Project_End,
    )


# registering and menu integration
def register():
    for cls in classes:
        bpy.utils.register_class(cls)


# unregistering and removing menus
def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
