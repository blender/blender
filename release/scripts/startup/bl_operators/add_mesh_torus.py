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

# <pep8-80 compliant>
import bpy
from bpy.types import Operator
import mathutils

from bpy.props import (FloatProperty,
                       IntProperty,
                       BoolProperty,
                       )

from bpy_extras import object_utils


def add_torus(major_rad, minor_rad, major_seg, minor_seg):
    from math import cos, sin, pi

    Vector = mathutils.Vector
    Quaternion = mathutils.Quaternion

    PI_2 = pi * 2.0
    z_axis = 0.0, 0.0, 1.0

    verts = []
    faces = []
    i1 = 0
    tot_verts = major_seg * minor_seg
    for major_index in range(major_seg):
        quat = Quaternion(z_axis, (major_index / major_seg) * PI_2)

        for minor_index in range(minor_seg):
            angle = 2 * pi * minor_index / minor_seg

            vec = quat * Vector((major_rad + (cos(angle) * minor_rad),
                                0.0,
                                (sin(angle) * minor_rad),
                                ))

            verts.extend(vec[:])

            if minor_index + 1 == minor_seg:
                i2 = (major_index) * minor_seg
                i3 = i1 + minor_seg
                i4 = i2 + minor_seg

            else:
                i2 = i1 + 1
                i3 = i1 + minor_seg
                i4 = i3 + 1

            if i2 >= tot_verts:
                i2 = i2 - tot_verts
            if i3 >= tot_verts:
                i3 = i3 - tot_verts
            if i4 >= tot_verts:
                i4 = i4 - tot_verts

            # stupid eekadoodle
            if i2:
                faces.extend([i1, i3, i4, i2])
            else:
                faces.extend([i2, i1, i3, i4])

            i1 += 1

    return verts, faces


class AddTorus(Operator, object_utils.AddObjectHelper):
    '''Add a torus mesh'''
    bl_idname = "mesh.primitive_torus_add"
    bl_label = "Add Torus"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    major_radius = FloatProperty(
            name="Major Radius",
            description=("Radius from the origin to the "
                         "center of the cross sections"),
            min=0.01, max=100.0,
            default=1.0,
            )
    minor_radius = FloatProperty(
            name="Minor Radius",
            description="Radius of the torus' cross section",
            min=0.01, max=100.0,
            default=0.25,
            )
    major_segments = IntProperty(
            name="Major Segments",
            description="Number of segments for the main ring of the torus",
            min=3, max=256,
            default=48,
            )
    minor_segments = IntProperty(
            name="Minor Segments",
            description="Number of segments for the minor ring of the torus",
            min=3, max=256,
            default=12,
            )
    use_abso = BoolProperty(
            name="Use Int+Ext Controls",
            description="Use the Int / Ext controls for torus dimensions",
            default=False,
            )
    abso_major_rad = FloatProperty(
            name="Exterior Radius",
            description="Total Exterior Radius of the torus",
            min=0.01, max=100.0,
            default=1.0,
            )
    abso_minor_rad = FloatProperty(
            name="Inside Radius",
            description="Total Interior Radius of the torus",
            min=0.01, max=100.0,
            default=0.5,
            )

    def execute(self, context):
        if self.use_abso == True:
            extra_helper = (self.abso_major_rad - self.abso_minor_rad) * 0.5
            self.major_radius = self.abso_minor_rad + extra_helper
            self.minor_radius = extra_helper

        verts_loc, faces = add_torus(self.major_radius,
                                     self.minor_radius,
                                     self.major_segments,
                                     self.minor_segments)

        mesh = bpy.data.meshes.new("Torus")

        mesh.vertices.add(len(verts_loc) // 3)

        nbr_loops = len(faces)
        nbr_polys = nbr_loops // 4
        mesh.loops.add(nbr_loops)
        mesh.polygons.add(nbr_polys)

        mesh.vertices.foreach_set("co", verts_loc)
        mesh.polygons.foreach_set("loop_start", range(0, nbr_loops, 4))
        mesh.polygons.foreach_set("loop_total", (4,) * nbr_polys)
        mesh.loops.foreach_set("vertex_index", faces)
        mesh.update()

        object_utils.object_data_add(context, mesh, operator=self)

        return {'FINISHED'}
