#
# Copyright 2011-2014 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# <pep8 compliant>

import bpy

from bpy.app.handlers import persistent


def check_is_new_shading_ntree(node_tree):
    for node in node_tree.nodes:
        # If material has any node with ONLY new shading system
        # compatibility then it's considered a Cycles material
        # and versioning code would need to perform on it.
        #
        # We can not check for whether NEW_SHADING in compatibility
        # because some nodes could have compatibility with both old
        # and new shading system and they can't be used for any
        # decision here.
        if node.shading_compatibility == {'NEW_SHADING'}:
            return True

        # If node is only compatible with old shading system
        # then material can not be Cycles material and we
        # can stopiterating nodes now.
        if node.shading_compatibility == {'OLD_SHADING'}:
            return False
    return False


def check_is_new_shading_material(material):
    if not material.node_tree:
        return False
    return check_is_new_shading_ntree(material.node_tree)


def check_is_new_shading_world(world):
    if not world.node_tree:
        return False
    return check_is_new_shading_ntree(world.node_tree)


def check_is_new_shading_lamp(lamp):
    if not lamp.node_tree:
        return False
    return check_is_new_shading_ntree(lamp.node_tree)


def foreach_notree_node(nodetree, callback, traversed):
    if nodetree in traversed:
        return
    traversed.add(nodetree)
    for node in nodetree.nodes:
        callback(node)
        if node.bl_idname == 'ShaderNodeGroup':
            foreach_notree_node(node.node_tree, callback, traversed)


def foreach_cycles_node(callback):
    traversed = set()
    for material in bpy.data.materials:
        if check_is_new_shading_material(material):
                foreach_notree_node(material.node_tree,
                                    callback,
                                    traversed)
    for world in bpy.data.worlds:
        if check_is_new_shading_world(world):
                foreach_notree_node(world.node_tree,
                                    callback,
                                    traversed)
    for lamp in bpy.data.lamps:
        if check_is_new_shading_world(lamp):
                foreach_notree_node(lamp.node_tree,
                                    callback,
                                    traversed)


def displacement_node_insert(material, nodetree, traversed):
    if nodetree in traversed:
        return
    traversed.add(nodetree)

    for node in nodetree.nodes:
        if node.bl_idname == 'ShaderNodeGroup':
            displacement_node_insert(material, node.node_tree, traversed)

    # Gather links to replace
    displacement_links = []
    for link in nodetree.links:
        if link.to_node.bl_idname == 'ShaderNodeOutputMaterial' and \
           link.from_node.bl_idname != 'ShaderNodeDisplacement' and \
           link.to_socket.identifier == 'Displacement':
           displacement_links.append(link)

    # Replace links with displacement node
    for link in displacement_links:
        from_node = link.from_node
        from_socket = link.from_socket
        to_node = link.to_node
        to_socket = link.to_socket

        nodetree.links.remove(link)

        node = nodetree.nodes.new(type='ShaderNodeDisplacement')
        node.location[0] = 0.5 * (from_node.location[0] + to_node.location[0]);
        node.location[1] = 0.5 * (from_node.location[1] + to_node.location[1]);
        node.inputs['Scale'].default_value = 0.1
        node.inputs['Midlevel'].default_value = 0.0

        nodetree.links.new(from_socket, node.inputs['Height'])
        nodetree.links.new(node.outputs['Displacement'], to_socket)

def displacement_nodes_insert():
    traversed = set()
    for material in bpy.data.materials:
        if check_is_new_shading_material(material):
            displacement_node_insert(material, material.node_tree, traversed)

def displacement_principled_nodes(node):
    if node.bl_idname == 'ShaderNodeDisplacement':
        if node.space != 'WORLD':
            node.space = 'OBJECT'
    if node.bl_idname == 'ShaderNodeBsdfPrincipled':
        if node.subsurface_method != 'RANDOM_WALK':
            node.subsurface_method = 'BURLEY'


