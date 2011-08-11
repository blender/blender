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

from bpy.props import *
from bpy import *
import mocap_constraints
import retarget
import mocap_tools

### reloads modules (for testing purposes only)
from imp import reload
reload(mocap_constraints)
reload(retarget)
reload(mocap_tools)

from mocap_constraints import *

# MocapConstraint class
# Defines MocapConstraint datatype, used to add and configute mocap constraints
# Attached to Armature data


class MocapConstraint(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty(name="Name",
        default="Mocap Fix",
        description="Name of Mocap Fix",
        update=setConstraint)
    constrained_bone = bpy.props.StringProperty(name="Bone",
        default="",
        description="Constrained Bone",
        update=updateConstraintBoneType)
    constrained_boneB = bpy.props.StringProperty(name="Bone (2)",
        default="",
        description="Other Constrained Bone (optional, depends on type)",
        update=setConstraint)
    s_frame = bpy.props.IntProperty(name="S",
        default=0,
        description="Start frame of Fix",
        update=setConstraint)
    e_frame = bpy.props.IntProperty(name="E",
        default=100,
        description="End frame of Fix",
        update=setConstraint)
    smooth_in = bpy.props.IntProperty(name="In",
        default=10,
        description="Amount of frames to smooth in",
        update=setConstraint,
        min=0)
    smooth_out = bpy.props.IntProperty(name="Out",
        default=10,
        description="Amount of frames to smooth out",
        update=setConstraint,
        min=0)
    targetMesh = bpy.props.StringProperty(name="Mesh",
        default="",
        description="Target of Fix - Mesh (optional, depends on type)",
        update=setConstraint)
    active = bpy.props.BoolProperty(name="Active",
        default=True,
        description="Fix is active",
        update=setConstraint)
    show_expanded = bpy.props.BoolProperty(name="Show Expanded",
        default=True,
        description="Fix is fully shown")
    targetPoint = bpy.props.FloatVectorProperty(name="Point", size=3,
        subtype="XYZ", default=(0.0, 0.0, 0.0),
        description="Target of Fix - Point",
        update=setConstraint)
    targetDist = bpy.props.FloatProperty(name="Offset",
        default=0.0,
        description="Distance and Floor Fixes - Desired offset",
        update=setConstraint)
    targetSpace = bpy.props.EnumProperty(
        items=[("WORLD", "World Space", "Evaluate target in global space"),
            ("LOCAL", "Object space", "Evaluate target in object space"),
            ("constrained_boneB", "Other Bone Space", "Evaluate target in specified other bone space")],
        name="Space",
        description="In which space should Point type target be evaluated",
        update=setConstraint)
    type = bpy.props.EnumProperty(name="Type of constraint",
        items=[("point", "Maintain Position", "Bone is at a specific point"),
            ("freeze", "Maintain Position at frame", "Bone does not move from location specified in target frame"),
            ("floor", "Stay above", "Bone does not cross specified mesh object eg floor"),
            ("distance", "Maintain distance", "Target bones maintained specified distance")],
        description="Type of Fix",
        update=updateConstraintBoneType)
    real_constraint = bpy.props.StringProperty()
    real_constraint_bone = bpy.props.StringProperty()


bpy.utils.register_class(MocapConstraint)

bpy.types.Armature.mocap_constraints = bpy.props.CollectionProperty(type=MocapConstraint)


class AnimationStitchSettings(bpy.types.PropertyGroup):
    first_action = bpy.props.StringProperty(name="Action 1",
            description="First action in stitch")
    second_action = bpy.props.StringProperty(name="Action 2",
            description="Second action in stitch")
    blend_frame = bpy.props.IntProperty(name="Stitch frame",
            description="Frame to locate stitch on")
    blend_amount = bpy.props.IntProperty(name="Blend amount",
            description="Size of blending transitiion, on both sides of the stitch",
            default=10)
    second_offset = bpy.props.IntProperty(name="Second offset",
            description="Frame offset for 2nd animation, where it should start",
            default=10)
    stick_bone = bpy.props.StringProperty(name="Stick Bone",
            description="Bone to freeze during transition",
            default="")

bpy.utils.register_class(AnimationStitchSettings)


class MocapNLATracks(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty()
    base_track = bpy.props.StringProperty()
    auto_fix_track = bpy.props.StringProperty()
    manual_fix_track = bpy.props.StringProperty()
    stride_action = bpy.props.StringProperty()

bpy.utils.register_class(MocapNLATracks)

                    
def advancedRetargetToggle(self, context):
    enduser_obj = context.active_object
    performer_obj = [obj for obj in context.selected_objects if obj != enduser_obj]
    if enduser_obj is None or len(performer_obj) != 1:
        print("Need active and selected armatures")
        return
    else:
        performer_obj = performer_obj[0]
    if self.advancedRetarget:
        retarget.preAdvancedRetargeting(performer_obj, enduser_obj)
    else:
        retarget.cleanTempConstraints(enduser_obj)



bpy.types.Armature.stitch_settings = bpy.props.PointerProperty(type=AnimationStitchSettings)
bpy.types.Armature.active_mocap =  bpy.props.StringProperty(update=retarget.NLASystemInitialize)
bpy.types.Armature.mocapNLATracks = bpy.props.CollectionProperty(type=MocapNLATracks)
bpy.types.Armature.advancedRetarget = bpy.props.BoolProperty(default=False, update=advancedRetargetToggle)

#Update function for IK functionality. Is called when IK prop checkboxes are toggled.


def toggleIKBone(self, context):
    if self.IKRetarget:
        if not self.is_in_ik_chain:
            print(self.name + " IK toggled ON!")
            ik = self.constraints.new('IK')
            #ik the whole chain up to the root, excluding
            chainLen = 0
            for parent_bone in self.parent_recursive:
                chainLen += 1
                if hasIKConstraint(parent_bone):
                    break
                deformer_children = [child for child in parent_bone.children if child.bone.use_deform]
                #~ if len(deformer_children) > 1:
                    #~ break
            ik.chain_count = chainLen
            for bone in self.parent_recursive:
                if bone.is_in_ik_chain:
                    bone.IKRetarget = True
    else:
        print(self.name + " IK toggled OFF!")
        cnstrn_bones = []
        newChainLength = []
        if hasIKConstraint(self):
            cnstrn_bones = [self]
        elif self.is_in_ik_chain:
            cnstrn_bones = [child for child in self.children_recursive if hasIKConstraint(child)]
            for cnstrn_bone in cnstrn_bones:
                newChainLength.append(cnstrn_bone.parent_recursive.index(self) + 1)
        if cnstrn_bones:
            # remove constraint, and update IK retarget for all parents of cnstrn_bone up to chain_len
            for i, cnstrn_bone in enumerate(cnstrn_bones):
                print(cnstrn_bone.name)
                if newChainLength:
                    ik = hasIKConstraint(cnstrn_bone)
                    ik.chain_count = newChainLength[i]
                else:
                    ik = hasIKConstraint(cnstrn_bone)
                    cnstrn_bone.constraints.remove(ik)
                    cnstrn_bone.IKRetarget = False
            for bone in cnstrn_bone.parent_recursive:
                if not bone.is_in_ik_chain:
                    bone.IKRetarget = False
        

class MocapMapping(bpy.types.PropertyGroup):
    name = bpy.props.StringProperty()

bpy.utils.register_class(MocapMapping)

bpy.types.Bone.map = bpy.props.StringProperty()
bpy.types.Bone.reverseMap = bpy.props.CollectionProperty(type=MocapMapping)
bpy.types.Bone.foot = bpy.props.BoolProperty(name="Foot",
    description="Marks this bone as a 'foot', which determines retargeted animation's translation",
    default=False)
bpy.types.PoseBone.IKRetarget = bpy.props.BoolProperty(name="IK",
    description="Toggles IK Retargeting method for given bone",
    update=toggleIKBone, default=False)


def updateIKRetarget():
    # ensures that Blender constraints and IK properties are in sync
    # currently runs when module is loaded, should run when scene is loaded
    # or user adds a constraint to armature. Will be corrected in the future,
    # once python callbacks are implemented
    for obj in bpy.data.objects:
        if obj.pose:
            bones = obj.pose.bones
            for pose_bone in bones:
                if pose_bone.is_in_ik_chain or hasIKConstraint(pose_bone):
                    pose_bone.IKRetarget = True
                else:
                    pose_bone.IKRetarget = False

updateIKRetarget()


class MocapPanel(bpy.types.Panel):
    # Motion capture retargeting panel
    bl_label = "Mocap tools"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        self.layout.label("Preprocessing")
        row = self.layout.row(align=True)
        row.alignment = 'EXPAND'
        row.operator("mocap.samples", text='Samples to Beziers')
        row.operator("mocap.denoise", text='Clean noise')
        row.operator("mocap.rotate_fix", text='Fix BVH Axis Orientation')
        row.operator("mocap.scale_fix", text='Auto scale Performer')
        row2 = self.layout.row(align=True)
        row2.operator("mocap.looper", text='Loop animation')
        row2.operator("mocap.limitdof", text='Constrain Rig')
        row2.operator("mocap.removelimitdof", text='Unconstrain Rig')
        self.layout.label("Retargeting")
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj]
        if enduser_obj is None or len(performer_obj) != 1:
            self.layout.label("Select performer rig and target rig (as active)")
        else:
            self.layout.operator("mocap.guessmapping", text="Guess Hiearchy Mapping")
            row3 = self.layout.row(align=True)
            column1 = row3.column(align=True)
            column1.label("Performer Rig")
            column2 = row3.column(align=True)
            column2.label("Enduser Rig")
            performer_obj = performer_obj[0]
            if performer_obj.data and enduser_obj.data:
                if performer_obj.data.name in bpy.data.armatures and enduser_obj.data.name in bpy.data.armatures:
                    perf = performer_obj.data
                    enduser_arm = enduser_obj.data
                    perf_pose_bones = enduser_obj.pose.bones
                    for bone in perf.bones:
                        row = self.layout.row()
                        row.prop(data=bone, property='foot', text='', icon='POSE_DATA')
                        row.label(bone.name)
                        row.prop_search(bone, "map", enduser_arm, "bones")
                        label_mod = "FK"
                        if bone.map:
                            pose_bone = perf_pose_bones[bone.map]
                            if pose_bone.is_in_ik_chain:
                                label_mod = "ik chain"
                            if hasIKConstraint(pose_bone):
                                label_mod = "ik end"
                            row.prop(pose_bone, 'IKRetarget')
                            row.label(label_mod)
                        else:
                            row.label(" ")
                            row.label(" ")
                    mapRow = self.layout.row()
                    mapRow.operator("mocap.savemapping", text='Save mapping')
                    mapRow.operator("mocap.loadmapping", text='Load mapping')
                    self.layout.prop(data=performer_obj.animation_data.action, property='name', text='Action Name')
                    self.layout.prop(enduser_arm, "advancedRetarget", text='Advanced Retarget')
                    self.layout.operator("mocap.retarget", text='RETARGET!')


class MocapConstraintsPanel(bpy.types.Panel):
    #Motion capture constraints panel
    bl_label = "Mocap Fixes"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        if context.active_object:
            if context.active_object.data:
                if context.active_object.data.name in bpy.data.armatures:
                    enduser_obj = context.active_object
                    enduser_arm = enduser_obj.data
                    layout.operator_menu_enum("mocap.addmocapfix", "type")
                    layout.operator("mocap.updateconstraints", text='Update Fixes')
                    bakeRow = layout.row()
                    bakeRow.operator("mocap.bakeconstraints", text='Bake Fixes')
                    bakeRow.operator("mocap.unbakeconstraints", text='Unbake Fixes')
                    layout.separator()
                    for i, m_constraint in enumerate(enduser_arm.mocap_constraints):
                        box = layout.box()
                        headerRow = box.row()
                        headerRow.prop(m_constraint, 'show_expanded', text='', icon='TRIA_DOWN' if m_constraint.show_expanded else 'TRIA_RIGHT', emboss=False)
                        headerRow.prop(m_constraint, 'type', text='')
                        headerRow.prop(m_constraint, 'name', text='')
                        headerRow.prop(m_constraint, 'active', icon='MUTE_IPO_ON' if not m_constraint.active else'MUTE_IPO_OFF', text='', emboss=False)
                        headerRow.operator("mocap.removeconstraint", text="", icon='X', emboss=False).constraint = i
                        if m_constraint.show_expanded:
                            box.separator()
                            box.prop_search(m_constraint, 'constrained_bone', enduser_obj.pose, "bones", icon='BONE_DATA')
                            if m_constraint.type == "distance" or m_constraint.type == "point":
                                box.prop_search(m_constraint, 'constrained_boneB', enduser_obj.pose, "bones", icon='CONSTRAINT_BONE')
                            frameRow = box.row()
                            frameRow.label("Frame Range:")
                            frameRow.prop(m_constraint, 's_frame')
                            frameRow.prop(m_constraint, 'e_frame')
                            smoothRow = box.row()
                            smoothRow.label("Smoothing:")
                            smoothRow.prop(m_constraint, 'smooth_in')
                            smoothRow.prop(m_constraint, 'smooth_out')
                            targetRow = box.row()
                            targetLabelCol = targetRow.column()
                            targetLabelCol.label("Target settings:")
                            targetPropCol = targetRow.column()
                            if m_constraint.type == "floor":
                                targetPropCol.prop_search(m_constraint, 'targetMesh', bpy.data, "objects")
                            if m_constraint.type == "point" or m_constraint.type == "freeze":
                                box.prop(m_constraint, 'targetSpace')
                            if m_constraint.type == "point":
                                targetPropCol.prop(m_constraint, 'targetPoint')
                            if m_constraint.type == "distance" or m_constraint.type == "floor":
                                targetPropCol.prop(m_constraint, 'targetDist')
                            layout.separator()


class ExtraToolsPanel(bpy.types.Panel):
    # Motion capture retargeting panel
    bl_label = "Extra Mocap Tools"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        layout.operator('mocap.pathediting', text="Follow Path")
        layout.label("Animation Stitching")
        activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        if activeIsArmature:
            enduser_arm = context.active_object.data
            layout.label("Retargeted Animations:")
            layout.prop_search(enduser_arm, "active_mocap",enduser_arm, "mocapNLATracks")
            settings = enduser_arm.stitch_settings
            layout.prop_search(settings, "first_action", enduser_arm, "mocapNLATracks")
            layout.prop_search(settings, "second_action", enduser_arm, "mocapNLATracks")
            layout.prop(settings, "blend_frame")
            layout.prop(settings, "blend_amount")
            layout.prop(settings, "second_offset")
            layout.prop_search(settings, "stick_bone", context.active_object.pose, "bones")
            layout.operator('mocap.animstitch', text="Stitch Animations")


class OBJECT_OT_RetargetButton(bpy.types.Operator):
    '''Retarget animation from selected armature to active armature '''
    bl_idname = "mocap.retarget"
    bl_label = "Retargets active action from Performer to Enduser"

    def execute(self, context):
        scene = context.scene
        s_frame = scene.frame_start
        e_frame = scene.frame_end
        enduser_obj = context.active_object
        performer_obj = [obj for obj in context.selected_objects if obj != enduser_obj]
        if enduser_obj is None or len(performer_obj) != 1:
            print("Need active and selected armatures")
        else:
            performer_obj = performer_obj[0]
            s_frame, e_frame = performer_obj.animation_data.action.frame_range
            s_frame = int(s_frame)
            e_frame = int(e_frame)
        retarget.totalRetarget(performer_obj, enduser_obj, scene, s_frame, e_frame)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False
            
    
    #~ class OBJECT_OT_AdvancedRetargetButton(bpy.types.Operator):
        #~ '''Prepare for advanced retargeting '''
        #~ bl_idname = "mocap.preretarget"
        #~ bl_label = "Prepares retarget of active action from Performer to Enduser"

        #~ def execute(self, context):
            #~ scene = context.scene
            #~ s_frame = scene.frame_start
            #~ e_frame = scene.frame_end
            #~ enduser_obj = context.active_object
            #~ performer_obj = [obj for obj in context.selected_objects if obj != enduser_obj]
            #~ if enduser_obj is None or len(performer_obj) != 1:
                #~ print("Need active and selected armatures")
            #~ else:
                #~ performer_obj = performer_obj[0]
            #~ retarget.preAdvancedRetargeting(performer_obj, enduser_obj)
            #~ return {"FINISHED"}

        #~ @classmethod
        #~ def poll(cls, context):
            #~ if context.active_object:
                #~ activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
            #~ performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
            #~ if performer_obj:
                #~ return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
            #~ else:
                #~ return False


class OBJECT_OT_SaveMappingButton(bpy.types.Operator):
    '''Save mapping to active armature (for future retargets) '''
    bl_idname = "mocap.savemapping"
    bl_label = "Saves user generated mapping from Performer to Enduser"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        retarget.createDictionary(performer_obj.data, enduser_obj.data)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False


class OBJECT_OT_LoadMappingButton(bpy.types.Operator):
    '''Load saved mapping from active armature'''
    bl_idname = "mocap.loadmapping"
    bl_label = "Loads user generated mapping from Performer to Enduser"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        retarget.loadMapping(performer_obj.data, enduser_obj.data)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False


class OBJECT_OT_ConvertSamplesButton(bpy.types.Operator):
    '''Convert active armature's sampled keyframed to beziers'''
    bl_idname = "mocap.samples"
    bl_label = "Converts samples / simplifies keyframes to beziers"

    def execute(self, context):
        mocap_tools.fcurves_simplify(context, context.active_object)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        return context.active_object.animation_data


class OBJECT_OT_LooperButton(bpy.types.Operator):
    '''Trim active armature's animation to a single cycle, given a cyclic animation (such as a walk cycle)'''
    bl_idname = "mocap.looper"
    bl_label = "loops animation / sampled mocap data"

    def execute(self, context):
        mocap_tools.autoloop_anim()
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        return context.active_object.animation_data


class OBJECT_OT_DenoiseButton(bpy.types.Operator):
    '''Denoise active armature's animation. Good for dealing with 'bad' frames inherent in mocap animation'''
    bl_idname = "mocap.denoise"
    bl_label = "Denoises sampled mocap data "

    def execute(self, context):
        mocap_tools.denoise_median()
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        return context.active_object

    @classmethod
    def poll(cls, context):
        return context.active_object.animation_data


class OBJECT_OT_LimitDOFButton(bpy.types.Operator):
    '''Create limit constraints on the active armature from the selected armature's animation's range of motion'''
    bl_idname = "mocap.limitdof"
    bl_label = "Analyzes animations Max/Min DOF and adds hard/soft constraints"

    def execute(self, context):
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object][0]
        mocap_tools.limit_dof(context, performer_obj, context.active_object)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False


