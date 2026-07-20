# SPDX-FileCopyrightText: 2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .. import joints as gltf2_blender_gather_joints


def gather_animated_node(type_element, elem_uuid, bone, export_settings):
    if type_element == 'OBJECT':

        if bone is not None:
            return gltf2_blender_gather_joints.gather_joint_vnode(
                export_settings['vtree'].nodes[elem_uuid].bones[bone], export_settings)
        else:
            return export_settings['vtree'].nodes[elem_uuid].node

    elif type_element == "MESH":
        return export_settings['vtree'].nodes[elem_uuid].node.mesh

    elif type_element == "KEY":
        return export_settings['vtree'].nodes[elem_uuid].node

    elif type_element == "MATERIAL":
        return export_settings['material_identifiers'][elem_uuid]['gltf']

    elif type_element == "NODETREE":
        return export_settings['material_identifiers'][elem_uuid]['gltf']

    elif type_element == "LIGHT":
        return export_settings['KHR_animation_pointer'][None]['lights'][elem_uuid]['glTF_light']

    elif type_element == "CAMERA":
        return export_settings['KHR_animation_pointer'][None]['cameras'][elem_uuid]['glTF_camera']

    else:
        return None
        # Not implemented (yet)


def gather_blender_element(blender_main_type, blender_type_data, blender_id, export_settings):
    on_type = None
    blender_bone_name = None
    if blender_main_type is None:
        if blender_type_data == "materials":
            blender_element = export_settings['material_identifiers'][blender_id]['blender']
            on_type = "MATERIAL"
        elif blender_type_data == "cameras":
            blender_element = [cam for cam in bpy.data.cameras if id(cam) == blender_id][0]
            on_type = "CAMERA"
        elif blender_type_data == "lights":
            blender_element = [light for light in bpy.data.lights if id(light) == blender_id][0]
            on_type = "LIGHT"
        else:
            pass  # Should not happen
    else:
        if blender_type_data == "materials":
            blender_element = export_settings['material_identifiers'][blender_id]['blender']
            on_type = "MATERIAL"
        elif blender_type_data == "meshes":
            blender_element = export_settings['mesh_identifiers'][blender_id]['blender']
            on_type = "MESH"
        elif blender_type_data == "objects":
            blender_element = [obj for obj in bpy.data.objects if id(obj) == blender_id][0]
            on_type = "OBJECT"
        elif blender_type_data == "cameras":
            blender_element = [cam for cam in bpy.data.cameras if id(cam) == blender_id][0]
            on_type = "CAMERA"
        elif blender_type_data == "lights":
            blender_element = [light for light in bpy.data.lights if id(light) == blender_id][0]
            on_type = "LIGHT"
        elif blender_type_data == "bones":
            blender_element = export_settings['KHR_animation_pointer']['extras']['bones'][blender_id]['blender_armature_object']
            blender_bone_name = export_settings['KHR_animation_pointer']['extras']['bones'][blender_id]['blender_bone_name']
            on_type = "OBJECT"
        else:
            pass  # Should not happen

    return blender_element, blender_bone_name, on_type


def gather_animated_node_for_data(blender_main_type, blender_type_data, blender_id, bone_name, export_settings):
    if blender_main_type is None:
        if blender_type_data == "materials":
            return export_settings['KHR_animation_pointer'][blender_main_type]['materials'][blender_id]['glTF_material']
        elif blender_type_data == "lights":
            return export_settings['KHR_animation_pointer'][blender_main_type]['lights'][blender_id]['glTF_light']
        elif blender_type_data == "cameras":
            return export_settings['KHR_animation_pointer'][blender_main_type]['cameras'][blender_id]['glTF_camera']
        else:
            pass  # This should never happen
    elif blender_main_type == "extras":
        if export_settings['gltf_extras'] and export_settings['gltf_export_anim_pointer']:
            if export_settings['gltf_animation_mode'] in ["ACTIONS", "ACTIVE_ACTIONS"]:
                if blender_type_data == "meshes":
                    used_blender_id = export_settings['vtree'].nodes[blender_id].mesh_id
                elif blender_type_data == "objects":
                    used_blender_id = export_settings['vtree'].nodes[blender_id].blender_object_id
                elif blender_type_data == "bones":
                    bone_uuid = export_settings['vtree'].nodes[blender_id[0]].bones[bone_name]
                    used_blender_id = id(export_settings['vtree'].nodes[bone_uuid].blender_bone)
                else:
                    used_blender_id = blender_id
            else:
                if blender_type_data == "bones":
                    bone_uuid = export_settings['vtree'].nodes[blender_id[0]].bones[bone_name]
                    used_blender_id = id(export_settings['vtree'].nodes[bone_uuid].blender_bone)
                else:
                    used_blender_id = blender_id
            return export_settings['KHR_animation_pointer'][blender_main_type][blender_type_data][used_blender_id]['glTF_extras']
    else:
        pass  # This should never happen


def gather_animated_blender_id(blender_main_type, blender_type_data, blender_id, bone_name, export_settings):
    if blender_main_type == "extras":
        if export_settings['gltf_animation_mode'] in ["ACTIONS", "ACTIVE_ACTIONS"]:
            if blender_type_data == "meshes":
                used_blender_id = export_settings['vtree'].nodes[blender_id].mesh_id
            elif blender_type_data == "objects":
                used_blender_id = export_settings['vtree'].nodes[blender_id].blender_object_id
            elif blender_type_data == "bones" and bone_name is not None:
                bone_uuid = export_settings['vtree'].nodes[blender_id[0]].bones[bone_name]
                used_blender_id = id(export_settings['vtree'].nodes[bone_uuid].blender_bone)
            else:
                used_blender_id = blender_id
        else:
            if blender_type_data == "bones" and bone_name is not None:
                bone_uuid = export_settings['vtree'].nodes[blender_id[0]].bones[bone_name]
                used_blender_id = id(export_settings['vtree'].nodes[bone_uuid].blender_bone)
            else:
                used_blender_id = blender_id
    else:
        used_blender_id = blender_id

    return used_blender_id


def get_impacted_data(id_type):

    return {
        'OBJECT': 'nodes',
        'BONE': 'nodes',
        'MESH': 'meshes',
        'MATERIAL': 'materials',
        'NODETREE': 'materials',
        'LIGHT': 'extensions/KHR_lights_punctual/lights',
        'CAMERA': 'cameras'}.get(id_type)
    # Warning for devs: there is another dict in generate_extras
    # Please keep them in sync for new entries


def get_gltf_name_from_blender_property(id_type, elem_uuid, blender_property, export_settings):
    # Retrieve data in paths to get corresponding glTF names from Blender ones
    id_type = {'LIGHT': 'lights', 'CAMERA': 'cameras', 'MATERIAL': 'materials'}.get(id_type)

    if id_type is None:
        return blender_property

    for path in export_settings['KHR_animation_pointer'][None][id_type][elem_uuid]['paths'].keys():
        if path == blender_property:
            return export_settings['KHR_animation_pointer'][None][id_type][elem_uuid]['paths'][path]['path'].split(
                "/")[-1]

    # Of course, there are always exceptions :)
    if id_type == "cameras":
        return {'lens': 'yfov'}.get(blender_property, blender_property)

    return blender_property
