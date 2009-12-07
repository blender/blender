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

import bpy
from Mathutils import Vector

# TODO, have these in a more general module
from rna_prop_ui import rna_idprop_ui_prop_get

empty_layer = [False] * 32


def auto_class(slots, name="ContainerClass", class_dict=None):

    if class_dict:
        class_dict = class_dict.copy()
    else:
        class_dict = {}

    class_dict["__slots__"] = tuple(slots)

    return type(name, (object,), class_dict)


def auto_class_instance(slots, name="ContainerClass", class_dict=None):
    return auto_class(slots, name, class_dict)()


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
            setattr(self, member + "_b", bbones.get(name, None))
            setattr(self, member + "_p", pbones.get(name, None))
            setattr(self, member + "_e", ebones.get(name, None))


def _bone_class_instance_rename(self, attr, new_name):
    ''' Rename bones, editmode only
    '''

    if self.obj.mode != 'EDIT':
        raise Exception("Only rename in editmode supported")

    ebone = getattr(self, attr + "_e")
    ebone.name = new_name

    # we may not get what is asked for so get the name from the editbone
    setattr(self, attr, ebone.name)


def _bone_class_instance_copy(self, from_prefix="", to_prefix=""):
    from_name_ls = []
    new_name_ls = []
    new_slot_ls = []

    for attr in self.attr_names:
        bone_name_orig = getattr(self, attr)
        ebone = getattr(self, attr + "_e")
        # orig_names[attr] = bone_name_orig

        # insert prefix
        if from_prefix:
            bone_name = from_prefix + bone_name_orig
            ebone.name = bone_name
            bone_name = ebone.name # cant be sure we get what we ask for
        else:
            bone_name = bone_name_orig

        setattr(self, attr, bone_name)

        new_slot_ls.append(attr)
        from_name_ls.append(bone_name)
        bone_name_orig = bone_name_orig.replace("ORG-", "") # XXX - we need a better way to do this
        new_name_ls.append(to_prefix + bone_name_orig)

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


def bone_class_instance(obj, slots, name="BoneContainer"):
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
        "update": _bone_class_instance_update, \
        "rename": _bone_class_instance_rename, \
        "names": _bone_class_instance_names, \
        "copy": _bone_class_instance_copy, \
        "blend": _bone_class_instance_blend, \
    }

    instance = auto_class_instance(slots, name, class_dict)
    return instance


def get_bone_data(obj, bone_name):
    arm = obj.data
    pbone = obj.pose.bones[bone_name]
    if obj.mode == 'EDIT':
        bone = arm.edit_bones[bone_name]
    else:
        bone = arm.bones[bone_name]

    return arm, pbone, bone


def copy_bone_simple(arm, from_bone, name, parent=False):
    ebone = arm.edit_bones[from_bone]
    ebone_new = arm.edit_bones.new(name)

    if parent:
        ebone_new.connected = ebone.connected
        ebone_new.parent = ebone.parent

    ebone_new.head = ebone.head
    ebone_new.tail = ebone.tail
    ebone_new.roll = ebone.roll
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


def blend_bone_list(obj, apply_bones, from_bones, to_bones, target_bone=None, target_prop="blend"):

    if obj.mode == 'EDIT':
        raise Exception("blending cant be called in editmode")

    # setup the blend property
    if target_bone is None:
        target_bone = apply_bones[-1] # default to the last bone

    prop_pbone = obj.pose.bones[target_bone]
    if prop_pbone.get(target_bone, None) is None:
        prop = rna_idprop_ui_prop_get(prop_pbone, target_prop, create=True)
        prop_pbone[target_prop] = 0.5
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

    driver_path = prop_pbone.path_to_id() + ('["%s"]' % target_prop)

    def blend_target(driver):
        tar = driver.targets.new()
        tar.name = target_bone
        tar.id_type = 'OBJECT'
        tar.id = obj
        tar.rna_path = driver_path

    def blend_location(new_pbone, from_bone_name, to_bone_name):
        con = new_pbone.constraints.new('COPY_LOCATION')
        con.target = obj
        con.subtarget = from_bone_name

        con = new_pbone.constraints.new('COPY_LOCATION')
        con.target = obj
        con.subtarget = to_bone_name

        fcurve = con.driver_add("influence", 0)
        driver = fcurve.driver
        driver.type = 'AVERAGE'
        fcurve.modifiers.remove(0) # grr dont need a modifier

        blend_target(driver)

    def blend_rotation(new_pbone, from_bone_name, to_bone_name):
        con = new_pbone.constraints.new('COPY_ROTATION')
        con.target = obj
        con.subtarget = from_bone_name

        con = new_pbone.constraints.new('COPY_ROTATION')
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

        if not new_pbone.bone.connected:
            blend_location(new_pbone, from_bone_name, to_bone_name)

        blend_rotation(new_pbone, from_bone_name, to_bone_name)


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


