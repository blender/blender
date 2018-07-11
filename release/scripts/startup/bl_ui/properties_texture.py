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
from bpy.types import Menu, Panel, UIList

from bpy.types import (
    Brush,
    FreestyleLineStyle,
    Object,
    ParticleSettings,
    Texture,
)

from rna_prop_ui import PropertyPanel

from .properties_paint_common import brush_texture_settings


class TEXTURE_MT_specials(Menu):
    bl_label = "Texture Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        layout.operator("texture.slot_copy", icon='COPYDOWN')
        layout.operator("texture.slot_paste", icon='PASTEDOWN')


class TEXTURE_UL_texslots(UIList):

    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        ma = data
        slot = item
        tex = slot.texture if slot else None
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if tex:
                layout.prop(tex, "name", text="", emboss=False, icon_value=icon)
            else:
                layout.label(text="", icon_value=icon)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


def context_tex_datablock(context):
    idblock = context.brush
    if idblock:
        return idblock

    idblock = context.line_style
    if idblock:
        return idblock

    if context.particle_system:
        idblock = context.particle_system.settings

    return idblock


class TextureButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "texture"


class TEXTURE_PT_preview(TextureButtonsPanel, Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        tex = context.texture
        return tex and (tex.type != 'NONE' or tex.use_nodes) and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        slot = getattr(context, "texture_slot", None)
        idblock = context_tex_datablock(context)

        if idblock:
            layout.template_preview(tex, parent=idblock, slot=slot)
        else:
            layout.template_preview(tex, slot=slot)

        # Show Alpha Button for Brush Textures, see #29502
        idblock = context_tex_datablock(context)
        if isinstance(idblock, Brush):
            layout.prop(tex, "use_preview_alpha")


class TEXTURE_PT_context(TextureButtonsPanel, Panel):
    bl_label = ""
    bl_context = "texture"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        space = context.space_data
        pin_id = space.pin_id
        use_pin_id = space.use_pin_id
        user = context.texture_user

        col = layout.column()

        if not (use_pin_id and isinstance(pin_id, bpy.types.Texture)):
            pin_id = None

        if not pin_id:
            col.template_texture_user()

        col.separator()

        if user or pin_id:
            if pin_id:
                col.template_ID(space, "pin_id")
            else:
                propname = context.texture_user_property.identifier
                col.template_ID(user, propname, new="texture.new")

            col.separator()

            if tex:
                split = col.split(percentage=0.2)
                split.label(text="Type")
                split.prop(tex, "type", text="")


class TEXTURE_PT_node(TextureButtonsPanel, Panel):
    bl_label = "Node"
    bl_context = "texture"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        node = context.texture_node
        return node and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        node = context.texture_node
        ntree = node.id_data
        layout.template_node_view(ntree, node, None)


class TEXTURE_PT_node_mapping(TextureButtonsPanel, Panel):
    bl_label = "Mapping"
    bl_context = "texture"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        node = context.texture_node
        # TODO(sergey): perform a faster/nicer check?
        return node and hasattr(node, 'texture_mapping') and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        node = context.texture_node

        mapping = node.texture_mapping

        layout.prop(mapping, "vector_type", expand=True)

        row = layout.row()

        row.column().prop(mapping, "translation")
        row.column().prop(mapping, "rotation")
        row.column().prop(mapping, "scale")

        layout.label(text="Projection:")

        row = layout.row()
        row.prop(mapping, "mapping_x", text="")
        row.prop(mapping, "mapping_y", text="")
        row.prop(mapping, "mapping_z", text="")


class TextureTypePanel(TextureButtonsPanel):

    @classmethod
    def poll(cls, context):
        tex = context.texture
        engine = context.engine
        return tex and ((tex.type == cls.tex_type and not tex.use_nodes) and (engine in cls.COMPAT_ENGINES))


class TEXTURE_PT_clouds(TextureTypePanel, Panel):
    bl_label = "Clouds"
    tex_type = 'CLOUDS'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.row().prop(tex, "cloud_type", expand=True)
        layout.label(text="Noise:")
        layout.row().prop(tex, "noise_type", text="Type", expand=True)
        layout.prop(tex, "noise_basis", text="Basis")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "noise_depth", text="Depth")

        split.prop(tex, "nabla", text="Nabla")


