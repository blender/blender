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
from bpy.types import Panel
from rna_prop_ui import PropertyPanel

from bl_ui.properties_physics_common import (
    point_cache_ui,
    effector_weights_ui,
    basic_force_field_settings_ui,
    basic_force_field_falloff_ui,
    )


def particle_panel_enabled(context, psys):
    if psys is None:
        return True
    phystype = psys.settings.physics_type
    if psys.settings.type in {'EMITTER', 'REACTOR'} and phystype in {'NO', 'KEYED'}:
        return True
    else:
        return (psys.point_cache.is_baked is False) and (not psys.is_edited) and (not context.particle_system_editable)


def particle_panel_poll(cls, context):
    psys = context.particle_system
    engine = context.scene.render.engine
    settings = 0

    if psys:
        settings = psys.settings
    elif isinstance(context.space_data.pin_id, bpy.types.ParticleSettings):
        settings = context.space_data.pin_id

    if not settings:
        return False

    return settings.is_fluid == False and (engine in cls.COMPAT_ENGINES)


def particle_get_settings(context):
    if context.particle_system:
        return context.particle_system.settings
    elif isinstance(context.space_data.pin_id, bpy.types.ParticleSettings):
        return context.space_data.pin_id
    return None


class ParticleButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "particle"

    @classmethod
    def poll(cls, context):
        return particle_panel_poll(cls, context)


