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

from math import pi

import bpy
from rna_prop_ui import rna_idprop_ui_prop_get
from mathutils import Vector

from ...utils import angle_on_plane, align_bone_roll, align_bone_z_axis
from ...utils import new_bone, copy_bone, put_bone, make_nonscaling_child
from ...utils import strip_org, make_mechanism_name, make_deformer_name, insert_before_lr
from ...utils import create_widget, create_limb_widget, create_line_widget, create_sphere_widget


class FKLimb:
    def __init__(self, obj, bone1, bone2, bone3, primary_rotation_axis, layers):
        self.obj = obj

        self.org_bones = [bone1, bone2, bone3]

        # Get (optional) parent
        if self.obj.data.bones[bone1].parent is None:
            self.org_parent = None
        else:
            self.org_parent = self.obj.data.bones[bone1].parent.name

        # Get the rig parameters
        self.layers = layers
        self.primary_rotation_axis = primary_rotation_axis

    def generate(self):
        bpy.ops.object.mode_set(mode='EDIT')

        # Create non-scaling parent bone
        if self.org_parent is not None:
            loc = Vector(self.obj.data.edit_bones[self.org_bones[0]].head)
            parent = make_nonscaling_child(self.obj, self.org_parent, loc, "_fk")
        else:
            parent = None

        # Create the control bones
        ulimb = copy_bone(self.obj, self.org_bones[0], strip_org(insert_before_lr(self.org_bones[0], ".fk")))
        flimb = copy_bone(self.obj, self.org_bones[1], strip_org(insert_before_lr(self.org_bones[1], ".fk")))
        elimb = copy_bone(self.obj, self.org_bones[2], strip_org(insert_before_lr(self.org_bones[2], ".fk")))

        # Create the end-limb mechanism bone
        elimb_mch = copy_bone(self.obj, self.org_bones[2], make_mechanism_name(strip_org(self.org_bones[2])))

        # Create the anti-stretch bones
        # These sit between a parent and its child, and counteract the
        # stretching of the parent so that the child is unaffected
        fantistr = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[0], "_antistr.fk"))))
        eantistr = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], "_antistr.fk"))))

        # Create the hinge bones
        if parent is not None:
            socket1 = copy_bone(self.obj, ulimb, make_mechanism_name(ulimb + ".socket1"))
            socket2 = copy_bone(self.obj, ulimb, make_mechanism_name(ulimb + ".socket2"))

        # Get edit bones
        eb = self.obj.data.edit_bones

        ulimb_e = eb[ulimb]
        flimb_e = eb[flimb]
        elimb_e = eb[elimb]

        fantistr_e = eb[fantistr]
        eantistr_e = eb[eantistr]

        elimb_mch_e = eb[elimb_mch]

        if parent is not None:
            socket1_e = eb[socket1]
            socket2_e = eb[socket2]

        # Parenting
        elimb_mch_e.use_connect = False
        elimb_mch_e.parent = elimb_e

        elimb_e.use_connect = False
        elimb_e.parent = eantistr_e

        eantistr_e.use_connect = False
        eantistr_e.parent = flimb_e

        flimb_e.use_connect = False
        flimb_e.parent = fantistr_e

        fantistr_e.use_connect = False
        fantistr_e.parent = ulimb_e

        if parent is not None:
            socket1_e.use_connect = False
            socket1_e.parent = eb[parent]

            socket2_e.use_connect = False
            socket2_e.parent = None

            ulimb_e.use_connect = False
            ulimb_e.parent = socket2_e

        # Positioning
        fantistr_e.length /= 8
        put_bone(self.obj, fantistr, Vector(ulimb_e.tail))
        eantistr_e.length /= 8
        put_bone(self.obj, eantistr, Vector(flimb_e.tail))

        if parent is not None:
            socket1_e.length /= 4
            socket2_e.length /= 3

        # Object mode, get pose bones
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        ulimb_p = pb[ulimb]
        flimb_p = pb[flimb]
        elimb_p = pb[elimb]

        fantistr_p = pb[fantistr]
        eantistr_p = pb[eantistr]

        if parent is not None:
            socket2_p = pb[socket2]

        # Lock axes
        ulimb_p.lock_location = (True, True, True)
        flimb_p.lock_location = (True, True, True)
        elimb_p.lock_location = (True, True, True)

        # Set the elbow to only bend on the x-axis.
        flimb_p.rotation_mode = 'XYZ'
        if 'X' in self.primary_rotation_axis:
            flimb_p.lock_rotation = (False, True, True)
        elif 'Y' in self.primary_rotation_axis:
            flimb_p.lock_rotation = (True, False, True)
        else:
            flimb_p.lock_rotation = (True, True, False)

        # Set up custom properties
        if parent is not None:
            prop = rna_idprop_ui_prop_get(ulimb_p, "isolate", create=True)
            ulimb_p["isolate"] = 0.0
            prop["soft_min"] = prop["min"] = 0.0
            prop["soft_max"] = prop["max"] = 1.0

        prop = rna_idprop_ui_prop_get(ulimb_p, "stretch_length", create=True)
        ulimb_p["stretch_length"] = 1.0
        prop["min"] = 0.05
        prop["max"] = 20.0
        prop["soft_min"] = 0.25
        prop["soft_max"] = 4.0

        # Stretch drivers
        def add_stretch_drivers(pose_bone):
            driver = pose_bone.driver_add("scale", 1).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "stretch_length"

            driver = pose_bone.driver_add("scale", 0).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "1/sqrt(stretch_length)"

            driver = pose_bone.driver_add("scale", 2).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "1/sqrt(stretch_length)"

        def add_antistretch_drivers(pose_bone):
            driver = pose_bone.driver_add("scale", 1).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "1/stretch_length"

            driver = pose_bone.driver_add("scale", 0).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "sqrt(stretch_length)"

            driver = pose_bone.driver_add("scale", 2).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "sqrt(stretch_length)"

        add_stretch_drivers(ulimb_p)
        add_stretch_drivers(flimb_p)
        add_antistretch_drivers(fantistr_p)
        add_antistretch_drivers(eantistr_p)

        # Hinge constraints / drivers
        if parent is not None:
            con = socket2_p.constraints.new('COPY_LOCATION')
            con.name = "copy_location"
            con.target = self.obj
            con.subtarget = socket1

            con = socket2_p.constraints.new('COPY_TRANSFORMS')
            con.name = "isolate_off"
            con.target = self.obj
            con.subtarget = socket1

            # Driver
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'AVERAGE'
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = ulimb_p.path_from_id() + '["isolate"]'
            mod = fcurve.modifiers[0]
            mod.poly_order = 1
            mod.coefficients[0] = 1.0
            mod.coefficients[1] = -1.0

        # Constrain org bones to controls
        con = pb[self.org_bones[0]].constraints.new('COPY_TRANSFORMS')
        con.name = "fk"
        con.target = self.obj
        con.subtarget = ulimb

        con = pb[self.org_bones[1]].constraints.new('COPY_TRANSFORMS')
        con.name = "fk"
        con.target = self.obj
        con.subtarget = flimb

        con = pb[self.org_bones[2]].constraints.new('COPY_TRANSFORMS')
        con.name = "fk"
        con.target = self.obj
        con.subtarget = elimb_mch

        # Set layers if specified
        if self.layers:
            ulimb_p.bone.layers = self.layers
            flimb_p.bone.layers = self.layers
            elimb_p.bone.layers = self.layers

        # Create control widgets
        create_limb_widget(self.obj, ulimb)
        create_limb_widget(self.obj, flimb)

        ob = create_widget(self.obj, elimb)
        if ob is not None:
            verts = [(0.7, 1.5, 0.0), (0.7, -0.25, 0.0), (-0.7, -0.25, 0.0), (-0.7, 1.5, 0.0), (0.7, 0.723, 0.0), (-0.7, 0.723, 0.0), (0.7, 0.0, 0.0), (-0.7, 0.0, 0.0)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (4, 6), (1, 6), (5, 7), (2, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        return [ulimb, flimb, elimb, elimb_mch]


class IKLimb:
    """ An IK limb rig, with an optional ik/fk switch.

    """
    def __init__(self, obj, bone1, bone2, bone3, pole_parent, pole_target_base_name, primary_rotation_axis, bend_hint, layers, ikfk_switch=False):
        self.obj = obj
        self.switch = ikfk_switch

        # Get the chain of 3 connected bones
        self.org_bones = [bone1, bone2, bone3]

        # Get (optional) parent
        if self.obj.data.bones[bone1].parent is None:
            self.org_parent = None
        else:
            self.org_parent = self.obj.data.bones[bone1].parent.name

        self.pole_parent = pole_parent

        # Get the rig parameters
        self.pole_target_base_name = pole_target_base_name
        self.layers = layers
        self.bend_hint = bend_hint
        self.primary_rotation_axis = primary_rotation_axis

    def generate(self):
        bpy.ops.object.mode_set(mode='EDIT')

        # Create non-scaling parent bone
        if self.org_parent is not None:
            loc = Vector(self.obj.data.edit_bones[self.org_bones[0]].head)
            parent = make_nonscaling_child(self.obj, self.org_parent, loc, "_ik")
            if self.pole_parent is None:
                self.pole_parent = parent
        else:
            parent = None

        # Create the bones
        ulimb = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[0], ".ik"))))
        flimb = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], ".ik"))))
        elimb = copy_bone(self.obj, self.org_bones[2], strip_org(insert_before_lr(self.org_bones[2], ".ik")))
        elimb_mch = copy_bone(self.obj, self.org_bones[2], make_mechanism_name(strip_org(self.org_bones[2])))

        ulimb_nostr = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[0], ".nostr.ik"))))
        flimb_nostr = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], ".nostr.ik"))))

        ulimb_str = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[0], ".stretch.ik"))))
        flimb_str = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], ".stretch.ik"))))

        pole_target_name = self.pole_target_base_name + "." + insert_before_lr(self.org_bones[0], ".ik").split(".", 1)[1]
        pole = copy_bone(self.obj, self.org_bones[0], pole_target_name)
        if self.pole_parent == self.org_bones[2]:
            self.pole_parent = elimb_mch
        if self.pole_parent is not None:
            pole_par = copy_bone(self.obj, self.pole_parent, make_mechanism_name(insert_before_lr(pole_target_name, "_parent")))

        viselimb = copy_bone(self.obj, self.org_bones[2], "VIS-" + strip_org(insert_before_lr(self.org_bones[2], ".ik")))
        vispole = copy_bone(self.obj, self.org_bones[1], "VIS-" + strip_org(insert_before_lr(self.org_bones[0], "_pole.ik")))

        # Get edit bones
        eb = self.obj.data.edit_bones

        if parent is not None:
            parent_e = eb[parent]
        ulimb_e = eb[ulimb]
        flimb_e = eb[flimb]
        elimb_e = eb[elimb]
        elimb_mch_e = eb[elimb_mch]
        ulimb_nostr_e = eb[ulimb_nostr]
        flimb_nostr_e = eb[flimb_nostr]
        ulimb_str_e = eb[ulimb_str]
        flimb_str_e = eb[flimb_str]
        pole_e = eb[pole]
        if self.pole_parent is not None:
            pole_par_e = eb[pole_par]
        viselimb_e = eb[viselimb]
        vispole_e = eb[vispole]

        # Parenting
        ulimb_e.use_connect = False
        ulimb_nostr_e.use_connect = False
        if parent is not None:
            ulimb_e.parent = parent_e
            ulimb_nostr_e.parent = parent_e

        flimb_e.parent = ulimb_e
        flimb_nostr_e.parent = ulimb_nostr_e

        elimb_e.use_connect = False
        elimb_e.parent = None

        elimb_mch_e.use_connect = False
        elimb_mch_e.parent = elimb_e

        ulimb_str_e.use_connect = False
        ulimb_str_e.parent = ulimb_e.parent

        flimb_str_e.use_connect = False
        flimb_str_e.parent = ulimb_e.parent

        pole_e.use_connect = False
        if self.pole_parent is not None:
            pole_par_e.parent = None
            pole_e.parent = pole_par_e

        viselimb_e.use_connect = False
        viselimb_e.parent = None

        vispole_e.use_connect = False
        vispole_e.parent = None

        # Misc
        elimb_e.use_local_location = False

        viselimb_e.hide_select = True
        vispole_e.hide_select = True

        # Positioning
        v1 = flimb_e.tail - ulimb_e.head
        if 'X' in self.primary_rotation_axis or 'Y' in self.primary_rotation_axis:
            v2 = v1.cross(flimb_e.x_axis)
            if (v2 * flimb_e.z_axis) > 0.0:
                v2 *= -1.0
        else:
            v2 = v1.cross(flimb_e.z_axis)
            if (v2 * flimb_e.x_axis) < 0.0:
                v2 *= -1.0
        v2.normalize()
        v2 *= v1.length

        if '-' in self.primary_rotation_axis:
            v2 *= -1

        pole_e.head = flimb_e.head + v2
        pole_e.tail = pole_e.head + (Vector((0, 1, 0)) * (v1.length / 8))
        pole_e.roll = 0.0
        if parent is not None:
            pole_par_e.length *= 0.75

        viselimb_e.tail = viselimb_e.head + Vector((0, 0, v1.length / 32))
        vispole_e.tail = vispole_e.head + Vector((0, 0, v1.length / 32))

        # Determine the pole offset value
        plane = (flimb_e.tail - ulimb_e.head).normalized()
        vec1 = ulimb_e.x_axis.normalized()
        vec2 = (pole_e.head - ulimb_e.head).normalized()
        pole_offset = angle_on_plane(plane, vec1, vec2)

        # Object mode, get pose bones
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        ulimb_p = pb[ulimb]
        flimb_p = pb[flimb]
        elimb_p = pb[elimb]
        ulimb_nostr_p = pb[ulimb_nostr]
        flimb_nostr_p = pb[flimb_nostr]
        ulimb_str_p = pb[ulimb_str]
        flimb_str_p = pb[flimb_str]
        pole_p = pb[pole]
        if self.pole_parent is not None:
            pole_par_p = pb[pole_par]
        viselimb_p = pb[viselimb]
        vispole_p = pb[vispole]

        # Set the elbow to only bend on the primary axis
        if 'X' in self.primary_rotation_axis:
            flimb_p.lock_ik_y = True
            flimb_p.lock_ik_z = True
            flimb_nostr_p.lock_ik_y = True
            flimb_nostr_p.lock_ik_z = True
        elif 'Y' in self.primary_rotation_axis:
            flimb_p.lock_ik_x = True
            flimb_p.lock_ik_z = True
            flimb_nostr_p.lock_ik_x = True
            flimb_nostr_p.lock_ik_z = True
        else:
            flimb_p.lock_ik_x = True
            flimb_p.lock_ik_y = True
            flimb_nostr_p.lock_ik_x = True
            flimb_nostr_p.lock_ik_y = True

        # Limb stretches
        ulimb_nostr_p.ik_stretch = 0.0
        flimb_nostr_p.ik_stretch = 0.0

        # This next bit is weird.  The values calculated cause
        # ulimb and flimb to preserve their relative lengths
        # while stretching.
        l1 = ulimb_p.length
        l2 = flimb_p.length
        if l1 < l2:
            ulimb_p.ik_stretch = (l1 ** (1 / 3)) / (l2 ** (1 / 3))
            flimb_p.ik_stretch = 1.0
        else:
            ulimb_p.ik_stretch = 1.0
            flimb_p.ik_stretch = (l2 ** (1 / 3)) / (l1 ** (1 / 3))

        # Pole target only translates
        pole_p.lock_location = False, False, False
        pole_p.lock_rotation = True, True, True
        pole_p.lock_rotation_w = True
        pole_p.lock_scale = True, True, True

        # Set up custom properties
        if self.switch is True:
            prop = rna_idprop_ui_prop_get(elimb_p, "ikfk_switch", create=True)
            elimb_p["ikfk_switch"] = 0.0
            prop["soft_min"] = prop["min"] = 0.0
            prop["soft_max"] = prop["max"] = 1.0

        if self.pole_parent is not None:
            prop = rna_idprop_ui_prop_get(pole_p, "follow", create=True)
            pole_p["follow"] = 1.0
            prop["soft_min"] = prop["min"] = 0.0
            prop["soft_max"] = prop["max"] = 1.0

        prop = rna_idprop_ui_prop_get(elimb_p, "stretch_length", create=True)
        elimb_p["stretch_length"] = 1.0
        prop["min"] = 0.05
        prop["max"] = 20.0
        prop["soft_min"] = 0.25
        prop["soft_max"] = 4.0

        prop = rna_idprop_ui_prop_get(elimb_p, "auto_stretch", create=True)
        elimb_p["auto_stretch"] = 1.0
        prop["soft_min"] = prop["min"] = 0.0
        prop["soft_max"] = prop["max"] = 1.0

        # Stretch parameter drivers
        def add_stretch_drivers(pose_bone):
            driver = pose_bone.driver_add("scale", 1).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = elimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "stretch_length"

            driver = pose_bone.driver_add("scale", 0).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = elimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "stretch_length"

            driver = pose_bone.driver_add("scale", 2).driver
            var = driver.variables.new()
            var.name = "stretch_length"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = elimb_p.path_from_id() + '["stretch_length"]'
            driver.type = 'SCRIPTED'
            driver.expression = "stretch_length"
        add_stretch_drivers(ulimb_nostr_p)

        # Bend direction hint
        def add_bend_hint(pose_bone, axis):
            con = pose_bone.constraints.new('LIMIT_ROTATION')
            con.name = "bend_hint"
            con.owner_space = 'LOCAL'
            if axis == 'X':
                con.use_limit_x = True
                con.min_x = pi / 10
                con.max_x = pi / 10
            elif axis == '-X':
                con.use_limit_x = True
                con.min_x = -pi / 10
                con.max_x = -pi / 10
            elif axis == 'Y':
                con.use_limit_y = True
                con.min_y = pi / 10
                con.max_y = pi / 10
            elif axis == '-Y':
                con.use_limit_y = True
                con.min_y = -pi / 10
                con.max_y = -pi / 10
            elif axis == 'Z':
                con.use_limit_z = True
                con.min_z = pi / 10
                con.max_z = pi / 10
            elif axis == '-Z':
                con.use_limit_z = True
                con.min_z = -pi / 10
                con.max_z = -pi / 10
        if self.bend_hint:
            add_bend_hint(flimb_p, self.primary_rotation_axis)
            add_bend_hint(flimb_nostr_p, self.primary_rotation_axis)

        # Constrain normal IK chain to no-stretch IK chain
        con = ulimb_p.constraints.new('COPY_TRANSFORMS')
        con.name = "pre_stretch"
        con.target = self.obj
        con.subtarget = ulimb_nostr

        con = flimb_p.constraints.new('COPY_TRANSFORMS')
        con.name = "pre_stretch"
        con.target = self.obj
        con.subtarget = flimb_nostr

        # IK Constraints
        con = flimb_nostr_p.constraints.new('IK')
        con.name = "ik"
        con.target = self.obj
        con.subtarget = elimb_mch
        con.pole_target = self.obj
        con.pole_subtarget = pole
        con.pole_angle = pole_offset
        con.chain_count = 2

        con = flimb_p.constraints.new('IK')
        con.name = "ik"
        con.target = self.obj
        con.subtarget = elimb_mch
        con.chain_count = 2

        # Driver to enable/disable auto stretching IK chain
        fcurve = con.driver_add("influence")
        driver = fcurve.driver
        var = driver.variables.new()
        driver.type = 'AVERAGE'
        var.name = "var"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = self.obj
        var.targets[0].data_path = elimb_p.path_from_id() + '["auto_stretch"]'

        # Stretch bone constraints
        con = ulimb_str_p.constraints.new('COPY_TRANSFORMS')
        con.name = "anchor"
        con.target = self.obj
        con.subtarget = ulimb
        con = ulimb_str_p.constraints.new('MAINTAIN_VOLUME')
        con.name = "stretch"
        con.owner_space = 'LOCAL'

        con = flimb_str_p.constraints.new('COPY_TRANSFORMS')
        con.name = "anchor"
        con.target = self.obj
        con.subtarget = flimb
        con = flimb_str_p.constraints.new('MAINTAIN_VOLUME')
        con.name = "stretch"
        con.owner_space = 'LOCAL'

        # Pole target parent
        if self.pole_parent is not None:
            con = pole_par_p.constraints.new('COPY_TRANSFORMS')
            con.name = "parent"
            con.target = self.obj
            con.subtarget = self.pole_parent

            driver = con.driver_add("influence").driver
            var = driver.variables.new()
            var.name = "follow"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = pole_p.path_from_id() + '["follow"]'
            driver.type = 'SUM'

        # Constrain org bones
        con = pb[self.org_bones[0]].constraints.new('COPY_TRANSFORMS')
        con.name = "ik"
        con.target = self.obj
        con.subtarget = ulimb_str
        if self.switch is True:
            # IK/FK switch driver
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'AVERAGE'
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = elimb_p.path_from_id() + '["ikfk_switch"]'

        con = pb[self.org_bones[1]].constraints.new('COPY_TRANSFORMS')
        con.name = "ik"
        con.target = self.obj
        con.subtarget = flimb_str
        if self.switch is True:
            # IK/FK switch driver
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'AVERAGE'
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = elimb_p.path_from_id() + '["ikfk_switch"]'

        con = pb[self.org_bones[2]].constraints.new('COPY_TRANSFORMS')
        con.name = "ik"
        con.target = self.obj
        con.subtarget = elimb_mch
        if self.switch is True:
            # IK/FK switch driver
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'AVERAGE'
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = elimb_p.path_from_id() + '["ikfk_switch"]'

        # VIS limb-end constraints
        con = viselimb_p.constraints.new('COPY_LOCATION')
        con.name = "copy_loc"
        con.target = self.obj
        con.subtarget = self.org_bones[2]

        con = viselimb_p.constraints.new('STRETCH_TO')
        con.name = "stretch_to"
        con.target = self.obj
        con.subtarget = elimb
        con.volume = 'NO_VOLUME'
        con.rest_length = viselimb_p.length

        # VIS pole constraints
        con = vispole_p.constraints.new('COPY_LOCATION')
        con.name = "copy_loc"
        con.target = self.obj
        con.subtarget = self.org_bones[1]

        con = vispole_p.constraints.new('STRETCH_TO')
        con.name = "stretch_to"
        con.target = self.obj
        con.subtarget = pole
        con.volume = 'NO_VOLUME'
        con.rest_length = vispole_p.length

        # Set layers if specified
        if self.layers:
            elimb_p.bone.layers = self.layers
            pole_p.bone.layers = self.layers
            viselimb_p.bone.layers = self.layers
            vispole_p.bone.layers = self.layers

        # Create widgets
        create_line_widget(self.obj, vispole)
        create_line_widget(self.obj, viselimb)
        create_sphere_widget(self.obj, pole)

        ob = create_widget(self.obj, elimb)
        if ob is not None:
            verts = [(0.7, 1.5, 0.0), (0.7, -0.25, 0.0), (-0.7, -0.25, 0.0), (-0.7, 1.5, 0.0), (0.7, 0.723, 0.0), (-0.7, 0.723, 0.0), (0.7, 0.0, 0.0), (-0.7, 0.0, 0.0)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (4, 6), (1, 6), (5, 7), (2, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        return [ulimb, flimb, elimb, elimb_mch, pole, vispole, viselimb]


class RubberHoseLimb:
    def __init__(self, obj, bone1, bone2, bone3, use_complex_limb, junc_base_name, primary_rotation_axis, layers):
        self.obj = obj

        # Get the chain of 3 connected bones
        self.org_bones = [bone1, bone2, bone3]

        # Get (optional) parent
        if self.obj.data.bones[bone1].parent is None:
            self.org_parent = None
        else:
            self.org_parent = self.obj.data.bones[bone1].parent.name

        # Get rig parameters
        self.layers = layers
        self.primary_rotation_axis = primary_rotation_axis
        self.use_complex_limb = use_complex_limb
        self.junc_base_name = junc_base_name

    def generate(self):
        bpy.ops.object.mode_set(mode='EDIT')

        # Create non-scaling parent bone
        if self.org_parent is not None:
            loc = Vector(self.obj.data.edit_bones[self.org_bones[0]].head)
            parent = make_nonscaling_child(self.obj, self.org_parent, loc, "_rh")
        else:
            parent = None

        if not self.use_complex_limb:
            # Simple rig

            # Create bones
            ulimb = copy_bone(self.obj, self.org_bones[0], make_deformer_name(strip_org(self.org_bones[0])))
            flimb = copy_bone(self.obj, self.org_bones[1], make_deformer_name(strip_org(self.org_bones[1])))
            elimb = copy_bone(self.obj, self.org_bones[2], make_deformer_name(strip_org(self.org_bones[2])))

            # Get edit bones
            eb = self.obj.data.edit_bones

            ulimb_e = eb[ulimb]
            flimb_e = eb[flimb]
            elimb_e = eb[elimb]

            # Parenting
            elimb_e.parent = flimb_e
            elimb_e.use_connect = True

            flimb_e.parent = ulimb_e
            flimb_e.use_connect = True

            if parent is not None:
                elimb_e.use_connect = False
                ulimb_e.parent = eb[parent]

            # Object mode, get pose bones
            bpy.ops.object.mode_set(mode='OBJECT')
            pb = self.obj.pose.bones

            ulimb_p = pb[ulimb]
            flimb_p = pb[flimb]
            elimb_p = pb[elimb]

            # Constrain def bones to org bones
            con = ulimb_p.constraints.new('COPY_TRANSFORMS')
            con.name = "def"
            con.target = self.obj
            con.subtarget = self.org_bones[0]

            con = flimb_p.constraints.new('COPY_TRANSFORMS')
            con.name = "def"
            con.target = self.obj
            con.subtarget = self.org_bones[1]

            con = elimb_p.constraints.new('COPY_TRANSFORMS')
            con.name = "def"
            con.target = self.obj
            con.subtarget = self.org_bones[2]

            return []
        else:
            # Complex rig

            # Get the .R or .L off the end of the upper limb name if it exists
            lr = self.org_bones[0].split(".", 1)
            if len(lr) == 1:
                lr = ""
            else:
                lr = lr[1]

            # Create bones
            # Deformation bones
            ulimb1 = copy_bone(self.obj, self.org_bones[0], make_deformer_name(strip_org(insert_before_lr(self.org_bones[0], ".01"))))
            ulimb2 = copy_bone(self.obj, self.org_bones[0], make_deformer_name(strip_org(insert_before_lr(self.org_bones[0], ".02"))))
            flimb1 = copy_bone(self.obj, self.org_bones[1], make_deformer_name(strip_org(insert_before_lr(self.org_bones[1], ".01"))))
            flimb2 = copy_bone(self.obj, self.org_bones[1], make_deformer_name(strip_org(insert_before_lr(self.org_bones[1], ".02"))))
            elimb = copy_bone(self.obj, self.org_bones[2], make_deformer_name(strip_org(self.org_bones[2])))

            # Bones for switchable smooth bbone transition at elbow/knee
            ulimb2_smoother = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[0], "_smth.02"))))
            flimb1_smoother = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], "_smth.01"))))
            flimb1_pos = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], ".01"))))

            # Elbow/knee junction bone
            junc = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(self.org_bones[1], ".junc"))))

            # Hose controls
            uhoseend = new_bone(self.obj, strip_org(insert_before_lr(self.org_bones[0], "_hose_end")))
            uhose = new_bone(self.obj, strip_org(insert_before_lr(self.org_bones[0], "_hose")))
            jhose = new_bone(self.obj, self.junc_base_name + "_hose." + lr)
            fhose = new_bone(self.obj, strip_org(insert_before_lr(self.org_bones[1], "_hose")))
            fhoseend = new_bone(self.obj, strip_org(insert_before_lr(self.org_bones[1], "_hose_end")))

            # Hose control parents
            uhoseend_par = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(uhoseend, "_p"))))
            uhose_par = copy_bone(self.obj, self.org_bones[0], make_mechanism_name(strip_org(insert_before_lr(uhose, "_p"))))
            jhose_par = copy_bone(self.obj, junc, make_mechanism_name(strip_org(insert_before_lr(jhose, "_p"))))
            fhose_par = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(fhose, "_p"))))
            fhoseend_par = copy_bone(self.obj, self.org_bones[1], make_mechanism_name(strip_org(insert_before_lr(fhoseend, "_p"))))

            # Get edit bones
            eb = self.obj.data.edit_bones

            if parent is not None:
                parent_e = eb[parent]
            else:
                parent_e = None

            ulimb1_e = eb[ulimb1]
            ulimb2_e = eb[ulimb2]
            flimb1_e = eb[flimb1]
            flimb2_e = eb[flimb2]
            elimb_e = eb[elimb]

            ulimb2_smoother_e = eb[ulimb2_smoother]
            flimb1_smoother_e = eb[flimb1_smoother]
            flimb1_pos_e = eb[flimb1_pos]

            junc_e = eb[junc]

            uhoseend_e = eb[uhoseend]
            uhose_e = eb[uhose]
            jhose_e = eb[jhose]
            fhose_e = eb[fhose]
            fhoseend_e = eb[fhoseend]

            uhoseend_par_e = eb[uhoseend_par]
            uhose_par_e = eb[uhose_par]
            jhose_par_e = eb[jhose_par]
            fhose_par_e = eb[fhose_par]
            fhoseend_par_e = eb[fhoseend_par]

            # Parenting
            if parent is not None:
                ulimb1_e.use_connect = False
                ulimb1_e.parent = parent_e

            ulimb2_e.use_connect = False
            ulimb2_e.parent = eb[self.org_bones[0]]

            ulimb2_smoother_e.use_connect = True
            ulimb2_smoother_e.parent = ulimb2_e

            flimb1_e.use_connect = True
            flimb1_e.parent = flimb1_smoother_e

            flimb1_smoother_e.use_connect = False
            flimb1_smoother_e.parent = flimb1_pos_e

            flimb1_pos_e.use_connect = False
            flimb1_pos_e.parent = eb[self.org_bones[1]]

            flimb2_e.use_connect = False
            flimb2_e.parent = eb[self.org_bones[1]]

            elimb_e.use_connect = False
            elimb_e.parent = eb[self.org_bones[2]]

            junc_e.use_connect = False
            junc_e.parent = eb[self.org_bones[0]]

            uhoseend_e.use_connect = False
            uhoseend_e.parent = uhoseend_par_e

            uhose_e.use_connect = False
            uhose_e.parent = uhose_par_e

            jhose_e.use_connect = False
            jhose_e.parent = jhose_par_e

            fhose_e.use_connect = False
            fhose_e.parent = fhose_par_e

            fhoseend_e.use_connect = False
            fhoseend_e.parent = fhoseend_par_e

            uhoseend_par_e.use_connect = False
            uhoseend_par_e.parent = parent_e

            uhose_par_e.use_connect = False
            uhose_par_e.parent = parent_e

            jhose_par_e.use_connect = False
            jhose_par_e.parent = parent_e

            fhose_par_e.use_connect = False
            fhose_par_e.parent = parent_e

            fhoseend_par_e.use_connect = False
            fhoseend_par_e.parent = parent_e

            # Positioning
            ulimb1_e.length *= 0.5
            ulimb2_e.head = Vector(ulimb1_e.tail)
            flimb1_e.length *= 0.5
            flimb2_e.head = Vector(flimb1_e.tail)
            align_bone_roll(self.obj, flimb2, elimb)

            ulimb2_smoother_e.tail = Vector(flimb1_e.tail)
            ulimb2_smoother_e.roll = flimb1_e.roll

            flimb1_smoother_e.head = Vector(ulimb1_e.tail)
            flimb1_pos_e.length *= 0.5

            junc_e.length *= 0.2

            uhoseend_par_e.length *= 0.25
            uhose_par_e.length *= 0.25
            jhose_par_e.length *= 0.15
            fhose_par_e.length *= 0.25
            fhoseend_par_e.length *= 0.25
            put_bone(self.obj, uhoseend_par, Vector(ulimb1_e.head))
            put_bone(self.obj, uhose_par, Vector(ulimb1_e.tail))
            put_bone(self.obj, jhose_par, Vector(ulimb2_e.tail))
            put_bone(self.obj, fhose_par, Vector(flimb1_e.tail))
            put_bone(self.obj, fhoseend_par, Vector(flimb2_e.tail))

            put_bone(self.obj, uhoseend, Vector(ulimb1_e.head))
            put_bone(self.obj, uhose, Vector(ulimb1_e.tail))
            put_bone(self.obj, jhose, Vector(ulimb2_e.tail))
            put_bone(self.obj, fhose, Vector(flimb1_e.tail))
            put_bone(self.obj, fhoseend, Vector(flimb2_e.tail))

            if 'X' in self.primary_rotation_axis:
                upoint = Vector(ulimb1_e.z_axis)
                fpoint = Vector(flimb1_e.z_axis)
            elif 'Z' in self.primary_rotation_axis:
                upoint = Vector(ulimb1_e.x_axis)
                fpoint = Vector(flimb1_e.x_axis)
            else:  # Y
                upoint = Vector(ulimb1_e.z_axis)
                fpoint = Vector(flimb1_e.z_axis)

            if '-' not in self.primary_rotation_axis:
                upoint *= -1
                fpoint *= -1

            if 'Y' in self.primary_rotation_axis:
                uside = Vector(ulimb1_e.x_axis)
                fside = Vector(flimb1_e.x_axis)
            else:
                uside = Vector(ulimb1_e.y_axis) * -1
                fside = Vector(flimb1_e.y_axis) * -1

            uhoseend_e.tail = uhoseend_e.head + upoint
            uhose_e.tail = uhose_e.head + upoint
            jhose_e.tail = fhose_e.head + upoint + fpoint
            fhose_e.tail = fhose_e.head + fpoint
            fhoseend_e.tail = fhoseend_e.head + fpoint

            align_bone_z_axis(self.obj, uhoseend, uside)
            align_bone_z_axis(self.obj, uhose, uside)
            align_bone_z_axis(self.obj, jhose, uside + fside)
            align_bone_z_axis(self.obj, fhose, fside)
            align_bone_z_axis(self.obj, fhoseend, fside)

            l = 0.125 * (ulimb1_e.length + ulimb2_e.length + flimb1_e.length + flimb2_e.length)
            uhoseend_e.length = l
            uhose_e.length = l
            jhose_e.length = l
            fhose_e.length = l
            fhoseend_e.length = l

            # Object mode, get pose bones
            bpy.ops.object.mode_set(mode='OBJECT')
            pb = self.obj.pose.bones

            ulimb1_p = pb[ulimb1]
            ulimb2_p = pb[ulimb2]
            flimb1_p = pb[flimb1]
            flimb2_p = pb[flimb2]
            elimb_p = pb[elimb]

            ulimb2_smoother_p = pb[ulimb2_smoother]
            flimb1_smoother_p = pb[flimb1_smoother]
            flimb1_pos_p = pb[flimb1_pos]

            junc_p = pb[junc]

            uhoseend_p = pb[uhoseend]
            uhose_p = pb[uhose]
            jhose_p = pb[jhose]
            fhose_p = pb[fhose]
            fhoseend_p = pb[fhoseend]

            #uhoseend_par_p = pb[uhoseend_par]
            uhose_par_p = pb[uhose_par]
            jhose_par_p = pb[jhose_par]
            fhose_par_p = pb[fhose_par]
            fhoseend_par_p = pb[fhoseend_par]

            # Lock axes
            uhose_p.lock_rotation = (True, True, True)
            uhose_p.lock_rotation_w = True
            uhose_p.lock_scale = (True, True, True)

            jhose_p.lock_rotation = (True, True, True)
            jhose_p.lock_rotation_w = True
            jhose_p.lock_scale = (True, True, True)

            fhose_p.lock_rotation = (True, True, True)
            fhose_p.lock_rotation_w = True
            fhose_p.lock_scale = (True, True, True)

            # B-bone settings
            ulimb2_p.bone.bbone_segments = 16
            ulimb2_p.bone.bbone_in = 0.0
            ulimb2_p.bone.bbone_out = 1.0

            ulimb2_smoother_p.bone.bbone_segments = 16
            ulimb2_smoother_p.bone.bbone_in = 1.0
            ulimb2_smoother_p.bone.bbone_out = 0.0

            flimb1_p.bone.bbone_segments = 16
            flimb1_p.bone.bbone_in = 1.0
            flimb1_p.bone.bbone_out = 0.0

            flimb1_smoother_p.bone.bbone_segments = 16
            flimb1_smoother_p.bone.bbone_in = 0.0
            flimb1_smoother_p.bone.bbone_out = 1.0

            # Custom properties
            prop = rna_idprop_ui_prop_get(jhose_p, "smooth_bend", create=True)
            jhose_p["smooth_bend"] = 0.0
            prop["soft_min"] = prop["min"] = 0.0
            prop["soft_max"] = prop["max"] = 1.0

            # Constraints
            con = ulimb1_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = uhoseend
            con = ulimb1_p.constraints.new('COPY_SCALE')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = self.org_bones[0]
            con = ulimb1_p.constraints.new('DAMPED_TRACK')
            con.name = "track"
            con.target = self.obj
            con.subtarget = uhose
            con = ulimb1_p.constraints.new('STRETCH_TO')
            con.name = "track"
            con.target = self.obj
            con.subtarget = uhose
            con.volume = 'NO_VOLUME'

            con = ulimb2_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = uhose
            con = ulimb2_p.constraints.new('DAMPED_TRACK')
            con.name = "track"
            con.target = self.obj
            con.subtarget = jhose
            con = ulimb2_p.constraints.new('STRETCH_TO')
            con.name = "track"
            con.target = self.obj
            con.subtarget = jhose
            con.volume = 'NO_VOLUME'

            con = ulimb2_smoother_p.constraints.new('COPY_TRANSFORMS')
            con.name = "smoother"
            con.target = self.obj
            con.subtarget = flimb1_pos
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'SUM'
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = jhose_p.path_from_id() + '["smooth_bend"]'

            con = flimb1_pos_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = jhose
            con = flimb1_pos_p.constraints.new('DAMPED_TRACK')
            con.name = "track"
            con.target = self.obj
            con.subtarget = fhose
            con = flimb1_pos_p.constraints.new('STRETCH_TO')
            con.name = "track"
            con.target = self.obj
            con.subtarget = fhose
            con.volume = 'NO_VOLUME'

            con = flimb1_p.constraints.new('COPY_TRANSFORMS')
            con.name = "position"
            con.target = self.obj
            con.subtarget = flimb1_pos

            con = flimb1_smoother_p.constraints.new('COPY_TRANSFORMS')
            con.name = "smoother"
            con.target = self.obj
            con.subtarget = ulimb2
            fcurve = con.driver_add("influence")
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'SUM'
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = jhose_p.path_from_id() + '["smooth_bend"]'
            con = flimb1_smoother_p.constraints.new('STRETCH_TO')
            con.name = "track"
            con.target = self.obj
            con.subtarget = jhose
            con.volume = 'NO_VOLUME'

            con = flimb2_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = fhose
            con = flimb2_p.constraints.new('COPY_ROTATION')
            con.name = "twist"
            con.target = self.obj
            con.subtarget = elimb
            con = flimb2_p.constraints.new('DAMPED_TRACK')
            con.name = "track"
            con.target = self.obj
            con.subtarget = fhoseend
            con = flimb2_p.constraints.new('STRETCH_TO')
            con.name = "track"
            con.target = self.obj
            con.subtarget = fhoseend
            con.volume = 'NO_VOLUME'

            con = junc_p.constraints.new('COPY_TRANSFORMS')
            con.name = "bend"
            con.target = self.obj
            con.subtarget = self.org_bones[1]
            con.influence = 0.5

            con = uhose_par_p.constraints.new('COPY_ROTATION')
            con.name = "follow"
            con.target = self.obj
            con.subtarget = self.org_bones[0]
            con.influence = 1.0
            con = uhose_par_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = self.org_bones[0]
            con.influence = 1.0
            con = uhose_par_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = jhose
            con.influence = 0.5

            con = jhose_par_p.constraints.new('COPY_ROTATION')
            con.name = "follow"
            con.target = self.obj
            con.subtarget = junc
            con.influence = 1.0
            con = jhose_par_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = junc
            con.influence = 1.0

            con = fhose_par_p.constraints.new('COPY_ROTATION')
            con.name = "follow"
            con.target = self.obj
            con.subtarget = self.org_bones[1]
            con.influence = 1.0
            con = fhose_par_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = jhose
            con.influence = 1.0
            con = fhose_par_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = self.org_bones[2]
            con.influence = 0.5

            con = fhoseend_par_p.constraints.new('COPY_ROTATION')
            con.name = "follow"
            con.target = self.obj
            con.subtarget = self.org_bones[1]
            con.influence = 1.0
            con = fhoseend_par_p.constraints.new('COPY_LOCATION')
            con.name = "anchor"
            con.target = self.obj
            con.subtarget = self.org_bones[2]
            con.influence = 1.0

            # Layers
            if self.layers:
                uhoseend_p.bone.layers = self.layers
                uhose_p.bone.layers = self.layers
                jhose_p.bone.layers = self.layers
                fhose_p.bone.layers = self.layers
                fhoseend_p.bone.layers = self.layers
            else:
                layers = list(pb[self.org_bones[0]].bone.layers)
                uhoseend_p.bone.layers = layers
                uhose_p.bone.layers = layers
                jhose_p.bone.layers = layers
                fhose_p.bone.layers = layers
                fhoseend_p.bone.layers = layers

            # Create widgets
            create_sphere_widget(self.obj, uhoseend)
            create_sphere_widget(self.obj, uhose)
            create_sphere_widget(self.obj, jhose)
            create_sphere_widget(self.obj, fhose)
            create_sphere_widget(self.obj, fhoseend)

            return [uhoseend, uhose, jhose, fhose, fhoseend]
