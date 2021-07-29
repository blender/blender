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
import re
import time
import traceback
import sys
from rna_prop_ui import rna_idprop_ui_prop_get

from .utils import MetarigError, new_bone, get_rig_type
from .utils import ORG_PREFIX, MCH_PREFIX, DEF_PREFIX, WGT_PREFIX, ROOT_NAME, make_original_name
from .utils import RIG_DIR
from .utils import create_root_widget
from .utils import random_id
from .utils import copy_attributes
from .utils import gamma_correct
from .rig_ui_template import UI_SLIDERS, layers_ui, UI_REGISTER


RIG_MODULE = "rigs"
ORG_LAYER = [n == 31 for n in range(0, 32)]  # Armature layer that original bones should be moved to.
MCH_LAYER = [n == 30 for n in range(0, 32)]  # Armature layer that mechanism bones should be moved to.
DEF_LAYER = [n == 29 for n in range(0, 32)]  # Armature layer that deformation bones should be moved to.
ROOT_LAYER = [n == 28 for n in range(0, 32)]  # Armature layer that root bone should be moved to.
WGT_LAYERS = [x == 19 for x in range(0, 20)]  # Widgets go on the last scene layer.


class Timer:
    def __init__(self):
        self.timez = time.time()

    def tick(self, string):
        t = time.time()
        print(string + "%.3f" % (t - self.timez))
        self.timez = t


