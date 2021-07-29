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

# ------------------------- COLORS / GROUPS EXCHANGER ------------------------ #
#                                                                              #
# Vertex Color to Vertex Group allow you to convert colors channles to weight  #
# maps.                                                                        #
# The main purpose is to use vertex colors to store information when importing #
# files from other softwares. The script works with the active vertex color    #
# slot.                                                                        #
# For use the command "Vertex Clors to Vertex Groups" use the search bar       #
# (space bar).                                                                 #
#                                                                              #
#                          (c)  Alessandro Zomparelli                          #
#                                     (2017)                                   #
#                                                                              #
# http://www.co-de-it.com/                                                     #
#                                                                              #
# ############################################################################ #

bl_info = {
    "name": "Colors/Groups Exchanger",
    "author": "Alessandro Zomparelli (Co-de-iT)",
    "version": (0, 3, 1),
    "blender": (2, 7, 8),
    "location": "",
    "description": ("Convert vertex colors channels to vertex groups and vertex"
                    " groups to colors"),
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        IntProperty,
        )
from bpy.types import Operator
from math import (
        pi, sin,
        )


class vertex_colors_to_vertex_groups(Operator):
    bl_idname = "object.vertex_colors_to_vertex_groups"
    bl_label = "Vertex Color"
    bl_description = ("Convert the active Vertex Color into a Vertex Group")
    bl_options = {'REGISTER', 'UNDO'}

    red = BoolProperty(
            name="Red Channel",
            default=False,
            description="Convert Red Channel"
            )
    green = BoolProperty(
            name="Green Channel",
            default=False,
            description="Convert Green Channel"
            )
    blue = BoolProperty(
            name="Blue Channel",
            default=False,
            description="Convert Blue Channel"
            )
    value = BoolProperty(
            name="Value Channel",
            default=True,
            description="Convert Value Channel"
            )
    invert = BoolProperty(
            name="Invert",
            default=False,
            description="Invert all Color Channels"
            )

    def execute(self, context):
        obj = bpy.context.active_object
        ids = len(obj.vertex_groups)
        id_red = ids
        id_green = ids
        id_blue = ids
        id_value = ids

        boolCol = len(obj.data.vertex_colors)
        if boolCol:
            col_name = obj.data.vertex_colors.active.name
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')

        if self.red and boolCol:
            bpy.ops.object.vertex_group_add()
            bpy.ops.object.vertex_group_assign()
            id_red = ids
            obj.vertex_groups[id_red].name = col_name + '_red'
            ids += 1
        if self.green and boolCol:
            bpy.ops.object.vertex_group_add()
            bpy.ops.object.vertex_group_assign()
            id_green = ids
            obj.vertex_groups[id_green].name = col_name + '_green'
            ids += 1
        if self.blue and boolCol:
            bpy.ops.object.vertex_group_add()
            bpy.ops.object.vertex_group_assign()
            id_blue = ids
            obj.vertex_groups[id_blue].name = col_name + '_blue'
            ids += 1
        if self.value and boolCol:
            bpy.ops.object.vertex_group_add()
            bpy.ops.object.vertex_group_assign()
            id_value = ids
            obj.vertex_groups[id_value].name = col_name + '_value'
            ids += 1

        mult = 1
        if self.invert:
            mult = -1

        bpy.ops.object.mode_set(mode='OBJECT')
        sub_red = 1 + self.value + self.blue + self.green
        sub_green = 1 + self.value + self.blue
        sub_blue = 1 + self.value
        sub_value = 1

        ids = len(obj.vertex_groups)
        if (id_red <= ids and id_green <= ids and id_blue <= ids and id_value <=
                    ids and boolCol):
            v_colors = obj.data.vertex_colors.active.data
            i = 0
            for f in obj.data.polygons:
                for v in f.vertices:
                    gr = obj.data.vertices[v].groups
                    if self.red:
                        gr[min(len(gr) - sub_red, id_red)].weight = \
                                self.invert + mult * v_colors[i].color.r

                    if self.green:
                        gr[min(len(gr) - sub_green, id_green)].weight = \
                                self.invert + mult * v_colors[i].color.g

                    if self.blue:
                        gr[min(len(gr) - sub_blue, id_blue)].weight = \
                                self.invert + mult * v_colors[i].color.b

                    if self.value:
                        gr[min(len(gr) - sub_value, id_value)].weight = \
                                self.invert + mult * v_colors[i].color.v
                    i += 1
            bpy.ops.paint.weight_paint_toggle()

        return {'FINISHED'}


