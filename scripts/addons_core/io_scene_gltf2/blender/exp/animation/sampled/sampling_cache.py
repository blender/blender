# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import mathutils
import bpy
import typing
from .....blender.com.data_path import get_sk_exported
from .....blender.com.conversion import inverted_trs_mapping_node, texture_transform_blender_to_gltf, yvof_blender_to_gltf
from ...cache import datacache
from ...tree import VExportNode
from ..drivers import get_sk_drivers

# Warning : If you change some parameter here, need to be changed in cache system


@datacache
def get_cache_data(path: str,
                   blender_obj_uuid: str,
                   bone: typing.Optional[str],
                   action_name: str,
                   current_frame: int,
                   step: int,
                   slot_identifier: str,
                   export_settings,
                   only_gather_provided=False
                   ):

    data = {}

    # Ranges are stored at action level, so no need to give the slot_identifier here
    min_, max_ = get_range(blender_obj_uuid, action_name, export_settings)

    if only_gather_provided:
        # If object is not in vtree, this is a material or light for pointers
        obj_uuids = [blender_obj_uuid] if blender_obj_uuid in export_settings['vtree'].nodes.keys() else []
    else:
        obj_uuids = [uid for (uid, n) in export_settings['vtree'].nodes.items()
                     if n.blender_type not in [VExportNode.BONE]]

    # For TRACK mode, we reset cache after each track export, so we don't need to keep others objects
    if export_settings['gltf_animation_mode'] in "NLA_TRACKS":
        # If object is not in vtree, this is a material or light for pointers
        obj_uuids = [blender_obj_uuid] if blender_obj_uuid in export_settings['vtree'].nodes.keys() else []

    # If there is only 1 object to cache, we can disable viewport for other objects (for performance)
    # This can be on these cases:
    # - TRACK mode
    # - Only one object to cache (but here, no really useful for performance)
    # - Action mode, where some object have multiple actions
        # - For this case, on first call, we will cache active action for all objects
        # - On next calls, we will cache only the action of current object, so we can disable viewport for others

    need_to_enable_again = False
    if export_settings['gltf_optimize_disable_viewport'] is True and len(obj_uuids) == 1:
        need_to_enable_again = True
        # Before baking, disabling from viewport all meshes
        for obj in [n.blender_object for n in export_settings['vtree'].nodes.values() if n.blender_type in
                    [VExportNode.OBJECT, VExportNode.ARMATURE, VExportNode.COLLECTION]]:
            if obj is None:
                continue
            obj.hide_viewport = True
        export_settings['vtree'].nodes[obj_uuids[0]].blender_object.hide_viewport = False
    # Work for drivers on shapekeys is already done at start of animation export

    depsgraph = bpy.context.evaluated_depsgraph_get()

    frame = min_
    while frame <= max_:
        bpy.context.scene.frame_set(int(frame))
        current_instance = {}  # For GN instances, we are going to track instances by their order in instance iterator

        object_caching(data, obj_uuids, current_instance, action_name, slot_identifier, frame, depsgraph, export_settings)

        # KHR_animation_pointer caching for materials, lights, cameras
        if export_settings['gltf_export_anim_pointer'] is True:
            material_nodetree_caching(data, action_name, slot_identifier, frame, export_settings)
            material_caching(data, action_name, slot_identifier, frame, export_settings)
            light_nodetree_caching(data, action_name, slot_identifier, frame, export_settings)
            camera_caching(data, action_name, slot_identifier, frame, export_settings)

        frame += step

    # And now, restoring meshes in viewport
    for node, obj in [(n, n.blender_object) for n in export_settings['vtree'].nodes.values() if n.blender_type in
                      [VExportNode.OBJECT, VExportNode.ARMATURE, VExportNode.COLLECTION]]:
        obj.hide_viewport = node.default_hide_viewport

    return data

# For perf, we may be more precise, and get a list of ranges to be exported that include all needed frames


