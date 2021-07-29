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

from .. import limb_common

from ....utils import MetarigError
from ....utils import copy_bone
from ....utils import connected_children_names, has_connected_children
from ....utils import strip_org, make_deformer_name


class Rig:
    """ A leg deform-bone setup.

    """
    def __init__(self, obj, bone, params):
        self.obj = obj
        self.params = params

        # Get the chain of 2 connected bones
        leg_bones = [bone] + connected_children_names(self.obj, bone)[:2]

        if len(leg_bones) != 2:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type -- leg bones != 2" % (strip_org(bone)))

        # Get the foot and heel
        foot = None
        heel = None
        for b in self.obj.data.bones[leg_bones[1]].children:
            if b.use_connect is True:
                if len(b.children) >= 1 and has_connected_children(b):
                    foot = b.name
                else:
                    heel = b.name

        if foot is None:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type -- could not find foot bone (that is, a bone with >1 children connected) attached to bone '%s'" % (strip_org(bone), strip_org(leg_bones[1])))
        if heel is None:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type -- could not find heel bone (that is, a bone with no children connected) attached to bone '%s'" % (strip_org(bone), strip_org(leg_bones[1])))
        # Get the toe
        toe = None
        for b in self.obj.data.bones[foot].children:
            if b.use_connect is True:
                toe = b.name

        if toe is None:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type -- toe is None" % (strip_org(bone)))

        self.org_bones = leg_bones + [foot, toe, heel]

        # Get rig parameters
        if params.separate_hose_layers:
            layers = list(params.hose_layers)
        else:
            layers = None
        use_complex_rig = params.use_complex_leg
        knee_base_name = params.knee_base_name
        primary_rotation_axis = params.primary_rotation_axis

        # Based on common limb
        self.rubber_hose_limb = limb_common.RubberHoseLimb(obj, self.org_bones[0], self.org_bones[1], self.org_bones[2], use_complex_rig, knee_base_name, primary_rotation_axis, layers)

    def generate(self):
        bone_list = self.rubber_hose_limb.generate()

        # Set up toe
        bpy.ops.object.mode_set(mode='EDIT')
        toe = copy_bone(self.obj, self.org_bones[3], make_deformer_name(strip_org(self.org_bones[3])))
        eb = self.obj.data.edit_bones
        eb[toe].use_connect = False
        eb[toe].parent = eb[self.org_bones[3]]

        return bone_list
