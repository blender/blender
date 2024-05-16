# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from mathutils import Vector, Quaternion, Matrix
from ...io.imp.gltf2_io_user_extensions import import_user_extensions
from .gltf2_blender_scene import BlenderScene


class BlenderGlTF():
    """Main glTF import class."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf):
        """Create glTF main method, with optional profiling"""

        import_user_extensions('gather_import_gltf_before_hook', gltf)

        profile = bpy.app.debug_value == 102
        if profile:
            import cProfile
            import pstats
            import io
            from pstats import SortKey
            pr = cProfile.Profile()
            pr.enable()
            BlenderGlTF._create(gltf)
            pr.disable()
            s = io.StringIO()
            sortby = SortKey.TIME
            ps = pstats.Stats(pr, stream=s).sort_stats(sortby)
            ps.print_stats()
            print(s.getvalue())
        else:
            BlenderGlTF._create(gltf)

    @staticmethod
    def _create(gltf):
        """Create glTF main worker method."""
        BlenderGlTF.set_convert_functions(gltf)
        BlenderGlTF.pre_compute(gltf)
        BlenderScene.create(gltf)

    @staticmethod
    def set_convert_functions(gltf):
        if bpy.app.debug_value != 100:
            # Unit conversion factor in (Blender units) per meter
            u = 1.0 / bpy.context.scene.unit_settings.scale_length

            # glTF Y-Up space --> Blender Z-up space
            # X,Y,Z --> X,-Z,Y
            def convert_loc(x): return u * Vector([x[0], -x[2], x[1]])
            def convert_quat(q): return Quaternion([q[3], q[0], -q[2], q[1]])
            def convert_scale(s): return Vector([s[0], s[2], s[1]])

            def convert_matrix(m):
                return Matrix([
                    [m[0], -m[8], m[4], m[12] * u],
                    [-m[2], m[10], -m[6], -m[14] * u],
                    [m[1], -m[9], m[5], m[13] * u],
                    [m[3] / u, -m[11] / u, m[7] / u, m[15]],
                ])

            # Batch versions operate in place on a numpy array
            def convert_locs_batch(locs):
                # x,y,z -> x,-z,y
                locs[:, [1, 2]] = locs[:, [2, 1]]
                locs[:, 1] *= -1
                # Unit conversion
                if u != 1:
                    locs *= u

            def convert_normals_batch(ns):
                ns[:, [1, 2]] = ns[:, [2, 1]]
                ns[:, 1] *= -1

            # Correction for cameras and lights.
            # glTF: right = +X, forward = -Z, up = +Y
            # glTF after Yup2Zup: right = +X, forward = +Y, up = +Z
            # Blender: right = +X, forward = -Z, up = +Y
            # Need to carry Blender --> glTF after Yup2Zup
            gltf.camera_correction = Quaternion((2**0.5 / 2, 2**0.5 / 2, 0.0, 0.0))

        else:
            def convert_loc(x): return Vector(x)
            def convert_quat(q): return Quaternion([q[3], q[0], q[1], q[2]])
            def convert_scale(s): return Vector(s)

            def convert_matrix(m):
                return Matrix([m[0::4], m[1::4], m[2::4], m[3::4]])

            def convert_locs_batch(_locs): return
            def convert_normals_batch(_ns): return

            # Same convention, no correction needed.
            gltf.camera_correction = None

        gltf.loc_gltf_to_blender = convert_loc
        gltf.locs_batch_gltf_to_blender = convert_locs_batch
        gltf.quaternion_gltf_to_blender = convert_quat
        gltf.normals_batch_gltf_to_blender = convert_normals_batch
        gltf.scale_gltf_to_blender = convert_scale
        gltf.matrix_gltf_to_blender = convert_matrix

    @staticmethod
    def pre_compute(gltf):
        """Pre compute, just before creation."""
        # default scene used
        gltf.blender_scene = None

        # Check if there is animation on object
        # Init is to False, and will be set to True during creation
        gltf.animation_object = False

        # Blender material
        if gltf.data.materials:
            for material in gltf.data.materials:
                material.blender_material = {}

        # images
        if gltf.data.images is not None:
            for img in gltf.data.images:
                img.blender_image_name = None

        if gltf.data.nodes is None:
            # Something is wrong in file, there is no nodes
            return

        for node in gltf.data.nodes:
            # Weight animation management
            node.weight_animation = False

        # Meshes initialization
        if gltf.data.meshes:
            for mesh in gltf.data.meshes:
                mesh.blender_name = {}  # caches Blender mesh name

        if gltf.data.extensions_used is not None and "KHR_animation_pointer" in gltf.data.extensions_used:
            # Meshes initialization
            if gltf.data.meshes:
                for mesh in gltf.data.meshes:
                    mesh.blender_name = {}  # caches Blender mesh name
                    mesh.weight_animation_on_mesh = None  # For KHR_animation_pointer, weights on mesh

            for cam in gltf.data.cameras if gltf.data.cameras is not None else []:
                cam.animations = {}

            for mat in gltf.data.materials if gltf.data.materials is not None else []:
                mat.animations = {}
                if mat.normal_texture is not None:
                    mat.normal_texture.animations = {}
                if mat.occlusion_texture is not None:
                    mat.occlusion_texture.animations = {}
                if mat.emissive_texture is not None:
                    mat.emissive_texture.animations = {}
                if mat.pbr_metallic_roughness is not None:
                    mat.pbr_metallic_roughness.animations = {}

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
                        mat.extensions[ext]["animations"] = {}

                texs = [
                    mat.emissive_texture,
                    mat.normal_texture,
                    mat.occlusion_texture,
                    mat.pbr_metallic_roughness.base_color_texture if mat.pbr_metallic_roughness is not None else None,
                    mat.pbr_metallic_roughness.metallic_roughness_texture if mat.pbr_metallic_roughness is not None else None,
                ]

                for tex in [t for t in texs if t is not None]:
                    if tex.extensions is not None and "KHR_texture_transform" in tex.extensions:
                        tex.extensions["KHR_texture_transform"]["animations"] = {}

                texs_ext = [
                    mat.extensions["KHR_materials_volume"].get("thicknessTexture") if mat.extensions and "KHR_materials_volume" in mat.extensions else None,
                    mat.extensions["KHR_materials_transmission"].get("transmissionTexture") if mat.extensions and "KHR_materials_transmission" in mat.extensions else None,
                    mat.extensions["KHR_materials_specular"].get("specularTexture") if mat.extensions and "KHR_materials_specular" in mat.extensions else None,
                    mat.extensions["KHR_materials_specular"].get("specularColorTexture") if mat.extensions and "KHR_materials_specular" in mat.extensions else None,
                    mat.extensions["KHR_materials_sheen"].get("sheenColorTexture") if mat.extensions and "KHR_materials_sheen" in mat.extensions else None,
                    mat.extensions["KHR_materials_sheen"].get("sheenRoughnessTexture") if mat.extensions and "KHR_materials_sheen" in mat.extensions else None,
                    mat.extensions["KHR_materials_clearcoat"].get("clearcoatTexture") if mat.extensions and "KHR_materials_clearcoat" in mat.extensions else None,
                    mat.extensions["KHR_materials_clearcoat"].get("clearcoatRoughnessTexture") if mat.extensions and "KHR_materials_clearcoat" in mat.extensions else None,
                    mat.extensions["KHR_materials_clearcoat"].get("clearcoatNormalTexture") if mat.extensions and "KHR_materials_clearcoat" in mat.extensions else None,
                    mat.extensions["KHR_materials_anisotropy"].get("anisotropyTexture") if mat.extensions and "KHR_materials_anisotropy" in mat.extensions else None,
                ]

                for tex in [t for t in texs_ext if t is not None]:
                    if 'extensions' in tex and "KHR_texture_transform" in tex['extensions']:
                        tex['extensions']["KHR_texture_transform"]["animations"] = {}

            for light in gltf.data.extensions["KHR_lights_punctual"]["lights"] \
                if gltf.data.extensions is not None and "KHR_lights_punctual" in gltf.data.extensions \
                    and "lights" in gltf.data.extensions["KHR_lights_punctual"] else []:
                light["animations"] = {}
                if "spot" in light:
                    light["spot"]["animations"] = {}

        # Dispatch animation
        if gltf.data.animations:
            for node in gltf.data.nodes:
                node.animations = {}

            track_names = set()
            for anim_idx, anim in enumerate(gltf.data.animations):
                # Pick pair-wise unique name for each animation to use as a name
                # for its NLA tracks.
                desired_name = anim.name or "Anim_%d" % anim_idx
                # TRS animations & Pointer will be created as separate tracks
                anim.track_name = BlenderGlTF.find_unused_name(track_names, desired_name)
                track_names.add(anim.track_name)

                for channel_idx, channel in enumerate(anim.channels):
                    if channel.target.node is None:
                        # Manage KHR_animation_pointer for node TRS and weights
                        BlenderGlTF.dispatch_animation_pointer(gltf, anim, anim_idx, channel, channel_idx)

                    # Core glTF animations
                    else:
                        if anim_idx not in gltf.data.nodes[channel.target.node].animations.keys():
                            gltf.data.nodes[channel.target.node].animations[anim_idx] = []
                        gltf.data.nodes[channel.target.node].animations[anim_idx].append(channel_idx)
                        # Manage node with animation on weights, that are animated in meshes in Blender (ShapeKeys)
                        if channel.target.path == "weights":
                            gltf.data.nodes[channel.target.node].weight_animation = True

        # For KHR_animation_pointer, weight on meshes
        # We broadcast mesh weight animations to corresponding nodes
        if gltf.data.extensions_used is not None and "KHR_animation_pointer" in gltf.data.extensions_used:
            for node in gltf.data.nodes:
                if node.mesh is not None and gltf.data.meshes[node.mesh].weight_animation_on_mesh is not None:
                    anim_idx, channel_idx = gltf.data.meshes[node.mesh].weight_animation_on_mesh
                    if anim_idx not in node.animations.keys():
                        node.animations[anim_idx] = []
                    node.animations[anim_idx].append(channel_idx)
                    node.weight_animation = True

        # Calculate names for each mesh's shapekeys
        for mesh in gltf.data.meshes or []:
            mesh.shapekey_names = []
            used_names = set(['Basis'])  # Be sure to not use 'Basis' name at import, this is a reserved name

            # Look for primitive with morph targets
            for prim in (mesh.primitives or []):
                if not prim.targets:
                    continue

                for sk, _ in enumerate(prim.targets):
                    # Skip shape key for target that doesn't morph POSITION
                    morphs_position = any(
                        (prim.targets and 'POSITION' in prim.targets[sk])
                        for prim in mesh.primitives
                    )
                    if not morphs_position:
                        mesh.shapekey_names.append(None)
                        continue

                    shapekey_name = None

                    # Try to use name from extras.targetNames
                    try:
                        shapekey_name = str(mesh.extras['targetNames'][sk])
                        if shapekey_name == "":  # Issue when shapekey name is empty
                            shapekey_name = None
                    except Exception:
                        pass

                    # Try to get name from first primitive's POSITION accessor
                    if shapekey_name is None:
                        try:
                            shapekey_name = gltf.data.accessors[mesh.primitives[0].targets[sk]['POSITION']].name
                        except Exception:
                            pass

                    if shapekey_name is None:
                        shapekey_name = "target_" + str(sk)

                    shapekey_name = BlenderGlTF.find_unused_name(used_names, shapekey_name)
                    used_names.add(shapekey_name)

                    mesh.shapekey_names.append(shapekey_name)

                break

        # Manage KHR_materials_variants
        BlenderGlTF.manage_material_variants(gltf)

    @staticmethod
    def dispatch_animation_pointer(gltf, anim, anim_idx, channel, channel_idx):
        if channel.target.path != "pointer":
            return

        if channel.target.extensions is None:
            return

        if "KHR_animation_pointer" not in channel.target.extensions and "pointer" not in channel.target.extensions[
                "KHR_animation_pointer"]:
            return

        pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")

        ### Nodes and Meshes
        if len(pointer_tab) >= 4 and pointer_tab[1] == "nodes" and pointer_tab[3] in [
                "translation", "rotation", "scale", "weights"]:
            if anim_idx not in gltf.data.nodes[int(pointer_tab[2])].animations.keys():
                gltf.data.nodes[int(pointer_tab[2])].animations[anim_idx] = []
            gltf.data.nodes[int(pointer_tab[2])].animations[anim_idx].append(channel_idx)
            if pointer_tab[3] == "weights":
                gltf.data.nodes[int(pointer_tab[2])].weight_animation = True
        elif len(pointer_tab) >= 4 and pointer_tab[1] == "meshes" and pointer_tab[3] == "weights":
            gltf.data.meshes[int(pointer_tab[2])].weight_animation_on_mesh = (anim_idx, channel_idx)

        # Camera
        if len(pointer_tab) == 5 and pointer_tab[1] == "cameras" and \
                pointer_tab[3] in ["perspective"] and \
                pointer_tab[4] in ["yfov", "znear", "zfar"]:

            if anim_idx not in gltf.data.cameras[int(pointer_tab[2])].animations.keys():
                gltf.data.cameras[int(pointer_tab[2])].animations[anim_idx] = []
            gltf.data.cameras[int(pointer_tab[2])].animations[anim_idx].append(channel_idx)

        if len(pointer_tab) == 5 and pointer_tab[1] == "cameras" and \
                pointer_tab[3] in ["orthographic"] and \
                pointer_tab[4] in ["ymag", "xmag"]:

            if anim_idx not in gltf.data.cameras[int(pointer_tab[2])].animations.keys():
                gltf.data.cameras[int(pointer_tab[2])].animations[anim_idx] = []
            gltf.data.cameras[int(pointer_tab[2])].animations[anim_idx].append(channel_idx)

        # Light
        if len(pointer_tab) == 6 and pointer_tab[1] == "extensions" and \
                pointer_tab[2] == "KHR_lights_punctual" and \
                pointer_tab[3] == "lights" and \
                pointer_tab[5] in ["intensity", "color", "range"]:

            if anim_idx not in gltf.data.extensions["KHR_lights_punctual"]["lights"][int(
                    pointer_tab[4])]["animations"].keys():
                gltf.data.extensions["KHR_lights_punctual"]["lights"][int(pointer_tab[4])]["animations"][anim_idx] = []
            gltf.data.extensions["KHR_lights_punctual"]["lights"][int(
                pointer_tab[4])]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "extensions" and \
                pointer_tab[2] == "KHR_lights_punctual" and \
                pointer_tab[3] == "lights" and \
                pointer_tab[5] in ["spot.outerConeAngle", "spot.innerConeAngle"]:

            if anim_idx not in gltf.data.extensions["KHR_lights_punctual"]["lights"][int(
                    pointer_tab[4])]["animations"].keys():
                gltf.data.extensions["KHR_lights_punctual"]["lights"][int(pointer_tab[4])]["animations"][anim_idx] = []
            gltf.data.extensions["KHR_lights_punctual"]["lights"][int(
                pointer_tab[4])]["animations"][anim_idx].append(channel_idx)

        # Materials
        if len(pointer_tab) == 4 and pointer_tab[1] == "materials" and \
                pointer_tab[3] in ["emissiveFactor", "alphaCutoff"]:

            if anim_idx not in gltf.data.materials[int(pointer_tab[2])].animations.keys():
                gltf.data.materials[int(pointer_tab[2])].animations[anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].animations[anim_idx].append(channel_idx)

        if len(pointer_tab) == 7 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "emissiveTexture" and \
                pointer_tab[4] == "extensions" and \
                pointer_tab[5] == "KHR_texture_transform" and \
                pointer_tab[6] in ["scale", "offset"]:

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].emissive_texture.extensions["KHR_texture_transform"]["animations"].keys():
                gltf.data.materials[int(
                    pointer_tab[2])].emissive_texture.extensions["KHR_texture_transform"]["animations"][anim_idx] = []
            gltf.data.materials[int(
                pointer_tab[2])].emissive_texture.extensions["KHR_texture_transform"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "normalTexture" and \
                pointer_tab[4] == "scale":

            if anim_idx not in gltf.data.materials[int(pointer_tab[2])].normal_texture.animations.keys():
                gltf.data.materials[int(pointer_tab[2])].normal_texture.animations[anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].normal_texture.animations[anim_idx].append(channel_idx)

        if len(pointer_tab) == 7 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "normalTexture" and \
                pointer_tab[4] == "extensions" and \
                pointer_tab[5] == "KHR_texture_transform" and \
                pointer_tab[6] in ["scale", "offset"]:

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].normal_texture.extensions["KHR_texture_transform"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].normal_texture.extensions["KHR_texture_transform"]["animations"][anim_idx] = []
            gltf.data.materials[int(
                pointer_tab[2])].normal_texture.extensions["KHR_texture_transform"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "occlusionTexture" and \
                pointer_tab[4] == "strength":

            if anim_idx not in gltf.data.materials[int(pointer_tab[2])].occlusion_texture.animations.keys():
                gltf.data.materials[int(pointer_tab[2])].occlusion_texture.animations[anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].occlusion_texture.animations[anim_idx].append(channel_idx)

        if len(pointer_tab) == 7 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "occlusionTexture" and \
                pointer_tab[4] == "extensions" and \
                pointer_tab[5] == "KHR_texture_transform" and \
                pointer_tab[6] in ["scale", "offset"]:

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].occlusion_texture.extensions["KHR_texture_transform"]["animations"].keys():
                gltf.data.materials[int(
                    pointer_tab[2])].occlusion_texture.extensions["KHR_texture_transform"]["animations"][anim_idx] = []
            gltf.data.materials[int(
                pointer_tab[2])].occlusion_texture.extensions["KHR_texture_transform"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 9 and pointer_tab[1] == "materials" and \
                pointer_tab[-1] in ["scale", "offset"] and \
                pointer_tab[-2] == "KHR_texture_transform" and \
                pointer_tab[-3] == "extensions" and \
                pointer_tab[3] == "extensions":

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions[pointer_tab[4]][pointer_tab[5]]['extensions']["KHR_texture_transform"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])].extensions[pointer_tab[4]][pointer_tab[5]
                                                                                    ]['extensions']["KHR_texture_transform"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].extensions[pointer_tab[4]][pointer_tab[5]
                                                                                ]['extensions']["KHR_texture_transform"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "pbrMetallicRoughness" and \
                pointer_tab[4] in ["baseColorFactor", "roughnessFactor", "metallicFactor"]:

            # This can be unlit (baseColorFactor) or pbr

            if anim_idx not in gltf.data.materials[int(pointer_tab[2])].pbr_metallic_roughness.animations.keys():
                gltf.data.materials[int(pointer_tab[2])].pbr_metallic_roughness.animations[anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].pbr_metallic_roughness.animations[anim_idx].append(channel_idx)

        if len(pointer_tab) == 8 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "pbrMetallicRoughness" and \
                pointer_tab[4] == "baseColorTexture" and \
                pointer_tab[5] == "extensions" and \
                pointer_tab[6] == "KHR_texture_transform" and \
                pointer_tab[7] in ["scale", "offset"]:

            # This can be unlit or pbr

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].pbr_metallic_roughness.base_color_texture.extensions["KHR_texture_transform"]["animations"].keys():
                gltf.data.materials[int(
                    pointer_tab[2])].pbr_metallic_roughness.base_color_texture.extensions["KHR_texture_transform"]["animations"][anim_idx] = []
            gltf.data.materials[int(
                pointer_tab[2])].pbr_metallic_roughness.base_color_texture.extensions["KHR_texture_transform"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 8 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "pbrMetallicRoughness" and \
                pointer_tab[4] == "metallicRoughnessTexture" and \
                pointer_tab[5] == "extensions" and \
                pointer_tab[6] == "KHR_texture_transform" and \
                pointer_tab[7] in ["scale", "offset"]:

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].pbr_metallic_roughness.metallic_roughness_texture.extensions["KHR_texture_transform"]["animations"].keys():
                gltf.data.materials[int(
                    pointer_tab[2])].pbr_metallic_roughness.metallic_roughness_texture.extensions["KHR_texture_transform"]["animations"][anim_idx] = []
            gltf.data.materials[int(
                pointer_tab[2])].pbr_metallic_roughness.metallic_roughness_texture.extensions["KHR_texture_transform"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_emissive_strength" and \
                pointer_tab[5] == "emissiveStrength":

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_emissive_strength"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].extensions["KHR_materials_emissive_strength"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].extensions["KHR_materials_emissive_strength"]["animations"][anim_idx].append(
                channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_volume" and \
                pointer_tab[5] in ["thicknessFactor", "attenuationDistance", "attenuationColor"]:

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_volume"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])].extensions["KHR_materials_volume"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_volume"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_ior" and \
                pointer_tab[5] == "ior":

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_ior"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])].extensions["KHR_materials_ior"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_ior"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_transmission" and \
                pointer_tab[5] == "transmissionFactor":

            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_transmission"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].extensions["KHR_materials_transmission"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])].extensions["KHR_materials_transmission"]["animations"][anim_idx].append(
                channel_idx)

        if len(pointer_tab) == 7 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_clearcoat" and \
                pointer_tab[5] == "clearcoatNormalTexture" and \
                pointer_tab[6] == "scale":
            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_clearcoat"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].extensions["KHR_materials_clearcoat"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_clearcoat"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_clearcoat" and \
                pointer_tab[5] in ["clearcoatFactor", "clearcoatRoughnessFactor"]:
            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_clearcoat"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].extensions["KHR_materials_clearcoat"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_clearcoat"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_sheen" and \
                pointer_tab[5] in ["sheenColorFactor", "sheenRoughnessFactor"]:
            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_sheen"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])].extensions["KHR_materials_sheen"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_sheen"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_specular" and \
                pointer_tab[5] in ["specularFactor", "specularColorFactor"]:
            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_specular"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].extensions["KHR_materials_specular"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_specular"]["animations"][anim_idx].append(channel_idx)

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_anisotropy" and \
                pointer_tab[5] in ["anisotropyStrength", "anisotropyRotation"]:
            if anim_idx not in gltf.data.materials[int(
                    pointer_tab[2])].extensions["KHR_materials_anisotropy"]["animations"].keys():
                gltf.data.materials[int(pointer_tab[2])
                                    ].extensions["KHR_materials_anisotropy"]["animations"][anim_idx] = []
            gltf.data.materials[int(pointer_tab[2])
                                ].extensions["KHR_materials_anisotropy"]["animations"][anim_idx].append(channel_idx)

    @staticmethod
    def find_unused_name(haystack, desired_name):
        """Finds a name not in haystack and <= 63 UTF-8 bytes.
        (the limit on the size of a Blender name.)
        If a is taken, tries a.001, then a.002, etc.
        """
        stem = desired_name[:63]
        suffix = ''
        cntr = 1
        while True:
            name = stem + suffix

            if len(name.encode('utf-8')) > 63:
                stem = stem[:-1]
                continue

            if name not in haystack:
                return name

            suffix = '.%03d' % cntr
            cntr += 1

    @staticmethod
    def manage_material_variants(gltf):
        if not (gltf.data.extensions is not None and 'KHR_materials_variants' in gltf.data.extensions.keys()):
            gltf.KHR_materials_variants = False
            return

        gltf.KHR_materials_variants = True
        # If there is no KHR_materials_variants data in scene, create it
        if bpy.context.preferences.addons['io_scene_gltf2'].preferences.KHR_materials_variants_ui is False:
            bpy.context.preferences.addons['io_scene_gltf2'].preferences.KHR_materials_variants_ui = True
            # Setting preferences as dirty, to be sure that option is saved
            bpy.context.preferences.is_dirty = True

        if len(bpy.data.scenes[0].gltf2_KHR_materials_variants_variants) > 0:
            bpy.data.scenes[0].gltf2_KHR_materials_variants_variants.clear()

        for idx_variant, variant in enumerate(gltf.data.extensions['KHR_materials_variants']['variants']):
            var = bpy.data.scenes[0].gltf2_KHR_materials_variants_variants.add()
            var.name = variant['name']
            var.variant_idx = idx_variant
