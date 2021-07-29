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
from rna_prop_ui import rna_idprop_ui_prop_get

from ..utils import MetarigError
from ..utils import copy_bone, new_bone, put_bone
from ..utils import connected_children_names
from ..utils import strip_org, make_mechanism_name, make_deformer_name
from ..utils import create_circle_widget


script1 = """
head_neck = ["%s", "%s"]
"""

script2 = """
if is_selected(head_neck[0]):
    layout.prop(pose_bones[head_neck[0]], '["isolate"]', text="Isolate (" + head_neck[0] + ")", slider=True)
"""

script3 = """
if is_selected(head_neck):
    layout.prop(pose_bones[head_neck[0]], '["neck_follow"]', text="Neck Follow Head (" + head_neck[0] + ")", slider=True)
"""


class Rig:
    """ A "neck" rig.  It turns a chain of bones into a rig with two controls:
        One for the head, and one for the neck.

    """
    def __init__(self, obj, bone_name, params):
        """ Gather and validate data about the rig.

        """
        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params

        if len(self.org_bones) <= 1:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of 2 or more bones" % (strip_org(bone_name)))

        self.isolate = False
        if self.obj.data.bones[bone_name].parent:
            self.isolate = True

    def gen_deform(self):
        """ Generate the deformation rig.

        """
        for name in self.org_bones:
            bpy.ops.object.mode_set(mode='EDIT')
            eb = self.obj.data.edit_bones

            # Create deform bone
            bone_e = eb[copy_bone(self.obj, name)]

            # Change its name
            bone_e.name = make_deformer_name(strip_org(name))
            bone_name = bone_e.name

            # Leave edit mode
            bpy.ops.object.mode_set(mode='OBJECT')

            # Get the pose bone
            bone = self.obj.pose.bones[bone_name]

            # Constrain to the original bone
            con = bone.constraints.new('COPY_TRANSFORMS')
            con.name = "copy_transforms"
            con.target = self.obj
            con.subtarget = name

    def gen_control(self):
        """ Generate the control rig.

        """
        #---------------------------------
        # Create the neck and head controls
        bpy.ops.object.mode_set(mode='EDIT')

        # Create bones
        neck_ctrl = copy_bone(self.obj, self.org_bones[0], strip_org(self.org_bones[0]))
        neck_follow = copy_bone(self.obj, self.org_bones[-1], make_mechanism_name(strip_org(self.org_bones[0] + ".follow")))
        neck_child = new_bone(self.obj, make_mechanism_name(strip_org(self.org_bones[0] + ".child")))

        head_ctrl = copy_bone(self.obj, self.org_bones[-1], strip_org(self.org_bones[-1]))
        head_mch = new_bone(self.obj, make_mechanism_name(strip_org(self.org_bones[-1])))
        if self.isolate:
            head_socket1 = copy_bone(self.obj, self.org_bones[-1], make_mechanism_name(strip_org(self.org_bones[-1] + ".socket1")))
            head_socket2 = copy_bone(self.obj, self.org_bones[-1], make_mechanism_name(strip_org(self.org_bones[-1] + ".socket2")))

        # Create neck chain bones
        neck = []
        helpers = []
        for name in self.org_bones:
            neck += [copy_bone(self.obj, name, make_mechanism_name(strip_org(name)))]
            helpers += [copy_bone(self.obj, neck_child, make_mechanism_name(strip_org(name + ".02")))]

        # Fetch edit bones
        eb = self.obj.data.edit_bones

        neck_ctrl_e = eb[neck_ctrl]
        neck_follow_e = eb[neck_follow]
        neck_child_e = eb[neck_child]
        head_ctrl_e = eb[head_ctrl]
        head_mch_e = eb[head_mch]
        if self.isolate:
            head_socket1_e = eb[head_socket1]
            head_socket2_e = eb[head_socket2]

        # Parenting
        head_ctrl_e.use_connect = False
        head_ctrl_e.parent = neck_ctrl_e.parent
        head_mch_e.use_connect = False
        head_mch_e.parent = head_ctrl_e

        if self.isolate:
            head_socket1_e.use_connect = False
            head_socket1_e.parent = neck_ctrl_e.parent

            head_socket2_e.use_connect = False
            head_socket2_e.parent = None

            head_ctrl_e.parent = head_socket2_e

        for (name1, name2) in zip(neck, helpers):
            eb[name1].use_connect = False
            eb[name1].parent = eb[name2]
            eb[name2].use_connect = False
            eb[name2].parent = neck_ctrl_e.parent

        neck_follow_e.use_connect = False
        neck_follow_e.parent = neck_ctrl_e.parent
        neck_child_e.use_connect = False
        neck_child_e.parent = neck_ctrl_e
        neck_ctrl_e.parent = neck_follow_e

        # Position
        put_bone(self.obj, neck_follow, neck_ctrl_e.head)
        put_bone(self.obj, neck_child, neck_ctrl_e.head)
        put_bone(self.obj, head_ctrl, neck_ctrl_e.head)
        put_bone(self.obj, head_mch, neck_ctrl_e.head)
        head_mch_e.length = head_ctrl_e.length / 2
        neck_child_e.length = neck_ctrl_e.length / 2

        if self.isolate:
            put_bone(self.obj, head_socket1, neck_ctrl_e.head)
            head_mch_e.length /= 2

            put_bone(self.obj, head_socket2, neck_ctrl_e.head)
            head_mch_e.length /= 3

        for (name1, name2) in zip(neck, helpers):
            put_bone(self.obj, name2, eb[name1].head)
            eb[name2].length = eb[name1].length / 2

        # Switch to object mode
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones
        neck_ctrl_p = pb[neck_ctrl]
        neck_follow_p = pb[neck_follow]
        # neck_child_p = pb[neck_child]  # UNUSED
        head_ctrl_p = pb[head_ctrl]
        if self.isolate:
            # head_socket1_p = pb[head_socket1]  # UNUSED
            head_socket2_p = pb[head_socket2]

        # Custom bone appearance
        neck_ctrl_p.custom_shape_transform = pb[self.org_bones[(len(self.org_bones) - 1) // 2]]
        head_ctrl_p.custom_shape_transform = pb[self.org_bones[-1]]

        # Custom properties
        prop = rna_idprop_ui_prop_get(head_ctrl_p, "inf_extent", create=True)
        head_ctrl_p["inf_extent"] = 0.5
        prop["min"] = 0.0
        prop["max"] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

        prop = rna_idprop_ui_prop_get(head_ctrl_p, "neck_follow", create=True)
        head_ctrl_p["neck_follow"] = 1.0
        prop["min"] = 0.0
        prop["max"] = 2.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

        if self.isolate:
            prop = rna_idprop_ui_prop_get(head_ctrl_p, "isolate", create=True)
            head_ctrl_p["isolate"] = 0.0
            prop["min"] = 0.0
            prop["max"] = 1.0
            prop["soft_min"] = 0.0
            prop["soft_max"] = 1.0

        # Constraints

        # Neck follow
        con = neck_follow_p.constraints.new('COPY_ROTATION')
        con.name = "copy_rotation"
        con.target = self.obj
        con.subtarget = head_ctrl

        fcurve = con.driver_add("influence")
        driver = fcurve.driver
        var = driver.variables.new()
        driver.type = 'SCRIPTED'
        var.name = "follow"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = self.obj
        var.targets[0].data_path = head_ctrl_p.path_from_id() + '["neck_follow"]'
        driver.expression = "follow / 2"

        # Isolate
        if self.isolate:
            con = head_socket2_p.constraints.new('COPY_LOCATION')
            con.name = "copy_location"
            con.target = self.obj
            con.subtarget = head_socket1

            con = head_socket2_p.constraints.new('COPY_TRANSFORMS')
            con.name = "copy_transforms"
            con.target = self.obj
            con.subtarget = head_socket1

            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'SCRIPTED'
            var.name = "isolate"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = head_ctrl_p.path_from_id() + '["isolate"]'
            driver.expression = "1.0 - isolate"

        # Neck chain
        first = True
        prev = None
        i = 0
        l = len(neck)
        for (name1, name2, org_name) in zip(neck, helpers, self.org_bones):
            con = pb[org_name].constraints.new('COPY_TRANSFORMS')
            con.name = "copy_transforms"
            con.target = self.obj
            con.subtarget = name1

            n_con = pb[name2].constraints.new('COPY_TRANSFORMS')
            n_con.name = "neck"
            n_con.target = self.obj
            n_con.subtarget = neck_child

            h_con = pb[name2].constraints.new('COPY_TRANSFORMS')
            h_con.name = "head"
            h_con.target = self.obj
            h_con.subtarget = head_mch

            con = pb[name2].constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            if first:
                con.subtarget = neck_ctrl
            else:
                con.subtarget = prev
                con.head_tail = 1.0

            # Drivers
            n = (i + 1) / l

            # Neck influence
            fcurve = n_con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'SCRIPTED'
            var.name = "ext"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = head_ctrl_p.path_from_id() + '["inf_extent"]'
            driver.expression = "1.0 if (%.4f > (1.0-ext) or (1.0-ext) == 0.0) else (%.4f / (1.0-ext))" % (n, n)

            # Head influence
            if (i + 1) == l:
                h_con.influence = 1.0
            else:
                fcurve = h_con.driver_add("influence")
                driver = fcurve.driver
                var = driver.variables.new()
                driver.type = 'SCRIPTED'
                var.name = "ext"
                var.targets[0].id_type = 'OBJECT'
                var.targets[0].id = self.obj
                var.targets[0].data_path = head_ctrl_p.path_from_id() + '["inf_extent"]'
                driver.expression = "0.0 if (%.4f <= (1.0-ext)) else ((%.4f - (1.0-ext)) / ext)" % (n, n)

            first = False
            prev = name1
            i += 1

        # Create control widgets
        create_circle_widget(self.obj, neck_ctrl, radius=1.0, head_tail=0.5, bone_transform_name=self.org_bones[(len(self.org_bones) - 1) // 2])
        create_circle_widget(self.obj, head_ctrl, radius=1.0, head_tail=0.5, bone_transform_name=self.org_bones[-1])

        # Return control bones
        return (head_ctrl, neck_ctrl)

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.

        """
        self.gen_deform()
        (head, neck) = self.gen_control()

        script = script1 % (head, neck)
        if self.isolate:
            script += script2
        script += script3

        return [script]


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('neck')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, -0.0500, 0.1500
    bone.roll = 0.0000
    bone.use_connect = False
    bones['neck'] = bone.name
    bone = arm.edit_bones.new('head')
    bone.head[:] = 0.0000, -0.0500, 0.1500
    bone.tail[:] = 0.0000, -0.0500, 0.4000
    bone.roll = 3.1416
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['neck']]
    bones['head'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['neck']]
    pbone.rigify_type = 'neck_short'
    pbone.lock_location = (True, True, True)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['head']]
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
