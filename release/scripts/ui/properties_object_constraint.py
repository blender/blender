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

narrowui = 180

class ConstraintButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "constraint"

    def draw_constraint(self, context, con):
        layout = self.layout

        box = layout.template_constraint(con)
        wide_ui = context.region.width > narrowui

        if box:
            # match enum type to our functions, avoids a lookup table.
            getattr(self, con.type)(context, box, con, wide_ui)

            # show/key buttons here are most likely obsolete now, with
            # keyframing functionality being part of every button
            if con.type not in ('RIGID_BODY_JOINT', 'SPLINE_IK', 'NULL'):
                box.itemR(con, "influence")

    def space_template(self, layout, con, wide_ui, target=True, owner=True):
        if target or owner:

            split = layout.split(percentage=0.2)

            if wide_ui:
                split.itemL(text="Space:")
                row = split.row()
            else:
                row = layout.row()


            if target:
                row.itemR(con, "target_space", text="")

            if wide_ui:
                if target and owner:
                    row.itemL(icon='ICON_ARROW_LEFTRIGHT')
            else:
                row = layout.row()
            if owner:
                row.itemR(con, "owner_space", text="")

    def target_template(self, layout, con, wide_ui, subtargets=True):
        if wide_ui:
            layout.itemR(con, "target") # XXX limiting settings for only 'curves' or some type of object
        else:
            layout.itemR(con, "target", text="")

        if con.target and subtargets:
            if con.target.type == 'ARMATURE':
                if wide_ui:
                    layout.item_pointerR(con, "subtarget", con.target.data, "bones", text="Bone")
                else:
                    layout.item_pointerR(con, "subtarget", con.target.data, "bones", text="")

                if con.type == 'COPY_LOCATION':
                    row = layout.row()
                    row.itemL(text="Head/Tail:")
                    row.itemR(con, "head_tail", text="")
            elif con.target.type in ('MESH', 'LATTICE'):
                layout.item_pointerR(con, "subtarget", con.target, "vertex_groups", text="Vertex Group")

    def ik_template(self, layout, con, wide_ui):
        # only used for iTaSC
        layout.itemR(con, "pole_target")

        if con.pole_target and con.pole_target.type == 'ARMATURE':
            layout.item_pointerR(con, "pole_subtarget", con.pole_target.data, "bones", text="Bone")

        if con.pole_target:
            row = layout.row()
            row.itemL()
            row.itemR(con, "pole_angle")

        split = layout.split(percentage=0.33)
        col = split.column()
        col.itemR(con, "tail")
        col.itemR(con, "stretch")

        col = split.column()
        col.itemR(con, "chain_length")
        col.itemR(con, "targetless")

    def CHILD_OF(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        split = layout.split()

        col = split.column()
        col.itemL(text="Location:")
        col.itemR(con, "locationx", text="X")
        col.itemR(con, "locationy", text="Y")
        col.itemR(con, "locationz", text="Z")

        col = split.column()
        col.itemL(text="Rotation:")
        col.itemR(con, "rotationx", text="X")
        col.itemR(con, "rotationy", text="Y")
        col.itemR(con, "rotationz", text="Z")

        col = split.column()
        col.itemL(text="Scale:")
        col.itemR(con, "sizex", text="X")
        col.itemR(con, "sizey", text="Y")
        col.itemR(con, "sizez", text="Z")

        split = layout.split()

        col = split.column()
        col.itemO("constraint.childof_set_inverse")

        if wide_ui:
            col = split.column()
        col.itemO("constraint.childof_clear_inverse")

    def TRACK_TO(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        row = layout.row()
        if wide_ui:
            row.itemL(text="To:")
        row.itemR(con, "track", expand=True)

        split = layout.split()

        col = split.column()
        col.itemR(con, "up", text="Up")

        if wide_ui:
            col = split.column()
        col.itemR(con, "target_z")

        self.space_template(layout, con, wide_ui)

    def IK(self, context, layout, con, wide_ui):
        if context.object.pose.ik_solver == "ITASC":
            layout.itemR(con, "ik_type")
            getattr(self, 'IK_' + con.ik_type)(context, layout, con)
        else:
            # Legacy IK constraint
            self.target_template(layout, con, wide_ui)
            if wide_ui:
                layout.itemR(con, "pole_target")
            else:
                layout.itemR(con, "pole_target", text="")
            if con.pole_target and con.pole_target.type == 'ARMATURE':
                if wide_ui:
                    layout.item_pointerR(con, "pole_subtarget", con.pole_target.data, "bones", text="Bone")
                else:
                    layout.item_pointerR(con, "pole_subtarget", con.pole_target.data, "bones", text="")

            if con.pole_target:
                row = layout.row()
                row.itemR(con, "pole_angle")
                if wide_ui:
                    row.itemL()

            split = layout.split()
            col = split.column()
            col.itemR(con, "iterations")
            col.itemR(con, "chain_length")

            col.itemL(text="Weight:")
            col.itemR(con, "weight", text="Position", slider=True)
            sub = col.column()
            sub.active = con.rotation
            sub.itemR(con, "orient_weight", text="Rotation", slider=True)

            if wide_ui:
                col = split.column()
            col.itemR(con, "tail")
            col.itemR(con, "stretch")
            col.itemS()
            col.itemR(con, "targetless")
            col.itemR(con, "rotation")

    def IK_COPY_POSE(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)
        self.ik_template(layout, con, wide_ui)

        row = layout.row()
        row.itemL(text="Axis Ref:")
        row.itemR(con, "axis_reference", expand=True)
        split = layout.split(percentage=0.33)
        split.row().itemR(con, "position")
        row = split.row()
        row.itemR(con, "weight", text="Weight", slider=True)
        row.active = con.position
        split = layout.split(percentage=0.33)
        row = split.row()
        row.itemL(text="Lock:")
        row = split.row()
        row.itemR(con, "pos_lock_x", text="X")
        row.itemR(con, "pos_lock_y", text="Y")
        row.itemR(con, "pos_lock_z", text="Z")
        split.active = con.position

        split = layout.split(percentage=0.33)
        split.row().itemR(con, "rotation")
        row = split.row()
        row.itemR(con, "orient_weight", text="Weight", slider=True)
        row.active = con.rotation
        split = layout.split(percentage=0.33)
        row = split.row()
        row.itemL(text="Lock:")
        row = split.row()
        row.itemR(con, "rot_lock_x", text="X")
        row.itemR(con, "rot_lock_y", text="Y")
        row.itemR(con, "rot_lock_z", text="Z")
        split.active = con.rotation

    def IK_DISTANCE(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)
        self.ik_template(layout, con, wide_ui)

        layout.itemR(con, "limit_mode")
        row = layout.row()
        row.itemR(con, "weight", text="Weight", slider=True)
        row.itemR(con, "distance", text="Distance", slider=True)

    def FOLLOW_PATH(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        split = layout.split()

        col = split.column()
        col.itemR(con, "use_curve_follow")
        col.itemR(con, "use_curve_radius")

        if wide_ui:
            col = split.column()
        col.itemR(con, "use_fixed_position")
        if con.use_fixed_position:
            col.itemR(con, "offset_factor", text="Offset")
        else:
            col.itemR(con, "offset")

        row = layout.row()
        if wide_ui:
            row.itemL(text="Forward:")
        row.itemR(con, "forward", expand=True)

        row = layout.row()
        row.itemR(con, "up", text="Up")
        if wide_ui:
            row.itemL()

    def LIMIT_ROTATION(self, context, layout, con, wide_ui):

        split = layout.split()

        col = split.column(align=True)
        col.itemR(con, "use_limit_x")
        sub = col.column()
        sub.active = con.use_limit_x
        sub.itemR(con, "minimum_x", text="Min")
        sub.itemR(con, "maximum_x", text="Max")

        if wide_ui:
            col = split.column(align=True)
        col.itemR(con, "use_limit_y")
        sub = col.column()
        sub.active = con.use_limit_y
        sub.itemR(con, "minimum_y", text="Min")
        sub.itemR(con, "maximum_y", text="Max")

        if wide_ui:
            col = split.column(align=True)
        col.itemR(con, "use_limit_z")
        sub = col.column()
        sub.active = con.use_limit_z
        sub.itemR(con, "minimum_z", text="Min")
        sub.itemR(con, "maximum_z", text="Max")

        row = layout.row()
        row.itemR(con, "limit_transform")
        if wide_ui:
            row.itemL()

        row = layout.row()
        if wide_ui:
            row.itemL(text="Convert:")
        row.itemR(con, "owner_space", text="")

    def LIMIT_LOCATION(self, context, layout, con, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemR(con, "use_minimum_x")
        sub = col.column()
        sub.active = con.use_minimum_x
        sub.itemR(con, "minimum_x", text="")
        col.itemR(con, "use_maximum_x")
        sub = col.column()
        sub.active = con.use_maximum_x
        sub.itemR(con, "maximum_x", text="")

        if wide_ui:
            col = split.column()
        col.itemR(con, "use_minimum_y")
        sub = col.column()
        sub.active = con.use_minimum_y
        sub.itemR(con, "minimum_y", text="")
        col.itemR(con, "use_maximum_y")
        sub = col.column()
        sub.active = con.use_maximum_y
        sub.itemR(con, "maximum_y", text="")

        if wide_ui:
            col = split.column()
        col.itemR(con, "use_minimum_z")
        sub = col.column()
        sub.active = con.use_minimum_z
        sub.itemR(con, "minimum_z", text="")
        col.itemR(con, "use_maximum_z")
        sub = col.column()
        sub.active = con.use_maximum_z
        sub.itemR(con, "maximum_z", text="")

        row = layout.row()
        row.itemR(con, "limit_transform")
        if wide_ui:
            row.itemL()

        row = layout.row()
        if wide_ui:
            row.itemL(text="Convert:")
        row.itemR(con, "owner_space", text="")

    def LIMIT_SCALE(self, context, layout, con, wide_ui):
        split = layout.split()

        col = split.column()
        col.itemR(con, "use_minimum_x")
        sub = col.column()
        sub.active = con.use_minimum_x
        sub.itemR(con, "minimum_x", text="")
        col.itemR(con, "use_maximum_x")
        sub = col.column()
        sub.active = con.use_maximum_x
        sub.itemR(con, "maximum_x", text="")

        if wide_ui:
            col = split.column()
        col.itemR(con, "use_minimum_y")
        sub = col.column()
        sub.active = con.use_minimum_y
        sub.itemR(con, "minimum_y", text="")
        col.itemR(con, "use_maximum_y")
        sub = col.column()
        sub.active = con.use_maximum_y
        sub.itemR(con, "maximum_y", text="")

        if wide_ui:
            col = split.column()
        col.itemR(con, "use_minimum_z")
        sub = col.column()
        sub.active = con.use_minimum_z
        sub.itemR(con, "minimum_z", text="")
        col.itemR(con, "use_maximum_z")
        sub = col.column()
        sub.active = con.use_maximum_z
        sub.itemR(con, "maximum_z", text="")

        row = layout.row()
        row.itemR(con, "limit_transform")
        if wide_ui:
            row.itemL()

        row = layout.row()
        if wide_ui:
            row.itemL(text="Convert:")
        row.itemR(con, "owner_space", text="")

    def COPY_ROTATION(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        split = layout.split()

        col = split.column()
        col.itemR(con, "rotate_like_x", text="X")
        sub = col.column()
        sub.active = con.rotate_like_x
        sub.itemR(con, "invert_x", text="Invert")

        col = split.column()
        col.itemR(con, "rotate_like_y", text="Y")
        sub = col.column()
        sub.active = con.rotate_like_y
        sub.itemR(con, "invert_y", text="Invert")

        col = split.column()
        col.itemR(con, "rotate_like_z", text="Z")
        sub = col.column()
        sub.active = con.rotate_like_z
        sub.itemR(con, "invert_z", text="Invert")

        layout.itemR(con, "offset")

        self.space_template(layout, con, wide_ui)

    def COPY_LOCATION(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        split = layout.split()

        col = split.column()
        col.itemR(con, "locate_like_x", text="X")
        sub = col.column()
        sub.active = con.locate_like_x
        sub.itemR(con, "invert_x", text="Invert")

        col = split.column()
        col.itemR(con, "locate_like_y", text="Y")
        sub = col.column()
        sub.active = con.locate_like_y
        sub.itemR(con, "invert_y", text="Invert")

        col = split.column()
        col.itemR(con, "locate_like_z", text="Z")
        sub = col.column()
        sub.active = con.locate_like_z
        sub.itemR(con, "invert_z", text="Invert")

        layout.itemR(con, "offset")

        self.space_template(layout, con, wide_ui)

    def COPY_SCALE(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        row = layout.row(align=True)
        row.itemR(con, "size_like_x", text="X")
        row.itemR(con, "size_like_y", text="Y")
        row.itemR(con, "size_like_z", text="Z")

        layout.itemR(con, "offset")

        self.space_template(layout, con, wide_ui)

    #def SCRIPT(self, context, layout, con):

    def ACTION(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        if wide_ui:
            layout.itemR(con, "action")
        else:
            layout.itemR(con, "action", text="")

        if wide_ui:
            layout.itemR(con, "transform_channel")
        else:
            layout.itemR(con, "transform_channel", text="")

        split = layout.split()

        col = split.column(align=True)
        col.itemL(text="Action Length:")
        col.itemR(con, "start_frame", text="Start")
        col.itemR(con, "end_frame", text="End")

        if wide_ui:
            col = split.column(align=True)
        col.itemL(text="Target Range:")
        col.itemR(con, "minimum", text="Min")
        col.itemR(con, "maximum", text="Max")

        row = layout.row()
        if wide_ui:
            row.itemL(text="Convert:")
        row.itemR(con, "target_space", text="")

    def LOCKED_TRACK(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        row = layout.row()
        if wide_ui:
            row.itemL(text="To:")
        row.itemR(con, "track", expand=True)

        row = layout.row()
        if wide_ui:
            row.itemL(text="Lock:")
        row.itemR(con, "locked", expand=True)

    def LIMIT_DISTANCE(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        col = layout.column(align=True)
        col.itemR(con, "distance")
        col.itemO("constraint.limitdistance_reset")

        row = layout.row()
        row.itemL(text="Clamp Region:")
        row.itemR(con, "limit_mode", text="")

    def STRETCH_TO(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        split = layout.split()

        col = split.column()
        col.itemR(con, "original_length", text="Rest Length")

        if wide_ui:
            col = split.column()
        col.itemO("constraint.stretchto_reset", text="Reset")

        col = layout.column()
        col.itemR(con, "bulge", text="Volume Variation")

        row = layout.row()
        if wide_ui:
            row.itemL(text="Volume:")
        row.itemR(con, "volume", expand=True)
        if not wide_ui:
            row = layout.row()
        row.itemL(text="Plane:")
        row.itemR(con, "keep_axis", expand=True)

    def FLOOR(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        split = layout.split()

        col = split.column()
        col.itemR(con, "sticky")

        if wide_ui:
            col = split.column()
        col.itemR(con, "use_rotation")

        layout.itemR(con, "offset")

        row = layout.row()
        if wide_ui:
            row.itemL(text="Min/Max:")
        row.itemR(con, "floor_location", expand=True)

    def RIGID_BODY_JOINT(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        if wide_ui:
            layout.itemR(con, "pivot_type")
        else:
            layout.itemR(con, "pivot_type", text="")
        if wide_ui:
            layout.itemR(con, "child")
        else:
            layout.itemR(con, "child", text="")

        split = layout.split()

        col = split.column()
        col.itemR(con, "disable_linked_collision", text="No Collision")

        if wide_ui:
            col = split.column()
        col.itemR(con, "draw_pivot", text="Display Pivot")

        split = layout.split()

        col = split.column(align=True)
        col.itemL(text="Pivot:")
        col.itemR(con, "pivot_x", text="X")
        col.itemR(con, "pivot_y", text="Y")
        col.itemR(con, "pivot_z", text="Z")

        if wide_ui:
            col = split.column(align=True)
        col.itemL(text="Axis:")
        col.itemR(con, "axis_x", text="X")
        col.itemR(con, "axis_y", text="Y")
        col.itemR(con, "axis_z", text="Z")

        #Missing: Limit arrays (not wrapped in RNA yet)

    def CLAMP_TO(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        row = layout.row()
        if wide_ui:
            row.itemL(text="Main Axis:")
        row.itemR(con, "main_axis", expand=True)

        row = layout.row()
        row.itemR(con, "cyclic")

    def TRANSFORM(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        layout.itemR(con, "extrapolate_motion", text="Extrapolate")

        col = layout.column()
        col.row().itemL(text="Source:")
        col.row().itemR(con, "map_from", expand=True)

        split = layout.split()

        sub = split.column(align=True)
        sub.itemL(text="X:")
        sub.itemR(con, "from_min_x", text="Min")
        sub.itemR(con, "from_max_x", text="Max")

        if wide_ui:
            sub = split.column(align=True)
        sub.itemL(text="Y:")
        sub.itemR(con, "from_min_y", text="Min")
        sub.itemR(con, "from_max_y", text="Max")

        if wide_ui:
            sub = split.column(align=True)
        sub.itemL(text="Z:")
        sub.itemR(con, "from_min_z", text="Min")
        sub.itemR(con, "from_max_z", text="Max")

        split = layout.split()

        col = split.column()
        col.itemL(text="Destination:")
        col.row().itemR(con, "map_to", expand=True)

        split = layout.split()

        col = split.column()
        col.itemL(text="X:")
        col.row().itemR(con, "map_to_x_from", expand=True)

        sub = col.column(align=True)
        sub.itemR(con, "to_min_x", text="Min")
        sub.itemR(con, "to_max_x", text="Max")

        if wide_ui:
            col = split.column()
        col.itemL(text="Y:")
        col.row().itemR(con, "map_to_y_from", expand=True)

        sub = col.column(align=True)
        sub.itemR(con, "to_min_y", text="Min")
        sub.itemR(con, "to_max_y", text="Max")

        if wide_ui:
            col = split.column()
        col.itemL(text="Z:")
        col.row().itemR(con, "map_to_z_from", expand=True)

        sub = col.column(align=True)
        sub.itemR(con, "to_min_z", text="Min")
        sub.itemR(con, "to_max_z", text="Max")

        self.space_template(layout, con, wide_ui)

    def SHRINKWRAP(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        layout.itemR(con, "distance")
        layout.itemR(con, "shrinkwrap_type")

        if con.shrinkwrap_type == 'PROJECT':
            row = layout.row(align=True)
            row.itemR(con, "axis_x")
            row.itemR(con, "axis_y")
            row.itemR(con, "axis_z")

    def DAMPED_TRACK(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        row = layout.row()
        if wide_ui:
            row.itemL(text="To:")
        row.itemR(con, "track", expand=True)

    def SPLINE_IK(self, context, layout, con, wide_ui):
        self.target_template(layout, con, wide_ui)

        col = layout.column()
        col.itemL(text="Spline Fitting:")
        col.itemR(con, "chain_length")
        col.itemR(con, "even_divisions")
        col.itemR(con, "chain_offset")

        col = layout.column()
        col.itemL(text="Chain Scaling:")
        col.itemR(con, "y_scaling")
        if wide_ui:
            col.itemR(con, "xz_scaling_mode")
        else:
            col.itemR(con, "xz_scaling_mode", text="")


class OBJECT_PT_constraints(ConstraintButtonsPanel):
    bl_label = "Object Constraints"
    bl_context = "constraint"

    def poll(self, context):
        return (context.object)

    def draw(self, context):
        layout = self.layout
        ob = context.object
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.item_menu_enumO("object.constraint_add", "type")
        if wide_ui:
            row.itemL()

        for con in ob.constraints:
            self.draw_constraint(context, con)


class BONE_PT_inverse_kinematics(ConstraintButtonsPanel):
    bl_label = "Inverse Kinematics"
    bl_default_closed = True
    bl_context = "bone_constraint"

    def poll(self, context):
        ob = context.object
        bone = context.bone

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
            return pchan.has_ik

        return False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        pchan = ob.pose.bones[bone.name]
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.itemR(ob.pose, "ik_solver")

        split = layout.split(percentage=0.25)
        split.itemR(pchan, "ik_dof_x", text="X")
        row = split.row()
        row.itemR(pchan, "ik_stiffness_x", text="Stiffness", slider=True)
        row.active = pchan.ik_dof_x

        if wide_ui:
            split = layout.split(percentage=0.25)
            sub = split.row()
        else:
            sub = layout.column(align=True)
        sub.itemR(pchan, "ik_limit_x", text="Limit")
        sub.active = pchan.ik_dof_x
        if wide_ui:
            sub = split.row(align=True)
        sub.itemR(pchan, "ik_min_x", text="")
        sub.itemR(pchan, "ik_max_x", text="")
        sub.active = pchan.ik_dof_x and pchan.ik_limit_x

        split = layout.split(percentage=0.25)
        split.itemR(pchan, "ik_dof_y", text="Y")
        row = split.row()
        row.itemR(pchan, "ik_stiffness_y", text="Stiffness", slider=True)
        row.active = pchan.ik_dof_y

        if wide_ui:
            split = layout.split(percentage=0.25)
            sub = split.row()
        else:
            sub = layout.column(align=True)
        sub.itemR(pchan, "ik_limit_y", text="Limit")
        sub.active = pchan.ik_dof_y
        if wide_ui:
            sub = split.row(align=True)
        sub.itemR(pchan, "ik_min_y", text="")
        sub.itemR(pchan, "ik_max_y", text="")
        sub.active = pchan.ik_dof_y and pchan.ik_limit_y

        split = layout.split(percentage=0.25)
        split.itemR(pchan, "ik_dof_z", text="Z")
        sub = split.row()
        sub.itemR(pchan, "ik_stiffness_z", text="Stiffness", slider=True)
        sub.active = pchan.ik_dof_z

        if wide_ui:
            split = layout.split(percentage=0.25)
            sub = split.row()
        else:
            sub = layout.column(align=True)
        sub.itemR(pchan, "ik_limit_z", text="Limit")
        sub.active = pchan.ik_dof_z
        if wide_ui:
            sub = split.row(align=True)
        sub.itemR(pchan, "ik_min_z", text="")
        sub.itemR(pchan, "ik_max_z", text="")
        sub.active = pchan.ik_dof_z and pchan.ik_limit_z
        split = layout.split()
        split.itemR(pchan, "ik_stretch", text="Stretch", slider=True)
        if wide_ui:
            split.itemL()

        if ob.pose.ik_solver == 'ITASC':
            split = layout.split()
            col = split.column()
            col.itemR(pchan, "ik_rot_control", text="Control Rotation")
            if wide_ui:
                col = split.column()
            col.itemR(pchan, "ik_rot_weight", text="Weight", slider=True)
            # not supported yet
            #row = layout.row()
            #row.itemR(pchan, "ik_lin_control", text="Joint Size")
            #row.itemR(pchan, "ik_lin_weight", text="Weight", slider=True)


class BONE_PT_iksolver_itasc(ConstraintButtonsPanel):
    bl_label = "iTaSC parameters"
    bl_default_closed = True
    bl_context = "bone_constraint"

    def poll(self, context):
        ob = context.object
        bone = context.bone

        if ob and bone:
            pchan = ob.pose.bones[bone.name]
            return pchan.has_ik and ob.pose.ik_solver == 'ITASC' and ob.pose.ik_param

        return False

    def draw(self, context):
        layout = self.layout

        ob = context.object
        itasc = ob.pose.ik_param
        wide_ui = context.region.width > narrowui

        layout.itemR(itasc, "mode", expand=True)
        simulation = itasc.mode == 'SIMULATION'
        if simulation:
            layout.itemL(text="Reiteration:")
            layout.itemR(itasc, "reiteration", expand=True)

        split = layout.split()
        split.active = not simulation or itasc.reiteration != 'NEVER'
        col = split.column()
        col.itemR(itasc, "precision")

        if wide_ui:
            col = split.column()
        col.itemR(itasc, "num_iter")


        if simulation:
            layout.itemR(itasc, "auto_step")
            row = layout.row()
            if itasc.auto_step:
                row.itemR(itasc, "min_step", text="Min")
                row.itemR(itasc, "max_step", text="Max")
            else:
                row.itemR(itasc, "num_step")

        layout.itemR(itasc, "solver")
        if simulation:
            layout.itemR(itasc, "feedback")
            layout.itemR(itasc, "max_velocity")
        if itasc.solver == 'DLS':
            row = layout.row()
            row.itemR(itasc, "dampmax", text="Damp", slider=True)
            row.itemR(itasc, "dampeps", text="Eps", slider=True)


class BONE_PT_constraints(ConstraintButtonsPanel):
    bl_label = "Bone Constraints"
    bl_context = "bone_constraint"

    def poll(self, context):
        ob = context.object
        return (ob and ob.type == 'ARMATURE' and context.bone)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        pchan = ob.pose.bones[context.bone.name]
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.item_menu_enumO("pose.constraint_add", "type")
        if wide_ui:
            row.itemL()

        for con in pchan.constraints:
            self.draw_constraint(context, con)

bpy.types.register(OBJECT_PT_constraints)
bpy.types.register(BONE_PT_iksolver_itasc)
bpy.types.register(BONE_PT_inverse_kinematics)
bpy.types.register(BONE_PT_constraints)
