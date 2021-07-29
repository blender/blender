#  Copyright (C) 2012 Bill Currie <bill@taniwha.org>
#  Date: 2012/2/20

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
import bmesh
from bpy.types import Operator
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        )
from mathutils import (
        Vector,
        Matrix,
        Quaternion,
        )
from math import (
        pi, cos,
        sin,
        )

cossin = []

# Initialize the cossin table based on the number of segments.
#
#   @param n  The number of segments into which the circle will be
#             divided.
#   @return   None


def build_cossin(n):
    global cossin
    cossin = []
    for i in range(n):
        a = 2 * pi * i / n
        cossin.append((cos(a), sin(a)))


def select_up(axis):
    # if axis.length != 0 and (abs(axis[0] / axis.length) < 1e-5 and abs(axis[1] / axis.length) < 1e-5):
    if (abs(axis[0] / axis.length) < 1e-5 and abs(axis[1] / axis.length) < 1e-5):
        up = Vector((-1, 0, 0))
    else:
        up = Vector((0, 0, 1))
    return up

# Make a single strut in non-manifold mode.
#
#   The strut will be a "cylinder" with @a n sides. The vertices of the
#   cylinder will be @a od / 2 from the center of the cylinder. Optionally,
#   extra loops will be placed (@a od - @a id) / 2 from either end. The
#   strut will be either a simple, open-ended single-surface "cylinder", or a
#   double walled "pipe" with the outer wall vertices @a od / 2 from the center
#   and the inner wall vertices @a id / 2 from the center. The two walls will
#   be joined together at the ends with a face ring such that the entire strut
#   is a manifold object. All faces of the strut will be quads.
#
#   @param v1       Vertex representing one end of the strut's center-line.
#   @param v2       Vertex representing the other end of the strut's
#                   center-line.
#   @param id       The diameter of the inner wall of a solid strut. Used for
#                   calculating the position of the extra loops irrespective
#                   of the solidity of the strut.
#   @param od       The diameter of the outer wall of a solid strut, or the
#                   diameter of a non-solid strut.
#   @param solid    If true, the strut will be made solid such that it has an
#                   inner wall (diameter @a id), an outer wall (diameter
#                   @a od), and face rings at either end of the strut such
#                   the strut is a manifold object. If false, the strut is
#                   a simple, open-ended "cylinder".
#   @param loops    If true, edge loops will be placed at either end of the
#                   strut, (@a od - @a id) / 2 from the end of the strut. The
#                   loops make subsurfed solid struts work nicely.
#   @return         A tuple containing a list of vertices and a list of faces.
#                   The face vertex indices are accurate only for the list of
#                   vertices for the created strut.


def make_strut(v1, v2, ind, od, n, solid, loops):
    v1 = Vector(v1)
    v2 = Vector(v2)
    axis = v2 - v1
    pos = [(0, od / 2)]
    if loops:
        pos += [((od - ind) / 2, od / 2),
                (axis.length - (od - ind) / 2, od / 2)]
    pos += [(axis.length, od / 2)]
    if solid:
        pos += [(axis.length, ind / 2)]
        if loops:
            pos += [(axis.length - (od - ind) / 2, ind / 2),
                    ((od - ind) / 2, ind / 2)]
        pos += [(0, ind / 2)]
    vps = len(pos)
    fps = vps
    if not solid:
        fps -= 1
    fw = axis.copy()
    fw.normalize()
    up = select_up(axis)
    lf = up.cross(fw)
    lf.normalize()
    up = fw.cross(lf)
    mat = Matrix((fw, lf, up))
    mat.transpose()
    verts = [None] * n * vps
    faces = [None] * n * fps
    for i in range(n):
        base = (i - 1) * vps
        x = cossin[i][0]
        y = cossin[i][1]
        for j in range(vps):
            p = Vector((pos[j][0], pos[j][1] * x, pos[j][1] * y))
            p = mat * p
            verts[i * vps + j] = p + v1
        if i:
            for j in range(fps):
                f = (i - 1) * fps + j
                faces[f] = [base + j, base + vps + j,
                            base + vps + (j + 1) % vps, base + (j + 1) % vps]
    base = len(verts) - vps
    i = n
    for j in range(fps):
        f = (i - 1) * fps + j
        faces[f] = [base + j, j, (j + 1) % vps, base + (j + 1) % vps]

    return verts, faces


# Project a point along a vector onto a plane.
#
#   Really, just find the intersection of the line represented by @a point
#   and @a dir with the plane represented by @a norm and @a p. However, if
#   the point is on or in front of the plane, or the line is parallel to
#   the plane, the original point will be returned.
#
#   @param point    The point to be projected onto the plane.
#   @param dir      The vector along which the point will be projected.
#   @param norm     The normal of the plane onto which the point will be
#                   projected.
#   @param p        A point through which the plane passes.
#   @return         A vector representing the projected point, or the
#                   original point.

