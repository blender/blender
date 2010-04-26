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

from properties_physics_common import point_cache_ui
from properties_physics_common import effector_weights_ui
from properties_physics_common import basic_force_field_settings_ui
from properties_physics_common import basic_force_field_falloff_ui

narrowui = 180


def particle_panel_enabled(context, psys):
    return (psys.point_cache.baked is False) and (not psys.edited) and (not context.particle_system_editable)


def particle_panel_poll(context):
    psys = context.particle_system
    if psys is None:
        return False
    if psys.settings is None:
        return False
    return psys.settings.type in ('EMITTER', 'REACTOR', 'HAIR')


class ParticleButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "particle"

    def poll(self, context):
        return particle_panel_poll(context)


class PARTICLE_PT_context_particles(ParticleButtonsPanel):
    bl_label = ""
    bl_show_header = False

    def poll(self, context):
        return (context.particle_system or context.object)

    def draw(self, context):
        layout = self.layout

        ob = context.object
        psys = context.particle_system

        if ob:
            row = layout.row()

            row.template_list(ob, "particle_systems", ob, "active_particle_system_index", rows=2)

            col = row.column(align=True)
            col.operator("object.particle_system_add", icon='ZOOMIN', text="")
            col.operator("object.particle_system_remove", icon='ZOOMOUT', text="")

        if psys and not psys.settings:
            split = layout.split(percentage=0.32)

            col = split.column()
            col.label(text="Name:")
            col.label(text="Settings:")

            col = split.column()
            col.prop(psys, "name", text="")
            col.template_ID(psys, "settings", new="particle.new")
        elif psys:
            part = psys.settings

            split = layout.split(percentage=0.32)
            col = split.column()
            col.label(text="Name:")
            if part.type in ('EMITTER', 'REACTOR', 'HAIR'):
                col.label(text="Settings:")
                col.label(text="Type:")

            col = split.column()
            col.prop(psys, "name", text="")
            if part.type in ('EMITTER', 'REACTOR', 'HAIR'):
                col.template_ID(psys, "settings", new="particle.new")

            #row = layout.row()
            #row.label(text="Viewport")
            #row.label(text="Render")

            if part:
                if part.type not in ('EMITTER', 'REACTOR', 'HAIR'):
                    layout.label(text="No settings for fluid particles")
                    return

                row = col.row()
                row.enabled = particle_panel_enabled(context, psys)
                row.prop(part, "type", text="")
                row.prop(psys, "seed")

                split = layout.split(percentage=0.65)
                if part.type == 'HAIR':
                    if psys.edited:
                        split.operator("particle.edited_clear", text="Free Edit")
                    else:
                        split.label(text="")
                    row = split.row()
                    row.enabled = particle_panel_enabled(context, psys)
                    row.prop(part, "hair_step")
                    if psys.edited:
                        if psys.global_hair:
                            layout.operator("particle.connect_hair")
                            layout.label(text="Hair is disconnected.")
                        else:
                            layout.operator("particle.disconnect_hair")
                            layout.label(text="")
                elif part.type == 'REACTOR':
                    split.enabled = particle_panel_enabled(context, psys)
                    split.prop(psys, "reactor_target_object")
                    split.prop(psys, "reactor_target_particle_system", text="Particle System")


class PARTICLE_PT_custom_props(ParticleButtonsPanel, PropertyPanel):
    _context_path = "particle_system.settings"


