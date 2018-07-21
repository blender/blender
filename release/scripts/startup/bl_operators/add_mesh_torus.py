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

from bpy.props import (
    BoolProperty,
    FloatProperty,
    IntProperty,
)
from bpy.app.translations import pgettext_data as data_

from bpy_extras import object_utils


def add_torus(major_rad, minor_rad, major_seg, minor_seg):
    from math import cos, sin, pi
    from mathutils import Vector, Matrix

    pi_2 = pi * 2.0

    verts = []
    faces = []
    i1 = 0
    tot_verts = major_seg * minor_seg
    for major_index in range(major_seg):
        matrix = Matrix.Rotation((major_index / major_seg) * pi_2, 3, 'Z')

        for minor_index in range(minor_seg):
            angle = pi_2 * minor_index / minor_seg

            vec = matrix * Vector((major_rad + (cos(angle) * minor_rad),
                                   0.0,
                                   sin(angle) * minor_rad,
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

            faces.extend([i1, i3, i4, i2])

            i1 += 1

    return verts, faces


def add_uvs(mesh, minor_seg, major_seg):
    from math import fmod

    mesh.uv_layers.new()
    uv_data = mesh.uv_layers.active.data
    polygons = mesh.polygons
    u_step = 1.0 / major_seg
    v_step = 1.0 / minor_seg

    # Round UV's, needed when segments aren't divisible by 4.
    u_init = 0.5 + fmod(0.5, u_step)
    v_init = 0.5 + fmod(0.5, v_step)

    # Calculate wrapping value under 1.0 to prevent
    # float precision errors wrapping at the wrong step.
    u_wrap = 1.0 - (u_step / 2.0)
    v_wrap = 1.0 - (v_step / 2.0)

    vertex_index = 0

    u_prev = u_init
    u_next = u_prev + u_step
    for major_index in range(major_seg):
        v_prev = v_init
        v_next = v_prev + v_step
        for minor_index in range(minor_seg):
            loops = polygons[vertex_index].loop_indices
            uv_data[loops[0]].uv = u_prev, v_prev
            uv_data[loops[1]].uv = u_next, v_prev
            uv_data[loops[3]].uv = u_prev, v_next
            uv_data[loops[2]].uv = u_next, v_next

            if v_next > v_wrap:
                v_prev = v_next - 1.0
            else:
                v_prev = v_next
            v_next = v_prev + v_step

            vertex_index += 1

        if u_next > u_wrap:
            u_prev = u_next - 1.0
        else:
            u_prev = u_next
        u_next = u_prev + u_step


class AddTorus(Operator, object_utils.AddObjectHelper):
    """Add a torus mesh"""
    bl_idname = "mesh.primitive_torus_add"
    bl_label = "Add Torus"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    def mode_update_callback(self, context):
        if self.mode == 'EXT_INT':
            self.abso_major_rad = self.major_radius + self.minor_radius
            self.abso_minor_rad = self.major_radius - self.minor_radius

    major_segments: IntProperty(
        name="Major Segments",
        description="Number of segments for the main ring of the torus",
        min=3, max=256,
        default=48,
    )
    minor_segments: IntProperty(
        name="Minor Segments",
        description="Number of segments for the minor ring of the torus",
        min=3, max=256,
        default=12,
    )
    mode: bpy.props.EnumProperty(
        name="Torus Dimensions",
        items=(("MAJOR_MINOR", "Major/Minor",
                "Use the major/minor radii for torus dimensions"),
               ("EXT_INT", "Exterior/Interior",
                "Use the exterior/interior radii for torus dimensions")),
        update=mode_update_callback,
    )
    major_radius: FloatProperty(
        name="Major Radius",
        description=("Radius from the origin to the "
                     "center of the cross sections"),
        min=0.01, max=100.0,
        default=1.0,
        subtype='DISTANCE',
        unit='LENGTH',
    )
    minor_radius: FloatProperty(
        name="Minor Radius",
        description="Radius of the torus' cross section",
        min=0.01, max=100.0,
        default=0.25,
        subtype='DISTANCE',
        unit='LENGTH',
    )
    abso_major_rad: FloatProperty(
        name="Exterior Radius",
        description="Total Exterior Radius of the torus",
        min=0.01, max=100.0,
        default=1.25,
        subtype='DISTANCE',
        unit='LENGTH',
    )
    abso_minor_rad: FloatProperty(
        name="Interior Radius",
        description="Total Interior Radius of the torus",
        min=0.01, max=100.0,
        default=0.75,
        subtype='DISTANCE',
        unit='LENGTH',
    )
    generate_uvs: BoolProperty(
        name="Generate UVs",
        description="Generate a default UV map",
        default=False,
    )

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(self, 'generate_uvs')
        col.separator()
        col.prop(self, 'view_align')

        col = layout.column(align=True)
        col.label(text="Location")
        col.prop(self, "location", text="")

        col = layout.column(align=True)
        col.label(text="Rotation")
        col.prop(self, "rotation", text="")

        col = layout.column(align=True)
        col.label(text="Major Segments")
        col.prop(self, "major_segments", text="")

        col = layout.column(align=True)
        col.label(text="Minor Segments")
        col.prop(self, "minor_segments", text="")

        col = layout.column(align=True)
        col.label(text="Torus Dimensions")
        col.row().prop(self, "mode", expand=True)

        if self.mode == 'MAJOR_MINOR':
            col = layout.column(align=True)
            col.label(text="Major Radius")
            col.prop(self, "major_radius", text="")

            col = layout.column(align=True)
            col.label(text="Minor Radius")
            col.prop(self, "minor_radius", text="")
        else:
            col = layout.column(align=True)
            col.label(text="Exterior Radius")
            col.prop(self, "abso_major_rad", text="")

            col = layout.column(align=True)
            col.label(text="Interior Radius")
            col.prop(self, "abso_minor_rad", text="")

    def invoke(self, context, event):
        object_utils.object_add_grid_scale_apply_operator(self, context)
        return self.execute(context)

    def execute(self, context):

        if self.mode == 'EXT_INT':
            extra_helper = (self.abso_major_rad - self.abso_minor_rad) * 0.5
            self.major_radius = self.abso_minor_rad + extra_helper
            self.minor_radius = extra_helper

        verts_loc, faces = add_torus(self.major_radius,
                                     self.minor_radius,
                                     self.major_segments,
                                     self.minor_segments)

        mesh = bpy.data.meshes.new(data_("Torus"))

        mesh.vertices.add(len(verts_loc) // 3)

        nbr_loops = len(faces)
        nbr_polys = nbr_loops // 4
        mesh.loops.add(nbr_loops)
        mesh.polygons.add(nbr_polys)

        mesh.vertices.foreach_set("co", verts_loc)
        mesh.polygons.foreach_set("loop_start", range(0, nbr_loops, 4))
        mesh.polygons.foreach_set("loop_total", (4,) * nbr_polys)
        mesh.loops.foreach_set("vertex_index", faces)

        if self.generate_uvs:
            add_uvs(mesh, self.minor_segments, self.major_segments)

        mesh.update()

        object_utils.object_data_add(context, mesh, operator=self)

        return {'FINISHED'}


classes = (
    AddTorus,
)
