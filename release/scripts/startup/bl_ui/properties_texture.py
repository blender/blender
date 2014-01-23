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

from bpy.types import (Brush,
                       Lamp,
                       Material,
                       Object,
                       ParticleSettings,
                       Texture,
                       World)

from rna_prop_ui import PropertyPanel

from bl_ui.properties_paint_common import brush_texture_settings


class TEXTURE_MT_specials(Menu):
    bl_label = "Texture Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        layout.operator("texture.slot_copy", icon='COPYDOWN')
        layout.operator("texture.slot_paste", icon='PASTEDOWN')


class TEXTURE_MT_envmap_specials(Menu):
    bl_label = "Environment Map Specials"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        layout.operator("texture.envmap_save", icon='IMAGEFILE')
        layout.operator("texture.envmap_clear", icon='FILE_REFRESH')
        layout.operator("texture.envmap_clear_all", icon='FILE_REFRESH')


class TEXTURE_UL_texslots(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.MaterialTextureSlot)
        ma = data
        slot = item
        tex = slot.texture if slot else None
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if tex:
                layout.prop(tex, "name", text="", emboss=False, icon_value=icon)
            else:
                layout.label(text="", icon_value=icon)
            if tex and isinstance(item, bpy.types.MaterialTextureSlot):
                layout.prop(ma, "use_textures", text="", index=index)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


from bl_ui.properties_material import active_node_mat


def context_tex_datablock(context):
    idblock = context.material
    if idblock:
        return active_node_mat(idblock)

    idblock = context.lamp
    if idblock:
        return idblock

    idblock = context.world
    if idblock:
        return idblock

    idblock = context.brush
    if idblock:
        return idblock

    if context.particle_system:
        idblock = context.particle_system.settings

    return idblock


def id_tex_datablock(bid):
    if isinstance(bid, Object):
        if bid.type == 'LAMP':
            return bid.data
        return bid.active_material

    return bid


class TextureButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "texture"

    @classmethod
    def poll(cls, context):
        tex = context.texture
        return tex and (tex.type != 'NONE' or tex.use_nodes) and (context.scene.render.engine in cls.COMPAT_ENGINES)


