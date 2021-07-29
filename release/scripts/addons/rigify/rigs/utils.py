from .limbs.super_limb import Rig as LimbRig
from ..utils import connected_children_names
import re


def get_limb_generated_names(rig):

    pbones = rig.pose.bones
    names = dict()

    for b in pbones:
        super_limb_orgs = []
        if re.match('^ORG', b.name) and b.rigify_type == 'limbs.super_limb':
            super_limb_orgs.append(b)
            children = connected_children_names(rig, b.name)
            for child in children:
                if re.match('^ORG', child) or re.match('^MCH', child):
                    super_limb_orgs.append(pbones[child])
            names[b.name] = LimbRig.get_future_names(super_limb_orgs)

    return names
