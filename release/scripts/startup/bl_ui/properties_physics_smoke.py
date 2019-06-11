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
from bpy.types import (
    Panel,
)
from bl_ui.properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
)


class PhysicButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @staticmethod
    def poll_smoke(context):
        ob = context.object
        if not ((ob and ob.type == 'MESH') and (context.smoke)):
            return False

        md = context.smoke
        return md and (context.smoke.smoke_type != 'NONE') and (bpy.app.build_options.mod_smoke)

    @staticmethod
    def poll_smoke_domain(context):
        if not PhysicButtonsPanel.poll_smoke(context):
            return False

        md = context.smoke
        return md and (md.smoke_type == 'DOMAIN')


class PHYSICS_PT_smoke(PhysicButtonsPanel, Panel):
    bl_label = "Smoke"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'MESH') and (context.engine in cls.COMPAT_ENGINES) and (context.smoke)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        if not bpy.app.build_options.mod_smoke:
            col = layout.column(align=True)
            col.alignment = 'RIGHT'
            col.label(text="Built without Smoke modifier")
            return

        md = context.smoke

        layout.prop(md, "smoke_type")


class PHYSICS_PT_smoke_settings(PhysicButtonsPanel, Panel):
    bl_label = "Settings"
    bl_parent_id = 'PHYSICS_PT_smoke'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.smoke
        ob = context.object

        if md.smoke_type == 'DOMAIN':
            domain = md.domain_settings

            flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

            col = flow.column()
            col.enabled = (not domain.point_cache.is_baked)
            col.prop(domain, "resolution_max", text="Resolution Divisions")
            col.prop(domain, "time_scale", text="Time Scale")

            col.separator()

            col = flow.column()
            sub = col.row()
            sub.enabled = (not domain.point_cache.is_baked)
            sub.prop(domain, "collision_extents", text="Border Collisions")

            # This can be tweaked after baking, for render.
            col.prop(domain, "clipping", text="Empty Space")

        elif md.smoke_type == 'FLOW':
            flow_smoke = md.flow_settings

            col = layout.column()
            col.prop(flow_smoke, "smoke_flow_type", expand=False)

            col.separator()

            flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)
            col = flow.column()

            if flow_smoke.smoke_flow_type != 'OUTFLOW':
                col.prop(flow_smoke, "smoke_flow_source", expand=False, text="Flow Source")

                if flow_smoke.smoke_flow_source == 'PARTICLES':
                    col.prop_search(
                        flow_smoke, "particle_system", ob, "particle_systems",
                        text="Particle System"
                    )
                else:
                    col.prop(flow_smoke, "surface_distance")
                    col.prop(flow_smoke, "volume_density")

                col = flow.column()
                col.prop(flow_smoke, "use_absolute")

                if flow_smoke.smoke_flow_type in {'SMOKE', 'BOTH'}:
                    col.prop(flow_smoke, "density")
                    col.prop(flow_smoke, "temperature", text="Temperature Diff.")

                    col.separator()

                    col = flow.column()
                    col.prop(flow_smoke, "smoke_color")

                if flow_smoke.smoke_flow_type in {'FIRE', 'BOTH'}:
                    col.prop(flow_smoke, "fuel_amount")

                col.prop(flow_smoke, "subframes", text="Sampling Subframes")

            col.separator()

            col.prop_search(flow_smoke, "density_vertex_group", ob, "vertex_groups", text="Vertex Group")

        elif md.smoke_type == 'COLLISION':
            coll = md.coll_settings

            col = layout.column()
            col.prop(coll, "collision_type")