class TEXTURE_PT_context_texture(TextureButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        #if not (hasattr(context, "texture_slot") or hasattr(context, "texture_node")):
            #return False
        return ((context.material or
                 context.world or
                 context.lamp or
                 context.texture or
                 context.particle_system or
                 isinstance(context.space_data.pin_id, ParticleSettings) or
                 context.texture_user) and
                (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        slot = getattr(context, "texture_slot", None)
        node = getattr(context, "texture_node", None)
        space = context.space_data
        tex = context.texture
        idblock = context_tex_datablock(context)
        pin_id = space.pin_id

        space.use_limited_texture_context = True

        if space.use_pin_id and not isinstance(pin_id, Texture):
            idblock = id_tex_datablock(pin_id)
            pin_id = None

        if not space.use_pin_id:
            layout.prop(space, "texture_context", expand=True)
            pin_id = None

        if space.texture_context == 'OTHER':
            if not pin_id:
                layout.template_texture_user()
            user = context.texture_user
            if user or pin_id:
                layout.separator()

                split = layout.split(percentage=0.65)
                col = split.column()

                if pin_id:
                    col.template_ID(space, "pin_id")
                else:
                    propname = context.texture_user_property.identifier
                    col.template_ID(user, propname, new="texture.new")

                if tex:
                    split = layout.split(percentage=0.2)
                    if tex.use_nodes:
                        if slot:
                            split.label(text="Output:")
                            split.prop(slot, "output_node", text="")
                    else:
                        split.label(text="Type:")
                        split.prop(tex, "type", text="")
            return

        tex_collection = (pin_id is None) and (node is None) and (not isinstance(idblock, Brush))

        if tex_collection:
            row = layout.row()

            row.template_list("TEXTURE_UL_texslots", "", idblock, "texture_slots", idblock, "active_texture_index", rows=2)

            col = row.column(align=True)
            col.operator("texture.slot_move", text="", icon='TRIA_UP').type = 'UP'
            col.operator("texture.slot_move", text="", icon='TRIA_DOWN').type = 'DOWN'
            col.menu("TEXTURE_MT_specials", icon='DOWNARROW_HLT', text="")

        if tex_collection:
            layout.template_ID(idblock, "active_texture", new="texture.new")
        elif node:
            layout.template_ID(node, "texture", new="texture.new")
        elif idblock:
            layout.template_ID(idblock, "texture", new="texture.new")

        if pin_id:
            layout.template_ID(space, "pin_id")

        if tex:
            split = layout.split(percentage=0.2)
            if tex.use_nodes:
                if slot:
                    split.label(text="Output:")
                    split.prop(slot, "output_node", text="")
            else:
                split.label(text="Type:")
                split.prop(tex, "type", text="")


class TEXTURE_PT_preview(TextureButtonsPanel, Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        slot = getattr(context, "texture_slot", None)
        idblock = context_tex_datablock(context)

        if idblock:
            layout.template_preview(tex, parent=idblock, slot=slot)
        else:
            layout.template_preview(tex, slot=slot)

        #Show Alpha Button for Brush Textures, see #29502
        if context.space_data.texture_context == 'BRUSH':
            layout.prop(tex, "use_preview_alpha")


class TEXTURE_PT_colors(TextureButtonsPanel, Panel):
    bl_label = "Colors"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "use_color_ramp", text="Ramp")
        if tex.use_color_ramp:
            layout.template_color_ramp(tex, "color_ramp", expand=True)

        split = layout.split()

        col = split.column()
        col.label(text="RGB Multiply:")
        sub = col.column(align=True)
        sub.prop(tex, "factor_red", text="R")
        sub.prop(tex, "factor_green", text="G")
        sub.prop(tex, "factor_blue", text="B")

        col = split.column()
        col.label(text="Adjust:")
        col.prop(tex, "intensity")
        col.prop(tex, "contrast")
        col.prop(tex, "saturation")

        col = layout.column()
        col.prop(tex, "use_clamp", text="Clamp")

# Texture Slot Panels #


class TextureSlotPanel(TextureButtonsPanel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        if not hasattr(context, "texture_slot"):
            return False

        engine = context.scene.render.engine
        return TextureButtonsPanel.poll(cls, context) and (engine in cls.COMPAT_ENGINES)


# Texture Type Panels #


class TextureTypePanel(TextureButtonsPanel):

    @classmethod
    def poll(cls, context):
        tex = context.texture
        engine = context.scene.render.engine
        return tex and ((tex.type == cls.tex_type and not tex.use_nodes) and (engine in cls.COMPAT_ENGINES))


class TEXTURE_PT_clouds(TextureTypePanel, Panel):
    bl_label = "Clouds"
    tex_type = 'CLOUDS'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "cloud_type", expand=True)
        layout.label(text="Noise:")
        layout.prop(tex, "noise_type", text="Type", expand=True)
        layout.prop(tex, "noise_basis", text="Basis")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_scale", text="Size")
        col.prop(tex, "noise_depth", text="Depth")

        split.prop(tex, "nabla", text="Nabla")


class TEXTURE_PT_wood(TextureTypePanel, Panel):
    bl_label = "Wood"
    tex_type = 'WOOD'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "noise_basis_2", expand=True)
        layout.prop(tex, "wood_type", expand=True)

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "marble_type", expand=True)
        layout.prop(tex, "noise_basis_2", expand=True)
        layout.label(text="Noise:")
        layout.prop(tex, "noise_type", text="Type", expand=True)
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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        row = layout.row()
        row.prop(tex, "noise_depth", text="Depth")
        row.prop(tex, "turbulence")


class TEXTURE_PT_blend(TextureTypePanel, Panel):
    bl_label = "Blend"
    tex_type = 'BLEND'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "stucci_type", expand=True)
        layout.label(text="Noise:")
        layout.prop(tex, "noise_type", text="Type", expand=True)
        layout.prop(tex, "noise_basis", text="Basis")

        row = layout.row()
        row.prop(tex, "noise_scale", text="Size")
        row.prop(tex, "turbulence")


class TEXTURE_PT_image(TextureTypePanel, Panel):
    bl_label = "Image"
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.template_image(tex, "image", tex.image_user)


def texture_filter_common(tex, layout):
    layout.label(text="Filter:")
    layout.prop(tex, "filter_type", text="")
    if tex.use_mipmap and tex.filter_type in {'AREA', 'EWA', 'FELINE'}:
        if tex.filter_type == 'FELINE':
            layout.prop(tex, "filter_probes", text="Probes")
        else:
            layout.prop(tex, "filter_eccentricity", text="Eccentricity")

    layout.prop(tex, "filter_size")
    layout.prop(tex, "use_filter_size_min")


