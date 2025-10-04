# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy

from ...io.imp.user_extensions import import_user_extensions
from ..com.extras import set_extras
from .pbrMetallicRoughness import MaterialHelper, pbr_metallic_roughness
from .KHR_materials_pbrSpecularGlossiness import pbr_specular_glossiness
from .KHR_materials_unlit import unlit


class BlenderMaterial():
    """Blender Material."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf, material_idx, vertex_color):
        """Material creation."""

        if material_idx is None:
            # If no material is specified, we create a default one
            mat = bpy.data.materials.new(name="DefaultMaterial")
            mat.node_tree.nodes.clear()
            output_node = mat.node_tree.nodes.new(type='ShaderNodeOutputMaterial')
            output_node.location = (0, 0)
            shader_node = mat.node_tree.nodes.new(type='ShaderNodeBsdfPrincipled')
            shader_node.location = (-200, 0)
            mat.node_tree.links.new(shader_node.outputs[0], output_node.inputs[0])
            if vertex_color:
                # Add vertex color node
                vertex_color_node = mat.node_tree.nodes.new(type='ShaderNodeVertexColor')
                vertex_color_node.location = (-400, 0)
                mat.node_tree.links.new(vertex_color_node.outputs[0], shader_node.inputs[0])
            return mat.name


        pymaterial = gltf.data.materials[material_idx]

        import_user_extensions('gather_import_material_before_hook', gltf, pymaterial, vertex_color)

        name = pymaterial.name
        if name is None:
            name = "Material_" + str(material_idx)

        mat = bpy.data.materials.new(name)
        pymaterial.blender_material[vertex_color] = mat.name

        set_extras(mat, pymaterial.extras)
        BlenderMaterial.set_double_sided(pymaterial, mat)
        BlenderMaterial.set_eevee_surface_render_method(pymaterial, mat)
        BlenderMaterial.set_viewport_color(pymaterial, mat, vertex_color)

        mat.node_tree.nodes.clear()

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
    def set_eevee_surface_render_method(pymaterial, mat):
        alpha_mode = pymaterial.alpha_mode or 'OPAQUE'
        if alpha_mode in ['OPAQUE', 'MASK']:
            mat.surface_render_method = 'DITHERED'
        else:
            mat.surface_render_method = 'BLENDED'

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
