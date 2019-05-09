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

from mathutils import Color, Vector

__all__ = (
    "PrincipledBSDFWrapper",
)


def _set_check(func):
    from functools import wraps

    @wraps(func)
    def wrapper(self, *args, **kwargs):
        if self.is_readonly:
            assert(not "Trying to set value to read-only shader!")
            return
        return func(self, *args, **kwargs)
    return wrapper

def rgb_to_rgba(rgb):
    return list(rgb) + [1.0]

def rgba_to_rgb(rgba):
    return Color((rgba[0], rgba[1], rgba[2]))


class ShaderWrapper():
    """
    Base class with minimal common ground for all types of shader interfaces we may want/need to implement.
    """

    # The two mandatory nodes any children class should support.
    NODES_LIST = (
        "node_out",

        "_node_texcoords",
    )

    __slots__ = (
        "is_readonly",
        "material",
        "_textures",
        "_grid_locations",
        *NODES_LIST,
    )

    _col_size = 300
    _row_size = 300

    def _grid_to_location(self, x, y, dst_node=None, ref_node=None):
        if ref_node is not None:  # x and y are relative to this node location.
            nx = round(ref_node.location.x / self._col_size)
            ny = round(ref_node.location.y / self._row_size)
            x += nx
            y += ny
        loc = None
        while True:
            loc = (x * self._col_size, y * self._row_size)
            if loc not in self._grid_locations:
                break
            loc = (x * self._col_size, (y - 1) * self._row_size)
            if loc not in self._grid_locations:
                break
            loc = (x * self._col_size, (y - 2) * self._row_size)
            if loc not in self._grid_locations:
                break
            x -= 1
        self._grid_locations.add(loc)
        if dst_node is not None:
            dst_node.location = loc
            dst_node.width = min(dst_node.width, self._col_size - 20)
        return loc

    def __init__(self, material, is_readonly=True, use_nodes=True):
        self.is_readonly = is_readonly
        self.material = material
        if not is_readonly:
            self.use_nodes = use_nodes
        self.update()

    def update(self):  # Should be re-implemented by children classes...
        for node in self.NODES_LIST:
            setattr(self, node, None)
        self._textures = {}
        self._grid_locations = set()


    def use_nodes_get(self):
        return self.material.use_nodes

    @_set_check
    def use_nodes_set(self, val):
        self.material.use_nodes = val
        self.update()

    use_nodes = property(use_nodes_get, use_nodes_set)


    def node_texcoords_get(self):
        if not self.use_nodes:
            return None
        if self._node_texcoords is ...:
            # Running only once, trying to find a valid texcoords node.
            for n in self.material.node_tree.nodes:
                if n.bl_idname == 'ShaderNodeTexCoord':
                    self._node_texcoords = n
                    self._grid_to_location(0, 0, ref_node=n)
                    break
            if self._node_texcoords is ...:
                self._node_texcoords = None
        if self._node_texcoords is None and not self.is_readonly:
            tree = self.material.node_tree
            nodes = tree.nodes
            # links = tree.links

            node_texcoords = nodes.new(type='ShaderNodeTexCoord')
            node_texcoords.label = "Texture Coords"
            self._grid_to_location(-5, 1, dst_node=node_texcoords)
            self._node_texcoords = node_texcoords
        return self._node_texcoords

    node_texcoords = property(node_texcoords_get)