def get_range(obj_uuid, key, export_settings):
    if export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
        return export_settings['ranges'][obj_uuid][key]['start'], export_settings['ranges'][obj_uuid][key]['end']
    else:
        min_ = None
        max_ = None
        for obj in export_settings['ranges'].keys():
            for anim in export_settings['ranges'][obj].keys():
                if min_ is None or min_ > export_settings['ranges'][obj][anim]['start']:
                    min_ = export_settings['ranges'][obj][anim]['start']
                if max_ is None or max_ < export_settings['ranges'][obj][anim]['end']:
                    max_ = export_settings['ranges'][obj][anim]['end']
    return min_, max_


def initialize_data_dict(data, key1, key2, key3, key4, key5):
    # No check on key1, this is already done before calling this function
    if key2 not in data[key1].keys():
        data[key1][key2] = {}
        data[key1][key2][key3] = {}
        data[key1][key2][key3][key4] = {}
        data[key1][key2][key3][key4][key5] = {}

    if key3 not in data[key1][key2].keys():
        data[key1][key2][key3] = {}
        data[key1][key2][key3][key4] = {}
        data[key1][key2][key3][key4][key5] = {}


def material_caching(data, action_name, slot_identifier, frame, export_settings):
    for mat in export_settings['KHR_animation_pointer']['materials'].keys():
        if len(export_settings['KHR_animation_pointer']['materials'][mat]['paths']) == 0:
            continue

        blender_material = [m for m in bpy.data.materials if id(m) == mat]
        if len(blender_material) == 0:
            # This is not a material from Blender (coming from Geometry Node for example, so no animation on it)
            continue
        else:
            blender_material = blender_material[0]
        if mat not in data.keys():
            data[mat] = {}

        if blender_material and blender_material.animation_data and blender_material.animation_data.action \
                and blender_material.animation_data.action_slot \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS"]:
            key1, key2, key3, key4 = mat, blender_material.animation_data.action.name, blender_material.animation_data.action_slot.identifier, "value"
        elif export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
            # We can keep the input slot_identifier here, as we are caching only one object / NLA track
            key1, key2, key3, key4 = mat, action_name, slot_identifier, "value"
        else:
            # case of baking materials (scene export).
            # There is no animation, so use id as key
            # slot_identifier is always None for scene export
            key1, key2, key3, key4 = mat, mat, slot_identifier, "value"

        if key2 not in data[key1].keys():
            data[key1][key2] = {}
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}

            for path in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        if key3 not in data[key1][key2].keys():
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}

            for path in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        for path in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys():

            if path.startswith("node_tree"):
                continue

            val = blender_material.path_resolve(path)
            if type(val).__name__ == "float":
                data[key1][key2][key3][key4][path][frame] = val
            else:
                data[key1][key2][key3][key4][path][frame] = list(val)


