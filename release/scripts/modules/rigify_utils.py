# ##### BEGIN GPL LICENSE BLOCK #####
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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# rigify its self does not depend on this module, however some of the
# rigify templates use these utility functions.
#
# So even though this can be for general purpose use, this module was created
# for rigify so in some cases seemingly generic functions make assumptions
# that a generic function would need to check for.

import bpy
from Mathutils import Vector
from rna_prop_ui import rna_idprop_ui_prop_get

DELIMITER = '-._'
EMPTY_LAYER = [False] * 32


def add_stretch_to(obj, from_name, to_name, name):
    '''
    Adds a bone that stretches from one to another
    '''

    mode_orig = obj.mode
    bpy.ops.object.mode_set(mode='EDIT')

    arm = obj.data
    stretch_ebone = arm.edit_bones.new(name)
    stretch_name = stretch_ebone.name
    del name

    head = stretch_ebone.head = arm.edit_bones[from_name].head.copy()
    #tail = stretch_ebone.tail = arm.edit_bones[to_name].head.copy()

    # annoying exception for zero length bones, since its using stretch_to the rest pose doesnt really matter
    #if (head - tail).length < 0.1:
    if 1:
        tail = stretch_ebone.tail = arm.edit_bones[from_name].tail.copy()


    # Now for the constraint
    bpy.ops.object.mode_set(mode='OBJECT')

    stretch_pbone = obj.pose.bones[stretch_name]

    con = stretch_pbone.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = from_name

    con = stretch_pbone.constraints.new('STRETCH_TO')
    con.target = obj
    con.subtarget = to_name
    con.original_length = (head - tail).length
    con.keep_axis = 'PLANE_X'
    con.volume = 'NO_VOLUME'

    bpy.ops.object.mode_set(mode=mode_orig)

    return stretch_name


def copy_bone_simple(arm, from_bone, name, parent=False):
    ebone = arm.edit_bones[from_bone]
    ebone_new = arm.edit_bones.new(name)

    if parent:
        ebone_new.connected = ebone.connected
        ebone_new.parent = ebone.parent

    ebone_new.head = ebone.head
    ebone_new.tail = ebone.tail
    ebone_new.roll = ebone.roll
    ebone_new.layer = list(ebone.layer)
    return ebone_new


def copy_bone_simple_list(arm, from_bones, to_bones, parent=False):

    if len(from_bones) != len(to_bones):
        raise Exception("bone list sizes must match")

    copy_bones = [copy_bone_simple(arm, bone_name, to_bones[i], True) for i, bone_name in enumerate(from_bones)]

    # now we need to re-parent
    for ebone in copy_bones:
        parent = ebone.parent
        if parent:
            try:
                i = from_bones.index(parent.name)
            except:
                i = -1

            if i == -1:
                ebone.parent = None
            else:
                ebone.parent = copy_bones[i]

    return copy_bones


def blend_bone_list(obj, apply_bones, from_bones, to_bones, target_bone=None, target_prop="blend", blend_default=0.5):

    if obj.mode == 'EDIT':
        raise Exception("blending cant be called in editmode")

    if len(apply_bones) != len(from_bones):
        raise Exception("lists differ in length (from -> apply): \n\t%s\n\t%s" % (from_bones, apply_bones))
    if len(apply_bones) != len(to_bones):
        raise Exception("lists differ in length (to -> apply): \n\t%s\n\t%s" % (to_bones, apply_bones))

    # setup the blend property
    if target_bone is None:
        target_bone = apply_bones[-1] # default to the last bone

    prop_pbone = obj.pose.bones[target_bone]
    if prop_pbone.get(target_bone) is None:
        prop = rna_idprop_ui_prop_get(prop_pbone, target_prop, create=True)
        prop_pbone[target_prop] = blend_default
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

    driver_path = prop_pbone.path_to_id() + ('["%s"]' % target_prop)

    def blend_target(driver):
        var = driver.variables.new()
        var.name = target_bone
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = driver_path

    def blend_transforms(new_pbone, from_bone_name, to_bone_name):
        con = new_pbone.constraints.new('COPY_TRANSFORMS')
        con.target = obj
        con.subtarget = from_bone_name

        con = new_pbone.constraints.new('COPY_TRANSFORMS')
        con.target = obj
        con.subtarget = to_bone_name

        fcurve = con.driver_add("influence", 0)
        driver = fcurve.driver
        driver.type = 'AVERAGE'
        fcurve.modifiers.remove(0) # grr dont need a modifier

        blend_target(driver)

    for i, new_bone_name in enumerate(apply_bones):
        from_bone_name = from_bones[i]
        to_bone_name = to_bones[i]

        # allow skipping some bones by having None in the list
        if None in (new_bone_name, from_bone_name, to_bone_name):
            continue

        new_pbone = obj.pose.bones[new_bone_name]

        blend_transforms(new_pbone, from_bone_name, to_bone_name)



