# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Menu,
    Panel,
    UIList,
)
from bpy.types import (
    Brush,
    FreestyleLineStyle,
    ParticleSettings,
    Texture,
)
from bpy.app.translations import contexts as i18n_contexts

from rna_prop_ui import PropertyPanel
from bl_ui.properties_paint_common import brush_texture_settings
from bl_ui.space_properties import PropertiesAnimationMixin


class TEXTURE_MT_context_menu(Menu):
    bl_label = "Texture Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, _context):
        layout = self.layout

        layout.operator("texture.slot_copy", icon='COPYDOWN')
        layout.operator("texture.slot_paste", icon='PASTEDOWN')


class TEXTURE_UL_texslots(UIList):

    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        slot = item
        tex = slot.texture if slot else None

        if tex:
            layout.prop(tex, "name", text="", emboss=False, icon_value=icon)
        else:
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
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

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

        # Show Alpha Button for Brush Textures, see #29502.
        idblock = context_tex_datablock(context)
        if isinstance(idblock, Brush):
            layout.prop(tex, "use_preview_alpha")


class TEXTURE_PT_context(TextureButtonsPanel, Panel):
    bl_label = ""
    bl_context = "texture"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

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

        if user or pin_id:
            col.separator()

            if pin_id:
                col.template_ID(space, "pin_id")
            else:
                propname = context.texture_user_property.identifier
                col.template_ID(user, propname, new="texture.new")

            if tex:
                col.separator()

                split = col.split(factor=0.2)
                split.label(text="Type")
                split.prop(tex, "type", text="")


class TEXTURE_PT_node(TextureButtonsPanel, Panel):
    bl_label = "Node"
    bl_context = "texture"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        node = context.texture_node
        return node and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        node = context.texture_node
        ntree = node.id_data
        layout.template_node_view(ntree, node, None)


class TextureTypePanel(TextureButtonsPanel):

    @classmethod
    def poll(cls, context):
        tex = context.texture
        engine = context.engine
        return tex and ((tex.type == cls.tex_type and not tex.use_nodes) and (engine in cls.COMPAT_ENGINES))


class TEXTURE_PT_clouds(TextureTypePanel, Panel):
    bl_label = "Clouds"
    tex_type = 'CLOUDS'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_basis", text="Noise Basis")

        col.separator()

        col.prop(tex, "noise_type", text="Type")

        col.separator()

        col = flow.column()
        col.prop(tex, "cloud_type")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "noise_depth", text="Depth")
        col.prop(tex, "nabla", text="Nabla")


class TEXTURE_PT_wood(TextureTypePanel, Panel):
    bl_label = "Wood"
    tex_type = 'WOOD'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=False)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_basis", text="Noise Basis")

        col.separator()

        col.prop(tex, "wood_type")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_basis_2", text="Second Basis")

        col = col.column()
        col.active = tex.wood_type in {'RINGNOISE', 'BANDNOISE'}
        col.prop(tex, "noise_type", text="Type")

        col.separator()

        sub = flow.column()
        sub.active = tex.wood_type in {'RINGNOISE', 'BANDNOISE'}
        sub.prop(tex, "noise_scale", text="Size")
        sub.prop(tex, "turbulence")
        sub.prop(tex, "nabla")


class TEXTURE_PT_marble(TextureTypePanel, Panel):
    bl_label = "Marble"
    tex_type = 'MARBLE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_basis", text="Noise Basis")

        col.separator()

        col.prop(tex, "marble_type")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_basis_2", text="Second Basis")
        col.prop(tex, "noise_type", text="Type")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "noise_depth", text="Depth")
        col.prop(tex, "turbulence")
        col.prop(tex, "nabla")


class TEXTURE_PT_magic(TextureTypePanel, Panel):
    bl_label = "Magic"
    tex_type = 'MAGIC'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_depth", text="Depth")

        col = flow.column()
        col.prop(tex, "turbulence")


class TEXTURE_PT_blend(TextureTypePanel, Panel):
    bl_label = "Blend"
    tex_type = 'BLEND'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "progression")

        col.separator()

        col = flow.column()
        col.active = (tex.progression in {'LINEAR', 'QUADRATIC', 'EASING', 'RADIAL'})
        col.prop(tex, "use_flip_axis", text="Orientation")


class TEXTURE_PT_stucci(TextureTypePanel, Panel):
    bl_label = "Stucci"
    tex_type = 'STUCCI'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_basis", text="Noise Basis")

        col.separator()

        col.row().prop(tex, "stucci_type")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_type", text="Type")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "turbulence")