def material_nodetree_caching(data, action_name, slot_identifier, frame, export_settings):
    # After caching objects, caching materials, for KHR_animation_pointer
    for mat in export_settings['KHR_animation_pointer']['materials'].keys():
        if len(export_settings['KHR_animation_pointer']['materials'][mat]['paths']) == 0:
            continue

        blender_material = [m for m in bpy.data.materials if id(m) == mat]
        if len(blender_material) == 0:
            # This is not a material from Blender (coming from Geometry Node for example, so no animation on it)
            continue
        else:
            blender_material = blender_material[0]
        if mat not in data.keys():
            data[mat] = {}

        if blender_material.node_tree and blender_material.node_tree.animation_data and blender_material.node_tree.animation_data.action \
                and blender_material.node_tree.animation_data.action_slot \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS"]:
            key1, key2, key3, key4 = mat, blender_material.node_tree.animation_data.action.name, blender_material.node_tree.animation_data.action_slot.identifier, "value"
        elif export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
            # We can keep the input slot_identifier here, as we are caching only one object / NLA track
            key1, key2, key3, key4 = mat, action_name, slot_identifier, "value"
        else:
            # case of baking materials (scene export).
            # There is no animation, so use id as key
            # slot_identifier is always None for scene export
            key1, key2, key3, key4 = mat, mat, slot_identifier, "value"

        if key2 not in data[key1].keys():
            data[key1][key2] = {}
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        if key3 not in data[key1][key2].keys():
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        baseColorFactor_alpha_merged_already_done = False
        for path in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys():

            if not path.startswith("node_tree"):
                continue

            # Manage special case where we merge baseColorFactor and alpha
            if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'] == "/materials/XXX/pbrMetallicRoughness/baseColorFactor" \
                    and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['length'] == 3:
                if baseColorFactor_alpha_merged_already_done is True:
                    continue
                val_color = blender_material.path_resolve(path)
                data_color = list(val_color)[:export_settings['KHR_animation_pointer']
                                             ['materials'][mat]['paths'][path]['length']]
                if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['additional_path'] is not None:
                    val_alpha = blender_material.path_resolve(
                        export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['additional_path'])
                else:
                    val_alpha = 1.0
                data[key1][key2][key3][key4][path][frame] = data_color + [val_alpha]
                baseColorFactor_alpha_merged_already_done = True
            # Manage special case where we merge baseColorFactor and alpha
            elif export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'] == "/materials/XXX/pbrMetallicRoughness/baseColorFactor" \
                    and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['length'] == 1:
                if baseColorFactor_alpha_merged_already_done is True:
                    continue
                val_alpha = blender_material.path_resolve(path)
                if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['additional_path'] is not None:
                    val_color = blender_material.path_resolve(
                        export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['additional_path'])
                    data_color = list(val_color)[:export_settings['KHR_animation_pointer']
                                                 ['materials'][mat]['paths']['additional_path']['length']]
                else:
                    data_color = [1.0, 1.0, 1.0]
                data[key1][key2][key3][key4][path][frame] = data_color + [val_alpha]
                baseColorFactor_alpha_merged_already_done = True

            # Manage special case for KHR_texture_transform offset, that needs
            # rotation and scale too (and not only translation)
            elif "KHR_texture_transform" in export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'] \
                    and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'].endswith("offset"):

                val_offset = blender_material.path_resolve(path)
                rotation_path = [
                    i for i in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys() if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][i]['path'].rsplit(
                        "/",
                        1)[0] == export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'].rsplit(
                        "/",
                        1)[0] and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][i]['path'].rsplit(
                        "/",
                        1)[1] == "rotation"][0]
                val_rotation = blender_material.path_resolve(rotation_path)
                scale_path = [
                    i for i in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys() if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][i]['path'].rsplit(
                        "/",
                        1)[0] == export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'].rsplit(
                        "/",
                        1)[0] and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][i]['path'].rsplit(
                        "/",
                        1)[1] == "scale"][0]
                val_scale = blender_material.path_resolve(scale_path)

                mapping_transform = {}
                mapping_transform["offset"] = [val_offset[0], val_offset[1]]
                mapping_transform["rotation"] = val_rotation
                mapping_transform["scale"] = [val_scale[0], val_scale[1]]

                if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['vector_type'] == "TEXTURE":
                    mapping_transform = inverted_trs_mapping_node(mapping_transform)
                    if mapping_transform is None:
                        # Can not be converted to TRS, so ... keeping default values
                        export_settings['log'].warning(
                            "Can not convert texture transform to TRS. Keeping default values.")
                        mapping_transform = {}
                        mapping_transform["offset"] = [0.0, 0.0]
                        mapping_transform["rotation"] = 0.0
                        mapping_transform["scale"] = [1.0, 1.0]
                elif export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['vector_type'] == "VECTOR":
                    # Vectors don't get translated
                    mapping_transform["offset"] = [0, 0]

                texture_transform = texture_transform_blender_to_gltf(mapping_transform)

                data[key1][key2][key3][key4][path][frame] = texture_transform['offset']
                data[key1][key2][key3][key4][rotation_path][frame] = texture_transform['rotation']
                data[key1][key2][key3][key4][scale_path][frame] = texture_transform['scale']
                if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['vector_type'] != "VECTOR":
                    # Already handled by offset
                    continue
                else:
                    val = blender_material.path_resolve(path)
                    mapping_transform = {}
                    mapping_transform["offset"] = [0, 0]  # Placeholder, not needed
                    mapping_transform["rotation"] = val
                    mapping_transform["scale"] = [1, 1]  # Placeholder, not needed
                    texture_transform = texture_transform_blender_to_gltf(mapping_transform)
                    data[key1][key2][key3][key4][path][frame] = texture_transform['rotation']
            elif "KHR_texture_transform" in export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'] \
                    and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'].endswith("scale"):
                if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['vector_type'] != "VECTOR":
                    # Already handled by offset
                    continue
                else:
                    val = blender_material.path_resolve(path)
                    mapping_transform = {}
                    mapping_transform["offset"] = [0, 0]  # Placeholder, not needed
                    mapping_transform["rotation"] = 0.0  # Placeholder, not needed
                    mapping_transform["scale"] = [val[0], val[1]]
                    texture_transform = texture_transform_blender_to_gltf(mapping_transform)
                    data[key1][key2][key3][key4][path][frame] = texture_transform['rotation']

            # Manage special cases for specularFactor & specularColorFactor
            elif export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'] == "/materials/XXX/extensions/KHR_materials_specular/specularFactor":
                val = blender_material.path_resolve(path)
                val = val * 2.0
                if val > 1.0:
                    fac = val
                    val = 1.0
                else:
                    fac = 1.0

                data[key1][key2][key3][key4][path][frame] = val

                # Retrieve specularColorFactor
                colorfactor_path = [
                    i for i in export_settings['KHR_animation_pointer']['materials'][mat]['paths'].keys() if export_settings['KHR_animation_pointer']['materials'][mat]['paths'][i]['path'].rsplit(
                        "/",
                        1)[0] == export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'].rsplit(
                        "/",
                        1)[0] and export_settings['KHR_animation_pointer']['materials'][mat]['paths'][i]['path'].rsplit(
                        "/",
                        1)[1] == "specularColorFactor"][0]
                val_colorfactor = blender_material.path_resolve(colorfactor_path)
                if fac > 1.0:
                    val_colorfactor = [i * fac for i in val_colorfactor]
                data[key1][key2][key3][key4][colorfactor_path][frame] = val_colorfactor
            elif export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['path'] == "/materials/XXX/extensions/KHR_materials_specular/specularColorFactor":
                # Already handled by specularFactor
                continue

            # Classic case
            else:
                val = blender_material.path_resolve(path)
                if type(val).__name__ == "float":
                    data[key1][key2][key3][key4][path][frame] = val
                else:
                    data[key1][key2][key3][key4][path][frame] = list(val)[
                        :export_settings['KHR_animation_pointer']['materials'][mat]['paths'][path]['length']]