def project_point(point, dir, norm, p):
    d = (point - p).dot(norm)
    if d >= 0:
        # the point is already on or in front of the plane
        return point
    v = dir.dot(norm)
    if v * v < 1e-8:
        # the plane is unreachable
        return point
    return point - dir * d / v


# Make a simple strut for debugging.
#
#   The strut is just a single quad representing the Z axis of the edge.
#
#   @param mesh     The base mesh. Used for finding the edge vertices.
#   @param edge_num The number of the current edge. For the face vertex
#                   indices.
#   @param edge     The edge for which the strut will be built.
#   @param od       Twice the width of the strut.
#   @return         A tuple containing a list of vertices and a list of faces.
#                   The face vertex indices are pre-adjusted by the edge
#                   number.
#   @fixme          The face vertex indices should be accurate for the local
#                   vertices (consistency)

def make_debug_strut(mesh, edge_num, edge, od):
    v = [mesh.verts[edge.verts[0].index].co,
         mesh.verts[edge.verts[1].index].co,
         None, None]
    v[2] = v[1] + edge.z * od / 2
    v[3] = v[0] + edge.z * od / 2
    f = [[edge_num * 4 + 0, edge_num * 4 + 1,
          edge_num * 4 + 2, edge_num * 4 + 3]]
    return v, f


# Make a cylinder with ends clipped to the end-planes of the edge.
#
#   The strut is just a single quad representing the Z axis of the edge.
#
#   @param mesh     The base mesh. Used for finding the edge vertices.
#   @param edge_num The number of the current edge. For the face vertex
#                   indices.
#   @param edge     The edge for which the strut will be built.
#   @param od       The diameter of the strut.
#   @return         A tuple containing a list of vertices and a list of faces.
#                   The face vertex indices are pre-adjusted by the edge
#                   number.
#   @fixme          The face vertex indices should be accurate for the local
#                   vertices (consistency)

def make_clipped_cylinder(mesh, edge_num, edge, od):
    n = len(cossin)
    cyl = [None] * n
    v0 = mesh.verts[edge.verts[0].index].co
    c0 = v0 + od * edge.y
    v1 = mesh.verts[edge.verts[1].index].co
    c1 = v1 - od * edge.y
    for i in range(n):
        x = cossin[i][0]
        y = cossin[i][1]
        r = (edge.z * x - edge.x * y) * od / 2
        cyl[i] = [c0 + r, c1 + r]
        for p in edge.verts[0].planes:
            cyl[i][0] = project_point(cyl[i][0], edge.y, p, v0)
        for p in edge.verts[1].planes:
            cyl[i][1] = project_point(cyl[i][1], -edge.y, p, v1)
    v = [None] * n * 2
    f = [None] * n
    base = edge_num * n * 2
    for i in range(n):
        v[i * 2 + 0] = cyl[i][1]
        v[i * 2 + 1] = cyl[i][0]
        f[i] = [None] * 4
        f[i][0] = base + i * 2 + 0
        f[i][1] = base + i * 2 + 1
        f[i][2] = base + (i * 2 + 3) % (n * 2)
        f[i][3] = base + (i * 2 + 2) % (n * 2)
    return v, f


# Represent a vertex in the base mesh, with additional information.
#
#   These vertices are @b not shared between edges.
#
#   @var index  The index of the vert in the base mesh
#   @var edge   The edge to which this vertex is attached.
#   @var edges  A tuple of indicess of edges attached to this vert, not
#               including the edge to which this vertex is attached.
#   @var planes List of vectors representing the normals of the planes that
#               bisect the angle between this vert's edge and each other
#               adjacant edge.

class SVert:
    # Create a vertex holding additional information about the bmesh vertex.
    #   @param bmvert   The bmesh vertex for which additional information is
    #                   to be stored.
    #   @param bmedge   The edge to which this vertex is attached.

    def __init__(self, bmvert, bmedge, edge):
        self.index = bmvert.index
        self.edge = edge
        edges = bmvert.link_edges[:]
        edges.remove(bmedge)
        self.edges = tuple(map(lambda e: e.index, edges))
        self.planes = []

    def calc_planes(self, edges):
        for ed in self.edges:
            self.planes.append(calc_plane_normal(self.edge, edges[ed]))