def add_pole_target_bone(obj, base_bone_name, name, mode='CROSS'):
    '''
    Does not actually create a poll target, just the bone to use as a poll target
    '''
    mode_orig = obj.mode
    bpy.ops.object.mode_set(mode='EDIT')

    arm = obj.data

    poll_ebone = arm.edit_bones.new(name)
    base_ebone = arm.edit_bones[base_bone_name]
    poll_name = poll_ebone.name
    parent_ebone = base_ebone.parent

    base_head = base_ebone.head.copy()
    base_tail = base_ebone.tail.copy()
    base_dir = base_head - base_tail

    parent_head = parent_ebone.head.copy()
    parent_tail = parent_ebone.tail.copy()
    parent_dir = parent_head - parent_tail

    distance = (base_dir.length + parent_dir.length)

    if mode == 'CROSS':
        # direction from the angle of the joint
        offset = base_dir.copy().normalize() - parent_dir.copy().normalize()
        offset.length = distance
    elif mode == 'ZAVERAGE':
        # between both bones Z axis
        z_axis_a = base_ebone.matrix.copy().rotation_part() * Vector(0.0, 0.0, -1.0)
        z_axis_b = parent_ebone.matrix.copy().rotation_part() * Vector(0.0, 0.0, -1.0)
        offset = (z_axis_a + z_axis_b).normalize() * distance
    else:
        # preset axis
        offset = Vector(0, 0, 0)
        if mode[0] == "+":
            val = distance
        else:
            val = - distance

        setattr(offset, mode[1].lower(), val)

    poll_ebone.head = base_head + offset
    poll_ebone.tail = base_head + (offset * (1.0 - (1.0 / 4.0)))

    bpy.ops.object.mode_set(mode=mode_orig)

    return poll_name


def get_side_name(name):
    '''
    Returns the last part of a string (typically a bone's name) indicating
    whether it is a a left or right (or center, or whatever) bone.
    Returns an empty string if nothing is found.
    '''
    if name[-2] in DELIMITER:
        return name[-2:]
    else:
        return ""


def get_base_name(name):
    '''
    Returns the part of a string (typically a bone's name) corresponding to it's
    base name (no sidedness, no ORG prefix).
    '''
    if name[-2] in DELIMITER:
        return name[:-2]
    else:
        return name


def write_meta_rig(obj, func_name="metarig_template"):
    '''
    Write a metarig as a python script, this rig is to have all info needed for
    generating the real rig with rigify.
    '''
    code = []

    code.append("def %s():" % func_name)
    code.append("    # generated by rigify.write_meta_rig")
    bpy.ops.object.mode_set(mode='EDIT')
    code.append("    bpy.ops.object.mode_set(mode='EDIT')")

    code.append("    obj = bpy.context.active_object")
    code.append("    arm = obj.data")

    arm = obj.data
    # write parents first
    bones = [(len(bone.parent_recursive), bone.name) for bone in arm.edit_bones]
    bones.sort(key=lambda item: item[0])
    bones = [item[1] for item in bones]


    for bone_name in bones:
        bone = arm.edit_bones[bone_name]
        code.append("    bone = arm.edit_bones.new('%s')" % bone.name)
        code.append("    bone.head[:] = %.4f, %.4f, %.4f" % bone.head.to_tuple(4))
        code.append("    bone.tail[:] = %.4f, %.4f, %.4f" % bone.tail.to_tuple(4))
        code.append("    bone.roll = %.4f" % bone.roll)
        code.append("    bone.connected = %s" % str(bone.connected))
        if bone.parent:
            code.append("    bone.parent = arm.edit_bones['%s']" % bone.parent.name)

    bpy.ops.object.mode_set(mode='OBJECT')
    code.append("")
    code.append("    bpy.ops.object.mode_set(mode='OBJECT')")

    for bone_name in bones:
        pbone = obj.pose.bones[bone_name]
        pbone_written = False

        # Only 1 level of props, simple types supported
        for key, value in pbone.items():
            if key.startswith("_"):
                continue

            if type(value) not in (float, str, int):
                print("Unsupported ID Prop:", str((key, value)))
                continue

            if type(value) == str:
                value = "'" + value + "'"

            if not pbone_written: # only write bones we need
                code.append("    pbone = obj.pose.bones['%s']" % bone_name)

            code.append("    pbone['%s'] = %s" % (key, value))

    return "\n".join(code)


# *** bone class collection ***