# TODO: generalize to take a group as input instead of an armature.
def generate_rig(context, metarig):
    """ Generates a rig from a metarig.

    """
    t = Timer()

    # Random string with time appended so that
    # different rigs don't collide id's
    rig_id = random_id(16)

    # Initial configuration
    # mode_orig = context.mode  # UNUSED
    rest_backup = metarig.data.pose_position
    metarig.data.pose_position = 'REST'

    bpy.ops.object.mode_set(mode='OBJECT')

    scene = context.scene
    id_store = context.window_manager
    #------------------------------------------
    # Create/find the rig object and set it up

    # Check if the generated rig already exists, so we can
    # regenerate in the same object.  If not, create a new
    # object to generate the rig in.
    print("Fetch rig.")

    rig_new_name = ""
    rig_old_name = ""
    if id_store.rigify_rig_basename:
        rig_new_name = id_store.rigify_rig_basename + "_rig"

    if id_store.rigify_generate_mode == 'overwrite':
        name = id_store.rigify_target_rig or "rig"
        try:
            obj = scene.objects[name]
            rig_old_name = name
            obj.name = rig_new_name or name
        except KeyError:
            rig_old_name = name
            name = rig_new_name or name
            obj = bpy.data.objects.new(name, bpy.data.armatures.new(name))
            obj.draw_type = 'WIRE'
            scene.objects.link(obj)
    else:
        name = rig_new_name or "rig"
        obj = bpy.data.objects.new(name, bpy.data.armatures.new(name))  # in case name 'rig' exists it will be rig.001
        obj.draw_type = 'WIRE'
        scene.objects.link(obj)

    id_store.rigify_target_rig = obj.name
    obj.data.pose_position = 'POSE'

    # Get rid of anim data in case the rig already existed
    print("Clear rig animation data.")
    obj.animation_data_clear()

    # Select generated rig object
    metarig.select = False
    obj.select = True
    scene.objects.active = obj

    # Remove wgts if force update is set
    wgts_group_name = "WGTS_" + (rig_old_name or obj.name)
    if wgts_group_name in scene.objects and id_store.rigify_force_widget_update:
        bpy.ops.object.select_all(action='DESELECT')
        for i, lyr in enumerate(WGT_LAYERS):
            if lyr:
                context.scene.layers[i] = True
        for wgt in bpy.data.objects[wgts_group_name].children:
            wgt.select = True
        bpy.ops.object.delete(use_global=False)
        for i, lyr in enumerate(WGT_LAYERS):
            if lyr:
                context.scene.layers[i] = False
        if rig_old_name:
            bpy.data.objects[wgts_group_name].name = "WGTS_" + obj.name

    wgts_group_name = "WGTS_" + obj.name

    # Get parented objects to restore later
    childs = {}  # {object: bone}
    for child in obj.children:
        childs[child] = child.parent_bone

    # Remove all bones from the generated rig armature.
    bpy.ops.object.mode_set(mode='EDIT')
    for bone in obj.data.edit_bones:
        obj.data.edit_bones.remove(bone)
    bpy.ops.object.mode_set(mode='OBJECT')

    # Create temporary duplicates for merging
    temp_rig_1 = metarig.copy()
    temp_rig_1.data = metarig.data.copy()
    scene.objects.link(temp_rig_1)

    temp_rig_2 = metarig.copy()
    temp_rig_2.data = obj.data
    scene.objects.link(temp_rig_2)

    # Select the temp rigs for merging
    for objt in scene.objects:
        objt.select = False  # deselect all objects
    temp_rig_1.select = True
    temp_rig_2.select = True
    scene.objects.active = temp_rig_2

    # Merge the temporary rigs
    bpy.ops.object.join()

    # Delete the second temp rig
    bpy.ops.object.delete()

    # Select the generated rig
    for objt in scene.objects:
        objt.select = False  # deselect all objects
    obj.select = True
    scene.objects.active = obj

    # Copy over bone properties
    for bone in metarig.data.bones:
        bone_gen = obj.data.bones[bone.name]

        # B-bone stuff
        bone_gen.bbone_segments = bone.bbone_segments
        bone_gen.bbone_in = bone.bbone_in
        bone_gen.bbone_out = bone.bbone_out

    # Copy over the pose_bone properties
    for bone in metarig.pose.bones:
        bone_gen = obj.pose.bones[bone.name]

        # Rotation mode and transform locks
        bone_gen.rotation_mode = bone.rotation_mode
        bone_gen.lock_rotation = tuple(bone.lock_rotation)
        bone_gen.lock_rotation_w = bone.lock_rotation_w
        bone_gen.lock_rotations_4d = bone.lock_rotations_4d
        bone_gen.lock_location = tuple(bone.lock_location)
        bone_gen.lock_scale = tuple(bone.lock_scale)

        # rigify_type and rigify_parameters
        bone_gen.rigify_type = bone.rigify_type
        for prop in dir(bone_gen.rigify_parameters):
            if (not prop.startswith("_")) \
            and (not prop.startswith("bl_")) \
            and (prop != "rna_type"):
                try:
                    setattr(bone_gen.rigify_parameters, prop, \
                            getattr(bone.rigify_parameters, prop))
                except AttributeError:
                    print("FAILED TO COPY PARAMETER: " + str(prop))

        # Custom properties
        for prop in bone.keys():
            try:
                bone_gen[prop] = bone[prop]
            except KeyError:
                pass

        # Constraints
        for con1 in bone.constraints:
            con2 = bone_gen.constraints.new(type=con1.type)
            copy_attributes(con1, con2)

            # Set metarig target to rig target
            if "target" in dir(con2):
                if con2.target == metarig:
                    con2.target = obj

    # Copy drivers
    if metarig.animation_data:
        for d1 in metarig.animation_data.drivers:
            d2 = obj.driver_add(d1.data_path)
            copy_attributes(d1, d2)
            copy_attributes(d1.driver, d2.driver)

            # Remove default modifiers, variables, etc.
            for m in d2.modifiers:
                d2.modifiers.remove(m)
            for v in d2.driver.variables:
                d2.driver.variables.remove(v)

            # Copy modifiers
            for m1 in d1.modifiers:
                m2 = d2.modifiers.new(type=m1.type)
                copy_attributes(m1, m2)

            # Copy variables
            for v1 in d1.driver.variables:
                v2 = d2.driver.variables.new()
                copy_attributes(v1, v2)
                for i in range(len(v1.targets)):
                    copy_attributes(v1.targets[i], v2.targets[i])
                    # Switch metarig targets to rig targets
                    if v2.targets[i].id == metarig:
                        v2.targets[i].id = obj

                    # Mark targets that may need to be altered after rig generation
                    tar = v2.targets[i]
                    # If a custom property
                    if v2.type == 'SINGLE_PROP' \
                    and re.match('^pose.bones\["[^"\]]*"\]\["[^"\]]*"\]$', tar.data_path):
                        tar.data_path = "RIGIFY-" + tar.data_path

            # Copy key frames
            for i in range(len(d1.keyframe_points)):
                d2.keyframe_points.add()
                k1 = d1.keyframe_points[i]
                k2 = d2.keyframe_points[i]
                copy_attributes(k1, k2)

    t.tick("Duplicate rig: ")
    #----------------------------------
    # Make a list of the original bones so we can keep track of them.
    original_bones = [bone.name for bone in obj.data.bones]

    # Add the ORG_PREFIX to the original bones.
    bpy.ops.object.mode_set(mode='OBJECT')
    for i in range(0, len(original_bones)):
        obj.data.bones[original_bones[i]].name = make_original_name(original_bones[i])
        original_bones[i] = make_original_name(original_bones[i])

    # Create a sorted list of the original bones, sorted in the order we're
    # going to traverse them for rigging.
    # (root-most -> leaf-most, alphabetical)
    bones_sorted = []
    for name in original_bones:
        bones_sorted += [name]
    bones_sorted.sort()  # first sort by names
    bones_sorted.sort(key=lambda bone: len(obj.pose.bones[bone].parent_recursive))  # then parents before children

    t.tick("Make list of org bones: ")
    #----------------------------------
    # Create the root bone.
    bpy.ops.object.mode_set(mode='EDIT')
    root_bone = new_bone(obj, ROOT_NAME)
    spread = get_xy_spread(metarig.data.bones) or metarig.data.bones[0].length
    spread = float('%.3g' % spread)
    scale = spread/0.589
    obj.data.edit_bones[root_bone].head = (0, 0, 0)
    obj.data.edit_bones[root_bone].tail = (0, scale, 0)
    obj.data.edit_bones[root_bone].roll = 0
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.bones[root_bone].layers = ROOT_LAYER

    # Put the rig_name in the armature custom properties
    rna_idprop_ui_prop_get(obj.data, "rig_id", create=True)
    obj.data["rig_id"] = rig_id

    t.tick("Create root bone: ")

    # Create Group widget
    # wgts_group_name = "WGTS"
    if wgts_group_name not in scene.objects:
        if wgts_group_name in bpy.data.objects:
            bpy.data.objects[wgts_group_name].user_clear()
            bpy.data.objects.remove(bpy.data.objects[wgts_group_name])
        mesh = bpy.data.meshes.new(wgts_group_name)
        wgts_obj = bpy.data.objects.new(wgts_group_name, mesh)
        scene.objects.link(wgts_obj)
        wgts_obj.layers = WGT_LAYERS
        t.tick("Create main WGTS: ")
    #
    # if id_store.rigify_generate_mode == 'new':
    #     bpy.ops.object.select_all(action='DESELECT')
    #     for wgt in bpy.data.objects[wgts_group_name].children:
    #         wgt.select = True
    #     for i, lyr in enumerate(WGT_LAYERS):
    #         if lyr:
    #             context.scene.layers[i] = True
    #     bpy.ops.object.make_single_user(obdata=True)
    #     for i, lyr in enumerate(WGT_LAYERS):
    #         if lyr:
    #             context.scene.layers[i] = False

    #----------------------------------
    try:
        # Collect/initialize all the rigs.
        rigs = []
        for bone in bones_sorted:
            bpy.ops.object.mode_set(mode='EDIT')
            rigs += get_bone_rigs(obj, bone)
        t.tick("Initialize rigs: ")

        # Generate all the rigs.
        ui_scripts = []
        for rig in rigs:
            # Go into editmode in the rig armature
            bpy.ops.object.mode_set(mode='OBJECT')
            context.scene.objects.active = obj
            obj.select = True
            bpy.ops.object.mode_set(mode='EDIT')
            scripts = rig.generate()
            if scripts is not None:
                ui_scripts += [scripts[0]]
        t.tick("Generate rigs: ")
    except Exception as e:
        # Cleanup if something goes wrong
        print("Rigify: failed to generate rig.")
        metarig.data.pose_position = rest_backup
        obj.data.pose_position = 'POSE'
        bpy.ops.object.mode_set(mode='OBJECT')

        # Continue the exception
        raise e

    #----------------------------------
    bpy.ops.object.mode_set(mode='OBJECT')

    # Get a list of all the bones in the armature
    bones = [bone.name for bone in obj.data.bones]

    # Parent any free-floating bones to the root excluding bones with child of constraint.
    pbones = obj.pose.bones


    ik_follow_drivers = []

    if obj.animation_data:
        for drv in obj.animation_data.drivers:
            for var in drv.driver.variables:
                if 'IK_follow' == var.name:
                    ik_follow_drivers.append(drv.data_path)

    noparent_bones = []
    for bone in bones:
        # if 'IK_follow' in pbones[bone].keys():
        #     noparent_bones += [bone]
        for d in ik_follow_drivers:
            if bone in d:
                noparent_bones += [bone]

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in bones:
        if bone in noparent_bones:
            continue
        elif obj.data.edit_bones[bone].parent is None:
            obj.data.edit_bones[bone].use_connect = False
            obj.data.edit_bones[bone].parent = obj.data.edit_bones[root_bone]

    bpy.ops.object.mode_set(mode='OBJECT')

    # Lock transforms on all non-control bones
    r = re.compile("[A-Z][A-Z][A-Z]-")
    for bone in bones:
        if r.match(bone):
            pb = obj.pose.bones[bone]
            pb.lock_location = (True, True, True)
            pb.lock_rotation = (True, True, True)
            pb.lock_rotation_w = True
            pb.lock_scale = (True, True, True)

    # Every bone that has a name starting with "DEF-" make deforming.  All the
    # others make non-deforming.
    for bone in bones:
        if obj.data.bones[bone].name.startswith(DEF_PREFIX):
            obj.data.bones[bone].use_deform = True
        else:
            obj.data.bones[bone].use_deform = False

    # Alter marked driver targets
    if obj.animation_data:
        for d in obj.animation_data.drivers:
            for v in d.driver.variables:
                for tar in v.targets:
                    if tar.data_path.startswith("RIGIFY-"):
                        temp, bone, prop = tuple([x.strip('"]') for x in tar.data_path.split('["')])
                        if bone in obj.data.bones \
                        and prop in obj.pose.bones[bone].keys():
                            tar.data_path = tar.data_path[7:]
                        else:
                            tar.data_path = 'pose.bones["%s"]["%s"]' % (make_original_name(bone), prop)

    # Move all the original bones to their layer.
    for bone in original_bones:
        obj.data.bones[bone].layers = ORG_LAYER

    # Move all the bones with names starting with "MCH-" to their layer.
    for bone in bones:
        if obj.data.bones[bone].name.startswith(MCH_PREFIX):
            obj.data.bones[bone].layers = MCH_LAYER

    # Move all the bones with names starting with "DEF-" to their layer.
    for bone in bones:
        if obj.data.bones[bone].name.startswith(DEF_PREFIX):
            obj.data.bones[bone].layers = DEF_LAYER

    # Create root bone widget
    create_root_widget(obj, "root")

    # Assign shapes to bones
    # Object's with name WGT-<bone_name> get used as that bone's shape.
    for bone in bones:
        wgt_name = (WGT_PREFIX + obj.name + '_' + obj.data.bones[bone].name)[:63]  # Object names are limited to 63 characters... arg
        if wgt_name in context.scene.objects:
            # Weird temp thing because it won't let me index by object name
            for ob in context.scene.objects:
                if ob.name == wgt_name:
                    obj.pose.bones[bone].custom_shape = ob
                    break
            # This is what it should do:
            # obj.pose.bones[bone].custom_shape = context.scene.objects[wgt_name]
    # Reveal all the layers with control bones on them
    vis_layers = [False for n in range(0, 32)]
    for bone in bones:
        for i in range(0, 32):
            vis_layers[i] = vis_layers[i] or obj.data.bones[bone].layers[i]
    for i in range(0, 32):
        vis_layers[i] = vis_layers[i] and not (ORG_LAYER[i] or MCH_LAYER[i] or DEF_LAYER[i])
    obj.data.layers = vis_layers

    # Ensure the collection of layer names exists
    for i in range(1 + len(metarig.data.rigify_layers), 29):
        metarig.data.rigify_layers.add()

    # Create list of layer name/row pairs
    layer_layout = []
    for l in metarig.data.rigify_layers:
        print(l.name)
        layer_layout += [(l.name, l.row)]

    # Generate the UI script
    if id_store.rigify_generate_mode == 'overwrite':
        rig_ui_name = id_store.rigify_rig_ui or 'rig_ui.py'
    else:
        rig_ui_name = 'rig_ui.py'

    if id_store.rigify_generate_mode == 'overwrite' and rig_ui_name in bpy.data.texts.keys():
        script = bpy.data.texts[rig_ui_name]
        script.clear()
    else:
        script = bpy.data.texts.new("rig_ui.py")

    rig_ui_old_name = ""
    if id_store.rigify_rig_basename:
        rig_ui_old_name = script.name
        script.name = id_store.rigify_rig_basename + "_rig_ui.py"

    id_store.rigify_rig_ui = script.name

    script.write(UI_SLIDERS % rig_id)
    for s in ui_scripts:
        script.write("\n        " + s.replace("\n", "\n        ") + "\n")
    script.write(layers_ui(vis_layers, layer_layout))
    script.write(UI_REGISTER)
    script.use_module = True

    # Run UI script
    exec(script.as_string(), {})

    # Create Selection Sets
    create_selection_sets(obj, metarig)

    # Create Bone Groups
    create_bone_groups(obj, metarig)

    # Add rig_ui to logic
    skip = False
    ctrls = obj.game.controllers

    for c in ctrls:
        if 'Python' in c.name and c.text.name == script.name:
            skip = True
            break
    if not skip:
        bpy.ops.logic.controller_add(type='PYTHON', object=obj.name)
        ctrl = obj.game.controllers[-1]
        ctrl.text = bpy.data.texts[script.name]


    t.tick("The rest: ")
    #----------------------------------
    # Deconfigure
    bpy.ops.object.mode_set(mode='OBJECT')
    metarig.data.pose_position = rest_backup
    obj.data.pose_position = 'POSE'

    # Restore parent to bones
    for child, sub_parent in childs.items():
        if sub_parent in obj.pose.bones:
            mat = child.matrix_world.copy()
            child.parent_bone = sub_parent
            child.matrix_world = mat