def armature_caching(data, obj_uuid, blender_obj, action_name, slot_identifier, frame, export_settings):
    bones = export_settings['vtree'].get_all_bones(obj_uuid)
    if blender_obj.animation_data and blender_obj.animation_data.action \
            and blender_obj.animation_data.action_slot \
            and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS", "BROADCAST"]:
        key1, key2, key3, key4 = obj_uuid, blender_obj.animation_data.action.name, blender_obj.animation_data.action_slot.identifier, "bone"
    elif blender_obj.animation_data \
            and export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
        # We can keep the input slot_identifier here, as we are caching only one object / NLA track
        key1, key2, key3, key4 = obj_uuid, action_name, slot_identifier, "bone"
    else:
        # slot_identifier is always None for scene export
        key1, key2, key3, key4 = obj_uuid, obj_uuid, slot_identifier, "bone"

    if key3 not in data[key1][key2].keys():
        data[key1][key2][key3] = {}
        data[key1][key2][key3][key4] = {}
    if key4 not in data[key1][key2][key3].keys():
        data[key1][key2][key3][key4] = {}

    for bone_uuid in [bone for bone in bones if export_settings['vtree'].nodes[bone].leaf_reference is None]:
        blender_bone = export_settings['vtree'].nodes[bone_uuid].blender_bone

        if export_settings['vtree'].nodes[bone_uuid].parent_uuid is not None and export_settings['vtree'].nodes[
                export_settings['vtree'].nodes[bone_uuid].parent_uuid].blender_type == VExportNode.BONE:
            blender_bone_parent = export_settings['vtree'].nodes[export_settings['vtree']
                                                                 .nodes[bone_uuid].parent_uuid].blender_bone
            rest_mat = blender_bone_parent.bone.matrix_local.inverted_safe() @ blender_bone.bone.matrix_local
            matrix = rest_mat.inverted_safe() @ blender_bone_parent.matrix.inverted_safe() @ blender_bone.matrix
        else:
            if blender_bone.parent is None:
                matrix = blender_bone.bone.matrix_local.inverted_safe() @ blender_bone.matrix
            else:
                # Bone has a parent, but in export, after filter, is at root of armature
                matrix = blender_bone.matrix.copy()

            # Because there is no armature object, we need to apply the TRS of armature to the root bone
            if export_settings['gltf_armature_object_remove'] is True:
                matrix = matrix @ blender_obj.matrix_world

        if blender_bone.name not in data[key1][key2][key3][key4].keys():
            data[key1][key2][key3][key4][blender_bone.name] = {}
        data[key1][key2][key3][key4][blender_bone.name][frame] = matrix