def add_pole_target_bone(obj, base_name, name, mode='CROSS'):
    '''
    Does not actually create a poll target, just the bone to use as a poll target
    '''
    mode_orig = obj.mode
    bpy.ops.object.mode_set(mode='EDIT')

    arm = obj.data

    poll_ebone = arm.edit_bones.new(base_name + "_poll")
    base_ebone = arm.edit_bones[base_name]
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
        offset = base_dir.copy().normalize() - parent_dir.copy().normalize()
        offset.length = distance
    else:
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


def generate_rig(context, obj_orig, prefix="ORG-"):
    from collections import OrderedDict

    global_undo = context.user_preferences.edit.global_undo
    context.user_preferences.edit.global_undo = False

    bpy.ops.object.mode_set(mode='OBJECT')


    # copy object and data
    obj_orig.selected = False
    obj = obj_orig.copy()
    obj.data = obj_orig.data.copy()
    scene = context.scene
    scene.objects.link(obj)
    scene.objects.active = obj
    obj.selected = True

    arm = obj.data

    # original name mapping
    base_names = {}

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone_name = bone.name
        bone.name = prefix + bone_name
        base_names[bone.name] = bone_name # new -> old mapping
    bpy.ops.object.mode_set(mode='OBJECT')

    # key: bone name
    # value: {type:definition, ...}
    #    where type is the submodule name - leg, arm etc
    #    and definition is a list of bone names
    bone_definitions = {}

    # key: bone name
    # value: [functions, ...]
    #    each function is from the module. eg leg.ik, arm.main
    bone_typeinfos = {}

    # inspect all bones and assign their definitions before modifying
    for pbone in obj.pose.bones:
        bone_name = pbone.name
        bone_type = obj.pose.bones[bone_name].get("type", "")
        bone_type_list = [bt for bt in bone_type.replace(",", " ").split()]

        for bone_type in bone_type_list:
            type_pair = bone_type.split(".")

            # 'leg.ik' will look for an ik function in the leg module
            # 'leg' will look up leg.main
            if len(type_pair) == 1:
                type_pair = type_pair[0], "main"

            submod_name, func_name = type_pair

            # from rigify import leg
            submod = __import__(name="%s.%s" % (__package__, submod_name), fromlist=[submod_name])
            reload(submod)

            bone_def_dict = bone_definitions.setdefault(bone_name, {})

            # Only calculate bone definitions once
            if submod_name not in bone_def_dict:
                metarig_definition_func = getattr(submod, "metarig_definition")
                bone_def_dict[submod_name] = metarig_definition_func(obj, bone_name)


            bone_typeinfo = bone_typeinfos.setdefault(bone_name, [])

            type_func = getattr(submod, func_name)
            bone_typeinfo.append((submod_name, type_func))


    # sort bones, not needed but gives more pradictable execution which may be useful in rare cases
    bones_sorted = obj.pose.bones.values()
    bones_sorted.sort(key=lambda pbone: pbone.name) # first sort by names
    bones_sorted.sort(key=lambda pbone: - len(pbone.parent_recursive)) # children before parents

    # now we have all the info about bones we can start operating on them
    # for pbone in obj.pose.bones:
    for pbone in bones_sorted:
        bone_name = pbone.name

        if bone_name not in bone_typeinfos:
            continue
        
        bone_def_dict = bone_definitions[bone_name]

        # Only blend results from the same submodule, eg.
        #    leg.ik and arm.fk could not be blended.
        results = OrderedDict()

        for submod_name, type_func in bone_typeinfos[bone_name]:
            # this bones definition of the current typeinfo
            definition = bone_def_dict[submod_name]

            bpy.ops.object.mode_set(mode='EDIT')
            ret = type_func(obj, definition, base_names)
            bpy.ops.object.mode_set(mode='OBJECT')

            if ret:
                result_submod = results.setdefault(submod_name, [])

                if result_submod and len(result_submod[-1]) != len(ret):
                    raise Exception("bone lists not compatible: %s, %s" % (result_submod[-1], ret))

                result_submod.append(ret)

        for result_submod in results.values():
            # blend 2 chains
            definition = bone_def_dict[submod_name]

            if len(result_submod) == 2:
                blend_bone_list(obj, definition, result_submod[0], result_submod[1])

    # needed to update driver deps
    # context.scene.update()

    # Only for demo'ing

    # obj.restrict_view = True
    obj.data.draw_axes = False

    context.user_preferences.edit.global_undo = global_undo

    return obj