class TEXTURE_PT_image(TextureTypePanel, Panel):
    bl_label = "Image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, _context):
        # TODO: maybe expose the template_ID from the template image here.
        layout = self.layout
        del layout


class TEXTURE_PT_image_settings(TextureTypePanel, Panel):
    bl_label = "Settings"
    bl_parent_id = "TEXTURE_PT_image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        layout.template_image(tex, "image", tex.image_user)


class TEXTURE_PT_image_sampling(TextureTypePanel, Panel):
    bl_label = "Sampling"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "TEXTURE_PT_image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "use_interpolation")
        col.prop(tex, "filter_size", text="Size")


class TEXTURE_PT_image_alpha(TextureTypePanel, Panel):
    bl_label = "Alpha"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "TEXTURE_PT_image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        tex = context.texture
        self.layout.prop(tex, "use_alpha", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        tex = context.texture

        col = layout.column()
        col.active = bool(tex.image and tex.image.alpha_mode != 'NONE')
        col.prop(tex, "use_calculate_alpha", text="Calculate")
        col.prop(tex, "invert_alpha", text="Invert")


class TEXTURE_PT_image_mapping(TextureTypePanel, Panel):
    bl_label = "Mapping"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "TEXTURE_PT_image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        tex = context.texture

        col = layout.column()
        col.prop(tex, "use_flip_axis", text="Flip Axes")

        col.separator()

        subcol = layout.column()
        subcol.prop(tex, "extension")  # use layout, to keep the same location in case of button cycling.

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        if tex.extension == 'REPEAT':

            col = flow.column()
            sub = col.column(align=True)
            sub.prop(tex, "repeat_x", text="Repeat X")
            sub.prop(tex, "repeat_y", text="Y")

            col = flow.column(heading="Mirror", heading_ctxt=i18n_contexts.id_image)
            sub = col.column()
            sub.active = (tex.repeat_x > 1)
            sub.prop(tex, "use_mirror_x", text="X")

            sub = col.column()
            sub.active = (tex.repeat_y > 1)
            sub.prop(tex, "use_mirror_y", text="Y")

        elif tex.extension == 'CHECKER':
            subcol.separator()

            col = flow.column()
            col.prop(tex, "checker_distance", text="Distance")

            col = flow.column(heading="Tiles")
            col.prop(tex, "use_checker_even", text="Even", text_ctxt=i18n_contexts.amount)
            col.prop(tex, "use_checker_odd", text="Odd")
        else:
            del flow


class TEXTURE_PT_image_mapping_crop(TextureTypePanel, Panel):
    bl_label = "Crop"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "TEXTURE_PT_image_mapping"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        tex = context.texture

        col = flow.column(align=True)
        # col.prop(tex, "crop_rectangle")
        col.prop(tex, "crop_min_x", text="Minimum X")
        col.prop(tex, "crop_min_y", text="Y")

        col = flow.column(align=True)
        col.prop(tex, "crop_max_x", text="Maximum X")
        col.prop(tex, "crop_max_y", text="Y")


class TEXTURE_PT_musgrave(TextureTypePanel, Panel):
    bl_label = "Musgrave"
    tex_type = 'MUSGRAVE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_basis", text="Noise Basis")

        col.separator()

        col.prop(tex, "musgrave_type")

        col.separator()

        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "nabla")

        col.separator()

        col = flow.column()
        col.prop(tex, "dimension_max", text="Dimension")
        col.prop(tex, "lacunarity")
        col.prop(tex, "octaves")

        col.separator()

        musgrave_type = tex.musgrave_type

        col = flow.column()

        if musgrave_type in {'HETERO_TERRAIN', 'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL'}:
            col.prop(tex, "offset")
        col.prop(tex, "noise_intensity", text="Intensity")

        if musgrave_type in {'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL'}:
            col.prop(tex, "gain")


class TEXTURE_PT_voronoi(TextureTypePanel, Panel):
    bl_label = "Voronoi"
    tex_type = 'VORONOI'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "distance_metric")

        sub = col.column()
        sub.active = tex.distance_metric == 'MINKOVSKY'
        sub.prop(tex, "minkovsky_exponent", text="Exponent")

        sub.separator()

        col = flow.column()
        col.prop(tex, "color_mode")
        col.prop(tex, "noise_intensity", text="Intensity")

        col.separator()

        col = flow.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "nabla")


