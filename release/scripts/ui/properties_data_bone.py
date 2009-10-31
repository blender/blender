# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

# <pep8 compliant>
import bpy


class BoneButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "bone"

    def poll(self, context):
        return (context.bone or context.edit_bone)


class BONE_PT_context_bone(BoneButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        bone = context.bone
        if not bone:
            bone = context.edit_bone

        row = layout.row()
        row.itemL(text="", icon='ICON_BONE_DATA')
        row.itemR(bone, "name", text="")


class BONE_PT_transform(BoneButtonsPanel):
    bl_label = "Transform"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        if not bone:
            bone = context.edit_bone

            row = layout.row()
            row.column().itemR(bone, "head")
            row.column().itemR(bone, "tail")

            col = row.column()
            sub = col.column(align=True)
            sub.itemL(text="Roll:")
            sub.itemR(bone, "roll", text="")
            sub.itemL()
            sub.itemR(bone, "locked")

        else:
            pchan = ob.pose.pose_channels[context.bone.name]

            row = layout.row()
            col = row.column()
            col.itemR(pchan, "location")
            col.active = not (bone.parent and bone.connected)

            col = row.column()
            if pchan.rotation_mode == 'QUATERNION':
                col.itemR(pchan, "rotation_quaternion", text="Rotation")
            elif pchan.rotation_mode == 'AXIS_ANGLE':
                #col.itemL(text="Rotation")
                #col.itemR(pchan, "rotation_angle", text="Angle")
                #col.itemR(pchan, "rotation_axis", text="Axis")
                col.itemR(pchan, "rotation_axis_angle", text="Rotation")
            else:
                col.itemR(pchan, "rotation_euler", text="Rotation")

            row.column().itemR(pchan, "scale")

            layout.itemR(pchan, "rotation_mode")


class BONE_PT_transform_locks(BoneButtonsPanel):
    bl_label = "Transform Locks"
    bl_default_closed = True

    def poll(self, context):
        return context.bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        pchan = ob.pose.pose_channels[context.bone.name]

        row = layout.row()
        col = row.column()
        col.itemR(pchan, "lock_location")
        col.active = not (bone.parent and bone.connected)

        col = row.column()
        if pchan.rotation_mode in ('QUATERNION', 'AXIS_ANGLE'):
            col.itemR(pchan, "lock_rotations_4d", text="Lock Rotation")
            if pchan.lock_rotations_4d:
                col.itemR(pchan, "lock_rotation_w", text="W")
            col.itemR(pchan, "lock_rotation", text="")
        else:
            col.itemR(pchan, "lock_rotation", text="Rotation")

        row.column().itemR(pchan, "lock_scale")


class BONE_PT_relations(BoneButtonsPanel):
    bl_label = "Relations"

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        arm = context.armature

        if not bone:
            bone = context.edit_bone
            pchan = None
        else:
            pchan = ob.pose.pose_channels[context.bone.name]

        split = layout.split()

        col = split.column()
        col.itemL(text="Layers:")
        col.itemR(bone, "layer", text="")

        col.itemS()

        if ob and pchan:
            col.itemL(text="Bone Group:")
            col.item_pointerR(pchan, "bone_group", ob.pose, "bone_groups", text="")

        col = split.column()
        col.itemL(text="Parent:")
        if context.bone:
            col.itemR(bone, "parent", text="")
        else:
            col.item_pointerR(bone, "parent", arm, "edit_bones", text="")

        sub = col.column()
        sub.active = bone.parent != None
        sub.itemR(bone, "connected")
        sub.itemR(bone, "hinge", text="Inherit Rotation")
        sub.itemR(bone, "inherit_scale", text="Inherit Scale")


class BONE_PT_display(BoneButtonsPanel):
    bl_label = "Display"

    def poll(self, context):
        return context.bone

    def draw(self, context):
        layout = self.layout

        ob = context.object
        bone = context.bone
        arm = context.armature

        if not bone:
            bone = context.edit_bone
            pchan = None
        else:
            pchan = ob.pose.pose_channels[context.bone.name]

        if ob and pchan:

            split = layout.split()

            col = split.column()

            col.itemR(bone, "draw_wire", text="Wireframe")
            col.itemR(bone, "hidden", text="Hide")

            col = split.column()

            col.itemL(text="Custom Shape:")
            col.itemR(pchan, "custom_shape", text="")


class BONE_PT_deform(BoneButtonsPanel):
    bl_label = "Deform"
    bl_default_closed = True

    def draw_header(self, context):
        bone = context.bone

        if not bone:
            bone = context.edit_bone

        self.layout.itemR(bone, "deform", text="")

    def draw(self, context):
        layout = self.layout

        bone = context.bone

        if not bone:
            bone = context.edit_bone

        layout.active = bone.deform

        split = layout.split()

        col = split.column()
        col.itemL(text="Envelope:")

        sub = col.column(align=True)
        sub.itemR(bone, "envelope_distance", text="Distance")
        sub.itemR(bone, "envelope_weight", text="Weight")
        col.itemR(bone, "multiply_vertexgroup_with_envelope", text="Multiply")

        sub = col.column(align=True)
        sub.itemL(text="Radius:")
        sub.itemR(bone, "head_radius", text="Head")
        sub.itemR(bone, "tail_radius", text="Tail")

        col = split.column()
        col.itemL(text="Curved Bones:")

        sub = col.column(align=True)
        sub.itemR(bone, "bbone_segments", text="Segments")
        sub.itemR(bone, "bbone_in", text="Ease In")
        sub.itemR(bone, "bbone_out", text="Ease Out")

        col.itemL(text="Offset:")
        col.itemR(bone, "cyclic_offset")

bpy.types.register(BONE_PT_context_bone)
bpy.types.register(BONE_PT_transform)
bpy.types.register(BONE_PT_transform_locks)
bpy.types.register(BONE_PT_relations)
bpy.types.register(BONE_PT_display)
bpy.types.register(BONE_PT_deform)
