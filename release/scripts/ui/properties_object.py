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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from rna_prop_ui import PropertyPanel

narrowui = 180


class ObjectButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"


class OBJECT_PT_context_object(ObjectButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        ob = context.object

        row = layout.row()
        row.label(text="", icon='OBJECT_DATA')
        row.prop(ob, "name", text="")


class OBJECT_PT_custom_props(ObjectButtonsPanel, PropertyPanel):
    _context_path = "object"


class OBJECT_PT_transform(ObjectButtonsPanel):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        if wide_ui:
            row = layout.row()

            row.column().prop(ob, "location")
            if ob.rotation_mode == 'QUATERNION':
                row.column().prop(ob, "rotation_quaternion", text="Rotation")
            elif ob.rotation_mode == 'AXIS_ANGLE':
                #row.column().label(text="Rotation")
                #row.column().prop(pchan, "rotation_angle", text="Angle")
                #row.column().prop(pchan, "rotation_axis", text="Axis")
                row.column().prop(ob, "rotation_axis_angle", text="Rotation")
            else:
                row.column().prop(ob, "rotation_euler", text="Rotation")

            row.column().prop(ob, "scale")

            layout.prop(ob, "rotation_mode")
        else:
            col = layout.column()
            col.prop(ob, "location")
            col.label(text="Rotation:")
            col.prop(ob, "rotation_mode", text="")
            if ob.rotation_mode == 'QUATERNION':
                col.prop(ob, "rotation_quaternion", text="")
            elif ob.rotation_mode == 'AXIS_ANGLE':
                col.prop(ob, "rotation_axis_angle", text="")
            else:
                col.prop(ob, "rotation_euler", text="")
            col.prop(ob, "scale")


class OBJECT_PT_transform_locks(ObjectButtonsPanel):
    bl_label = "Transform Locks"
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        ob = context.object
        # wide_ui = context.region.width > narrowui

        row = layout.row()

        col = row.column()
        col.prop(ob, "lock_location", text="Location")

        col = row.column()
        if ob.rotation_mode in ('QUATERNION', 'AXIS_ANGLE'):
            col.prop(ob, "lock_rotations_4d", text="Rotation")
            if ob.lock_rotations_4d:
                col.prop(ob, "lock_rotation_w", text="W")
            col.prop(ob, "lock_rotation", text="")
        else:
            col.prop(ob, "lock_rotation", text="Rotation")

        row.column().prop(ob, "lock_scale", text="Scale")


class OBJECT_PT_relations(ObjectButtonsPanel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(ob, "layers")
        col.separator()
        col.prop(ob, "pass_index")

        if wide_ui:
            col = split.column()
        col.label(text="Parent:")
        col.prop(ob, "parent", text="")

        sub = col.column()
        sub.prop(ob, "parent_type", text="")
        parent = ob.parent
        if parent and ob.parent_type == 'BONE' and parent.type == 'ARMATURE':
            sub.prop_object(ob, "parent_bone", parent.data, "bones", text="")
        sub.active = (parent is not None)


class OBJECT_PT_groups(ObjectButtonsPanel):
    bl_label = "Groups"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        if wide_ui:
            split = layout.split()
            split.operator_menu_enum("object.group_add", "group")
            split.label()
        else:
            layout.operator_menu_enum("object.group_add", "group")

        index = 0
        value = str(tuple(context.scene.cursor_location))
        for group in bpy.data.groups:
            if ob.name in group.objects:
                col = layout.column(align=True)

                col.set_context_pointer("group", group)

                row = col.box().row()
                row.prop(group, "name", text="")
                row.operator("object.group_remove", text="", icon='X')

                split = col.box().split()

                col = split.column()
                col.prop(group, "layer", text="Dupli")

                if wide_ui:
                    col = split.column()
                col.prop(group, "dupli_offset", text="")

                prop = col.operator("wm.context_set_value", text="From Cursor")
                prop.path = "object.group_users[%d].dupli_offset" % index
                prop.value = value
                index += 1


class OBJECT_PT_display(ObjectButtonsPanel):
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        split = layout.split()
        col = split.column()
        col.prop(ob, "max_draw_type", text="Type")

        if wide_ui:
            col = split.column()
        row = col.row()
        row.prop(ob, "draw_bounds", text="Bounds")
        sub = row.row()
        sub.active = ob.draw_bounds
        sub.prop(ob, "draw_bounds_type", text="")

        split = layout.split()

        col = split.column()
        col.prop(ob, "draw_name", text="Name")
        col.prop(ob, "draw_axis", text="Axis")
        col.prop(ob, "draw_wire", text="Wire")

        if wide_ui:
            col = split.column()
        col.prop(ob, "draw_texture_space", text="Texture Space")
        col.prop(ob, "x_ray", text="X-Ray")
        col.prop(ob, "draw_transparent", text="Transparency")


class OBJECT_PT_duplication(ObjectButtonsPanel):
    bl_label = "Duplication"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(ob, "dupli_type", expand=True)
        else:
            layout.prop(ob, "dupli_type", text="")

        if ob.dupli_type == 'FRAMES':
            split = layout.split()

            col = split.column(align=True)
            col.prop(ob, "dupli_frames_start", text="Start")
            col.prop(ob, "dupli_frames_end", text="End")

            if wide_ui:
                col = split.column(align=True)
            col.prop(ob, "dupli_frames_on", text="On")
            col.prop(ob, "dupli_frames_off", text="Off")

            layout.prop(ob, "use_dupli_frames_speed", text="Speed")

        elif ob.dupli_type == 'VERTS':
            layout.prop(ob, "use_dupli_verts_rotation", text="Rotation")

        elif ob.dupli_type == 'FACES':
            split = layout.split()

            col = split.column()
            col.prop(ob, "use_dupli_faces_scale", text="Scale")

            if wide_ui:
                col = split.column()
            col.prop(ob, "dupli_faces_scale", text="Inherit Scale")

        elif ob.dupli_type == 'GROUP':
            if wide_ui:
                layout.prop(ob, "dupli_group", text="Group")
            else:
                layout.prop(ob, "dupli_group", text="")


class OBJECT_PT_animation(ObjectButtonsPanel):
    bl_label = "Animation"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Time Offset:")
        col.prop(ob, "time_offset_edit", text="Edit")
        row = col.row()
        row.prop(ob, "time_offset_particle", text="Particle")
        row.active = len(ob.particle_systems) != 0
        row = col.row()
        row.prop(ob, "time_offset_parent", text="Parent")
        row.active = (ob.parent is not None)
        row = col.row()
        row.prop(ob, "slow_parent")
        row.active = (ob.parent is not None)
        col.prop(ob, "time_offset", text="Offset")

        if wide_ui:
            col = split.column()
        col.label(text="Track:")
        col.prop(ob, "track", text="")
        col.prop(ob, "track_axis", text="Axis")
        col.prop(ob, "up_axis", text="Up Axis")
        row = col.row()
        row.prop(ob, "track_override_parent", text="Override Parent")
        row.active = (ob.parent is not None)


bpy.types.register(OBJECT_PT_context_object)
bpy.types.register(OBJECT_PT_transform)
bpy.types.register(OBJECT_PT_transform_locks)
bpy.types.register(OBJECT_PT_relations)
bpy.types.register(OBJECT_PT_groups)
bpy.types.register(OBJECT_PT_display)
bpy.types.register(OBJECT_PT_duplication)
bpy.types.register(OBJECT_PT_animation)

bpy.types.register(OBJECT_PT_custom_props)
