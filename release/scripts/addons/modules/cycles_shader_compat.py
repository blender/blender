# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy

__all__ = (
    "CyclesShaderWrapper",
    )


class CyclesShaderWrapper():
    """
    Hard coded shader setup.
    Suitable for importers, adds basic:
    diffuse/spec/alpha/normal/bump/reflect.
    """
    __slots__ = (
        "material",

        "node_out",
        "node_mix_shader_spec",
        "node_mix_shader_alpha",
        "node_mix_shader_refl",

        "node_bsdf_alpha",
        "node_bsdf_diff",
        "node_bsdf_spec",
        "node_bsdf_refl",

        "node_mix_color_alpha",
        "node_mix_color_diff",
        "node_mix_color_spec",
        "node_mix_color_hard",
        "node_mix_color_refl",
        "node_mix_color_bump",

        "node_normalmap",
        "node_texcoords",

        "node_image_alpha",
        "node_image_diff",
        "node_image_spec",
        "node_image_hard",
        "node_image_refl",
        "node_image_bump",
        "node_image_normalmap",
        )

    _col_size = 200
    _row_size = 220

    def __init__(self, material):

        COLOR_WHITE = 1.0, 1.0, 1.0, 1.0
        COLOR_BLACK = 0.0, 0.0, 0.0, 1.0

        self.material = material
        self.material.use_nodes = True

        tree = self.material.node_tree

        nodes = tree.nodes
        links = tree.links
        nodes.clear()

        # ----
        # Add shaders
        node = nodes.new(type='ShaderNodeOutputMaterial')
        node.label = "Material Out"
        node.location = self._grid_location(6, 4)
        self.node_out = node
        del node

        node = nodes.new(type='ShaderNodeAddShader')
        node.label = "Shader Add Refl"
        node.location = self._grid_location(5, 4)
        self.node_mix_shader_refl = node
        del node
        # Link
        links.new(self.node_mix_shader_refl.outputs["Shader"],
                  self.node_out.inputs["Surface"])

        node = nodes.new(type='ShaderNodeAddShader')
        node.label = "Shader Add Spec"
        node.location = self._grid_location(4, 4)
        self.node_mix_shader_spec = node
        del node
        # Link
        links.new(self.node_mix_shader_spec.outputs["Shader"],
                  self.node_mix_shader_refl.inputs[0])

        # --------------------------------------------------------------------
        # Reflection
        node = nodes.new(type='ShaderNodeBsdfRefraction')
        node.label = "Refl BSDF"
        node.location = self._grid_location(6, 1)
        node.mute = True  # unmute on use
        self.node_bsdf_refl = node
        del node
        # Link
        links.new(self.node_bsdf_refl.outputs["BSDF"],
                  self.node_mix_shader_refl.inputs[1])

        # Mix Refl Color
        node = nodes.new(type='ShaderNodeMixRGB')
        node.label = "Mix Color/Refl"
        node.location = self._grid_location(5, 1)
        node.blend_type = 'MULTIPLY'
        node.inputs["Fac"].default_value = 1.0
        # reverse of most other mix nodes
        node.inputs["Color1"].default_value = COLOR_WHITE  # color
        node.inputs["Color2"].default_value = COLOR_BLACK  # factor
        self.node_mix_color_refl = node
        del node
        # Link
        links.new(self.node_mix_color_refl.outputs["Color"],
                  self.node_bsdf_refl.inputs["Color"])

        # --------------------------------------------------------------------
        # Alpha

        # ----
        # Mix shader
        node = nodes.new(type='ShaderNodeMixShader')
        node.label = "Shader Mix Alpha"
        node.location = self._grid_location(3, 4)
        node.inputs["Fac"].default_value = 1.0  # no alpha by default
        self.node_mix_shader_alpha = node
        del node
        # Link
        links.new(self.node_mix_shader_alpha.outputs["Shader"],
                  self.node_mix_shader_spec.inputs[0])

        # Alpha BSDF
        node = nodes.new(type='ShaderNodeBsdfTransparent')
        node.label = "Alpha BSDF"
        node.location = self._grid_location(2, 4)
        node.mute = True  # unmute on use
        self.node_bsdf_alpha = node
        del node
        # Link
        links.new(self.node_bsdf_alpha.outputs["BSDF"],
                  self.node_mix_shader_alpha.inputs[1])  # first 'Shader'

        # Mix Alpha Color
        node = nodes.new(type='ShaderNodeMixRGB')
        node.label = "Mix Color/Alpha"
        node.location = self._grid_location(1, 5)
        node.blend_type = 'MULTIPLY'
        node.inputs["Fac"].default_value = 1.0
        node.inputs["Color1"].default_value = COLOR_WHITE
        node.inputs["Color2"].default_value = COLOR_WHITE
        self.node_mix_color_alpha = node
        del node
        # Link
        links.new(self.node_mix_color_alpha.outputs["Color"],
                  self.node_mix_shader_alpha.inputs["Fac"])

        # --------------------------------------------------------------------
        # Diffuse

        # Diffuse BSDF
        node = nodes.new(type='ShaderNodeBsdfDiffuse')
        node.label = "Diff BSDF"
        node.location = self._grid_location(2, 3)
        self.node_bsdf_diff = node
        del node
        # Link
        links.new(self.node_bsdf_diff.outputs["BSDF"],
                  self.node_mix_shader_alpha.inputs[2])  # first 'Shader'

        # Mix Diffuse Color
        node = nodes.new(type='ShaderNodeMixRGB')
        node.label = "Mix Color/Diffuse"
        node.location = self._grid_location(1, 3)
        node.blend_type = 'MULTIPLY'
        node.inputs["Fac"].default_value = 1.0
        node.inputs["Color1"].default_value = COLOR_WHITE
        node.inputs["Color2"].default_value = COLOR_WHITE
        self.node_mix_color_diff = node
        del node
        # Link
        links.new(self.node_mix_color_diff.outputs["Color"],
                  self.node_bsdf_diff.inputs["Color"])

        # --------------------------------------------------------------------
        # Specular
        node = nodes.new(type='ShaderNodeBsdfGlossy')
        node.label = "Spec BSDF"
        node.location = self._grid_location(2, 1)
        node.mute = True  # unmute on use
        self.node_bsdf_spec = node
        del node
        # Link (with add shader)
        links.new(self.node_bsdf_spec.outputs["BSDF"],
                  self.node_mix_shader_spec.inputs[1])  # second 'Shader' slot

        node = nodes.new(type='ShaderNodeMixRGB')
        node.label = "Mix Color/Spec"
        node.location = self._grid_location(1, 1)
        node.blend_type = 'MULTIPLY'
        node.inputs["Fac"].default_value = 1.0
        node.inputs["Color1"].default_value = COLOR_WHITE
        node.inputs["Color2"].default_value = COLOR_BLACK
        self.node_mix_color_spec = node
        del node
        # Link
        links.new(self.node_mix_color_spec.outputs["Color"],
                  self.node_bsdf_spec.inputs["Color"])

        node = nodes.new(type='ShaderNodeMixRGB')
        node.label = "Mix Color/Hardness"
        node.location = self._grid_location(1, 0)
        node.blend_type = 'MULTIPLY'
        node.inputs["Fac"].default_value = 1.0
        node.inputs["Color1"].default_value = COLOR_WHITE
        node.inputs["Color2"].default_value = COLOR_WHITE
        self.node_mix_color_hard = node
        del node
        # Link
        links.new(self.node_mix_color_hard.outputs["Color"],
                  self.node_bsdf_spec.inputs["Roughness"])

        # --------------------------------------------------------------------
        # Normal Map
        node = nodes.new(type='ShaderNodeNormalMap')
        node.label = "Normal/Map"
        node.location = self._grid_location(1, 2)
        node.mute = True  # unmute on use
        self.node_normalmap = node
        del node

        # Link (with diff shader)
        socket_src = self.node_normalmap.outputs["Normal"]
        links.new(socket_src,
                  self.node_bsdf_diff.inputs["Normal"])
        # Link (with spec shader)
        links.new(socket_src,
                  self.node_bsdf_spec.inputs["Normal"])
        # Link (with refl shader)
        links.new(socket_src,
                  self.node_bsdf_refl.inputs["Normal"])
        del socket_src

        # --------------------------------------------------------------------
        # Bump Map
        # Mix Refl Color
        node = nodes.new(type='ShaderNodeMixRGB')
        node.label = "Bump/Map"
        node.location = self._grid_location(5, 3)
        node.mute = True  # unmute on use
        node.blend_type = 'MULTIPLY'
        node.inputs["Fac"].default_value = 1.0
        # reverse of most other mix nodes
        node.inputs["Color1"].default_value = COLOR_WHITE  # color
        node.inputs["Color2"].default_value = COLOR_BLACK  # factor
        self.node_mix_color_bump = node
        del node
        # Link
        links.new(self.node_mix_color_bump.outputs["Color"],
                  self.node_out.inputs["Displacement"])

        # --------------------------------------------------------------------
        # Tex Coords
        node = nodes.new(type='ShaderNodeTexCoord')
        node.label = "Texture Coords"
        node.location = self._grid_location(-3, 3)
        self.node_texcoords = node
        del node
        # no links, only use when needed!

    @staticmethod
    def _image_create_helper(image, node_dst, sockets_dst, use_alpha=False, projection='FLAT'):
        tree = node_dst.id_data
        nodes = tree.nodes
        links = tree.links

        node = nodes.new(type='ShaderNodeTexImage')
        node.image = image
        node.projection = projection
        node.location = node_dst.location
        node.location.x -= CyclesShaderWrapper._col_size
        for socket in sockets_dst:
            links.new(node.outputs["Alpha" if use_alpha else "Color"],
                      socket)
        return node

    @staticmethod
    def _mapping_create_helper(node_dst, socket_src,
                               translation, rotation, scale, clamp):
        tree = node_dst.id_data
        nodes = tree.nodes
        links = tree.links

        # in most cases:
        # (socket_src == self.node_texcoords.outputs['UV'])

        node_map = None

        # find an existing mapping node (allows multiple calls)
        if node_dst.inputs["Vector"].links:
            node_map = node_dst.inputs["Vector"].links[0].from_node

        if node_map is None:
            node_map = nodes.new(type='ShaderNodeMapping')
            node_map.vector_type = 'TEXTURE'
            node_map.location = node_dst.location
            node_map.location.x -= CyclesShaderWrapper._col_size

            node_map.width = 160.0

            # link mapping -> image node
            links.new(node_map.outputs["Vector"],
                      node_dst.inputs["Vector"])

            # link coord -> mapping
            links.new(socket_src,
                      node_map.inputs["Vector"])

        if translation is not None:
            node_map.translation = translation
        if scale is not None:
            node_map.scale = scale
        if rotation is not None:
            node_map.rotation = rotation
        if clamp is not None:
            # awkward conversion UV clamping to minmax
            node_map.min = (0.0, 0.0, 0.0)
            node_map.max = (1.0, 1.0, 1.0)

            if clamp in {(False, False), (True, True)}:
                node_map.use_min = node_map.use_max = clamp[0]
            else:
                node_map.use_min = node_map.use_max = True
                # use bool as index
                node_map.min[not clamp[0]] = -1000000000.0
                node_map.max[not clamp[0]] = 1000000000.0

        return node_map

    # note, all ***_mapping_set() functions currenly work the same way
    # (only with different image arg), could generalize.

    @staticmethod
    def _grid_location(x, y):
        return (x * CyclesShaderWrapper._col_size,
                y * CyclesShaderWrapper._row_size)

    def diffuse_color_set(self, color):
        self.node_mix_color_diff.inputs["Color1"].default_value[0:3] = color

    def diffuse_image_set(self, image, projection='FLAT'):
        node = self.node_mix_color_diff
        self.node_image_diff = (
            self._image_create_helper(image, node, (node.inputs["Color2"],), projection=projection))

    def diffuse_mapping_set(self, coords='UV',
                            translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_diff, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def specular_color_set(self, color):
        self.node_bsdf_spec.mute = max(color) <= 0.0
        self.node_mix_color_spec.inputs["Color1"].default_value[0:3] = color

    def specular_image_set(self, image):
        node = self.node_mix_color_spec
        self.node_image_spec = (
            self._image_create_helper(image, node, (node.inputs["Color2"],)))

    def specular_mapping_set(self, coords='UV',
                             translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_spec, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def hardness_value_set(self, value):
        node = self.node_mix_color_hard
        node.inputs["Color1"].default_value = (value,) * 4

    def hardness_image_set(self, image):
        node = self.node_mix_color_hard
        self.node_image_hard = (
            self._image_create_helper(image, node, (node.inputs["Color2"],)))

    def hardness_mapping_set(self, coords='UV',
                             translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_hard, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def reflect_color_set(self, color):
        node = self.node_mix_color_refl
        node.inputs["Color1"].default_value[0:3] = color

    def reflect_factor_set(self, value):
        # XXX, conflicts with image
        self.node_bsdf_refl.mute = value <= 0.0
        node = self.node_mix_color_refl
        node.inputs["Color2"].default_value = (value,) * 4

    def reflect_image_set(self, image):
        self.node_bsdf_refl.mute = False
        node = self.node_mix_color_refl
        self.node_image_refl = (
            self._image_create_helper(image, node, (node.inputs["Color2"],)))

    def reflect_mapping_set(self, coords='UV',
                            translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_refl, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def alpha_value_set(self, value):
        self.node_bsdf_alpha.mute &= (value >= 1.0)
        node = self.node_mix_color_alpha
        node.inputs["Color1"].default_value = (value,) * 4

    def alpha_image_set(self, image):
        self.node_bsdf_alpha.mute = False
        node = self.node_mix_color_alpha
        # note: use_alpha may need to be configurable
        # its not always the case that alpha channels use the image alpha
        # a grayscale image may also be used.
        self.node_image_alpha = (
            self._image_create_helper(image, node, (node.inputs["Color2"],), use_alpha=True))

    def alpha_mapping_set(self, coords='UV',
                          translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_alpha, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def alpha_image_set_from_diffuse(self):
        # XXX, remove?
        tree = self.node_mix_color_diff.id_data
        links = tree.links

        self.node_bsdf_alpha.mute = False
        node_image = self.node_image_diff
        node = self.node_mix_color_alpha
        if 1:
            links.new(node_image.outputs["Alpha"],
                      node.inputs["Color2"])
        else:
            self.alpha_image_set(node_image.image)
            self.node_image_alpha.label = "Image Texture_ALPHA"

    def normal_factor_set(self, value):
        node = self.node_normalmap
        node.inputs["Strength"].default_value = value

    def normal_image_set(self, image):
        self.node_normalmap.mute = False
        node = self.node_normalmap
        self.node_image_normalmap = (
            self._image_create_helper(image, node, (node.inputs["Color"],)))
        self.node_image_normalmap.color_space = 'NONE'

    def normal_mapping_set(self, coords='UV',
                           translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_normalmap, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def bump_factor_set(self, value):
        node = self.node_mix_color_bump
        node.mute = (value <= 0.0)
        node.inputs["Color1"].default_value = (value,) * 4

    def bump_image_set(self, image):
        node = self.node_mix_color_bump
        self.node_image_bump = (
            self._image_create_helper(image, node, (node.inputs["Color2"],)))

    def bump_mapping_set(self, coords='UV',
                         translation=None, rotation=None, scale=None, clamp=None):
        return self._mapping_create_helper(
            self.node_image_bump, self.node_texcoords.outputs[coords], translation, rotation, scale, clamp)

    def mapping_set_from_diffuse(self,
                                 specular=True,
                                 hardness=True,
                                 reflect=True,
                                 alpha=True,
                                 normal=True,
                                 bump=True):
        """
        Set all mapping based on diffuse
        (sometimes we want to assume default mapping follows diffuse).
        """
        # get mapping from diffuse
        if not hasattr(self, "node_image_diff"):
            return

        links = self.node_image_diff.inputs["Vector"].links
        if not links:
            return

        mapping_out_socket = links[0].from_socket

        tree = self.material.node_tree
        links = tree.links

        def node_image_mapping_apply(node_image_attr):
            # ensure strings are valid attrs
            assert(node_image_attr in self.__slots__)

            node_image = getattr(self, node_image_attr, None)

            if node_image is not None:
                node_image_input_socket = node_image.inputs["Vector"]
                # don't overwrite existing sockets
                if not node_image_input_socket.links:
                    links.new(mapping_out_socket,
                              node_image_input_socket)

        if specular:
            node_image_mapping_apply("node_image_spec")
        if hardness:
            node_image_mapping_apply("node_image_hard")
        if reflect:
            node_image_mapping_apply("node_image_refl")
        if alpha:
            node_image_mapping_apply("node_image_alpha")
        if normal:
            node_image_mapping_apply("node_image_normalmap")
        if bump:
            node_image_mapping_apply("node_image_bump")
