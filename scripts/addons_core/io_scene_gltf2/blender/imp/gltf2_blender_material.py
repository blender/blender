# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy

from ...io.imp.gltf2_io_user_extensions import import_user_extensions
from ..com.gltf2_blender_extras import set_extras
from .gltf2_blender_pbrMetallicRoughness import MaterialHelper, pbr_metallic_roughness
from .gltf2_blender_KHR_materials_pbrSpecularGlossiness import pbr_specular_glossiness
from .gltf2_blender_KHR_materials_unlit import unlit


class BlenderMaterial():
    """Blender Material."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf, material_idx, vertex_color):
        """Material creation."""
        pymaterial = gltf.data.materials[material_idx]

        import_user_extensions('gather_import_material_before_hook', gltf, pymaterial, vertex_color)

        name = pymaterial.name
        if name is None:
            name = "Material_" + str(material_idx)

        mat = bpy.data.materials.new(name)
        pymaterial.blender_material[vertex_color] = mat.name

        set_extras(mat, pymaterial.extras)
        BlenderMaterial.set_double_sided(pymaterial, mat)
        BlenderMaterial.set_eevee_blend_method(pymaterial, mat)
        BlenderMaterial.set_viewport_color(pymaterial, mat, vertex_color)

        mat.use_nodes = True
        while mat.node_tree.nodes:  # clear all nodes
            mat.node_tree.nodes.remove(mat.node_tree.nodes[0])

        mh = MaterialHelper(gltf, pymaterial, mat, vertex_color)

        exts = pymaterial.extensions or {}
        if 'KHR_materials_unlit' in exts:
            unlit(mh)
            pymaterial.pbr_metallic_roughness.blender_nodetree = mat.node_tree  # Used in case of for KHR_animation_pointer
            # Used in case of for KHR_animation_pointer #TODOPointer Vertex Color...
            pymaterial.pbr_metallic_roughness.blender_mat = mat
        elif 'KHR_materials_pbrSpecularGlossiness' in exts:
            pbr_specular_glossiness(mh)
        else:
            pbr_metallic_roughness(mh)
            pymaterial.pbr_metallic_roughness.blender_nodetree = mat.node_tree  # Used in case of for KHR_animation_pointer
            # Used in case of for KHR_animation_pointer #TODOPointer Vertex Color...
            pymaterial.pbr_metallic_roughness.blender_mat = mat

        # Manage KHR_materials_variants
        # We need to store link between material idx in glTF and Blender Material id
        if gltf.KHR_materials_variants is True:
            gltf.variant_mapping[str(material_idx) + str(vertex_color)] = mat

        pymaterial.blender_nodetree = mat.node_tree  # Used in case of for KHR_animation_pointer
        pymaterial.blender_mat = mat  # Used in case of for KHR_animation_pointer #TODOPointer Vertex Color...

        import_user_extensions('gather_import_material_after_hook', gltf, pymaterial, vertex_color, mat)

    @staticmethod
    def set_double_sided(pymaterial, mat):
        mat.use_backface_culling = (pymaterial.double_sided != True)

    @staticmethod
    def set_eevee_blend_method(pymaterial, mat):
        alpha_mode = pymaterial.alpha_mode or 'OPAQUE'
        if alpha_mode == 'OPAQUE':
            mat.blend_method = 'OPAQUE'
        elif alpha_mode == 'BLEND':
            mat.blend_method = 'BLEND'
        elif alpha_mode == 'MASK':
            # Alpha clipping is done with nodes, NOT with the
            # blend_method, since the blend_method only affects Eevee
            # legacy. Using the CLIP method here just hints to Eevee
            # that the alpha is always 0 or 1.
            mat.blend_method = 'CLIP'
            mat.alpha_threshold = 0.5

    @staticmethod
    def set_viewport_color(pymaterial, mat, vertex_color):
        # If there is no texture and no vertex color, use the base color as
        # the color for the Solid view.
        if vertex_color:
            return

        exts = pymaterial.extensions or {}
        if 'KHR_materials_pbrSpecularGlossiness' in exts:
            # TODO
            return
        else:
            pbr = pymaterial.pbr_metallic_roughness
            if pbr is None or pbr.base_color_texture is not None:
                return
            color = pbr.base_color_factor or [1, 1, 1, 1]

        mat.diffuse_color = color
