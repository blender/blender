# GPL # "author": Paulo_Gomes

import bpy
from mathutils import Quaternion, Vector
from math import cos, sin, pi
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        )


# Create a new mesh (object) from verts/edges/faces
# verts/edges/faces ... List of vertices/edges/faces for the
#                       new mesh (as used in from_pydata)
# name ... Name of the new mesh (& object)

def create_mesh_object(context, verts, edges, faces, name):

    # Create new mesh
    mesh = bpy.data.meshes.new(name)

    # Make a mesh from a list of verts/edges/faces
    mesh.from_pydata(verts, edges, faces)

    # Update mesh geometry after adding stuff
    mesh.update()

    from bpy_extras import object_utils
    return object_utils.object_data_add(context, mesh, operator=None)


# A very simple "bridge" tool

def createFaces(vertIdx1, vertIdx2, closed=False, flipped=False):
    faces = []

    if not vertIdx1 or not vertIdx2:
        return None

    if len(vertIdx1) < 2 and len(vertIdx2) < 2:
        return None

    fan = False
    if (len(vertIdx1) != len(vertIdx2)):
        if (len(vertIdx1) == 1 and len(vertIdx2) > 1):
            fan = True
        else:
            return None

    total = len(vertIdx2)

    if closed:
        # Bridge the start with the end
        if flipped:
            face = [
                vertIdx1[0],
                vertIdx2[0],
                vertIdx2[total - 1]]
            if not fan:
                face.append(vertIdx1[total - 1])
            faces.append(face)

        else:
            face = [vertIdx2[0], vertIdx1[0]]
            if not fan:
                face.append(vertIdx1[total - 1])
            face.append(vertIdx2[total - 1])
            faces.append(face)

    # Bridge the rest of the faces
    for num in range(total - 1):
        if flipped:
            if fan:
                face = [vertIdx2[num], vertIdx1[0], vertIdx2[num + 1]]
            else:
                face = [vertIdx2[num], vertIdx1[num],
                    vertIdx1[num + 1], vertIdx2[num + 1]]
            faces.append(face)
        else:
            if fan:
                face = [vertIdx1[0], vertIdx2[num], vertIdx2[num + 1]]
            else:
                face = [vertIdx1[num], vertIdx2[num],
                    vertIdx2[num + 1], vertIdx1[num + 1]]
            faces.append(face)

    return faces


def add_twisted_torus(major_rad, minor_rad, major_seg, minor_seg, twists):
    PI_2 = pi * 2.0
    z_axis = (0.0, 0.0, 1.0)

    verts = []
    faces = []

    edgeloop_prev = []
    for major_index in range(major_seg):
        quat = Quaternion(z_axis, (major_index / major_seg) * PI_2)
        rot_twists = PI_2 * major_index / major_seg * twists

        edgeloop = []

        # Create section ring
        for minor_index in range(minor_seg):
            angle = (PI_2 * minor_index / minor_seg) + rot_twists

            vec = Vector((
                major_rad + (cos(angle) * minor_rad),
                0.0,
                sin(angle) * minor_rad))
            vec = quat * vec

            edgeloop.append(len(verts))
            verts.append(vec)

        # Remember very first edgeloop
        if major_index == 0:
            edgeloop_first = edgeloop

        # Bridge last with current ring
        if edgeloop_prev:
            f = createFaces(edgeloop_prev, edgeloop, closed=True)
            faces.extend(f)

        edgeloop_prev = edgeloop

    # Bridge first and last ring
    f = createFaces(edgeloop_prev, edgeloop_first, closed=True)
    faces.extend(f)

    return verts, faces


class AddTwistedTorus(bpy.types.Operator):
    bl_idname = "mesh.primitive_twisted_torus_add"
    bl_label = "Add Twisted Torus"
    bl_description = "Construct a twisted torus mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    major_radius = FloatProperty(
        name="Major Radius",
        description="Radius from the origin to the"
                    " center of the cross section",
        min=0.01,
        max=100.0,
        default=1.0
        )
    minor_radius = FloatProperty(
        name="Minor Radius",
        description="Radius of the torus' cross section",
        min=0.01,
        max=100.0,
        default=0.25
        )
    major_segments = IntProperty(
        name="Major Segments",
        description="Number of segments for the main ring of the torus",
        min=3,
        max=256,
        default=48
        )
    minor_segments = IntProperty(
        name="Minor Segments",
        description="Number of segments for the minor ring of the torus",
        min=3,
        max=256,
        default=12
        )
    twists = IntProperty(
        name="Twists",
        description="Number of twists of the torus",
        min=0,
        max=256,
        default=1
        )
    use_abso = BoolProperty(
        name="Use Int/Ext Controls",
        description="Use the Int/Ext controls for torus dimensions",
        default=False
        )
    abso_major_rad = FloatProperty(
        name="Exterior Radius",
        description="Total Exterior Radius of the torus",
        min=0.01,
        max=100.0,
        default=1.0
        )
    abso_minor_rad = FloatProperty(
        name="Inside Radius",
        description="Total Interior Radius of the torus",
        min=0.01,
        max=100.0,
        default=0.5
        )

    def execute(self, context):

        if self.use_abso is True:
            extra_helper = (self.abso_major_rad - self.abso_minor_rad) * 0.5
            self.major_radius = self.abso_minor_rad + extra_helper
            self.minor_radius = extra_helper

        verts, faces = add_twisted_torus(
            self.major_radius,
            self.minor_radius,
            self.major_segments,
            self.minor_segments,
            self.twists
            )

        # Create the mesh object from this geometry data.
        obj = create_mesh_object(context, verts, [], faces, "TwistedTorus")

        return {'FINISHED'}
