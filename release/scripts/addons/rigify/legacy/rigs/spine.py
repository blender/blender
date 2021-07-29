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

""" TODO:
    - Add parameters for bone transform alphas.
    - Add IK spine controls
"""

from math import floor

import bpy
from mathutils import Vector
from rna_prop_ui import rna_idprop_ui_prop_get

from ..utils import MetarigError
from ..utils import copy_bone, new_bone, flip_bone, put_bone
from ..utils import connected_children_names
from ..utils import strip_org, make_mechanism_name, make_deformer_name
from ..utils import create_circle_widget, create_cube_widget

script = """
main = "%s"
spine = [%s]
if is_selected([main]+ spine):
    layout.prop(pose_bones[main], '["pivot_slide"]', text="Pivot Slide (" + main + ")", slider=True)

for name in spine[1:-1]:
    if is_selected(name):
        layout.prop(pose_bones[name], '["auto_rotate"]', text="Auto Rotate (" + name + ")", slider=True)
"""


class Rig:
    """ A "spine" rig.  It turns a chain of bones into a rig with two controls:
        One for the hips, and one for the rib cage.

    """
    def __init__(self, obj, bone_name, params):
        """ Gather and validate data about the rig.

        """
        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params

        # Collect control bone indices
        self.control_indices = [0, len(self.org_bones) - 1]
        temp = self.params.chain_bone_controls.split(",")
        for i in temp:
            try:
                j = int(i) - 1
            except ValueError:
                pass
            else:
                if (j > 0) and (j < len(self.org_bones)) and (j not in self.control_indices):
                    self.control_indices += [j]
        self.control_indices.sort()

        self.pivot_rest = self.params.rest_pivot_slide
        # Clamp pivot_rest to within the middle bones of the spine
        self.pivot_rest = max(self.pivot_rest, 1.0 / len(self.org_bones))
        self.pivot_rest = min(self.pivot_rest, 1.0 - (1.0 / len(self.org_bones)))

        if len(self.org_bones) <= 1:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of 2 or more bones" % (strip_org(bone_name)))

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
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones
        #-------------------------
        # Get rest slide position
        a = self.pivot_rest * len(self.org_bones)
        i = floor(a)
        a -= i
        if i == len(self.org_bones):
            i -= 1
            a = 1.0

        pivot_rest_pos = eb[self.org_bones[i]].head.copy()
        pivot_rest_pos += eb[self.org_bones[i]].vector * a

        #----------------------
        # Create controls

        # Create control bones
        controls = []
        for i in self.control_indices:
            name = copy_bone(self.obj, self.org_bones[i], strip_org(self.org_bones[i]))
            controls += [name]

        # Create control parents
        control_parents = []
        for i in self.control_indices[1:-1]:
            name = new_bone(self.obj, make_mechanism_name("par_" + strip_org(self.org_bones[i])))
            control_parents += [name]

        # Create sub-control bones
        subcontrols = []
        for i in self.control_indices:
            name = new_bone(self.obj, make_mechanism_name("sub_" + strip_org(self.org_bones[i])))
            subcontrols += [name]

        # Create main control bone
        main_control = new_bone(self.obj, self.params.spine_main_control_name)

        eb = self.obj.data.edit_bones

        # Parent the main control
        eb[main_control].use_connect = False
        eb[main_control].parent = eb[self.org_bones[0]].parent

        # Parent the controls and sub-controls
        for name, subname in zip(controls, subcontrols):
            eb[name].use_connect = False
            eb[name].parent = eb[main_control]
            eb[subname].use_connect = False
            eb[subname].parent = eb[name]

        # Parent the control parents
        for name, par_name in zip(controls[1:-1], control_parents):
            eb[par_name].use_connect = False
            eb[par_name].parent = eb[main_control]
            eb[name].parent = eb[par_name]

        # Position the main bone
        put_bone(self.obj, main_control, pivot_rest_pos)
        eb[main_control].length = sum([eb[b].length for b in self.org_bones]) / 2

        # Position the controls and sub-controls
        for name, subname in zip(controls, subcontrols):
            put_bone(self.obj, name, pivot_rest_pos)
            put_bone(self.obj, subname, pivot_rest_pos)
            eb[subname].length = eb[name].length / 3

        # Position the control parents
        for name, par_name in zip(controls[1:-1], control_parents):
            put_bone(self.obj, par_name, pivot_rest_pos)
            eb[par_name].length = eb[name].length / 2

        #-----------------------------------------
        # Control bone constraints and properties
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Lock control locations
        for name in controls:
            bone = pb[name]
            bone.lock_location = True, True, True

        # Main control doesn't use local location
        pb[main_control].bone.use_local_location = False

        # Intermediate controls follow hips and spine
        for name, par_name, i in zip(controls[1:-1], control_parents, self.control_indices[1:-1]):
            bone = pb[par_name]

            # Custom bend_alpha property
            prop = rna_idprop_ui_prop_get(pb[name], "bend_alpha", create=True)
            pb[name]["bend_alpha"] = i / (len(self.org_bones) - 1)  # set bend alpha
            prop["min"] = 0.0
            prop["max"] = 1.0
            prop["soft_min"] = 0.0
            prop["soft_max"] = 1.0

            # Custom auto_rotate
            prop = rna_idprop_ui_prop_get(pb[name], "auto_rotate", create=True)
            pb[name]["auto_rotate"] = 1.0
            prop["min"] = 0.0
            prop["max"] = 1.0
            prop["soft_min"] = 0.0
            prop["soft_max"] = 1.0

            # Constraints
            con1 = bone.constraints.new('COPY_TRANSFORMS')
            con1.name = "copy_transforms"
            con1.target = self.obj
            con1.subtarget = subcontrols[0]

            con2 = bone.constraints.new('COPY_TRANSFORMS')
            con2.name = "copy_transforms"
            con2.target = self.obj
            con2.subtarget = subcontrols[-1]

            # Drivers
            fcurve = con1.driver_add("influence")
            driver = fcurve.driver
            driver.type = 'AVERAGE'
            var = driver.variables.new()
            var.name = "auto"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = pb[name].path_from_id() + '["auto_rotate"]'

            fcurve = con2.driver_add("influence")
            driver = fcurve.driver
            driver.type = 'SCRIPTED'
            driver.expression = "alpha * auto"
            var = driver.variables.new()
            var.name = "alpha"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = pb[name].path_from_id() + '["bend_alpha"]'
            var = driver.variables.new()
            var.name = "auto"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = pb[name].path_from_id() + '["auto_rotate"]'

        #-------------------------
        # Create flex spine chain
        bpy.ops.object.mode_set(mode='EDIT')
        flex_bones = []
        flex_subs = []
        prev_bone = None
        for b in self.org_bones:
            # Create bones
            bone = copy_bone(self.obj, b, make_mechanism_name(strip_org(b) + ".flex"))
            sub = new_bone(self.obj, make_mechanism_name(strip_org(b) + ".flex_s"))
            flex_bones += [bone]
            flex_subs += [sub]

            eb = self.obj.data.edit_bones
            bone_e = eb[bone]
            sub_e = eb[sub]

            # Parenting
            bone_e.use_connect = False
            sub_e.use_connect = False
            if prev_bone is None:
                sub_e.parent = eb[controls[0]]
            else:
                sub_e.parent = eb[prev_bone]
            bone_e.parent = sub_e

            # Position
            put_bone(self.obj, sub, bone_e.head)
            sub_e.length = bone_e.length / 4
            if prev_bone is not None:
                sub_e.use_connect = True

            prev_bone = bone

        #----------------------------
        # Create reverse spine chain

        # Create bones/parenting/positioning
        bpy.ops.object.mode_set(mode='EDIT')
        rev_bones = []
        prev_bone = None
        for b in zip(flex_bones, self.org_bones):
            # Create bones
            bone = copy_bone(self.obj, b[1], make_mechanism_name(strip_org(b[1]) + ".reverse"))
            rev_bones += [bone]
            eb = self.obj.data.edit_bones
            bone_e = eb[bone]

            # Parenting
            bone_e.use_connect = False
            bone_e.parent = eb[b[0]]

            # Position
            flip_bone(self.obj, bone)
            bone_e.tail = Vector(eb[b[0]].head)
            #bone_e.head = Vector(eb[b[0]].tail)
            if prev_bone is None:
                put_bone(self.obj, bone, pivot_rest_pos)
            else:
                put_bone(self.obj, bone, eb[prev_bone].tail)

            prev_bone = bone

        # Constraints
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones
        prev_bone = None
        for bone in rev_bones:
            bone_p = pb[bone]

            con = bone_p.constraints.new('COPY_LOCATION')
            con.name = "copy_location"
            con.target = self.obj
            if prev_bone is None:
                con.subtarget = main_control
            else:
                con.subtarget = prev_bone
                con.head_tail = 1.0
            prev_bone = bone

        #----------------------------------------
        # Constrain original bones to flex spine
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        for obone, fbone in zip(self.org_bones, flex_bones):
            con = pb[obone].constraints.new('COPY_TRANSFORMS')
            con.name = "copy_transforms"
            con.target = self.obj
            con.subtarget = fbone

        #---------------------------
        # Create pivot slide system
        pb = self.obj.pose.bones
        bone_p = pb[self.org_bones[0]]
        main_control_p = pb[main_control]

        # Custom pivot_slide property
        prop = rna_idprop_ui_prop_get(main_control_p, "pivot_slide", create=True)
        main_control_p["pivot_slide"] = self.pivot_rest
        prop["min"] = 0.0
        prop["max"] = 1.0
        prop["soft_min"] = 1.0 / len(self.org_bones)
        prop["soft_max"] = 1.0 - (1.0 / len(self.org_bones))

        # Anchor constraints
        con = bone_p.constraints.new('COPY_LOCATION')
        con.name = "copy_location"
        con.target = self.obj
        con.subtarget = rev_bones[0]

        # Slide constraints
        i = 1
        tot = len(rev_bones)
        for rb in rev_bones:
            con = bone_p.constraints.new('COPY_LOCATION')
            con.name = "slide." + str(i)
            con.target = self.obj
            con.subtarget = rb
            con.head_tail = 1.0

            # Driver
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'AVERAGE'
            var.name = "slide"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = main_control_p.path_from_id() + '["pivot_slide"]'
            mod = fcurve.modifiers[0]
            mod.poly_order = 1
            mod.coefficients[0] = 1 - i
            mod.coefficients[1] = tot

            i += 1

        #----------------------------------
        # Constrain flex spine to controls
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Constrain the bones that correspond exactly to the controls
        for i, name in zip(self.control_indices, subcontrols):
            con = pb[flex_subs[i]].constraints.new('COPY_TRANSFORMS')
            con.name = "copy_transforms"
            con.target = self.obj
            con.subtarget = name

        # Constrain the bones in-between the controls
        for i, j, name1, name2 in zip(self.control_indices, self.control_indices[1:], subcontrols, subcontrols[1:]):
            if (i + 1) < j:
                for n in range(i + 1, j):
                    bone = pb[flex_subs[n]]
                    # Custom bend_alpha property
                    prop = rna_idprop_ui_prop_get(bone, "bend_alpha", create=True)
                    bone["bend_alpha"] = (n - i) / (j - i)  # set bend alpha
                    prop["min"] = 0.0
                    prop["max"] = 1.0
                    prop["soft_min"] = 0.0
                    prop["soft_max"] = 1.0

                    con = bone.constraints.new('COPY_TRANSFORMS')
                    con.name = "copy_transforms"
                    con.target = self.obj
                    con.subtarget = name1

                    con = bone.constraints.new('COPY_TRANSFORMS')
                    con.name = "copy_transforms"
                    con.target = self.obj
                    con.subtarget = name2

                    # Driver
                    fcurve = con.driver_add("influence")
                    driver = fcurve.driver
                    var = driver.variables.new()
                    driver.type = 'AVERAGE'
                    var.name = "alpha"
                    var.targets[0].id_type = 'OBJECT'
                    var.targets[0].id = self.obj
                    var.targets[0].data_path = bone.path_from_id() + '["bend_alpha"]'

        #-------------
        # Final stuff
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Control appearance
        # Main
        create_cube_widget(self.obj, main_control)

        # Spines
        for name, i in zip(controls[1:-1], self.control_indices[1:-1]):
            pb[name].custom_shape_transform = pb[self.org_bones[i]]
            # Create control widgets
            create_circle_widget(self.obj, name, radius=1.0, head_tail=0.5, with_line=True, bone_transform_name=self.org_bones[i])

        # Hips
        pb[controls[0]].custom_shape_transform = pb[self.org_bones[0]]
        # Create control widgets
        create_circle_widget(self.obj, controls[0], radius=1.0, head_tail=0.5, with_line=True, bone_transform_name=self.org_bones[0])

        # Ribs
        pb[controls[-1]].custom_shape_transform = pb[self.org_bones[-1]]
        # Create control widgets
        create_circle_widget(self.obj, controls[-1], radius=1.0, head_tail=0.5, with_line=True, bone_transform_name=self.org_bones[-1])

        # Layers
        pb[main_control].bone.layers = pb[self.org_bones[0]].bone.layers

        return [main_control] + controls

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.

        """
        self.gen_deform()
        controls = self.gen_control()

        controls_string = ", ".join(["'" + x + "'" for x in controls[1:]])
        return [script % (controls[0], controls_string)]


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """
    params.spine_main_control_name = bpy.props.StringProperty(name="Main control name", default="torso", description="Name that the main control bone should be given")
    params.rest_pivot_slide = bpy.props.FloatProperty(name="Rest Pivot Slide", default=0.0, min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, description="The pivot slide value in the rest pose")
    params.chain_bone_controls = bpy.props.StringProperty(name="Control bone list", default="", description="Define which bones have controls")


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters.
    """
    r = layout.row()
    r.prop(params, "spine_main_control_name")

    r = layout.row()
    r.prop(params, "rest_pivot_slide", slider=True)

    r = layout.row()
    r.prop(params, "chain_bone_controls")


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('hips')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = -0.0000, -0.0590, 0.2804
    bone.roll = -0.0000
    bone.use_connect = False
    bones['hips'] = bone.name
    bone = arm.edit_bones.new('spine')
    bone.head[:] = -0.0000, -0.0590, 0.2804
    bone.tail[:] = 0.0000, 0.0291, 0.5324
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['hips']]
    bones['spine'] = bone.name
    bone = arm.edit_bones.new('ribs')
    bone.head[:] = 0.0000, 0.0291, 0.5324
    bone.tail[:] = -0.0000, 0.0000, 1.0000
    bone.roll = -0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine']]
    bones['ribs'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['hips']]
    pbone.rigify_type = 'spine'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ribs']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['hips']]
    pbone['rigify_type'] = 'spine'
    pbone.rigify_parameters.chain_bone_controls = "1, 2, 3"

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
