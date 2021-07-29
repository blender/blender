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

import re

import bpy
from rna_prop_ui import rna_idprop_ui_prop_get
from mathutils import Vector

from ..utils import MetarigError
from ..utils import copy_bone
from ..utils import connected_children_names
from ..utils import strip_org, make_mechanism_name, make_deformer_name
from ..utils import create_widget, create_limb_widget


class Rig:
    """ A finger rig.  It takes a single chain of bones.
        This is a control and deformation rig.
    """
    def __init__(self, obj, bone, params):
        """ Gather and validate data about the rig.
        """
        self.obj = obj
        self.org_bones = [bone] + connected_children_names(obj, bone)
        self.params = params

        if len(self.org_bones) <= 1:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of 2 or more bones" % (strip_org(bone)))

        # Get user-specified layers, if they exist
        if params.separate_extra_layers:
            self.ex_layers = list(params.extra_layers)
        else:
            self.ex_layers = None

        # Get other rig parameters
        self.primary_rotation_axis = params.primary_rotation_axis
        self.use_digit_twist = params.use_digit_twist

    def deform(self):
        """ Generate the deformation rig.
            Just a copy of the original bones, except the first digit which is a twist bone.
        """
        bpy.ops.object.mode_set(mode='EDIT')

        # Create the bones
        # First bone is a twist bone
        if self.use_digit_twist:
            b1a = copy_bone(self.obj, self.org_bones[0], make_deformer_name(strip_org(self.org_bones[0] + ".01")))
            b1b = copy_bone(self.obj, self.org_bones[0], make_deformer_name(strip_org(self.org_bones[0] + ".02")))
            b1tip = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(self.org_bones[0] + ".tip")))
        else:
            b1 = copy_bone(self.obj, self.org_bones[0], make_deformer_name(strip_org(self.org_bones[0])))

        # The rest are normal
        bones = []
        for bone in self.org_bones[1:]:
            bones += [copy_bone(self.obj, bone, make_deformer_name(strip_org(bone)))]

        # Position bones
        eb = self.obj.data.edit_bones
        if self.use_digit_twist:
            b1a_e = eb[b1a]
            b1b_e = eb[b1b]
            b1tip_e = eb[b1tip]

            b1tip_e.use_connect = False
            b1tip_e.tail += Vector((0.1, 0, 0))
            b1tip_e.head = b1b_e.tail
            b1tip_e.length = b1a_e.length / 4

            center = (b1a_e.head + b1a_e.tail) / 2
            b1a_e.tail = center
            b1b_e.use_connect = False
            b1b_e.head = center

        # Parenting
        if self.use_digit_twist:
            b1b_e.parent = eb[self.org_bones[0]]
            b1tip_e.parent = eb[self.org_bones[0]]
        else:
            eb[b1].use_connect = False
            eb[b1].parent = eb[self.org_bones[0]]

        for (ba, bb) in zip(bones, self.org_bones[1:]):
            eb[ba].use_connect = False
            eb[ba].parent = eb[bb]

        # Constraints
        if self.use_digit_twist:
            bpy.ops.object.mode_set(mode='OBJECT')
            pb = self.obj.pose.bones

            b1a_p = pb[b1a]

            con = b1a_p.constraints.new('COPY_LOCATION')
            con.name = "copy_location"
            con.target = self.obj
            con.subtarget = self.org_bones[0]

            con = b1a_p.constraints.new('COPY_SCALE')
            con.name = "copy_scale"
            con.target = self.obj
            con.subtarget = self.org_bones[0]

            con = b1a_p.constraints.new('DAMPED_TRACK')
            con.name = "track_to"
            con.target = self.obj
            con.subtarget = b1tip

    def control(self):
        """ Generate the control rig.
        """
        bpy.ops.object.mode_set(mode='EDIT')

        # Figure out the name for the control bone (remove the last .##)
        ctrl_name = re.sub("([0-9]+\.)", "", strip_org(self.org_bones[0])[::-1], count=1)[::-1]

        # Create the bones
        ctrl = copy_bone(self.obj, self.org_bones[0], ctrl_name)

        helpers = []
        bones = []
        for bone in self.org_bones:
            bones += [copy_bone(self.obj, bone, strip_org(bone))]
            helpers += [copy_bone(self.obj, bone, make_mechanism_name(strip_org(bone)))]

        # Position bones
        eb = self.obj.data.edit_bones

        length = 0.0
        for bone in helpers:
            length += eb[bone].length
            eb[bone].length /= 2

        eb[ctrl].length = length * 1.5

        # Parent bones
        prev = eb[self.org_bones[0]].parent
        for (b, h) in zip(bones, helpers):
            b_e = eb[b]
            h_e = eb[h]
            b_e.use_connect = False
            h_e.use_connect = False

            b_e.parent = h_e
            h_e.parent = prev

            prev = b_e

        # Transform locks and rotation mode
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        for bone in bones[1:]:
            pb[bone].lock_location = True, True, True

        if pb[self.org_bones[0]].bone.use_connect is True:
            pb[bones[0]].lock_location = True, True, True

        pb[ctrl].lock_scale = True, False, True

        for bone in helpers:
            pb[bone].rotation_mode = 'XYZ'

        # Drivers
        i = 1
        val = 1.2 / (len(self.org_bones) - 1)
        for bone in helpers:
            # Add custom prop
            prop_name = "bend_%02d" % i
            prop = rna_idprop_ui_prop_get(pb[ctrl], prop_name, create=True)
            prop["min"] = 0.0
            prop["max"] = 1.0
            prop["soft_min"] = 0.0
            prop["soft_max"] = 1.0
            if i == 1:
                pb[ctrl][prop_name] = 0.0
            else:
                pb[ctrl][prop_name] = val

            # Add driver
            if 'X' in self.primary_rotation_axis:
                fcurve = pb[bone].driver_add("rotation_euler", 0)
            elif 'Y' in self.primary_rotation_axis:
                fcurve = pb[bone].driver_add("rotation_euler", 1)
            else:
                fcurve = pb[bone].driver_add("rotation_euler", 2)

            driver = fcurve.driver
            driver.type = 'SCRIPTED'

            var = driver.variables.new()
            var.name = "ctrl_y"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = pb[ctrl].path_from_id() + '.scale[1]'

            var = driver.variables.new()
            var.name = "bend"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = pb[ctrl].path_from_id() + '["' + prop_name + '"]'

            if '-' in self.primary_rotation_axis:
                driver.expression = "-(1.0-ctrl_y) * bend * 3.14159 * 2"
            else:
                driver.expression = "(1.0-ctrl_y) * bend * 3.14159 * 2"

            i += 1

        # Constraints
        con = pb[helpers[0]].constraints.new('COPY_LOCATION')
        con.name = "copy_location"
        con.target = self.obj
        con.subtarget = ctrl

        con = pb[helpers[0]].constraints.new('COPY_ROTATION')
        con.name = "copy_rotation"
        con.target = self.obj
        con.subtarget = ctrl

        # Constrain org bones to the control bones
        for (bone, org) in zip(bones, self.org_bones):
            con = pb[org].constraints.new('COPY_TRANSFORMS')
            con.name = "copy_transforms"
            con.target = self.obj
            con.subtarget = bone

        # Set layers for extra control bones
        if self.ex_layers:
            for bone in bones:
                pb[bone].bone.layers = self.ex_layers

        # Create control widgets
        w = create_widget(self.obj, ctrl)
        if w is not None:
            mesh = w.data
            verts = [(0, 0, 0), (0, 1, 0), (0.05, 1, 0), (0.05, 1.1, 0), (-0.05, 1.1, 0), (-0.05, 1, 0)]
            if 'Z' in self.primary_rotation_axis:
                # Flip x/z coordinates
                temp = []
                for v in verts:
                    temp += [(v[2], v[1], v[0])]
                verts = temp
            edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 1)]
            mesh.from_pydata(verts, edges, [])
            mesh.update()

        for bone in bones:
            create_limb_widget(self.obj, bone)

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.
        """
        self.deform()
        self.control()


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """
    items = [('X', 'X', ''), ('Y', 'Y', ''), ('Z', 'Z', ''), ('-X', '-X', ''), ('-Y', '-Y', ''), ('-Z', '-Z', '')]
    params.primary_rotation_axis = bpy.props.EnumProperty(items=items, name="Primary Rotation Axis", default='X')

    params.separate_extra_layers = bpy.props.BoolProperty(name="Separate Secondary Control Layers:", default=False, description="Enable putting the secondary controls on a separate layer from the primary controls")
    params.extra_layers = bpy.props.BoolVectorProperty(size=32, description="Layers for the secondary controls to be on")

    params.use_digit_twist = bpy.props.BoolProperty(name="Digit Twist", default=True, description="Generate the dual-bone twist setup for the first finger digit")


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters.
    """
    r = layout.row()
    r.prop(params, "separate_extra_layers")

    r = layout.row()
    r.active = params.separate_extra_layers

    col = r.column(align=True)
    row = col.row(align=True)
    row.prop(params, "extra_layers", index=0, toggle=True, text="")
    row.prop(params, "extra_layers", index=1, toggle=True, text="")
    row.prop(params, "extra_layers", index=2, toggle=True, text="")
    row.prop(params, "extra_layers", index=3, toggle=True, text="")
    row.prop(params, "extra_layers", index=4, toggle=True, text="")
    row.prop(params, "extra_layers", index=5, toggle=True, text="")
    row.prop(params, "extra_layers", index=6, toggle=True, text="")
    row.prop(params, "extra_layers", index=7, toggle=True, text="")
    row = col.row(align=True)
    row.prop(params, "extra_layers", index=16, toggle=True, text="")
    row.prop(params, "extra_layers", index=17, toggle=True, text="")
    row.prop(params, "extra_layers", index=18, toggle=True, text="")
    row.prop(params, "extra_layers", index=19, toggle=True, text="")
    row.prop(params, "extra_layers", index=20, toggle=True, text="")
    row.prop(params, "extra_layers", index=21, toggle=True, text="")
    row.prop(params, "extra_layers", index=22, toggle=True, text="")
    row.prop(params, "extra_layers", index=23, toggle=True, text="")

    col = r.column(align=True)
    row = col.row(align=True)
    row.prop(params, "extra_layers", index=8, toggle=True, text="")
    row.prop(params, "extra_layers", index=9, toggle=True, text="")
    row.prop(params, "extra_layers", index=10, toggle=True, text="")
    row.prop(params, "extra_layers", index=11, toggle=True, text="")
    row.prop(params, "extra_layers", index=12, toggle=True, text="")
    row.prop(params, "extra_layers", index=13, toggle=True, text="")
    row.prop(params, "extra_layers", index=14, toggle=True, text="")
    row.prop(params, "extra_layers", index=15, toggle=True, text="")
    row = col.row(align=True)
    row.prop(params, "extra_layers", index=24, toggle=True, text="")
    row.prop(params, "extra_layers", index=25, toggle=True, text="")
    row.prop(params, "extra_layers", index=26, toggle=True, text="")
    row.prop(params, "extra_layers", index=27, toggle=True, text="")
    row.prop(params, "extra_layers", index=28, toggle=True, text="")
    row.prop(params, "extra_layers", index=29, toggle=True, text="")
    row.prop(params, "extra_layers", index=30, toggle=True, text="")
    row.prop(params, "extra_layers", index=31, toggle=True, text="")

    r = layout.row()
    r.label(text="Bend rotation axis:")
    r.prop(params, "primary_rotation_axis", text="")

    col = layout.column()
    col.prop(params, "use_digit_twist")


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('finger.01')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.2529, 0.0000, 0.0000
    bone.roll = 3.1416
    bone.use_connect = False
    bones['finger.01'] = bone.name
    bone = arm.edit_bones.new('finger.02')
    bone.head[:] = 0.2529, 0.0000, 0.0000
    bone.tail[:] = 0.4024, 0.0000, -0.0264
    bone.roll = -2.9671
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['finger.01']]
    bones['finger.02'] = bone.name
    bone = arm.edit_bones.new('finger.03')
    bone.head[:] = 0.4024, 0.0000, -0.0264
    bone.tail[:] = 0.4975, -0.0000, -0.0610
    bone.roll = -2.7925
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['finger.02']]
    bones['finger.03'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['finger.01']]
    pbone.rigify_type = 'finger'
    pbone.lock_location = (True, True, True)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'YZX'
    pbone = obj.pose.bones[bones['finger.02']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'YZX'
    pbone = obj.pose.bones[bones['finger.03']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'YZX'

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
