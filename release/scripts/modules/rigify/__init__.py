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

    submod_name, func_name = type_pair

    # from rigify import leg
    submod = __import__(name="%s.%s" % (__package__, submod_name), fromlist=[submod_name])
    reload(submod)
    return submod, getattr(submod, func_name)
    

def validate_rig(context, obj):
    for pbone in obj.pose.bones:
        bone_name = pbone.name
        bone_type = pbone.get("type", "")

        if bone_type:
            bone_type_list = [bt for bt in bone_type.replace(",", " ").split()]
        else:
            bone_type_list = []

        for bone_type in bone_type_list:
            submod, type_func = submodule_func_from_type(bone_type)
            reload(submod)
            submod.metarig_definition(obj, bone_name)
        
        # missing, - check for duplicate root bone.


def generate_rig(context, obj_orig, prefix="ORG-", META_DEF=True):
    from collections import OrderedDict
    import rigify_utils
    reload(rigify_utils)

    global_undo = context.user_preferences.edit.global_undo
    context.user_preferences.edit.global_undo = False
    mode_orig = context.mode

    bpy.ops.object.mode_set(mode='OBJECT')

    scene = context.scene

    # copy object and data
    obj_orig.selected = False
    obj = obj_orig.copy()
    obj.data = obj_orig.data.copy()
    scene.objects.link(obj)
    scene.objects.active = obj
    obj.selected = True
    
    if META_DEF:
        obj_def = obj_orig.copy()
        obj_def.data = obj_orig.data.copy()
        scene.objects.link(obj_def)

    arm = obj.data

    # original name mapping
    base_names = {}
    
    # add all new parentless children to this bone
    root_bone = None

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
            
        if bone_type_list == ["root"]: # special case!
            if root_bone:
                raise Exception("cant have more then 1 root bone, found '%s' and '%s' to have type==root" % (root_bone, bone_name))
            root_bone = bone_name
            bone_type_list[:] = []

        for bone_type in bone_type_list:
            submod, type_func = submodule_func_from_type(bone_type)
            reload(submod)
            submod_name = submod.__name__
            
            bone_def_dict = bone_definitions.setdefault(bone_name, {})

            # Only calculate bone definitions once
            if submod_name not in bone_def_dict:
                bone_def_dict[submod_name] = submod.metarig_definition(obj, bone_name)

            bone_typeinfo = bone_typeinfos.setdefault(bone_name, [])
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
        
        bone_names_pre = set([bone.name for bone in arm.bones])

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
                blend_bone_list(obj, definition, result_submod[0], result_submod[1], target_bone=bone_name)


        bone_names_post = set([bone.name for bone in arm.bones])
        
        # Store which bones were created from this one
        bone_genesis[bone_name] = list(bone_names_post - bone_names_pre)
    
    # need a reverse lookup on bone_genesis so as to know immediately
    # where a bone comes from
    bone_genesis_reverse = {}
    for bone_name, bone_children in bone_genesis.items():
        for bone_child_name in bone_children:
            bone_genesis_reverse[bone_child_name] = bone_name
    

    if root_bone:
        # assign all new parentless bones to this
        
        bpy.ops.object.mode_set(mode='EDIT')
        root_ebone = arm.edit_bones[root_bone]
        for ebone in arm.edit_bones:
            bone_name = ebone.name
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
                ebone.parent = root_ebone

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

    # Only for demo'ing

    # obj.restrict_view = True
    obj.data.draw_axes = False

    bpy.ops.object.mode_set(mode=mode_orig)

    context.user_preferences.edit.global_undo = global_undo
    
    return obj


def generate_test(context, metarig_type="", GENERATE_FINAL=True):
    import os
    new_objects = []

    scene = context.scene

    def create_empty_armature(name):
        obj_new = bpy.data.add_object('ARMATURE', name)
        armature = bpy.data.add_armature(name)
        obj_new.data = armature
        scene.objects.link(obj_new)
        scene.objects.active = obj_new
        for obj in scene.objects:
            obj.selected = False
        obj_new.selected = True

    files = os.listdir(os.path.dirname(__file__))
    for f in files:
        if f.startswith("_"):
            continue

        if not f.endswith(".py"):
            continue

        module_name = f[:-3]
        
        if (metarig_type and module_name != metarig_type):
            continue
        
        submodule = __import__(name="%s.%s" % (__package__, module_name), fromlist=[module_name])

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
            print("note: rig type '%s' has no metarig_template(), can't test this", module_name)

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