def bone_class_instance(obj, slots, name="BoneContainer"):
    '''
    bone collection utility class to help manage cases with
    edit/pose/bone bones where switching modes can invalidate some of the members.

    there are also utility functions for manipulating all members.
    '''

    if len(slots) != len(set(slots)):
        raise Exception("duplicate entries found %s" % attr_names)

    attr_names = tuple(slots) # dont modify the original
    slots = list(slots) # dont modify the original
    for i in range(len(slots)):
        member = slots[i]
        slots.append(member + "_b") # bone bone
        slots.append(member + "_p") # pose bone
        slots.append(member + "_e") # edit bone

    class_dict = { \
        "obj": obj, \
        "attr_names": attr_names, \
        "attr_initialize": _bone_class_instance_attr_initialize, \
        "update": _bone_class_instance_update, \
        "rename": _bone_class_instance_rename, \
        "names": _bone_class_instance_names, \
        "copy": _bone_class_instance_copy, \
        "blend": _bone_class_instance_blend, \
    }

    instance = auto_class_instance(slots, name, class_dict)
    return instance


def auto_class(slots, name="ContainerClass", class_dict=None):

    if class_dict:
        class_dict = class_dict.copy()
    else:
        class_dict = {}

    class_dict["__slots__"] = tuple(slots)

    return type(name, (object,), class_dict)


def auto_class_instance(slots, name="ContainerClass", class_dict=None):
    return auto_class(slots, name, class_dict)()


def _bone_class_instance_attr_initialize(self, attr_names, bone_names):
    ''' Initializes attributes, both lists must be aligned
    '''
    for attr in self.attr_names:
        i = attr_names.index(attr)
        setattr(self, attr, bone_names[i])

    self.update()


def _bone_class_instance_update(self):
    ''' Re-Assigns bones from the blender data
    '''
    arm = self.obj.data
    bbones = arm.bones
    pbones = self.obj.pose.bones
    ebones = arm.edit_bones

    for member in self.attr_names:
        name = getattr(self, member, None)
        if name is not None:
            setattr(self, member + "_b", bbones.get(name))
            setattr(self, member + "_p", pbones.get(name))
            setattr(self, member + "_e", ebones.get(name))


def _bone_class_instance_rename(self, attr, new_name):
    ''' Rename bones, editmode only
    '''

    if self.obj.mode != 'EDIT':
        raise Exception("Only rename in editmode supported")

    ebone = getattr(self, attr + "_e")
    ebone.name = new_name

    # we may not get what is asked for so get the name from the editbone
    setattr(self, attr, ebone.name)


def _bone_class_instance_copy(self, from_fmt="%s", to_fmt="%s", exclude_attrs=(), base_names=None):
    from_name_ls = []
    new_name_ls = []
    new_slot_ls = []

    for attr in self.attr_names:

        if attr in exclude_attrs:
            continue

        bone_name_orig = getattr(self, attr)
        ebone = getattr(self, attr + "_e")
        # orig_names[attr] = bone_name_orig

        # insert formatting
        if from_fmt != "%s":
            bone_name = from_fmt % bone_name_orig
            ebone.name = bone_name
            bone_name = ebone.name # cant be sure we get what we ask for
        else:
            bone_name = bone_name_orig

        setattr(self, attr, bone_name)

        new_slot_ls.append(attr)
        from_name_ls.append(bone_name)
        if base_names:
            bone_name_orig = base_names[bone_name_orig]
        new_name_ls.append(to_fmt % bone_name_orig)

    new_bones = copy_bone_simple_list(self.obj.data, from_name_ls, new_name_ls, True)
    new_bc = bone_class_instance(self.obj, new_slot_ls)

    for i, attr in enumerate(new_slot_ls):
        ebone = new_bones[i]
        setattr(new_bc, attr + "_e", ebone)
        setattr(new_bc, attr, ebone.name)

    return new_bc


def _bone_class_instance_names(self):
    return [getattr(self, attr) for attr in self.attr_names]


def _bone_class_instance_blend(self, from_bc, to_bc, target_bone=None, target_prop="blend"):
    '''
    Use for blending bone chains.

    blend_target = (bone_name, bone_property)
    default to the last bone, blend prop

    XXX - toggles editmode, need to re-validate all editbones :(
    '''

    if self.attr_names != from_bc.attr_names or self.attr_names != to_bc.attr_names:
        raise Exception("can only blend between matching chains")

    apply_bones = [getattr(self, attr) for attr in self.attr_names]
    from_bones = [getattr(from_bc, attr) for attr in from_bc.attr_names]
    to_bones = [getattr(to_bc, attr) for attr in to_bc.attr_names]

    blend_bone_list(self.obj, apply_bones, from_bones, to_bones, target_bone, target_prop)