class PARTICLE_PT_emission(ParticleButtonsPanel):
    bl_label = "Emission"

    def poll(self, context):
        if particle_panel_poll(context):
            return not context.particle_system.point_cache.external
        else:
            return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings
        wide_ui = context.region.width > narrowui

        layout.enabled = particle_panel_enabled(context, psys) and not psys.multiple_caches

        row = layout.row()
        row.active = part.distribution != 'GRID'
        row.prop(part, "amount")

        if part.type != 'HAIR':
            split = layout.split()

            col = split.column(align=True)
            col.prop(part, "frame_start")
            col.prop(part, "frame_end")

            col = split.column(align=True)
            col.prop(part, "lifetime")
            col.prop(part, "random_lifetime", slider=True)

        layout.row().label(text="Emit From:")

        row = layout.row()
        if wide_ui:
            row.prop(part, "emit_from", expand=True)
        else:
            row.prop(part, "emit_from", text="")
        row = layout.row()
        row.prop(part, "trand")
        if part.distribution != 'GRID':
            row.prop(part, "even_distribution")

        if part.emit_from == 'FACE' or part.emit_from == 'VOLUME':
            row = layout.row()
            if wide_ui:
                row.prop(part, "distribution", expand=True)
            else:
                row.prop(part, "distribution", text="")

            row = layout.row()

            if part.distribution == 'JIT':
                row.prop(part, "userjit", text="Particles/Face")
                row.prop(part, "jitter_factor", text="Jittering Amount", slider=True)
            elif part.distribution == 'GRID':
                row.prop(part, "grid_resolution")


class PARTICLE_PT_hair_dynamics(ParticleButtonsPanel):
    bl_label = "Hair dynamics"
    bl_default_closed = True

    def poll(self, context):
        psys = context.particle_system
        if psys is None:
            return False
        if psys.settings is None:
            return False
        return psys.settings.type == 'HAIR'

    def draw_header(self, context):
        #cloth = context.cloth.collision_settings

        #self.layout.active = cloth_panel_enabled(context.cloth)
        #self.layout.prop(cloth, "enable_collision", text="")
        psys = context.particle_system
        self.layout.prop(psys, "hair_dynamics", text="")

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system

        if not psys.cloth:
            return

        #part = psys.settings
        cloth = psys.cloth.settings

        layout.enabled = psys.hair_dynamics

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


class PARTICLE_PT_cache(ParticleButtonsPanel):
    bl_label = "Cache"
    bl_default_closed = True

    def poll(self, context):
        psys = context.particle_system
        if psys is None:
            return False
        if psys.settings is None:
            return False
        phystype = psys.settings.physics_type
        if phystype == 'NO' or phystype == 'KEYED':
            return False
        return psys.settings.type in ('EMITTER', 'REACTOR') or (psys.settings.type == 'HAIR' and psys.hair_dynamics)

    def draw(self, context):

        psys = context.particle_system

        point_cache_ui(self, context, psys.point_cache, particle_panel_enabled(context, psys), not psys.hair_dynamics, 0)


class PARTICLE_PT_velocity(ParticleButtonsPanel):
    bl_label = "Velocity"

    def poll(self, context):
        if particle_panel_poll(context):
            psys = context.particle_system
            return psys.settings.physics_type != 'BOIDS' and not psys.point_cache.external
        else:
            return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings

        layout.enabled = particle_panel_enabled(context, psys)

        split = layout.split()

        sub = split.column()
        sub.label(text="Emitter Geometry:")
        sub.prop(part, "normal_factor")
        subsub = sub.column(align=True)
        subsub.prop(part, "tangent_factor")
        subsub.prop(part, "tangent_phase", slider=True)

        sub = split.column()
        sub.label(text="Emitter Object")
        sub.prop(part, "object_aligned_factor", text="")

        layout.row().label(text="Other:")
        split = layout.split()
        sub = split.column()
        if part.emit_from == 'PARTICLE':
            sub.prop(part, "particle_factor")
        else:
            sub.prop(part, "object_factor", slider=True)
        sub = split.column()
        sub.prop(part, "random_factor")

        #if part.type=='REACTOR':
        #    sub.prop(part, "reactor_factor")
        #    sub.prop(part, "reaction_shape", slider=True)