class PrincipledBSDFWrapper(ShaderWrapper):
    """
    Hard coded shader setup, based in Principled BSDF.
    Should cover most common cases on import, and gives a basic nodal shaders support for export.
    Supports basic: diffuse/spec/reflect/transparency/normal, with texturing.
    """
    NODES_LIST = (
        "node_out",
        "node_principled_bsdf",

        "_node_normalmap",
        "_node_texcoords",
    )

    __slots__ = (
        "is_readonly",
        "material",
        *NODES_LIST,
    )

    NODES_LIST = ShaderWrapper.NODES_LIST + NODES_LIST

    def __init__(self, material, is_readonly=True, use_nodes=True):
        super(PrincipledBSDFWrapper, self).__init__(material, is_readonly, use_nodes)


    def update(self):
        super(PrincipledBSDFWrapper, self).update()

        if not self.use_nodes:
            return

        tree = self.material.node_tree

        nodes = tree.nodes
        links = tree.links

        # --------------------------------------------------------------------
        # Main output and shader.
        node_out = None
        node_principled = None
        for n in nodes:
            if n.bl_idname == 'ShaderNodeOutputMaterial' and n.inputs[0].is_linked:
                node_out = n
                node_principled = n.inputs[0].links[0].from_node
            elif n.bl_idname == 'ShaderNodeBsdfPrincipled' and n.outputs[0].is_linked:
                node_principled = n
                for lnk in n.outputs[0].links:
                    node_out = lnk.to_node
                    if node_out.bl_idname == 'ShaderNodeOutputMaterial':
                        break
            if (
                    node_out is not None and node_principled is not None and
                    node_out.bl_idname == 'ShaderNodeOutputMaterial' and
                    node_principled.bl_idname == 'ShaderNodeBsdfPrincipled'
            ):
                break
            node_out = node_principled = None  # Could not find a valid pair, let's try again

        if node_out is not None:
            self._grid_to_location(0, 0, ref_node=node_out)
        elif not self.is_readonly:
            node_out = nodes.new(type='ShaderNodeOutputMaterial')
            node_out.label = "Material Out"
            node_out.target = 'ALL'
            self._grid_to_location(1, 1, dst_node=node_out)
        self.node_out = node_out

        if node_principled is not None:
            self._grid_to_location(0, 0, ref_node=node_principled)
        elif not self.is_readonly:
            node_principled = nodes.new(type='ShaderNodeBsdfPrincipled')
            node_principled.label = "Principled BSDF"
            self._grid_to_location(0, 1, dst_node=node_principled)
            # Link
            links.new(node_principled.outputs["BSDF"], self.node_out.inputs["Surface"])
        self.node_principled_bsdf = node_principled

        # --------------------------------------------------------------------
        # Normal Map, lazy initialization...
        self._node_normalmap = ...

        # --------------------------------------------------------------------
        # Tex Coords, lazy initialization...
        self._node_texcoords = ...


    def node_normalmap_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return None
        node_principled = self.node_principled_bsdf
        if self._node_normalmap is ...:
            # Running only once, trying to find a valid normalmap node.
            if node_principled.inputs["Normal"].is_linked:
                node_normalmap = node_principled.inputs["Normal"].links[0].from_node
                if node_normalmap.bl_idname == 'ShaderNodeNormalMap':
                    self._node_normalmap = node_normalmap
                    self._grid_to_location(0, 0, ref_node=node_normalmap)
            if self._node_normalmap is ...:
                self._node_normalmap = None
        if self._node_normalmap is None and not self.is_readonly:
            tree = self.material.node_tree
            nodes = tree.nodes
            links = tree.links

            node_normalmap = nodes.new(type='ShaderNodeNormalMap')
            node_normalmap.label = "Normal/Map"
            self._grid_to_location(-1, -2, dst_node=node_normalmap, ref_node=node_principled)
            # Link
            links.new(node_normalmap.outputs["Normal"], node_principled.inputs["Normal"])
            self._node_normalmap = node_normalmap
        return self._node_normalmap

    node_normalmap = property(node_normalmap_get)


    # --------------------------------------------------------------------
    # Base Color.

    def base_color_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return self.material.diffuse_color
        return rgba_to_rgb(self.node_principled_bsdf.inputs["Base Color"].default_value)

    @_set_check
    def base_color_set(self, color):
        color = rgb_to_rgba(color)
        self.material.diffuse_color = color
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["Base Color"].default_value = color

    base_color = property(base_color_get, base_color_set)


    def base_color_texture_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return None
        return ShaderImageTextureWrapper(
            self, self.node_principled_bsdf,
            self.node_principled_bsdf.inputs["Base Color"],
            grid_row_diff=1,
        )

    base_color_texture = property(base_color_texture_get)


    # --------------------------------------------------------------------
    # Specular.

    def specular_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return self.material.specular_intensity
        return self.node_principled_bsdf.inputs["Specular"].default_value

    @_set_check
    def specular_set(self, value):
        self.material.specular_intensity = value
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["Specular"].default_value = value

    specular = property(specular_get, specular_set)


    def specular_tint_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return 0.0
        return self.node_principled_bsdf.inputs["Specular Tint"].default_value

    @_set_check
    def specular_tint_set(self, value):
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["Specular Tint"].default_value = value

    specular_tint = property(specular_tint_get, specular_tint_set)


    # Will only be used as gray-scale one...
    def specular_texture_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            print("NO NODES!")
            return None
        return ShaderImageTextureWrapper(
            self, self.node_principled_bsdf,
            self.node_principled_bsdf.inputs["Specular"],
            grid_row_diff=0,
        )

    specular_texture = property(specular_texture_get)


    # --------------------------------------------------------------------
    # Roughness (also sort of inverse of specular hardness...).

    def roughness_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return self.material.roughness
        return self.node_principled_bsdf.inputs["Roughness"].default_value

    @_set_check
    def roughness_set(self, value):
        self.material.roughness = value
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["Roughness"].default_value = value

    roughness = property(roughness_get, roughness_set)


    # Will only be used as gray-scale one...
    def roughness_texture_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return None
        return ShaderImageTextureWrapper(
            self, self.node_principled_bsdf,
            self.node_principled_bsdf.inputs["Roughness"],
            grid_row_diff=0,
        )

    roughness_texture = property(roughness_texture_get)


    # --------------------------------------------------------------------
    # Metallic (a.k.a reflection, mirror).

    def metallic_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return self.material.metallic
        return self.node_principled_bsdf.inputs["Metallic"].default_value

    @_set_check
    def metallic_set(self, value):
        self.material.metallic = value
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["Metallic"].default_value = value

    metallic = property(metallic_get, metallic_set)


    # Will only be used as gray-scale one...
    def metallic_texture_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return None
        return ShaderImageTextureWrapper(
            self, self.node_principled_bsdf,
            self.node_principled_bsdf.inputs["Metallic"],
            grid_row_diff=0,
        )

    metallic_texture = property(metallic_texture_get)


    # --------------------------------------------------------------------
    # Transparency settings.

    def ior_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return 1.0
        return self.node_principled_bsdf.inputs["IOR"].default_value

    @_set_check
    def ior_set(self, value):
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["IOR"].default_value = value

    ior = property(ior_get, ior_set)


    # Will only be used as gray-scale one...
    def ior_texture_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return None
        return ShaderImageTextureWrapper(
            self, self.node_principled_bsdf,
            self.node_principled_bsdf.inputs["IOR"],
            grid_row_diff=-1,
        )

    ior_texture = property(ior_texture_get)


    def transmission_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return 0.0
        return self.node_principled_bsdf.inputs["Transmission"].default_value

    @_set_check
    def transmission_set(self, value):
        if self.use_nodes and self.node_principled_bsdf is not None:
            self.node_principled_bsdf.inputs["Transmission"].default_value = value

    transmission = property(transmission_get, transmission_set)


    # Will only be used as gray-scale one...
    def transmission_texture_get(self):
        if not self.use_nodes or self.node_principled_bsdf is None:
            return None
        return ShaderImageTextureWrapper(
            self, self.node_principled_bsdf,
            self.node_principled_bsdf.inputs["Transmission"],
            grid_row_diff=-1,
        )

    transmission_texture = property(transmission_texture_get)


    # TODO: Do we need more complex handling for alpha (allowing masking and such)?
    #       Would need extra mixing nodes onto Base Color maybe, or even its own shading chain...

    # --------------------------------------------------------------------
    # Normal map.

    def normalmap_strength_get(self):
        if not self.use_nodes or self.node_normalmap is None:
            return 0.0
        return self.node_normalmap.inputs["Strength"].default_value

    @_set_check
    def normalmap_strength_set(self, value):
        if self.use_nodes and self.node_normalmap is not None:
            self.node_normalmap.inputs["Strength"].default_value = value

    normalmap_strength = property(normalmap_strength_get, normalmap_strength_set)


    def normalmap_texture_get(self):
        if not self.use_nodes or self.node_normalmap is None:
            return None
        return ShaderImageTextureWrapper(
            self, self.node_normalmap,
            self.node_normalmap.inputs["Color"],
            grid_row_diff=-2,
        )

    normalmap_texture = property(normalmap_texture_get)