def create_selection_sets(obj, metarig):

    # Check if selection sets addon is installed
    if 'bone_selection_groups' not in bpy.context.user_preferences.addons \
            and 'bone_selection_sets' not in bpy.context.user_preferences.addons:
        return

    bpy.ops.object.mode_set(mode='POSE')

    bpy.context.scene.objects.active = obj
    obj.select = True
    metarig.select = False
    pbones = obj.pose.bones

    for i, name in enumerate(metarig.data.rigify_layers.keys()):
        if name == '' or not metarig.data.rigify_layers[i].set:
            continue

        bpy.ops.pose.select_all(action='DESELECT')
        for b in pbones:
            if b.bone.layers[i]:
                b.bone.select = True

        #bpy.ops.pose.selection_set_add()
        obj.selection_sets.add()
        obj.selection_sets[-1].name = name
        if 'bone_selection_sets' in bpy.context.user_preferences.addons:
            act_sel_set = obj.selection_sets[-1]

            # iterate only the selected bones in current pose that are not hidden
            for bone in bpy.context.selected_pose_bones:
                if bone.name not in act_sel_set.bone_ids:
                    bone_id = act_sel_set.bone_ids.add()
                    bone_id.name = bone.name


def create_bone_groups(obj, metarig):

    bpy.ops.object.mode_set(mode='OBJECT')
    pb = obj.pose.bones
    layers = metarig.data.rigify_layers
    groups = metarig.data.rigify_colors

    # Create BGs
    for l in layers:
        if l.group == 0:
            continue
        g_id = l.group - 1
        name = groups[g_id].name
        if name not in obj.pose.bone_groups.keys():
            bg = obj.pose.bone_groups.new(name)
            bg.color_set = 'CUSTOM'
            bg.colors.normal = gamma_correct(groups[g_id].normal)
            bg.colors.select = gamma_correct(groups[g_id].select)
            bg.colors.active = gamma_correct(groups[g_id].active)

    for b in pb:
        try:
            layer_index = b.bone.layers[:].index(True)
        except ValueError:
            continue
        if layer_index > len(layers) - 1:   # bone is on reserved layers
            continue
        g_id = layers[layer_index].group - 1
        if g_id >= 0:
            name = groups[g_id].name
            b.bone_group = obj.pose.bone_groups[name]