class OBJECT_OT_RemoveLimitDOFButton(bpy.types.Operator):
    '''Removes previously created limit constraints on the active armature'''
    bl_idname = "mocap.removelimitdof"
    bl_label = "Removes previously created limit constraints on the active armature"

    def execute(self, context):
        mocap_tools.limit_dof_toggle_off(context, context.active_object)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        activeIsArmature = False
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        return activeIsArmature


class OBJECT_OT_RotateFixArmature(bpy.types.Operator):
    '''Realign the active armature's axis system to match Blender (Commonly needed after bvh import)'''
    bl_idname = "mocap.rotate_fix"
    bl_label = "Rotates selected armature 90 degrees (fix for bvh import)"

    def execute(self, context):
        mocap_tools.rotate_fix_armature(context.active_object.data)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_ScaleFixArmature(bpy.types.Operator):
    '''Rescale selected armature to match the active animation, for convienence'''
    bl_idname = "mocap.scale_fix"
    bl_label = "Scales performer armature to match target armature"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        mocap_tools.scale_fix_armature(performer_obj, enduser_obj)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False


class MOCAP_OT_AddMocapFix(bpy.types.Operator):
    '''Add a post-retarget fix - useful for fixing certain artifacts following the retarget'''
    bl_idname = "mocap.addmocapfix"
    bl_label = "Add Mocap Fix to target armature"
    type = bpy.props.EnumProperty(name="Type of Fix",
    items=[("point", "Maintain Position", "Bone is at a specific point"),
        ("freeze", "Maintain Position at frame", "Bone does not move from location specified in target frame"),
        ("floor", "Stay above", "Bone does not cross specified mesh object eg floor"),
        ("distance", "Maintain distance", "Target bones maintained specified distance")],
    description="Type of fix")

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        enduser_arm = enduser_obj.data
        new_mcon = enduser_arm.mocap_constraints.add()
        new_mcon.type = self.type
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_RemoveMocapConstraint(bpy.types.Operator):
    '''Remove this post-retarget fix'''
    bl_idname = "mocap.removeconstraint"
    bl_label = "Removes fixes from target armature"
    constraint = bpy.props.IntProperty()

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        enduser_arm = enduser_obj.data
        m_constraints = enduser_arm.mocap_constraints
        m_constraint = m_constraints[self.constraint]
        if m_constraint.real_constraint:
            bone = enduser_obj.pose.bones[m_constraint.real_constraint_bone]
            cons_obj = getConsObj(bone)
            removeConstraint(m_constraint, cons_obj)
        m_constraints.remove(self.constraint)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_BakeMocapConstraints(bpy.types.Operator):
    '''Bake all post-retarget fixes to the Retarget Fixes NLA Track'''
    bl_idname = "mocap.bakeconstraints"
    bl_label = "Bake all fixes to target armature"

    def execute(self, context):
        bakeConstraints(context)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_UnbakeMocapConstraints(bpy.types.Operator):
    '''Unbake all post-retarget fixes - removes the baked data from the Retarget Fixes NLA Track'''
    bl_idname = "mocap.unbakeconstraints"
    bl_label = "Unbake all fixes to target armature"

    def execute(self, context):
        unbakeConstraints(context)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_UpdateMocapConstraints(bpy.types.Operator):
    '''Updates all post-retarget fixes - needed after changes to armature object or pose'''
    bl_idname = "mocap.updateconstraints"
    bl_label = "Updates all fixes to target armature - neccesary to take under consideration changes to armature object or pose"

    def execute(self, context):
        updateConstraints(context.active_object, context)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_GuessHierachyMapping(bpy.types.Operator):
    '''Attemps to auto figure out hierarchy mapping'''
    bl_idname = "mocap.guessmapping"
    bl_label = "Attemps to auto figure out hierarchy mapping"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        mocap_tools.guessMapping(performer_obj, enduser_obj)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False


class OBJECT_OT_PathEditing(bpy.types.Operator):
    '''Sets active object (stride object) to follow the selected curve'''
    bl_idname = "mocap.pathediting"
    bl_label = "Sets active object (stride object) to follow the selected curve"

    def execute(self, context):
        path = [obj for obj in context.selected_objects if obj != context.active_object][0]
        mocap_tools.path_editing(context, context.active_object, path)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            selected_objs = [obj for obj in context.selected_objects if obj != context.active_object and isinstance(obj.data, bpy.types.Curve)]
            return selected_objs
        else:
            return False


class OBJECT_OT_AnimationStitchingButton(bpy.types.Operator):
    '''Stitches two defined animations into a single one via alignment of NLA Tracks'''
    bl_idname = "mocap.animstitch"
    bl_label = "Stitches two defined animations into a single one via alignment of NLA Tracks"

    def execute(self, context):
        mocap_tools.anim_stitch(context, context.active_object)
        return {"FINISHED"}

    @classmethod
    def poll(cls, context):
        activeIsArmature = False
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
            if activeIsArmature:
                stitch_settings = context.active_object.data.stitch_settings
                return (stitch_settings.first_action and stitch_settings.second_action)
        return False
    

def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