class TEXTURE_PT_voronoi_feature_weights(TextureTypePanel, Panel):
    bl_label = "Feature Weights"
    bl_parent_id = "TEXTURE_PT_voronoi"
    tex_type = 'VORONOI'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column(align=True)
        col.prop(tex, "weight_1", text="First", slider=True)
        col.prop(tex, "weight_2", text="Second", slider=True)

        sub = flow.column(align=True)
        sub.prop(tex, "weight_3", text="Third", slider=True)
        sub.prop(tex, "weight_4", text="Fourth", slider=True)


class TEXTURE_PT_distortednoise(TextureTypePanel, Panel):
    bl_label = "Distorted Noise"
    tex_type = 'DISTORTED_NOISE'
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "noise_basis", text="Noise Basis")

        col.separator()

        col.prop(tex, "noise_distortion", text="Distortion")

        col.separator()

        col = flow.column()
        col.prop(tex, "distortion", text="Amount")
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "nabla")


class TextureSlotPanel(TextureButtonsPanel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        if not hasattr(context, "texture_slot"):
            return False

        return (context.engine in cls.COMPAT_ENGINES)


class TEXTURE_PT_mapping(TextureSlotPanel, Panel):
    bl_label = "Mapping"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

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
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        idblock = context_tex_datablock(context)

        tex = context.texture_slot

        if isinstance(idblock, Brush):
            if context.sculpt_object or context.image_paint_object:
                brush_texture_settings(layout, idblock, context.sculpt_object)
        else:
            col = flow.column()

            col.prop(tex, "texture_coords", text="Coordinates")

            # Note: the ORCO case used to call ob.data, "texco_mesh" prop.
            if tex.texture_coords == 'UV':
                ob = context.object

                if ob and ob.type == 'MESH':
                    col.prop_search(tex, "uv_layer", ob.data, "uv_layers", text="Map")
                else:
                    col.prop(tex, "uv_layer", text="Map")

            elif tex.texture_coords == 'OBJECT':
                col.prop(tex, "object", text="Object")

            col.separator()

            if isinstance(idblock, FreestyleLineStyle):
                col = flow.column()
                col.prop(tex, "mapping", text="Projection")

                col.separator()

                col = flow.column()
                col.prop(tex, "mapping_x", text="Mapping X")
                col.prop(tex, "mapping_y", text="Y")
                col.prop(tex, "mapping_z", text="Z")

                col.separator()

            col = flow.column(align=True)
            col.column().prop(tex, "offset")

            col = flow.column(align=True)
            col.column().prop(tex, "scale")


class TEXTURE_PT_influence(TextureSlotPanel, Panel):
    bl_label = "Influence"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

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
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=False)

        idblock = context_tex_datablock(context)
        tex = context.texture_slot

        def factor_but(layout, toggle, factor, name):
            row = layout.row(align=True)
            row.active = getattr(tex, toggle)

            row.prop(tex, factor, text=name, slider=True)
            sub = row.row(align=True)
            sub.prop(tex, toggle, text="")
            return sub  # XXX, temp. use_map_normal needs to override.

        if isinstance(idblock, ParticleSettings):
            col = flow.column()
            factor_but(col, "use_map_time", "time_factor", "General Time")
            factor_but(col, "use_map_life", "life_factor", "Lifetime")
            factor_but(col, "use_map_density", "density_factor", "Density")
            factor_but(col, "use_map_size", "size_factor", "Size")

            col.separator()

            col = flow.column()
            factor_but(col, "use_map_velocity", "velocity_factor", "Physics Velocity")
            factor_but(col, "use_map_damp", "damp_factor", "Damp")
            factor_but(col, "use_map_gravity", "gravity_factor", "Gravity")
            factor_but(col, "use_map_field", "field_factor", "Force Fields")

            col.separator()

            col = flow.column()
            factor_but(col, "use_map_length", "length_factor", "Hair Length")
            factor_but(col, "use_map_clump", "clump_factor", "Clump")
            factor_but(col, "use_map_twist", "twist_factor", "Twist")

            col = flow.column()
            factor_but(col, "use_map_kink_amp", "kink_amp_factor", "Kink Amplitude")
            factor_but(col, "use_map_kink_freq", "kink_freq_factor", "Kink Frequency")
            factor_but(col, "use_map_rough", "rough_factor", "Rough")

        elif isinstance(idblock, FreestyleLineStyle):
            col = flow.column()
            factor_but(col, "use_map_color_diffuse", "diffuse_color_factor", "Color")
            factor_but(col, "use_map_alpha", "alpha_factor", "Alpha")

        if not isinstance(idblock, ParticleSettings):
            col = flow.column()

            col.prop(tex, "blend_type", text="Blend")

            # Color is used on gray-scale textures
            col.prop(tex, "color", text="")