class PARTICLE_PT_rotation(ParticleButtonsPanel):
    bl_label = "Rotation"

    def poll(self, context):
        if particle_panel_poll(context):
            psys = context.particle_system
            return psys.settings.physics_type != 'BOIDS' and not psys.point_cache.external
        else:
            return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings
        wide_ui = context.region.width > narrowui

        layout.enabled = particle_panel_enabled(context, psys)

        split = layout.split()
        split.label(text="Initial Rotation:")
        split.prop(part, "rotation_dynamic")
        split = layout.split()

        sub = split.column(align=True)
        sub.prop(part, "rotation_mode", text="")
        sub.prop(part, "random_rotation_factor", slider=True, text="Random")

        sub = split.column(align=True)
        sub.prop(part, "phase_factor", slider=True)
        sub.prop(part, "random_phase_factor", text="Random", slider=True)

        layout.row().label(text="Angular Velocity:")
        if wide_ui:
            layout.row().prop(part, "angular_velocity_mode", expand=True)
        else:
            layout.row().prop(part, "angular_velocity_mode", text="")
        split = layout.split()

        sub = split.column()

        if part.angular_velocity_mode != 'NONE':
            sub.prop(part, "angular_velocity_factor", text="")


class PARTICLE_PT_physics(ParticleButtonsPanel):
    bl_label = "Physics"

    def poll(self, context):
        if particle_panel_poll(context):
            return not context.particle_system.point_cache.external
        else:
            return False

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings
        wide_ui = context.region.width > narrowui

        layout.enabled = particle_panel_enabled(context, psys)

        row = layout.row()
        if wide_ui:
            row.prop(part, "physics_type", expand=True)
        else:
            row.prop(part, "physics_type", text="")
            
        if part.physics_type != 'NO':
            row = layout.row()
            col = row.column(align=True)
            col.prop(part, "particle_size")
            col.prop(part, "random_size", slider=True)
            col = row.column(align=True)
            col.prop(part, "mass")
            col.prop(part, "sizemass", text="Multiply mass with size")

        if part.physics_type == 'NEWTON':
            split = layout.split()
            sub = split.column()

            sub.label(text="Forces:")
            sub.prop(part, "brownian_factor")
            sub.prop(part, "drag_factor", slider=True)
            sub.prop(part, "damp_factor", slider=True)
            sub = split.column()
            sub.label(text="Integration:")
            sub.prop(part, "integrator", text="")
            sub.prop(part, "time_tweak")
            sub.prop(part, "subframes")
            sub = layout.row()
            sub.prop(part, "size_deflect")
            sub.prop(part, "die_on_collision")

        elif part.physics_type == 'FLUID':
            fluid = part.fluid
            split = layout.split()
            sub = split.column()

            sub.label(text="Forces:")
            sub.prop(part, "brownian_factor")
            sub.prop(part, "drag_factor", slider=True)
            sub.prop(part, "damp_factor", slider=True)
            sub = split.column()
            sub.label(text="Integration:")
            sub.prop(part, "integrator", text="")
            sub.prop(part, "time_tweak")
            sub.prop(part, "subframes")
            sub = layout.row()
            sub.prop(part, "size_deflect")
            sub.prop(part, "die_on_collision")

            split = layout.split()
            sub = split.column()
            sub.label(text="Fluid Interaction:")
            sub.prop(fluid, "fluid_radius", slider=True)
            sub.prop(fluid, "stiffness_k")
            sub.prop(fluid, "stiffness_knear")
            sub.prop(fluid, "rest_density")

            sub.label(text="Viscosity:")
            sub.prop(fluid, "viscosity_omega", text="Linear")
            sub.prop(fluid, "viscosity_beta", text="Square")

            sub = split.column()

            sub.label(text="Springs:")
            sub.prop(fluid, "spring_k", text="Force", slider=True)
            sub.prop(fluid, "rest_length", slider=True)
            layout.label(text="Multiple fluids interactions:")

            sub.label(text="Buoyancy:")
            sub.prop(fluid, "buoyancy", slider=True)

        elif part.physics_type == 'KEYED':
            split = layout.split()
            sub = split.column()

            row = layout.row()
            col = row.column()
            col.active = not psys.keyed_timing
            col.prop(part, "keyed_loops", text="Loops")
            row.prop(psys, "keyed_timing", text="Use Timing")

            layout.label(text="Keys:")
        elif part.physics_type == 'BOIDS':
            boids = part.boids


            row = layout.row()
            row.prop(boids, "allow_flight")
            row.prop(boids, "allow_land")
            row.prop(boids, "allow_climb")

            split = layout.split()

            sub = split.column()
            col = sub.column(align=True)
            col.active = boids.allow_flight
            col.prop(boids, "air_max_speed")
            col.prop(boids, "air_min_speed", slider=True)
            col.prop(boids, "air_max_acc", slider=True)
            col.prop(boids, "air_max_ave", slider=True)
            col.prop(boids, "air_personal_space")
            row = col.row()
            row.active = (boids.allow_land or boids.allow_climb) and boids.allow_flight
            row.prop(boids, "landing_smoothness")

            sub = split.column()
            col = sub.column(align=True)
            col.active = boids.allow_land or boids.allow_climb
            col.prop(boids, "land_max_speed")
            col.prop(boids, "land_jump_speed")
            col.prop(boids, "land_max_acc", slider=True)
            col.prop(boids, "land_max_ave", slider=True)
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
            col.prop(boids, "banking", slider=True)
            col.prop(boids, "height", slider=True)

        if part.physics_type == 'KEYED' or part.physics_type == 'BOIDS' or part.physics_type == 'FLUID':
            if part.physics_type == 'BOIDS':
                layout.label(text="Relations:")

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
                    #col.red_alert = key.valid
                    col.prop(key, "object", text="")
                    col.prop(key, "system", text="System")
                    col = row.column()
                    col.active = psys.keyed_timing
                    col.prop(key, "time")
                    col.prop(key, "duration")
                elif part.physics_type == 'BOIDS':
                    sub = row.row()
                    #doesn't work yet
                    #sub.red_alert = key.valid
                    sub.prop(key, "object", text="")
                    sub.prop(key, "system", text="System")

                    layout.prop(key, "mode", expand=True)
                elif part.physics_type == 'FLUID':
                    sub = row.row()
                    #doesn't work yet
                    #sub.red_alert = key.valid
                    sub.prop(key, "object", text="")
                    sub.prop(key, "system", text="System")