def object_caching(data, obj_uuids, current_instance, action_name, slot_identifier, frame, depsgraph, export_settings):
    for obj_uuid in obj_uuids:

        # Do not cache real collection
        if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
            continue

        blender_obj = export_settings['vtree'].nodes[obj_uuid].blender_object
        if blender_obj is None:  # GN instance
            if export_settings['vtree'].nodes[obj_uuid].parent_uuid not in current_instance.keys():
                current_instance[export_settings['vtree'].nodes[obj_uuid].parent_uuid] = 0

        # TODO: we may want to avoid looping on all objects, but an accurate filter must be found

        # calculate local matrix
        if export_settings['vtree'].nodes[obj_uuid].parent_uuid is None:
            parent_mat = mathutils.Matrix.Identity(4).freeze()
        else:
            if export_settings['vtree'].nodes[export_settings['vtree'].nodes[obj_uuid].parent_uuid].blender_type not in [
                    VExportNode.BONE]:
                if export_settings['vtree'].nodes[export_settings['vtree']
                                                  .nodes[obj_uuid].parent_uuid].blender_type != VExportNode.COLLECTION:
                    parent_mat = export_settings['vtree'].nodes[export_settings['vtree']
                                                                .nodes[obj_uuid].parent_uuid].blender_object.matrix_world
                else:
                    parent_mat = export_settings['vtree'].nodes[export_settings['vtree']
                                                                .nodes[obj_uuid].parent_uuid].matrix_world
            else:
                # Object animated is parented to a bone
                blender_bone = export_settings['vtree'].nodes[export_settings['vtree']
                                                              .nodes[obj_uuid].parent_bone_uuid].blender_bone
                armature_object = export_settings['vtree'].nodes[export_settings['vtree']
                                                                 .nodes[export_settings['vtree'].nodes[obj_uuid].parent_bone_uuid].armature].blender_object
                axis_basis_change = mathutils.Matrix(
                    ((1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, -1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))

                parent_mat = armature_object.matrix_world @ blender_bone.matrix @ axis_basis_change

        # For object inside collection (at root), matrix world is already expressed regarding collection parent
        if export_settings['vtree'].nodes[obj_uuid].parent_uuid is not None and export_settings['vtree'].nodes[
                export_settings['vtree'].nodes[obj_uuid].parent_uuid].blender_type == VExportNode.INST_COLLECTION:
            parent_mat = mathutils.Matrix.Identity(4).freeze()

        if blender_obj:
            mat = parent_mat.inverted_safe() @ blender_obj.matrix_world
        else:
            eval = export_settings['vtree'].nodes[export_settings['vtree'].nodes[obj_uuid].parent_uuid].blender_object.evaluated_get(
                depsgraph)
            cpt_inst = 0
            for inst in depsgraph.object_instances:  # use only as iterator
                if inst.parent == eval:
                    if current_instance[export_settings['vtree'].nodes[obj_uuid].parent_uuid] == cpt_inst:
                        mat = inst.matrix_world.copy()
                        current_instance[export_settings['vtree'].nodes[obj_uuid].parent_uuid] += 1
                        break
                    cpt_inst += 1

        if obj_uuid not in data.keys():
            data[obj_uuid] = {}

        if blender_obj and blender_obj.animation_data and blender_obj.animation_data.action \
                and blender_obj.animation_data.action_slot \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS", "BROADCAST"]:
            key1, key2, key3, key4, key5 = obj_uuid, blender_obj.animation_data.action.name, blender_obj.animation_data.action_slot.identifier, "matrix", None
        elif export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
            # We can keep the input slot_identifier here, as we are caching only one object / NLA track
            key1, key2, key3, key4, key5 = obj_uuid, action_name, slot_identifier, "matrix", None
        else:
            # case of baking object.
            # There is no animation, so use uuid of object as key
            # slot_identifier is always None for scene export
            key1, key2, key3, key4, key5 = obj_uuid, obj_uuid, slot_identifier, "matrix", None

        initialize_data_dict(data, key1, key2, key3, key4, key5)
        data[key1][key2][key3][key4][key5][frame] = mat

        # Store data for all bones, if object is an armature

        if blender_obj and blender_obj.type == "ARMATURE":
            armature_caching(data, obj_uuid, blender_obj, action_name, slot_identifier, frame, export_settings)

        elif blender_obj is None:  # GN instances
            # case of baking object, for GN instances
            # There is no animation, so use uuid of object as key
            # slot_identifier is always None for baking
            key1, key2, key3, key4, key5 = obj_uuid, obj_uuid, slot_identifier, "matrix", None
            initialize_data_dict(data, key1, key2, key3, key4, key5)
            data[key1][key2][key3][key4][key5][frame] = mat

        # Check SK animation here, as we are caching data
        # This will avoid to have to do it again when exporting SK animation
        cache_sk = False
        if export_settings['gltf_morph_anim'] and blender_obj and blender_obj.type == "MESH" \
                and blender_obj.data is not None \
                and blender_obj.data.shape_keys is not None \
                and blender_obj.data.shape_keys.animation_data is not None \
                and blender_obj.data.shape_keys.animation_data.action is not None \
                and blender_obj.data.shape_keys.animation_data.action_slot is not None \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS", "BROADCAST"]:

            key1, key2, key3, key4, key5 = obj_uuid, blender_obj.data.shape_keys.animation_data.action.name, blender_obj.data.shape_keys.animation_data.action_slot.identifier, "sk", None
            cache_sk = True

        elif export_settings['gltf_morph_anim'] and blender_obj and blender_obj.type == "MESH" \
                and blender_obj.data is not None \
                and blender_obj.data.shape_keys is not None \
                and blender_obj.data.shape_keys.animation_data is not None \
                and export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:

            # We can keep the input slot_identifier here, as we are caching only one object / NLA track
            key1, key2, key3, key4, key5 = obj_uuid, action_name, slot_identifier, "sk", None
            cache_sk = True

        elif export_settings['gltf_morph_anim'] and blender_obj and blender_obj.type == "MESH" \
                and blender_obj.data is not None \
                and blender_obj.data.shape_keys is not None:
            # slot_identifier is always None for scene export
            key1, key2, key3, key4, key5 = obj_uuid, obj_uuid, slot_identifier, "sk", None
            cache_sk = True

        if cache_sk:
            initialize_data_dict(data, key1, key2, key3, key4, key5)
            if key4 not in data[key1][key2][key3].keys():
                data[key1][key2][key3][key4] = {}
                data[key1][key2][key3][key4][key5] = {}
            if key5 not in data[key1][key2][key3][key4].keys():
                data[key1][key2][key3][key4][key5] = {}
            data[key1][key2][key3][key4][key5][frame] = [
                k.value for k in get_sk_exported(
                    blender_obj.data.shape_keys.key_blocks)]
            cache_sk = False

        # caching driver sk meshes
        # This will avoid to have to do it again when exporting SK animation
        if blender_obj and blender_obj.type == "ARMATURE":
            sk_drivers = get_sk_drivers(obj_uuid, export_settings)
            for dr_obj in sk_drivers:
                cache_sk = False
                driver_object = export_settings['vtree'].nodes[dr_obj].blender_object
                if dr_obj not in data.keys():
                    data[dr_obj] = {}
                if blender_obj.animation_data and blender_obj.animation_data.action \
                        and blender_obj.animation_data.action_slot \
                        and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS", "BROADCAST"]:
                    key1, key2, key3, key4, key5 = dr_obj, obj_uuid + "_" + blender_obj.animation_data.action.name, blender_obj.animation_data.action_slot.identifier, "sk", None
                    cache_sk = True
                elif blender_obj.animation_data \
                        and export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
                    # We can keep the input slot_identifier here, as we are caching only one object / NLA track
                    key1, key2, key3, key4, key5 = dr_obj, obj_uuid + "_" + action_name, slot_identifier, "sk", None
                    cache_sk = True
                else:
                    # slot_identifier is always None for scene export
                    key1, key2, key3, key4, key5 = dr_obj, obj_uuid + "_" + obj_uuid, slot_identifier, "sk", None
                    cache_sk = True

                if cache_sk:
                    initialize_data_dict(data, key1, key2, key3, key4, key5)
                    if export_settings['gltf_optimize_disable_viewport'] is True:
                        # Retrieve data from custom properties instead of shape keys
                        data[key1][key2][key3][key4][key5][frame] = list(
                            blender_obj['gltf_' + dr_obj])  # This include only exported SK
                    else:
                        data[key1][key2][key3][key4][key5][frame] = [
                            k.value for k in get_sk_exported(
                                driver_object.data.shape_keys.key_blocks)]
                    cache_sk = False


def light_nodetree_caching(data, action_name, slot_identifier, frame, export_settings):
    # After caching materials, caching lights, for KHR_animation_pointer
    for light in export_settings['KHR_animation_pointer']['lights'].keys():
        if len(export_settings['KHR_animation_pointer']['lights'][light]['paths']) == 0:
            continue

        blender_light = [m for m in bpy.data.lights if id(m) == light][0]
        if light not in data.keys():
            data[light] = {}

        if blender_light.node_tree and blender_light.node_tree.animation_data and blender_light.node_tree.animation_data.action \
                and blender_light.node_tree.animation_data.action_slot \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS"]:
            key1, key2, key3, key4 = light, blender_light.node_tree.animation_data.action.name, blender_light.node_tree.animation_data.action_slot.identifier, "value"
        elif export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
            # We can keep the input slot_identifier here, as we are caching only one object / NLA track
            key1, key2, key3, key4 = light, action_name, slot_identifier, "value"
        else:
            # case of baking materials (scene export).
            # There is no animation, so use id as key
            # slot_identifier is always None for scene export
            key1, key2, key3, key4 = light, light, slot_identifier, "value"

        if key2 not in data[key1].keys():
            data[key1][key2] = {}
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['lights'][light]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}
        if key3 not in data[key1][key2].keys():
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['lights'][light]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        for path in export_settings['KHR_animation_pointer']['lights'][light]['paths'].keys():
            val = blender_light.path_resolve(path)
            if type(val).__name__ == "float":
                data[key1][key2][key3][key4][path][frame] = val
            else:
                data[key1][key2][key3][key4][path][frame] = list(val)


