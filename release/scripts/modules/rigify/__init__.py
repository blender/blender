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
SPECIAL_TYPES = "root",
LAYER_TYPES = "main", "extra", "ik", "fk"

ORG_LAYERS = [n==31 for n in range(0,32)]
MCH_LAYERS = [n==30 for n in range(0,32)]
DEF_LAYERS = [n==29 for n in range(0,32)]
ROOT_LAYERS = [n==28 for n in range(0,32)] 

ORG_PREFIX = "ORG-"
MCH_PREFIX = "MCH-"
DEF_PREFIX = "DEF-"

WGT_PREFIX = "WGT-"



class RigifyError(Exception):
    """Exception raised for errors in the metarig.
    """

    def __init__(self, message):
        self.message = message

    def __str__(self):
        return repr(self.message)


def submodule_func_from_type(bone_type):
    type_pair = bone_type.split(".")

    # 'leg.ik' will look for an ik function in the leg module
    # 'leg' will look up leg.main
    if len(type_pair) == 1:
        type_pair = type_pair[0], "main"

    type_name, func_name = type_pair

    # from rigify import leg
    try:
        submod = __import__(name="%s.%s" % (__package__, type_name), fromlist=[type_name])
    except ImportError:
        raise RigifyError("python module for type '%s' not found" % type_name)

    reload(submod)
    return type_name, submod, getattr(submod, func_name)


def get_submodule_types():
    import os
    submodules = []
    files = os.listdir(os.path.dirname(__file__))
    for f in files:
        if not f.startswith("_") and f.endswith(".py"):
            submodules.append(f[:-3])

    return sorted(submodules)


def get_bone_type_options(pbone, type_name):
    options = {}
    bone_name = pbone.name
    for key, value in pbone.items():
        key_pair = key.rsplit(".")
        # get all bone properties
        """"
        if key_pair[0] == type_name:
            if len(key_pair) != 2:
                raise RigifyError("option error for bone '%s', property name was not a pair '%s'" % (bone_name, key_pair))
            options[key_pair[1]] = value
        """
        options[key] = value

    return options


def get_layer_dict(options):
    '''
    Extracts layer info from a bone options dict
    defaulting to the layer index if not set.
    '''
    layer_default = [False] * 32
    result = {}
    for i, layer_type in enumerate(LAYER_TYPES):
        # no matter if its not defined
        layer_index = options.get("layer_" + layer_type, i + 2)
        layer = layer_default[:]
        layer[layer_index-1] = True
        result[layer_type] = layer
    return result


def validate_rig(context, obj):
    '''
    Makes no changes
    only runs the metarig definitions and reports errors
    '''
    type_found = False

    for pbone in obj.pose.bones:
        bone_name = pbone.name
        bone_type = pbone.get("type", "")

        if bone_type:
            bone_type_list = [bt for bt in bone_type.replace(",", " ").split()]
        else:
            bone_type_list = []

        for bone_type in bone_type_list:
            if bone_type.split(".")[0] in SPECIAL_TYPES:
                continue

            type_name, submod, type_func = submodule_func_from_type(bone_type)
            reload(submod)
            submod.metarig_definition(obj, bone_name)
            type_found = True

            get_bone_type_options(pbone, bone_type)

        # missing, - check for duplicate root bone.

    if not type_found:
        raise RigifyError("This rig has no 'type' properties defined on any pose bones, nothing to do")