class TEXTURE_PT_image_sampling(TextureTypePanel, Panel):
    bl_label = "Image Sampling"
    bl_options = {'DEFAULT_CLOSED'}
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        if context.scene.render.engine == 'BLENDER_GAME':
            self.draw_bge(context)
        else:
            self.draw_bi(context)

    def draw_bi(self, context):
        layout = self.layout

        idblock = context_tex_datablock(context)
        tex = context.texture
        slot = getattr(context, "texture_slot", None)

        split = layout.split()

        col = split.column()
        col.label(text="Alpha:")
        row = col.row()
        row.active = bool(tex.image and tex.image.use_alpha)
        row.prop(tex, "use_alpha", text="Use")
        col.prop(tex, "use_calculate_alpha", text="Calculate")
        col.prop(tex, "invert_alpha", text="Invert")
        col.separator()
        col.prop(tex, "use_flip_axis", text="Flip X/Y Axis")

        col = split.column()

        #Only for Material based textures, not for Lamp/World...
        if slot and isinstance(idblock, Material):
            col.prop(tex, "use_normal_map")
            row = col.row()
            row.active = tex.use_normal_map
            row.prop(slot, "normal_map_space", text="")

            row = col.row()
            row.active = not tex.use_normal_map
            row.prop(tex, "use_derivative_map")

        col.prop(tex, "use_mipmap")
        row = col.row()
        row.active = tex.use_mipmap
        row.prop(tex, "use_mipmap_gauss")
        col.prop(tex, "use_interpolation")

        texture_filter_common(tex, col)

    def draw_bge(self, context):
        layout = self.layout

        idblock = context_tex_datablock(context)
        tex = context.texture
        slot = getattr(context, "texture_slot", None)

        split = layout.split()

        col = split.column()
        col.label(text="Alpha:")
        col.prop(tex, "use_calculate_alpha", text="Calculate")
        col.prop(tex, "invert_alpha", text="Invert")

        col = split.column()

        #Only for Material based textures, not for Lamp/World...
        if slot and isinstance(idblock, Material):
            col.prop(tex, "use_normal_map")
            row = col.row()
            row.active = tex.use_normal_map
            row.prop(slot, "normal_map_space", text="")

            row = col.row()
            row.active = not tex.use_normal_map
            row.prop(tex, "use_derivative_map")


class TEXTURE_PT_image_mapping(TextureTypePanel, Panel):
    bl_label = "Image Mapping"
    bl_options = {'DEFAULT_CLOSED'}
    tex_type = 'IMAGE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.prop(tex, "extension")

        split = layout.split()

        if tex.extension == 'REPEAT':
            col = split.column(align=True)
            col.label(text="Repeat:")
            col.prop(tex, "repeat_x", text="X")
            col.prop(tex, "repeat_y", text="Y")

            col = split.column(align=True)
            col.label(text="Mirror:")
            row = col.row(align=True)
            row.prop(tex, "use_mirror_x", text="X")
            row.active = (tex.repeat_x > 1)
            row = col.row(align=True)
            row.prop(tex, "use_mirror_y", text="Y")
            row.active = (tex.repeat_y > 1)
            layout.separator()

        elif tex.extension == 'CHECKER':
            col = split.column(align=True)
            row = col.row(align=True)
            row.prop(tex, "use_checker_even", text="Even")
            row.prop(tex, "use_checker_odd", text="Odd")

            col = split.column()
            col.prop(tex, "checker_distance", text="Distance")

            layout.separator()

        split = layout.split()

        col = split.column(align=True)
        #col.prop(tex, "crop_rectangle")
        col.label(text="Crop Minimum:")
        col.prop(tex, "crop_min_x", text="X")
        col.prop(tex, "crop_min_y", text="Y")

        col = split.column(align=True)
        col.label(text="Crop Maximum:")
        col.prop(tex, "crop_max_x", text="X")
        col.prop(tex, "crop_max_y", text="Y")


