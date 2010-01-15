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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from rna_prop_ui import PropertyPanel

narrowui = 180


def active_node_mat(mat):
    if mat:
        mat_node = mat.active_node_material
        if mat_node:
            return mat_node
        else:
            return mat

    return None


def context_tex_datablock(context):
    idblock = active_node_mat(context.material)
    if idblock:
        return idblock

    idblock = context.lamp
    if idblock:
        return idblock

    idblock = context.world
    if idblock:
        return idblock

    idblock = context.brush
    return idblock


class TextureButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "texture"

    def poll(self, context):
        tex = context.texture
        return (tex and (tex.type != 'NONE' or tex.use_nodes))


class TEXTURE_PT_preview(TextureButtonsPanel):
    bl_label = "Preview"

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        slot = context.texture_slot

        idblock = context_tex_datablock(context)

        if idblock:
            layout.template_preview(tex, parent=idblock, slot=slot)
        else:
            layout.template_preview(tex, slot=slot)


class TEXTURE_PT_context_texture(TextureButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def poll(self, context):
        return (context.material or context.world or context.lamp or context.brush or context.texture)

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        tex = context.texture
        wide_ui = context.region.width > narrowui
        idblock = context_tex_datablock(context)
        tex_collection = space.pin_id == None and type(idblock) != bpy.types.Brush

        

        if tex_collection:
            row = layout.row()

            row.template_list(idblock, "textures", idblock, "active_texture_index", rows=2)

            col = row.column(align=True)
            col.operator("texture.slot_move", text="", icon='TRIA_UP').type = 'UP'
            col.operator("texture.slot_move", text="", icon='TRIA_DOWN').type = 'DOWN'
        
        if wide_ui:
            split = layout.split(percentage=0.65)
            col = split.column()
        else:
            col = layout.column()
            
        if tex_collection:
            col.template_ID(idblock, "active_texture", new="texture.new")
        elif idblock:
            col.template_ID(idblock, "texture", new="texture.new")
        
        if space.pin_id:
            col.template_ID(space, "pin_id")
        
        if wide_ui:
            col = split.column()
        
        if (not space.pin_id) and (
            context.sculpt_object or
            context.vertex_paint_object or
            context.weight_paint_object or
            context.texture_paint_object):

            col.prop(space, "brush_texture", text="Brush", toggle=True)

        if tex:
            layout.prop(tex, "use_nodes")

            split = layout.split(percentage=0.2)

            if tex.use_nodes:
                slot = context.texture_slot

                if slot:
                    split.label(text="Output:")
                    split.prop(slot, "output_node", text="")

            else:
                if wide_ui:
                    split.label(text="Type:")
                    split.prop(tex, "type", text="")
                else:
                    layout.prop(tex, "type", text="")


class TEXTURE_PT_custom_props(TextureButtonsPanel, PropertyPanel):
    _context_path = "texture"

    def poll(self, context): # use alternate poll since NONE texture type is ok
        return context.texture


class TEXTURE_PT_colors(TextureButtonsPanel):
    bl_label = "Colors"
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

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

        if wide_ui:
            col = split.column()
        col.label(text="Adjust:")
        col.prop(tex, "brightness")
        col.prop(tex, "contrast")

# Texture Slot Panels #


class TextureSlotPanel(TextureButtonsPanel):

    def poll(self, context):
        return (context.texture_slot and
                TextureButtonsPanel.poll(self, context))


class TEXTURE_PT_mapping(TextureSlotPanel):
    bl_label = "Mapping"

    def draw(self, context):
        layout = self.layout

        idblock = context_tex_datablock(context)

        tex = context.texture_slot
        # textype = context.texture
        wide_ui = context.region.width > narrowui

        if type(idblock) != bpy.types.Brush:
            split = layout.split(percentage=0.3)
            col = split.column()
            col.label(text="Coordinates:")
            col = split.column()
            col.prop(tex, "texture_coordinates", text="")

            if tex.texture_coordinates == 'ORCO':
                """
                ob = context.object
                if ob and ob.type == 'MESH':
                    split = layout.split(percentage=0.3)
                    split.label(text="Mesh:")
                    split.prop(ob.data, "texco_mesh", text="")
                """
            elif tex.texture_coordinates == 'UV':
                split = layout.split(percentage=0.3)
                split.label(text="Layer:")
                ob = context.object
                if ob and ob.type == 'MESH':
                    split.prop_object(tex, "uv_layer", ob.data, "uv_textures", text="")
                else:
                    split.prop(tex, "uv_layer", text="")

            elif tex.texture_coordinates == 'OBJECT':
                split = layout.split(percentage=0.3)
                split.label(text="Object:")
                split.prop(tex, "object", text="")

        if type(idblock) == bpy.types.Brush:
            layout.prop(tex, "map_mode", expand=True)

            row = layout.row()
            row.active = tex.map_mode in ('FIXED', 'TILED')
            row.prop(tex, "angle")

            row = layout.row()
            row.active = tex.map_mode in ('TILED', '3D')
            row.column().prop(tex, "size")
        else:
            if type(idblock) == bpy.types.Material:
                split = layout.split(percentage=0.3)
                split.label(text="Projection:")
                split.prop(tex, "mapping", text="")

                split = layout.split()

                col = split.column()
                if tex.texture_coordinates in ('ORCO', 'UV'):
                    col.prop(tex, "from_dupli")
                elif tex.texture_coordinates == 'OBJECT':
                    col.prop(tex, "from_original")
                elif wide_ui:
                    col.label()

                if wide_ui:
                    col = split.column()
                row = col.row()
                row.prop(tex, "x_mapping", text="")
                row.prop(tex, "y_mapping", text="")
                row.prop(tex, "z_mapping", text="")

            # any non brush
            split = layout.split()

            col = split.column()
            col.prop(tex, "offset")

            if wide_ui:
                col = split.column()
            col.prop(tex, "size")


class TEXTURE_PT_influence(TextureSlotPanel):
    bl_label = "Influence"

    def poll(self, context):
        idblock = context_tex_datablock(context)
        if type(idblock) == bpy.types.Brush:
            return False
    
        return context.texture_slot

    def draw(self, context):

        layout = self.layout

        idblock = context_tex_datablock(context)

        # textype = context.texture
        tex = context.texture_slot
        wide_ui = context.region.width > narrowui

        def factor_but(layout, active, toggle, factor, name):
            row = layout.row(align=True)
            row.prop(tex, toggle, text="")
            sub = row.row()
            sub.active = active
            sub.prop(tex, factor, text=name, slider=True)

        if type(idblock) == bpy.types.Material:
            if idblock.type in ('SURFACE', 'HALO', 'WIRE'):
                split = layout.split()

                col = split.column()
                col.label(text="Diffuse:")
                factor_but(col, tex.map_diffuse, "map_diffuse", "diffuse_factor", "Intensity")
                factor_but(col, tex.map_colordiff, "map_colordiff", "colordiff_factor", "Color")
                factor_but(col, tex.map_alpha, "map_alpha", "alpha_factor", "Alpha")
                factor_but(col, tex.map_translucency, "map_translucency", "translucency_factor", "Translucency")

                col.label(text="Specular:")
                factor_but(col, tex.map_specular, "map_specular", "specular_factor", "Intensity")
                factor_but(col, tex.map_colorspec, "map_colorspec", "colorspec_factor", "Color")
                factor_but(col, tex.map_hardness, "map_hardness", "hardness_factor", "Hardness")

                if wide_ui:
                    col = split.column()
                col.label(text="Shading:")
                factor_but(col, tex.map_ambient, "map_ambient", "ambient_factor", "Ambient")
                factor_but(col, tex.map_emit, "map_emit", "emit_factor", "Emit")
                factor_but(col, tex.map_mirror, "map_mirror", "mirror_factor", "Mirror")
                factor_but(col, tex.map_raymir, "map_raymir", "raymir_factor", "Ray Mirror")

                col.label(text="Geometry:")
                factor_but(col, tex.map_normal, "map_normal", "normal_factor", "Normal")
                factor_but(col, tex.map_warp, "map_warp", "warp_factor", "Warp")
                factor_but(col, tex.map_displacement, "map_displacement", "displacement_factor", "Displace")

                #sub = col.column()
                #sub.active = tex.map_translucency or tex.map_emit or tex.map_alpha or tex.map_raymir or tex.map_hardness or tex.map_ambient or tex.map_specularity or tex.map_reflection or tex.map_mirror
                #sub.prop(tex, "default_value", text="Amount", slider=True)
            elif idblock.type == 'VOLUME':
                split = layout.split()

                col = split.column()
                factor_but(col, tex.map_density, "map_density", "density_factor", "Density")
                factor_but(col, tex.map_emission, "map_emission", "emission_factor", "Emission")
                factor_but(col, tex.map_scattering, "map_scattering", "scattering_factor", "Scattering")
                factor_but(col, tex.map_reflection, "map_reflection", "reflection_factor", "Reflection")

                if wide_ui:
                    col = split.column()
                    col.label(text=" ")
                factor_but(col, tex.map_alpha, "map_coloremission", "coloremission_factor", "Emission Color")
                factor_but(col, tex.map_colortransmission, "map_colortransmission", "colortransmission_factor", "Transmission Color")
                factor_but(col, tex.map_colorreflection, "map_colorreflection", "colorreflection_factor", "Reflection Color")

        elif type(idblock) == bpy.types.Lamp:
            split = layout.split()

            col = split.column()
            factor_but(col, tex.map_color, "map_color", "color_factor", "Color")

            if wide_ui:
                col = split.column()
            factor_but(col, tex.map_shadow, "map_shadow", "shadow_factor", "Shadow")

        elif type(idblock) == bpy.types.World:
            split = layout.split()

            col = split.column()
            factor_but(col, tex.map_blend, "map_blend", "blend_factor", "Blend")
            factor_but(col, tex.map_horizon, "map_horizon", "horizon_factor", "Horizon")

            if wide_ui:
                col = split.column()
            factor_but(col, tex.map_zenith_up, "map_zenith_up", "zenith_up_factor", "Zenith Up")
            factor_but(col, tex.map_zenith_down, "map_zenith_down", "zenith_down_factor", "Zenith Down")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(tex, "blend_type", text="Blend")
        col.prop(tex, "rgb_to_intensity")
        sub = col.column()
        sub.active = tex.rgb_to_intensity
        sub.prop(tex, "color", text="")

        if wide_ui:
            col = split.column()
        col.prop(tex, "negate", text="Negative")
        col.prop(tex, "stencil")

        if type(idblock) in (bpy.types.Material, bpy.types.World):
            col.prop(tex, "default_value", text="DVar", slider=True)

# Texture Type Panels #


class TextureTypePanel(TextureButtonsPanel):

    def poll(self, context):
        tex = context.texture
        return (tex and tex.type == self.tex_type and not tex.use_nodes)


class TEXTURE_PT_clouds(TextureTypePanel):
    bl_label = "Clouds"
    tex_type = 'CLOUDS'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        layout.prop(tex, "stype", expand=True)
        layout.label(text="Noise:")
        layout.prop(tex, "noise_type", text="Type", expand=True)
        if wide_ui:
            layout.prop(tex, "noise_basis", text="Basis")
        else:
            layout.prop(tex, "noise_basis", text="")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_size", text="Size")
        col.prop(tex, "noise_depth", text="Depth")

        if wide_ui:
            col = split.column()
        col.prop(tex, "nabla", text="Nabla")


class TEXTURE_PT_wood(TextureTypePanel):
    bl_label = "Wood"
    tex_type = 'WOOD'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        layout.prop(tex, "noisebasis2", expand=True)
        if wide_ui:
            layout.prop(tex, "stype", expand=True)
        else:
            layout.prop(tex, "stype", text="")

        col = layout.column()
        col.active = tex.stype in ('RINGNOISE', 'BANDNOISE')
        col.label(text="Noise:")
        col.row().prop(tex, "noise_type", text="Type", expand=True)
        if wide_ui:
            layout.prop(tex, "noise_basis", text="Basis")
        else:
            layout.prop(tex, "noise_basis", text="")

        split = layout.split()
        split.active = tex.stype in ('RINGNOISE', 'BANDNOISE')

        col = split.column()
        col.prop(tex, "noise_size", text="Size")
        col.prop(tex, "turbulence")

        col = split.column()
        col.prop(tex, "nabla")


class TEXTURE_PT_marble(TextureTypePanel):
    bl_label = "Marble"
    tex_type = 'MARBLE'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        layout.prop(tex, "stype", expand=True)
        layout.prop(tex, "noisebasis2", expand=True)
        layout.label(text="Noise:")
        layout.prop(tex, "noise_type", text="Type", expand=True)
        if wide_ui:
            layout.prop(tex, "noise_basis", text="Basis")
        else:
            layout.prop(tex, "noise_basis", text="")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_size", text="Size")
        col.prop(tex, "noise_depth", text="Depth")

        if wide_ui:
            col = split.column()
        col.prop(tex, "turbulence")
        col.prop(tex, "nabla")


class TEXTURE_PT_magic(TextureTypePanel):
    bl_label = "Magic"
    tex_type = 'MAGIC'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_depth", text="Depth")

        if wide_ui:
            col = split.column()
        col.prop(tex, "turbulence")


class TEXTURE_PT_blend(TextureTypePanel):
    bl_label = "Blend"
    tex_type = 'BLEND'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(tex, "progression")
        else:
            layout.prop(tex, "progression", text="")

        sub = layout.row()

        sub.active = (tex.progression in ('LINEAR', 'QUADRATIC', 'EASING', 'RADIAL'))
        sub.prop(tex, "flip_axis", expand=True)


class TEXTURE_PT_stucci(TextureTypePanel):
    bl_label = "Stucci"
    tex_type = 'STUCCI'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        layout.prop(tex, "stype", expand=True)
        layout.label(text="Noise:")
        layout.prop(tex, "noise_type", text="Type", expand=True)
        if wide_ui:
            layout.prop(tex, "noise_basis", text="Basis")
        else:
            layout.prop(tex, "noise_basis", text="")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_size", text="Size")

        if wide_ui:
            col = split.column()
        col.prop(tex, "turbulence")


class TEXTURE_PT_image(TextureTypePanel):
    bl_label = "Image"
    tex_type = 'IMAGE'

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.template_image(tex, "image", tex.image_user)


class TEXTURE_PT_image_sampling(TextureTypePanel):
    bl_label = "Image Sampling"
    bl_default_closed = True
    tex_type = 'IMAGE'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        # slot = context.texture_slot
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Alpha:")
        col.prop(tex, "use_alpha", text="Use")
        col.prop(tex, "calculate_alpha", text="Calculate")
        col.prop(tex, "invert_alpha", text="Invert")
        col.separator()
        col.prop(tex, "flip_axis", text="Flip X/Y Axis")

        if wide_ui:
            col = split.column()
        else:
            col.separator()
        col.prop(tex, "normal_map")
        row = col.row()
        row.active = tex.normal_map
        row.prop(tex, "normal_space", text="")

        col.label(text="Filter:")
        col.prop(tex, "filter", text="")
        col.prop(tex, "filter_size")
        col.prop(tex, "filter_size_minimum")
        col.prop(tex, "mipmap")

        row = col.row()
        row.active = tex.mipmap
        row.prop(tex, "mipmap_gauss")

        col.prop(tex, "interpolation")
        if tex.mipmap and tex.filter != 'DEFAULT':
            if tex.filter == 'FELINE':
                col.prop(tex, "filter_probes", text="Probes")
            else:
                col.prop(tex, "filter_eccentricity", text="Eccentricity")


class TEXTURE_PT_image_mapping(TextureTypePanel):
    bl_label = "Image Mapping"
    bl_default_closed = True
    tex_type = 'IMAGE'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(tex, "extension")
        else:
            layout.prop(tex, "extension", text="")

        split = layout.split()

        if tex.extension == 'REPEAT':
            col = split.column(align=True)
            col.label(text="Repeat:")
            col.prop(tex, "repeat_x", text="X")
            col.prop(tex, "repeat_y", text="Y")

            if wide_ui:
                col = split.column(align=True)
            col.label(text="Mirror:")
            col.prop(tex, "mirror_x", text="X")
            col.prop(tex, "mirror_y", text="Y")
            layout.separator()

        elif tex.extension == 'CHECKER':
            col = split.column(align=True)
            row = col.row()
            row.prop(tex, "checker_even", text="Even")
            row.prop(tex, "checker_odd", text="Odd")

            if wide_ui:
                col = split.column()
            col.prop(tex, "checker_distance", text="Distance")

            layout.separator()

        split = layout.split()

        col = split.column(align=True)
        #col.prop(tex, "crop_rectangle")
        col.label(text="Crop Minimum:")
        col.prop(tex, "crop_min_x", text="X")
        col.prop(tex, "crop_min_y", text="Y")

        if wide_ui:
            col = split.column(align=True)
        col.label(text="Crop Maximum:")
        col.prop(tex, "crop_max_x", text="X")
        col.prop(tex, "crop_max_y", text="Y")


class TEXTURE_PT_plugin(TextureTypePanel):
    bl_label = "Plugin"
    tex_type = 'PLUGIN'

    def draw(self, context):
        layout = self.layout

        # tex = context.texture

        layout.label(text="Nothing yet")


class TEXTURE_PT_envmap(TextureTypePanel):
    bl_label = "Environment Map"
    tex_type = 'ENVIRONMENT_MAP'

    def draw(self, context):
        layout = self.layout

        # tex = context.texture

        layout.label(text="Nothing yet")


class TEXTURE_PT_musgrave(TextureTypePanel):
    bl_label = "Musgrave"
    tex_type = 'MUSGRAVE'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(tex, "musgrave_type")
        else:
            layout.prop(tex, "musgrave_type", text="")

        split = layout.split()

        col = split.column()
        col.prop(tex, "highest_dimension", text="Dimension")
        col.prop(tex, "lacunarity")
        col.prop(tex, "octaves")

        if wide_ui:
            col = split.column()
        if (tex.musgrave_type in ('HETERO_TERRAIN', 'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL')):
            col.prop(tex, "offset")
        if (tex.musgrave_type in ('RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL')):
            col.prop(tex, "gain")
            col.prop(tex, "noise_intensity", text="Intensity")

        layout.label(text="Noise:")

        if wide_ui:
            layout.prop(tex, "noise_basis", text="Basis")
        else:
            layout.prop(tex, "noise_basis", text="")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_size", text="Size")

        if wide_ui:
            col = split.column()
        col.prop(tex, "nabla")


class TEXTURE_PT_voronoi(TextureTypePanel):
    bl_label = "Voronoi"
    tex_type = 'VORONOI'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Distance Metric:")
        col.prop(tex, "distance_metric", text="")
        sub = col.column()
        sub.active = tex.distance_metric == 'MINKOVSKY'
        sub.prop(tex, "minkovsky_exponent", text="Exponent")
        col.label(text="Coloring:")
        col.prop(tex, "coloring", text="")
        col.prop(tex, "noise_intensity", text="Intensity")

        if wide_ui:
            col = split.column()
        sub = col.column(align=True)
        sub.label(text="Feature Weights:")
        sub.prop(tex, "weight_1", text="1", slider=True)
        sub.prop(tex, "weight_2", text="2", slider=True)
        sub.prop(tex, "weight_3", text="3", slider=True)
        sub.prop(tex, "weight_4", text="4", slider=True)

        layout.label(text="Noise:")

        split = layout.split()

        col = split.column()
        col.prop(tex, "noise_size", text="Size")

        if wide_ui:
            col = split.column()
        col.prop(tex, "nabla")


class TEXTURE_PT_distortednoise(TextureTypePanel):
    bl_label = "Distorted Noise"
    tex_type = 'DISTORTED_NOISE'

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(tex, "noise_distortion")
            layout.prop(tex, "noise_basis", text="Basis")
        else:
            layout.prop(tex, "noise_distortion", text="")
            layout.prop(tex, "noise_basis", text="")

        split = layout.split()

        col = split.column()
        col.prop(tex, "distortion", text="Distortion")
        col.prop(tex, "noise_size", text="Size")

        if wide_ui:
            col = split.column()
        col.prop(tex, "nabla")


class TEXTURE_PT_voxeldata(TextureButtonsPanel):
    bl_label = "Voxel Data"

    def poll(self, context):
        tex = context.texture
        return (tex and tex.type == 'VOXEL_DATA')

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        vd = tex.voxeldata

        layout.prop(vd, "file_format")
        if vd.file_format in ['BLENDER_VOXEL', 'RAW_8BIT']:
            layout.prop(vd, "source_path")
        if vd.file_format == 'RAW_8BIT':
            layout.prop(vd, "resolution")
        elif vd.file_format == 'SMOKE':
            layout.prop(vd, "domain_object")
        elif vd.file_format == 'IMAGE_SEQUENCE':
            layout.template_image(tex, "image", tex.image_user) 

        layout.prop(vd, "still")
        row = layout.row()
        row.active = vd.still
        row.prop(vd, "still_frame_number")

        layout.prop(vd, "interpolation")
        layout.prop(vd, "extension")
        layout.prop(vd, "intensity")


class TEXTURE_PT_pointdensity(TextureButtonsPanel):
    bl_label = "Point Density"

    def poll(self, context):
        tex = context.texture
        return (tex and tex.type == 'POINT_DENSITY')

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        pd = tex.pointdensity
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(pd, "point_source", expand=True)
        else:
            layout.prop(pd, "point_source", text="")

        split = layout.split()

        col = split.column()
        if pd.point_source == 'PARTICLE_SYSTEM':
            col.label(text="Object:")
            col.prop(pd, "object", text="")

            sub = col.column()
            sub.enabled = bool(pd.object)
            if pd.object:
                sub.label(text="System:")
                sub.prop_object(pd, "particle_system", pd.object, "particle_systems", text="")
            sub.label(text="Cache:")
            sub.prop(pd, "particle_cache", text="")
        else:
            col.label(text="Object:")
            col.prop(pd, "object", text="")
            col.label(text="Cache:")
            col.prop(pd, "vertices_cache", text="")

        col.separator()

        col.label(text="Color Source:")
        col.prop(pd, "color_source", text="")
        if pd.color_source in ('PARTICLE_SPEED', 'PARTICLE_VELOCITY'):
            col.prop(pd, "speed_scale")
        if pd.color_source in ('PARTICLE_SPEED', 'PARTICLE_AGE'):
            layout.template_color_ramp(pd, "color_ramp", expand=True)

        if wide_ui:
            col = split.column()
        col.label()
        col.prop(pd, "radius")
        col.label(text="Falloff:")
        col.prop(pd, "falloff", text="")
        if pd.falloff == 'SOFT':
            col.prop(pd, "falloff_softness")


class TEXTURE_PT_pointdensity_turbulence(TextureButtonsPanel):
    bl_label = "Turbulence"

    def poll(self, context):
        tex = context.texture
        return (tex and tex.type == 'POINT_DENSITY')

    def draw_header(self, context):
        layout = self.layout

        tex = context.texture
        pd = tex.pointdensity

        layout.prop(pd, "turbulence", text="")

    def draw(self, context):
        layout = self.layout

        tex = context.texture
        pd = tex.pointdensity
        layout.active = pd.turbulence
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Influence:")
        col.prop(pd, "turbulence_influence", text="")
        col.label(text="Noise Basis:")
        col.prop(pd, "noise_basis", text="")

        if wide_ui:
            col = split.column()
            col.label()
        col.prop(pd, "turbulence_size")
        col.prop(pd, "turbulence_depth")
        col.prop(pd, "turbulence_strength")

bpy.types.register(TEXTURE_PT_context_texture)
bpy.types.register(TEXTURE_PT_preview)

bpy.types.register(TEXTURE_PT_clouds) # Texture Type Panels
bpy.types.register(TEXTURE_PT_wood)
bpy.types.register(TEXTURE_PT_marble)
bpy.types.register(TEXTURE_PT_magic)
bpy.types.register(TEXTURE_PT_blend)
bpy.types.register(TEXTURE_PT_stucci)
bpy.types.register(TEXTURE_PT_image)
bpy.types.register(TEXTURE_PT_image_sampling)
bpy.types.register(TEXTURE_PT_image_mapping)
bpy.types.register(TEXTURE_PT_plugin)
bpy.types.register(TEXTURE_PT_envmap)
bpy.types.register(TEXTURE_PT_musgrave)
bpy.types.register(TEXTURE_PT_voronoi)
bpy.types.register(TEXTURE_PT_distortednoise)
bpy.types.register(TEXTURE_PT_voxeldata)
bpy.types.register(TEXTURE_PT_pointdensity)
bpy.types.register(TEXTURE_PT_pointdensity_turbulence)

bpy.types.register(TEXTURE_PT_colors)
bpy.types.register(TEXTURE_PT_mapping)
bpy.types.register(TEXTURE_PT_influence)

bpy.types.register(TEXTURE_PT_custom_props)
