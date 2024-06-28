# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from ..utils.rig import connected_children_names
from ..utils.naming import strip_mch, strip_org, make_mechanism_name
import re


def get_future_names_arm(bones):
    if len(bones) != 3:
        return

    names = dict()

    uarm = strip_mch(strip_org(bones[0].name))
    farm = strip_mch(strip_org(bones[1].name))
    hand = strip_mch(strip_org(bones[2].name))

    suffix = ''
    if uarm[-2:] == '.L' or uarm[-2:] == '.R':
        suffix = uarm[-2:]
        uarm = uarm.rstrip(suffix)
        farm = farm.rstrip(suffix)
        hand = hand.rstrip(suffix)

    # the following is declared in rig_ui
    # controls = ['upper_arm_ik.L', 'upper_arm_fk.L', 'forearm_fk.L', 'hand_fk.L', 'hand_ik.L', 'MCH-hand_fk.L',
    #             'upper_arm_parent.L']
    # tweaks = ['upper_arm_tweak.L.001', 'forearm_tweak.L', 'forearm_tweak.L.001']
    # ik_ctrl = ['hand_ik.L', 'MCH-upper_arm_ik.L', 'MCH-upper_arm_ik_target.L']
    # fk_ctrl = 'upper_arm_fk.L'
    # parent = 'upper_arm_parent.L'
    # hand_fk = 'hand_fk.L'
    # pole = 'upper_arm_ik_target.L'

    names['controls'] = [uarm + '_ik', uarm + '_fk', farm + '_fk', hand + '_fk', hand + '_ik',
                         make_mechanism_name(hand + '_fk'), uarm + '_parent']
    names['ik_ctrl'] = [hand + '_ik', make_mechanism_name(uarm) + '_ik', make_mechanism_name(uarm) + '_ik_target']
    names['fk_ctrl'] = uarm + '_fk' + suffix
    names['parent'] = uarm + '_parent' + suffix
    names['hand_fk'] = hand + '_fk' + suffix
    names['pole'] = uarm + '_ik_target' + suffix
    names['limb_type'] = 'arm'

    if suffix:
        for i, name in enumerate(names['controls']):
            names['controls'][i] = name + suffix
        for i, name in enumerate(names['ik_ctrl']):
            names['ik_ctrl'][i] = name + suffix

    return names


def get_future_names_leg(bones):
    if len(bones) != 4:
        return

    names = dict()

    thigh = strip_mch(strip_org(bones[0].name))
    shin = strip_mch(strip_org(bones[1].name))
    foot = strip_mch(strip_org(bones[2].name))
    toe = strip_mch(strip_org(bones[3].name))

    suffix = ''
    if thigh[-2:] == '.L' or thigh[-2:] == '.R':
        suffix = thigh[-2:]
        thigh = thigh.rstrip(suffix)
        shin = shin.rstrip(suffix)
        foot = foot.rstrip(suffix)
        toe = toe.rstrip(suffix)

    # the following is declared in rig_ui
    # controls = ['thigh_ik.R', 'thigh_fk.R', 'shin_fk.R', 'foot_fk.R', 'toe.R', 'foot_heel_ik.R', 'foot_ik.R',
    #             'MCH-foot_fk.R', 'thigh_parent.R']
    # tweaks = ['thigh_tweak.R.001', 'shin_tweak.R', 'shin_tweak.R.001']
    # ik_ctrl = ['foot_ik.R', 'MCH-thigh_ik.R', 'MCH-thigh_ik_target.R']
    # fk_ctrl = 'thigh_fk.R'
    # parent = 'thigh_parent.R'
    # foot_fk = 'foot_fk.R'
    # pole = 'thigh_ik_target.R'

    names['controls'] = [thigh + '_ik', thigh + '_fk', shin + '_fk', foot + '_fk', toe, foot + '_heel_ik',
                         foot + '_ik', make_mechanism_name(foot + '_fk'), thigh + '_parent']
    names['ik_ctrl'] = [foot + '_ik', make_mechanism_name(thigh) + '_ik', make_mechanism_name(thigh) + '_ik_target']
    names['fk_ctrl'] = thigh + '_fk' + suffix
    names['parent'] = thigh + '_parent' + suffix
    names['foot_fk'] = foot + '_fk' + suffix
    names['pole'] = thigh + '_ik_target' + suffix

    names['limb_type'] = 'leg'

    if suffix:
        for i, name in enumerate(names['controls']):
            names['controls'][i] = name + suffix
        for i, name in enumerate(names['ik_ctrl']):
            names['ik_ctrl'][i] = name + suffix

    return names