class PARTICLE_PT_boidbrain(ParticleButtonsPanel):
    bl_label = "Boid Brain"

    def poll(self, context):
        psys = context.particle_system
        if psys is None:
            return False
        if psys.settings is None:
            return False
        if psys.point_cache.external:
            return False
        return psys.settings.physics_type == 'BOIDS'

    def draw(self, context):
        layout = self.layout

        boids = context.particle_system.settings.boids

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
            row.prop(state, "rule_fuzziness", slider=True)
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
            row.prop(rule, "in_air", icon='MOVE_UP_VEC', text="")
            row.prop(rule, "on_land", icon='MOVE_DOWN_VEC', text="")

            row = layout.row()

            if rule.type == 'GOAL':
                row.prop(rule, "object")
                row = layout.row()
                row.prop(rule, "predict")
            elif rule.type == 'AVOID':
                row.prop(rule, "object")
                row = layout.row()
                row.prop(rule, "predict")
                row.prop(rule, "fear_factor")
            elif rule.type == 'FOLLOW_PATH':
                row.label(text="Not yet functional.")
            elif rule.type == 'AVOID_COLLISION':
                row.prop(rule, "boids")
                row.prop(rule, "deflectors")
                row.prop(rule, "look_ahead")
            elif rule.type == 'FOLLOW_LEADER':
                row.prop(rule, "object", text="")
                row.prop(rule, "distance")
                row = layout.row()
                row.prop(rule, "line")
                sub = row.row()
                sub.active = rule.line
                sub.prop(rule, "queue_size")
            elif rule.type == 'AVERAGE_SPEED':
                row.prop(rule, "speed", slider=True)
                row.prop(rule, "wander", slider=True)
                row.prop(rule, "level", slider=True)
            elif rule.type == 'FIGHT':
                row.prop(rule, "distance")
                row.prop(rule, "flee_distance")