# Represent an edge in the base mesh, with additional information.
#
#   Edges do not share vertices so that the edge is always on the front (back?
#   must verify) side of all the planes attached to its vertices. If the
#   vertices were shared, the edge could be on either side of the planes, and
#   there would be planes attached to the vertex that are irrelevant to the
#   edge.
#
#   @var index      The index of the edge in the base mesh.
#   @var bmedge     Cached reference to this edge's bmedge
#   @var verts      A tuple of 2 SVert vertices, one for each end of the
#                   edge. The vertices are @b not shared between edges.
#                   However, if two edges are connected via a vertex in the
#                   bmesh, their corresponding SVert vertices will have the
#                   the same index value.
#   @var x          The x axis of the edges local frame of reference.
#                   Initially invalid.
#   @var y          The y axis of the edges local frame of reference.
#                   Initialized such that the edge runs from verts[0] to
#                   verts[1] along the negative y axis.
#   @var z          The z axis of the edges local frame of reference.
#                   Initially invalid.


class SEdge:

    def __init__(self, bmesh, bmedge):

        self.index = bmedge.index
        self.bmedge = bmedge
        bmesh.verts.ensure_lookup_table()
        self.verts = (SVert(bmedge.verts[0], bmedge, self),
                      SVert(bmedge.verts[1], bmedge, self))
        self.y = (bmesh.verts[self.verts[0].index].co -
                  bmesh.verts[self.verts[1].index].co)
        self.y.normalize()
        self.x = self.z = None

    def set_frame(self, up):
        self.x = self.y.cross(up)
        self.x.normalize()
        self.z = self.x.cross(self.y)

    def calc_frame(self, base_edge):
        baxis = base_edge.y
        if (self.verts[0].index == base_edge.verts[0].index or
              self.verts[1].index == base_edge.verts[1].index):
            axis = -self.y
        elif (self.verts[0].index == base_edge.verts[1].index or
                self.verts[1].index == base_edge.verts[0].index):
            axis = self.y
        else:
            raise ValueError("edges not connected")
        if baxis.dot(axis) in (-1, 1):
            # aligned axis have their up/z aligned
            up = base_edge.z
        else:
            # Get the unit vector dividing the angle (theta) between baxis and
            # axis in two equal parts
            h = (baxis + axis)
            h.normalize()
            # (cos(theta/2), sin(theta/2) * n) where n is the unit vector of the
            # axis rotating baxis onto axis
            q = Quaternion([baxis.dot(h)] + list(baxis.cross(h)))
            # rotate the base edge's up around the rotation axis (blender
            # quaternion shortcut:)
            up = q * base_edge.z
        self.set_frame(up)

    def calc_vert_planes(self, edges):
        for v in self.verts:
            v.calc_planes(edges)

    def bisect_faces(self):
        n1 = self.bmedge.link_faces[0].normal
        if len(self.bmedge.link_faces) > 1:
            n2 = self.bmedge.link_faces[1].normal
            return (n1 + n2).normalized()
        return n1

    def calc_simple_frame(self):
        return self.y.cross(select_up(self.y)).normalized()

    def find_edge_frame(self, sedges):
        if self.bmedge.link_faces:
            return self.bisect_faces()
        if self.verts[0].edges or self.verts[1].edges:
            edges = list(self.verts[0].edges + self.verts[1].edges)
            for i in range(len(edges)):
                edges[i] = sedges[edges[i]]
            while edges and edges[-1].y.cross(self.y).length < 1e-3:
                edges.pop()
            if not edges:
                return self.calc_simple_frame()
            n1 = edges[-1].y.cross(self.y).normalized()
            edges.pop()
            while edges and edges[-1].y.cross(self.y).cross(n1).length < 1e-3:
                edges.pop()
            if not edges:
                return n1
            n2 = edges[-1].y.cross(self.y).normalized()
            return (n1 + n2).normalized()
        return self.calc_simple_frame()


def calc_plane_normal(edge1, edge2):
    if edge1.verts[0].index == edge2.verts[0].index:
        axis1 = -edge1.y
        axis2 = edge2.y
    elif edge1.verts[1].index == edge2.verts[1].index:
        axis1 = edge1.y
        axis2 = -edge2.y
    elif edge1.verts[0].index == edge2.verts[1].index:
        axis1 = -edge1.y
        axis2 = -edge2.y
    elif edge1.verts[1].index == edge2.verts[0].index:
        axis1 = edge1.y
        axis2 = edge2.y
    else:
        raise ValueError("edges not connected")
    # Both axis1 and axis2 are unit vectors, so this will produce a vector
    # bisects the two, so long as they are not 180 degrees apart (in which
    # there are infinite solutions).
    return (axis1 + axis2).normalized()


def build_edge_frames(edges):
    edge_set = set(edges)
    while edge_set:
        edge_queue = [edge_set.pop()]
        edge_queue[0].set_frame(edge_queue[0].find_edge_frame(edges))
        while edge_queue:
            current_edge = edge_queue.pop()
            for i in (0, 1):
                for e in current_edge.verts[i].edges:
                    edge = edges[e]
                    if edge.x is not None:  # edge already processed
                        continue
                    edge_set.remove(edge)
                    edge_queue.append(edge)
                    edge.calc_frame(current_edge)


