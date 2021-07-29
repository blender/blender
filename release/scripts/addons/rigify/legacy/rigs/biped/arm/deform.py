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
from ....utils import connected_children_names
from ....utils import strip_org


class Rig:
    """ An FK arm rig, with hinge switch.

    """
    def __init__(self, obj, bone, params):
        self.obj = obj
        self.params = params

        # Get the chain of 3 connected bones
        self.org_bones = [bone] + connected_children_names(self.obj, bone)[:2]

        if len(self.org_bones) != 3:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of 3 bones" % (strip_org(bone)))

        # Get rig parameters
        if params.separate_hose_layers:
            layers = list(params.hose_layers)
        else:
            layers = None
        use_complex_rig = params.use_complex_arm
        elbow_base_name = params.elbow_base_name
        primary_rotation_axis = params.primary_rotation_axis

        # Based on common limb
        self.rubber_hose_limb = limb_common.RubberHoseLimb(obj, self.org_bones[0], self.org_bones[1], self.org_bones[2], use_complex_rig, elbow_base_name, primary_rotation_axis, layers)

    def generate(self):
        bone_list = self.rubber_hose_limb.generate()
        return bone_list