class PARTICLE_PT_render(ParticleButtonsPanel):
    bl_label = "Render"

    def poll(self, context):
        psys = context.particle_system
        if psys is None:
            return False
        if psys.settings is None:
            return False
        return True

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.prop(part, "material")
        row.prop(psys, "parent")

        split = layout.split()

        sub = split.column()
        sub.prop(part, "emitter")
        sub.prop(part, "parent")
        sub = split.column()
        sub.prop(part, "unborn")
        sub.prop(part, "died")

        row = layout.row()
        if wide_ui:
            row.prop(part, "ren_as", expand=True)
        else:
            row.prop(part, "ren_as", text="")

        split = layout.split()

        sub = split.column()

        if part.ren_as == 'LINE':
            sub.prop(part, "line_length_tail")
            sub.prop(part, "line_length_head")
            sub = split.column()
            sub.prop(part, "velocity_length")
        elif part.ren_as == 'PATH':

            if part.type != 'HAIR' and part.physics_type != 'KEYED' and (psys.point_cache.baked is False):
                box = layout.box()
                box.label(text="Baked or keyed particles needed for correct rendering.")
                return

            sub.prop(part, "render_strand")
            subsub = sub.column()
            subsub.active = (part.render_strand is False)
            subsub.prop(part, "render_adaptive")
            subsub = sub.column()
            subsub.active = part.render_adaptive or part.render_strand == True
            subsub.prop(part, "adaptive_angle")
            subsub = sub.column()
            subsub.active = (part.render_adaptive is True and part.render_strand is False)
            subsub.prop(part, "adaptive_pix")
            sub.prop(part, "hair_bspline")
            sub.prop(part, "render_step", text="Steps")

            sub = split.column()
            sub.label(text="Timing:")
            sub.prop(part, "abs_path_time")
            sub.prop(part, "path_start", text="Start", slider=not part.abs_path_time)
            sub.prop(part, "path_end", text="End", slider=not part.abs_path_time)
            sub.prop(part, "random_length", text="Random", slider=True)

            row = layout.row()
            col = row.column()

            if part.type == 'HAIR' and part.render_strand == True and part.child_type == 'FACES':
                layout.prop(part, "enable_simplify")
                if part.enable_simplify == True:
                    row = layout.row()
                    row.prop(part, "simplify_refsize")
                    row.prop(part, "simplify_rate")
                    row.prop(part, "simplify_transition")
                    row = layout.row()
                    row.prop(part, "viewport")
                    sub = row.row()
                    sub.active = part.viewport == True
                    sub.prop(part, "simplify_viewport")

        elif part.ren_as == 'OBJECT':
            sub.prop(part, "dupli_object")
            sub.prop(part, "use_global_dupli")
        elif part.ren_as == 'GROUP':
            sub.prop(part, "dupli_group")
            split = layout.split()
            sub = split.column()
            sub.prop(part, "whole_group")
            subsub = sub.column()
            subsub.active = (part.whole_group is False)
            subsub.prop(part, "use_group_count")

            sub = split.column()
            subsub = sub.column()
            subsub.active = (part.whole_group is False)
            subsub.prop(part, "use_global_dupli")
            subsub.prop(part, "rand_group")

            if part.use_group_count and not part.whole_group:
                row = layout.row()
                row.template_list(part, "dupliweights", part, "active_dupliweight_index")

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

        elif part.ren_as == 'BILLBOARD':
            sub.label(text="Align:")

            row = layout.row()
            if wide_ui:
                row.prop(part, "billboard_align", expand=True)
            else:
                row.prop(part, "billboard_align", text="")
            row.prop(part, "billboard_lock", text="Lock")
            row = layout.row()
            row.prop(part, "billboard_object")

            row = layout.row()
            col = row.column(align=True)
            col.label(text="Tilt:")
            col.prop(part, "billboard_tilt", text="Angle", slider=True)
            col.prop(part, "billboard_random_tilt", slider=True)
            col = row.column()
            col.prop(part, "billboard_offset")

            row = layout.row()
            row.prop(psys, "billboard_normal_uv")
            row = layout.row()
            row.prop(psys, "billboard_time_index_uv")

            row = layout.row()
            row.label(text="Split uv's:")
            row.prop(part, "billboard_uv_split", text="Number of splits")
            row = layout.row()
            row.prop(psys, "billboard_split_uv")
            row = layout.row()
            row.label(text="Animate:")
            row.prop(part, "billboard_animation", text="")
            row.label(text="Offset:")
            row.prop(part, "billboard_split_offset", text="")

        if part.ren_as == 'HALO' or part.ren_as == 'LINE' or part.ren_as == 'BILLBOARD':
            row = layout.row()
            col = row.column()
            col.prop(part, "trail_count")
            if part.trail_count > 1:
                col.prop(part, "abs_path_time", text="Length in frames")
                col = row.column()
                col.prop(part, "path_end", text="Length", slider=not part.abs_path_time)
                col.prop(part, "random_length", text="Random", slider=True)
            else:
                col = row.column()
                col.label(text="")


