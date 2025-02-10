# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ...io.imp.user_extensions import import_user_extensions
from .animation_node import BlenderNodeAnim
from .animation_weight import BlenderWeightAnim
from .animation_pointer import BlenderPointerAnim
from .animation_utils import simulate_stash, restore_animation_on_object
from .vnode import VNode


class BlenderAnimation():
    """Dispatch Animation to node or morph weights animation, or via KHR_animation_pointer"""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def anim(gltf, anim_idx):
        """Create actions/tracks for one animation."""
        # Caches the action/slot for each object, keyed by:
        #   - anim_idx
        #   - obj_name
        #   - target_id_type
        gltf.action_cache = {}
        # Things we need to stash when we're done.
        gltf.needs_stash = []

        import_user_extensions('gather_import_animation_before_hook', gltf, anim_idx)

        for vnode_id in gltf.vnodes:
            if isinstance(vnode_id, int):
                BlenderNodeAnim.anim(gltf, anim_idx, vnode_id)
            BlenderWeightAnim.anim(gltf, anim_idx, vnode_id)

        if gltf.data.extensions_used is not None and "KHR_animation_pointer" in gltf.data.extensions_used:
            for cam_idx, cam in enumerate(gltf.data.cameras if gltf.data.cameras else []):
                if len(cam.animations) == 0:
                    continue
                BlenderPointerAnim.anim(gltf, anim_idx, cam, cam_idx, 'CAMERA')

            if gltf.data.extensions is not None and "KHR_lights_punctual" in gltf.data.extensions:
                for light_idx, light in enumerate(gltf.data.extensions["KHR_lights_punctual"]["lights"]):
                    if len(light["animations"]) == 0:
                        continue
                    BlenderPointerAnim.anim(gltf, anim_idx, light, light_idx, 'LIGHT')

            for mat_idx, mat in enumerate(gltf.data.materials if gltf.data.materials else []):
                if len(mat.blender_material) == 0:
                    # The animated material is not used in Blender, so do not animate it
                    continue
                if len(mat.animations) != 0:
                    BlenderPointerAnim.anim(gltf, anim_idx, mat, mat_idx, 'MATERIAL')
                if mat.normal_texture is not None and len(mat.normal_texture.animations) != 0:
                    BlenderPointerAnim.anim(gltf, anim_idx, mat.normal_texture, mat_idx, 'MATERIAL_PBR', name=mat.name)
                if mat.occlusion_texture is not None and len(mat.occlusion_texture.animations) != 0:
                    BlenderPointerAnim.anim(gltf, anim_idx, mat.occlusion_texture,
                                            mat_idx, 'MATERIAL_PBR', name=mat.name)
                if mat.pbr_metallic_roughness is not None and len(mat.pbr_metallic_roughness.animations) != 0:
                    # This can be a regulat PBR or unlit material
                    is_unlit = mat.extensions is not None and "KHR_materials_unlit" in mat.extensions
                    BlenderPointerAnim.anim(gltf, anim_idx, mat.pbr_metallic_roughness, mat_idx,
                                            'MATERIAL_PBR', name=mat.name, is_unlit=is_unlit)

                texs = [
                    mat.emissive_texture,
                    mat.normal_texture,
                    mat.occlusion_texture,
                    mat.pbr_metallic_roughness.base_color_texture if mat.pbr_metallic_roughness is not None else None,
                    mat.pbr_metallic_roughness.metallic_roughness_texture if mat.pbr_metallic_roughness is not None else None,
                ]

                for tex in [t for t in texs if t is not None]:
                    if tex.extensions is not None and "KHR_texture_transform" in tex.extensions:
                        # This can be a regulat PBR or unlit material
                        is_unlit = mat.extensions is not None and "KHR_materials_unlit" in mat.extensions
                        BlenderPointerAnim.anim(
                            gltf,
                            anim_idx,
                            tex.extensions["KHR_texture_transform"],
                            mat_idx,
                            'TEX_TRANSFORM',
                            name=mat.name,
                            is_unlit=is_unlit)

                if mat.extensions is not None:
                    texs = [
                        mat.extensions["KHR_materials_volume"].get("thicknessTexture") if "KHR_materials_volume" in mat.extensions else None,
                        mat.extensions["KHR_materials_transmission"].get("transmissionTexture") if "KHR_materials_transmission" in mat.extensions else None,
                        mat.extensions["KHR_materials_specular"].get("specularTexture") if "KHR_materials_specular" in mat.extensions else None,
                        mat.extensions["KHR_materials_specular"].get("specularColorTexture") if "KHR_materials_specular" in mat.extensions else None,
                        mat.extensions["KHR_materials_sheen"].get("sheenColorTexture") if "KHR_materials_sheen" in mat.extensions else None,
                        mat.extensions["KHR_materials_sheen"].get("sheenRoughnessTexture") if "KHR_materials_sheen" in mat.extensions else None,
                        mat.extensions["KHR_materials_clearcoat"].get("clearcoatTexture") if "KHR_materials_clearcoat" in mat.extensions else None,
                        mat.extensions["KHR_materials_clearcoat"].get("clearcoatRoughnessTexture") if "KHR_materials_clearcoat" in mat.extensions else None,
                        mat.extensions["KHR_materials_clearcoat"].get("clearcoatNormalTexture") if "KHR_materials_clearcoat" in mat.extensions else None,
                        mat.extensions["KHR_materials_anisotropy"].get("anisotropyTexture") if "KHR_materials_anisotropy" in mat.extensions else None,
                    ]

                    for tex in [t for t in texs if t is not None]:
                        if 'extensions' in tex and "KHR_texture_transform" in tex['extensions']:
                            BlenderPointerAnim.anim(
                                gltf,
                                anim_idx,
                                tex['extensions']["KHR_texture_transform"],
                                mat_idx,
                                'TEX_TRANSFORM',
                                name=mat.name)

                for ext in [
                        "KHR_materials_emissive_strength",
                        # "KHR_materials_iridescence",
                        "KHR_materials_volume",
                        "KHR_materials_ior",
                        "KHR_materials_transmission",
                        "KHR_materials_clearcoat",
                        "KHR_materials_sheen",
                        "KHR_materials_specular",
                        "KHR_materials_anisotropy"
                ]:
                    if mat.extensions is not None and ext in mat.extensions:
                        BlenderPointerAnim.anim(gltf, anim_idx, mat.extensions[ext], mat_idx, 'EXT', name=mat.name)

        # Push all actions onto NLA tracks with this animation's name
        track_name = gltf.data.animations[anim_idx].track_name
        for (obj, action, slot) in gltf.needs_stash:
            simulate_stash(obj, track_name, action, slot)

        import_user_extensions('gather_import_animation_after_hook', gltf, anim_idx, track_name)

        if hasattr(bpy.data.scenes[0], 'gltf2_animation_tracks') is False:
            return

        if track_name not in [track.name for track in bpy.data.scenes[0].gltf2_animation_tracks]:
            new_ = bpy.data.scenes[0].gltf2_animation_tracks.add()
            new_.name = track_name
        # reverse order, as animation are created in reverse order (because of NLA adding tracks are reverted)
        bpy.data.scenes[0].gltf2_animation_tracks.move(len(bpy.data.scenes[0].gltf2_animation_tracks) - 1, 0)

    @staticmethod
    def restore_animation(gltf, animation_name):
        """Restores the actions for an animation by its track name."""
        for vnode_id in gltf.vnodes:
            vnode = gltf.vnodes[vnode_id]
            if vnode.type == VNode.Bone:
                obj = gltf.vnodes[vnode.bone_arma].blender_object
            elif vnode.type == VNode.Object:
                obj = vnode.blender_object
            else:
                continue

            restore_animation_on_object(obj, animation_name)
            if obj.data and hasattr(obj.data, 'shape_keys'):
                restore_animation_on_object(obj.data.shape_keys, animation_name)

        if gltf.data.extensions_used is not None and "KHR_animation_pointer" in gltf.data.extensions_used:
            for cam in gltf.data.cameras if gltf.data.cameras else []:
                restore_animation_on_object(cam.blender_object_data, animation_name)

            if gltf.data.extensions and "KHR_lights_punctual" in gltf.data.extensions:
                for light in gltf.data.extensions['KHR_lights_punctual']['lights']:
                    restore_animation_on_object(light['blender_object_data'], animation_name)

            for mat in gltf.data.materials if gltf.data.materials else []:
                if len(mat.blender_material) == 0:
                    # The animated material is not used in Blender, so do not animate it
                    continue
                restore_animation_on_object(mat.blender_nodetree, animation_name)
                restore_animation_on_object(mat.blender_mat, animation_name)
