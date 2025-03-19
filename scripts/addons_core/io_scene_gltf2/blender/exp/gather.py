# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy

from ...io.com import gltf2_io
from ...io.exp.user_extensions import export_user_extensions
from ..com.extras import generate_extras
from .cache import cached
from . import nodes as gltf2_blender_gather_nodes
from . import joints as gltf2_blender_gather_joints
from . import tree as gltf2_blender_gather_tree
from .animation.sampled.object.keyframes import get_cache_data
from .animation.animations import gather_animations


def gather_gltf2(export_settings):
    """
    Gather glTF properties from the current state of blender.

    :return: list of scene graphs to be added to the glTF export
    """
    scenes = []
    animations = []  # unfortunately animations in gltf2 are just as 'root' as scenes.
    active_scene = None
    store_user_scene = bpy.context.scene
    if export_settings['gltf_collection'] == "" and export_settings['gltf_active_scene'] is False:
        # If no collection export and no active scene export, we need to export all scenes
        scenes_to_export = bpy.data.scenes
    elif export_settings['gltf_collection'] == "" and export_settings['gltf_active_scene'] is True:
        # If no collection export and active scene export, we need to export only the active scene
        scenes_to_export = [
        scene for scene in bpy.data.scenes if scene.name == store_user_scene.name]
    elif export_settings['gltf_collection'] != "":
        # If collection export, we need to export only the collection, so keeping only the active scene
        scenes_to_export = [
        scene for scene in bpy.data.scenes if scene.name == store_user_scene.name]
    else:
        # This should never happen
        raise Exception("Unknown export settings")

    for blender_scene in scenes_to_export:
        scenes.append(__gather_scene(blender_scene, export_settings))
        if export_settings['gltf_animations']:
            # resetting object cache
            get_cache_data.reset_cache()
            animations += gather_animations(export_settings)
        if bpy.context.scene.name == store_user_scene.name:
            active_scene = len(scenes) - 1

    # restore user scene
    bpy.context.window.scene = store_user_scene
    return active_scene, scenes, animations


@cached
def __gather_scene(blender_scene, export_settings):
    scene = gltf2_io.Scene(
        extensions=None,
        extras=__gather_extras(blender_scene, export_settings),
        name=__gather_name(blender_scene, export_settings),
        nodes=[]
    )

    # Initialize some data needed for animation pointer
    export_settings['KHR_animation_pointer'] = {}
    export_settings['KHR_animation_pointer']['materials'] = {}
    export_settings['KHR_animation_pointer']['lights'] = {}
    export_settings['KHR_animation_pointer']['cameras'] = {}

    vtree = gltf2_blender_gather_tree.VExportTree(export_settings)
    vtree.construct(blender_scene)
    vtree.search_missing_armature()  # In case armature are no parented correctly
    if export_settings['gltf_armature_object_remove'] is True:
        vtree.check_if_we_can_remove_armature()  # Check if we can remove the armatures objects

    export_user_extensions('vtree_before_filter_hook', export_settings, vtree)

    # Now, we can filter tree if needed
    vtree.filter()

    vtree.bake_armature_bone_list()  # Used in case we remove the armature. Doing it after filter, as filter can remove some bones

    if export_settings['gltf_flatten_bones_hierarchy'] is True:
        vtree.break_bone_hierarchy()
    if export_settings['gltf_flatten_obj_hierarchy'] is True:
        vtree.break_obj_hierarchy()

    # Now we filtered the tree, in case of Collection Export,
    # We need to calculate the collection center,
    # In order to set the scene center to the collection center
    # Using object center barycenter for now (another option could be to use bounding box center)
    if export_settings['gltf_collection'] and export_settings['gltf_at_collection_center']:
        vtree.calculate_collection_center()

    vtree.variants_reset_to_original()

    export_user_extensions('vtree_after_filter_hook', export_settings, vtree)

    export_settings['vtree'] = vtree

    # If we don't remove armature object, we can't have bones directly at root of scene
    # So looping only on root nodes, as they are all nodes, not bones
    if export_settings['gltf_armature_object_remove'] is False:
        for r in [vtree.nodes[r] for r in vtree.roots]:
            node = gltf2_blender_gather_nodes.gather_node(
                r, export_settings)
            if node is not None:
                scene.nodes.append(node)
    else:
        # If we remove armature objects, we can have bone at root of scene
        armature_root_joints = {}
        for r in [vtree.nodes[r] for r in vtree.roots]:
            # Classic Object/node case
            if r.blender_type != gltf2_blender_gather_tree.VExportNode.BONE:
                node = gltf2_blender_gather_nodes.gather_node(
                    r, export_settings)
                if node is not None:
                    scene.nodes.append(node)
            else:
                # We can have bone are root of scene because we remove the armature object
                # and the armature was at root of scene
                node = gltf2_blender_gather_joints.gather_joint_vnode(
                    r.uuid, export_settings)
                if node is not None:
                    scene.nodes.append(node)
                    if r.armature not in armature_root_joints.keys():
                        armature_root_joints[r.armature] = []
                    armature_root_joints[r.armature].append(node)

        # Manage objects parented to bones, now we go through all root objects
        for k, v in armature_root_joints.items():
            gltf2_blender_gather_nodes.get_objects_parented_to_bones(k, v, export_settings)

    vtree.add_neutral_bones()

    export_user_extensions('gather_scene_hook', export_settings, scene, blender_scene)

    return scene


def __gather_extras(blender_object, export_settings):
    if export_settings['gltf_extras']:
        # If case of collection export, use custom properties of the collection instead of the scene
        # So Collection custom properties are exported as glTF Scene extras
        if export_settings['gltf_collection']:
            return generate_extras(bpy.data.collections[export_settings['gltf_collection']])
        return generate_extras(blender_object)
    return None

def __gather_name(blender_scene, export_settings):
    if export_settings['gltf_collection']:
        return export_settings['gltf_collection']
    return blender_scene.name