def make_manifold_struts(truss_obj, od, segments):
    bpy.context.scene.objects.active = truss_obj
    bpy.ops.object.editmode_toggle()
    truss_mesh = bmesh.from_edit_mesh(truss_obj.data).copy()
    bpy.ops.object.editmode_toggle()
    edges = [None] * len(truss_mesh.edges)
    for i, e in enumerate(truss_mesh.edges):
        edges[i] = SEdge(truss_mesh, e)
    build_edge_frames(edges)
    verts = []
    faces = []
    for e, edge in enumerate(edges):
        # v, f = make_debug_strut(truss_mesh, e, edge, od)
        edge.calc_vert_planes(edges)
        v, f = make_clipped_cylinder(truss_mesh, e, edge, od)
        verts += v
        faces += f
    return verts, faces


def make_simple_struts(truss_mesh, ind, od, segments, solid, loops):
    vps = 2
    if solid:
        vps *= 2
    if loops:
        vps *= 2
    fps = vps
    if not solid:
        fps -= 1

    verts = [None] * len(truss_mesh.edges) * segments * vps
    faces = [None] * len(truss_mesh.edges) * segments * fps
    vbase = 0
    fbase = 0

    for e in truss_mesh.edges:
        v1 = truss_mesh.vertices[e.vertices[0]]
        v2 = truss_mesh.vertices[e.vertices[1]]
        v, f = make_strut(v1.co, v2.co, ind, od, segments, solid, loops)
        for fv in f:
            for i in range(len(fv)):
                fv[i] += vbase
        for i in range(len(v)):
            verts[vbase + i] = v[i]
        for i in range(len(f)):
            faces[fbase + i] = f[i]
        # if not base % 12800:
        #    print (base * 100 / len(verts))
        vbase += vps * segments
        fbase += fps * segments

    return verts, faces


def create_struts(self, context, ind, od, segments, solid, loops, manifold):
    build_cossin(segments)

    for truss_obj in bpy.context.scene.objects:
        if not truss_obj.select:
            continue
        truss_obj.select = False
        truss_mesh = truss_obj.to_mesh(context.scene, True, 'PREVIEW')
        if not truss_mesh.edges:
            continue
        if manifold:
            verts, faces = make_manifold_struts(truss_obj, od, segments)
        else:
            verts, faces = make_simple_struts(truss_mesh, ind, od, segments,
                                              solid, loops)
        mesh = bpy.data.meshes.new("Struts")
        mesh.from_pydata(verts, [], faces)
        obj = bpy.data.objects.new("Struts", mesh)
        bpy.context.scene.objects.link(obj)
        obj.select = True
        obj.location = truss_obj.location
        bpy.context.scene.objects.active = obj
        mesh.update()


class Struts(Operator):
    bl_idname = "mesh.generate_struts"
    bl_label = "Struts"
    bl_description = ("Add one or more struts meshes based on selected truss meshes \n"
                      "Note: can get very high poly\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {'REGISTER', 'UNDO'}

    ind = FloatProperty(
            name="Inside Diameter",
            description="Diameter of inner surface",
            min=0.0, soft_min=0.0,
            max=100, soft_max=100,
            default=0.04
            )
    od = FloatProperty(
            name="Outside Diameter",
            description="Diameter of outer surface",
            min=0.001, soft_min=0.001,
            max=100, soft_max=100,
            default=0.05
            )
    manifold = BoolProperty(
            name="Manifold",
            description="Connect struts to form a single solid",
            default=False
            )
    solid = BoolProperty(
            name="Solid",
            description="Create inner surface",
            default=False
            )
    loops = BoolProperty(
            name="Loops",
            description="Create sub-surf friendly loops",
            default=False
            )
    segments = IntProperty(
            name="Segments",
            description="Number of segments around strut",
            min=3, soft_min=3,
            max=64, soft_max=64,
            default=12
            )

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(self, "ind")
        col.prop(self, "od")
        col.prop(self, "segments")
        col.separator()

        col.prop(self, "manifold")
        col.prop(self, "solid")
        col.prop(self, "loops")

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == "MESH"

    def execute(self, context):
        store_undo = bpy.context.user_preferences.edit.use_global_undo
        bpy.context.user_preferences.edit.use_global_undo = False
        keywords = self.as_keywords()

        try:
            create_struts(self, context, **keywords)
            bpy.context.user_preferences.edit.use_global_undo = store_undo

            return {"FINISHED"}

        except Exception as e:
            bpy.context.user_preferences.edit.use_global_undo = store_undo
            self.report({"WARNING"},
                        "Make Struts could not be performed. Operation Cancelled")
            print("\n[mesh.generate_struts]\n{}".format(e))
            return {"CANCELLED"}


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