class vertex_group_to_vertex_colors(Operator):
    bl_idname = "object.vertex_group_to_vertex_colors"
    bl_label = "Vertex Group"
    bl_description = ("Convert the active Vertex Group into a Vertex Color")
    bl_options = {'REGISTER', 'UNDO'}

    channel = EnumProperty(
            items=[('Blue', 'Blue Channel', 'Convert to Blue Channel'),
                   ('Green', 'Green Channel', 'Convert to Green Channel'),
                   ('Red', 'Red Channel', 'Convert to Red Channel'),
                   ('Value', 'Value Channel', 'Convert to Grayscale'),
                   ('False Colors', 'False Colors', 'Convert to False Colors')],
            name="Convert to",
            description="Choose how to convert vertex group",
            default="Value",
            options={'LIBRARY_EDITABLE'}
            )

    invert = BoolProperty(
            name="Invert",
            default=False,
            description="Invert Color Channel"
            )

    def execute(self, context):
        obj = bpy.context.active_object
        group_id = obj.vertex_groups.active_index

        if (group_id == -1):
            return {'FINISHED'}

        bpy.ops.object.mode_set(mode='OBJECT')
        group_name = obj.vertex_groups[group_id].name
        bpy.ops.mesh.vertex_color_add()
        colors_id = obj.data.vertex_colors.active_index

        colors_name = group_name
        if(self.channel == 'False Colors'):
            colors_name += "_false_colors"
        elif(self.channel == 'Value'):
            colors_name += "_value"
        elif(self.channel == 'Red'):
            colors_name += "_red"
        elif(self.channel == 'Green'):
            colors_name += "_green"
        elif(self.channel == 'Blue'):
            colors_name += "_blue"
        bpy.context.object.data.vertex_colors[colors_id].name = colors_name

        v_colors = obj.data.vertex_colors.active.data

        mult = 1
        if self.invert:
            mult = -1

        i = 0
        for f in obj.data.polygons:
            for v in f.vertices:
                gr = obj.data.vertices[v].groups

                if(self.channel == 'False Colors'):
                    v_colors[i].color = (0, 0, 1)
                else:
                    v_colors[i].color = (0, 0, 0)

                for g in gr:
                    if g.group == group_id:
                        if(self.channel == 'False Colors'):
                            if g.weight < 0.25:
                                v_colors[i].color = (0, g.weight * 4, 1)
                            elif g.weight < 0.5:
                                v_colors[i].color = (0, 1, 1 - (g.weight - 0.25) * 4)
                            elif g.weight < 0.75:
                                v_colors[i].color = ((g.weight - 0.5) * 4, 1, 0)
                            else:
                                v_colors[i].color = (1, 1 - (g.weight - 0.75) * 4, 0)
                        elif(self.channel == 'Value'):
                            v_colors[i].color = (
                                self.invert + mult * g.weight,
                                self.invert + mult * g.weight,
                                self.invert + mult * g.weight)
                        elif(self.channel == 'Red'):
                            v_colors[i].color = (
                                self.invert + mult * g.weight, 0, 0)
                        elif(self.channel == 'Green'):
                            v_colors[i].color = (
                                0, self.invert + mult * g.weight, 0)
                        elif(self.channel == 'Blue'):
                            v_colors[i].color = (
                                0, 0, self.invert + mult * g.weight)
                i += 1
        bpy.ops.paint.vertex_paint_toggle()
        bpy.context.object.data.vertex_colors[colors_id].active_render = True
        return {'FINISHED'}


class curvature_to_vertex_groups(Operator):
    bl_idname = "object.curvature_to_vertex_groups"
    bl_label = "Curvature"
    bl_description = ("Generate a Vertex Group based on the curvature of the"
                      "mesh. Is based on Dirty Vertex Color")
    bl_options = {'REGISTER', 'UNDO'}

    invert = BoolProperty(
            name="Invert",
            default=False,
            description="Invert Values"
            )
    blur_strength = FloatProperty(
            name="Blur Strength",
            default=1,
            min=0.001,
            max=1,
            description="Blur strength per iteration"
            )
    blur_iterations = IntProperty(
            name="Blur Iterations",
            default=1,
            min=0,
            max=40,
            description="Number of times to blur the values"
            )
    min_angle = FloatProperty(
            name="Min Angle",
            default=0,
            min=0,
            max=pi / 2,
            subtype='ANGLE',
            description="Minimum angle"
            )
    max_angle = FloatProperty(
            name="Max Angle",
            default=pi,
            min=pi / 2,
            max=pi,
            subtype='ANGLE',
            description="Maximum angle"
            )
    invert = BoolProperty(
            name="Invert",
            default=False,
            description="Invert the curvature map"
            )

    def execute(self, context):
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.mesh.vertex_color_add()
        vertex_colors = bpy.context.active_object.data.vertex_colors
        vertex_colors[-1].active = True
        vertex_colors[-1].active_render = True
        vertex_colors[-1].name = "Curvature"

        for c in vertex_colors[-1].data:
            c.color.r, c.color.g, c.color.b = 1, 1, 1

        bpy.ops.object.mode_set(mode='VERTEX_PAINT')
        bpy.ops.paint.vertex_color_dirt(
                blur_strength=self.blur_strength,
                blur_iterations=self.blur_iterations,
                clean_angle=self.max_angle,
                dirt_angle=self.min_angle
                )
        bpy.ops.object.vertex_colors_to_vertex_groups(invert=self.invert)
        bpy.ops.mesh.vertex_color_remove()

        return {'FINISHED'}