class PARTICLE_PT_draw(ParticleButtonsPanel):
    bl_label = "Display"
    bl_default_closed = True

    def poll(self, context):
        psys = context.particle_system
        if psys is None:
            return False
        if psys.settings is None:
            return False
        return True

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings
        wide_ui = context.region.width > narrowui

        row = layout.row()
        if wide_ui:
            row.prop(part, "draw_as", expand=True)
        else:
            row.prop(part, "draw_as", text="")

        if part.draw_as == 'NONE' or (part.ren_as == 'NONE' and part.draw_as == 'RENDER'):
            return

        path = (part.ren_as == 'PATH' and part.draw_as == 'RENDER') or part.draw_as == 'PATH'

        if path and part.type != 'HAIR' and part.physics_type != 'KEYED' and psys.point_cache.baked is False:
            box = layout.box()
            box.label(text="Baked or keyed particles needed for correct drawing.")
            return

        row = layout.row()
        row.prop(part, "display", slider=True)
        if part.draw_as != 'RENDER' or part.ren_as == 'HALO':
            row.prop(part, "draw_size")
        else:
            row.label(text="")

        row = layout.row()
        col = row.column()
        col.prop(part, "show_size")
        col.prop(part, "velocity")
        col.prop(part, "num")
        if part.physics_type == 'BOIDS':
            col.prop(part, "draw_health")

        col = row.column()
        col.prop(part, "material_color", text="Use material color")

        if (path):
            col.prop(part, "draw_step")
        else:
            sub = col.column()
            sub.active = (part.material_color is False)
            #sub.label(text="color")
            #sub.label(text="Override material color")


class PARTICLE_PT_children(ParticleButtonsPanel):
    bl_label = "Children"
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        psys = context.particle_system
        part = psys.settings
        wide_ui = context.region.width > narrowui

        if wide_ui:
            layout.row().prop(part, "child_type", expand=True)
        else:
            layout.row().prop(part, "child_type", text="")

        if part.child_type == 'NONE':
            return

        row = layout.row()

        col = row.column(align=True)
        col.prop(part, "child_nbr", text="Display")
        col.prop(part, "rendered_child_nbr", text="Render")

        col = row.column(align=True)

        if part.child_type == 'FACES':
            col.prop(part, "virtual_parents", slider=True)
        else:
            col.prop(part, "child_radius", text="Radius")
            col.prop(part, "child_roundness", text="Roundness", slider=True)

            col = row.column(align=True)
            col.prop(part, "child_size", text="Size")
            col.prop(part, "child_random_size", text="Random")

        layout.row().label(text="Effects:")

        row = layout.row()

        col = row.column(align=True)
        col.prop(part, "clump_factor", slider=True)
        col.prop(part, "clumppow", slider=True)

        col = row.column(align=True)
        col.prop(part, "rough_endpoint")
        col.prop(part, "rough_end_shape")

        row = layout.row()

        col = row.column(align=True)
        col.prop(part, "rough1")
        col.prop(part, "rough1_size")

        col = row.column(align=True)
        col.prop(part, "rough2")
        col.prop(part, "rough2_size")
        col.prop(part, "rough2_thres", slider=True)

        row = layout.row()
        col = row.column(align=True)
        col.prop(part, "child_length", slider=True)
        col.prop(part, "child_length_thres", slider=True)

        col = row.column(align=True)
        col.label(text="Space reserved for")
        col.label(text="hair parting controls")

        layout.row().label(text="Kink:")
        if wide_ui:
            layout.row().prop(part, "kink", expand=True)
        else:
            layout.row().prop(part, "kink", text="")

        split = layout.split()

        col = split.column()
        col.prop(part, "kink_amplitude")
        col.prop(part, "kink_frequency")
        col = split.column()
        col.prop(part, "kink_shape", slider=True)