class PHYSICS_PT_smoke_settings_initial_velocity(PhysicButtonsPanel, Panel):
    bl_label = "Initial Velocity"
    bl_parent_id = 'PHYSICS_PT_smoke_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke(context):
            return False

        md = context.smoke
        return (md and (md.smoke_type == 'FLOW')
                and md.flow_settings and md.flow_settings.smoke_flow_type != 'OUTFLOW'
                and context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.smoke
        flow_smoke = md.flow_settings

        self.layout.prop(flow_smoke, "use_initial_velocity", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

        md = context.smoke
        flow_smoke = md.flow_settings

        flow.active = flow_smoke.use_initial_velocity

        col = flow.column(align=True)
        col.prop(flow_smoke, "velocity_factor")

        if flow_smoke.smoke_flow_source == 'MESH':
            col = flow.column()
            col.prop(flow_smoke, "velocity_normal")
            # sub.prop(flow_smoke, "velocity_random")


class PHYSICS_PT_smoke_settings_particle_size(PhysicButtonsPanel, Panel):
    bl_label = "Particle Size"
    bl_parent_id = 'PHYSICS_PT_smoke_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke(context):
            return False

        md = context.smoke
        return (md and (md.smoke_type == 'FLOW')
                and md.flow_settings and md.flow_settings.smoke_flow_type != 'OUTFLOW'
                and md.flow_settings.smoke_flow_source == 'PARTICLES'
                and context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.smoke
        flow_smoke = md.flow_settings

        self.layout.prop(flow_smoke, "use_particle_size", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.smoke
        flow_smoke = md.flow_settings

        layout.active = flow_smoke.use_particle_size

        layout.prop(flow_smoke, "particle_size")


class PHYSICS_PT_smoke_behavior(PhysicButtonsPanel, Panel):
    bl_label = "Behavior"
    bl_parent_id = 'PHYSICS_PT_smoke_settings'
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.smoke
        domain = md.domain_settings

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = (not domain.point_cache.is_baked)

        col = flow.column()
        col.prop(domain, "alpha")
        col.prop(domain, "beta", text="Temperature Diff.")
        col = flow.column()
        col.prop(domain, "vorticity")


class PHYSICS_PT_smoke_behavior_dissolve(PhysicButtonsPanel, Panel):
    bl_label = "Dissolve"
    bl_parent_id = 'PHYSICS_PT_smoke_behavior'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.smoke
        domain = md.domain_settings

        self.layout.prop(domain, "use_dissolve_smoke", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.smoke
        domain = md.domain_settings

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = (not domain.point_cache.is_baked)

        layout.active = domain.use_dissolve_smoke

        col = flow.column()
        col.prop(domain, "dissolve_speed", text="Time")

        col = flow.column()
        col.prop(domain, "use_dissolve_smoke_log", text="Slow")


class PHYSICS_PT_smoke_flow_texture(PhysicButtonsPanel, Panel):
    bl_label = "Texture"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke(context):
            return False

        md = context.smoke
        return (md and (md.smoke_type == 'FLOW')
                and (md.flow_settings.smoke_flow_source == 'MESH')
                and (context.engine in cls.COMPAT_ENGINES))

    def draw_header(self, context):
        md = context.smoke
        flow_smoke = md.flow_settings

        self.layout.prop(flow_smoke, "use_texture", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        ob = context.object
        flow_smoke = context.smoke.flow_settings

        sub = flow.column()
        sub.active = flow_smoke.use_texture
        sub.prop(flow_smoke, "noise_texture")
        sub.prop(flow_smoke, "texture_map_type", text="Mapping")

        col = flow.column()
        sub = col.column()
        sub.active = flow_smoke.use_texture

        if flow_smoke.texture_map_type == 'UV':
            sub.prop_search(flow_smoke, "uv_layer", ob.data, "uv_layers")

        if flow_smoke.texture_map_type == 'AUTO':
            sub.prop(flow_smoke, "texture_size")

        sub.prop(flow_smoke, "texture_offset")


class PHYSICS_PT_smoke_fire(PhysicButtonsPanel, Panel):
    bl_label = "Flames"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.smoke.domain_settings

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)
        flow.enabled = (not domain.point_cache.is_baked)

        col = flow.column()
        col.prop(domain, "burning_rate", text="Reaction Speed")
        col.prop(domain, "flame_smoke")
        col.prop(domain, "flame_vorticity")

        col.separator()

        col = flow.column(align=True)
        col.prop(domain, "flame_ignition", text="Temperature Ignition")
        col.prop(domain, "flame_max_temp")

        col.separator()

        sub = col.column()
        sub.prop(domain, "flame_smoke_color")


class PHYSICS_PT_smoke_adaptive_domain(PhysicButtonsPanel, Panel):
    bl_label = "Adaptive Domain"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.smoke.domain_settings

        self.layout.prop(md, "use_adaptive_domain", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.smoke.domain_settings
        layout.active = domain.use_adaptive_domain

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)
        flow.enabled = (not domain.point_cache.is_baked)

        col = flow.column()
        col.prop(domain, "additional_res", text="Add Resolution")
        col.prop(domain, "adapt_margin")

        col.separator()

        col = flow.column()
        col.prop(domain, "adapt_threshold", text="Threshold")


class PHYSICS_PT_smoke_highres(PhysicButtonsPanel, Panel):
    bl_label = "High Resolution"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        md = context.smoke.domain_settings

        self.layout.prop(md, "use_high_resolution", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        md = context.smoke.domain_settings
        layout.active = md.use_high_resolution

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        col = flow.column()
        col.enabled = not md.point_cache.is_baked
        col.prop(md, "amplify", text="Resolution Divisions")
        col.prop(md, "highres_sampling", text="Flow Sampling")

        col.separator()

        col = flow.column()
        col.enabled = not md.point_cache.is_baked
        col.prop(md, "noise_type", text="Noise Method")
        col.prop(md, "strength")

        layout.prop(md, "show_high_resolution")


class PHYSICS_PT_smoke_collections(PhysicButtonsPanel, Panel):
    bl_label = "Collections"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.smoke.domain_settings

        col = layout.column()
        col.prop(domain, "fluid_collection", text="Flow")

        # col = layout.column()
        # col.prop(domain, "effector_collection", text="Effector")
        col.prop(domain, "collision_collection", text="Collision")


class PHYSICS_PT_smoke_cache(PhysicButtonsPanel, Panel):
    bl_label = "Cache"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        domain = context.smoke.domain_settings
        cache_file_format = domain.cache_file_format

        col = flow.column()
        col.prop(domain, "cache_file_format")

        if cache_file_format == 'POINTCACHE':
            col = flow.column()
            col.prop(domain, "point_cache_compress_type", text="Compression")
            col.separator()

        elif cache_file_format == 'OPENVDB':
            if not bpy.app.build_options.openvdb:
                row = layout.row(align=True)
                row.alignment = 'RIGHT'
                row.label(text="Built without OpenVDB support")
                return

            col = flow.column()
            col.prop(domain, "openvdb_cache_compress_type", text="Compression")
            col.prop(domain, "data_depth", text="Data Depth")
            col.separator()

        cache = domain.point_cache
        point_cache_ui(self, cache, (cache.is_baked is False), 'SMOKE')


class PHYSICS_PT_smoke_field_weights(PhysicButtonsPanel, Panel):
    bl_label = "Field Weights"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        if not PhysicButtonsPanel.poll_smoke_domain(context):
            return False

        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        domain = context.smoke.domain_settings
        effector_weights_ui(self, domain.effector_weights, 'SMOKE')


class PHYSICS_PT_smoke_viewport_display(PhysicButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_parent_id = 'PHYSICS_PT_smoke'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (PhysicButtonsPanel.poll_smoke_domain(context))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        domain = context.smoke.domain_settings

        col = flow.column()
        col.prop(domain, "display_thickness")

        col.separator()

        col.prop(domain, "slice_method", text="Slicing")

        slice_method = domain.slice_method
        axis_slice_method = domain.axis_slice_method

        do_axis_slicing = (slice_method == 'AXIS_ALIGNED')
        do_full_slicing = (axis_slice_method == 'FULL')

        col = col.column()
        col.enabled = do_axis_slicing
        col.prop(domain, "axis_slice_method")

        col = flow.column()
        sub = col.column()
        sub.enabled = not do_full_slicing and do_axis_slicing
        sub.prop(domain, "slice_axis")
        sub.prop(domain, "slice_depth")

        row = col.row()
        row.enabled = do_full_slicing or not do_axis_slicing
        row.prop(domain, "slice_per_voxel")

        col.prop(domain, "display_interpolation")


class PHYSICS_PT_smoke_viewport_display_color(PhysicButtonsPanel, Panel):
    bl_label = "Color Mapping"
    bl_parent_id = 'PHYSICS_PT_smoke_viewport_display'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (PhysicButtonsPanel.poll_smoke_domain(context))

    def draw_header(self, context):
        md = context.smoke.domain_settings

        self.layout.prop(md, "use_color_ramp", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        domain = context.smoke.domain_settings
        col = layout.column()
        col.enabled = domain.use_color_ramp

        col.prop(domain, "coba_field")

        col.use_property_split = False

        col = col.column()
        col.template_color_ramp(domain, "color_ramp", expand=True)


class PHYSICS_PT_smoke_viewport_display_debug(PhysicButtonsPanel, Panel):
    bl_label = "Debug Velocity"
    bl_parent_id = 'PHYSICS_PT_smoke_viewport_display'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (PhysicButtonsPanel.poll_smoke_domain(context))

    def draw_header(self, context):
        md = context.smoke.domain_settings

        self.layout.prop(md, "show_velocity", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        domain = context.smoke.domain_settings

        col = flow.column()
        col.enabled = domain.show_velocity
        col.prop(domain, "vector_display_type", text="Display As")
        col.prop(domain, "vector_scale")


classes = (
    PHYSICS_PT_smoke,
    PHYSICS_PT_smoke_settings,
    PHYSICS_PT_smoke_settings_initial_velocity,
    PHYSICS_PT_smoke_settings_particle_size,
    PHYSICS_PT_smoke_behavior,
    PHYSICS_PT_smoke_behavior_dissolve,
    PHYSICS_PT_smoke_adaptive_domain,
    PHYSICS_PT_smoke_cache,
    PHYSICS_PT_smoke_field_weights,
    PHYSICS_PT_smoke_fire,
    PHYSICS_PT_smoke_flow_texture,
    PHYSICS_PT_smoke_collections,
    PHYSICS_PT_smoke_highres,
    PHYSICS_PT_smoke_viewport_display,
    PHYSICS_PT_smoke_viewport_display_color,
    PHYSICS_PT_smoke_viewport_display_debug,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
