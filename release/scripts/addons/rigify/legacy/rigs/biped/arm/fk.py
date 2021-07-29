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
from ....utils import create_widget
from ....utils import strip_org
from ....utils import get_layers


class Rig:
    """ An FK arm rig, with hinge switch.

    """
    def __init__(self, obj, bone, params):
        """ Gather and validate data about the rig.
            Store any data or references to data that will be needed later on.
            In particular, store references to bones that will be needed, and
            store names of bones that will be needed.
            Do NOT change any data in the scene.  This is a gathering phase only.

        """
        self.obj = obj

        # Get the chain of 3 connected bones
        self.org_bones = [bone] + connected_children_names(self.obj, bone)[:2]

        if len(self.org_bones) != 3:
            raise MetarigError("RIGIFY ERROR: Bone '%s': input to rig type must be a chain of at least 3 bones" % (strip_org(bone)))

        # Get params
        if "layers" in params:
            layers = get_layers(params["layers"])
        else:
            layers = None

        primary_rotation_axis = params.primary_rotation_axis

        # Arm is based on common limb
        self.fk_limb = limb_common.FKLimb(obj, self.org_bones[0], self.org_bones[1], self.org_bones[2], primary_rotation_axis, layers)

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.

        """
        bone_list = self.fk_limb.generate()
        uarm = bone_list[0]
        farm = bone_list[1]
        hand = bone_list[2]

        # Create hand widget
        ob = create_widget(self.obj, hand)
        if ob is not None:
            verts = [(0.7, 1.5, 0.0), (0.7, -0.25, 0.0), (-0.7, -0.25, 0.0), (-0.7, 1.5, 0.0), (0.7, 0.723, 0.0), (-0.7, 0.723, 0.0), (0.7, 0.0, 0.0), (-0.7, 0.0, 0.0)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (4, 6), (1, 6), (5, 7), (2, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        return [uarm, farm, hand]