def generate_rig(context, obj_orig, prefix="ORG-", META_DEF=True):
    '''
    Main function for generating
    '''
    from collections import OrderedDict
    import rigify_utils
    reload(rigify_utils)
    
    print("Begin...")

    # Not needed but catches any errors before duplicating
    validate_rig(context, obj_orig)

    global_undo = context.user_preferences.edit.global_undo
    context.user_preferences.edit.global_undo = False
    mode_orig = context.mode
    rest_backup = obj_orig.data.pose_position
    obj_orig.data.pose_position = 'REST'

    bpy.ops.object.mode_set(mode='OBJECT')

    scene = context.scene

    # Check if the generated rig already exists, so we can
    # regenerate in the same object.  If not, create a new
    # object to generate the rig in.
    print("Fetch rig.")
    try:
        name = obj_orig["rig_object_name"]
    except KeyError:
        name = "rig"
        
    try:
        obj = scene.objects[name]
    except KeyError:
        obj = bpy.data.objects.new(name, type='ARMATURE')
        obj.data = bpy.data.armatures.new(name)
        scene.objects.link(obj)
        
    obj.data.pose_position = 'POSE'
    
    # Get rid of anim data in case the rig already existed
    print("Clear rig animation data.")
    obj.animation_data_clear()
        
    # Select generated rig object
    obj_orig.selected = False
    obj.selected = True
    scene.objects.active = obj
    
    # Remove all bones from the generated rig armature.
    bpy.ops.object.mode_set(mode='EDIT')
    for bone in obj.data.edit_bones:
        obj.data.edit_bones.remove(bone)
    bpy.ops.object.mode_set(mode='OBJECT')
    
    # Create temporary duplicates for merging
    temp_rig_1 = obj_orig.copy()
    temp_rig_1.data = obj_orig.data.copy()
    scene.objects.link(temp_rig_1)
    
    temp_rig_2 = obj_orig.copy()
    temp_rig_2.data = obj.data
    scene.objects.link(temp_rig_2)
    
    # Select the temp rigs for merging
    for objt in scene.objects:
        objt.selected = False # deselect all objects
    temp_rig_1.selected = True
    temp_rig_2.selected = True
    scene.objects.active = temp_rig_2
    
    # Merge the temporary rigs
    bpy.ops.object.join(context)
    
    # Delete the second temp rig
    bpy.ops.object.delete()
    
    # Select the generated rig
    for objt in scene.objects:
        objt.selected = False # deselect all objects
    obj.selected = True
    scene.objects.active = obj
    
    # Copy over the pose_bone properties
    for bone in obj_orig.pose.bones:
        bone_gen = obj.pose.bones[bone.name]
        
        # Rotation mode and transform locks
        bone_gen.rotation_mode     = bone.rotation_mode
        bone_gen.lock_rotation     = tuple(bone.lock_rotation)
        bone_gen.lock_rotation_w   = bone.lock_rotation_w
        bone_gen.lock_rotations_4d = bone.lock_rotations_4d
        bone_gen.lock_location     = tuple(bone.lock_location)
        bone_gen.lock_scale        = tuple(bone.lock_scale)
        
        # Custom properties
        for prop in bone.keys():
            bone_gen[prop] = bone[prop]
    
    # Copy over bone properties
    for bone in obj_orig.data.bones:
        bone_gen = obj.data.bones[bone.name]
        
        # B-bone stuff
        bone_gen.bbone_segments = bone.bbone_segments
        bone_gen.bbone_in = bone.bbone_in
        bone_gen.bbone_out = bone.bbone_out
    
    
    # Create proxy deformation rig
    # TODO: remove this
    if META_DEF:
        obj_def = obj_orig.copy()
        obj_def.data = obj_orig.data.copy()
        scene.objects.link(obj_def)
    
    scene.update()
    print("On to the real work.")

    arm = obj.data

    # prepend the ORG prefix to the bones, and create the base_names mapping
    base_names = {}
    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone_name = bone.name
        bone.name = ORG_PREFIX + bone_name
        base_names[bone.name] = bone_name

    # create root_bone
    bpy.ops.object.mode_set(mode='EDIT')
    edit_bone = obj.data.edit_bones.new("root")
    root_bone = edit_bone.name
    edit_bone.head = (0.0, 0.0, 0.0)
    edit_bone.tail = (0.0, 1.0, 0.0)
    edit_bone.roll = 0.0
    edit_bone.layer = ROOT_LAYERS
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

    # key: bone name
    # value: [new_bone_name, ...]
    #   where each bone with a 'type' stores a list of bones that it created
    #   ...needed so we can override the root parent
    bone_genesis = {}


    # inspect all bones and assign their definitions before modifying
    for pbone in obj.pose.bones:
        bone_name = pbone.name
        bone_type = pbone.get("type", "")
        if bone_type:
            bone_type_list = [bt for bt in bone_type.replace(",", " ").split()]

            # not essential but means running autorig again wont do anything
            del pbone["type"]
        else:
            bone_type_list = []

        for bone_type in bone_type_list:
            type_name, submod, type_func = submodule_func_from_type(bone_type)
            reload(submod)

            bone_def_dict = bone_definitions.setdefault(bone_name, {})

            # Only calculate bone definitions once
            if type_name not in bone_def_dict:
                bone_def_dict[type_name] = submod.metarig_definition(obj, bone_name)

            bone_typeinfo = bone_typeinfos.setdefault(bone_name, [])
            bone_typeinfo.append((type_name, type_func))


    # sort bones, not needed but gives more pradictable execution which may be useful in rare cases
    bones_sorted = obj.pose.bones.values()
    bones_sorted.sort(key=lambda pbone: pbone.name) # first sort by names
    bones_sorted.sort(key=lambda pbone: len(pbone.parent_recursive)) # parents before children

    # now we have all the info about bones we can start operating on them
    # for pbone in obj.pose.bones:
    for pbone in bones_sorted:
        bone_name = pbone.name
        print(bone_name)
        if bone_name not in bone_typeinfos:
            continue

        bone_def_dict = bone_definitions[bone_name]

        # Only blend results from the same submodule, eg.
        #    leg.ik and arm.fk could not be blended.
        results = OrderedDict()

        bone_names_pre = {bone.name for bone in arm.bones}

        for type_name, type_func in bone_typeinfos[bone_name]:
            print("    " + type_name)
            # this bones definition of the current typeinfo
            definition = bone_def_dict[type_name]
            options = get_bone_type_options(pbone, type_name)

            bpy.ops.object.mode_set(mode='EDIT')
            ret = type_func(obj, definition, base_names, options)
            bpy.ops.object.mode_set(mode='OBJECT')

            if ret:
                result_submod = results.setdefault(type_name, [])

                if result_submod and len(result_submod[-1]) != len(ret):
                    raise RigifyError("bone lists not compatible: %s, %s" % (result_submod[-1], ret))

                result_submod.append(ret)

        for result_submod in results.values():
            # blend 2 chains
            definition = bone_def_dict[type_name]

            if len(result_submod) == 2:
                blend_bone_list(obj, definition, result_submod[0], result_submod[1], target_bone=bone_name)


        bone_names_post = {bone.name for bone in arm.bones}

        # Store which bones were created from this one
        bone_genesis[bone_name] = list(bone_names_post - bone_names_pre)

    # need a reverse lookup on bone_genesis so as to know immediately
    # where a bone comes from
    bone_genesis_reverse = {}
    '''
    for bone_name, bone_children in bone_genesis.items():
        for bone_child_name in bone_children:
            bone_genesis_reverse[bone_child_name] = bone_name
    '''


    if root_bone:
        # assign all new parentless bones to this

        bpy.ops.object.mode_set(mode='EDIT')
        root_ebone = arm.edit_bones[root_bone]
        for ebone in arm.edit_bones:
            bone_name = ebone.name
            if ebone.parent is None:
                ebone.parent = root_ebone
            '''
            if ebone.parent is None and bone_name not in base_names:
                # check for override
                bone_creator = bone_genesis_reverse[bone_name]
                pbone_creator = obj.pose.bones[bone_creator]
                root_bone_override = pbone_creator.get("root", "")

                if root_bone_override:
                    root_ebone_tmp = arm.edit_bones[root_bone_override]
                else:
                    root_ebone_tmp = root_ebone

                ebone.connected = False
                ebone.parent = root_ebone_tmp
            '''

        bpy.ops.object.mode_set(mode='OBJECT')


    if META_DEF:
        # for pbone in obj_def.pose.bones:
        for bone_name, bone_name_new in base_names.items():
            #pbone_from = bone_name
            pbone = obj_def.pose.bones[bone_name_new]

            con = pbone.constraints.new('COPY_ROTATION')
            con.target = obj
            con.subtarget = bone_name

            if not pbone.bone.connected:
                con = pbone.constraints.new('COPY_LOCATION')
                con.target = obj
                con.subtarget = bone_name

        # would be 'REST' from when copied
        obj_def.data.pose_position = 'POSE'

    # todo - make a more generic system?
    layer_tot = [False] * 32
    layer_last = layer_tot[:]
    layer_last[31] = True
    layer_second_last = layer_tot[:]
    layer_second_last[30] = True

    for bone_name, bone in arm.bones.items():
        bone.deform = False  # Non DEF bones shouldn't deform
        if bone_name.startswith(ORG_PREFIX):
            bone.layer = ORG_LAYERS
        elif bone_name.startswith(MCH_PREFIX): # XXX fixme
            bone.layer = MCH_LAYERS
        elif bone_name.startswith(DEF_PREFIX): # XXX fixme
            bone.layer = DEF_LAYERS
            bone.deform = True
        else:
            # Assign bone appearance if there is a widget for it
            obj.pose.bones[bone_name].custom_shape = context.scene.objects.get(WGT_PREFIX+bone_name)

        layer_tot[:] = [max(lay) for lay in zip(layer_tot, bone.layer)]

    # Only for demo'ing
    layer_show = [a and not (b or c or d) for a,b,c,d in zip(layer_tot, ORG_LAYERS, MCH_LAYERS, DEF_LAYERS)]
    arm.layer = layer_show


    # obj.restrict_view = True
    obj.data.draw_axes = False

    bpy.ops.object.mode_set(mode=mode_orig)
    obj_orig.data.pose_position = rest_backup
    obj.data.pose_position = 'POSE'
    obj_orig.data.pose_position = 'POSE'
    context.user_preferences.edit.global_undo = global_undo
    
    print("Done.\n")

    return obj