def get_bone_rigs(obj, bone_name, halt_on_missing=False):
    """ Fetch all the rigs specified on a bone.
    """
    rigs = []
    rig_type = obj.pose.bones[bone_name].rigify_type
    rig_type = rig_type.replace(" ", "")

    if rig_type == "":
        pass
    else:
        # Gather parameters
        params = obj.pose.bones[bone_name].rigify_parameters

        # Get the rig
        try:
            rig = get_rig_type(rig_type).Rig(obj, bone_name, params)
        except ImportError:
            message = "Rig Type Missing: python module for type '%s' not found (bone: %s)" % (rig_type, bone_name)
            if halt_on_missing:
                raise MetarigError(message)
            else:
                print(message)
                print('print_exc():')
                traceback.print_exc(file=sys.stdout)
        else:
            rigs += [rig]
    return rigs


def get_xy_spread(bones):
    x_max = 0
    y_max = 0
    for b in bones:
        x_max = max((x_max, abs(b.head[0]), abs(b.tail[0])))
        y_max = max((y_max, abs(b.head[1]), abs(b.tail[1])))

    return max((x_max, y_max))


def param_matches_type(param_name, rig_type):
    """ Returns True if the parameter name is consistent with the rig type.
    """
    if param_name.rsplit(".", 1)[0] == rig_type:
        return True
    else:
        return False


def param_name(param_name, rig_type):
    """ Get the actual parameter name, sans-rig-type.
    """
    return param_name[len(rig_type) + 1:]
