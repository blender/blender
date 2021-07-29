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
from ....utils import create_widget


class Rig:
    """ An IK arm rig, with an optional ik/fk switch.

    """
    def __init__(self, obj, bone, params, ikfk_switch=False):
        """ Gather and validate data about the rig.
            Store any data or references to data that will be needed later on.
            In particular, store references to bones that will be needed, and
            store names of bones that will be needed.
            Do NOT change any data in the scene.  This is a gathering phase only.

            ikfk_switch: if True, create an ik/fk switch slider
        """
        self.obj = obj
        self.params = params

        # Get the chain of 3 connected bones
        self.org_bones = [bone] + connected_children_names(self.obj, bone)[:2]
        if len(self.org_bones) != 3:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of 3 bones" % (strip_org(bone)))

        # Get the rig parameters
        if params.separate_ik_layers:
            layers = list(params.ik_layers)
        else:
            layers = None
        bend_hint = params.bend_hint
        primary_rotation_axis = params.primary_rotation_axis
        pole_target_base_name = self.params.elbow_base_name + "_target"

        # Arm is based on common limb
        self.ik_limb = limb_common.IKLimb(obj, self.org_bones[0], self.org_bones[1], self.org_bones[2], None, pole_target_base_name, primary_rotation_axis, bend_hint, layers, ikfk_switch)

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.

        """
        bone_list = self.ik_limb.generate()
        uarm = bone_list[0]
        farm = bone_list[1]
        hand = bone_list[2]
        # hand_mch = bone_list[3]
        pole = bone_list[4]
        # vispole = bone_list[5]
        # vishand = bone_list[6]

        ob = create_widget(self.obj, hand)
        if ob is not None:
            verts = [(0.7, 1.5, 0.0), (0.7, -0.25, 0.0), (-0.7, -0.25, 0.0), (-0.7, 1.5, 0.0), (0.7, 0.723, 0.0), (-0.7, 0.723, 0.0), (0.7, 0.0, 0.0), (-0.7, 0.0, 0.0)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (4, 6), (1, 6), (5, 7), (2, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        return [uarm, farm, hand, pole]