class TEXTURE_PT_wood(TextureTypePanel, Panel):
    bl_label = "Wood"
    tex_type = 'WOOD'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.row().prop(tex, "noise_basis_2", expand=True)
        layout.row().prop(tex, "wood_type", expand=True)

        col = layout.column()
        col.active = tex.wood_type in {'RINGNOISE', 'BANDNOISE'}
        col.label(text="Noise:")
        col.row().prop(tex, "noise_type", text="Type", expand=True)
        layout.prop(tex, "noise_basis", text="Basis")

        split = layout.split()
        split.active = tex.wood_type in {'RINGNOISE', 'BANDNOISE'}

        col = split.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "turbulence")

        split.prop(tex, "nabla")


class TEXTURE_PT_marble(TextureTypePanel, Panel):
    bl_label = "Marble"
    tex_type = 'MARBLE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.row().prop(tex, "marble_type", expand=True)
        layout.row().prop(tex, "noise_basis_2", expand=True)
        layout.label(text="Noise:")
        layout.row().prop(tex, "noise_type", text="Type", expand=True)
        layout.prop(tex, "noise_basis", text="Basis")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "noise_depth", text="Depth")

        col = split.column()
        col.prop(tex, "turbulence")
        col.prop(tex, "nabla")


class TEXTURE_PT_magic(TextureTypePanel, Panel):
    bl_label = "Magic"
    tex_type = 'MAGIC'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        row = layout.row()
        row.prop(tex, "noise_depth", text="Depth")
        row.prop(tex, "turbulence")


class TEXTURE_PT_blend(TextureTypePanel, Panel):
    bl_label = "Blend"
    tex_type = 'BLEND'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "progression")

        sub = layout.row()

        sub.active = (tex.progression in {'LINEAR', 'QUADRATIC', 'EASING', 'RADIAL'})
        sub.prop(tex, "use_flip_axis", expand=True)


class TEXTURE_PT_stucci(TextureTypePanel, Panel):
    bl_label = "Stucci"
    tex_type = 'STUCCI'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.row().prop(tex, "stucci_type", expand=True)
        layout.label(text="Noise:")
        layout.row().prop(tex, "noise_type", text="Type", expand=True)
        layout.prop(tex, "noise_basis", text="Basis")

        row = layout.row()
        row.prop(tex, "noise_scale", text="Size")
        row.prop(tex, "turbulence")


class TEXTURE_PT_image(TextureTypePanel, Panel):
    bl_label = "Image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        layout.template_image(tex, "image", tex.image_user)


def texture_filter_common(tex, layout):
    layout.prop(tex, "filter_type", text="Filter Type")
    if tex.use_mipmap and tex.filter_type in {'AREA', 'EWA', 'FELINE'}:
        if tex.filter_type == 'FELINE':
            layout.prop(tex, "filter_lightprobes", text="Light Probes")
        else:
            layout.prop(tex, "filter_eccentricity", text="Eccentricity")

    layout.prop(tex, "filter_size", text="Size")
    layout.prop(tex, "use_filter_size_min", text="Minimum Size")


class TEXTURE_PT_image_sampling(TextureTypePanel, Panel):
    bl_label = "Sampling"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = 'TEXTURE_PT_image'
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        idblock = context_tex_datablock(context)
        tex = context.texture
        slot = getattr(context, "texture_slot", None)

        col = flow.column()
        col.prop(tex, "use_flip_axis", text="Flip X/Y Axis")
        col.prop(tex, "use_interpolation")

        col.separator()

        col = flow.column()
        col.prop(tex, "use_mipmap")
        sub = col.column()
        sub.active = tex.use_mipmap
        sub.prop(tex, "use_mipmap_gauss", text="Gaussian Filter")

        col.separator()

        col = flow.column()
        texture_filter_common(tex, col)


