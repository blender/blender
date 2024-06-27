# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from mathutils import Vector
from ...utils import copy_bone, put_bone, org, align_bone_y_axis, align_bone_x_axis, align_bone_z_axis
from ...utils import strip_org, make_deformer_name, connected_children_names
from ...utils import create_chain_widget
from ...utils import make_mechanism_name
from ...utils import ControlLayersOption
from ...utils.mechanism import make_property, make_driver
from ..limbs.limb_utils import get_bone_name


class Rig:

    def __init__(self, obj, bone_name, params):
        """ Chain with pivot Rig """

        eb = obj.data.bones

        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params
        self.spine_length = sum([eb[b].length for b in self.org_bones])
        self.bbones = params.bbones
        self.SINGLE_BONE = (len(self.org_bones) == 1)

    def orient_bone(self, eb, axis, scale, reverse=False):
        v = Vector((0, 0, 0))

        setattr(v, axis, scale)

        if reverse:
            tail_vec = v @ self.obj.matrix_world
            eb.head[:] = eb.tail
            eb.tail[:] = eb.head + tail_vec
        else:
            tail_vec = v @ self.obj.matrix_world
            eb.tail[:] = eb.head + tail_vec

    def create_pivot(self, pivot=None):
        """ Create the pivot control and mechanism bones """

        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        if not pivot:
            pivot = int(len(org_bones)/2)

        pivot_name = org_bones[pivot]
        if '.L' in pivot_name:
            prefix = strip_org(org_bones[pivot]).split('.')[0] + '.L'
        elif '.R' in pivot_name:
            prefix = strip_org(org_bones[pivot]).split('.')[0] + '.R'
        else:
            prefix = strip_org(org_bones[pivot]).split('.')[0]

        ctrl_name = get_bone_name(prefix, 'ctrl', 'pivot')
        ctrl_name = copy_bone(self.obj, pivot_name, ctrl_name)
        ctrl_eb = eb[ctrl_name]

        self.orient_bone(ctrl_eb, 'y', self.spine_length / 2.5)

        pivot_loc = eb[pivot_name].head + ((eb[pivot_name].tail - eb[pivot_name].head)/2)*(len(org_bones) % 2)

        put_bone(self.obj, ctrl_name, pivot_loc)

        v = eb[org_bones[-1]].tail - eb[org_bones[0]].head  # Create a vector from head of first ORG to tail of last
        v.normalize()
        v_proj = eb[org_bones[0]].y_axis.dot(v)*v   # projection of first ORG to v
        v_point = eb[org_bones[0]].y_axis - v_proj  # a vector co-planar to first ORG and v directed out of the chain

        if v_point.magnitude < eb[org_bones[0]].y_axis.magnitude*1e-03:     # if v_point is too small it's not usable
            v_point = eb[org_bones[0]].x_axis

        if self.params.tweak_axis == 'auto':
            align_bone_y_axis(self.obj, ctrl_name, v)
            align_bone_z_axis(self.obj, ctrl_name, -v_point)
        elif self.params.tweak_axis == 'x':
            align_bone_y_axis(self.obj, ctrl_name, Vector((1, 0, 0)))
            align_bone_x_axis(self.obj, ctrl_name, Vector((0, 0, 1)))
        elif self.params.tweak_axis == 'y':
            align_bone_y_axis(self.obj, ctrl_name, Vector((0, 1, 0)))
            align_bone_x_axis(self.obj, ctrl_name, Vector((1, 0, 0)))
        elif self.params.tweak_axis == 'z':
            align_bone_y_axis(self.obj, ctrl_name, Vector((0, 0, 1)))
            align_bone_x_axis(self.obj, ctrl_name, Vector((1, 0, 0)))

        return {
            'ctrl': ctrl_name
        }

    def create_deform(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')

        def_bones = []
        for o in org_bones:
            def_name = make_deformer_name(strip_org(o))
            def_name = copy_bone(self.obj, o, def_name)
            def_bones.append(def_name)

        bpy.ops.object.mode_set(mode='POSE')
        # Create bbone segments
        for bone in def_bones:
            self.obj.data.bones[bone].bbone_segments = self.bbones

        if not self.SINGLE_BONE:
            self.obj.data.bones[def_bones[0]].bbone_easein = 0.0
            self.obj.data.bones[def_bones[-1]].bbone_easeout = 0.0
        else:
            self.obj.data.bones[def_bones[0]].bbone_easein = 1.0
            self.obj.data.bones[def_bones[-1]].bbone_easeout = 1.0
        bpy.ops.object.mode_set(mode='EDIT')

        conv_def = ""
        if self.params.conv_bone and self.params.conv_def:
            b = org(self.params.conv_bone)
            conv_def = make_deformer_name(strip_org(b))
            conv_def = copy_bone(self.obj, b, conv_def)

        return def_bones, conv_def

    def create_chain(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        twk, mch, mch_ctrl, ctrl = [], [], [], []

        suffix = ''
        if '.L' in org_bones[0]:
            suffix = '.L'
        elif '.R' in org_bones[0]:
            suffix = '.R'

        mch_auto = ''
        if not self.SINGLE_BONE:
            mch_name = copy_bone(self.obj, org(org_bones[0]),
                                 'MCH-AUTO-' + strip_org(org_bones[0]).split('.')[0] + suffix)
            eb[mch_name].head = eb[org_bones[0]].head
            eb[mch_name].tail = eb[org_bones[-1]].tail

            mch_auto = mch_name

        # Intermediary bones
        for b in org_bones:     # All

            if self.SINGLE_BONE:
                mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
                eb[mch_name].length /= 4
                put_bone(self.obj, mch_name, eb[b].head - (eb[mch_name].tail - eb[mch_name].head))
                align_bone_z_axis(self.obj, mch_name, eb[b].z_axis)
                mch += [mch_name]
                mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
                eb[mch_name].length /= 4
                put_bone(self.obj, mch_name, eb[b].tail)
                mch += [mch_name]
                break
            else:
                mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
                eb[mch_name].length /= 4

                mch += [mch_name]

                if b == org_bones[-1]:  # Add extra
                    mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
                    eb[mch_name].length /= 4
                    put_bone(self.obj, mch_name, eb[b].tail)
                    mch += [mch_name]

        # Tweak & Ctrl bones
        v = eb[org_bones[-1]].tail - eb[org_bones[0]].head  # Create a vector from head of first ORG to tail of last
        v.normalize()
        v_proj = eb[org_bones[0]].y_axis.dot(v)*v   # projection of first ORG to v
        v_point = eb[org_bones[0]].y_axis - v_proj  # a vector co-planar to first ORG and v directed out of the chain

        if v_point.magnitude < eb[org_bones[0]].y_axis.magnitude*1e-03:
            v_point = eb[org_bones[0]].x_axis

        for b in org_bones:     # All

            suffix = ''
            if '.L' in b:
                suffix = '.L'
            elif '.R' in b:
                suffix = '.R'

            if b == org_bones[0]:
                name = get_bone_name(b.split('.')[0] + suffix, 'ctrl', 'ctrl')
                name = copy_bone(self.obj, org(b), name)
                align_bone_x_axis(self.obj, name, eb[org(b)].x_axis)
                ctrl += [name]
            else:
                name = 'tweak_' + strip_org(b)
                name = copy_bone(self.obj, org(b), name)
                twk += [name]

            self.orient_bone(eb[name], 'y', eb[name].length / 2)

            if self.params.tweak_axis == 'auto':
                align_bone_y_axis(self.obj, name, v)
                align_bone_z_axis(self.obj, name, -v_point)  # invert?
            elif self.params.tweak_axis == 'x':
                align_bone_y_axis(self.obj, name, Vector((1, 0, 0)))
                align_bone_x_axis(self.obj, name, Vector((0, 0, 1)))
            elif self.params.tweak_axis == 'y':
                align_bone_y_axis(self.obj, name, Vector((0, 1, 0)))
                align_bone_x_axis(self.obj, name, Vector((1, 0, 0)))
            elif self.params.tweak_axis == 'z':
                align_bone_y_axis(self.obj, name, Vector((0, 0, 1)))
                align_bone_x_axis(self.obj, name, Vector((1, 0, 0)))

            if b == org_bones[-1]:      # Add extra
                ctrl_name = get_bone_name(b.split('.')[0] + suffix, 'ctrl', 'ctrl')
                ctrl_name = copy_bone(self.obj, org(b), ctrl_name)

                self.orient_bone(eb[ctrl_name], 'y', eb[ctrl_name].length / 2)

                # TODO check this if else
                if self.params.conv_bone:
                    align_bone_y_axis(self.obj, ctrl_name, eb[org(self.params.conv_bone)].y_axis)
                    align_bone_x_axis(self.obj, ctrl_name, eb[org(self.params.conv_bone)].x_axis)
                    align_bone_z_axis(self.obj, ctrl_name, eb[org(self.params.conv_bone)].z_axis)

                else:
                    if twk:
                        lastname = twk[-1]
                    else:
                        lastname = ctrl[-1]
                    align_bone_y_axis(self.obj, ctrl_name, eb[lastname].y_axis)
                    align_bone_x_axis(self.obj, ctrl_name, eb[lastname].x_axis)
                put_bone(self.obj, ctrl_name, eb[b].tail)

                ctrl += [ctrl_name]

        conv_twk = ''
        # Convergence tweak
        if self.params.conv_bone:
            conv_twk = 'tweak_' + strip_org(self.params.conv_bone)

            if not(conv_twk in eb.keys()):
                conv_twk = copy_bone(self.obj, org(self.params.conv_bone), conv_twk)

        for b in org_bones:

            if self.SINGLE_BONE:
                break

            # Mch controls
            suffix = ''
            if '.L' in b:
                suffix = '.L'
            elif '.R' in b:
                suffix = '.R'

            mch_ctrl_name = "MCH-CTRL-" + strip_org(b).split('.')[0] + suffix
            mch_ctrl_name = copy_bone(self.obj, twk[0] if twk else ctrl[0], mch_ctrl_name)

            eb[mch_ctrl_name].length /= 6

            put_bone(self.obj, mch_ctrl_name, eb[b].head)

            mch_ctrl += [mch_ctrl_name]

            if b == org_bones[-1]:  # Add extra
                mch_ctrl_name = "MCH-CTRL-" + strip_org(b).split('.')[0] + suffix
                mch_ctrl_name = copy_bone(self.obj, twk[0] if twk else ctrl[0], mch_ctrl_name)

                eb[mch_ctrl_name].length /= 6

                put_bone(self.obj, mch_ctrl_name, eb[b].tail)

                mch_ctrl += [mch_ctrl_name]

        return {
            'mch': mch,
            'mch_ctrl': mch_ctrl,
            'mch_auto': mch_auto,
            'tweak': twk,
            'ctrl': ctrl,
            'conv': conv_twk
        }

    def parent_bones(self, bones):

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Parent deform bones
        for i, b in enumerate(bones['def']):
            if i > 0:   # For all bones but the first (which has no parent)
                eb[b].parent = eb[bones['def'][i-1]]  # to previous
                eb[b].use_connect = True
            elif self.SINGLE_BONE:
                eb[b].parent = eb[bones['chain']['mch'][0]]
                eb[b].use_connect = True

        # Todo check case when sup_chain is in bigger rig
        eb[bones['def'][0]].parent = eb[bones['chain']['mch'][0]]

        for i, twk in enumerate(bones['chain']['tweak']):
            eb[twk].parent = eb[bones['chain']['mch_ctrl'][i+1]]
            eb[twk].inherit_scale = 'NONE'

        eb[bones['chain']['ctrl'][0]].parent =\
            eb[bones['chain']['mch_ctrl'][0]] if bones['chain']['mch_ctrl'] else None
        eb[bones['chain']['ctrl'][0]].inherit_scale = 'NONE'
        eb[bones['chain']['ctrl'][1]].parent =\
            eb[bones['chain']['mch_ctrl'][-1]] if bones['chain']['mch_ctrl'] else None
        eb[bones['chain']['ctrl'][1]].inherit_scale = 'NONE'

        if 'pivot' in bones.keys():
            eb[bones['pivot']['ctrl']].inherit_scale = 'NONE'

        for i, mch in enumerate(bones['chain']['mch']):
            if mch == bones['chain']['mch'][0]:
                eb[mch].parent = eb[bones['chain']['ctrl'][0]]
            elif mch == bones['chain']['mch'][-1]:
                eb[mch].parent = eb[bones['chain']['ctrl'][1]]
            else:
                eb[mch].parent = eb[bones['chain']['tweak'][i-1]]

        if 'parent' in bones.keys():
            if self.SINGLE_BONE:
                eb[bones['chain']['ctrl'][0]].parent = eb[bones['parent']]
                eb[bones['chain']['ctrl'][-1]].parent = eb[bones['parent']]
            else:
                eb[bones['chain']['mch_auto']].parent = eb[bones['parent']]
                eb[bones['chain']['mch_ctrl'][0]].parent = eb[bones['parent']]
                eb[bones['chain']['mch_ctrl'][-1]].parent = eb[bones['parent']]

        for i, mch_ctrl in enumerate(bones['chain']['mch_ctrl'][1:-1]):
            eb[mch_ctrl].parent = eb[bones['chain']['mch_auto']]

        if 'pivot' in bones.keys():
            eb[bones['pivot']['ctrl']].parent = eb[bones['chain']['mch_auto']]

        if bones['chain']['conv']:
            eb[bones['chain']['ctrl'][-1]].parent = eb[bones['chain']['conv']]
            eb[bones['chain']['conv']].parent = eb[bones['chain']['conv']].parent

        if self.SINGLE_BONE:
            eb[bones['chain']['mch'][0]].parent = eb[bones['chain']['ctrl'][0]]
            eb[bones['chain']['mch'][1]].parent = eb[bones['chain']['ctrl'][1]]

        return

    def make_constraint(self, bone, constraint):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        owner_pb = pb[bone]
        const = owner_pb.constraints.new(constraint['constraint'])
        const.target = self.obj

        # filter constraint props to those that actually exist in the current type of constraint,
        # then assign values to each
        for p in [k for k in constraint.keys() if k in dir(const)]:
            setattr(const, p, constraint[p])

    def constrain_bones(self, bones):

        deform = bones['def']
        mch = bones['chain']['mch']
        mch_ctrl = bones['chain']['mch_ctrl']
        ctrls = bones['chain']['ctrl']
        tweaks = [ctrls[0]] + bones['chain']['tweak'] + [ctrls[-1]]

        # ORG bones
        for i, org_bone in enumerate(self.org_bones):
            self.make_constraint(org_bone, {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': tweaks[i],
                'owner_space': 'WORLD',
                'target_space': 'WORLD'
            })

        # DEF bones

        for i, d in enumerate(deform):

            if len(deform) > 1:
                self.make_constraint(d, {
                    'constraint': 'COPY_TRANSFORMS',
                    'subtarget': mch[i],
                    'owner_space': 'POSE',
                    'target_space': 'POSE'
                })

            self.make_constraint(d, {
                'constraint': 'STRETCH_TO',
                'subtarget': tweaks[i+1]
            })

        if bones['conv_def']:
            self.make_constraint(bones['conv_def'], {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': bones['chain']['conv'],
                'owner_space': 'POSE',
                'target_space': 'POSE'
            })

        if 'pivot' in bones.keys():
            step = 2/(len(self.org_bones))
            for i, b in enumerate(mch_ctrl):
                x_val = i*step
                influence = 2*x_val - x_val**2    # parabolic influence of pivot
                if (i != 0) and (i != len(mch_ctrl)-1):
                    self.make_constraint(b, {
                        'constraint': 'COPY_TRANSFORMS',
                        'subtarget': bones['pivot']['ctrl'],
                        'influence': influence,
                        'owner_space': 'LOCAL',
                        'target_space': 'LOCAL'
                    })

        # MCH-AUTO

        mch_auto = bones['chain']['mch_auto']

        if mch_auto:
            self.make_constraint(mch_auto, {
                'constraint': 'COPY_LOCATION',
                'subtarget': mch[0],
                'owner_space': 'WORLD',
                'target_space': 'WORLD'
            })

            self.make_constraint(mch_auto, {
                'constraint': 'STRETCH_TO',
                'subtarget': tweaks[-1]
            })

        # PIVOT CTRL

        if 'pivot' in bones.keys():

            pivot = bones['pivot']['ctrl']

            self.make_constraint(pivot, {
                'constraint': 'COPY_ROTATION',
                'subtarget': tweaks[0],
                'influence': 0.33,
                'owner_space': 'LOCAL',
                'target_space': 'LOCAL'
            })

            self.make_constraint(pivot, {
                'constraint': 'COPY_ROTATION',
                'subtarget': tweaks[-1],
                'influence':   0.33,
                'owner_space': 'LOCAL',
                'target_space': 'LOCAL'
            })

        return

    def stick_to_bendy_bones(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        deform = bones['def']
        pb = self.obj.pose.bones

        if len(deform) > 1:  # Only for single bone sup chain
            return

        def_pb = pb[deform[0]]
        ctrl_start = pb[bones['chain']['ctrl'][0]]
        ctrl_end = pb[bones['chain']['ctrl'][-1]]
        mch_start = pb[bones['chain']['mch'][0]]
        mch_end = pb[bones['chain']['mch_ctrl'][-1]] if bones['chain']['mch_ctrl'] else pb[bones['chain']['mch'][-1]]

        if not self.SINGLE_BONE:
            def_pb.bone.bbone_custom_handle_start = ctrl_start.bone
            def_pb.bone.bbone_custom_handle_end = ctrl_end.bone
        else:
            def_pb.bone.bbone_custom_handle_start = mch_start.bone
            def_pb.bone.bbone_custom_handle_end = mch_end.bone

        def_pb.bone.bbone_handle_type_start = 'ABSOLUTE'
        def_pb.bone.bbone_handle_type_end = 'ABSOLUTE'

    def create_drivers(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Setting the torso's props
        torso = pb[bones['pivot']['ctrl']]

        props = ["head_follow", "neck_follow"]
        owners = [bones['neck']['mch_head'], bones['neck']['mch_neck']]

        for prop in props:
            if prop == 'neck_follow':
                def_val = 0.5
            else:
                def_val = 0.0

            make_property(torso, prop, def_val)

        # driving the follow rotation switches for neck and head
        for bone, prop, in zip(owners, props):
            # Add driver to copy rotation constraint
            make_driver(
                pb[bone].constraints[0], "influence",
                variables=[(self.obj, torso, prop)], polynomial=[1.0, -1.0]
            )

    def locks_and_widgets(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Locks
        mch_ctrl = bones['chain']['mch_ctrl']

        for b in mch_ctrl:
            pb[b].lock_rotation = False, False, False
            pb[b].lock_location = False, False, False
            pb[b].lock_scale = False, False, False

        for b in bones['chain']['tweak']:
            pb[b].lock_rotation = True, False, True

        for b in bones['chain']['ctrl']:
            pb[b].lock_rotation = True, False, True

        if 'pivot' in bones.keys():
            pb[bones['pivot']['ctrl']].lock_rotation = True, False, True

        # Assigning a widget to main ctrl bone
        if 'pivot' in bones.keys():
            create_chain_widget(
                self.obj,
                bones['pivot']['ctrl'],
                cube=True,
                radius=0.15,
                bone_transform_name=None,
                axis=self.params.wgt_align_axis,
                offset=self.params.wgt_offset*pb[bones['chain']['ctrl'][0]].length
            )

        for bone in bones['chain']['tweak']:
            create_chain_widget(
                self.obj,
                bone,
                cube=True,
                radius=0.2,
                bone_transform_name=None,
                axis=self.params.wgt_align_axis,
                offset=self.params.wgt_offset*pb[bones['chain']['ctrl'][0]].length
            )

        create_chain_widget(
            self.obj,
            bones['chain']['ctrl'][0],
            invert=False,
            radius=0.3,
            bone_transform_name=None,
            axis=self.params.wgt_align_axis,
            offset=self.params.wgt_offset*pb[bones['chain']['ctrl'][0]].length
        )

        invert_last = True
        if self.params.wgt_align_axis not in {'y', '-y'}:
            invert_last = False

        create_chain_widget(
            self.obj,
            bones['chain']['ctrl'][-1],
            invert=invert_last,
            radius=0.3,
            bone_transform_name=None,
            axis=self.params.wgt_align_axis,
            offset=self.params.wgt_offset*pb[bones['chain']['ctrl'][0]].length
        )

        if bones['chain']['conv']:
            create_chain_widget(
                self.obj,
                bones['chain']['conv'],
                cube=True,
                radius=0.5,
                bone_transform_name=None,
                axis=self.params.wgt_align_axis,
                offset=self.params.wgt_offset*pb[bones['chain']['ctrl'][0]].length
            )

        # Assigning layers to tweaks and ctrls
        ControlLayersOption.TWEAK.assign(self.params, pb, bones['chain']['tweak'])

        return

    def aggregate_ctrls(self, bones):

        if not self.params.cluster_ctrls:
            return

        other_ctrls = []
        all_ctrls = []
        eb = self.obj.data.edit_bones

        head_start = eb[bones['chain']['ctrl'][0]].head
        head_tip = eb[bones['chain']['ctrl'][-1]].head

        all_ctrls.extend(bones['chain']['ctrl'])
        all_ctrls.extend(bones['chain']['tweak'])
        all_ctrls.extend(bones['chain']['conv'])
        for bone_name in eb.keys():
            if bone_name not in all_ctrls and (bone_name.startswith('tweak') or 'ctrl' in bone_name):
                other_ctrls.append(bone_name)

        for bone_name in other_ctrls:
            if eb[bone_name].head == head_start:
                for child in eb[bones['chain']['ctrl'][0]].children:
                    child.parent = eb[bone_name]
                eb.remove(eb[bones['chain']['ctrl'][0]])
                bones['chain']['ctrl'][0] = bone_name
                break

        for bone_name in other_ctrls:
            if eb[bone_name].head == head_tip:
                eb.remove(eb[bones['chain']['ctrl'][-1]])
                bones['chain']['ctrl'][-1] = bone_name
                break

    def generate(self):

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        bones = {}
        if eb[self.org_bones[0]].parent:
            def_name = make_deformer_name(strip_org(eb[self.org_bones[0]].parent.name))
            if self.params.def_parenting and def_name in eb.keys():
                bones['parent'] = def_name
            else:
                bones['parent'] = eb[self.org_bones[0]].parent.name

        # Clear parents for org bones
        for bone in self.org_bones[0:]:
            eb[bone].use_connect = False
            eb[bone].parent = None

        bones['def'], bones['conv_def'] = self.create_deform()
        if len(self.org_bones) > 2:
            bones['pivot'] = self.create_pivot()
        bones['chain'] = self.create_chain()

        self.parent_bones(bones)

        # ctrls snapping pass
        # self.aggregate_ctrls(bones)

        self.constrain_bones(bones)
        self.stick_to_bendy_bones(bones)
        self.locks_and_widgets(bones)

        return


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """

    items = [
        ('auto', 'Auto', ''),
        ('x', 'X-Global', ''),
        ('y', 'Y-Global', ''),
        ('z', 'Z-Global', '')
    ]

    params.tweak_axis = bpy.props.EnumProperty(
        items=items,
        name="Orient y-axis to",
        description="Targets all ctrls y-axis to defined axis (global space)",
        default='auto'
    )

    axes = [
        ('x', 'X', ''),
        ('y', 'Y', ''),
        ('z', 'Z', ''),
        ('-x', '-X', ''),
        ('-y', '-Y', ''),
        ('-z', '-Z', '')
    ]

    params.wgt_align_axis = bpy.props.EnumProperty(
        items=axes,
        name="Custom Widget orient",
        description="Targets custom WGTs to defined axis (global space)",
        default='y'
    )

    params.conv_bone = bpy.props.StringProperty(
        name='Convergence bone',
        default=''
    )

    params.conv_def = bpy.props.BoolProperty(
        name="Add DEF on convergence",
        default=False,
        description=""
        )

    params.def_parenting = bpy.props.BoolProperty(
        name="Prefer DEF parenting",
        default=False,
        description=""
        )

    params.cluster_ctrls = bpy.props.BoolProperty(
        name="Cluster controls",
        default=False,
        description="Clusterize controls in the same position"
        )

    params.bbones = bpy.props.IntProperty(
        name='B-Bone Segments',
        default=10,
        min=1,
        description='Number of B-Bone segments'
    )

    params.wgt_offset = bpy.props.FloatProperty(
        name='Widget Offset',
        default=0.0,
        min=-10.0,
        max=10.0
    )

    ControlLayersOption.TWEAK.add_parameters(params)


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    pb = bpy.context.object.pose

    r = layout.row()
    col = r.column(align=True)
    row = col.row(align=True)
    row.prop(params, "tweak_axis")

    r = layout.row()
    r.prop(params, "wgt_align_axis")

    r = layout.row()
    r.prop(params, "wgt_offset")

    r = layout.row()
    r.prop(params, "bbones")

    r = layout.row()
    r.prop_search(params, 'conv_bone', pb, "bones", text="Convergence Bone")

    r = layout.row()
    r.prop(params, 'conv_def')

    r = layout.row()
    r.prop(params, 'def_parenting')

    # r = layout.row()
    # r.prop(params, 'cluster_ctrls')

    ControlLayersOption.TWEAK.parameters_ui(layout, params)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('spine')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.5000/8, 1.0000/8
    bone.roll = 0.0000
    bone.use_connect = False
    bones['spine'] = bone.name
    bone = arm.edit_bones.new('spine.001')
    bone.head[:] = 0.0000, 0.5000/8, 1.0000/8
    bone.tail[:] = 0.0000, 0.7500/8, 2.0000/8
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine']]
    bones['spine.001'] = bone.name
    bone = arm.edit_bones.new('spine.002')
    bone.head[:] = 0.0000, 0.7500/8, 2.0000/8
    bone.tail[:] = 0.0000, 0.5000/8, 3.0000/8
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.001']]
    bones['spine.002'] = bone.name
    bone = arm.edit_bones.new('spine.003')
    bone.head[:] = 0.0000, 0.5000/8, 3.0000/8
    bone.tail[:] = 0.0000, 0.0000, 4.0000/8
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.002']]
    bones['spine.003'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['spine']]
    pbone.rigify_type = 'experimental.super_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.003']]
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
        if bcoll := arm.collections.active:
            bcoll.assign(bone)