def write_meta_rig(obj, func_name="metarig_template"):
    ''' Must be in editmode
    '''
    code = []

    code.append("def %s():" % func_name)
    code.append("    # generated by rigify.write_meta_rig")
    bpy.ops.object.mode_set(mode='EDIT')
    code.append("    bpy.ops.object.mode_set(mode='EDIT')")

    code.append("    obj = bpy.context.object")
    code.append("    arm = obj.data")

    arm = obj.data
    # write parents first
    bones = [(len(bone.parent_recursive), bone.name) for bone in arm.edit_bones]
    bones.sort(key=lambda item: item[0])
    bones = [item[1] for item in bones]


    for bone_name in bones:
        bone = arm.edit_bones[bone_name]
        code.append("    bone = arm.edit_bones.new('%s')" % bone.name)
        code.append("    bone.head[:] = %.4f, %.4f, %.4f" % bone.head.toTuple(4))
        code.append("    bone.tail[:] = %.4f, %.4f, %.4f" % bone.tail.toTuple(4))
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


def generate_test(context):
    import os
    new_objects = []

    scene = context.scene

    def create_empty_armature(name):
        obj_new = bpy.data.add_object('ARMATURE', name)
        armature = bpy.data.add_armature(name)
        obj_new.data = armature
        scene.objects.link(obj_new)
        scene.objects.active = obj_new

    files = os.listdir(os.path.dirname(__file__))
    for f in files:
        if f.startswith("_"):
            continue

        if not f.endswith(".py"):
            continue

        module_name = f[:-3]
        submodule = __import__(name="%s.%s" % (__package__, module_name), fromlist=[module_name])

        metarig_template = getattr(submodule, "metarig_template", None)

        if metarig_template:
            create_empty_armature("meta_" + module_name) # sets active
            metarig_template()
            obj = context.object
            obj_new = generate_rig(context, obj)

            new_objects.append((obj, obj_new))
        else:
            print("note: rig type '%s' has no metarig_template(), can't test this", module_name)

    return new_objects


def generate_test_all(context):
    import rigify
    import graphviz_export
    import os
    reload(rigify)
    reload(graphviz_export)

    new_objects = rigify.generate_test(context)

    base_name = os.path.splitext(bpy.data.filename)[0]
    for obj, obj_new in new_objects:
        for obj in (obj, obj_new):
            fn = base_name + "-" + bpy.utils.clean_name(obj.name)

            path_dot = fn + ".dot"
            path_png = fn + ".png"
            saved = graphviz_export.graph_armature(obj, path_dot, CONSTRAINTS=True, DRIVERS=True)

            #if saved:
            #    os.system("dot -Tpng %s > %s; eog %s" % (path_dot, path_png, path_png))

    i = 0
    for obj, obj_new in new_objects:
        obj.data.drawtype = 'STICK'
        obj.location[1] += i
        obj_new.location[1] += i
        obj_new.selected = False
        obj.selected = True
        i += 4


if __name__ == "__main__":
    generate_rig(bpy.context, bpy.context.object)
