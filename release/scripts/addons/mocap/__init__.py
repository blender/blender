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

bl_info = {
    "name": "Motion Capture Tools",
    "author": "Benjy Cook",
    "blender": (2, 73, 0),
    "version": (1, 1, 1),
    "location": "Active Armature > Object Properties > Mocap tools",
    "description": "Various tools for working with motion capture animation",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Animation/Motion_Capture_Tools",
    "support": 'OFFICIAL',
    "category": "Animation",
}

if "bpy" in locals():
    import importlib
    if "mocap_constraints" in locals():
        importlib.reload(mocap_constraints)
    if "retarget" in locals():
        importlib.reload(retarget)
    if "mocap_tools" in locals():
        importlib.reload(mocap_tools)
else:
    import bpy
    from bpy.props import (
            BoolProperty,
            CollectionProperty,
            EnumProperty,
            FloatProperty,
            FloatVectorProperty,
            IntProperty,
            PointerProperty,
            StringProperty,
            )
    from . import (
            mocap_constraints,
            retarget,
            mocap_tools,
            )


# MocapConstraint class
# Defines MocapConstraint datatype, used to add and configute mocap constraints
# Attached to Armature data

def hasIKConstraint(pose_bone):
    #utility function / predicate, returns True if given bone has IK constraint
    ik = [constraint for constraint in pose_bone.constraints if constraint.type == "IK"]
    if ik:
        return ik[0]
    else:
        return False


class MocapConstraint(bpy.types.PropertyGroup):
    name = StringProperty(name="Name",
        default="Mocap Fix",
        description="Name of Mocap Fix",
        update=mocap_constraints.setConstraint)
    constrained_bone = StringProperty(name="Bone",
        default="",
        description="Constrained Bone",
        update=mocap_constraints.updateConstraintBoneType)
    constrained_boneB = StringProperty(name="Bone (2)",
        default="",
        description="Other Constrained Bone (optional, depends on type)",
        update=mocap_constraints.setConstraint)
    s_frame = IntProperty(name="S",
        default=0,
        description="Start frame of Fix",
        update=mocap_constraints.setConstraint)
    e_frame = IntProperty(name="E",
        default=100,
        description="End frame of Fix",
        update=mocap_constraints.setConstraint)
    smooth_in = IntProperty(name="In",
        default=10,
        description="Number of frames to smooth in",
        update=mocap_constraints.setConstraint,
        min=0)
    smooth_out = IntProperty(name="Out",
        default=10,
        description="Number of frames to smooth out",
        update=mocap_constraints.setConstraint,
        min=0)
    targetMesh = StringProperty(name="Mesh",
        default="",
        description="Target of Fix - Mesh (optional, depends on type)",
        update=mocap_constraints.setConstraint)
    active = BoolProperty(name="Active",
        default=True,
        description="Fix is active",
        update=mocap_constraints.setConstraint)
    show_expanded = BoolProperty(name="Show Expanded",
        default=True,
        description="Fix is fully shown")
    targetPoint = FloatVectorProperty(name="Point", size=3,
        subtype="XYZ", default=(0.0, 0.0, 0.0),
        description="Target of Fix - Point",
        update=mocap_constraints.setConstraint)
    targetDist = FloatProperty(name="Offset",
        default=0.0,
        description="Distance and Floor Fixes - Desired offset",
        update=mocap_constraints.setConstraint)
    targetSpace = EnumProperty(
        items=[("WORLD", "World Space", "Evaluate target in global space"),
            ("LOCAL", "Object space", "Evaluate target in object space"),
            ("constrained_boneB", "Other Bone Space", "Evaluate target in specified other bone space")],
        name="Space",
        description="In which space should Point type target be evaluated",
        update=mocap_constraints.setConstraint)
    type = EnumProperty(name="Type of constraint",
        items=[("point", "Maintain Position", "Bone is at a specific point"),
            ("freeze", "Maintain Position at frame", "Bone does not move from location specified in target frame"),
            ("floor", "Stay above", "Bone does not cross specified mesh object eg floor"),
            ("distance", "Maintain distance", "Target bones maintained specified distance")],
        description="Type of Fix",
        update=mocap_constraints.updateConstraintBoneType)
    real_constraint = StringProperty()
    real_constraint_bone = StringProperty()