class TextureColorsPoll:
    @classmethod
    def poll(cls, context):
        tex = context.texture
        return tex and (tex.type != 'NONE' or tex.use_nodes) and (context.engine in cls.COMPAT_ENGINES)


class TEXTURE_PT_colors(TextureButtonsPanel, TextureColorsPoll, Panel):
    bl_label = "Colors"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=False, even_rows=False, align=False)

        tex = context.texture

        col = flow.column()
        col.prop(tex, "use_clamp", text="Clamp")

        col = flow.column(align=True)
        col.prop(tex, "factor_red", text="Multiply R")
        col.prop(tex, "factor_green", text="G", text_ctxt=i18n_contexts.color)
        col.prop(tex, "factor_blue", text="B", text_ctxt=i18n_contexts.color)

        col.separator()

        col = flow.column()
        col.prop(tex, "intensity")
        col.prop(tex, "contrast")
        col.prop(tex, "saturation")


class TEXTURE_PT_colors_ramp(TextureButtonsPanel, TextureColorsPoll, Panel):
    bl_label = "Color Ramp"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "TEXTURE_PT_colors"
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }

    def draw_header(self, context):
        tex = context.texture
        self.layout.prop(tex, "use_color_ramp", text="")

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        # Note: TODO after creation of a new texture, the template_color_ramp will be blank.
        #       Possibly needs to be fixed in the template itself.
        is_active = bool(tex and tex.use_color_ramp)
        if is_active:
            layout.template_color_ramp(tex, "color_ramp", expand=True)
        else:
            layout.label(text="Enable the Color Ramp first")


class TEXTURE_PT_animation(TextureButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    @classmethod
    def poll(cls, context):
        return bool(context.texture)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        texture = context.texture

        # Assumption: the texture user is a particle system texture slot,
        # something like `bpy.data.particles["ParticleSettings"].texture_slots[0]`.
        # Since at the top of the properties panel the user is shown first, and
        # underneath that the texture itself, this panel uses the same order.
        if texture_user := context.texture_user:
            texture_user_id = texture_user.id_data
            col = layout.column(align=True)
            # NOTE(@sybren): I tested with particle settings, and then this just shows
            # "Particle Settings". If there are other users of Texture data-blocks
            # still around, and this produces unwanted results, let's adjust.
            col.label(text=texture_user_id.bl_rna.name)
            self.draw_action_and_slot_selector(context, col, texture_user_id)

        col = layout.column(align=True)
        col.label(text="Texture")
        self.draw_action_and_slot_selector(context, col, texture)

        if node_tree := texture.node_tree:
            col = layout.column(align=True)
            col.label(text="Shader Node Tree")
            self.draw_action_and_slot_selector(context, col, node_tree)


class TEXTURE_PT_custom_props(TextureButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_WORKBENCH',
    }
    _context_path = "texture"
    _property_type = Texture

    @classmethod
    def poll(cls, context):
        return context.texture and (context.engine in cls.COMPAT_ENGINES)


classes = (
    TEXTURE_MT_context_menu,
    TEXTURE_UL_texslots,
    TEXTURE_PT_preview,
    TEXTURE_PT_context,
    TEXTURE_PT_node,
    TEXTURE_PT_clouds,
    TEXTURE_PT_wood,
    TEXTURE_PT_marble,
    TEXTURE_PT_magic,
    TEXTURE_PT_blend,
    TEXTURE_PT_stucci,
    TEXTURE_PT_image,
    TEXTURE_PT_image_settings,
    TEXTURE_PT_image_alpha,
    TEXTURE_PT_image_mapping,
    TEXTURE_PT_image_mapping_crop,
    TEXTURE_PT_image_sampling,
    TEXTURE_PT_musgrave,
    TEXTURE_PT_voronoi,
    TEXTURE_PT_voronoi_feature_weights,
    TEXTURE_PT_distortednoise,
    TEXTURE_PT_influence,
    TEXTURE_PT_mapping,
    TEXTURE_PT_colors,
    TEXTURE_PT_colors_ramp,
    TEXTURE_PT_animation,
    TEXTURE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