class ShaderImageTextureWrapper():
    """
    Generic 'image texture'-like wrapper, handling image node, some mapping (texture coordinates transformations),
    and texture coordinates source.
    """

    # Note: this class assumes we are using nodes, otherwise it should never be used...

    NODES_LIST = (
        "node_dst",
        "socket_dst",

        "_node_image",
        "_node_mapping",
    )

    __slots__ = (
        "owner_shader",
        "is_readonly",
        "grid_row_diff",
        "use_alpha",
        *NODES_LIST,
    )

    def __new__(cls, owner_shader: ShaderWrapper, node_dst, socket_dst, *_args, **_kwargs):
        instance = owner_shader._textures.get((node_dst, socket_dst), None)
        if instance is not None:
            return instance
        instance = super(ShaderImageTextureWrapper, cls).__new__(cls)
        owner_shader._textures[(node_dst, socket_dst)] = instance
        return instance

    def __init__(self, owner_shader: ShaderWrapper, node_dst, socket_dst, grid_row_diff=0, use_alpha=False):
        self.owner_shader = owner_shader
        self.is_readonly = owner_shader.is_readonly
        self.node_dst = node_dst
        self.socket_dst = socket_dst
        self.grid_row_diff = grid_row_diff
        self.use_alpha = use_alpha

        self._node_image = ...
        self._node_mapping = ...

        # tree = node_dst.id_data
        # nodes = tree.nodes
        # links = tree.links

        if socket_dst.is_linked:
            from_node = socket_dst.links[0].from_node
            if from_node.bl_idname == 'ShaderNodeTexImage':
                self._node_image = from_node

        if self.node_image is not None:
            socket_dst = self.node_image.inputs["Vector"]
            if socket_dst.is_linked:
                from_node = socket_dst.links[0].from_node
                if from_node.bl_idname == 'ShaderNodeMapping':
                    self._node_mapping = from_node


    def copy_from(self, tex):
        # Avoid generating any node in source texture.
        is_readonly_back = tex.is_readonly
        tex.is_readonly = True

        if tex.node_image is not None:
            self.image = tex.image
            self.projection = tex.projection
            self.texcoords = tex.texcoords
            self.copy_mapping_from(tex)

        tex.is_readonly = is_readonly_back


    def copy_mapping_from(self, tex):
        # Avoid generating any node in source texture.
        is_readonly_back = tex.is_readonly
        tex.is_readonly = True

        if tex.node_mapping is None:  # Used to actually remove mapping node.
            if self.has_mapping_node():
                # We assume node_image can never be None in that case...
                # Find potential existing link into image's Vector input.
                socket_dst = socket_src = None
                if self.node_mapping.inputs["Vector"].is_linked:
                    socket_dst = self.node_image.inputs["Vector"]
                    socket_src = self.node_mapping.inputs["Vector"].links[0].from_socket

                tree = self.owner_shader.material.node_tree
                tree.nodes.remove(self.node_mapping)
                self._node_mapping = None

                # If previously existing, re-link texcoords -> image
                if socket_src is not None:
                    tree.links.new(socket_src, socket_dst)
        elif self.node_mapping is not None:
            self.translation = tex.translation
            self.rotation = tex.rotation
            self.scale = tex.scale
            self.use_min = tex.use_min
            self.use_max = tex.use_max
            self.min = tex.min
            self.max = tex.max

        tex.is_readonly = is_readonly_back


    # --------------------------------------------------------------------
    # Image.

    def node_image_get(self):
        if self._node_image is ...:
            # Running only once, trying to find a valid image node.
            if self.socket_dst.is_linked:
                node_image = self.socket_dst.links[0].from_node
                if node_image.bl_idname == 'ShaderNodeTexImage':
                    self._node_image = node_image
                    self.owner_shader._grid_to_location(0, 0, ref_node=node_image)
            if self._node_image is ...:
                self._node_image = None
        if self._node_image is None and not self.is_readonly:
            tree = self.owner_shader.material.node_tree

            node_image = tree.nodes.new(type='ShaderNodeTexImage')
            self.owner_shader._grid_to_location(-1, 0 + self.grid_row_diff, dst_node=node_image, ref_node=self.node_dst)

            tree.links.new(node_image.outputs["Alpha" if self.use_alpha else "Color"], self.socket_dst)

            self._node_image = node_image
        return self._node_image

    node_image = property(node_image_get)


    def image_get(self):
        return self.node_image.image if self.node_image is not None else None

    @_set_check
    def image_set(self, image):
        self.node_image.image = image

    image = property(image_get, image_set)


    def projection_get(self):
        return self.node_image.projection if self.node_image is not None else 'FLAT'

    @_set_check
    def projection_set(self, projection):
        self.node_image.projection = projection

    projection = property(projection_get, projection_set)


    def texcoords_get(self):
        if self.node_image is not None:
            socket = (self.node_mapping if self.has_mapping_node() else self.node_image).inputs["Vector"]
            if socket.is_linked:
                return socket.links[0].from_socket.name
        return 'UV'

    @_set_check
    def texcoords_set(self, texcoords):
        # Image texture node already defaults to UVs, no extra node needed.
        # ONLY in case we do not have any texcoords mapping!!!
        if texcoords == 'UV' and not self.has_mapping_node():
            return
        tree = self.node_image.id_data
        links = tree.links
        node_dst = self.node_mapping if self.has_mapping_node() else self.node_image
        socket_src = self.owner_shader.node_texcoords.outputs[texcoords]
        links.new(socket_src, node_dst.inputs["Vector"])

    texcoords = property(texcoords_get, texcoords_set)


    def extension_get(self):
        return self.node_image.extension if self.node_image is not None else 'REPEAT'

    @_set_check
    def extension_set(self, extension):
        self.node_image.extension = extension

    extension = property(extension_get, extension_set)


    # --------------------------------------------------------------------
    # Mapping.

    def has_mapping_node(self):
        return self._node_mapping not in {None, ...}

    def node_mapping_get(self):
        if self._node_mapping is ...:
            # Running only once, trying to find a valid mapping node.
            if self.node_image is None:
                return None
            if self.node_image.inputs["Vector"].is_linked:
                node_mapping = self.node_image.inputs["Vector"].links[0].from_node
                if node_mapping.bl_idname == 'ShaderNodeMapping':
                    self._node_mapping = node_mapping
                    self.owner_shader._grid_to_location(0, 0 + self.grid_row_diff, ref_node=node_mapping)
            if self._node_mapping is ...:
                self._node_mapping = None
        if self._node_mapping is None and not self.is_readonly:
            # Find potential existing link into image's Vector input.
            socket_dst = self.node_image.inputs["Vector"]
            # If not already existing, we need to create texcoords -> mapping link (from UV).
            socket_src = (socket_dst.links[0].from_socket if socket_dst.is_linked
                                                          else self.owner_shader.node_texcoords.outputs['UV'])

            tree = self.owner_shader.material.node_tree
            node_mapping = tree.nodes.new(type='ShaderNodeMapping')
            node_mapping.vector_type = 'TEXTURE'
            self.owner_shader._grid_to_location(-1, 0, dst_node=node_mapping, ref_node=self.node_image)

            # Link mapping -> image node.
            tree.links.new(node_mapping.outputs["Vector"], socket_dst)
            # Link texcoords -> mapping.
            tree.links.new(socket_src, node_mapping.inputs["Vector"])

            self._node_mapping = node_mapping
        return self._node_mapping

    node_mapping = property(node_mapping_get)


    def translation_get(self):
        return self.node_mapping.translation if self.node_mapping is not None else Vector((0.0, 0.0, 0.0))

    @_set_check
    def translation_set(self, translation):
        self.node_mapping.translation = translation

    translation = property(translation_get, translation_set)


    def rotation_get(self):
        return self.node_mapping.rotation if self.node_mapping is not None else Vector((0.0, 0.0, 0.0))

    @_set_check
    def rotation_set(self, rotation):
        self.node_mapping.rotation = rotation

    rotation = property(rotation_get, rotation_set)


    def scale_get(self):
        return self.node_mapping.scale if self.node_mapping is not None else Vector((1.0, 1.0, 1.0))

    @_set_check
    def scale_set(self, scale):
        self.node_mapping.scale = scale

    scale = property(scale_get, scale_set)


    def use_min_get(self):
        return self.node_mapping.use_min if self_mapping.node is not None else False

    @_set_check
    def use_min_set(self, use_min):
        self.node_mapping.use_min = use_min

    use_min = property(use_min_get, use_min_set)


    def use_max_get(self):
        return self.node_mapping.use_max if self_mapping.node is not None else False

    @_set_check
    def use_max_set(self, use_max):
        self.node_mapping.use_max = use_max

    use_max = property(use_max_get, use_max_set)


    def min_get(self):
        return self.node_mapping.min if self.node_mapping is not None else Vector((0.0, 0.0, 0.0))

    @_set_check
    def min_set(self, min):
        self.node_mapping.min = min

    min = property(min_get, min_set)


    def max_get(self):
        return self.node_mapping.max if self.node_mapping is not None else Vector((0.0, 0.0, 0.0))

    @_set_check
    def max_set(self, max):
        self.node_mapping.max = max

    max = property(max_get, max_set)