def light_caching(data, action_name, slot_identifier, frame, export_settings):
    # After caching materials, caching lights, for KHR_animation_pointer
    for light in export_settings['KHR_animation_pointer']['lights'].keys():
        if len(export_settings['KHR_animation_pointer']['lights'][light]['paths']) == 0:
            continue

        blender_light = [m for m in bpy.data.lights if id(m) == light][0]
        if light not in data.keys():
            data[light] = {}

        if blender_light and blender_light.animation_data and blender_light.animation_data.action \
                and blender_light.animation_data.action_slot \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS"]:
            key1, key2, key3, key4 = light, blender_light.animation_data.action.name, blender_light.animation_data.action_slot.identifier, "value"
        elif export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
            key1, key2, key3, key4 = light, action_name, slot_identifier, "value"
        else:
            # case of baking materials (scene export).
            # There is no animation, so use id as key
            # slot_identifier is always None for scene export
            key1, key2, key3, key4 = light, light, slot_identifier, "value"

        if key2 not in data[key1].keys():
            data[key1][key2] = {}
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['lights'][light]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        if key3 not in data[key1][key2].keys():
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['lights'][light]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        for path in export_settings['KHR_animation_pointer']['lights'][light]['paths'].keys():
            # Manage special case for innerConeAngle because it requires spot_size & spot_blend
            if export_settings['KHR_animation_pointer']['lights'][light]['paths'][path]['path'] == "/extensions/KHR_lights_punctual/lights/XXX/spot.innerConeAngle":
                val = blender_light.path_resolve(path)
                val_size = blender_light.path_resolve(
                    export_settings['KHR_animation_pointer']['lights'][light]['paths'][path]['additional_path'])
                data[key1][key2][key3][key4][path][frame] = (val_size * 0.5) - ((val_size * 0.5) * val)
            else:
                # classic case
                val = blender_light.path_resolve(path)
                if type(val).__name__ == "float":
                    data[key1][key2][key3][path][frame] = val
                else:
                    # When color is coming from a node, it is 4 values (RGBA), so need to convert it to 3 values (RGB)
                    if export_settings['KHR_animation_pointer']['lights'][light]['paths'][path]['length'] == 3 and len(
                            val) == 4:
                        val = val[:3]
                    data[key1][key2][key3][key4][path][frame] = list(val)