# Animation Stitch Settings, used for animation stitching of 2 retargeted animations.
class AnimationStitchSettings(bpy.types.PropertyGroup):
    first_action = StringProperty(name="Action 1",
            description="First action in stitch")
    second_action = StringProperty(name="Action 2",
            description="Second action in stitch")
    blend_frame = IntProperty(name="Stitch frame",
            description="Frame to locate stitch on")
    blend_amount = IntProperty(name="Blend amount",
            description="Size of blending transition, on both sides of the stitch",
            default=10)
    second_offset = IntProperty(name="Second offset",
            description="Frame offset for 2nd animation, where it should start",
            default=10)
    stick_bone = StringProperty(name="Stick Bone",
            description="Bone to freeze during transition",
            default="")


# MocapNLA Tracks. Stores which tracks/actions are associated with each retargeted animation.
class MocapNLATracks(bpy.types.PropertyGroup):
    name = StringProperty()
    base_track = StringProperty()
    auto_fix_track = StringProperty()
    manual_fix_track = StringProperty()
    stride_action = StringProperty()


#Update function for Advanced Retarget boolean variable.
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


def toggleIKBone(self, context):
    #Update function for IK functionality. Is called when IK prop checkboxes are toggled.
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
                #~ deformer_children = [child for child in parent_bone.children if child.bone.use_deform]
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


#MocapMap class for storing mapping on enduser performer,
# where a bone may be linked to more than one on the performer
class MocapMapping(bpy.types.PropertyGroup):
    name = StringProperty()

# Disabling for now [#28933] - campbell
'''
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
'''


def hasIKConstraint(pose_bone):
    #utility function / predicate, returns True if given bone has IK constraint
    ik = [constraint for constraint in pose_bone.constraints if constraint.type == "IK"]
    if ik:
        return ik[0]
    else:
        return False