class PARTICLE_PT_context_particles(ParticleButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (context.particle_system or context.object or context.space_data.pin_id) and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        if context.scene.render.engine == "BLENDER_GAME":
            layout.label("Not available in the Game Engine")
            return

        ob = context.object
        psys = context.particle_system
        part = 0

        if ob:
            row = layout.row()

            row.template_list(ob, "particle_systems", ob.particle_systems, "active_index", rows=2)

            col = row.column(align=True)
            col.operator("object.particle_system_add", icon='ZOOMIN', text="")
            col.operator("object.particle_system_remove", icon='ZOOMOUT', text="")

        if psys is None:
            part = particle_get_settings(context)

            if part is None:
                return

            layout.template_ID(context.space_data, "pin_id")

            if part.is_fluid:
                layout.label(text="Settings used for fluid")
                return

            layout.prop(part, "type", text="Type")

        elif not psys.settings:
            split = layout.split(percentage=0.32)

            col = split.column()
            col.label(text="Name:")
            col.label(text="Settings:")

            col = split.column()
            col.prop(psys, "name", text="")
            col.template_ID(psys, "settings", new="particle.new")
        else:
            part = psys.settings

            split = layout.split(percentage=0.32)
            col = split.column()
            col.label(text="Name:")
            if part.is_fluid == False:
                col.label(text="Settings:")
                col.label(text="Type:")

            col = split.column()
            col.prop(psys, "name", text="")
            if part.is_fluid == False:
                row = col.row()
                row.enabled = particle_panel_enabled(context, psys)
                row.template_ID(psys, "settings", new="particle.new")

            #row = layout.row()
            #row.label(text="Viewport")
            #row.label(text="Render")

            if part.is_fluid:
                layout.label(text="{} fluid particles for this frame".format(str(part.count)))
                return

            row = col.row()
            row.enabled = particle_panel_enabled(context, psys)
            row.prop(part, "type", text="")
            row.prop(psys, "seed")

        if part:
            split = layout.split(percentage=0.65)
            if part.type == 'HAIR':
                if psys is not None and psys.is_edited:
                    split.operator("particle.edited_clear", text="Free Edit")
                else:
                    row = split.row()
                    row.enabled = particle_panel_enabled(context, psys)
                    row.prop(part, "regrow_hair")
                    row.prop(part, "use_advanced_hair")
                row = split.row()
                row.enabled = particle_panel_enabled(context, psys)
                row.prop(part, "hair_step")
                if psys is not None and psys.is_edited:
                    if psys.is_global_hair:
                        layout.operator("particle.connect_hair")
                    else:
                        layout.operator("particle.disconnect_hair")
            elif psys is not None and part.type == 'REACTOR':
                split.enabled = particle_panel_enabled(context, psys)
                split.prop(psys, "reactor_target_object")
                split.prop(psys, "reactor_target_particle_system", text="Particle System")


class PARTICLE_PT_emission(ParticleButtonsPanel, Panel):
    bl_label = "Emission"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        psys = context.particle_system
        settings = particle_get_settings(context)

        if settings is None:
            return False
        if settings.is_fluid:
            return False
        if particle_panel_poll(PARTICLE_PT_emission, context):
            return psys is None or not context.particle_system.point_cache.use_external
        return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = particle_get_settings(context)

        layout.enabled = particle_panel_enabled(context, psys) and (psys is None or not psys.has_multiple_caches)

        row = layout.row()
        row.active = part.distribution != 'GRID'
        row.prop(part, "count")

        if part.type == 'HAIR' and not part.use_advanced_hair:
            row.prop(part, "hair_length")
            return

        if part.type != 'HAIR':
            split = layout.split()

            col = split.column(align=True)
            col.prop(part, "frame_start")
            col.prop(part, "frame_end")

            col = split.column(align=True)
            col.prop(part, "lifetime")
            col.prop(part, "lifetime_random", slider=True)

        layout.label(text="Emit From:")
        layout.prop(part, "emit_from", expand=True)

        row = layout.row()
        if part.emit_from == 'VERT':
            row.prop(part, "use_emit_random")
        elif part.distribution == 'GRID':
            row.prop(part, "invert_grid")
            row.prop(part, "hexagonal_grid")
        else:
            row.prop(part, "use_emit_random")
            row.prop(part, "use_even_distribution")

        if part.emit_from == 'FACE' or part.emit_from == 'VOLUME':
            layout.prop(part, "distribution", expand=True)

            row = layout.row()
            if part.distribution == 'JIT':
                row.prop(part, "userjit", text="Particles/Face")
                row.prop(part, "jitter_factor", text="Jittering Amount", slider=True)
            elif part.distribution == 'GRID':
                row.prop(part, "grid_resolution")
                row.prop(part, "grid_random", text="Random", slider=True)


class PARTICLE_PT_hair_dynamics(ParticleButtonsPanel, Panel):
    bl_label = "Hair dynamics"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        psys = context.particle_system
        engine = context.scene.render.engine
        if psys is None:
            return False
        if psys.settings is None:
            return False
        return psys.settings.type == 'HAIR' and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        psys = context.particle_system
        self.layout.prop(psys, "use_hair_dynamics", text="")

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system

        if not psys.cloth:
            return

        cloth = psys.cloth.settings

        layout.enabled = psys.use_hair_dynamics and psys.point_cache.is_baked == False

        split = layout.split()

        col = split.column()
        col.label(text="Material:")
        sub = col.column(align=True)
        sub.prop(cloth, "pin_stiffness", text="Stiffness")
        sub.prop(cloth, "mass")
        sub.prop(cloth, "bending_stiffness", text="Bending")
        sub.prop(cloth, "internal_friction", slider=True)
        sub.prop(cloth, "collider_friction", slider=True)

        col = split.column()
        col.label(text="Damping:")
        sub = col.column(align=True)
        sub.prop(cloth, "spring_damping", text="Spring")
        sub.prop(cloth, "air_damping", text="Air")

        col.label(text="Quality:")
        col.prop(cloth, "quality", text="Steps", slider=True)


class PARTICLE_PT_cache(ParticleButtonsPanel, Panel):
    bl_label = "Cache"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        psys = context.particle_system
        engine = context.scene.render.engine
        if psys is None:
            return False
        if psys.settings is None:
            return False
        if psys.settings.is_fluid:
            return False
        phystype = psys.settings.physics_type
        if phystype == 'NO' or phystype == 'KEYED':
            return False
        return (psys.settings.type in {'EMITTER', 'REACTOR'} or (psys.settings.type == 'HAIR' and (psys.use_hair_dynamics or psys.point_cache.is_baked))) and engine in cls.COMPAT_ENGINES

    def draw(self, context):
        psys = context.particle_system

        point_cache_ui(self, context, psys.point_cache, True, 'HAIR' if (psys.settings.type == 'HAIR') else 'PSYS')


class PARTICLE_PT_velocity(ParticleButtonsPanel, Panel):
    bl_label = "Velocity"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        if particle_panel_poll(PARTICLE_PT_velocity, context):
            psys = context.particle_system
            settings = particle_get_settings(context)

            if settings.type == 'HAIR' and not settings.use_advanced_hair:
                return False
            return settings.physics_type != 'BOIDS' and (psys is None or not psys.point_cache.use_external)
        else:
            return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = particle_get_settings(context)

        layout.enabled = particle_panel_enabled(context, psys)

        split = layout.split()

        col = split.column()
        col.label(text="Emitter Geometry:")
        col.prop(part, "normal_factor")
        sub = col.column(align=True)
        sub.prop(part, "tangent_factor")
        sub.prop(part, "tangent_phase", slider=True)

        col = split.column()
        col.label(text="Emitter Object:")
        col.prop(part, "object_align_factor", text="")

        layout.label(text="Other:")
        row = layout.row()
        if part.emit_from == 'PARTICLE':
            row.prop(part, "particle_factor")
        else:
            row.prop(part, "object_factor", slider=True)
        row.prop(part, "factor_random")

        #if part.type=='REACTOR':
        #    sub.prop(part, "reactor_factor")
        #    sub.prop(part, "reaction_shape", slider=True)


class PARTICLE_PT_rotation(ParticleButtonsPanel, Panel):
    bl_label = "Rotation"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        if particle_panel_poll(PARTICLE_PT_rotation, context):
            psys = context.particle_system
            settings = particle_get_settings(context)

            if settings.type == 'HAIR' and not settings.use_advanced_hair:
                return False
            return settings.physics_type != 'BOIDS' and (psys is None or not psys.point_cache.use_external)
        else:
            return False

    def draw_header(self, context):
        psys = context.particle_system
        if psys:
            part = psys.settings
        else:
            part = context.space_data.pin_id

        self.layout.prop(part, "use_rotations", text="")

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        if psys:
            part = psys.settings
        else:
            part = context.space_data.pin_id

        layout.enabled = particle_panel_enabled(context, psys) and part.use_rotations

        layout.label(text="Initial Orientation:")

        split = layout.split()

        col = split.column(align=True)
        col.prop(part, "rotation_mode", text="")
        col.prop(part, "rotation_factor_random", slider=True, text="Random")

        col = split.column(align=True)
        col.prop(part, "phase_factor", slider=True)
        col.prop(part, "phase_factor_random", text="Random", slider=True)

        if part.type != 'HAIR':
            layout.label(text="Angular Velocity:")

            split = layout.split()
            col = split.column(align=True)
            col.prop(part, "angular_velocity_mode", text="")
            sub = col.column()
            sub.active = part.angular_velocity_mode != 'NONE'
            sub.prop(part, "angular_velocity_factor", text="")

            col = split.column()
            col.prop(part, "use_dynamic_rotation")


class PARTICLE_PT_physics(ParticleButtonsPanel, Panel):
    bl_label = "Physics"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        if particle_panel_poll(PARTICLE_PT_physics, context):
            psys = context.particle_system
            settings = particle_get_settings(context)

            if settings.type == 'HAIR' and not settings.use_advanced_hair:
                return False
            return psys is None or not psys.point_cache.use_external
        else:
            return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = particle_get_settings(context)

        layout.enabled = particle_panel_enabled(context, psys)

        layout.prop(part, "physics_type", expand=True)

        row = layout.row()
        col = row.column(align=True)
        col.prop(part, "particle_size")
        col.prop(part, "size_random", slider=True)

        if part.physics_type != 'NO':
            col = row.column(align=True)
            col.prop(part, "mass")
            col.prop(part, "use_multiply_size_mass", text="Multiply mass with size")

        if part.physics_type in {'NEWTON', 'FLUID'}:
            split = layout.split()

            col = split.column()
            col.label(text="Forces:")
            col.prop(part, "brownian_factor")
            col.prop(part, "drag_factor", slider=True)
            col.prop(part, "damping", slider=True)

            col = split.column()
            col.label(text="Integration:")
            col.prop(part, "integrator", text="")
            col.prop(part, "timestep")
            sub = col.row()
            if part.adaptive_subframes:
                sub.prop(part, "courant_target", text="Threshold")
            else:
                sub.prop(part, "subframes")
            sub.prop(part, "adaptive_subframes", text="")

            row = layout.row()
            row.prop(part, "use_size_deflect")
            row.prop(part, "use_die_on_collision")

            if part.physics_type == 'FLUID':
                fluid = part.fluid

                split = layout.split()

                col = split.column()
                col.label(text="Fluid properties:")
                col.prop(fluid, "stiffness", text="Stiffness")
                col.prop(fluid, "linear_viscosity", text="Viscosity")
                col.prop(fluid, "buoyancy", text="Buoyancy", slider=True)

                col = split.column()
                col.label(text="Advanced:")

                sub = col.row()
                sub.prop(fluid, "repulsion", slider=fluid.factor_repulsion)
                sub.prop(fluid, "factor_repulsion", text="")

                sub = col.row()
                sub.prop(fluid, "stiff_viscosity", slider=fluid.factor_stiff_viscosity)
                sub.prop(fluid, "factor_stiff_viscosity", text="")

                sub = col.row()
                sub.prop(fluid, "fluid_radius", slider=fluid.factor_radius)
                sub.prop(fluid, "factor_radius", text="")

                sub = col.row()
                sub.prop(fluid, "rest_density", slider=fluid.factor_density)
                sub.prop(fluid, "factor_density", text="")

                split = layout.split()

                col = split.column()
                col.label(text="Springs:")
                col.prop(fluid, "spring_force", text="Force")
                col.prop(fluid, "use_viscoelastic_springs")
                sub = col.column(align=True)
                sub.active = fluid.use_viscoelastic_springs
                sub.prop(fluid, "yield_ratio", slider=True)
                sub.prop(fluid, "plasticity", slider=True)

                col = split.column()
                col.label(text="Advanced:")
                sub = col.row()
                sub.prop(fluid, "rest_length", slider=fluid.factor_rest_length)
                sub.prop(fluid, "factor_rest_length", text="")
                col.label(text="")
                sub = col.column()
                sub.active = fluid.use_viscoelastic_springs
                sub.prop(fluid, "use_initial_rest_length")
                sub.prop(fluid, "spring_frames", text="Frames")

        elif part.physics_type == 'KEYED':
            split = layout.split()
            sub = split.column()

            row = layout.row()
            col = row.column()
            col.active = not psys.use_keyed_timing
            col.prop(part, "keyed_loops", text="Loops")
            if psys:
                row.prop(psys, "use_keyed_timing", text="Use Timing")

            layout.label(text="Keys:")
        elif part.physics_type == 'BOIDS':
            boids = part.boids

            row = layout.row()
            row.prop(boids, "use_flight")
            row.prop(boids, "use_land")
            row.prop(boids, "use_climb")

            split = layout.split()

            sub = split.column()
            col = sub.column(align=True)
            col.active = boids.use_flight
            col.prop(boids, "air_speed_max")
            col.prop(boids, "air_speed_min", slider=True)
            col.prop(boids, "air_acc_max", slider=True)
            col.prop(boids, "air_ave_max", slider=True)
            col.prop(boids, "air_personal_space")
            row = col.row()
            row.active = (boids.use_land or boids.use_climb) and boids.use_flight
            row.prop(boids, "land_smooth")

            sub = split.column()
            col = sub.column(align=True)
            col.active = boids.use_land or boids.use_climb
            col.prop(boids, "land_speed_max")
            col.prop(boids, "land_jump_speed")
            col.prop(boids, "land_acc_max", slider=True)
            col.prop(boids, "land_ave_max", slider=True)
            col.prop(boids, "land_personal_space")
            col.prop(boids, "land_stick_force")

            row = layout.row()

            col = row.column(align=True)
            col.label(text="Battle:")
            col.prop(boids, "health")
            col.prop(boids, "strength")
            col.prop(boids, "aggression")
            col.prop(boids, "accuracy")
            col.prop(boids, "range")

            col = row.column()
            col.label(text="Misc:")
            col.prop(boids, "bank", slider=True)
            col.prop(boids, "pitch", slider=True)
            col.prop(boids, "height", slider=True)

        if psys and part.physics_type in {'KEYED', 'BOIDS', 'FLUID'}:
            if part.physics_type == 'BOIDS':
                layout.label(text="Relations:")
            elif part.physics_type == 'FLUID':
                layout.label(text="Fluid interaction:")

            row = layout.row()
            row.template_list(psys, "targets", psys, "active_particle_target_index")

            col = row.column()
            sub = col.row()
            subsub = sub.column(align=True)
            subsub.operator("particle.new_target", icon='ZOOMIN', text="")
            subsub.operator("particle.target_remove", icon='ZOOMOUT', text="")
            sub = col.row()
            subsub = sub.column(align=True)
            subsub.operator("particle.target_move_up", icon='MOVE_UP_VEC', text="")
            subsub.operator("particle.target_move_down", icon='MOVE_DOWN_VEC', text="")

            key = psys.active_particle_target
            if key:
                row = layout.row()
                if part.physics_type == 'KEYED':
                    col = row.column()
                    #doesn't work yet
                    #col.alert = key.valid
                    col.prop(key, "object", text="")
                    col.prop(key, "system", text="System")
                    col = row.column()
                    col.active = psys.use_keyed_timing
                    col.prop(key, "time")
                    col.prop(key, "duration")
                elif part.physics_type == 'BOIDS':
                    sub = row.row()
                    #doesn't work yet
                    #sub.alert = key.valid
                    sub.prop(key, "object", text="")
                    sub.prop(key, "system", text="System")

                    layout.prop(key, "alliance", expand=True)
                elif part.physics_type == 'FLUID':
                    sub = row.row()
                    #doesn't work yet
                    #sub.alert = key.valid
                    sub.prop(key, "object", text="")
                    sub.prop(key, "system", text="System")


class PARTICLE_PT_boidbrain(ParticleButtonsPanel, Panel):
    bl_label = "Boid Brain"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        psys = context.particle_system
        settings = particle_get_settings(context)
        engine = context.scene.render.engine

        if settings is None:
            return False
        if psys is not None and psys.point_cache.use_external:
            return False
        return settings.physics_type == 'BOIDS' and engine in cls.COMPAT_ENGINES

    def draw(self, context):
        layout = self.layout

        boids = particle_get_settings(context).boids

        layout.enabled = particle_panel_enabled(context, context.particle_system)

        # Currently boids can only use the first state so these are commented out for now.
        #row = layout.row()
        #row.template_list(boids, "states", boids, "active_boid_state_index", compact="True")
        #col = row.row()
        #sub = col.row(align=True)
        #sub.operator("boid.state_add", icon='ZOOMIN', text="")
        #sub.operator("boid.state_del", icon='ZOOMOUT', text="")
        #sub = row.row(align=True)
        #sub.operator("boid.state_move_up", icon='MOVE_UP_VEC', text="")
        #sub.operator("boid.state_move_down", icon='MOVE_DOWN_VEC', text="")

        state = boids.active_boid_state

        #layout.prop(state, "name", text="State name")

        row = layout.row()
        row.prop(state, "ruleset_type")
        if state.ruleset_type == 'FUZZY':
            row.prop(state, "rule_fuzzy", slider=True)
        else:
            row.label(text="")

        row = layout.row()
        row.template_list(state, "rules", state, "active_boid_rule_index")

        col = row.column()
        sub = col.row()
        subsub = sub.column(align=True)
        subsub.operator_menu_enum("boid.rule_add", "type", icon='ZOOMIN', text="")
        subsub.operator("boid.rule_del", icon='ZOOMOUT', text="")
        sub = col.row()
        subsub = sub.column(align=True)
        subsub.operator("boid.rule_move_up", icon='MOVE_UP_VEC', text="")
        subsub.operator("boid.rule_move_down", icon='MOVE_DOWN_VEC', text="")

        rule = state.active_boid_rule

        if rule:
            row = layout.row()
            row.prop(rule, "name", text="")
            #somebody make nice icons for boids here please! -jahka
            row.prop(rule, "use_in_air", icon='MOVE_UP_VEC', text="")
            row.prop(rule, "use_on_land", icon='MOVE_DOWN_VEC', text="")

            row = layout.row()

            if rule.type == 'GOAL':
                row.prop(rule, "object")
                row = layout.row()
                row.prop(rule, "use_predict")
            elif rule.type == 'AVOID':
                row.prop(rule, "object")
                row = layout.row()
                row.prop(rule, "use_predict")
                row.prop(rule, "fear_factor")
            elif rule.type == 'FOLLOW_PATH':
                row.label(text="Not yet functional")
            elif rule.type == 'AVOID_COLLISION':
                row.prop(rule, "use_avoid")
                row.prop(rule, "use_avoid_collision")
                row.prop(rule, "look_ahead")
            elif rule.type == 'FOLLOW_LEADER':
                row.prop(rule, "object", text="")
                row.prop(rule, "distance")
                row = layout.row()
                row.prop(rule, "use_line")
                sub = row.row()
                sub.active = rule.line
                sub.prop(rule, "queue_count")
            elif rule.type == 'AVERAGE_SPEED':
                row.prop(rule, "speed", slider=True)
                row.prop(rule, "wander", slider=True)
                row.prop(rule, "level", slider=True)
            elif rule.type == 'FIGHT':
                row.prop(rule, "distance")
                row.prop(rule, "flee_distance")


class PARTICLE_PT_render(ParticleButtonsPanel, Panel):
    bl_label = "Render"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        settings = particle_get_settings(context)
        engine = context.scene.render.engine
        if settings is None:
            return False

        return engine in cls.COMPAT_ENGINES

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = particle_get_settings(context)

        row = layout.row()
        row.prop(part, "material")
        if psys:
            row.prop(psys, "parent")

        split = layout.split()

        col = split.column()
        col.prop(part, "use_render_emitter")
        col.prop(part, "use_parent_particles")

        col = split.column()
        col.prop(part, "show_unborn")
        col.prop(part, "use_dead")

        layout.prop(part, "render_type", expand=True)

        split = layout.split()

        col = split.column()

        if part.render_type == 'LINE':
            col.prop(part, "line_length_tail")
            col.prop(part, "line_length_head")

            split.prop(part, "use_velocity_length")
        elif part.render_type == 'PATH':
            col.prop(part, "use_strand_primitive")
            sub = col.column()
            sub.active = (part.use_strand_primitive is False)
            sub.prop(part, "use_render_adaptive")
            sub = col.column()
            sub.active = part.use_render_adaptive or part.use_strand_primitive == True
            sub.prop(part, "adaptive_angle")
            sub = col.column()
            sub.active = (part.use_render_adaptive is True and part.use_strand_primitive is False)
            sub.prop(part, "adaptive_pixel")
            col.prop(part, "use_hair_bspline")
            col.prop(part, "render_step", text="Steps")

            col = split.column()
            col.label(text="Timing:")
            col.prop(part, "use_absolute_path_time")
            col.prop(part, "path_start", text="Start", slider=not part.use_absolute_path_time)
            col.prop(part, "path_end", text="End", slider=not part.use_absolute_path_time)
            col.prop(part, "length_random", text="Random", slider=True)

            row = layout.row()
            col = row.column()

            if part.type == 'HAIR' and part.use_strand_primitive == True and part.child_type == 'INTERPOLATED':
                layout.prop(part, "use_simplify")
                if part.use_simplify == True:
                    row = layout.row()
                    row.prop(part, "simplify_refsize")
                    row.prop(part, "simplify_rate")
                    row.prop(part, "simplify_transition")
                    row = layout.row()
                    row.prop(part, "use_simplify_viewport")
                    sub = row.row()
                    sub.active = part.use_simplify_viewport == True
                    sub.prop(part, "simplify_viewport")

        elif part.render_type == 'OBJECT':
            col.prop(part, "dupli_object")
            sub = col.row()
            sub.prop(part, "use_global_dupli")
            sub.prop(part, "use_rotation_dupli")
        elif part.render_type == 'GROUP':
            col.prop(part, "dupli_group")
            split = layout.split()

            col = split.column()
            col.prop(part, "use_whole_group")
            sub = col.column()
            sub.active = (part.use_whole_group is False)
            sub.prop(part, "use_group_pick_random")
            sub.prop(part, "use_group_count")

            col = split.column()
            sub = col.column()
            sub.active = (part.use_whole_group is False)
            sub.prop(part, "use_global_dupli")
            sub.prop(part, "use_rotation_dupli")

            if part.use_group_count and not part.use_whole_group:
                row = layout.row()
                row.template_list(part, "dupli_weights", part, "active_dupliweight_index")

                col = row.column()
                sub = col.row()
                subsub = sub.column(align=True)
                subsub.operator("particle.dupliob_copy", icon='ZOOMIN', text="")
                subsub.operator("particle.dupliob_remove", icon='ZOOMOUT', text="")
                subsub.operator("particle.dupliob_move_up", icon='MOVE_UP_VEC', text="")
                subsub.operator("particle.dupliob_move_down", icon='MOVE_DOWN_VEC', text="")

                weight = part.active_dupliweight
                if weight:
                    row = layout.row()
                    row.prop(weight, "count")

        elif part.render_type == 'BILLBOARD':
            ob = context.object

            col.label(text="Align:")

            row = layout.row()
            row.prop(part, "billboard_align", expand=True)
            row.prop(part, "lock_billboard", text="Lock")
            row = layout.row()
            row.prop(part, "billboard_object")

            row = layout.row()
            col = row.column(align=True)
            col.label(text="Tilt:")
            col.prop(part, "billboard_tilt", text="Angle", slider=True)
            col.prop(part, "billboard_tilt_random", text="Random", slider=True)
            col = row.column()
            col.prop(part, "billboard_offset")

            row = layout.row()
            col = row.column()
            col.prop(part, "billboard_size", text="Scale")
            if part.billboard_align == 'VEL':
                col = row.column(align=True)
                col.label("Velocity Scale:")
                col.prop(part, "billboard_velocity_head", text="Head")
                col.prop(part, "billboard_velocity_tail", text="Tail")

            if psys:
                col = layout.column()
                col.prop_search(psys, "billboard_normal_uv", ob.data, "uv_textures")
                col.prop_search(psys, "billboard_time_index_uv", ob.data, "uv_textures")

            split = layout.split(percentage=0.33)
            split.label(text="Split UVs:")
            split.prop(part, "billboard_uv_split", text="Number of splits")

            if psys:
                col = layout.column()
                col.active = part.billboard_uv_split > 1
                col.prop_search(psys, "billboard_split_uv", ob.data, "uv_textures")

            row = col.row()
            row.label(text="Animate:")
            row.prop(part, "billboard_animation", text="")
            row.label(text="Offset:")
            row.prop(part, "billboard_offset_split", text="")

        if part.render_type == 'HALO' or part.render_type == 'LINE' or part.render_type == 'BILLBOARD':
            row = layout.row()
            col = row.column()
            col.prop(part, "trail_count")
            if part.trail_count > 1:
                col.prop(part, "use_absolute_path_time", text="Length in frames")
                col = row.column()
                col.prop(part, "path_end", text="Length", slider=not part.use_absolute_path_time)
                col.prop(part, "length_random", text="Random", slider=True)
            else:
                col = row.column()
                col.label(text="")

        if part.render_type in {'OBJECT', 'GROUP'} and not part.use_advanced_hair:
            row = layout.row(align=True)
            row.prop(part, "particle_size")
            row.prop(part, "size_random", slider=True)


class PARTICLE_PT_draw(ParticleButtonsPanel, Panel):
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        settings = particle_get_settings(context)
        engine = context.scene.render.engine
        if settings is None:
            return False
        return engine in cls.COMPAT_ENGINES

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = particle_get_settings(context)

        row = layout.row()
        row.prop(part, "draw_method", expand=True)

        if part.draw_method == 'NONE' or (part.render_type == 'NONE' and part.draw_method == 'RENDER'):
            return

        path = (part.render_type == 'PATH' and part.draw_method == 'RENDER') or part.draw_method == 'PATH'

        row = layout.row()
        row.prop(part, "draw_percentage", slider=True)
        if part.draw_method != 'RENDER' or part.render_type == 'HALO':
            row.prop(part, "draw_size")
        else:
            row.label(text="")

        if part.draw_percentage != 100 and psys is not None:
            if part.type == 'HAIR':
                if psys.use_hair_dynamics and psys.point_cache.is_baked == False:
                    layout.row().label(text="Display percentage makes dynamics inaccurate without baking!")
            else:
                phystype = part.physics_type
                if phystype != 'NO' and phystype != 'KEYED' and psys.point_cache.is_baked == False:
                    layout.row().label(text="Display percentage makes dynamics inaccurate without baking!")

        row = layout.row()
        col = row.column()
        col.prop(part, "show_size")
        col.prop(part, "show_velocity")
        col.prop(part, "show_number")
        if part.physics_type == 'BOIDS':
            col.prop(part, "show_health")

        col = row.column(align=True)
        col.label(text="Color:")
        col.prop(part, "draw_color", text="")
        sub = col.row()
        sub.active = (part.draw_color in {'VELOCITY', 'ACCELERATION'})
        sub.prop(part, "color_maximum", text="Max")

        if (path):
            col.prop(part, "draw_step")


class PARTICLE_PT_children(ParticleButtonsPanel, Panel):
    bl_label = "Children"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        return particle_panel_poll(cls, context)

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = particle_get_settings(context)

        layout.row().prop(part, "child_type", expand=True)

        if part.child_type == 'NONE':
            return

        row = layout.row()

        col = row.column(align=True)
        col.prop(part, "child_nbr", text="Display")
        col.prop(part, "rendered_child_count", text="Render")

        if part.child_type == 'INTERPOLATED':
            col = row.column()
            if psys:
                col.prop(psys, "child_seed", text="Seed")
            col.prop(part, "virtual_parents", slider=True)
            col.prop(part, "create_long_hair_children")
        else:
            col = row.column(align=True)
            col.prop(part, "child_size", text="Size")
            col.prop(part, "child_size_random", text="Random")

        split = layout.split()

        col = split.column()
        col.label(text="Effects:")

        sub = col.column(align=True)
        sub.prop(part, "clump_factor", slider=True)
        sub.prop(part, "clump_shape", slider=True)

        sub = col.column(align=True)
        sub.prop(part, "child_length", slider=True)
        sub.prop(part, "child_length_threshold", slider=True)

        if part.child_type == 'SIMPLE':
            sub = col.column(align=True)
            sub.prop(part, "child_radius", text="Radius")
            sub.prop(part, "child_roundness", text="Roundness", slider=True)
            if psys:
                sub.prop(psys, "child_seed", text="Seed")
        elif part.virtual_parents > 0.0:
            sub = col.column(align=True)
            sub.label(text="Parting not")
            sub.label(text="available with")
            sub.label(text="virtual parents")
        else:
            sub = col.column(align=True)
            sub.prop(part, "child_parting_factor", text="Parting", slider=True)
            sub.prop(part, "child_parting_min", text="Min")
            sub.prop(part, "child_parting_max", text="Max")

        col = split.column()
        col.label(text="Roughness:")

        sub = col.column(align=True)
        sub.prop(part, "roughness_1", text="Uniform")
        sub.prop(part, "roughness_1_size", text="Size")

        sub = col.column(align=True)
        sub.prop(part, "roughness_endpoint", "Endpoint")
        sub.prop(part, "roughness_end_shape")

        sub = col.column(align=True)
        sub.prop(part, "roughness_2", text="Random")
        sub.prop(part, "roughness_2_size", text="Size")
        sub.prop(part, "roughness_2_threshold", slider=True)

        layout.row().label(text="Kink:")
        layout.row().prop(part, "kink", expand=True)

        split = layout.split()
        split.active = part.kink != 'NO'

        col = split.column()
        sub = col.column(align=True)
        sub.prop(part, "kink_amplitude")
        sub.prop(part, "kink_amplitude_clump", text="Clump", slider=True)
        col.prop(part, "kink_flat", slider=True)
        col = split.column()
        sub = col.column(align=True)
        sub.prop(part, "kink_frequency")
        sub.prop(part, "kink_shape", slider=True)


class PARTICLE_PT_field_weights(ParticleButtonsPanel, Panel):
    bl_label = "Field Weights"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        return particle_panel_poll(cls, context)

    def draw(self, context):
        part = particle_get_settings(context)
        effector_weights_ui(self, context, part.effector_weights)

        if part.type == 'HAIR':
            row = self.layout.row()
            row.prop(part.effector_weights, "apply_to_hair_growing")
            row.prop(part, "apply_effector_to_children")
            row = self.layout.row()
            row.prop(part, "effect_hair", slider=True)


class PARTICLE_PT_force_fields(ParticleButtonsPanel, Panel):
    bl_label = "Force Field Settings"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        part = particle_get_settings(context)

        row = layout.row()
        row.prop(part, "use_self_effect")
        row.prop(part, "effector_amount", text="Amount")

        split = layout.split(percentage=0.2)
        split.label(text="Type 1:")
        split.prop(part.force_field_1, "type", text="")
        basic_force_field_settings_ui(self, context, part.force_field_1)
        if part.force_field_1.type != 'NONE':
            layout.label(text="Falloff:")
        basic_force_field_falloff_ui(self, context, part.force_field_1)

        if part.force_field_1.type != 'NONE':
            layout.label(text="")

        split = layout.split(percentage=0.2)
        split.label(text="Type 2:")
        split.prop(part.force_field_2, "type", text="")
        basic_force_field_settings_ui(self, context, part.force_field_2)
        if part.force_field_2.type != 'NONE':
            layout.label(text="Falloff:")
        basic_force_field_falloff_ui(self, context, part.force_field_2)


class PARTICLE_PT_vertexgroups(ParticleButtonsPanel, Panel):
    bl_label = "Vertex Groups"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        if context.particle_system is None:
            return False
        return particle_panel_poll(cls, context)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        psys = context.particle_system

        split = layout.split(percentage=0.85)

        col = split.column()
        col.label(text="Vertex Group:")
        col.prop_search(psys, "vertex_group_density", ob, "vertex_groups", text="Density")
        col.prop_search(psys, "vertex_group_length", ob, "vertex_groups", text="Length")
        col.prop_search(psys, "vertex_group_clump", ob, "vertex_groups", text="Clump")
        col.prop_search(psys, "vertex_group_kink", ob, "vertex_groups", text="Kink")
        col.prop_search(psys, "vertex_group_roughness_1", ob, "vertex_groups", text="Roughness 1")
        col.prop_search(psys, "vertex_group_roughness_2", ob, "vertex_groups", text="Roughness 2")
        col.prop_search(psys, "vertex_group_roughness_end", ob, "vertex_groups", text="Roughness End")

        col = split.column()
        col.label(text="Negate:")
        col.alignment = 'RIGHT'
        col.prop(psys, "invert_vertex_group_density", text="")
        col.prop(psys, "invert_vertex_group_length", text="")
        col.prop(psys, "invert_vertex_group_clump", text="")
        col.prop(psys, "invert_vertex_group_kink", text="")
        col.prop(psys, "invert_vertex_group_roughness_1", text="")
        col.prop(psys, "invert_vertex_group_roughness_2", text="")
        col.prop(psys, "invert_vertex_group_roughness_end", text="")

        # Commented out vertex groups don't work and are still waiting for better implementation
        # row = layout.row()
        # row.prop_search(psys, "vertex_group_velocity", ob, "vertex_groups", text="Velocity")
        # row.prop(psys, "invert_vertex_group_velocity", text="")

        # row = layout.row()
        # row.prop_search(psys, "vertex_group_size", ob, "vertex_groups", text="Size")
        # row.prop(psys, "invert_vertex_group_size", text="")

        # row = layout.row()
        # row.prop_search(psys, "vertex_group_tangent", ob, "vertex_groups", text="Tangent")
        # row.prop(psys, "invert_vertex_group_tangent", text="")

        # row = layout.row()
        # row.prop_search(psys, "vertex_group_rotation", ob, "vertex_groups", text="Rotation")
        # row.prop(psys, "invert_vertex_group_rotation", text="")

        # row = layout.row()
        # row.prop_search(psys, "vertex_group_field", ob, "vertex_groups", text="Field")
        # row.prop(psys, "invert_vertex_group_field", text="")


class PARTICLE_PT_custom_props(ParticleButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER'}
    _context_path = "particle_system.settings"
    _property_type = bpy.types.ParticleSettings

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