class TEXTURE_PT_envmap(TextureTypePanel, Panel):
    bl_label = "Environment Map"
    tex_type = 'ENVIRONMENT_MAP'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        env = tex.environment_map

        row = layout.row()
        row.prop(env, "source", expand=True)
        row.menu("TEXTURE_MT_envmap_specials", icon='DOWNARROW_HLT', text="")

        if env.source == 'IMAGE_FILE':
            layout.template_ID(tex, "image", open="image.open")
            layout.template_image(tex, "image", tex.image_user, compact=True)
        else:
            layout.prop(env, "mapping")
            if env.mapping == 'PLANE':
                layout.prop(env, "zoom")
            layout.prop(env, "viewpoint_object")

            split = layout.split()

            col = split.column()
            col.prop(env, "layers_ignore")
            col.prop(env, "resolution")
            col.prop(env, "depth")

            col = split.column(align=True)

            col.label(text="Clipping:")
            col.prop(env, "clip_start", text="Start")
            col.prop(env, "clip_end", text="End")


class TEXTURE_PT_envmap_sampling(TextureTypePanel, Panel):
    bl_label = "Environment Map Sampling"
    bl_options = {'DEFAULT_CLOSED'}
    tex_type = 'ENVIRONMENT_MAP'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        texture_filter_common(tex, layout)


class TEXTURE_PT_musgrave(TextureTypePanel, Panel):
    bl_label = "Musgrave"
    tex_type = 'MUSGRAVE'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

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
        if musgrave_type in {'MULTIFRACTAL', 'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL'}:
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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

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
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

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