class TEXTURE_PT_image_alpha(TextureTypePanel, Panel):
    bl_label = "Alpha"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = 'TEXTURE_PT_image'
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    def draw_header(self, context):
        tex = context.texture
        self.layout.prop(tex, "use_alpha", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        tex = context.texture

        col = layout.column()
        col.active = bool(tex.image and tex.image.use_alpha)
        col.prop(tex, "use_calculate_alpha", text="Calculate")
        col.prop(tex, "invert_alpha", text="Invert")


class TEXTURE_PT_image_mapping(TextureTypePanel, Panel):
    bl_label = "Mapping"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = 'TEXTURE_PT_image'
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "extension")

        if tex.extension == 'REPEAT':
            sub = col.column(align=True)
            sub.prop(tex, "repeat_x", text="Repeat X")
            sub.prop(tex, "repeat_y", text="Y")

            sub = col.column()
            sub.prop(tex, "use_mirror_x", text="Mirror X")
            sub.active = (tex.repeat_x > 1)

            sub = col.column()
            sub.prop(tex, "use_mirror_y", text="Y")
            sub.active = (tex.repeat_y > 1)

        elif tex.extension == 'CHECKER':
            col = layout.column(align=True)
            col.prop(tex, "use_checker_even", text="Even")
            col.prop(tex, "use_checker_odd", text="Odd")

            col = layout.column()
            col.prop(tex, "checker_distance", text="Distance")

        col = flow.column()
        sub = col.column(align=True)
        # col.prop(tex, "crop_rectangle")
        sub.prop(tex, "crop_min_x", text="Crop Minimum X")
        sub.prop(tex, "crop_min_y", text="Y")

        sub = col.column(align=True)
        sub.prop(tex, "crop_max_x", text="Crop Maximum X")
        sub.prop(tex, "crop_max_y", text="Y")


class TEXTURE_PT_musgrave(TextureTypePanel, Panel):
    bl_label = "Musgrave"
    tex_type = 'MUSGRAVE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "musgrave_type")

        split = layout.split()

        col = split.column()
        col.prop(tex, "dimension_max", text="Dimension")
        col.prop(tex, "lacunarity")
        col.prop(tex, "octaves")

        musgrave_type = tex.musgrave_type
        col = split.column()
        if musgrave_type in {'HETERO_TERRAIN', 'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL'}:
            col.prop(tex, "offset")
        col.prop(tex, "noise_intensity", text="Intensity")
        if musgrave_type in {'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL'}:
            col.prop(tex, "gain")

        layout.label(text="Noise:")

        layout.prop(tex, "noise_basis", text="Basis")

        row = layout.row()
        row.prop(tex, "noise_scale", text="Size")
        row.prop(tex, "nabla")


class TEXTURE_PT_voronoi(TextureTypePanel, Panel):
    bl_label = "Voronoi"
    tex_type = 'VORONOI'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        split = layout.split()

        col = split.column()
        col.label(text="Distance Metric:")
        col.prop(tex, "distance_metric", text="")
        sub = col.column()
        sub.active = tex.distance_metric == 'MINKOVSKY'
        sub.prop(tex, "minkovsky_exponent", text="Exponent")
        col.label(text="Coloring:")
        col.prop(tex, "color_mode", text="")
        col.prop(tex, "noise_intensity", text="Intensity")

        col = split.column()
        sub = col.column(align=True)
        sub.label(text="Feature Weights:")
        sub.prop(tex, "weight_1", text="1", slider=True)
        sub.prop(tex, "weight_2", text="2", slider=True)
        sub.prop(tex, "weight_3", text="3", slider=True)
        sub.prop(tex, "weight_4", text="4", slider=True)

        layout.label(text="Noise:")
        row = layout.row()
        row.prop(tex, "noise_scale", text="Size")
        row.prop(tex, "nabla")


