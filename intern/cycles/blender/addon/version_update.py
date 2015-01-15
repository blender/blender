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


def check_is_new_shading_material(material):
    if not material.node_tree:
        return False
    for node in material.node_tree.nodes:
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


def mapping_node_order_flip(node):
    """
    Flip euler order of mapping shader node
    """
    if node.bl_idname == 'ShaderNodeMapping':
        rot = node.rotation.copy()
        rot.order = 'ZYX'
        quat = rot.to_quaternion()
        node.rotation = quat.to_euler('XYZ')


@persistent
def do_versions(self):
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
