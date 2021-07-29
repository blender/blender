# GPL #  Author, Anthony D'Agostino

import bpy
from mathutils import Vector
from math import sin, cos, pi
from bpy.props import IntProperty


def create_mesh_object(context, verts, edges, faces, name):
    # Create new mesh
    mesh = bpy.data.meshes.new(name)
    # Make a mesh from a list of verts/edges/faces.
    mesh.from_pydata(verts, edges, faces)
    # Update mesh geometry after adding stuff.
    mesh.update()
    from bpy_extras import object_utils
    return object_utils.object_data_add(context, mesh, operator=None)


# ========================
# === Torus Knot Block ===
# ========================

def k1(t):
    x = cos(t) - 2 * cos(2 * t)
    y = sin(t) + 2 * sin(2 * t)
    z = sin(3 * t)
    return Vector([x, y, z])


def k2(t):
    x = 10 * (cos(t) + cos(3 * t)) + cos(2 * t) + cos(4 * t)
    y = 6 * sin(t) + 10 * sin(3 * t)
    z = 4 * sin(3 * t) * sin(5 * t / 2) + 4 * sin(4 * t) - 2 * sin(6 * t)
    return Vector([x, y, z]) * 0.2


def k3(t):
    x = 2.5 * cos(t + pi) / 3 + 2 * cos(3 * t)
    y = 2.5 * sin(t) / 3 + 2 * sin(3 * t)
    z = 1.5 * sin(4 * t) + sin(2 * t) / 3
    return Vector([x, y, z])


def make_verts(ures, vres, r2, knotfunc):
    verts = []
    for i in range(ures):
        t1 = (i + 0) * 2 * pi / ures
        t2 = (i + 1) * 2 * pi / ures
        a = knotfunc(t1)        # curr point
        b = knotfunc(t2)        # next point
        a, b = map(Vector, (a, b))
        e = a - b
        f = a + b
        g = e.cross(f)
        h = e.cross(g)
        g.normalize()
        h.normalize()
        for j in range(vres):
            k = j * 2 * pi / vres
            l = (cos(k), 0.0, sin(k))
            l = Vector(l)
            m = l * r2
            x, y, z = m
            n = h * x
            o = g * z
            p = n + o
            q = a + p
            verts.append(q)
    return verts


def make_faces(ures, vres):
    faces = []
    for u in range(0, ures):
        for v in range(0, vres):
            p1 = v + u * vres
            p2 = v + ((u + 1) % ures) * vres
            p4 = (v + 1) % vres + u * vres
            p3 = (v + 1) % vres + ((u + 1) % ures) * vres
            faces.append([p4, p3, p2, p1])
    return faces


def make_knot(knotidx, ures):
    knots = [k1, k2, k3]
    knotfunc = knots[knotidx - 1]
    vres = ures // 10
    r2 = 0.5
    verts = make_verts(ures, vres, r2, knotfunc)
    faces = make_faces(ures, vres)
    return (verts, faces)


class AddTorusKnot(bpy.types.Operator):
    bl_idname = "mesh.primitive_torusknot_add"
    bl_label = "Add Torus Knot"
    bl_description = "Construct a torus knot mesh"
    bl_options = {"REGISTER", "UNDO"}

    resolution = IntProperty(
        name="Resolution",
        description="Resolution of the Torus Knot",
        default=80,
        min=30, max=256
        )
    objecttype = IntProperty(
        name="Knot Type",
        description="Type of Knot",
        default=1,
        min=1, max=3
        )

    def execute(self, context):
        verts, faces = make_knot(self.objecttype, self.resolution)
        obj = create_mesh_object(context, verts, [], faces, "Torus Knot")

        return {'FINISHED'}