class TEXTURE_PT_distortednoise(TextureTypePanel, Panel):
    bl_label = "Distorted Noise"
    tex_type = 'DISTORTED_NOISE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "noise_distortion")
        layout.prop(tex, "noise_basis", text="Basis")

        split = layout.split()

        col = split.column()
        col.prop(tex, "distortion", text="Distortion")
        col.prop(tex, "noise_scale", text="Size")

        split.prop(tex, "nabla")


class TextureSlotPanel(TextureButtonsPanel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        if not hasattr(context, "texture_slot"):
            return False

        return (context.engine in cls.COMPAT_ENGINES)


class TEXTURE_PT_mapping(TextureSlotPanel, Panel):
    bl_label = "Mapping"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        idblock = context_tex_datablock(context)
        if isinstance(idblock, Brush) and not context.sculpt_object:
            return False

        if not getattr(context, "texture_slot", None):
            return False

        engine = context.engine
        return (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        idblock = context_tex_datablock(context)

        tex = context.texture_slot

        if not isinstance(idblock, Brush):
            split = layout.split(percentage=0.3)
            col = split.column()
            col.label(text="Coordinates:")
            col = split.column()
            col.prop(tex, "texture_coords", text="")

            if tex.texture_coords == 'ORCO':
                """
                ob = context.object
                if ob and ob.type == 'MESH':
                    split = layout.split(percentage=0.3)
                    split.label(text="Mesh:")
                    split.prop(ob.data, "texco_mesh", text="")
                """
            elif tex.texture_coords == 'UV':
                split = layout.split(percentage=0.3)
                split.label(text="Map:")
                ob = context.object
                if ob and ob.type == 'MESH':
                    split.prop_search(tex, "uv_layer", ob.data, "uv_layers", text="")
                else:
                    split.prop(tex, "uv_layer", text="")

            elif tex.texture_coords == 'OBJECT':
                split = layout.split(percentage=0.3)
                split.label(text="Object:")
                split.prop(tex, "object", text="")

            elif tex.texture_coords == 'ALONG_STROKE':
                split = layout.split(percentage=0.3)
                split.label(text="Use Tips:")
                split.prop(tex, "use_tips", text="")

        if isinstance(idblock, Brush):
            if context.sculpt_object or context.image_paint_object:
                brush_texture_settings(layout, idblock, context.sculpt_object)
        else:
            if isinstance(idblock, FreestyleLineStyle):
                split = layout.split(percentage=0.3)
                split.label(text="Projection:")
                split.prop(tex, "mapping", text="")

                split = layout.split(percentage=0.3)
                split.separator()
                row = split.row()
                row.prop(tex, "mapping_x", text="")
                row.prop(tex, "mapping_y", text="")
                row.prop(tex, "mapping_z", text="")

            row = layout.row()
            row.column().prop(tex, "offset")
            row.column().prop(tex, "scale")


class TEXTURE_PT_influence(TextureSlotPanel, Panel):
    bl_label = "Influence"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        idblock = context_tex_datablock(context)
        if isinstance(idblock, Brush):
            return False

        if not getattr(context, "texture_slot", None):
            return False

        engine = context.engine
        return (engine in cls.COMPAT_ENGINES)

    def draw(self, context):

        layout = self.layout

        idblock = context_tex_datablock(context)

        tex = context.texture_slot

        def factor_but(layout, toggle, factor, name):
            row = layout.row(align=True)
            row.prop(tex, toggle, text="")
            sub = row.row(align=True)
            sub.active = getattr(tex, toggle)
            sub.prop(tex, factor, text=name, slider=True)
            return sub  # XXX, temp. use_map_normal needs to override.

        if isinstance(idblock, ParticleSettings):
            split = layout.split()

            col = split.column()
            col.label(text="General:")
            factor_but(col, "use_map_time", "time_factor", "Time")
            factor_but(col, "use_map_life", "life_factor", "Lifetime")
            factor_but(col, "use_map_density", "density_factor", "Density")
            factor_but(col, "use_map_size", "size_factor", "Size")

            col = split.column()
            col.label(text="Physics:")
            factor_but(col, "use_map_velocity", "velocity_factor", "Velocity")
            factor_but(col, "use_map_damp", "damp_factor", "Damp")
            factor_but(col, "use_map_gravity", "gravity_factor", "Gravity")
            factor_but(col, "use_map_field", "field_factor", "Force Fields")

            layout.label(text="Hair:")

            split = layout.split()

            col = split.column()
            factor_but(col, "use_map_length", "length_factor", "Length")
            factor_but(col, "use_map_clump", "clump_factor", "Clump")
            factor_but(col, "use_map_twist", "twist_factor", "Twist")

            col = split.column()
            factor_but(col, "use_map_kink_amp", "kink_amp_factor", "Kink Amplitude")
            factor_but(col, "use_map_kink_freq", "kink_freq_factor", "Kink Frequency")
            factor_but(col, "use_map_rough", "rough_factor", "Rough")

        elif isinstance(idblock, FreestyleLineStyle):
            split = layout.split()

            col = split.column()
            factor_but(col, "use_map_color_diffuse", "diffuse_color_factor", "Color")
            col = split.column()
            factor_but(col, "use_map_alpha", "alpha_factor", "Alpha")

        layout.separator()

        if not isinstance(idblock, ParticleSettings):
            split = layout.split()

            col = split.column()
            col.prop(tex, "blend_type", text="Blend")
            col.prop(tex, "use_rgb_to_intensity")
            # color is used on gray-scale textures even when use_rgb_to_intensity is disabled.
            col.prop(tex, "color", text="")

            col = split.column()
            col.prop(tex, "invert", text="Negative")
            col.prop(tex, "use_stencil")


class TEXTURE_PT_colors(TextureButtonsPanel, Panel):
    bl_label = "Colors"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        tex = context.texture
        return tex and (tex.type != 'NONE' or tex.use_nodes) and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        tex = context.texture

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(tex, "factor_red", text="Multiply R")
        sub.prop(tex, "factor_green", text="G")
        sub.prop(tex, "factor_blue", text="B")

        col.prop(tex, "intensity")
        col.prop(tex, "contrast")
        col.prop(tex, "saturation")

        col.prop(tex, "use_clamp", text="Clamp")
        col.prop(tex, "use_color_ramp", text="Ramp")
        if tex.use_color_ramp:
            layout.use_property_split = False
            layout.template_color_ramp(tex, "color_ramp", expand=True)


class TEXTURE_PT_custom_props(TextureButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    _context_path = "texture"
    _property_type = Texture

    @classmethod
    def poll(cls, context):
        return context.texture and (context.engine in cls.COMPAT_ENGINES)


classes = (
    TEXTURE_MT_specials,
    TEXTURE_UL_texslots,
    TEXTURE_PT_preview,
    TEXTURE_PT_context,
    TEXTURE_PT_node,
    TEXTURE_PT_node_mapping,
    TEXTURE_PT_mapping,
    TEXTURE_PT_influence,
    TEXTURE_PT_clouds,
    TEXTURE_PT_wood,
    TEXTURE_PT_marble,
    TEXTURE_PT_magic,
    TEXTURE_PT_blend,
    TEXTURE_PT_stucci,
    TEXTURE_PT_image,
    TEXTURE_PT_image_alpha,
    TEXTURE_PT_image_sampling,
    TEXTURE_PT_image_mapping,
    TEXTURE_PT_musgrave,
    TEXTURE_PT_voronoi,
    TEXTURE_PT_distortednoise,
    TEXTURE_PT_colors,
    TEXTURE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
