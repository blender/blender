# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy

from .node import BlenderNode
from .animation import BlenderAnimation
from .vnode import VNode, compute_vnodes
from ..com.extras import set_extras
from ...io.imp.user_extensions import import_user_extensions


class BlenderScene():
    """Blender Scene."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf):
        """Scene creation."""
        scene = bpy.context.scene
        gltf.blender_scene = scene.name
        if bpy.context.collection.name in bpy.data.collections:  # avoid master collection
            gltf.blender_active_collection = bpy.context.collection.name

        if gltf.data.scene is not None:
            import_user_extensions('gather_import_scene_before_hook', gltf, gltf.data.scenes[gltf.data.scene], scene)
            pyscene = gltf.data.scenes[gltf.data.scene]
            # Special case for scene extras:
            # As the scene may already exists in Blender, custom properties can be overwritten
            # So, there is an option to know if the user want to set extras or not
            if gltf.import_settings['import_scene_extras']:
                set_extras(scene, pyscene.extras)

        compute_vnodes(gltf)

        gltf.display_current_node = 0  # for debugging
        BlenderNode.create_vnode(gltf, 'root')

        # User extensions before scene creation
        gltf_scene = None
        if gltf.data.scene is not None:
            gltf_scene = gltf.data.scenes[gltf.data.scene]
        import_user_extensions('gather_import_scene_after_nodes_hook', gltf, gltf_scene, scene)

        BlenderScene.create_animations(gltf)

        # User extensions after scene creation
        gltf_scene = None
        if gltf.data.scene is not None:
            gltf_scene = gltf.data.scenes[gltf.data.scene]
        import_user_extensions('gather_import_scene_after_animation_hook', gltf, gltf_scene, scene)

        if bpy.context.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        if gltf.import_settings['import_select_created_objects'] and gltf.import_settings['import_scene_as_collection'] is True:
            BlenderScene.select_imported_objects(gltf)
            BlenderScene.set_active_object(gltf)

        # Exclude not default scene(s) collection(s), if we are in collection
        if gltf.import_settings['import_scene_as_collection'] is True:
            if gltf.data.scene is not None:
                for scene_idx, coll in gltf.blender_collections.items():
                    if scene_idx != gltf.data.scene:
                        bpy.context.layer_collection.children[coll.name].exclude = True

    @staticmethod
    def create_animations(gltf):
        """Create animations."""

        # Use a class here, to be able to pass data by reference to hook (to be able to change them inside hook)
        class IMPORT_animation_options:
            def __init__(self, restore_first_anim: bool = True):
                self.restore_first_anim = restore_first_anim

        animation_options = IMPORT_animation_options()
        import_user_extensions('gather_import_animations', gltf, gltf.data.animations, animation_options)

        if gltf.data.animations:
            # NLA tracks are added bottom to top, so create animations in
            # reverse so the first winds up on top
            for anim_idx in reversed(range(len(gltf.data.animations))):
                BlenderAnimation.anim(gltf, anim_idx)

            # Restore first animation
            if animation_options.restore_first_anim:
                anim_name = gltf.data.animations[0].track_name
                BlenderAnimation.restore_animation(gltf, anim_name)

                if hasattr(bpy.data.scenes[0], "gltf2_animation_applied"):
                    bpy.data.scenes[0].gltf2_animation_applied = bpy.data.scenes[0].gltf2_animation_tracks.find(
                        gltf.data.animations[0].track_name)

    @staticmethod
    def select_imported_objects(gltf):
        """Select all (and only) the imported objects."""
        if bpy.ops.object.select_all.poll():
            bpy.ops.object.select_all(action='DESELECT')

        for vnode in gltf.vnodes.values():
            if vnode.type == VNode.Object:
                vnode.blender_object.select_set(state=True)

    @staticmethod
    def set_active_object(gltf):
        """Make the first root object from the default glTF scene active.
        If no default scene, use the first scene, or just any root object.
        """
        vnode = None

        if gltf.data.scene is not None:
            pyscene = gltf.data.scenes[gltf.data.scene]
            if pyscene.nodes:
                vnode = gltf.vnodes[pyscene.nodes[0]]

        if not vnode:
            for pyscene in gltf.data.scenes or []:
                if pyscene.nodes:
                    vnode = gltf.vnodes[pyscene.nodes[0]]
                    break

        if not vnode:
            vnode = gltf.vnodes['root']
            if vnode.type == VNode.DummyRoot:
                if not vnode.children:
                    return  # no nodes
                vnode = gltf.vnodes[vnode.children[0]]

        if vnode.type == VNode.Bone:
            vnode = gltf.vnodes[vnode.bone_arma]

        bpy.context.view_layer.objects.active = vnode.blender_object