class face_area_to_vertex_groups(Operator):
    bl_idname = "object.face_area_to_vertex_groups"
    bl_label = "Area"
    bl_description = ("Generate a Vertex Group based on the area of individual"
                      "faces")
    bl_options = {'REGISTER', 'UNDO'}

    invert = BoolProperty(
            name="Invert",
            default=False,
            description="Invert Values"
            )

    def execute(self, context):
        obj = bpy.context.active_object
        id_value = len(obj.vertex_groups)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.object.vertex_group_add()
        bpy.ops.object.vertex_group_assign()
        obj.vertex_groups[id_value].name = 'faces_area'
        mult = 1

        if self.invert:
            mult = -1

        bpy.ops.object.mode_set(mode='OBJECT')
        min_area = False
        max_area = False
        n_values = [0] * len(obj.data.vertices)
        values = [0] * len(obj.data.vertices)
        for p in obj.data.polygons:
            for v in p.vertices:
                n_values[v] += 1
                values[v] += p.area

                if min_area:
                    if min_area > p.area:
                        min_area = p.area
                else:
                    min_area = p.area

                if max_area:
                    if max_area < p.area:
                        max_area = p.area
                else:
                    max_area = p.area

        for v in obj.data.vertices:
            gr = v.groups
            index = v.index
            try:
                gr[min(len(gr) - 1, id_value)].weight = \
                        self.invert + mult * \
                        ((values[index] / n_values[index] - min_area) /
                        (max_area - min_area))
            except:
                gr[min(len(gr) - 1, id_value)].weight = 0.5

        bpy.ops.paint.weight_paint_toggle()

        return {'FINISHED'}


class harmonic_weight(Operator):
    bl_idname = "object.harmonic_weight"
    bl_label = "Harmonic"
    bl_description = "Create an harmonic variation of the active Vertex Group"
    bl_options = {'REGISTER', 'UNDO'}

    freq = FloatProperty(
            name="Frequency",
            default=20,
            soft_min=0,
            soft_max=100,
            description="Wave frequency"
            )
    amp = FloatProperty(
            name="Amplitude",
            default=1,
            soft_min=0,
            soft_max=10,
            description="Wave amplitude"
            )
    midlevel = FloatProperty(
            name="Midlevel",
            default=0,
            min=-1,
            max=1,
            description="Midlevel"
            )
    add = FloatProperty(
            name="Add",
            default=0,
            min=-1,
            max=1,
            description="Add to the Weight"
            )
    mult = FloatProperty(
            name="Multiply",
            default=0,
            min=0,
            max=1,
            description="Multiply for he Weight"
            )

    def execute(self, context):
        obj = bpy.context.active_object
        try:
            group_id = obj.vertex_groups.active_index
            for v in obj.data.vertices:
                val = v.groups[group_id].weight
                v.groups[group_id].weight = (self.amp * (sin(val * self.freq) -
                                            self.midlevel) / 2 + 0.5 + self.add * val) * \
                                            (1 - (1 - val) * self.mult)
        except:
            self.report({'ERROR'}, "Active object doesn't have vertex groups")

            return {'CANCELLED'}

        bpy.ops.object.mode_set(mode='WEIGHT_PAINT')

        return {'FINISHED'}


class colors_groups_exchanger_panel(bpy.types.Panel):
    bl_label = "Tissue Tools"
    bl_category = "Tools"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        try:
            if bpy.context.active_object.type == 'MESH':
                layout = self.layout
                col = layout.column(align=True)
                col.label(text="Transform:")
                col.operator("object.dual_mesh")
                col.separator()
                col.label(text="Weight from:")
                col.operator(
                    "object.vertex_colors_to_vertex_groups", icon="GROUP_VCOL")
                col.operator("object.face_area_to_vertex_groups", icon="SNAP_FACE")
                col.operator("object.curvature_to_vertex_groups", icon="SMOOTHCURVE")
                col.operator("object.harmonic_weight", icon="IPO_ELASTIC")
                col.separator()
                col.label(text="Vertex Color from:")
                col.operator("object.vertex_group_to_vertex_colors", icon="GROUP_VERTEX")
        except:
            pass


def register():
    bpy.utils.register_class(vertex_colors_to_vertex_groups)
    bpy.utils.register_class(vertex_group_to_vertex_colors)
    bpy.utils.register_class(face_area_to_vertex_groups)
    bpy.utils.register_class(colors_groups_exchanger_panel)
    bpy.utils.register_class(harmonic_weight)


def unregister():
    bpy.utils.unregister_class(vertex_colors_to_vertex_groups)
    bpy.utils.unregister_class(vertex_group_to_vertex_colors)
    bpy.utils.unregister_class(face_area_to_vertex_groups)
    bpy.utils.unregister_class(colors_groups_exchanger_panel)
    bpy.utils.unregister_class(harmonic_weight)


if __name__ == "__main__":
    register()