class TEXTURE_PT_voxeldata(TextureButtonsPanel, Panel):
    bl_label = "Voxel Data"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        tex = context.texture
        engine = context.scene.render.engine
        return tex and (tex.type == 'VOXEL_DATA' and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        vd = tex.voxel_data

        layout.prop(vd, "file_format")
        if vd.file_format in {'BLENDER_VOXEL', 'RAW_8BIT'}:
            layout.prop(vd, "filepath")
        if vd.file_format == 'RAW_8BIT':
            layout.prop(vd, "resolution")
        elif vd.file_format == 'SMOKE':
            layout.prop(vd, "domain_object")
            layout.prop(vd, "smoke_data_type")
        elif vd.file_format == 'IMAGE_SEQUENCE':
            layout.template_ID(tex, "image", open="image.open")
            layout.template_image(tex, "image", tex.image_user, compact=True)
            #layout.prop(vd, "frame_duration")

        if vd.file_format in {'BLENDER_VOXEL', 'RAW_8BIT'}:
            layout.prop(vd, "use_still_frame")
            row = layout.row()
            row.active = vd.use_still_frame
            row.prop(vd, "still_frame")

        layout.prop(vd, "interpolation")
        layout.prop(vd, "extension")
        layout.prop(vd, "intensity")


class TEXTURE_PT_pointdensity(TextureButtonsPanel, Panel):
    bl_label = "Point Density"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        tex = context.texture
        engine = context.scene.render.engine
        return tex and (tex.type == 'POINT_DENSITY' and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        pd = tex.point_density

        layout.prop(pd, "point_source", expand=True)

        split = layout.split()

        col = split.column()
        if pd.point_source == 'PARTICLE_SYSTEM':
            col.label(text="Object:")
            col.prop(pd, "object", text="")

            sub = col.column()
            sub.enabled = bool(pd.object)
            if pd.object:
                sub.label(text="System:")
                sub.prop_search(pd, "particle_system", pd.object, "particle_systems", text="")
            sub.label(text="Cache:")
            sub.prop(pd, "particle_cache_space", text="")
        else:
            col.label(text="Object:")
            col.prop(pd, "object", text="")
            col.label(text="Cache:")
            col.prop(pd, "vertex_cache_space", text="")

        col.separator()

        if pd.point_source == 'PARTICLE_SYSTEM':
            col.label(text="Color Source:")
            col.prop(pd, "color_source", text="")
            if pd.color_source in {'PARTICLE_SPEED', 'PARTICLE_VELOCITY'}:
                col.prop(pd, "speed_scale")
            if pd.color_source in {'PARTICLE_SPEED', 'PARTICLE_AGE'}:
                layout.template_color_ramp(pd, "color_ramp", expand=True)

        col = split.column()
        col.label()
        col.prop(pd, "radius")
        col.label(text="Falloff:")
        col.prop(pd, "falloff", text="")
        if pd.falloff == 'SOFT':
            col.prop(pd, "falloff_soft")
        if pd.falloff == 'PARTICLE_VELOCITY':
            col.prop(pd, "falloff_speed_scale")

        col.prop(pd, "use_falloff_curve")

        if pd.use_falloff_curve:
            col = layout.column()
            col.label(text="Falloff Curve")
            col.template_curve_mapping(pd, "falloff_curve", brush=False)


class TEXTURE_PT_pointdensity_turbulence(TextureButtonsPanel, Panel):
    bl_label = "Turbulence"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        tex = context.texture
        engine = context.scene.render.engine
        return tex and (tex.type == 'POINT_DENSITY' and (engine in cls.COMPAT_ENGINES))

    def draw_header(self, context):
        pd = context.texture.point_density

        self.layout.prop(pd, "use_turbulence", text="")

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        pd = tex.point_density
        layout.active = pd.use_turbulence

        split = layout.split()

        col = split.column()
        col.label(text="Influence:")
        col.prop(pd, "turbulence_influence", text="")
        col.label(text="Noise Basis:")
        col.prop(pd, "noise_basis", text="")

        col = split.column()
        col.label()
        col.prop(pd, "turbulence_scale")
        col.prop(pd, "turbulence_depth")
        col.prop(pd, "turbulence_strength")


class TEXTURE_PT_ocean(TextureTypePanel, Panel):
    bl_label = "Ocean"
    tex_type = 'OCEAN'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        ot = tex.ocean

        col = layout.column()
        col.prop(ot, "ocean_object")
        col.prop(ot, "output")


class TEXTURE_PT_mapping(TextureSlotPanel, Panel):
    bl_label = "Mapping"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        idblock = context_tex_datablock(context)
        if isinstance(idblock, Brush) and not context.sculpt_object:
            return False

        if not getattr(context, "texture_slot", None):
            return False

        engine = context.scene.render.engine
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
                    split.prop_search(tex, "uv_layer", ob.data, "uv_textures", text="")
                else:
                    split.prop(tex, "uv_layer", text="")

            elif tex.texture_coords == 'OBJECT':
                split = layout.split(percentage=0.3)
                split.label(text="Object:")
                split.prop(tex, "object", text="")

        if isinstance(idblock, Brush):
            if context.sculpt_object or context.image_paint_object:
                brush_texture_settings(layout, idblock, context.sculpt_object)
        else:
            if isinstance(idblock, Material):
                split = layout.split(percentage=0.3)
                split.label(text="Projection:")
                split.prop(tex, "mapping", text="")

                split = layout.split()

                col = split.column()
                if tex.texture_coords in {'ORCO', 'UV'}:
                    col.prop(tex, "use_from_dupli")
                    if (idblock.type == 'VOLUME' and tex.texture_coords == 'ORCO'):
                        col.prop(tex, "use_map_to_bounds")
                elif tex.texture_coords == 'OBJECT':
                    col.prop(tex, "use_from_original")
                    if (idblock.type == 'VOLUME'):
                        col.prop(tex, "use_map_to_bounds")
                else:
                    col.label()

                col = split.column()
                row = col.row()
                row.prop(tex, "mapping_x", text="")
                row.prop(tex, "mapping_y", text="")
                row.prop(tex, "mapping_z", text="")

            row = layout.row()
            row.column().prop(tex, "offset")
            row.column().prop(tex, "scale")


class TEXTURE_PT_influence(TextureSlotPanel, Panel):
    bl_label = "Influence"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        idblock = context_tex_datablock(context)
        if isinstance(idblock, Brush):
            return False

        if not getattr(context, "texture_slot", None):
            return False

        engine = context.scene.render.engine
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

        if isinstance(idblock, Material):
            if idblock.type in {'SURFACE', 'WIRE'}:
                split = layout.split()

                col = split.column()
                col.label(text="Diffuse:")
                factor_but(col, "use_map_diffuse", "diffuse_factor", "Intensity")
                factor_but(col, "use_map_color_diffuse", "diffuse_color_factor", "Color")
                factor_but(col, "use_map_alpha", "alpha_factor", "Alpha")
                factor_but(col, "use_map_translucency", "translucency_factor", "Translucency")

                col.label(text="Specular:")
                factor_but(col, "use_map_specular", "specular_factor", "Intensity")
                factor_but(col, "use_map_color_spec", "specular_color_factor", "Color")
                factor_but(col, "use_map_hardness", "hardness_factor", "Hardness")

                col = split.column()
                col.label(text="Shading:")
                factor_but(col, "use_map_ambient", "ambient_factor", "Ambient")
                factor_but(col, "use_map_emit", "emit_factor", "Emit")
                factor_but(col, "use_map_mirror", "mirror_factor", "Mirror")
                factor_but(col, "use_map_raymir", "raymir_factor", "Ray Mirror")

                col.label(text="Geometry:")
                # XXX replace 'or' when displacement is fixed to not rely on normal influence value.
                sub_tmp = factor_but(col, "use_map_normal", "normal_factor", "Normal")
                sub_tmp.active = (tex.use_map_normal or tex.use_map_displacement)
                # END XXX

                factor_but(col, "use_map_warp", "warp_factor", "Warp")
                factor_but(col, "use_map_displacement", "displacement_factor", "Displace")

                #~ sub = col.column()
                #~ sub.active = tex.use_map_translucency or tex.map_emit or tex.map_alpha or tex.map_raymir or tex.map_hardness or tex.map_ambient or tex.map_specularity or tex.map_reflection or tex.map_mirror
                #~ sub.prop(tex, "default_value", text="Amount", slider=True)
            elif idblock.type == 'HALO':
                layout.label(text="Halo:")

                split = layout.split()

                col = split.column()
                factor_but(col, "use_map_color_diffuse", "diffuse_color_factor", "Color")
                factor_but(col, "use_map_alpha", "alpha_factor", "Alpha")

                col = split.column()
                factor_but(col, "use_map_raymir", "raymir_factor", "Size")
                factor_but(col, "use_map_hardness", "hardness_factor", "Hardness")
                factor_but(col, "use_map_translucency", "translucency_factor", "Add")
            elif idblock.type == 'VOLUME':
                split = layout.split()

                col = split.column()
                factor_but(col, "use_map_density", "density_factor", "Density")
                factor_but(col, "use_map_emission", "emission_factor", "Emission")
                factor_but(col, "use_map_scatter", "scattering_factor", "Scattering")
                factor_but(col, "use_map_reflect", "reflection_factor", "Reflection")

                col = split.column()
                col.label(text=" ")
                factor_but(col, "use_map_color_emission", "emission_color_factor", "Emission Color")
                factor_but(col, "use_map_color_transmission", "transmission_color_factor", "Transmission Color")
                factor_but(col, "use_map_color_reflection", "reflection_color_factor", "Reflection Color")

        elif isinstance(idblock, Lamp):
            split = layout.split()

            col = split.column()
            factor_but(col, "use_map_color", "color_factor", "Color")

            col = split.column()
            factor_but(col, "use_map_shadow", "shadow_factor", "Shadow")

        elif isinstance(idblock, World):
            split = layout.split()

            col = split.column()
            factor_but(col, "use_map_blend", "blend_factor", "Blend")
            factor_but(col, "use_map_horizon", "horizon_factor", "Horizon")

            col = split.column()
            factor_but(col, "use_map_zenith_up", "zenith_up_factor", "Zenith Up")
            factor_but(col, "use_map_zenith_down", "zenith_down_factor", "Zenith Down")
        elif isinstance(idblock, ParticleSettings):
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

            col = split.column()
            factor_but(col, "use_map_kink", "kink_factor", "Kink")
            factor_but(col, "use_map_rough", "rough_factor", "Rough")

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

        if isinstance(idblock, Material) or isinstance(idblock, World):
            col.prop(tex, "default_value", text="DVar", slider=True)

        if isinstance(idblock, Material):
            layout.label(text="Bump Mapping:")

            # only show bump settings if activated but not for normal-map images
            row = layout.row()

            sub = row.row()
            sub.active = (tex.use_map_normal or tex.use_map_warp) and not (tex.texture.type == 'IMAGE' and (tex.texture.use_normal_map or tex.texture.use_derivative_map))
            sub.prop(tex, "bump_method", text="Method")

            # the space setting is supported for: derivative-maps + bump-maps (DEFAULT,BEST_QUALITY), not for normal-maps
            sub = row.row()
            sub.active = (tex.use_map_normal or tex.use_map_warp) and not (tex.texture.type == 'IMAGE' and tex.texture.use_normal_map) and ((tex.bump_method in {'BUMP_LOW_QUALITY', 'BUMP_MEDIUM_QUALITY', 'BUMP_BEST_QUALITY'}) or (tex.texture.type == 'IMAGE' and tex.texture.use_derivative_map))
            sub.prop(tex, "bump_objectspace", text="Space")


class TEXTURE_PT_custom_props(TextureButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    _context_path = "texture"
    _property_type = Texture

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
