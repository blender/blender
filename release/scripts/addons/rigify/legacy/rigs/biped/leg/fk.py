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
from mathutils import Vector

from .. import limb_common

from ....utils import MetarigError
from ....utils import connected_children_names, has_connected_children
from ....utils import strip_org
from ....utils import get_layers
from ....utils import create_widget


class Rig:
    """ An FK leg rig, with hinge switch.

    """
    def __init__(self, obj, bone, params):
        """ Gather and validate data about the rig.
            Store any data or references to data that will be needed later on.
            In particular, store references to bones that will be needed, and
            store names of bones that will be needed.
            Do NOT change any data in the scene.  This is a gathering phase only.

        """
        self.obj = obj
        self.params = params

        # Get the chain of 2 connected bones
        leg_bones = [bone] + connected_children_names(self.obj, bone)[:2]

        if len(leg_bones) != 2:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type" % (strip_org(bone)))

        # Get the foot and heel
        foot = None
        heel = None
        for b in self.obj.data.bones[leg_bones[1]].children:
            if b.use_connect is True:
                if len(b.children) >= 1 and has_connected_children(b):
                    foot = b.name
                else:
                    heel = b.name

        if foot is None or heel is None:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type" % (strip_org(bone)))

        # Get the toe
        toe = None
        for b in self.obj.data.bones[foot].children:
            if b.use_connect is True:
                toe = b.name

        # Get the toe
        if toe is None:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type" % (strip_org(bone)))

        self.org_bones = leg_bones + [foot, toe, heel]

        # Get (optional) parent
        if self.obj.data.bones[bone].parent is None:
            self.org_parent = None
        else:
            self.org_parent = self.obj.data.bones[bone].parent.name

        # Get rig parameters
        if "layers" in params:
            layers = get_layers(params["layers"])
        else:
            layers = None

        primary_rotation_axis = params.primary_rotation_axis

        # Leg is based on common limb
        self.fk_limb = limb_common.FKLimb(obj, self.org_bones[0], self.org_bones[1], self.org_bones[2], primary_rotation_axis, layers)

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.

        """
        ctrl_bones = self.fk_limb.generate()
        thigh = ctrl_bones[0]
        shin = ctrl_bones[1]
        foot = ctrl_bones[2]
        foot_mch = ctrl_bones[3]

        # Position foot control
        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones
        foot_e = eb[foot]
        vec = Vector(eb[self.org_bones[3]].vector)
        vec.normalize()
        foot_e.tail = foot_e.head + (vec * foot_e.length)
        foot_e.roll = eb[self.org_bones[3]].roll
        bpy.ops.object.mode_set(mode='OBJECT')

        # Create foot widget
        ob = create_widget(self.obj, foot)
        if ob is not None:
            verts = [(0.7, 1.5, 0.0), (0.7, -0.25, 0.0), (-0.7, -0.25, 0.0), (-0.7, 1.5, 0.0), (0.7, 0.723, 0.0), (-0.7, 0.723, 0.0), (0.7, 0.0, 0.0), (-0.7, 0.0, 0.0)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (4, 6), (1, 6), (5, 7), (2, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        return [thigh, shin, foot, foot_mch]