class MocapPanel(bpy.types.Panel):
    # Motion capture retargeting panel
    bl_label = "Mocap tools"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        obj = context.object
        return obj.type == 'ARMATURE' and context.active_object is not None and context.mode in {'EDIT_ARMATURE',
                                                                                                'POSE',
                                                                                                'OBJECT'}

    def draw(self, context):
        layout = self.layout

        layout.label("Preprocessing:")

        row = layout.row(align=True)
        row.operator("mocap.denoise", text='Clean noise')
        row.operator("mocap.rotate_fix", text='Fix BVH Axis Orientation')
        row.operator("mocap.scale_fix", text='Auto scale Performer')

        row = layout.row(align=True)
        row.operator("mocap.looper", text='Loop animation')
        row.operator("mocap.limitdof", text='Constrain Rig')
        row.operator("mocap.removelimitdof", text='Unconstrain Rig')

        layout.label("Retargeting:")
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj]
        if enduser_obj is None or len(performer_obj) != 1:
            layout.label("Select performer rig and target rig (as active)")
        else:
            layout.operator("mocap.guessmapping", text="Guess Hierarchy Mapping")
            labelRow = layout.row(align=True)
            labelRow.label("Performer Rig")
            labelRow.label("End user Rig")
            performer_obj = performer_obj[0]
            if performer_obj.data and enduser_obj.data:
                if performer_obj.data.name in bpy.data.armatures and enduser_obj.data.name in bpy.data.armatures:
                    perf = performer_obj.data
                    enduser_arm = enduser_obj.data
                    perf_pose_bones = enduser_obj.pose.bones
                    MappingRow = layout.row(align=True)
                    footCol = MappingRow.column(align=True)
                    nameCol = MappingRow.column(align=True)
                    nameCol.scale_x = 2
                    mapCol = MappingRow.column(align=True)
                    mapCol.scale_x = 2
                    selectCol = MappingRow.column(align=True)
                    twistCol = MappingRow.column(align=True)
                    IKCol = MappingRow.column(align=True)
                    IKCol.scale_x = 0.3
                    IKLabel = MappingRow.column(align=True)
                    IKLabel.scale_x = 0.2
                    for bone in perf.bones:
                        footCol.prop(data=bone, property='foot', text='', icon='POSE_DATA')
                        nameCol.label(bone.name)
                        mapCol.prop_search(bone, "map", enduser_arm, "bones", text='')
                        selectCol.operator("mocap.selectmap", text='', icon='CURSOR').perf_bone = bone.name
                        label_mod = "FK"
                        if bone.map:
                            pose_bone = perf_pose_bones[bone.map]
                            if pose_bone.is_in_ik_chain:
                                label_mod = "ik chain"
                            if hasIKConstraint(pose_bone):
                                label_mod = "ik end"
                            end_bone = enduser_obj.data.bones[bone.map]
                            twistCol.prop(data=end_bone, property='twistFix', text='', icon='RNA')
                            IKCol.prop(pose_bone, 'IKRetarget')
                            IKLabel.label(label_mod)
                        else:
                            twistCol.label(" ")
                            IKCol.label(" ")
                            IKLabel.label(" ")
                    mapRow = layout.row()
                    mapRow.operator("mocap.savemapping", text='Save mapping')
                    mapRow.operator("mocap.loadmapping", text='Load mapping')
                    extraSettings = self.layout.box()
                    if performer_obj.animation_data and performer_obj.animation_data.action:
                        extraSettings.prop(data=performer_obj.animation_data.action, property='name', text='Action Name')
                    extraSettings.prop(enduser_arm, "frameStep")
                    extraSettings.prop(enduser_arm, "advancedRetarget", text='Advanced Retarget')
                    layout.operator("mocap.retarget", text='RETARGET!')


class MocapConstraintsPanel(bpy.types.Panel):
    #Motion capture constraints panel
    bl_label = "Mocap Fixes"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return obj.type == 'ARMATURE' and context.active_object is not None and context.mode in {'EDIT_ARMATURE',
                                                                                                'POSE',
                                                                                                'OBJECT'}

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
                            box.prop_search(m_constraint, 'constrained_bone', enduser_obj.pose, "bones", icon='BONE_DATA', text="")
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
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        obj = context.object
        return obj.type == 'ARMATURE' and context.active_object is not None and context.mode in {'EDIT_ARMATURE',
                                                                                                'POSE',
                                                                                                'OBJECT'}
    def draw(self, context):
        layout = self.layout
        layout.operator("mocap.samples", text='Samples to Beziers')
        layout.operator('mocap.pathediting', text="Follow Path")
        activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        if activeIsArmature:
            enduser_arm = context.active_object.data
            selectBox = layout.box()
            selectRetargets = selectBox.row()
            selectRetargets.label("Retargeted Animations:")
            selectRetargets.prop_search(enduser_arm, "active_mocap", enduser_arm, "mocapNLATracks")
            stitchBox = layout.box()
            stitchBox.label("Animation Stitching")
            settings = enduser_arm.stitch_settings
            stitchBox.prop_search(settings, "first_action", enduser_arm, "mocapNLATracks")
            stitchBox.prop_search(settings, "second_action", enduser_arm, "mocapNLATracks")
            stitchSettings = stitchBox.row()
            stitchSettings.prop(settings, "blend_frame")
            stitchSettings.prop(settings, "blend_amount")
            stitchSettings.prop(settings, "second_offset")
            stitchBox.prop_search(settings, "stick_bone", context.active_object.pose, "bones")
            stitchBox.operator('mocap.animstitchguess', text="Guess Settings")
            stitchBox.operator('mocap.animstitch', text="Stitch Animations")


