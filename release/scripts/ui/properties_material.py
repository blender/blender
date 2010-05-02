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
from rna_prop_ui import PropertyPanel

narrowui = 180


def active_node_mat(mat):
    # TODO, 2.4x has a pipeline section, for 2.5 we need to communicate
    # which settings from node-materials are used
    if mat:
        mat_node = mat.active_node_material
        if mat_node:
            return mat_node
        else:
            return mat

    return None


class MATERIAL_MT_sss_presets(bpy.types.Menu):
    bl_label = "SSS Presets"
    preset_subdir = "sss"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class MATERIAL_MT_specials(bpy.types.Menu):
    bl_label = "Material Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.material_slot_copy", icon='COPY_ID')
        layout.operator("material.copy", icon='COPYDOWN')
        layout.operator("material.paste", icon='PASTEDOWN')


class MaterialButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    def poll(self, context):
        mat = context.material
        engine = context.scene.render.engine
        return mat and (engine in self.COMPAT_ENGINES)


class MATERIAL_PT_preview(MaterialButtonsPanel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        self.layout.template_preview(context.material)


class MATERIAL_PT_context_material(MaterialButtonsPanel):
    bl_label = ""
    bl_show_header = False
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        # An exception, dont call the parent poll func because
        # this manages materials for all engine types

        engine = context.scene.render.engine
        return (context.material or context.object) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = context.material
        ob = context.object
        slot = context.material_slot
        space = context.space_data
        wide_ui = context.region.width > narrowui

        if ob:
            row = layout.row()

            row.template_list(ob, "material_slots", ob, "active_material_index", rows=2)

            col = row.column(align=True)
            col.operator("object.material_slot_add", icon='ZOOMIN', text="")
            col.operator("object.material_slot_remove", icon='ZOOMOUT', text="")

            col.menu("MATERIAL_MT_specials", icon='DOWNARROW_HLT', text="")

            if ob.mode == 'EDIT':
                row = layout.row(align=True)
                row.operator("object.material_slot_assign", text="Assign")
                row.operator("object.material_slot_select", text="Select")
                row.operator("object.material_slot_deselect", text="Deselect")

        if wide_ui:
            split = layout.split(percentage=0.65)

            if ob:
                split.template_ID(ob, "active_material", new="material.new")
                row = split.row()
                if slot:
                    row.prop(slot, "link", text="")
                else:
                    row.label()
            elif mat:
                split.template_ID(space, "pin_id")
                split.separator()
        else:
            if ob:
                layout.template_ID(ob, "active_material", new="material.new")
            elif mat:
                layout.template_ID(space, "pin_id")

        if mat:
            if wide_ui:
                layout.prop(mat, "type", expand=True)
            else:
                layout.prop(mat, "type", text="")


class MATERIAL_PT_custom_props(MaterialButtonsPanel, PropertyPanel):
    COMPAT_ENGINES = {'BLENDER_RENDER'}
    _context_path = "material"


class MATERIAL_PT_shading(MaterialButtonsPanel):
    bl_label = "Shading"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE', 'HALO')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        wide_ui = context.region.width > narrowui

        if mat.type in ('SURFACE', 'WIRE'):
            split = layout.split()

            col = split.column()
            sub = col.column()
            sub.active = not mat.shadeless
            sub.prop(mat, "emit")
            sub.prop(mat, "ambient")
            sub = col.column()
            sub.prop(mat, "translucency")

            if wide_ui:
                col = split.column()
            col.prop(mat, "shadeless")
            sub = col.column()
            sub.active = not mat.shadeless
            sub.prop(mat, "tangent_shading")
            sub.prop(mat, "cubic")

        elif mat.type == 'HALO':
            layout.prop(mat, "alpha")


class MATERIAL_PT_strand(MaterialButtonsPanel):
    bl_label = "Strand"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        mat = context.material
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE', 'HALO')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = context.material # dont use node material
        tan = mat.strand
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        sub = col.column(align=True)
        sub.label(text="Size:")
        sub.prop(tan, "root_size", text="Root")
        sub.prop(tan, "tip_size", text="Tip")
        sub.prop(tan, "min_size", text="Minimum")
        sub.prop(tan, "blender_units")
        sub = col.column()
        sub.active = (not mat.shadeless)
        sub.prop(tan, "tangent_shading")
        col.prop(tan, "shape")

        if wide_ui:
            col = split.column()
        col.label(text="Shading:")
        col.prop(tan, "width_fade")
        ob = context.object
        if ob and ob.type == 'MESH':
            col.prop_object(tan, "uv_layer", ob.data, "uv_textures", text="")
        else:
            col.prop(tan, "uv_layer", text="")
        col.separator()
        sub = col.column()
        sub.active = (not mat.shadeless)
        sub.prop(tan, "surface_diffuse")
        sub = col.column()
        sub.active = tan.surface_diffuse
        sub.prop(tan, "blend_distance", text="Distance")


class MATERIAL_PT_physics(MaterialButtonsPanel):
    bl_label = "Physics"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        phys = context.material.physics # dont use node material
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(phys, "distance")
        col.prop(phys, "friction")
        col.prop(phys, "align_to_normal")

        if wide_ui:
            col = split.column()
        col.prop(phys, "force", slider=True)
        col.prop(phys, "elasticity", slider=True)
        col.prop(phys, "damp", slider=True)


class MATERIAL_PT_options(MaterialButtonsPanel):
    bl_label = "Options"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE', 'HALO')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(mat, "traceable")
        col.prop(mat, "full_oversampling")
        col.prop(mat, "use_sky")
        col.prop(mat, "exclude_mist")
        col.prop(mat, "invert_z")
        sub = col.row()
        sub.prop(mat, "z_offset")
        sub.active = mat.transparency and mat.transparency_method == 'Z_TRANSPARENCY'
        sub = col.column(align=True)
        sub.label(text="Light Group:")
        sub.prop(mat, "light_group", text="")
        row = sub.row()
        row.active = bool(mat.light_group)
        row.prop(mat, "light_group_exclusive", text="Exclusive")

        if wide_ui:
            col = split.column()
        col.prop(mat, "face_texture")
        sub = col.column()
        sub.active = mat.face_texture
        sub.prop(mat, "face_texture_alpha")
        col.separator()
        col.prop(mat, "vertex_color_paint")
        col.prop(mat, "vertex_color_light")
        col.prop(mat, "object_color")


class MATERIAL_PT_shadow(MaterialButtonsPanel):
    bl_label = "Shadow"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(mat, "shadows", text="Receive")
        col.prop(mat, "receive_transparent_shadows", text="Receive Transparent")
        col.prop(mat, "only_shadow", text="Shadows Only")
        col.prop(mat, "cast_shadows_only", text="Cast Only")
        col.prop(mat, "shadow_casting_alpha", text="Casting Alpha")

        if wide_ui:
            col = split.column()
        col.prop(mat, "cast_buffer_shadows")
        sub = col.column()
        sub.active = mat.cast_buffer_shadows
        sub.prop(mat, "shadow_buffer_bias", text="Buffer Bias")
        col.prop(mat, "ray_shadow_bias", text="Auto Ray Bias")
        sub = col.column()
        sub.active = (not mat.ray_shadow_bias)
        sub.prop(mat, "shadow_ray_bias", text="Ray Bias")
        col.prop(mat, "cast_approximate")


class MATERIAL_PT_diffuse(MaterialButtonsPanel):
    bl_label = "Diffuse"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(mat, "diffuse_color", text="")
        sub = col.column()
        sub.active = (not mat.shadeless)
        sub.prop(mat, "diffuse_intensity", text="Intensity")

        if wide_ui:
            col = split.column()
        col.active = (not mat.shadeless)
        col.prop(mat, "diffuse_shader", text="")
        col.prop(mat, "use_diffuse_ramp", text="Ramp")

        col = layout.column()
        col.active = (not mat.shadeless)
        if mat.diffuse_shader == 'OREN_NAYAR':
            col.prop(mat, "roughness")
        elif mat.diffuse_shader == 'MINNAERT':
            col.prop(mat, "darkness")
        elif mat.diffuse_shader == 'TOON':
            split = col.split()

            col = split.column()
            col.prop(mat, "diffuse_toon_size", text="Size")

            if wide_ui:
                col = split.column()
            col.prop(mat, "diffuse_toon_smooth", text="Smooth")
        elif mat.diffuse_shader == 'FRESNEL':
            split = col.split()

            col = split.column()
            col.prop(mat, "diffuse_fresnel", text="Fresnel")

            if wide_ui:
                col = split.column()
            col.prop(mat, "diffuse_fresnel_factor", text="Factor")

        if mat.use_diffuse_ramp:
            layout.separator()
            layout.template_color_ramp(mat, "diffuse_ramp", expand=True)
            layout.separator()

            split = layout.split()

            col = split.column()
            col.prop(mat, "diffuse_ramp_input", text="Input")

            if wide_ui:
                col = split.column()
            col.prop(mat, "diffuse_ramp_blend", text="Blend")
            row = layout.row()
            row.prop(mat, "diffuse_ramp_factor", text="Factor")


class MATERIAL_PT_specular(MaterialButtonsPanel):
    bl_label = "Specular"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE')) and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        wide_ui = context.region.width > narrowui

        layout.active = (not mat.shadeless)

        split = layout.split()

        col = split.column()
        col.prop(mat, "specular_color", text="")
        col.prop(mat, "specular_intensity", text="Intensity")

        if wide_ui:
            col = split.column()
        col.prop(mat, "specular_shader", text="")
        col.prop(mat, "use_specular_ramp", text="Ramp")

        col = layout.column()
        if mat.specular_shader in ('COOKTORR', 'PHONG'):
            col.prop(mat, "specular_hardness", text="Hardness")
        elif mat.specular_shader == 'BLINN':
            split = layout.split()

            col = split.column()
            col.prop(mat, "specular_hardness", text="Hardness")

            if wide_ui:
                col = split.column()
            col.prop(mat, "specular_ior", text="IOR")
        elif mat.specular_shader == 'WARDISO':
            col.prop(mat, "specular_slope", text="Slope")
        elif mat.specular_shader == 'TOON':
            split = layout.split()

            col = split.column()
            col.prop(mat, "specular_toon_size", text="Size")

            if wide_ui:
                col = split.column()
            col.prop(mat, "specular_toon_smooth", text="Smooth")

        if mat.use_specular_ramp:
            layout.separator()
            layout.template_color_ramp(mat, "specular_ramp", expand=True)
            layout.separator()
            split = layout.split()

            col = split.column()
            col.prop(mat, "specular_ramp_input", text="Input")

            if wide_ui:
                col = split.column()
            col.prop(mat, "specular_ramp_blend", text="Blend")

            row = layout.row()
            row.prop(mat, "specular_ramp_factor", text="Factor")


class MATERIAL_PT_sss(MaterialButtonsPanel):
    bl_label = "Subsurface Scattering"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE')) and (engine in self.COMPAT_ENGINES)

    def draw_header(self, context):
        mat = active_node_mat(context.material)
        sss = mat.subsurface_scattering

        self.layout.active = (not mat.shadeless)
        self.layout.prop(sss, "enabled", text="")

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        sss = mat.subsurface_scattering
        wide_ui = context.region.width > narrowui

        layout.active = (sss.enabled) and (not mat.shadeless)

        row = layout.row().split()
        sub = row.row(align=True).split(percentage=0.75)
        sub.menu("MATERIAL_MT_sss_presets", text=bpy.types.MATERIAL_MT_sss_presets.bl_label)
        sub.operator("material.sss_preset_add", text="", icon="ZOOMIN")

        split = layout.split()

        col = split.column()
        col.prop(sss, "ior")
        col.prop(sss, "scale")
        col.prop(sss, "color", text="")
        col.prop(sss, "radius", text="RGB Radius", expand=True)

        if wide_ui:
            col = split.column()
        sub = col.column(align=True)
        sub.label(text="Blend:")
        sub.prop(sss, "color_factor", text="Color")
        sub.prop(sss, "texture_factor", text="Texture")
        sub.label(text="Scattering Weight:")
        sub.prop(sss, "front")
        sub.prop(sss, "back")
        col.separator()
        col.prop(sss, "error_tolerance", text="Error")


class MATERIAL_PT_mirror(MaterialButtonsPanel):
    bl_label = "Mirror"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE')) and (engine in self.COMPAT_ENGINES)

    def draw_header(self, context):
        raym = active_node_mat(context.material).raytrace_mirror

        self.layout.prop(raym, "enabled", text="")

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        raym = mat.raytrace_mirror
        wide_ui = context.region.width > narrowui

        layout.active = raym.enabled

        split = layout.split()

        col = split.column()
        col.prop(raym, "reflect_factor")
        col.prop(mat, "mirror_color", text="")

        if wide_ui:
            col = split.column()
        col.prop(raym, "fresnel")
        sub = col.column()
        sub.active = raym.fresnel > 0
        sub.prop(raym, "fresnel_factor", text="Blend")

        split = layout.split()

        col = split.column()
        col.separator()
        col.prop(raym, "depth")
        col.prop(raym, "distance", text="Max Dist")
        col.separator()
        sub = col.split(percentage=0.4)
        sub.active = raym.distance > 0.0
        sub.label(text="Fade To:")
        sub.prop(raym, "fade_to", text="")

        if wide_ui:
            col = split.column()
        col.label(text="Gloss:")
        col.prop(raym, "gloss_factor", text="Amount")
        sub = col.column()
        sub.active = raym.gloss_factor < 1.0
        sub.prop(raym, "gloss_threshold", text="Threshold")
        sub.prop(raym, "gloss_samples", text="Samples")
        sub.prop(raym, "gloss_anisotropic", text="Anisotropic")


class MATERIAL_PT_transp(MaterialButtonsPanel):
    bl_label = "Transparency"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat and (mat.type in ('SURFACE', 'WIRE')) and (engine in self.COMPAT_ENGINES)

    def draw_header(self, context):
        mat = active_node_mat(context.material)

        self.layout.prop(mat, "transparency", text="")

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        rayt = mat.raytrace_transparency
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.active = mat.transparency and (not mat.shadeless)
        if wide_ui:
            row.prop(mat, "transparency_method", expand=True)
        else:
            row.prop(mat, "transparency_method", text="")

        split = layout.split()

        col = split.column()
        col.prop(mat, "alpha")
        row = col.row()
        row.active = mat.transparency and (not mat.shadeless)
        row.prop(mat, "specular_alpha", text="Specular")

        if wide_ui:
            col = split.column()
        col.active = (not mat.shadeless)
        col.prop(rayt, "fresnel")
        sub = col.column()
        sub.active = rayt.fresnel > 0
        sub.prop(rayt, "fresnel_factor", text="Blend")

        if mat.transparency_method == 'RAYTRACE':
            layout.separator()
            split = layout.split()
            split.active = mat.transparency

            col = split.column()
            col.prop(rayt, "ior")
            col.prop(rayt, "filter")
            col.prop(rayt, "falloff")
            col.prop(rayt, "limit")
            col.prop(rayt, "depth")

            if wide_ui:
                col = split.column()
            col.label(text="Gloss:")
            col.prop(rayt, "gloss_factor", text="Amount")
            sub = col.column()
            sub.active = rayt.gloss_factor < 1.0
            sub.prop(rayt, "gloss_threshold", text="Threshold")
            sub.prop(rayt, "gloss_samples", text="Samples")


class MATERIAL_PT_transp_game(MaterialButtonsPanel):
    bl_label = "Transparency"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_GAME'}

    def poll(self, context):
        mat = active_node_mat(context.material)
        engine = context.scene.render.engine
        return mat  and (engine in self.COMPAT_ENGINES)

    def draw_header(self, context):
        mat = active_node_mat(context.material)

        self.layout.prop(mat, "transparency", text="")

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        rayt = mat.raytrace_transparency
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.active = mat.transparency and (not mat.shadeless)
        if wide_ui:
            row.prop(mat, "transparency_method", expand=True)
        else:
            row.prop(mat, "transparency_method", text="")

        split = layout.split()

        col = split.column()
        col.prop(mat, "alpha")


class MATERIAL_PT_halo(MaterialButtonsPanel):
    bl_label = "Halo"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        mat = context.material
        engine = context.scene.render.engine
        return mat and (mat.type == 'HALO') and (engine in self.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        mat = context.material # dont use node material
        halo = mat.halo
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(mat, "diffuse_color", text="")
        col.prop(halo, "size")
        col.prop(halo, "hardness")
        col.prop(halo, "add")
        col.label(text="Options:")
        col.prop(halo, "texture")
        col.prop(halo, "vertex_normal")
        col.prop(halo, "xalpha")
        col.prop(halo, "shaded")
        col.prop(halo, "soft")

        if wide_ui:
            col = split.column()
        col.prop(halo, "ring")
        sub = col.column()
        sub.active = halo.ring
        sub.prop(halo, "rings")
        sub.prop(mat, "mirror_color", text="")
        col.separator()
        col.prop(halo, "lines")
        sub = col.column()
        sub.active = halo.lines
        sub.prop(halo, "line_number", text="Lines")
        sub.prop(mat, "specular_color", text="")
        col.separator()
        col.prop(halo, "star")
        sub = col.column()
        sub.active = halo.star
        sub.prop(halo, "star_tips")


class MATERIAL_PT_flare(MaterialButtonsPanel):
    bl_label = "Flare"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        mat = context.material
        engine = context.scene.render.engine
        return mat and (mat.type == 'HALO') and (engine in self.COMPAT_ENGINES)

    def draw_header(self, context):
        halo = context.material.halo

        self.layout.prop(halo, "flare_mode", text="")

    def draw(self, context):
        layout = self.layout

        mat = context.material # dont use node material
        halo = mat.halo
        wide_ui = context.region.width > narrowui

        layout.active = halo.flare_mode

        split = layout.split()

        col = split.column()
        col.prop(halo, "flare_size", text="Size")
        col.prop(halo, "flare_boost", text="Boost")
        col.prop(halo, "flare_seed", text="Seed")
        if wide_ui:
            col = split.column()
        col.prop(halo, "flares_sub", text="Subflares")
        col.prop(halo, "flare_subsize", text="Subsize")


class VolumeButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"

    def poll(self, context):
        mat = context.material
        engine = context.scene.render.engine
        return mat and (mat.type == 'VOLUME') and (engine in self.COMPAT_ENGINES)


class MATERIAL_PT_volume_density(VolumeButtonsPanel):
    bl_label = "Density"
    bl_default_closed = False
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        vol = context.material.volume # dont use node material
        wide_ui = context.region.width > narrowui

        split = layout.split()
        col = split.column()
        col.prop(vol, "density")

        if wide_ui:
            col = split.column()
        col.prop(vol, "density_scale")


class MATERIAL_PT_volume_shading(VolumeButtonsPanel):
    bl_label = "Shading"
    bl_default_closed = False
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        vol = context.material.volume # dont use node material
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(vol, "scattering")
        col.prop(vol, "asymmetry")
        col.prop(vol, "transmission_color")

        if wide_ui:
            col = split.column()
        sub = col.column(align=True)
        sub.prop(vol, "emission")
        sub.prop(vol, "emission_color", text="")
        sub = col.column(align=True)
        sub.prop(vol, "reflection")
        sub.prop(vol, "reflection_color", text="")


class MATERIAL_PT_volume_lighting(VolumeButtonsPanel):
    bl_label = "Lighting"
    bl_default_closed = False
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        vol = context.material.volume # dont use node material
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(vol, "lighting_mode", text="")

        if wide_ui:
            col = split.column()

        if vol.lighting_mode == 'SHADED':
            col.prop(vol, "external_shadows")
            col.prop(vol, "light_cache")
            sub = col.column()
            sub.active = vol.light_cache
            sub.prop(vol, "cache_resolution")
        elif vol.lighting_mode in ('MULTIPLE_SCATTERING', 'SHADED_PLUS_MULTIPLE_SCATTERING'):
            sub = col.column()
            sub.enabled = True
            sub.active = False
            sub.prop(vol, "light_cache")
            col.prop(vol, "cache_resolution")

            sub = col.column(align=True)
            sub.prop(vol, "ms_diffusion")
            sub.prop(vol, "ms_spread")
            sub.prop(vol, "ms_intensity")


class MATERIAL_PT_volume_transp(VolumeButtonsPanel):
    bl_label = "Transparency"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        mat = context.material # dont use node material
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.prop(mat, "transparency_method", expand=True)
        else:
            layout.prop(mat, "transparency_method", text="")


class MATERIAL_PT_volume_integration(VolumeButtonsPanel):
    bl_label = "Integration"
    bl_default_closed = False
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        vol = context.material.volume # dont use node material
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Step Calculation:")
        col.prop(vol, "step_calculation", text="")
        col = col.column(align=True)
        col.prop(vol, "step_size")

        if wide_ui:
            col = split.column()
        col.label()
        col.prop(vol, "depth_cutoff")


class MATERIAL_PT_volume_options(VolumeButtonsPanel):
    bl_label = "Options"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        mat = active_node_mat(context.material)
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(mat, "traceable")
        col.prop(mat, "full_oversampling")
        col.prop(mat, "exclude_mist")

        col = split.column()
        col.label(text="Light Group:")
        col.prop(mat, "light_group", text="")
        row = col.row()
        row.active = bool(mat.light_group)
        row.prop(mat, "light_group_exclusive", text="Exclusive")


classes = [
    MATERIAL_PT_context_material,
    MATERIAL_PT_preview,
    MATERIAL_PT_diffuse,
    MATERIAL_PT_specular,
    MATERIAL_PT_shading,
    MATERIAL_PT_transp,
    MATERIAL_PT_mirror,
    MATERIAL_PT_sss,
    MATERIAL_PT_halo,
    MATERIAL_PT_flare,
    MATERIAL_PT_physics,
    MATERIAL_PT_strand,
    MATERIAL_PT_options,
    MATERIAL_PT_shadow,
    MATERIAL_PT_transp_game,

    MATERIAL_MT_sss_presets,
    MATERIAL_MT_specials,

    MATERIAL_PT_volume_density,
    MATERIAL_PT_volume_shading,
    MATERIAL_PT_volume_lighting,
    MATERIAL_PT_volume_transp,
    MATERIAL_PT_volume_integration,
    MATERIAL_PT_volume_options,

    MATERIAL_PT_custom_props]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