class PARTICLE_PT_field_weights(ParticleButtonsPanel):
    bl_label = "Field Weights"
    bl_default_closed = True

    def draw(self, context):
        part = context.particle_system.settings
        effector_weights_ui(self, context, part.effector_weights)

        if part.type == 'HAIR':
            self.layout.prop(part.effector_weights, "do_growing_hair")


class PARTICLE_PT_force_fields(ParticleButtonsPanel):
    bl_label = "Force Field Settings"
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        part = context.particle_system.settings

        layout.prop(part, "self_effect")

        split = layout.split(percentage=0.2)
        split.label(text="Type 1:")
        split.prop(part.force_field_1, "type", text="")
        basic_force_field_settings_ui(self, context, part.force_field_1)
        basic_force_field_falloff_ui(self, context, part.force_field_1)

        if part.force_field_1.type != 'NONE':
            layout.label(text="")

        split = layout.split(percentage=0.2)
        split.label(text="Type 2:")
        split.prop(part.force_field_2, "type", text="")
        basic_force_field_settings_ui(self, context, part.force_field_2)
        basic_force_field_falloff_ui(self, context, part.force_field_2)


class PARTICLE_PT_vertexgroups(ParticleButtonsPanel):
    bl_label = "Vertexgroups"
    bl_default_closed = True

    def draw(self, context):
        layout = self.layout

        ob = context.object
        psys = context.particle_system
        # part = psys.settings

        # layout.label(text="Nothing here yet.")

        row = layout.row()
        row.label(text="Vertex Group")
        row.label(text="Negate")


        row = layout.row()
        row.prop_object(psys, "vertex_group_density", ob, "vertex_groups", text="Density")
        row.prop(psys, "vertex_group_density_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_velocity", ob, "vertex_groups", text="Velocity")
        row.prop(psys, "vertex_group_velocity_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_length", ob, "vertex_groups", text="Length")
        row.prop(psys, "vertex_group_length_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_clump", ob, "vertex_groups", text="Clump")
        row.prop(psys, "vertex_group_clump_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_kink", ob, "vertex_groups", text="Kink")
        row.prop(psys, "vertex_group_kink_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_roughness1", ob, "vertex_groups", text="Roughness 1")
        row.prop(psys, "vertex_group_roughness1_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_roughness2", ob, "vertex_groups", text="Roughness 2")
        row.prop(psys, "vertex_group_roughness2_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_roughness_end", ob, "vertex_groups", text="Roughness End")
        row.prop(psys, "vertex_group_roughness_end_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_size", ob, "vertex_groups", text="Size")
        row.prop(psys, "vertex_group_size_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_tangent", ob, "vertex_groups", text="Tangent")
        row.prop(psys, "vertex_group_tangent_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_rotation", ob, "vertex_groups", text="Rotation")
        row.prop(psys, "vertex_group_rotation_negate", text="")

        row = layout.row()
        row.prop_object(psys, "vertex_group_field", ob, "vertex_groups", text="Field")
        row.prop(psys, "vertex_group_field_negate", text="")


classes = [
    PARTICLE_PT_context_particles,
    PARTICLE_PT_hair_dynamics,
    PARTICLE_PT_cache,
    PARTICLE_PT_emission,
    PARTICLE_PT_velocity,
    PARTICLE_PT_rotation,
    PARTICLE_PT_physics,
    PARTICLE_PT_boidbrain,
    PARTICLE_PT_render,
    PARTICLE_PT_draw,
    PARTICLE_PT_children,
    PARTICLE_PT_field_weights,
    PARTICLE_PT_force_fields,
    PARTICLE_PT_vertexgroups,

    PARTICLE_PT_custom_props]


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