class OBJECT_OT_RetargetButton(bpy.types.Operator):
    #Retargeting operator. Assumes selected and active armatures, where the performer (the selected one)
    # has an action for retargeting
    """Retarget animation from selected armature to active armature"""
    bl_idname = "mocap.retarget"
    bl_label = "Retarget"
    bl_options = {'REGISTER', 'UNDO'}

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
        if retarget.isRigAdvanced(enduser_obj) and not enduser_obj.data.advancedRetarget:
            print("Recommended to use Advanced Retargeting method")
            enduser_obj.data.advancedRetarget = True
        else:
            retarget.totalRetarget(performer_obj, enduser_obj, scene, s_frame, e_frame)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature) and performer_obj[0].animation_data
        else:
            return False


class OBJECT_OT_SaveMappingButton(bpy.types.Operator):
    #Operator for saving mapping to enduser armature
    """Save mapping to active armature (for future retargets)"""
    bl_idname = "mocap.savemapping"
    bl_label = "Save Mapping"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        retarget.createDictionary(performer_obj.data, enduser_obj.data)
        return {'FINISHED'}

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
    """Load saved mapping from active armature"""
    #Operator for loading mapping to enduser armature
    bl_idname = "mocap.loadmapping"
    bl_label = "Load Mapping"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        retarget.loadMapping(performer_obj.data, enduser_obj.data)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object]
        if performer_obj:
            return activeIsArmature and isinstance(performer_obj[0].data, bpy.types.Armature)
        else:
            return False


class OBJECT_OT_SelectMapBoneButton(bpy.types.Operator):
    #Operator for setting selected bone in enduser armature to the performer mapping
    """Select a bone for faster mapping"""
    bl_idname = "mocap.selectmap"
    bl_label = "Select Mapping Bone"
    perf_bone = StringProperty()

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        selectedBone = ""
        for bone in enduser_obj.data.bones:
            boneVis = bone.layers
            for i in range(32):
                if boneVis[i] and enduser_obj.data.layers[i]:
                    if bone.select:
                        selectedBone = bone.name
                        break
        performer_obj.data.bones[self.perf_bone].map = selectedBone
        return {'FINISHED'}

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
    #Operator to convert samples to beziers on the selected object
    """Convert active armature's sampled keyframed to beziers"""
    bl_idname = "mocap.samples"
    bl_label = "Convert Samples"

    def execute(self, context):
        mocap_tools.fcurves_simplify(context, context.active_object)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        return context.active_object.animation_data


class OBJECT_OT_LooperButton(bpy.types.Operator):
    #Operator to trim fcurves which contain a few loops to a single one on the selected object
    """Trim active armature's animation to a single cycle, given """ \
    """a cyclic animation (such as a walk cycle)"""
    bl_idname = "mocap.looper"
    bl_label = "Loop Mocap"

    def execute(self, context):
        mocap_tools.autoloop_anim()
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        return context.active_object.animation_data


class OBJECT_OT_DenoiseButton(bpy.types.Operator):
    #Operator to denoise impluse noise on the active object's fcurves
    """Removes spikes from all fcurves on the selected object"""
    bl_idname = "mocap.denoise"
    bl_label = "Denoise Mocap"

    def execute(self, context):
        obj = context.active_object
        mocap_tools.denoise(obj, obj.animation_data.action.fcurves)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.animation_data and obj.animation_data.action


class OBJECT_OT_LimitDOFButton(bpy.types.Operator):
    #Operator to analyze performer armature and apply rotation constraints on the enduser armature
    """Create limit constraints on the active armature from """ \
    """the selected armature's animation's range of motion"""
    bl_idname = "mocap.limitdof"
    bl_label = "Set DOF Constraints"

    def execute(self, context):
        performer_obj = [obj for obj in context.selected_objects if obj != context.active_object][0]
        mocap_tools.limit_dof(context, performer_obj, context.active_object)
        return {'FINISHED'}

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
    #Removes constraints created by above operator
    """Remove previously created limit constraints on the active armature"""
    bl_idname = "mocap.removelimitdof"
    bl_label = "Remove DOF Constraints"

    def execute(self, context):
        mocap_tools.limit_dof_toggle_off(context, context.active_object)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        activeIsArmature = False
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
        return activeIsArmature