def mapping_node_order_flip(node):
    """
    Flip euler order of mapping shader node
    """
    if node.bl_idname == 'ShaderNodeMapping':
        rot = node.rotation.copy()
        rot.order = 'ZYX'
        quat = rot.to_quaternion()
        node.rotation = quat.to_euler('XYZ')


def vector_curve_node_remap(node):
    """
    Remap values of vector curve node from normalized to absolute values
    """
    if node.bl_idname == 'ShaderNodeVectorCurve':
        node.mapping.use_clip = False
        for curve in node.mapping.curves:
            for point in curve.points:
                point.location.x = (point.location.x * 2.0) - 1.0
                point.location.y = (point.location.y - 0.5) * 2.0
        node.mapping.update()


def custom_bake_remap(scene):
    """
    Remap bake types into the new types and set the flags accordingly
    """
    bake_lookup = (
        'COMBINED',
        'AO',
        'SHADOW',
        'NORMAL',
        'UV',
        'EMIT',
        'ENVIRONMENT',
        'DIFFUSE_DIRECT',
        'DIFFUSE_INDIRECT',
        'DIFFUSE_COLOR',
        'GLOSSY_DIRECT',
        'GLOSSY_INDIRECT',
        'GLOSSY_COLOR',
        'TRANSMISSION_DIRECT',
        'TRANSMISSION_INDIRECT',
        'TRANSMISSION_COLOR',
        'SUBSURFACE_DIRECT',
        'SUBSURFACE_INDIRECT',
        'SUBSURFACE_COLOR')

    diffuse_direct_idx = bake_lookup.index('DIFFUSE_DIRECT')

    cscene = scene.cycles

    # Old bake type
    bake_type_idx = cscene.get("bake_type")

    if bake_type_idx is None:
        cscene.bake_type = 'COMBINED'
        return

    # File doesn't need versioning
    if bake_type_idx < diffuse_direct_idx:
        return

    # File needs versioning
    bake_type = bake_lookup[bake_type_idx]
    cscene.bake_type, end = bake_type.split('_')

    if end == 'DIRECT':
        scene.render.bake.use_pass_indirect = False
        scene.render.bake.use_pass_color = False

    elif end == 'INDIRECT':
        scene.render.bake.use_pass_direct = False
        scene.render.bake.use_pass_color = False

    elif end == 'COLOR':
        scene.render.bake.use_pass_direct = False
        scene.render.bake.use_pass_indirect = False