def get_future_names_paw(bones):
    if len(bones) != 4:
        return

    names = dict()

    thigh = strip_mch(strip_org(bones[0].name))
    shin = strip_mch(strip_org(bones[1].name))
    foot = strip_mch(strip_org(bones[2].name))
    toe = strip_mch(strip_org(bones[3].name))

    suffix = ''
    if thigh[-2:] == '.L' or thigh[-2:] == '.R':
        suffix = thigh[-2:]
        thigh = thigh.rstrip(suffix)
        shin = shin.rstrip(suffix)
        foot = foot.rstrip(suffix)
        toe = toe.rstrip(suffix)

    # the following is declared in rig_ui
    # controls = ['thigh_ik.R', 'thigh_fk.R', 'shin_fk.R', 'foot_fk.R', 'toe.R', 'foot_heel_ik.R', 'foot_ik.R',
    #             'MCH-foot_fk.R', 'thigh_parent.R']
    # tweaks = ['thigh_tweak.R.001', 'shin_tweak.R', 'shin_tweak.R.001']
    # ik_ctrl = ['foot_ik.R', 'MCH-thigh_ik.R', 'MCH-thigh_ik_target.R']
    # fk_ctrl = 'thigh_fk.R'
    # parent = 'thigh_parent.R'
    # foot_fk = 'foot_fk.R'
    # pole = 'thigh_ik_target.R'

    names['controls'] = [thigh + '_ik', thigh + '_fk', shin + '_fk', foot + '_fk', toe, foot + '_heel_ik',
                         foot + '_ik', make_mechanism_name(foot + '_fk'), thigh + '_parent']
    names['ik_ctrl'] = [foot + '_ik', make_mechanism_name(thigh) + '_ik', make_mechanism_name(thigh) + '_ik_target']
    names['fk_ctrl'] = thigh + '_fk' + suffix
    names['parent'] = thigh + '_parent' + suffix
    names['foot_fk'] = foot + '_fk' + suffix
    names['pole'] = thigh + '_ik_target' + suffix

    names['limb_type'] = 'paw'

    if suffix:
        for i, name in enumerate(names['controls']):
            names['controls'][i] = name + suffix
        for i, name in enumerate(names['ik_ctrl']):
            names['ik_ctrl'][i] = name + suffix

    return names


def get_future_names(bones):
    if bones[0].rigify_parameters.limb_type == 'arm':
        return get_future_names_arm(bones)
    elif bones[0].rigify_parameters.limb_type == 'leg':
        return get_future_names_leg(bones)
    elif bones[0].rigify_parameters.limb_type == 'paw':
        return get_future_names_paw(bones)


def get_limb_generated_names(rig):

    pose_bones = rig.pose.bones
    names = dict()

    for b in pose_bones:
        super_limb_orgs = []
        if re.match('^ORG', b.name) and b.rigify_type == 'limbs.super_limb':
            super_limb_orgs.append(b)
            children = connected_children_names(rig, b.name)
            for child in children:
                if re.match('^ORG', child) or re.match('^MCH', child):
                    super_limb_orgs.append(pose_bones[child])
            names[b.name] = get_future_names(super_limb_orgs)

    return names