class OBJECT_OT_RotateFixArmature(bpy.types.Operator):
    #Operator to fix common imported Mocap data issue of wrong axis system on active object
    """Realign the active armature's axis system to match Blender """ \
    """(commonly needed after bvh import)"""
    bl_idname = "mocap.rotate_fix"
    bl_label = "Rotate Fix"

    def execute(self, context):
        mocap_tools.rotate_fix_armature(context.active_object.data)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_ScaleFixArmature(bpy.types.Operator):
    #Operator to scale down the selected armature to match the active one
    """Rescale selected armature to match the active animation, """ \
    """for convenience"""
    bl_idname = "mocap.scale_fix"
    bl_label = "Scale Fix"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        mocap_tools.scale_fix_armature(performer_obj, enduser_obj)
        return {'FINISHED'}

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
    #Operator to add a post-retarget fix
    """Add a post-retarget fix - useful for fixing certain """ \
    """artifacts following the retarget"""
    bl_idname = "mocap.addmocapfix"
    bl_label = "Add Mocap Fix"
    type = EnumProperty(name="Type of Fix",
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
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_RemoveMocapConstraint(bpy.types.Operator):
    #Operator to remove a post-retarget fix
    """Remove this post-retarget fix"""
    bl_idname = "mocap.removeconstraint"
    bl_label = "Remove Mocap Fix"
    constraint = IntProperty()

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        enduser_arm = enduser_obj.data
        m_constraints = enduser_arm.mocap_constraints
        m_constraint = m_constraints[self.constraint]
        if m_constraint.real_constraint:
            bone = enduser_obj.pose.bones[m_constraint.real_constraint_bone]
            cons_obj = mocap_constraints.getConsObj(bone)
            mocap_constraints.removeConstraint(m_constraint, cons_obj)
        m_constraints.remove(self.constraint)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_BakeMocapConstraints(bpy.types.Operator):
    #Operator to bake all post-retarget fixes
    """Bake all post-retarget fixes to the Retarget Fixes NLA Track"""
    bl_idname = "mocap.bakeconstraints"
    bl_label = "Bake Mocap Fixes"

    def execute(self, context):
        mocap_constraints.bakeConstraints(context)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_UnbakeMocapConstraints(bpy.types.Operator):
    #Operator to unbake all post-retarget fixes
    """Unbake all post-retarget fixes - removes the baked data """ \
    """from the Retarget Fixes NLA Track"""
    bl_idname = "mocap.unbakeconstraints"
    bl_label = "Unbake Mocap Fixes"

    def execute(self, context):
        mocap_constraints.unbakeConstraints(context)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_UpdateMocapConstraints(bpy.types.Operator):
    #Operator to update all post-retarget fixes, similar to update dependencies on drivers
    #Needed because python properties lack certain callbacks and some fixes take a while to recalculate.
    """Update all post-retarget fixes (neccesary to take under """ \
    """consideration changes to armature object or pose)"""
    bl_idname = "mocap.updateconstraints"
    bl_label = "Update Mocap Fixes"

    def execute(self, context):
        mocap_constraints.updateConstraints(context.active_object, context)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            return isinstance(context.active_object.data, bpy.types.Armature)


class OBJECT_OT_GuessHierachyMapping(bpy.types.Operator):
    #Operator which calls heurisitic function to guess mapping between 2 armatures
    """Attempt to auto figure out hierarchy mapping"""
    bl_idname = "mocap.guessmapping"
    bl_label = "Guess Hierarchy Mapping"

    def execute(self, context):
        enduser_obj = bpy.context.active_object
        performer_obj = [obj for obj in bpy.context.selected_objects if obj != enduser_obj][0]
        mocap_tools.guessMapping(performer_obj, enduser_obj)
        return {'FINISHED'}

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
    #Operator which calls path editing function, making active object follow the selected curve.
    """Set active object (stride object) to follow the selected curve"""
    bl_idname = "mocap.pathediting"
    bl_label = "Set Path"

    def execute(self, context):
        path = [obj for obj in context.selected_objects if obj != context.active_object][0]
        mocap_tools.path_editing(context, context.active_object, path)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        if context.active_object:
            selected_objs = [obj for obj in context.selected_objects if obj != context.active_object and isinstance(obj.data, bpy.types.Curve)]
            return selected_objs
        else:
            return False


class OBJECT_OT_AnimationStitchingButton(bpy.types.Operator):
    #Operator which calls stitching function, combining 2 animations onto the NLA.
    """Stitch two defined animations into a single one via alignment of NLA Tracks"""
    bl_idname = "mocap.animstitch"
    bl_label = "Stitch Animations"

    def execute(self, context):
        mocap_tools.anim_stitch(context, context.active_object)
        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        activeIsArmature = False
        if context.active_object:
            activeIsArmature = isinstance(context.active_object.data, bpy.types.Armature)
            if activeIsArmature:
                stitch_settings = context.active_object.data.stitch_settings
                return (stitch_settings.first_action and stitch_settings.second_action)
        return False


class OBJECT_OT_GuessAnimationStitchingButton(bpy.types.Operator):
    #Operator which calls stitching function heuristic, setting good values for above operator.
    """Guess the stitch frame and second offset for animation stitch"""
    bl_idname = "mocap.animstitchguess"
    bl_label = "Guess Animation Stitch"

    def execute(self, context):
        mocap_tools.guess_anim_stitch(context, context.active_object)
        return {'FINISHED'}

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
    bpy.utils.register_class(MocapConstraint)
    bpy.types.Armature.mocap_constraints = CollectionProperty(type=MocapConstraint)
    bpy.utils.register_class(MocapMapping)
    #string property for storing performer->enduser mapping
    bpy.types.Bone.map = StringProperty()
    #Collection Property for storing enduser->performer mapping
    bpy.types.Bone.reverseMap = CollectionProperty(type=MocapMapping)
    #Boolean property for storing foot bone toggle
    bpy.types.Bone.foot = BoolProperty(name="Foot",
        description="Mark this bone as a 'foot', which determines retargeted animation's translation",
        default=False)
    #Boolean property for storing if this bone is twisted along the y axis,
    # which can happen due to various sources of performers
    bpy.types.Bone.twistFix = BoolProperty(name="Twist Fix",
        description="Fix Twist on this bone",
        default=False)
    #Boolean property for toggling ik retargeting for this bone
    bpy.types.PoseBone.IKRetarget = BoolProperty(name="IK",
        description="Toggle IK Retargeting method for given bone",
        update=toggleIKBone, default=False)
    bpy.utils.register_class(AnimationStitchSettings)
    bpy.utils.register_class(MocapNLATracks)
    #Animation Stitch Settings Property
    bpy.types.Armature.stitch_settings = PointerProperty(type=AnimationStitchSettings)
    #Current/Active retargeted animation on the armature
    bpy.types.Armature.active_mocap = StringProperty(update=retarget.NLASystemInitialize)
    #Collection of retargeted animations and their NLA Tracks on the armature
    bpy.types.Armature.mocapNLATracks = CollectionProperty(type=MocapNLATracks)
    #Advanced retargeting boolean property
    bpy.types.Armature.advancedRetarget = BoolProperty(default=False, update=advancedRetargetToggle)
    #frame step - frequency of frames to retarget. Skipping is useful for previewing, faster work etc.
    bpy.types.Armature.frameStep = IntProperty(name="Frame Skip",
            default=1,
            description="Number of frames to skip - for previewing retargets quickly (1 is fully sampled)",
            min=1)
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