@persistent
def do_versions(self):
    if bpy.context.user_preferences.version <= (2, 78, 1):
        prop = bpy.context.user_preferences.addons[__package__].preferences
        system = bpy.context.user_preferences.system
        if not prop.is_property_set("compute_device_type"):
            # Device might not currently be available so this can fail
            try:
                if system.legacy_compute_device_type == 1:
                    prop.compute_device_type = 'OPENCL'
                elif system.legacy_compute_device_type == 2:
                    prop.compute_device_type = 'CUDA'
                else:
                    prop.compute_device_type = 'NONE'
            except:
                pass

            # Init device list for UI
            prop.get_devices()

    # We don't modify startup file because it assumes to
    # have all the default values only.
    if not bpy.data.is_saved:
        return

    # Clamp Direct/Indirect separation in 270
    if bpy.data.version <= (2, 70, 0):
        for scene in bpy.data.scenes:
            cscene = scene.cycles
            sample_clamp = cscene.get("sample_clamp", False)
            if (sample_clamp and
                not cscene.is_property_set("sample_clamp_direct") and
                not cscene.is_property_set("sample_clamp_indirect")):

                cscene.sample_clamp_direct = sample_clamp
                cscene.sample_clamp_indirect = sample_clamp

    # Change of Volume Bounces in 271
    if bpy.data.version <= (2, 71, 0):
        for scene in bpy.data.scenes:
            cscene = scene.cycles
            if not cscene.is_property_set("volume_bounces"):
                cscene.volume_bounces = 1

    # Caustics Reflective/Refractive separation in 272
    if bpy.data.version <= (2, 72, 0):
        for scene in bpy.data.scenes:
            cscene = scene.cycles
            if (cscene.get("no_caustics", False) and
                not cscene.is_property_set("caustics_reflective") and
                not cscene.is_property_set("caustics_refractive")):

                cscene.caustics_reflective = False
                cscene.caustics_refractive = False

    # Euler order was ZYX in previous versions.
    if bpy.data.version <= (2, 73, 4):
        foreach_cycles_node(mapping_node_order_flip)

    if bpy.data.version <= (2, 76, 5):
        foreach_cycles_node(vector_curve_node_remap)

    # Baking types changed
    if bpy.data.version <= (2, 76, 6):
        for scene in bpy.data.scenes:
            custom_bake_remap(scene)

    # Several default changes for 2.77
    if bpy.data.version <= (2, 76, 8):
        for scene in bpy.data.scenes:
            cscene = scene.cycles

            # Samples
            if not cscene.is_property_set("samples"):
                cscene.samples = 10

            # Preview Samples
            if not cscene.is_property_set("preview_samples"):
                cscene.preview_samples = 10

            # Filter
            if not cscene.is_property_set("filter_type"):
                cscene.pixel_filter_type = 'GAUSSIAN'

            # Tile Order
            if not cscene.is_property_set("tile_order"):
                cscene.tile_order = 'CENTER'

        for lamp in bpy.data.lamps:
            clamp = lamp.cycles

            # MIS
            if not clamp.is_property_set("use_multiple_importance_sampling"):
                clamp.use_multiple_importance_sampling = False

        for mat in bpy.data.materials:
            cmat = mat.cycles

            # Volume Sampling
            if not cmat.is_property_set("volume_sampling"):
                cmat.volume_sampling = 'DISTANCE'

    if bpy.data.version <= (2, 76, 9):
        for world in bpy.data.worlds:
            cworld = world.cycles

            # World MIS
            if not cworld.is_property_set("sample_as_light"):
                cworld.sample_as_light = False

            # World MIS Samples
            if not cworld.is_property_set("samples"):
                cworld.samples = 4

            # World MIS Resolution
            if not cworld.is_property_set("sample_map_resolution"):
                cworld.sample_map_resolution = 256

    if bpy.data.version <= (2, 76, 10):
        for scene in bpy.data.scenes:
            cscene = scene.cycles
            if cscene.is_property_set("filter_type"):
                if not cscene.is_property_set("pixel_filter_type"):
                    cscene.pixel_filter_type = cscene.filter_type
                if cscene.filter_type == 'BLACKMAN_HARRIS':
                    cscene.filter_type = 'GAUSSIAN'

    if bpy.data.version <= (2, 78, 2):
        for scene in bpy.data.scenes:
            cscene = scene.cycles
            if not cscene.is_property_set("light_sampling_threshold"):
                cscene.light_sampling_threshold = 0.0

    if bpy.data.version <= (2, 79, 0):
        for scene in bpy.data.scenes:
            cscene = scene.cycles
            # Default changes
            if not cscene.is_property_set("aa_samples"):
                cscene.aa_samples = 4
            if not cscene.is_property_set("preview_aa_samples"):
                cscene.preview_aa_samples = 4
            if not cscene.is_property_set("blur_glossy"):
                cscene.blur_glossy = 0.0
            if not cscene.is_property_set("sample_clamp_indirect"):
                cscene.sample_clamp_indirect = 0.0

    if bpy.data.version <= (2, 79, 1):
        displacement_nodes_insert()

    if bpy.data.version <= (2, 79, 2):
        for mat in bpy.data.materials:
            cmat = mat.cycles
            if not cmat.is_property_set("displacement_method"):
                cmat.displacement_method = 'BUMP'

        foreach_cycles_node(displacement_principled_nodes)
