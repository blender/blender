#====================== BEGIN GPL LICENSE BLOCK ======================
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
#======================= END GPL LICENSE BLOCK ========================

# <pep8 compliant>

import bpy

from ...utils import MetarigError
from ...utils import copy_bone
from ...utils import connected_children_names
from ...utils import strip_org, make_deformer_name
from ...utils import create_bone_widget


class Rig:
    """ A "copy_chain" rig.  All it does is duplicate the original bone chain
        and constrain it.
        This is a control and deformation rig.

    """
    def __init__(self, obj, bone_name, params):
        """ Gather and validate data about the rig.
        """
        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params
        self.make_controls = params.make_controls
        self.make_deforms = params.make_deforms

        if len(self.org_bones) <= 1:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of 2 or more bones" % (strip_org(bone_name)))

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.

        """
        bpy.ops.object.mode_set(mode='EDIT')

        # Create the deformation and control bone chains.
        # Just copies of the original chain.
        def_chain = []
        ctrl_chain = []
        for i in range(len(self.org_bones)):
            name = self.org_bones[i]

            # Control bone
            if self.make_controls:
                # Copy
                ctrl_bone = copy_bone(self.obj, name)
                eb = self.obj.data.edit_bones
                ctrl_bone_e = eb[ctrl_bone]
                # Name
                ctrl_bone_e.name = strip_org(name)
                # Parenting
                if i == 0:
                    # First bone
                    ctrl_bone_e.parent = eb[self.org_bones[0]].parent
                else:
                    # The rest
                    ctrl_bone_e.parent = eb[ctrl_chain[-1]]
                # Add to list
                ctrl_chain += [ctrl_bone_e.name]
            else:
                ctrl_chain += [None]

            # Deformation bone
            if self.make_deforms:
                # Copy
                def_bone = copy_bone(self.obj, name)
                eb = self.obj.data.edit_bones
                def_bone_e = eb[def_bone]
                # Name
                def_bone_e.name = make_deformer_name(strip_org(name))
                # Parenting
                if i == 0:
                    # First bone
                    def_bone_e.parent = eb[self.org_bones[0]].parent
                else:
                    # The rest
                    def_bone_e.parent = eb[def_chain[-1]]
                # Add to list
                def_chain += [def_bone_e.name]
            else:
                def_chain += [None]

        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Constraints for org and def
        for org, ctrl, defrm in zip(self.org_bones, ctrl_chain, def_chain):
            if self.make_controls:
                con = pb[org].constraints.new('COPY_TRANSFORMS')
                con.name = "copy_transforms"
                con.target = self.obj
                con.subtarget = ctrl

            if self.make_deforms:
                con = pb[defrm].constraints.new('COPY_TRANSFORMS')
                con.name = "copy_transforms"
                con.target = self.obj
                con.subtarget = org

        # Create control widgets
        if self.make_controls:
            for bone in ctrl_chain:
                create_bone_widget(self.obj, bone)


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """
    params.make_controls = bpy.props.BoolProperty(name="Controls", default=True, description="Create control bones for the copy")
    params.make_deforms = bpy.props.BoolProperty(name="Deform", default=True, description="Create deform bones for the copy")


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters.
    """
    r = layout.row()
    r.prop(params, "make_controls")
    r = layout.row()
    r.prop(params, "make_deforms")


def create_sample(obj):
    """ Create a sample metarig for this rig type.
    """
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('bone.01')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.0000, 0.3333
    bone.roll = 0.0000
    bone.use_connect = False
    bones['bone.01'] = bone.name
    bone = arm.edit_bones.new('bone.02')
    bone.head[:] = 0.0000, 0.0000, 0.3333
    bone.tail[:] = 0.0000, 0.0000, 0.6667
    bone.roll = 3.1416
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['bone.01']]
    bones['bone.02'] = bone.name
    bone = arm.edit_bones.new('bone.03')
    bone.head[:] = 0.0000, 0.0000, 0.6667
    bone.tail[:] = 0.0000, 0.0000, 1.0000
    bone.roll = 3.1416
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['bone.02']]
    bones['bone.03'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['bone.01']]
    pbone.rigify_type = 'basic.copy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['bone.02']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['bone.03']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone.select = False
        bone.select_head = False
        bone.select_tail = False
    for b in bones:
        bone = arm.edit_bones[bones[b]]
        bone.select = True
        bone.select_head = True
        bone.select_tail = True
        arm.edit_bones.active = bone
