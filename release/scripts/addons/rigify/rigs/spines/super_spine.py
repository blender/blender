import bpy
from mathutils import Vector
from ...utils import copy_bone, flip_bone, put_bone, org, align_bone_y_axis, align_bone_x_axis
from ...utils import strip_org, make_deformer_name, connected_children_names
from ...utils import create_circle_widget, create_sphere_widget, create_neck_bend_widget, create_neck_tweak_widget
from ..widgets import create_ballsocket_widget
from ...utils import MetarigError, make_mechanism_name, create_cube_widget
from rna_prop_ui import rna_idprop_ui_prop_get

script = """
controls = [%s]
torso    = '%s'

if is_selected( controls ):
    if hasattr(pose_bones[torso],'["%s"]'):
        layout.prop( pose_bones[ torso ], '["%s"]', slider = True )
    if hasattr(pose_bones[torso],'["%s"]'):
        layout.prop( pose_bones[ torso ], '["%s"]', slider = True )
    if hasattr(pose_bones[torso],'["%s"]'):
        layout.prop( pose_bones[ torso ], '["%s"]', slider = True )
"""


class Rig:

    def __init__(self, obj, bone_name, params):
        """ Initialize torso rig and key rig properties """

        eb = obj.data.edit_bones

        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params
        # self.spine_length = sum([eb[b].length for b in self.org_bones])
        self.copy_rotation_axes = params.copy_rotation_axes
        self.use_head = params.use_head
        self.use_tail = params.use_tail

        # Check if user provided the pivot position
        if params.pivot_pos:
            self.pivot_pos = params.pivot_pos + 1
        else:
            raise MetarigError(
                "RIGIFY ERROR: please specify pivot bone position"
            )

        # Check if neck is lower than pivot
        if self.use_head and params.neck_pos <= params.pivot_pos and params.neck_pos != 0:
            raise MetarigError(
                "RIGIFY ERROR: Neck cannot be below or the same as pivot. (use 0 for no neck)"
            )
        else:
            self.neck_pos = params.neck_pos

        if not self.use_head:
            self.neck_pos = len(self.org_bones)

        if self.use_tail and self.pivot_pos - 2 > 0:
            self.tail_pos = params.tail_pos

        # Assign values to tweak layers props if opted by user
        if params.tweak_extra_layers:
            self.tweak_layers = list(params.tweak_layers)
        else:
            self.tweak_layers = None

        # Report error of user created less than the minimum of bones for rig
        min_bone_number = 3
        if self.use_head:
            min_bone_number += 1
        if self.use_tail:
            min_bone_number += 2

        if len(self.org_bones) < min_bone_number:
            raise MetarigError(
                "RIGIFY ERROR: invalid rig structure on %s" % (strip_org(bone_name))
            )

    def build_bone_structure(self):
        """ Divide meta-rig into lists of bones according to torso rig anatomy:
            Neck --> Upper torso --> Lower torso --> Tail (optional) """

        if self.pivot_pos and (self.neck_pos == 0 or self.neck_pos > self.pivot_pos):

            neck_index = self.neck_pos - 1
            pivot_index = self.pivot_pos - 1

            tail_index = 0
            if self.use_tail and self.tail_pos > 1:     # 2 bones for the tail at least
                tail_index = self.tail_pos - 1

            if self.use_head:
                neck_bones = self.org_bones[neck_index::]
                upper_torso_bones = self.org_bones[pivot_index :neck_index]
            else:
                neck_bones = []
                upper_torso_bones = self.org_bones[pivot_index ::]

            tail_bones = []
            if tail_index:
                lower_torso_bones = self.org_bones[tail_index + 1:pivot_index ]
                tail_bones = self.org_bones[:tail_index+1]
            else:
                lower_torso_bones = self.org_bones[:pivot_index ]

            torso_bones = upper_torso_bones + lower_torso_bones
            eb = self.obj.data.edit_bones
            self.spine_length = sum([eb[b].length for b in torso_bones])

            return {
                'neck': neck_bones,
                'upper': upper_torso_bones,
                'lower': lower_torso_bones,
                'tail': tail_bones
            }

        else:
            return 'ERROR'

    def orient_bone(self, eb, axis, scale, reverse=False):
        v = Vector((0,0,0))

        setattr(v, axis, scale)

        if reverse:
            tail_vec = v * self.obj.matrix_world
            eb.head[:] = eb.tail
            eb.tail[:] = eb.head + tail_vec
        else:
            tail_vec = v * self.obj.matrix_world
            eb.tail[:] = eb.head + tail_vec

    def create_pivot(self, pivot):
        """ Create the pivot control and mechanism bones """
        org_bones = self.org_bones
        pivot_name = org_bones[pivot-1]

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Create torso control bone
        torso_name = 'torso'
        ctrl_name = copy_bone(self.obj, pivot_name, torso_name)
        ctrl_eb = eb[ctrl_name]

        self.orient_bone(ctrl_eb, 'y', self.spine_length * 0.6)

        # Create mch_pivot
        mch_name = make_mechanism_name('pivot')
        mch_name = copy_bone(self.obj, ctrl_name, mch_name)
        mch_eb = eb[mch_name]

        mch_eb.length /= 4

        # Positioning pivot in a more usable location for animators
        # if self.use_tail and self.tail_pos > 0:
        #     pivot_loc = eb[org_bones[pivot-1]].head
        if self.use_tail and self.tail_pos > 0:
            first_torso_bone = self.tail_pos
            pivot_loc = (eb[org_bones[first_torso_bone]].head + eb[org_bones[first_torso_bone]].tail)/2
        else:
            pivot_loc = (eb[org_bones[0]].head + eb[org_bones[0]].tail) / 2

        put_bone(self.obj, ctrl_name, pivot_loc)

        return {
            'ctrl': ctrl_name,
            'mch': mch_name
        }

    def create_deform(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        def_bones = []
        for org_b in org_bones:
            def_name = make_deformer_name(strip_org(org_b))
            def_name = copy_bone(self.obj, org_b, def_name)
            def_bones.append(def_name)

        return def_bones

    def create_neck(self, neck_bones):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        if not self.use_head:
            return {
                'ctrl_neck': '',
                'ctrl': '',
                'mch_str': '',
                'mch_neck': '',
                'mch_head': '',
                'mch': [],
                'tweak': [],
                'neck_bend': '',
                'original_names': neck_bones
            }

        neck, neck_bend = '', ''
        if len(neck_bones) >= 2:
            # Create neck control
            neck = copy_bone(self.obj, org(neck_bones[0]), 'neck')
            neck_eb = eb[neck]

            # Neck spans all neck bones (except head)
            neck_eb.tail[:] = eb[org(neck_bones[-1])].head

            if len(neck_bones) > 3:

                # Create neck bend control
                neck_bend = copy_bone(self.obj, org(neck_bones[0]), 'neck_bend')
                neck_bend_eb = eb[neck_bend]

                # Neck pivot position
                if (len(neck_bones)-1) % 2:     # odd num of neck bones (head excluded)
                    center_bone = org(neck_bones[int((len(neck_bones))/2) - 1])
                    neck_bend_eb.head = (eb[center_bone].head + eb[center_bone].tail)/2
                else:
                    center_bone = org(neck_bones[int((len(neck_bones)-1)/2) - 1])
                    neck_bend_eb.head = eb[center_bone].tail

                align_bone_y_axis(self.obj, neck_bend, eb[neck].y_axis)
                align_bone_x_axis(self.obj, neck_bend, eb[neck].x_axis)
                eb[neck_bend].length = eb[neck].length / 2

        # Create head control
        head = copy_bone(self.obj, org(neck_bones[-1]), 'head')

        # MCH bones
        mch_str, mch_neck = '', ''
        if len(neck_bones) >= 2:
            # Neck MCH stretch
            mch_str = copy_bone(self.obj, neck, make_mechanism_name('STR-neck'))

            # Neck MCH rotation
            mch_neck = copy_bone(
                self.obj, neck, make_mechanism_name('ROT-neck')
            )

            self.orient_bone(eb[mch_neck], 'y', self.spine_length / 10)

        # Head MCH rotation
        mch_head = copy_bone(self.obj, head, make_mechanism_name('ROT-head'))
        self.orient_bone(eb[mch_head], 'y', self.spine_length / 10)

        twk, mch = [], []

        if len(neck_bones) >= 2:
            # Intermediary bones
            for b in neck_bones[1:-1]:  # All except 1st (neck) and last (head)
                mch_name = copy_bone(self.obj, org(b), make_mechanism_name(b))
                eb[mch_name].length /= 4
                align_bone_y_axis(self.obj, mch_name, eb[neck].y_axis)
                align_bone_x_axis(self.obj, mch_name, eb[neck].x_axis)
                eb[mch_name].use_inherit_scale = False
                mch += [mch_name]

            # Tweak bones
            for b in neck_bones[:-1]:   # All except last bone
                twk_name = "tweak_" + b
                twk_name = copy_bone(self.obj, org(b), twk_name)

                eb[twk_name].length /= 2

                twk += [twk_name]

        return {
            'ctrl_neck': neck,
            'ctrl': head,
            'mch_str': mch_str,
            'mch_neck': mch_neck,
            'mch_head': mch_head,
            'mch': mch,
            'tweak': twk,
            'neck_bend': neck_bend,
            'original_names': neck_bones
        }

    def create_chest(self, chest_bones):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # get total spine length

        # Create chest control bone
        chest = copy_bone(self.obj, org(chest_bones[0]), 'chest')
        self.orient_bone(eb[chest], 'y', self.spine_length / 3)

        # create chest mch_wgt
        mch_wgt = copy_bone(
            self.obj, org(chest_bones[-1]),
            make_mechanism_name('WGT-chest')
        )

        # Create mch and twk bones
        twk, mch = [], []

        for b in chest_bones:
            mch_name = copy_bone( self.obj, org(b), make_mechanism_name(b) )
            self.orient_bone( eb[mch_name], 'y', self.spine_length / 10 )

            twk_name = "tweak_" + b
            twk_name = copy_bone( self.obj, org(b), twk_name )
            eb[twk_name].length /= 2

            mch += [ mch_name ]
            twk += [ twk_name ]

        return {
            'ctrl'    : chest,
            'mch'     : mch,
            'tweak'   : twk,
            'mch_wgt' : mch_wgt
        }

    def create_hips(self, hip_bones):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Create hips control bone
        hips = copy_bone(self.obj, org(hip_bones[-1]), 'hips')
        self.orient_bone(
            eb[hips],
            'y',
            self.spine_length / 4,
            reverse = True
        )

        # create hips mch_wgt
        mch_wgt = copy_bone(
            self.obj, org(hip_bones[0]),
            make_mechanism_name('WGT-hips')
        )

        # Create mch and tweak bones
        twk, mch = [], []
        for b in hip_bones:
            mch_name = copy_bone( self.obj, org(b), make_mechanism_name(b) )
            self.orient_bone(
                eb[mch_name], 'y', self.spine_length / 10, reverse = True
            )

            twk_name = "tweak_" + b
            twk_name = copy_bone( self.obj, org(b), twk_name )

            eb[twk_name].length /= 2

            mch += [ mch_name ]
            twk += [ twk_name ]

        return {
            'ctrl'    : hips,
            'mch'     : mch,
            'tweak'   : twk,
            'mch_wgt' : mch_wgt
        }

    def create_tail(self, tail_bones):
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones
        org_bones = self.org_bones

        ctrl_chain = []
        for i in range(len(tail_bones)):
            name = tail_bones[i]

            ctrl_bone = copy_bone(
                self.obj,
                org(name),
                strip_org(name)
            )

            flip_bone(self.obj, ctrl_bone)
            ctrl_chain.append(ctrl_bone)

        # Main ctrl
        name = tail_bones[-1]
        main_ctrl_bone = copy_bone(
            self.obj,
            org(name),
            strip_org(name).split('.')[0] + "_master"
        )
        flip_bone(self.obj, main_ctrl_bone)

        mch_tail = ''
        tail_first = org_bones[self.tail_pos-1]
        mch_rot_tail = copy_bone(
                self.obj,
                org(tail_first),
                make_mechanism_name("ROT-tail")
            )

        self.orient_bone(eb[mch_rot_tail], 'y', eb[tail_first].length)
        put_bone(self.obj, mch_rot_tail, eb[tail_first].tail)
        mch_tail = mch_rot_tail

        tweak_chain = []
        for i in range(len(tail_bones)):
            name = tail_bones[i]

            tweak_bone = copy_bone(
                self.obj,
                org(name),
                "tweak_" + strip_org(name)
            )

            tweak_e = eb[tweak_bone]
            tweak_e.length /= 2  # Set size to half

            # Position tweaks
            flip_bone(self.obj, tweak_bone)
            put_bone(self.obj, tweak_bone, eb[org(name)].head)

            tweak_chain.append(tweak_bone)

        return {
            'ctrl': ctrl_chain,
            'ctrl_tail': main_ctrl_bone,
            'mch_tail': mch_tail,
            'tweak': tweak_chain,
            'original_names': tail_bones
        }

    def parent_bones(self, bones):
        org_bones = self.org_bones
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Parent deform bones
        for i, b in enumerate(bones['def']):
            if i > 0:   # For all bones but the first (which has no parent)
                eb[b].parent = eb[bones['def'][i-1]]    # to previous
                eb[b].use_connect = True

        # Parent control bones
        # Head control => MCH-rotation_head
        if self.use_head:
            eb[bones['neck']['ctrl']].parent = eb[bones['neck']['mch_head']]

        # Tail control chain
        if self.use_tail:
            tail_ctrl = bones['tail']['ctrl']
            for i, b in enumerate(tail_ctrl[:-1]):
                eb[b].parent = eb[tail_ctrl[i+1]]
            eb[tail_ctrl[-1]].parent = eb[bones['tail']['mch_tail']]
            eb[bones['tail']['ctrl_tail']].parent = eb[org_bones[self.tail_pos]]

        if bones['neck']['ctrl_neck']:
            # MCH stretch => neck ctrl
            eb[ bones['neck']['mch_str']].parent = eb[ bones['neck']['ctrl_neck']]

            # Neck control => MCH-rotation_neck
            eb[bones['neck']['ctrl_neck']].parent = eb[bones['neck']['mch_neck']]

            # Neck pivot => MCH-rotation_neck
            if bones['neck']['neck_bend']:
                # eb[bones['neck']['neck_bend']].parent = eb[bones['neck']['ctrl_neck']]
                eb[bones['neck']['neck_bend']].parent = eb[bones['neck']['mch_str']]

        # Parent hips and chest controls to torso
        eb[bones['chest']['ctrl']].parent = eb[bones['pivot']['ctrl']]
        eb[bones['hips']['ctrl']].parent = eb[bones['pivot']['ctrl']]

        # Parent mch bones
        if bones['neck']['ctrl_neck']:
            # Neck mch
            eb[bones['neck']['mch_head']].parent = eb[bones['neck']['ctrl_neck']]
        elif self.use_head:
            eb[bones['neck']['mch_head']].parent = eb[bones['chest']['mch'][-1]]

        for i, b in enumerate([eb[n] for n in bones['neck']['mch']]):
            b.parent = eb[bones['neck']['mch_str']]
            # for org_b in bones['neck']['original_names']:
            #     if org_b in b.name:
            #         b.parent = eb[org(org_b)]

        # Chest mch bones and neck mch
        chest_mch = bones['chest']['mch'] + [bones['neck']['mch_neck']]
        for i, b in enumerate(chest_mch):
            if i == 0:
                eb[b].parent = eb[bones['pivot']['ctrl']]
            elif b:
                eb[b].parent = eb[chest_mch[i-1]]

        # Hips mch bones
        for i, b in enumerate(bones['hips']['mch']):
            if i == len(bones['hips']['mch']) - 1:
                eb[b].parent = eb[bones['pivot']['ctrl']]
            else:
                eb[b].parent = eb[bones['hips']['mch'][i+1]]

        # mch pivot
        eb[bones['pivot']['mch']].parent = eb[bones['chest']['mch'][0]]

        # MCH widgets
        eb[bones['chest']['mch_wgt']].parent = eb[bones['chest']['mch'][-1]]
        eb[bones['hips']['mch_wgt']].parent = eb[bones['hips']['mch'][0]]

        # Neck Tweaks
        if bones['neck']['tweak']:
            # Neck tweaks
            for i, twk in enumerate( bones['neck']['tweak']):
                if i == 0:
                    eb[twk].parent = eb[ bones['neck']['ctrl_neck']]
                else:
                    eb[twk].parent = eb[ bones['neck']['mch'][i-1]]

        # Chest tweaks
        for twk, mch in zip( bones['chest']['tweak'], bones['chest']['mch']):
            if bones['chest']['tweak'].index(twk) == 0:
                eb[twk].parent = eb[bones['pivot']['mch']]
            else:
                eb[twk].parent = eb[mch]

        # Hips tweaks
        for i, twk in enumerate(bones['hips']['tweak']):
            if i == 0:
                eb[twk].parent = eb[bones['hips']['mch'][i]]
            else:
                eb[twk].parent = eb[bones['hips']['mch'][i-1]]

        # Tail mchs
        if self.use_tail:
            mch_rot_tail = bones['tail']['mch_tail']
            eb[mch_rot_tail].parent = eb[bones['hips']['tweak'][0]]

        # Tail tweaks
        if self.use_tail:
            for i, twk in enumerate(bones['tail']['tweak']):
                if i == 0:
                    eb[twk].parent = eb[bones['tail']['ctrl'][i]]
                else:
                    eb[twk].parent = eb[bones['tail']['ctrl'][i-1]]

        # Parent orgs to matching tweaks
        tweaks = []
        if self.use_tail:
            tweaks += bones['tail']['tweak']

        tweaks += bones['hips']['tweak'] + bones['chest']['tweak']
        if self.use_head:
            tweaks += bones['neck']['tweak'] + [bones['neck']['ctrl']]

        original_neck_bones = [org(b) for b in bones['neck']['original_names']]
        original_neck_bones = original_neck_bones[1:-1]  # exclude first neck bone and head
        for b, twk in zip(org_bones[:-1], tweaks):
            if b in original_neck_bones and len(bones['neck']['original_names']) > 3:
                idx = org_bones.index(b)
                org_parent = org_bones[idx-1]
                eb[b].parent = eb[org_parent]
            else:
                eb[b].parent = eb[twk]

        if self.use_head:
            eb[org_bones[-1]].parent = eb[bones['neck']['ctrl']]

    def make_constraint(self, bone, constraint):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        owner_pb = pb[bone]
        const = owner_pb.constraints.new(constraint['constraint'])
        const.target = self.obj

        # filter contraint props to those that actually exist in the currnet
        # type of constraint, then assign values to each
        for p in [k for k in constraint.keys() if k in dir(const)]:
            setattr(const, p, constraint[p])

    def constrain_bones(self, bones):
        # MCH bones

        # head and neck MCH bones
        for b in [bones['neck']['mch_head'], bones['neck']['mch_neck']]:
            if b:
                self.make_constraint(b, {
                    'constraint': 'COPY_ROTATION',
                    'subtarget': bones['pivot']['ctrl'],
                })
                self.make_constraint(b, {
                    'constraint': 'COPY_SCALE',
                    'subtarget': bones['pivot']['ctrl'],
                })

        if bones['neck']['mch_str']:
            # Neck MCH Stretch
            self.make_constraint(bones['neck']['mch_str'], {
                'constraint': 'DAMPED_TRACK',
                'subtarget': bones['neck']['ctrl'],
            })
            self.make_constraint(bones['neck']['mch_str'], {
                'constraint': 'STRETCH_TO',
                'subtarget': bones['neck']['ctrl'],
            })

        # Intermediary mch bones
        intermediaries = [bones['neck'], bones['chest'], bones['hips']]

        for i, l in enumerate(intermediaries):
            mch = l['mch']

            for j, b in enumerate(mch):

                if i == 0:      # Neck mch-s
                    if len(bones['neck']['original_names']) > 3:
                        self.make_constraint(b, {
                            'constraint': 'COPY_LOCATION',
                            'subtarget': org(l['original_names'][j+1]),
                            'influence': 1.0
                        })
                    else:
                        nfactor = float((j + 1) / len(mch))
                        self.make_constraint(b, {
                            'constraint': 'COPY_ROTATION',
                            'subtarget': l['ctrl'],
                            'influence': nfactor
                        })

                    step = 2/(len(mch)+1)
                    xval = (j+1)*step
                    influence = 2*xval - xval**2    #parabolic influence of pivot

                    if bones['neck']['neck_bend']:
                        self.make_constraint(b, {
                            'constraint': 'COPY_LOCATION',
                            'subtarget': l['neck_bend'],
                            'influence': influence,
                            'use_offset': True,
                            'owner_space': 'LOCAL',
                            'target_space': 'LOCAL'
                        })

                    if len(bones['neck']['original_names']) > 3:
                        self.make_constraint(b, {
                            'constraint': 'COPY_SCALE',
                            'subtarget': org(l['original_names'][j+1]),
                            'influence': 1.0
                        })

                else:
                    factor = float(1 / len(l['tweak']))
                    self.make_constraint(b, {
                        'constraint': 'COPY_TRANSFORMS',
                        'subtarget': l['ctrl'],
                        'influence': factor,
                        'owner_space': 'LOCAL',
                        'target_space': 'LOCAL'
                    })

        # Tail ctrls
        if self.use_tail:
            tail_ctrl = bones['tail']['ctrl']
            tail_ctrl.append(bones['tail']['ctrl_tail'])

            for i, b in enumerate(tail_ctrl[:-1]):
                self.make_constraint(b, {
                    'constraint': 'COPY_ROTATION',
                    'subtarget': tail_ctrl[i+1],
                    'influence': 1.0,
                    'use_x': self.copy_rotation_axes[0],
                    'use_y': self.copy_rotation_axes[1],
                    'use_z': self.copy_rotation_axes[2],
                    'use_offset': True,
                    'owner_space': 'LOCAL',
                    'target_space': 'LOCAL'
                })

            b = bones['tail']['mch_tail']
            self.make_constraint(b, {
                'constraint': 'COPY_ROTATION',
                'subtarget': bones['pivot']['ctrl'],
                'influence': 1.0,
            })

        # MCH pivot
        self.make_constraint(bones['pivot']['mch'], {
            'constraint': 'COPY_TRANSFORMS',
            'subtarget': bones['hips']['mch'][-1],
            'owner_space': 'LOCAL',
            'target_space': 'LOCAL'
        })

        # DEF bones
        deform = bones['def']
        tweaks = []
        if self.use_tail:
            tweaks += bones['tail']['tweak']

        tweaks += bones['hips']['tweak'] + bones['chest']['tweak']
        if self.use_head:
            tweaks += bones['neck']['tweak'] + [bones['neck']['ctrl']]

        for d, t in zip(deform, tweaks):
            tidx = tweaks.index(t)

            self.make_constraint(d, {
                'constraint': 'COPY_TRANSFORMS',
                'subtarget': t
            })

            if tidx != len(tweaks) - 1:
                if self.use_tail and t in bones['tail']['tweak']:
                    self.make_constraint(d, {
                        'constraint': 'DAMPED_TRACK',
                        'subtarget': tweaks[tidx + 1],
                        'track_axis': 'TRACK_NEGATIVE_Y'
                    })
                else:
                    self.make_constraint(d, {
                        'constraint': 'DAMPED_TRACK',
                        'subtarget': tweaks[tidx + 1],
                    })

                self.make_constraint(d, {
                    'constraint': 'STRETCH_TO',
                    'subtarget': tweaks[tidx + 1],
                })

        pb = self.obj.pose.bones

        if bones['neck']['neck_bend']:
            pb[bones['neck']['neck_bend']].rotation_mode = 'ZXY'
            pb[bones['neck']['neck_bend']].lock_rotation[0] = True
            pb[bones['neck']['neck_bend']].lock_rotation[2] = True

        for t in tweaks:
            if t != bones['neck']['ctrl']:
                pb[t].rotation_mode = 'ZXY'

        original_neck_bones = [org(b) for b in bones['neck']['original_names']]
        # make IK on neck ORGs
        if len(original_neck_bones) > 3:
            last_neck = original_neck_bones[-2]
            self.make_constraint(last_neck, {
                'constraint': 'IK',
                'subtarget': bones['neck']['ctrl'],
                'chain_count': len(original_neck_bones) - 1
            })

            for b in original_neck_bones[:-1]:
                pb[b].ik_stretch = 0.1

    def create_drivers(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # Setting the torso's props
        torso = pb[bones['pivot']['ctrl']]

        props = []
        owners = []

        if self.use_head:
            props += ["head_follow"]
            owners += [bones['neck']['mch_head']]
            if bones['neck']['mch_neck']:
                props += ["neck_follow"]
                owners += [bones['neck']['mch_neck']]
        if self.use_tail:
            props += ["tail_follow"]
            owners += [bones['tail']['mch_tail']]

        for prop in props:
            if prop == 'neck_follow':
                torso[prop] = 0.5
            else:
                torso[prop] = 0.0

            prop = rna_idprop_ui_prop_get(torso, prop, create=True)
            prop["min"] = 0.0
            prop["max"] = 1.0
            prop["soft_min"] = 0.0
            prop["soft_max"] = 1.0
            prop["description"] = prop

        # driving the follow rotation switches for neck and head
        for bone, prop, in zip(owners, props):
            # Add driver to copy rotation constraint
            drv = pb[bone].constraints[0].driver_add("influence").driver
            drv.type = 'AVERAGE'

            var = drv.variables.new()
            var.name = prop
            var.type = "SINGLE_PROP"
            var.targets[0].id = self.obj
            var.targets[0].data_path = \
                torso.path_from_id() + '[' + '"' + prop + '"' + ']'

            drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

            drv_modifier.mode = 'POLYNOMIAL'
            drv_modifier.poly_order = 1
            drv_modifier.coefficients[0] = 1.0
            drv_modifier.coefficients[1] = -1.0

    def locks_and_widgets(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        # deform bones bbone segements
        for bone in bones['def'][:-1]:
            self.obj.data.bones[bone].bbone_segments = 8

        self.obj.data.bones[bones['def'][0]].bbone_in = 0.0
        self.obj.data.bones[bones['def'][-2]].bbone_out = 1.0

        # Locks
        tweaks = bones['neck']['tweak'] + bones['chest']['tweak']
        tweaks += bones['hips']['tweak']

        if self.use_tail:
            tweaks += bones['tail']['tweak']
            pb[bones['tail']['ctrl_tail']].lock_location = True, True, True

        # Tweak bones locks
        for bone in tweaks:
            pb[bone].lock_rotation = True, False, True
            pb[bone].lock_scale = False, True, False

        # Widgets

        # Assigning a widget to torso bone
        create_cube_widget(
            self.obj,
            bones['pivot']['ctrl'],
            radius=0.5,
            bone_transform_name=None
        )

        # Assigning widgets to control bones
        gen_ctrls = [
            bones['chest']['ctrl'],
            bones['hips']['ctrl']
        ]

        tail_ctrls = []
        if self.use_tail and bones['tail']['ctrl']:
            tail_ctrls = bones['tail']['ctrl'] + [bones['tail']['ctrl_tail']]
            gen_ctrls.extend(bones['tail']['ctrl'])

            create_ballsocket_widget(
                self.obj,
                bones['tail']['ctrl_tail'],
                size=0.7,
                bone_transform_name=None
            )

        for bone in gen_ctrls:

            if bone in tail_ctrls:
                radius = 0.5
            else:
                radius = 1.0

            # place chest on neck-base for very long necks
            if bone == bones['chest']['ctrl'] and len(bones['neck']['original_names']) > 3:
                head_tail = 0.0
            else:
                head_tail = 0.75

            create_circle_widget(
                self.obj,
                bone,
                radius=radius,
                head_tail=head_tail,
                with_line=False,
                bone_transform_name=None
            )

        if bones['neck']['ctrl_neck']:
            # Neck ctrl widget
            if len(bones['neck']['mch']) == 0:
                radius = 1
            else:
                radius = 1/(len(bones['neck']['mch']))
            create_circle_widget(
                self.obj,
                bones['neck']['ctrl_neck'],
                radius=radius,
                head_tail=0.5,
                bone_transform_name=None
            )

        if bones['neck']['neck_bend']:
            # Neck pivot widget
            if len(bones['neck']['mch']) == 0:
                radius = 0.5
            else:
                radius = 1/(2*len(bones['neck']['mch']))
            create_neck_bend_widget(
                self.obj,
                bones['neck']['neck_bend'],
                radius=radius,
                head_tail=0.0,
                bone_transform_name=None
            )

        # Head widget
        # place wgt @ middle of head bone for long necks
        if len(bones['neck']['original_names']) > 3:
            head_tail = 0.5
        else:
            head_tail = 1.0
        if self.use_head:
            create_circle_widget(
                self.obj,
                bones['neck']['ctrl'],
                radius              = 0.5,
                head_tail           = head_tail,
                with_line           = False,
                bone_transform_name = None
            )

        # place widgets on correct bones
        chest_widget_loc = pb[bones['chest']['mch_wgt']]
        pb[bones['chest']['ctrl']].custom_shape_transform = chest_widget_loc

        hips_widget_loc = pb[bones['hips']['mch_wgt']]

        if self.use_tail:
            hips_widget_loc = pb[bones['def'][self.tail_pos]]
            pb[bones['tail']['ctrl_tail']].custom_shape_transform = pb[bones['tail']['tweak'][0]]

        pb[bones['hips']['ctrl']].custom_shape_transform = hips_widget_loc

        # Assigning widgets to tweak bones and layers
        for bone in tweaks:

            if bones['neck']['tweak'] and bone == bones['neck']['tweak'][0] \
                    and len(bones['neck']['original_names']) > 3:
                create_neck_tweak_widget(self.obj, bone, size=1.0, bone_transform_name=None)
                continue
            create_sphere_widget(self.obj, bone, bone_transform_name=None)

            if self.tweak_layers:
                pb[bone].bone.layers = self.tweak_layers

    def generate(self):
        # Torso Rig Anatomy:
        # Neck: all bones above neck point, last bone is head
        # Upper torso: all bones between pivot and neck start
        # Lower torso: all bones below pivot until tail point
        # Tail: all bones below tail point

        bone_chains = self.build_bone_structure()

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Clear parents for org bones
        for bone in self.org_bones:
            eb[bone].use_connect = False
            eb[bone].parent = None

        if bone_chains != 'ERROR':

            # Create lists of bones and strip "ORG" from their names
            neck_bones = [strip_org(b) for b in bone_chains['neck']]
            upper_torso_bones = [strip_org(b) for b in bone_chains['upper']]
            lower_torso_bones = [strip_org(b) for b in bone_chains['lower']]
            tail_bones = [strip_org(b) for b in bone_chains['tail']]

            bones = {}

            bones['def'] = self.create_deform()     # Gets org bones from self
            bones['pivot'] = self.create_pivot(self.pivot_pos)
            bones['neck'] = self.create_neck(neck_bones)
            bones['chest'] = self.create_chest(upper_torso_bones)
            bones['hips'] = self.create_hips(lower_torso_bones)

            # TODO: Add create tail
            if tail_bones:
                bones['tail'] = self.create_tail(tail_bones)

            # TEST
            bpy.ops.object.mode_set(mode='EDIT')
            eb = self.obj.data.edit_bones

            self.parent_bones(bones)
            self.constrain_bones(bones)
            self.create_drivers(bones)
            self.locks_and_widgets(bones)

        else:
            return

        controls = [bones['neck']['ctrl'],  bones['neck']['ctrl_neck']]
        controls += [bones['chest']['ctrl'], bones['hips']['ctrl']]
        controls += [bones['pivot']['ctrl']]

        if self.use_tail:
            controls.extend(bones['tail']['ctrl'])

        # Create UI
        controls_string = ", ".join(["'" + x + "'" for x in controls])
        return [script % (
            controls_string,
            bones['pivot']['ctrl'],
            'head_follow',
            'head_follow',
            'neck_follow',
            'neck_follow',
            'tail_follow',
            'tail_follow',
            )]


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """
    params.neck_pos = bpy.props.IntProperty(
        name        = 'neck_position',
        default     = 6,
        min         = 0,
        description = 'Neck start position'
    )

    params.pivot_pos = bpy.props.IntProperty(
        name='pivot_position',
        default=2,
        min=0,
        description='Position of the torso control and pivot point'
    )

    params.copy_rotation_axes = bpy.props.BoolVectorProperty(
        size=3,
        description="Automation axes",
        default=tuple([i == 0 for i in range(0, 3)])
    )

    params.tail_pos = bpy.props.IntProperty(
        name='tail_position',
        default=2,
        min=2,
        description='Where the tail starts'
    )

    params.use_tail = bpy.props.BoolProperty(
        name='use_tail',
        default=False,
        description='Create tail bones'
    )

    params.use_head = bpy.props.BoolProperty(
        name='use_head',
        default=True,
        description='Create head and neck bones'
    )

    # Setting up extra layers for the FK and tweak
    params.tweak_extra_layers = bpy.props.BoolProperty(
        name="tweak_extra_layers",
        default=True,
        description=""
        )

    params.tweak_layers = bpy.props.BoolVectorProperty(
        size        = 32,
        description = "Layers for the tweak controls to be on",
        default     = tuple( [ i == 1 for i in range(0, 32) ] )
        )


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    r = layout.row(align=True)
    r.prop(params, "use_head", toggle=True, text="Head")
    r.prop(params, "use_tail", toggle=True, text="Tail")

    r = layout.row()
    r.prop(params, "neck_pos")
    r.enabled = params.use_head

    r = layout.row()
    r.prop(params, "pivot_pos")

    r = layout.row()
    r.prop(params, "tail_pos")
    r.enabled = params.use_tail

    r = layout.row()
    col = r.column(align=True)
    row = col.row(align=True)
    for i, axis in enumerate(['x', 'y', 'z']):
        row.prop(params, "copy_rotation_axes", index=i, toggle=True, text=axis)
    r.enabled = params.use_tail

    r = layout.row()
    r.prop(params, "tweak_extra_layers")
    r.active = params.tweak_extra_layers

    col = r.column(align=True)
    row = col.row(align=True)

    bone_layers = bpy.context.active_pose_bone.bone.layers[:]

    for i in range(8):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    row = col.row(align=True)

    for i in range(16, 24):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    col = r.column(align=True)
    row = col.row(align=True)

    for i in range(8, 16):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    row = col.row(align=True)

    for i in range(24, 32):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('spine')
    bone.head[:] = 0.0000, 0.0552, 1.0099
    bone.tail[:] = 0.0000, 0.0172, 1.1573
    bone.roll = 0.0000
    bone.use_connect = False
    bones['spine'] = bone.name

    bone = arm.edit_bones.new('spine.001')
    bone.head[:] = 0.0000, 0.0172, 1.1573
    bone.tail[:] = 0.0000, 0.0004, 1.2929
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine']]
    bones['spine.001'] = bone.name

    bone = arm.edit_bones.new('spine.002')
    bone.head[:] = 0.0000, 0.0004, 1.2929
    bone.tail[:] = 0.0000, 0.0059, 1.4657
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.001']]
    bones['spine.002'] = bone.name

    bone = arm.edit_bones.new('spine.003')
    bone.head[:] = 0.0000, 0.0059, 1.4657
    bone.tail[:] = 0.0000, 0.0114, 1.6582
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.002']]
    bones['spine.003'] = bone.name

    bone = arm.edit_bones.new('spine.004')
    bone.head[:] = 0.0000, 0.0114, 1.6582
    bone.tail[:] = 0.0000, -0.013, 1.7197
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.003']]
    bones['spine.004'] = bone.name

    bone = arm.edit_bones.new('spine.005')
    bone.head[:] = 0.0000, -0.013, 1.7197
    bone.tail[:] = 0.0000, -0.0247, 1.7813
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.004']]
    bones['spine.005'] = bone.name

    bone = arm.edit_bones.new('spine.006')
    bone.head[:] = 0.0000, -0.0247, 1.7813
    bone.tail[:] = 0.0000, -0.0247, 1.9796
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.005']]
    bones['spine.006'] = bone.name


    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['spine']]
    pbone.rigify_type = 'spines.super_spine'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

    try:
        pbone.rigify_parameters.neck_pos = 5
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.tweak_layers = [False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    except AttributeError:
        pass
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
    pbone = obj.pose.bones[bones['spine.004']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.005']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.006']]
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
