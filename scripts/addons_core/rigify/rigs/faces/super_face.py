# SPDX-FileCopyrightText: 2014-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import re

from mathutils import Vector
from ...utils import copy_bone, flip_bone
from ...utils import org, strip_org, make_deformer_name, connected_children_names,\
    make_mechanism_name
from ...utils import create_widget
from ...utils.mechanism import make_property
from ...utils.layers import ControlLayersOption
from ..widgets import create_face_widget, create_eye_widget, create_eyes_widget,\
    create_ear_widget, create_jaw_widget, create_teeth_widget


script = """
all_controls   = [%s]
jaw_ctrl_name  = '%s'
eyes_ctrl_name = '%s'

if is_selected(all_controls):
    layout.prop(pose_bones[jaw_ctrl_name],  '["%s"]', slider=True)
    layout.prop(pose_bones[eyes_ctrl_name], '["%s"]', slider=True)
"""


# noinspection PyPep8Naming
class Rig:

    def __init__(self, obj, bone_name, params):
        self.obj = obj

        children = [
            "nose", "lip.T.L", "lip.B.L", "jaw", "ear.L", "ear.R", "lip.T.R",
            "lip.B.R", "brow.B.L", "lid.T.L", "brow.B.R", "lid.T.R",
            "forehead.L", "forehead.R", "forehead.L.001", "forehead.R.001",
            "forehead.L.002", "forehead.R.002", "eye.L", "eye.R", "cheek.T.L",
            "cheek.T.R", "teeth.T", "teeth.B", "tongue", "temple.L",
            "temple.R"
        ]

        # create_pose_lib( self.obj )

        children = [org(b) for b in children]
        grand_children = []

        for child in children:
            grand_children += connected_children_names(self.obj, child)

        self.org_bones = [bone_name] + children + grand_children
        self.face_length = obj.data.bones[self.org_bones[0]].length
        self.params = params

    def orient_org_bones(self):

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Adjust eye bones roll
        eb['ORG-eye.L'].roll = 0.0
        eb['ORG-eye.R'].roll = 0.0

    @staticmethod
    def symmetrical_split(bones):
        # RE pattern match right or left parts
        # match the letter "L" (or "R"), followed by an optional dot (".")
        # and 0 or more digits at the end of the string
        left_pattern = r'L\.?\d*$'
        right_pattern = r'R\.?\d*$'

        left = sorted([name for name in bones if re.search(left_pattern,  name)])
        right = sorted([name for name in bones if re.search(right_pattern, name)])

        return left, right

    def create_deformation(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        def_bones = []

        for org_name in org_bones:
            if 'face' in org_name or 'teeth' in org_name or 'eye' in org_name:
                continue

            def_name = make_deformer_name(strip_org(org_name))
            def_name = copy_bone(self.obj, org_name, def_name)
            def_bones.append(def_name)

            eb[def_name].use_connect = False
            eb[def_name].parent = None

        brow_top_names = [bone for bone in def_bones if 'brow.T' in bone]
        forehead_names = [bone for bone in def_bones if 'forehead' in bone]

        brow_left, brow_right = self.symmetrical_split(brow_top_names)
        forehead_left, forehead_right = self.symmetrical_split(forehead_names)

        brow_left = brow_left[1:]
        brow_right = brow_right[1:]
        brow_left.reverse()
        brow_right.reverse()

        for browL, browR, foreheadL, foreheadR in zip(
                brow_left, brow_right, forehead_left, forehead_right):

            eb[foreheadL].tail = eb[browL].head
            eb[foreheadR].tail = eb[browR].head

        return {'all': def_bones}

    def create_ctrl(self, bones):
        # create control bones
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        eyeL_ctrl_name = strip_org(bones['eyes'][0])
        eyeR_ctrl_name = strip_org(bones['eyes'][1])

        eyeL_ctrl_name = copy_bone(self.obj, bones['eyes'][0], eyeL_ctrl_name)
        eyeR_ctrl_name = copy_bone(self.obj, bones['eyes'][1], eyeR_ctrl_name)
        eyes_ctrl_name = copy_bone(self.obj, bones['eyes'][0], 'eyes')

        eyeL_ctrl_e = eb[eyeL_ctrl_name]
        eyeR_ctrl_e = eb[eyeR_ctrl_name]
        eyes_ctrl_e = eb['eyes']

        # eyes ctrls
        eyeL_e = eb[bones['eyes'][0]]
        eyeR_e = eb[bones['eyes'][1]]

        interpupillary_distance = eyeL_e.head - eyeR_e.head
        distance = (eyeL_e.head - eyeR_e.head) * 3
        distance = distance.cross((0, 0, 1))

        eyeL_ctrl_e.head += distance
        eyeR_ctrl_e.head += distance
        eyes_ctrl_e.head[:] = (eyeL_ctrl_e.head + eyeR_ctrl_e.head) / 2

        for bone in [eyeL_ctrl_e, eyeR_ctrl_e, eyes_ctrl_e]:
            # bone.tail[:] = bone.head + Vector( [ 0, 0, eyeL_e.length * 1.35 ] )
            bone.tail[:] = bone.head + Vector([0, 0, interpupillary_distance.length * 0.3144])

        # Widget for transforming the both eyes
        eye_master_names = []
        for bone in bones['eyes']:
            eye_master = copy_bone(
                self.obj,
                bone,
                'master_' + strip_org(bone)
            )

            eye_master_names.append(eye_master)

        # turbo: adding a master nose for transforming the whole nose
        master_nose = copy_bone(self.obj, 'ORG-nose.004', 'nose_master')
        eb[master_nose].tail[:] = \
            eb[master_nose].head + Vector([0, self.face_length / -4, 0])

        # ears ctrls
        earL_name = strip_org(bones['ears'][0])
        earR_name = strip_org(bones['ears'][1])

        earL_ctrl_name = copy_bone(self.obj, org(bones['ears'][0]), earL_name)
        earR_ctrl_name = copy_bone(self.obj, org(bones['ears'][1]), earR_name)

        # jaw ctrl
        jaw_ctrl_name = strip_org(bones['jaw'][2]) + '_master'
        jaw_ctrl_name = copy_bone(self.obj, bones['jaw'][2], jaw_ctrl_name)

        jawL_org_e = eb[bones['jaw'][0]]
        jawR_org_e = eb[bones['jaw'][1]]
        # jaw_org_e = eb[bones['jaw'][2]]

        eb[jaw_ctrl_name].head[:] = (jawL_org_e.head + jawR_org_e.head) / 2

        # teeth ctrls
        teethT_name = strip_org(bones['teeth'][0])
        teethB_name = strip_org(bones['teeth'][1])

        teethT_ctrl_name = copy_bone(self.obj, org(bones['teeth'][0]), teethT_name)
        teethB_ctrl_name = copy_bone(self.obj, org(bones['teeth'][1]), teethB_name)

        # tongue ctrl
        tongue_org = bones['tongue'].pop()
        tongue_name = strip_org(tongue_org) + '_master'

        tongue_ctrl_name = copy_bone(self.obj, tongue_org, tongue_name)

        flip_bone(self.obj, tongue_ctrl_name)

        # Assign widgets
        bpy.ops.object.mode_set(mode='OBJECT')

        # Assign each eye widgets
        create_eye_widget(self.obj, eyeL_ctrl_name)
        create_eye_widget(self.obj, eyeR_ctrl_name)

        # Assign eyes widgets
        create_eyes_widget(self.obj, eyes_ctrl_name)

        # Assign each eye_master widgets
        for master in eye_master_names:
            create_square_widget(self.obj, master)

        # Assign nose_master widget
        create_square_widget(self.obj, master_nose, size=1)

        # Assign ears widget
        create_ear_widget(self.obj, earL_ctrl_name)
        create_ear_widget(self.obj, earR_ctrl_name)

        # Assign jaw widget
        create_jaw_widget(self.obj, jaw_ctrl_name)

        # Assign teeth widget
        create_teeth_widget(self.obj, teethT_ctrl_name)
        create_teeth_widget(self.obj, teethB_ctrl_name)

        # Assign tongue widget ( using the jaw widget )
        create_jaw_widget(self.obj, tongue_ctrl_name)

        return {
            'eyes': [
                eyeL_ctrl_name,
                eyeR_ctrl_name,
                eyes_ctrl_name,
            ] + eye_master_names,
            'ears'   : [earL_ctrl_name, earR_ctrl_name    ],  # noqa: E202,E203
            'jaw'    : [jaw_ctrl_name                     ],  # noqa: E202,E203
            'teeth'  : [teethT_ctrl_name, teethB_ctrl_name],  # noqa: E202,E203
            'tongue' : [tongue_ctrl_name                  ],  # noqa: E202,E203
            'nose'   : [master_nose                       ]   # noqa: E202,E203
            }

    def create_tweak(self, bones, uniques, tails):
        # create tweak bones
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        tweaks = []

        for bone in bones + list(uniques.keys()):

            tweak_name = strip_org(bone)

            # pick name for unique bone from the uniques dictionary
            if bone in list(uniques.keys()):
                tweak_name = uniques[bone]

            tweak_name = copy_bone(self.obj, bone, tweak_name)
            eb[tweak_name].use_connect = False
            eb[tweak_name].parent = None

            if bone in uniques:
                if bone.split('.')[-1] == 'L':
                    symmetrical_bone = bone[:-1] + 'R'
                elif bone.split('.')[-1] == 'R':
                    symmetrical_bone = bone[:-1] + 'L'
                else:
                    assert False
                middle_position = (eb[bone].head + eb[symmetrical_bone].head)/2
                eb[tweak_name].head = middle_position
                eb[tweak_name].tail[0] = middle_position[0]
                eb[tweak_name].tail[1] = middle_position[1]

            tweaks.append(tweak_name)

            eb[tweak_name].tail[:] = \
                eb[tweak_name].head + Vector((0, 0, self.face_length / 7))

            # create tail bone
            if bone in tails:
                if 'lip.T.L.001' in bone:
                    tweak_name = copy_bone(self.obj, bone,  'lips.L')
                elif 'lip.T.R.001' in bone:
                    tweak_name = copy_bone(self.obj, bone,  'lips.R')
                else:
                    tweak_name = copy_bone(self.obj, bone,  tweak_name)

                eb[tweak_name].use_connect = False
                eb[tweak_name].parent = None

                eb[tweak_name].head = eb[bone].tail
                eb[tweak_name].tail[:] = \
                    eb[tweak_name].head + Vector((0, 0, self.face_length / 7))

                tweaks.append(tweak_name)

        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        primary_tweaks = [
            "lid.B.L.002", "lid.T.L.002", "lid.B.R.002", "lid.T.R.002",
            "chin", "brow.T.L.001", "brow.T.L.002", "brow.T.L.003",
            "brow.T.R.001", "brow.T.R.002", "brow.T.R.003", "lip.B",
            "lip.B.L.001", "lip.B.R.001", "cheek.B.L.001", "cheek.B.R.001",
            "lips.L", "lips.R", "lip.T.L.001", "lip.T.R.001", "lip.T",
            "nose.002", "nose.L.001", "nose.R.001"
        ]

        for bone in tweaks:
            if bone in primary_tweaks:
                ControlLayersOption.FACE_PRIMARY.assign(self.params, pb, [bone])
                create_face_widget(self.obj, bone, size=1.5)
            else:
                ControlLayersOption.FACE_SECONDARY.assign(self.params, pb, [bone])
                create_face_widget(self.obj, bone)

        return {'all': tweaks}

    def all_controls(self):
        org_bones = self.org_bones

        org_tongue_bones = sorted([bone for bone in org_bones if 'tongue' in bone])

        org_to_ctrls = {
            'eyes'   : ['eye.L',   'eye.R'       ],  # noqa: E202,E203
            'ears'   : ['ear.L',   'ear.R'       ],  # noqa: E202,E203
            'jaw'    : ['jaw.L',   'jaw.R', 'jaw'],  # noqa: E202,E203
            'teeth'  : ['teeth.T', 'teeth.B'     ],  # noqa: E202,E203
            'tongue' : [org_tongue_bones[0]      ]   # noqa: E202,E203
        }

        tweak_unique = {'lip.T.L': 'lip.T',
                        'lip.B.L': 'lip.B'}

        org_to_ctrls = {key: [org(bone) for bone in org_to_ctrls[key]]
                        for key in org_to_ctrls.keys()}
        tweak_unique = {org(key): tweak_unique[key]
                        for key in tweak_unique.keys()}

        tweak_exceptions = []  # bones not used to create tweaks
        tweak_exceptions += [bone for bone in org_bones if 'forehead' in bone or 'temple' in bone]

        tweak_tail = ['brow.B.L.003', 'brow.B.R.003', 'nose.004', 'chin.001']
        tweak_tail += ['lip.T.L.001', 'lip.T.R.001', 'tongue.002']

        tweak_exceptions += ['lip.T.R', 'lip.B.R', 'ear.L.001', 'ear.R.001'] + list(tweak_unique.keys())
        tweak_exceptions += ['face', 'cheek.T.L', 'cheek.T.R', 'cheek.B.L', 'cheek.B.R']
        tweak_exceptions += ['ear.L', 'ear.R', 'eye.L', 'eye.R']

        tweak_exceptions += org_to_ctrls.keys()
        tweak_exceptions += org_to_ctrls['teeth']

        tweak_exceptions.pop(tweak_exceptions.index('tongue'))
        tweak_exceptions.pop(tweak_exceptions.index('jaw'))

        tweak_exceptions = [org(bone) for bone in tweak_exceptions]
        tweak_tail = [org(bone) for bone in tweak_tail]

        org_to_tweak = sorted([bone for bone in org_bones if bone not in tweak_exceptions])

        ctrls = self.create_ctrl(org_to_ctrls)
        tweaks = self.create_tweak(org_to_tweak, tweak_unique, tweak_tail)

        return {'ctrls': ctrls, 'tweaks': tweaks}, tweak_unique

    def create_mch(self, jaw_ctrl, tongue_ctrl):
        org_bones = self.org_bones
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Create eyes mch bones
        eyes = [bone for bone in org_bones if 'eye' in bone]

        mch_bones = {strip_org(eye): [] for eye in eyes}

        for eye in eyes:
            mch_name = make_mechanism_name(strip_org(eye))
            mch_name = copy_bone(self.obj, eye, mch_name)
            eb[mch_name].use_connect = False
            eb[mch_name].parent = None

            mch_bones[strip_org(eye)].append(mch_name)

            mch_name = copy_bone(self.obj, eye, mch_name)
            eb[mch_name].use_connect = False
            eb[mch_name].parent = None

            mch_bones[strip_org(eye)].append(mch_name)

            eb[mch_name].head[:] = eb[mch_name].tail
            eb[mch_name].tail[:] = eb[mch_name].head + Vector((0, 0, 0.005))

        # Create the eyes' parent mch
        face = [bone for bone in org_bones if 'face' in bone].pop()

        mch_name = 'eyes_parent'
        mch_name = make_mechanism_name(mch_name)
        mch_name = copy_bone(self.obj, face, mch_name)
        eb[mch_name].use_connect = False
        eb[mch_name].parent = None

        eb[mch_name].length /= 4

        mch_bones['eyes_parent'] = [mch_name]

        # Create the lids' mch bones
        all_lids = [bone for bone in org_bones if 'lid' in bone]
        lids_L, lids_R = self.symmetrical_split(all_lids)

        all_lids = [lids_L, lids_R]

        mch_bones['lids'] = []

        for i in range(2):
            for bone in all_lids[i]:
                mch_name = make_mechanism_name(strip_org(bone))
                mch_name = copy_bone(self.obj, eyes[i], mch_name)

                eb[mch_name].use_connect = False
                eb[mch_name].parent = None

                eb[mch_name].tail[:] = eb[bone].head

                mch_bones['lids'].append(mch_name)

        mch_bones['jaw'] = []

        length_subtractor = eb[jaw_ctrl].length / 6
        # Create the jaw mch bones
        for i in range(6):
            if i == 0:
                mch_name = make_mechanism_name('mouth_lock')
            else:
                mch_name = make_mechanism_name(jaw_ctrl)

            mch_name = copy_bone(self.obj, jaw_ctrl, mch_name)

            eb[mch_name].use_connect = False
            eb[mch_name].parent = None

            eb[mch_name].length = eb[jaw_ctrl].length - length_subtractor * i

            mch_bones['jaw'].append(mch_name)

        # Tongue mch bones

        mch_bones['tongue'] = []

        # create mch bones for all tongue org_bones except the first one
        for bone in sorted([org_name for org_name in org_bones if 'tongue' in org_name])[1:]:
            mch_name = make_mechanism_name(strip_org(bone))
            mch_name = copy_bone(self.obj, tongue_ctrl, mch_name)

            eb[mch_name].use_connect = False
            eb[mch_name].parent = None

            mch_bones['tongue'].append(mch_name)

        return mch_bones

    def parent_bones(self, all_bones, tweak_unique):
        org_bones = self.org_bones
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        face_name = [bone for bone in org_bones if 'face' in bone].pop()

        # Initially parenting all bones to the face org bone.
        for category in list(all_bones.keys()):
            for area in list(all_bones[category]):
                for bone in all_bones[category][area]:
                    eb[bone].parent = eb[face_name]

        # Parenting all deformation bones and org bones

        # Parent all the deformation bones that have respective tweaks
        def_tweaks = [bone for bone in all_bones['deform']['all']
                      if bone[4:] in all_bones['tweaks']['all']]

        # Parent all org bones to the ORG-face
        for bone in [bone for bone in org_bones if 'face' not in bone]:
            eb[bone].parent = eb[org('face')]

        for bone in def_tweaks:
            # the def and the matching org bone are parented to their corresponding tweak,
            # whose name is the same as that of the def bone, without the "DEF-" (first 4 chars)
            eb[bone].parent = eb[bone[4:]]
            eb[org(bone[4:])].parent = eb[bone[4:]]

        # Parent ORG eyes to corresponding mch bones
        for bone in [bone for bone in org_bones if 'eye' in bone]:
            eb[bone].parent = eb[make_mechanism_name(strip_org(bone))]

        for lip_tweak in list(tweak_unique.values()):
            # find the def bones that match unique lip_tweaks by slicing [4:-2]
            # example: 'lip.B' matches 'DEF-lip.B.R' and 'DEF-lip.B.L' if
            # you cut off the "DEF-" [4:] and the ".L" or ".R" [:-2]
            lip_defs = [bone for bone in all_bones['deform']['all'] if bone[4:-2] == lip_tweak]

            for bone in lip_defs:
                eb[bone].parent = eb[lip_tweak]

        # parent cheek bones top respective tweaks
        lips = ['lips.L',   'lips.R']
        brows = ['brow.T.L', 'brow.T.R']
        cheekB_defs = ['DEF-cheek.B.L', 'DEF-cheek.B.R']
        cheekT_defs = ['DEF-cheek.T.L', 'DEF-cheek.T.R']

        for lip, brow, cheekB, cheekT in zip(lips, brows, cheekB_defs, cheekT_defs):
            eb[cheekB].parent = eb[lip]
            eb[cheekT].parent = eb[brow]

        # parent ear deform bones to their controls
        ear_defs = ['DEF-ear.L', 'DEF-ear.L.001', 'DEF-ear.R', 'DEF-ear.R.001']
        ear_ctrls = ['ear.L', 'ear.R']

        eb['DEF-jaw'].parent = eb['jaw']  # Parent jaw def bone to jaw tweak

        for ear_ctrl in ear_ctrls:
            for ear_def in ear_defs:
                if ear_ctrl in ear_def:
                    eb[ear_def].parent = eb[ear_ctrl]

        # Parent eyelid deform bones (each lid def bone is parented to its respective MCH bone)
        def_lids = [bone for bone in all_bones['deform']['all'] if 'lid' in bone]

        for bone in def_lids:
            mch = make_mechanism_name(bone[4:])
            eb[bone].parent = eb[mch]

        # Parenting all mch bones

        eb['MCH-eyes_parent'].parent = None  # eyes_parent will be parented to root

        # parent all mch tongue bones to the jaw master control bone
        for bone in all_bones['mch']['tongue']:
            eb[bone].parent = eb[all_bones['ctrls']['jaw'][0]]

        # Parenting the control bones

        # parent teeth.B and tongue master controls to the jaw master control bone
        for bone in ['teeth.B', 'tongue_master']:
            eb[bone].parent = eb[all_bones['ctrls']['jaw'][0]]

        # eyes
        eb['eyes'].parent = eb['MCH-eyes_parent']

        eyes = [
            bone for bone in all_bones['ctrls']['eyes'] if 'eyes' not in bone
        ][0:2]

        for eye in eyes:
            eb[eye].parent = eb['eyes']

        # turbo: parent eye master bones to face
        for eye_master in eyes[2:]:
            eb[eye_master].parent = eb['ORG-face']

        # Parent brow.b, eyes mch and lid tweaks and mch bones to masters
        tweaks = [
            b for b in all_bones['tweaks']['all'] if 'lid' in b or 'brow.B' in b
        ]
        mch = (all_bones['mch']['lids'] +
               all_bones['mch']['eye.R'] +
               all_bones['mch']['eye.L'])

        everyone = tweaks + mch

        left, right = self.symmetrical_split(everyone)

        for l_name in left:
            eb[l_name].parent = eb['master_eye.L']

        for r_name in right:
            eb[r_name].parent = eb['master_eye.R']

        # turbo: nose to mch jaw.004
        eb[all_bones['ctrls']['nose'].pop()].parent = eb['MCH-jaw_master.004']

        # Parenting the tweak bones

        # Jaw children (values) groups and their parents (keys)
        groups = {
            'jaw_master': [
                'jaw',
                'jaw.R.001',
                'jaw.L.001',
                'chin.L',
                'chin.R',
                'chin',
                'tongue.003'
                ],
            'MCH-jaw_master': [
                 'lip.B'
                ],
            'MCH-jaw_master.001': [
                'lip.B.L.001',
                'lip.B.R.001'
                ],
            'MCH-jaw_master.002': [
                'lips.L',
                'lips.R',
                'cheek.B.L.001',
                'cheek.B.R.001'
                ],
            'MCH-jaw_master.003': [
                'lip.T',
                'lip.T.L.001',
                'lip.T.R.001'
                ],
            'MCH-jaw_master.004': [
                'cheek.T.L.001',
                'cheek.T.R.001'
                ],
            'nose_master': [
                'nose.002',
                'nose.004',
                'nose.L.001',
                'nose.R.001'
                ]
             }

        for parent in list(groups.keys()):
            for bone in groups[parent]:
                eb[bone].parent = eb[parent]

        # Remaining arbitrary relationships for tweak bone parenting
        eb['chin.001'].parent = eb['chin']
        eb['chin.002'].parent = eb['lip.B']
        eb['nose.001'].parent = eb['nose.002']
        eb['nose.003'].parent = eb['nose.002']
        eb['nose.005'].parent = eb['lip.T']
        eb['tongue'].parent = eb['tongue_master']
        eb['tongue.001'].parent = eb['MCH-tongue.001']
        eb['tongue.002'].parent = eb['MCH-tongue.002']

        for bone in ['ear.L.002', 'ear.L.003', 'ear.L.004']:
            eb[bone].parent = eb['ear.L']
            eb[bone.replace('.L', '.R')].parent = eb['ear.R']

    def make_constraints(self, constraint_type: str, bone: str, subtarget: str, influence: float = 1):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        owner_pb = pb[bone]

        if constraint_type == 'def_tweak':

            const = owner_pb.constraints.new('STRETCH_TO')
            const.target = self.obj
            const.subtarget = subtarget
            const.keep_axis = 'SWING_Y'

        elif constraint_type == 'def_lids':

            const = owner_pb.constraints.new('STRETCH_TO')
            const.target = self.obj
            const.subtarget = subtarget
            const.head_tail = 1.0
            const.keep_axis = 'SWING_Y'

        elif constraint_type == 'mch_eyes':

            const = owner_pb.constraints.new('DAMPED_TRACK')
            const.target = self.obj
            const.subtarget = subtarget

        elif constraint_type == 'mch_eyes_lids_follow':

            const = owner_pb.constraints.new('COPY_LOCATION')
            const.target = self.obj
            const.subtarget = subtarget
            const.head_tail = 1.0

        elif constraint_type == 'mch_eyes_parent':

            const = owner_pb.constraints.new('COPY_TRANSFORMS')
            const.target = self.obj
            const.subtarget = subtarget

        elif constraint_type == 'mch_jaw_master':

            const = owner_pb.constraints.new('COPY_TRANSFORMS')
            const.target = self.obj
            const.subtarget = subtarget
            const.influence = influence

        elif constraint_type == 'teeth':

            const = owner_pb.constraints.new('COPY_TRANSFORMS')
            const.target = self.obj
            const.subtarget = subtarget
            const.influence = influence

        elif constraint_type == 'tweak_copy_loc':

            const = owner_pb.constraints.new('COPY_LOCATION')
            const.target = self.obj
            const.subtarget = subtarget
            const.influence = influence
            const.use_offset = True
            const.target_space = 'LOCAL'
            const.owner_space = 'LOCAL'

        elif constraint_type == 'tweak_copy_rot_scl':

            const = owner_pb.constraints.new('COPY_ROTATION')
            const.target = self.obj
            const.subtarget = subtarget
            const.mix_mode = 'OFFSET'
            const.target_space = 'LOCAL'
            const.owner_space = 'LOCAL'

            const = owner_pb.constraints.new('COPY_SCALE')
            const.target = self.obj
            const.subtarget = subtarget
            const.use_offset = True
            const.target_space = 'LOCAL'
            const.owner_space = 'LOCAL'

        elif constraint_type == 'tweak_copy_loc_inv':

            const = owner_pb.constraints.new('COPY_LOCATION')
            const.target = self.obj
            const.subtarget = subtarget
            const.influence = influence
            const.target_space = 'LOCAL'
            const.owner_space = 'LOCAL'
            const.use_offset = True
            const.invert_x = True
            const.invert_y = True
            const.invert_z = True

        elif constraint_type == 'mch_tongue_copy_trans':

            const = owner_pb.constraints.new('COPY_TRANSFORMS')
            const.target = self.obj
            const.subtarget = subtarget
            const.influence = influence

    def constraints(self, all_bones):
        # Def bone constraints

        def_specials = {
            # 'bone'             : 'target'
            'DEF-jaw'               : 'chin',                 # noqa: E203
            'DEF-chin.L'            : 'lips.L',               # noqa: E203
            'DEF-jaw.L.001'         : 'chin.L',               # noqa: E203
            'DEF-chin.R'            : 'lips.R',               # noqa: E203
            'DEF-jaw.R.001'         : 'chin.R',               # noqa: E203
            'DEF-brow.T.L.003'      : 'nose',                 # noqa: E203
            'DEF-ear.L'             : None,                   # noqa: E203
            'DEF-ear.L.003'         : 'ear.L.004',            # noqa: E203
            'DEF-ear.L.004'         : 'ear.L',                # noqa: E203
            'DEF-ear.R'             : None,                   # noqa: E203
            'DEF-ear.R.003'         : 'ear.R.004',            # noqa: E203
            'DEF-ear.R.004'         : 'ear.R',                # noqa: E203
            'DEF-lip.B.L.001'       : 'lips.L',               # noqa: E203
            'DEF-lip.B.R.001'       : 'lips.R',               # noqa: E203
            'DEF-cheek.B.L.001'     : 'brow.T.L',             # noqa: E203
            'DEF-cheek.B.R.001'     : 'brow.T.R',             # noqa: E203
            'DEF-lip.T.L.001'       : 'lips.L',               # noqa: E203
            'DEF-lip.T.R.001'       : 'lips.R',               # noqa: E203
            'DEF-cheek.T.L.001'     : 'nose.L',               # noqa: E203
            'DEF-nose.L.001'        : 'nose.002',             # noqa: E203
            'DEF-cheek.T.R.001'     : 'nose.R',               # noqa: E203
            'DEF-nose.R.001'        : 'nose.002',             # noqa: E203
            'DEF-forehead.L'        : 'brow.T.L.003',         # noqa: E203
            'DEF-forehead.L.001'    : 'brow.T.L.002',         # noqa: E203
            'DEF-forehead.L.002'    : 'brow.T.L.001',         # noqa: E203
            'DEF-temple.L'          : 'jaw.L',                # noqa: E203
            'DEF-brow.T.R.003'      : 'nose',                 # noqa: E203
            'DEF-forehead.R'        : 'brow.T.R.003',         # noqa: E203
            'DEF-forehead.R.001'    : 'brow.T.R.002',         # noqa: E203
            'DEF-forehead.R.002'    : 'brow.T.R.001',         # noqa: E203
            'DEF-temple.R'          : 'jaw.R'                 # noqa: E203
        }

        pattern = r'^DEF-(\w+\.?\w?\.?\w?)(\.?)(\d*?)(\d?)$'

        for bone in [bone for bone in all_bones['deform']['all'] if 'lid' not in bone]:
            if bone in def_specials:
                if def_specials[bone] is not None:
                    self.make_constraints('def_tweak', bone, def_specials[bone])
            else:
                matches = re.match(pattern, bone).groups()
                if len(matches) > 1 and matches[-1]:
                    num = int(matches[-1]) + 1
                    str_list = list(matches)[:-1] + [str(num)]
                    tweak = "".join(str_list)
                else:
                    tweak = "".join(matches) + ".001"
                self.make_constraints('def_tweak', bone, tweak)

        def_lids = sorted([bone for bone in all_bones['deform']['all'] if 'lid' in bone])
        mch_lids = sorted([bone for bone in all_bones['mch']['lids']])

        def_lidsL, def_lidsR = self.symmetrical_split(def_lids)
        mch_lidsL, mch_lidsR = self.symmetrical_split(mch_lids)

        # Take the last mch_lid bone and place it at the end
        mch_lidsL = mch_lidsL[1:] + [mch_lidsL[0]]
        mch_lidsR = mch_lidsR[1:] + [mch_lidsR[0]]

        for boneL, boneR, mchL, mchR in zip(def_lidsL, def_lidsR, mch_lidsL, mch_lidsR):
            self.make_constraints('def_lids', boneL, mchL)
            self.make_constraints('def_lids', boneR, mchR)

        # MCH constraints

        # mch lids constraints
        for bone in all_bones['mch']['lids']:
            tweak = bone[4:]  # remove "MCH-" from bone name
            self.make_constraints('mch_eyes', bone, tweak)

        # mch eyes constraints
        for bone in ['MCH-eye.L', 'MCH-eye.R']:
            ctrl = bone[4:]  # remove "MCH-" from bone name
            self.make_constraints('mch_eyes', bone, ctrl)

        for bone in ['MCH-eye.L.001', 'MCH-eye.R.001']:
            target = bone[:-4]  # remove number from the end of the name
            self.make_constraints('mch_eyes_lids_follow', bone, target)

        # mch eyes parent constraints
        self.make_constraints('mch_eyes_parent', 'MCH-eyes_parent', 'ORG-face')

        # Jaw constraints

        # jaw master mch bones
        self.make_constraints('mch_jaw_master', 'MCH-mouth_lock', 'jaw_master', 0.20)
        self.make_constraints('mch_jaw_master', 'MCH-jaw_master', 'jaw_master', 1.00)
        self.make_constraints('mch_jaw_master', 'MCH-jaw_master.001', 'jaw_master', 0.75)
        self.make_constraints('mch_jaw_master', 'MCH-jaw_master.002', 'jaw_master', 0.35)
        self.make_constraints('mch_jaw_master', 'MCH-jaw_master.003', 'jaw_master', 0.10)
        self.make_constraints('mch_jaw_master', 'MCH-jaw_master.004', 'jaw_master', 0.025)

        self.make_constraints('teeth', 'ORG-teeth.T', 'teeth.T', 1.00)
        self.make_constraints('teeth', 'ORG-teeth.B', 'teeth.B', 1.00)

        for bone in all_bones['mch']['jaw'][1:-1]:
            self.make_constraints('mch_jaw_master', bone, 'MCH-mouth_lock')

        # Tweak bones constraints

        # copy location constraints for tweak bones of both sides
        tweak_copy_loc_L = {
            'brow.T.L.002'  : [['brow.T.L.001', 'brow.T.L.003'   ], [0.5, 0.5  ]],  # noqa: E202,E203
            'ear.L.003'     : [['ear.L.004', 'ear.L.002'         ], [0.5, 0.5  ]],  # noqa: E202,E203
            'brow.B.L.001'  : [['brow.B.L.002'                   ], [0.6       ]],  # noqa: E202,E203
            'brow.B.L.003'  : [['brow.B.L.002'                   ], [0.6       ]],  # noqa: E202,E203
          # 'brow.B.L.002'  : [['lid.T.L.001',                   ], [0.25      ]],  # noqa: E202,E203
            'brow.B.L.002'  : [['brow.T.L.002',                  ], [0.25      ]],  # noqa: E202,E203
            'lid.T.L.001'   : [['lid.T.L.002'                    ], [0.6       ]],  # noqa: E202,E203
            'lid.T.L.003'   : [['lid.T.L.002',                   ], [0.6       ]],  # noqa: E202,E203
            'lid.T.L.002'   : [['MCH-eye.L.001',                 ], [0.5       ]],  # noqa: E202,E203
            'lid.B.L.001'   : [['lid.B.L.002',                   ], [0.6       ]],  # noqa: E202,E203
            'lid.B.L.003'   : [['lid.B.L.002',                   ], [0.6       ]],  # noqa: E202,E203
            'lid.B.L.002'   : [['MCH-eye.L.001', 'cheek.T.L.001' ], [0.5, 0.1  ]],  # noqa: E202,E203
            'cheek.T.L.001' : [['cheek.B.L.001',                 ], [0.5       ]],  # noqa: E202,E203
            'nose.L'        : [['nose.L.001',                    ], [0.25      ]],  # noqa: E202,E203
            'nose.L.001'    : [['lip.T.L.001',                   ], [0.2       ]],  # noqa: E202,E203
            'cheek.B.L.001' : [['lips.L',                        ], [0.5       ]],  # noqa: E202,E203
            'lip.T.L.001'   : [['lips.L', 'lip.T'                ], [0.25, 0.5 ]],  # noqa: E202,E203
            'lip.B.L.001'   : [['lips.L', 'lip.B'                ], [0.25, 0.5 ]]   # noqa: E202,E203
            }

        for owner in list(tweak_copy_loc_L.keys()):

            targets, influences = tweak_copy_loc_L[owner]
            for target, influence in zip(targets, influences):

                # Left side constraints
                self.make_constraints('tweak_copy_loc', owner, target, influence)

                # create constraints for the right side too
                ownerR = owner.replace('.L', '.R')
                targetR = target.replace('.L', '.R')
                self.make_constraints('tweak_copy_loc', ownerR, targetR, influence)

        # copy rotation & scale constraints for tweak bones of both sides
        tweak_copy_rot_scl_L = {
            'lip.T.L.001': 'lip.T',
            'lip.B.L.001': 'lip.B'
        }

        for owner in list(tweak_copy_rot_scl_L.keys()):
            target = tweak_copy_rot_scl_L[owner]
            self.make_constraints('tweak_copy_rot_scl', owner, target)

            # create constraints for the right side too
            owner = owner.replace('.L', '.R')
            self.make_constraints('tweak_copy_rot_scl', owner, target)

        # inverted tweak bones constraints
        tweak_nose = {
            'nose.001': ['nose.002', 0.35],  # noqa: E202
            'nose.003': ['nose.002', 0.5 ],  # noqa: E202
            'nose.005': ['lip.T',    0.5 ],  # noqa: E202
            'chin.002': ['lip.B',    0.5 ]   # noqa: E202
        }

        for owner in list(tweak_nose.keys()):
            target = tweak_nose[owner][0]
            influence = tweak_nose[owner][1]
            self.make_constraints('tweak_copy_loc_inv', owner, target, influence)

        # MCH tongue constraints
        divider = len(all_bones['mch']['tongue']) + 1
        factor = len(all_bones['mch']['tongue'])

        for owner in all_bones['mch']['tongue']:
            self.make_constraints('mch_tongue_copy_trans', owner, 'tongue_master', (1 / divider) * factor)
            factor -= 1

    def drivers_and_props(self, all_bones):

        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        jaw_ctrl = all_bones['ctrls']['jaw'][0]
        eyes_ctrl = all_bones['ctrls']['eyes'][2]

        jaw_prop = 'mouth_lock'
        eyes_prop = 'eyes_follow'

        for bone, prop_name in zip([jaw_ctrl, eyes_ctrl], [jaw_prop, eyes_prop]):
            if bone == jaw_ctrl:
                def_val = 0.0
            else:
                def_val = 1.0

            make_property(pb[bone], prop_name, def_val)

        # Jaw drivers
        mch_jaws = all_bones['mch']['jaw'][1:-1]

        for bone in mch_jaws:
            drv = pb[bone].constraints[1].driver_add("influence").driver
            drv.type = 'SUM'

            var = drv.variables.new()
            var.name = jaw_prop
            var.type = "SINGLE_PROP"
            var.targets[0].id = self.obj
            var.targets[0].data_path = pb[jaw_ctrl].path_from_id() + '[' + '"' + jaw_prop + '"' + ']'

        # Eyes driver
        mch_eyes_parent = all_bones['mch']['eyes_parent'][0]

        drv = pb[mch_eyes_parent].constraints[0].driver_add("influence").driver
        drv.type = 'SUM'

        var = drv.variables.new()
        var.name = eyes_prop
        var.type = "SINGLE_PROP"
        var.targets[0].id = self.obj
        var.targets[0].data_path = pb[eyes_ctrl].path_from_id() + '[' + '"' + eyes_prop + '"' + ']'

        return jaw_prop, eyes_prop

    def create_bones(self):
        org_bones = self.org_bones
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        # Clear parents for org bones
        for bone in [bone for bone in org_bones if 'face' not in bone]:
            eb[bone].use_connect = False
            eb[bone].parent = None

        def_names = self.create_deformation()
        ctrls, tweak_unique = self.all_controls()
        mch = self.create_mch(
            ctrls['ctrls']['jaw'][0],
            ctrls['ctrls']['tongue'][0]
        )

        return {
            'deform': def_names,
            'ctrls': ctrls['ctrls'],
            'tweaks': ctrls['tweaks'],
            'mch': mch
            }, tweak_unique

    def generate(self):

        self.orient_org_bones()
        all_bones, tweak_unique = self.create_bones()
        self.parent_bones(all_bones, tweak_unique)
        self.constraints(all_bones)
        jaw_prop, eyes_prop = self.drivers_and_props(all_bones)

        # Create UI
        all_controls = []
        all_controls += [
            bone for bone in [
                bone_group for bone_group in [
                    all_bones['ctrls'][group]
                    for group in list(all_bones['ctrls'].keys())
                ]
            ]
        ]
        all_controls += [
            bone for bone in [
                bone_group for bone_group in [
                    all_bones['tweaks'][group]
                    for group in list(all_bones['tweaks'].keys())
                ]
            ]
        ]

        all_ctrls = []
        for group in all_controls:
            for bone in group:
                all_ctrls.append(bone)

        controls_string = ", ".join(["'" + x + "'" for x in all_ctrls])
        return [script % (
            controls_string,
            all_bones['ctrls']['jaw'][0],
            all_bones['ctrls']['eyes'][2],
            jaw_prop,
            eyes_prop)
            ]


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """

    # Setting up extra layers for the tweak bones
    ControlLayersOption.FACE_PRIMARY.add_parameters(params)
    ControlLayersOption.FACE_SECONDARY.add_parameters(params)


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    layout.label(text='This monolithic face rig is deprecated.', icon='INFO')
    layout.operator("pose.rigify_upgrade_face")
    layout.separator()

    ControlLayersOption.FACE_PRIMARY.parameters_ui(layout, params)
    ControlLayersOption.FACE_SECONDARY.parameters_ui(layout, params)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('face')
    bone.head[:] = -0.0000, -0.0013, 0.0437
    bone.tail[:] = -0.0000, -0.0013, 0.1048
    bone.roll = 0.0000
    bone.use_connect = False
    bones['face'] = bone.name
    bone = arm.edit_bones.new('nose')
    bone.head[:] = 0.0000, -0.0905, 0.1125
    bone.tail[:] = 0.0000, -0.1105, 0.0864
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['nose'] = bone.name
    bone = arm.edit_bones.new('lip.T.L')
    bone.head[:] = 0.0000, -0.1022, 0.0563
    bone.tail[:] = 0.0131, -0.0986, 0.0567
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['lip.T.L'] = bone.name
    bone = arm.edit_bones.new('lip.B.L')
    bone.head[:] = 0.0000, -0.0993, 0.0455
    bone.tail[:] = 0.0124, -0.0938, 0.0488
    bone.roll = -0.0789
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['lip.B.L'] = bone.name
    bone = arm.edit_bones.new('jaw')
    bone.head[:] = 0.0000, -0.0389, 0.0222
    bone.tail[:] = 0.0000, -0.0923, 0.0044
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['jaw'] = bone.name
    bone = arm.edit_bones.new('ear.L')
    bone.head[:] = 0.0616, -0.0083, 0.0886
    bone.tail[:] = 0.0663, -0.0101, 0.1151
    bone.roll = -0.0324
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['ear.L'] = bone.name
    bone = arm.edit_bones.new('ear.R')
    bone.head[:] = -0.0616, -0.0083, 0.0886
    bone.tail[:] = -0.0663, -0.0101, 0.1151
    bone.roll = 0.0324
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['ear.R'] = bone.name
    bone = arm.edit_bones.new('lip.T.R')
    bone.head[:] = -0.0000, -0.1022, 0.0563
    bone.tail[:] = -0.0131, -0.0986, 0.0567
    bone.roll = -0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['lip.T.R'] = bone.name
    bone = arm.edit_bones.new('lip.B.R')
    bone.head[:] = -0.0000, -0.0993, 0.0455
    bone.tail[:] = -0.0124, -0.0938, 0.0488
    bone.roll = 0.0789
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['lip.B.R'] = bone.name
    bone = arm.edit_bones.new('brow.B.L')
    bone.head[:] = 0.0530, -0.0705, 0.1153
    bone.tail[:] = 0.0472, -0.0780, 0.1192
    bone.roll = 0.0412
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['brow.B.L'] = bone.name
    bone = arm.edit_bones.new('lid.T.L')
    bone.head[:] = 0.0515, -0.0692, 0.1104
    bone.tail[:] = 0.0474, -0.0785, 0.1136
    bone.roll = 0.1166
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['lid.T.L'] = bone.name
    bone = arm.edit_bones.new('brow.B.R')
    bone.head[:] = -0.0530, -0.0705, 0.1153
    bone.tail[:] = -0.0472, -0.0780, 0.1192
    bone.roll = -0.0412
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['brow.B.R'] = bone.name
    bone = arm.edit_bones.new('lid.T.R')
    bone.head[:] = -0.0515, -0.0692, 0.1104
    bone.tail[:] = -0.0474, -0.0785, 0.1136
    bone.roll = -0.1166
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['lid.T.R'] = bone.name
    bone = arm.edit_bones.new('forehead.L')
    bone.head[:] = 0.0113, -0.0764, 0.1611
    bone.tail[:] = 0.0144, -0.0912, 0.1236
    bone.roll = 1.4313
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['forehead.L'] = bone.name
    bone = arm.edit_bones.new('forehead.R')
    bone.head[:] = -0.0113, -0.0764, 0.1611
    bone.tail[:] = -0.0144, -0.0912, 0.1236
    bone.roll = -1.4313
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['forehead.R'] = bone.name
    bone = arm.edit_bones.new('eye.L')
    bone.head[:] = 0.0360, -0.0686, 0.1107
    bone.tail[:] = 0.0360, -0.0848, 0.1107
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['eye.L'] = bone.name
    bone = arm.edit_bones.new('eye.R')
    bone.head[:] = -0.0360, -0.0686, 0.1107
    bone.tail[:] = -0.0360, -0.0848, 0.1107
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['eye.R'] = bone.name
    bone = arm.edit_bones.new('cheek.T.L')
    bone.head[:] = 0.0568, -0.0506, 0.1052
    bone.tail[:] = 0.0379, -0.0834, 0.0816
    bone.roll = -0.0096
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['cheek.T.L'] = bone.name
    bone = arm.edit_bones.new('cheek.T.R')
    bone.head[:] = -0.0568, -0.0506, 0.1052
    bone.tail[:] = -0.0379, -0.0834, 0.0816
    bone.roll = 0.0096
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['cheek.T.R'] = bone.name
    bone = arm.edit_bones.new('teeth.T')
    bone.head[:] = 0.0000, -0.0927, 0.0613
    bone.tail[:] = 0.0000, -0.0621, 0.0613
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['teeth.T'] = bone.name
    bone = arm.edit_bones.new('teeth.B')
    bone.head[:] = 0.0000, -0.0881, 0.0397
    bone.tail[:] = 0.0000, -0.0575, 0.0397
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['teeth.B'] = bone.name
    bone = arm.edit_bones.new('tongue')
    bone.head[:] = 0.0000, -0.0781, 0.0493
    bone.tail[:] = 0.0000, -0.0620, 0.0567
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['face']]
    bones['tongue'] = bone.name
    bone = arm.edit_bones.new('nose.001')
    bone.head[:] = 0.0000, -0.1105, 0.0864
    bone.tail[:] = 0.0000, -0.1193, 0.0771
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['nose']]
    bones['nose.001'] = bone.name
    bone = arm.edit_bones.new('lip.T.L.001')
    bone.head[:] = 0.0131, -0.0986, 0.0567
    bone.tail[:] = 0.0236, -0.0877, 0.0519
    bone.roll = 0.0236
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.T.L']]
    bones['lip.T.L.001'] = bone.name
    bone = arm.edit_bones.new('lip.B.L.001')
    bone.head[:] = 0.0124, -0.0938, 0.0488
    bone.tail[:] = 0.0236, -0.0877, 0.0519
    bone.roll = 0.0731
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.B.L']]
    bones['lip.B.L.001'] = bone.name
    bone = arm.edit_bones.new('chin')
    bone.head[:] = 0.0000, -0.0923, 0.0044
    bone.tail[:] = 0.0000, -0.0921, 0.0158
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['jaw']]
    bones['chin'] = bone.name
    bone = arm.edit_bones.new('ear.L.001')
    bone.head[:] = 0.0663, -0.0101, 0.1151
    bone.tail[:] = 0.0804, 0.0065, 0.1189
    bone.roll = 0.0656
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.L']]
    bones['ear.L.001'] = bone.name
    bone = arm.edit_bones.new('ear.R.001')
    bone.head[:] = -0.0663, -0.0101, 0.1151
    bone.tail[:] = -0.0804, 0.0065, 0.1189
    bone.roll = -0.0656
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.R']]
    bones['ear.R.001'] = bone.name
    bone = arm.edit_bones.new('lip.T.R.001')
    bone.head[:] = -0.0131, -0.0986, 0.0567
    bone.tail[:] = -0.0236, -0.0877, 0.0519
    bone.roll = -0.0236
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.T.R']]
    bones['lip.T.R.001'] = bone.name
    bone = arm.edit_bones.new('lip.B.R.001')
    bone.head[:] = -0.0124, -0.0938, 0.0488
    bone.tail[:] = -0.0236, -0.0877, 0.0519
    bone.roll = -0.0731
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.B.R']]
    bones['lip.B.R.001'] = bone.name
    bone = arm.edit_bones.new('brow.B.L.001')
    bone.head[:] = 0.0472, -0.0780, 0.1192
    bone.tail[:] = 0.0387, -0.0832, 0.1202
    bone.roll = 0.0192
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.B.L']]
    bones['brow.B.L.001'] = bone.name
    bone = arm.edit_bones.new('lid.T.L.001')
    bone.head[:] = 0.0474, -0.0785, 0.1136
    bone.tail[:] = 0.0394, -0.0838, 0.1147
    bone.roll = 0.0791
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.L']]
    bones['lid.T.L.001'] = bone.name
    bone = arm.edit_bones.new('brow.B.R.001')
    bone.head[:] = -0.0472, -0.0780, 0.1192
    bone.tail[:] = -0.0387, -0.0832, 0.1202
    bone.roll = -0.0192
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.B.R']]
    bones['brow.B.R.001'] = bone.name
    bone = arm.edit_bones.new('lid.T.R.001')
    bone.head[:] = -0.0474, -0.0785, 0.1136
    bone.tail[:] = -0.0394, -0.0838, 0.1147
    bone.roll = -0.0791
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.R']]
    bones['lid.T.R.001'] = bone.name
    bone = arm.edit_bones.new('forehead.L.001')
    bone.head[:] = 0.0321, -0.0663, 0.1646
    bone.tail[:] = 0.0394, -0.0828, 0.1310
    bone.roll = 0.9928
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['forehead.L']]
    bones['forehead.L.001'] = bone.name
    bone = arm.edit_bones.new('forehead.R.001')
    bone.head[:] = -0.0321, -0.0663, 0.1646
    bone.tail[:] = -0.0394, -0.0828, 0.1310
    bone.roll = -0.9928
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['forehead.R']]
    bones['forehead.R.001'] = bone.name
    bone = arm.edit_bones.new('cheek.T.L.001')
    bone.head[:] = 0.0379, -0.0834, 0.0816
    bone.tail[:] = 0.0093, -0.0846, 0.1002
    bone.roll = 0.0320
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.T.L']]
    bones['cheek.T.L.001'] = bone.name
    bone = arm.edit_bones.new('cheek.T.R.001')
    bone.head[:] = -0.0379, -0.0834, 0.0816
    bone.tail[:] = -0.0093, -0.0846, 0.1002
    bone.roll = -0.0320
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.T.R']]
    bones['cheek.T.R.001'] = bone.name
    bone = arm.edit_bones.new('tongue.001')
    bone.head[:] = 0.0000, -0.0620, 0.0567
    bone.tail[:] = 0.0000, -0.0406, 0.0584
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['tongue']]
    bones['tongue.001'] = bone.name
    bone = arm.edit_bones.new('nose.002')
    bone.head[:] = 0.0000, -0.1193, 0.0771
    bone.tail[:] = 0.0000, -0.1118, 0.0739
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['nose.001']]
    bones['nose.002'] = bone.name
    bone = arm.edit_bones.new('chin.001')
    bone.head[:] = 0.0000, -0.0921, 0.0158
    bone.tail[:] = 0.0000, -0.0914, 0.0404
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['chin']]
    bones['chin.001'] = bone.name
    bone = arm.edit_bones.new('ear.L.002')
    bone.head[:] = 0.0804, 0.0065, 0.1189
    bone.tail[:] = 0.0808, 0.0056, 0.0935
    bone.roll = -0.0265
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.L.001']]
    bones['ear.L.002'] = bone.name
    bone = arm.edit_bones.new('ear.R.002')
    bone.head[:] = -0.0804, 0.0065, 0.1189
    bone.tail[:] = -0.0808, 0.0056, 0.0935
    bone.roll = 0.0265
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.R.001']]
    bones['ear.R.002'] = bone.name
    bone = arm.edit_bones.new('brow.B.L.002')
    bone.head[:] = 0.0387, -0.0832, 0.1202
    bone.tail[:] = 0.0295, -0.0826, 0.1179
    bone.roll = -0.0278
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.B.L.001']]
    bones['brow.B.L.002'] = bone.name
    bone = arm.edit_bones.new('lid.T.L.002')
    bone.head[:] = 0.0394, -0.0838, 0.1147
    bone.tail[:] = 0.0317, -0.0832, 0.1131
    bone.roll = -0.0356
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.L.001']]
    bones['lid.T.L.002'] = bone.name
    bone = arm.edit_bones.new('brow.B.R.002')
    bone.head[:] = -0.0387, -0.0832, 0.1202
    bone.tail[:] = -0.0295, -0.0826, 0.1179
    bone.roll = 0.0278
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.B.R.001']]
    bones['brow.B.R.002'] = bone.name
    bone = arm.edit_bones.new('lid.T.R.002')
    bone.head[:] = -0.0394, -0.0838, 0.1147
    bone.tail[:] = -0.0317, -0.0832, 0.1131
    bone.roll = 0.0356
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.R.001']]
    bones['lid.T.R.002'] = bone.name
    bone = arm.edit_bones.new('forehead.L.002')
    bone.head[:] = 0.0482, -0.0506, 0.1620
    bone.tail[:] = 0.0556, -0.0689, 0.1249
    bone.roll = 0.4509
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['forehead.L.001']]
    bones['forehead.L.002'] = bone.name
    bone = arm.edit_bones.new('forehead.R.002')
    bone.head[:] = -0.0482, -0.0506, 0.1620
    bone.tail[:] = -0.0556, -0.0689, 0.1249
    bone.roll = -0.4509
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['forehead.R.001']]
    bones['forehead.R.002'] = bone.name
    bone = arm.edit_bones.new('nose.L')
    bone.head[:] = 0.0093, -0.0846, 0.1002
    bone.tail[:] = 0.0118, -0.0966, 0.0757
    bone.roll = -0.0909
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.T.L.001']]
    bones['nose.L'] = bone.name
    bone = arm.edit_bones.new('nose.R')
    bone.head[:] = -0.0093, -0.0846, 0.1002
    bone.tail[:] = -0.0118, -0.0966, 0.0757
    bone.roll = 0.0909
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.T.R.001']]
    bones['nose.R'] = bone.name
    bone = arm.edit_bones.new('tongue.002')
    bone.head[:] = 0.0000, -0.0406, 0.0584
    bone.tail[:] = 0.0000, -0.0178, 0.0464
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['tongue.001']]
    bones['tongue.002'] = bone.name
    bone = arm.edit_bones.new('nose.003')
    bone.head[:] = 0.0000, -0.1118, 0.0739
    bone.tail[:] = 0.0000, -0.1019, 0.0733
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['nose.002']]
    bones['nose.003'] = bone.name
    bone = arm.edit_bones.new('ear.L.003')
    bone.head[:] = 0.0808, 0.0056, 0.0935
    bone.tail[:] = 0.0677, -0.0109, 0.0752
    bone.roll = 0.3033
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.L.002']]
    bones['ear.L.003'] = bone.name
    bone = arm.edit_bones.new('ear.R.003')
    bone.head[:] = -0.0808, 0.0056, 0.0935
    bone.tail[:] = -0.0677, -0.0109, 0.0752
    bone.roll = -0.3033
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.R.002']]
    bones['ear.R.003'] = bone.name
    bone = arm.edit_bones.new('brow.B.L.003')
    bone.head[:] = 0.0295, -0.0826, 0.1179
    bone.tail[:] = 0.0201, -0.0812, 0.1095
    bone.roll = 0.0417
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.B.L.002']]
    bones['brow.B.L.003'] = bone.name
    bone = arm.edit_bones.new('lid.T.L.003')
    bone.head[:] = 0.0317, -0.0832, 0.1131
    bone.tail[:] = 0.0237, -0.0826, 0.1058
    bone.roll = 0.0245
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.L.002']]
    bones['lid.T.L.003'] = bone.name
    bone = arm.edit_bones.new('brow.B.R.003')
    bone.head[:] = -0.0295, -0.0826, 0.1179
    bone.tail[:] = -0.0201, -0.0812, 0.1095
    bone.roll = -0.0417
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.B.R.002']]
    bones['brow.B.R.003'] = bone.name
    bone = arm.edit_bones.new('lid.T.R.003')
    bone.head[:] = -0.0317, -0.0832, 0.1131
    bone.tail[:] = -0.0237, -0.0826, 0.1058
    bone.roll = -0.0245
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.R.002']]
    bones['lid.T.R.003'] = bone.name
    bone = arm.edit_bones.new('temple.L')
    bone.head[:] = 0.0585, -0.0276, 0.1490
    bone.tail[:] = 0.0607, -0.0295, 0.0962
    bone.roll = -0.0650
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['forehead.L.002']]
    bones['temple.L'] = bone.name
    bone = arm.edit_bones.new('temple.R')
    bone.head[:] = -0.0585, -0.0276, 0.1490
    bone.tail[:] = -0.0607, -0.0295, 0.0962
    bone.roll = 0.0650
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['forehead.R.002']]
    bones['temple.R'] = bone.name
    bone = arm.edit_bones.new('nose.L.001')
    bone.head[:] = 0.0118, -0.0966, 0.0757
    bone.tail[:] = 0.0000, -0.1193, 0.0771
    bone.roll = 0.1070
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['nose.L']]
    bones['nose.L.001'] = bone.name
    bone = arm.edit_bones.new('nose.R.001')
    bone.head[:] = -0.0118, -0.0966, 0.0757
    bone.tail[:] = -0.0000, -0.1193, 0.0771
    bone.roll = -0.1070
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['nose.R']]
    bones['nose.R.001'] = bone.name
    bone = arm.edit_bones.new('nose.004')
    bone.head[:] = 0.0000, -0.1019, 0.0733
    bone.tail[:] = 0.0000, -0.1014, 0.0633
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['nose.003']]
    bones['nose.004'] = bone.name
    bone = arm.edit_bones.new('ear.L.004')
    bone.head[:] = 0.0677, -0.0109, 0.0752
    bone.tail[:] = 0.0616, -0.0083, 0.0886
    bone.roll = 0.1518
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.L.003']]
    bones['ear.L.004'] = bone.name
    bone = arm.edit_bones.new('ear.R.004')
    bone.head[:] = -0.0677, -0.0109, 0.0752
    bone.tail[:] = -0.0616, -0.0083, 0.0886
    bone.roll = -0.1518
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['ear.R.003']]
    bones['ear.R.004'] = bone.name
    bone = arm.edit_bones.new('lid.B.L')
    bone.head[:] = 0.0237, -0.0826, 0.1058
    bone.tail[:] = 0.0319, -0.0831, 0.1050
    bone.roll = -0.1108
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.L.003']]
    bones['lid.B.L'] = bone.name
    bone = arm.edit_bones.new('lid.B.R')
    bone.head[:] = -0.0237, -0.0826, 0.1058
    bone.tail[:] = -0.0319, -0.0831, 0.1050
    bone.roll = 0.1108
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.T.R.003']]
    bones['lid.B.R'] = bone.name
    bone = arm.edit_bones.new('jaw.L')
    bone.head[:] = 0.0607, -0.0295, 0.0962
    bone.tail[:] = 0.0451, -0.0338, 0.0533
    bone.roll = 0.0871
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['temple.L']]
    bones['jaw.L'] = bone.name
    bone = arm.edit_bones.new('jaw.R')
    bone.head[:] = -0.0607, -0.0295, 0.0962
    bone.tail[:] = -0.0451, -0.0338, 0.0533
    bone.roll = -0.0871
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['temple.R']]
    bones['jaw.R'] = bone.name
    bone = arm.edit_bones.new('lid.B.L.001')
    bone.head[:] = 0.0319, -0.0831, 0.1050
    bone.tail[:] = 0.0389, -0.0826, 0.1050
    bone.roll = -0.0207
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.B.L']]
    bones['lid.B.L.001'] = bone.name
    bone = arm.edit_bones.new('lid.B.R.001')
    bone.head[:] = -0.0319, -0.0831, 0.1050
    bone.tail[:] = -0.0389, -0.0826, 0.1050
    bone.roll = 0.0207
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.B.R']]
    bones['lid.B.R.001'] = bone.name
    bone = arm.edit_bones.new('jaw.L.001')
    bone.head[:] = 0.0451, -0.0338, 0.0533
    bone.tail[:] = 0.0166, -0.0758, 0.0187
    bone.roll = 0.0458
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['jaw.L']]
    bones['jaw.L.001'] = bone.name
    bone = arm.edit_bones.new('jaw.R.001')
    bone.head[:] = -0.0451, -0.0338, 0.0533
    bone.tail[:] = -0.0166, -0.0758, 0.0187
    bone.roll = -0.0458
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['jaw.R']]
    bones['jaw.R.001'] = bone.name
    bone = arm.edit_bones.new('lid.B.L.002')
    bone.head[:] = 0.0389, -0.0826, 0.1050
    bone.tail[:] = 0.0472, -0.0781, 0.1068
    bone.roll = 0.0229
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.B.L.001']]
    bones['lid.B.L.002'] = bone.name
    bone = arm.edit_bones.new('lid.B.R.002')
    bone.head[:] = -0.0389, -0.0826, 0.1050
    bone.tail[:] = -0.0472, -0.0781, 0.1068
    bone.roll = -0.0229
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.B.R.001']]
    bones['lid.B.R.002'] = bone.name
    bone = arm.edit_bones.new('chin.L')
    bone.head[:] = 0.0166, -0.0758, 0.0187
    bone.tail[:] = 0.0236, -0.0877, 0.0519
    bone.roll = 0.1513
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['jaw.L.001']]
    bones['chin.L'] = bone.name
    bone = arm.edit_bones.new('chin.R')
    bone.head[:] = -0.0166, -0.0758, 0.0187
    bone.tail[:] = -0.0236, -0.0877, 0.0519
    bone.roll = -0.1513
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['jaw.R.001']]
    bones['chin.R'] = bone.name
    bone = arm.edit_bones.new('lid.B.L.003')
    bone.head[:] = 0.0472, -0.0781, 0.1068
    bone.tail[:] = 0.0515, -0.0692, 0.1104
    bone.roll = -0.0147
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.B.L.002']]
    bones['lid.B.L.003'] = bone.name
    bone = arm.edit_bones.new('lid.B.R.003')
    bone.head[:] = -0.0472, -0.0781, 0.1068
    bone.tail[:] = -0.0515, -0.0692, 0.1104
    bone.roll = 0.0147
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid.B.R.002']]
    bones['lid.B.R.003'] = bone.name
    bone = arm.edit_bones.new('cheek.B.L')
    bone.head[:] = 0.0236, -0.0877, 0.0519
    bone.tail[:] = 0.0493, -0.0691, 0.0632
    bone.roll = 0.0015
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['chin.L']]
    bones['cheek.B.L'] = bone.name
    bone = arm.edit_bones.new('cheek.B.R')
    bone.head[:] = -0.0236, -0.0877, 0.0519
    bone.tail[:] = -0.0493, -0.0691, 0.0632
    bone.roll = -0.0015
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['chin.R']]
    bones['cheek.B.R'] = bone.name
    bone = arm.edit_bones.new('cheek.B.L.001')
    bone.head[:] = 0.0493, -0.0691, 0.0632
    bone.tail[:] = 0.0568, -0.0506, 0.1052
    bone.roll = -0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.B.L']]
    bones['cheek.B.L.001'] = bone.name
    bone = arm.edit_bones.new('cheek.B.R.001')
    bone.head[:] = -0.0493, -0.0691, 0.0632
    bone.tail[:] = -0.0568, -0.0506, 0.1052
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.B.R']]
    bones['cheek.B.R.001'] = bone.name
    bone = arm.edit_bones.new('brow.T.L')
    bone.head[:] = 0.0568, -0.0506, 0.1052
    bone.tail[:] = 0.0556, -0.0689, 0.1249
    bone.roll = 0.1990
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.B.L.001']]
    bones['brow.T.L'] = bone.name
    bone = arm.edit_bones.new('brow.T.R')
    bone.head[:] = -0.0568, -0.0506, 0.1052
    bone.tail[:] = -0.0556, -0.0689, 0.1249
    bone.roll = -0.1990
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['cheek.B.R.001']]
    bones['brow.T.R'] = bone.name
    bone = arm.edit_bones.new('brow.T.L.001')
    bone.head[:] = 0.0556, -0.0689, 0.1249
    bone.tail[:] = 0.0394, -0.0828, 0.1310
    bone.roll = 0.2372
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.T.L']]
    bones['brow.T.L.001'] = bone.name
    bone = arm.edit_bones.new('brow.T.R.001')
    bone.head[:] = -0.0556, -0.0689, 0.1249
    bone.tail[:] = -0.0394, -0.0828, 0.1310
    bone.roll = -0.2372
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.T.R']]
    bones['brow.T.R.001'] = bone.name
    bone = arm.edit_bones.new('brow.T.L.002')
    bone.head[:] = 0.0394, -0.0828, 0.1310
    bone.tail[:] = 0.0144, -0.0912, 0.1236
    bone.roll = 0.0724
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.T.L.001']]
    bones['brow.T.L.002'] = bone.name
    bone = arm.edit_bones.new('brow.T.R.002')
    bone.head[:] = -0.0394, -0.0828, 0.1310
    bone.tail[:] = -0.0144, -0.0912, 0.1236
    bone.roll = -0.0724
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.T.R.001']]
    bones['brow.T.R.002'] = bone.name
    bone = arm.edit_bones.new('brow.T.L.003')
    bone.head[:] = 0.0144, -0.0912, 0.1236
    bone.tail[:] = 0.0003, -0.0905, 0.1125
    bone.roll = -0.0423
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.T.L.002']]
    bones['brow.T.L.003'] = bone.name
    bone = arm.edit_bones.new('brow.T.R.003')
    bone.head[:] = -0.0144, -0.0912, 0.1236
    bone.tail[:] = -0.0003, -0.0905, 0.1125
    bone.roll = 0.0423
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['brow.T.R.002']]
    bones['brow.T.R.003'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['face']]
    pbone.rigify_type = 'faces.super_face'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['jaw']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.T.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.B.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['forehead.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['forehead.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['eye.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['eye.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.T.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['teeth.T']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['teeth.B']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tongue']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.T.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.B.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['chin']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.T.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip.B.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['forehead.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['forehead.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.T.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.T.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tongue.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['chin.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.L.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.R.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.L.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.L.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.R.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.R.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['forehead.L.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['forehead.R.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tongue.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.L.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.R.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.L.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.L.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.B.R.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.T.R.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['temple.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['temple.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['nose.004']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.L.004']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['ear.R.004']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['jaw.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['jaw.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['jaw.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['jaw.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.L.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.R.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['chin.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['chin.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.L.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid.B.R.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.B.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.B.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['cheek.B.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.L.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.R.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.L.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.R.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.L.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['brow.T.R.003']]
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


def create_square_widget(rig, bone_name, size=1.0, bone_transform_name=None):
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts = [
            (0.5 * size, -2.9802322387695312e-08 * size,  0.5 * size),
            (-0.5 * size, -2.9802322387695312e-08 * size,  0.5 * size),
            (0.5 * size,  2.9802322387695312e-08 * size, -0.5 * size),
            (-0.5 * size,  2.9802322387695312e-08 * size, -0.5 * size),
        ]

        edges = [(0, 1), (2, 3), (0, 2), (3, 1)]
        faces = []

        mesh = obj.data
        mesh.from_pydata(verts, edges, faces)
        mesh.update()
        mesh.update()
        return obj
    else:
        return None