def camera_caching(data, action_name, slot_identifier, frame, export_settings):
    # After caching lights, caching cameras, for KHR_animation_pointer
    for cam in export_settings['KHR_animation_pointer']['cameras'].keys():
        if len(export_settings['KHR_animation_pointer']['cameras'][cam]['paths']) == 0:
            continue

        blender_camera = [m for m in bpy.data.cameras if id(m) == cam][0]
        if cam not in data.keys():
            data[cam] = {}

        if blender_camera and blender_camera.animation_data and blender_camera.animation_data.action \
                and blender_camera.animation_data.action_slot \
                and export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS"]:
            key1, key2, key3, key4 = cam, blender_camera.animation_data.action.name, blender_camera.animation_data.action_slot.identifier, "value"
        elif export_settings['gltf_animation_mode'] in ["NLA_TRACKS"]:
            # We can keep the input slot_identifier here, as we are caching only one object / NLA track
            key1, key2, key3, key4 = cam, action_name, slot_identifier, "value"
        else:
            # case of baking camera data (scene export).
            # There is no animation, so use id as key
            # No really matter for slot_identifier, as we bake all when exporting with scene export
            key1, key2, key3, key4 = cam, cam, slot_identifier, "value"

        if key2 not in data[key1].keys():
            data[key1][key2] = {}
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['cameras'][cam]['paths'].keys():
                data[key1][key2][key3][key4][path] = {}

        if key3 not in data[key1][key2].keys():
            data[key1][key2][key3] = {}
            data[key1][key2][key3][key4] = {}
            for path in export_settings['KHR_animation_pointer']['cameras'][cam]['paths'].keys():
                data[key1][key2][key3][ley4][path] = {}

        for path in export_settings['KHR_animation_pointer']['cameras'][cam]['paths'].keys():
            _render = bpy.context.scene.render
            width = _render.pixel_aspect_x * _render.resolution_x
            height = _render.pixel_aspect_y * _render.resolution_y
            del _render
            # Manage special case for yvof because it requires sensor_fit, aspect ratio, angle
            if export_settings['KHR_animation_pointer']['cameras'][cam]['paths'][path]['path'] == "/cameras/XXX/perspective/yfov":
                val = yvof_blender_to_gltf(blender_camera.angle, width, height, blender_camera.sensor_fit)
                data[key1][key2][key3][key4][path][frame] = val
            # Manage special case for xmag because it requires ortho_scale & scene data
            elif export_settings['KHR_animation_pointer']['cameras'][cam]['paths'][path]['path'] == "/cameras/XXX/orthographic/xmag":
                val = blender_camera.ortho_scale
                data[key1][key2][key3][key4][path][frame] = val * (width / max(width, height)) / 2.0
            # Manage special case for ymag because it requires ortho_scale  & scene data
            elif export_settings['KHR_animation_pointer']['cameras'][cam]['paths'][path]['path'] == "/cameras/XXX/orthographic/ymag":
                val = blender_camera.ortho_scale
                data[key1][key2][key3][key4][path][frame] = val * (height / max(width, height)) / 2.0
            else:
                # classic case
                val = blender_camera.path_resolve(path)
                if type(val).__name__ == "float":
                    data[key1][key2][key3][key4][path][frame] = val
                else:
                    data[key1][key2][key3][key4][path][frame] = list(val)