def generate_test(context, metarig_type="", GENERATE_FINAL=True):
    import os
    new_objects = []

    scene = context.scene

    def create_empty_armature(name):
        obj_new = bpy.data.objects.new(name, 'ARMATURE')
        armature = bpy.data.armatures.new(name)
        obj_new.data = armature
        scene.objects.link(obj_new)
        scene.objects.active = obj_new
        for obj in scene.objects:
            obj.selected = False
        obj_new.selected = True

    for module_name in get_submodule_types():
        if (metarig_type and module_name != metarig_type):
            continue

        # XXX workaround!, problem with updating the pose matrix.
        if module_name == "delta":
            continue

        type_name, submodule, func = submodule_func_from_type(module_name)

        metarig_template = getattr(submodule, "metarig_template", None)

        if metarig_template:
            create_empty_armature("meta_" + module_name) # sets active
            metarig_template()
            obj = context.active_object
            obj.location = scene.cursor_location

            if GENERATE_FINAL:
                obj_new = generate_rig(context, obj)
                new_objects.append((obj, obj_new))
            else:
                new_objects.append((obj, None))
        else:
            print("note: rig type '%s' has no metarig_template(), can't test this" % module_name)

    return new_objects


def generate_test_all(context, GRAPH=False):
    import rigify
    import rigify_utils
    import graphviz_export
    import os
    reload(rigify)
    reload(rigify_utils)
    reload(graphviz_export)

    new_objects = rigify.generate_test(context)

    if GRAPH:
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
    generate_rig(bpy.context, bpy.context.active_object)
