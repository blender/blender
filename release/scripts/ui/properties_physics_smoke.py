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

narrowui = 180


from properties_physics_common import point_cache_ui
from properties_physics_common import effector_weights_ui


class PhysicButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    def poll(self, context):
        ob = context.object
        rd = context.scene.render_data
        return (ob and ob.type == 'MESH') and (not rd.use_game_engine)


class PHYSICS_PT_smoke(PhysicButtonsPanel):
    bl_label = "Smoke"

    def draw(self, context):
        layout = self.layout

        md = context.smoke
        ob = context.object
        wide_ui = context.region.width > narrowui

        split = layout.split()
        split.operator_context = 'EXEC_DEFAULT'

        if md:
            # remove modifier + settings
            split.set_context_pointer("modifier", md)
            split.itemO("object.modifier_remove", text="Remove")

            row = split.row(align=True)
            row.itemR(md, "render", text="")
            row.itemR(md, "realtime", text="")

        else:
            # add modifier
            split.item_enumO("object.modifier_add", "type", 'SMOKE', text="Add")
            if wide_ui:
                split.itemL()

        if md:
            if wide_ui:
                layout.itemR(md, "smoke_type", expand=True)
            else:
                layout.itemR(md, "smoke_type", text="")

            if md.smoke_type == 'TYPE_DOMAIN':

                domain = md.domain_settings

                split = layout.split()

                col = split.column()
                col.itemL(text="Resolution:")
                col.itemR(domain, "maxres", text="Divisions")

                if wide_ui:
                     col = split.column()
                col.itemL(text="Behavior:")
                col.itemR(domain, "alpha")
                col.itemR(domain, "beta")
                col.itemR(domain, "dissolve_smoke", text="Dissolve")
                sub = col.column()
                sub.active = domain.dissolve_smoke
                sub.itemR(domain, "dissolve_speed", text="Time")
                sub.itemR(domain, "dissolve_smoke_log", text="Slow")

            elif md.smoke_type == 'TYPE_FLOW':

                flow = md.flow_settings

                split = layout.split()

                col = split.column()
                col.itemR(flow, "outflow")
                col.itemL(text="Particle System:")
                col.item_pointerR(flow, "psys", ob, "particle_systems", text="")

                if md.flow_settings.outflow:
                    if wide_ui:
                        col = split.column()
                else:
                    if wide_ui:
                       col = split.column()
                    col.itemL(text="Behavior:")
                    col.itemR(flow, "temperature")
                    col.itemR(flow, "density")

            #elif md.smoke_type == 'TYPE_COLL':
            #	layout.itemS()


class PHYSICS_PT_smoke_groups(PhysicButtonsPanel):
    bl_label = "Smoke Groups"
    bl_default_closed = True

    def poll(self, context):
        md = context.smoke
        return md and (md.smoke_type == 'TYPE_DOMAIN')

    def draw(self, context):
        layout = self.layout

        group = context.smoke.domain_settings
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.itemL(text="Flow Group:")
        col.itemR(group, "fluid_group", text="")

        #col.itemL(text="Effector Group:")
        #col.itemR(group, "eff_group", text="")

        if wide_ui:
            col = split.column()
        col.itemL(text="Collision Group:")
        col.itemR(group, "coll_group", text="")


class PHYSICS_PT_smoke_cache(PhysicButtonsPanel):
    bl_label = "Smoke Cache"
    bl_default_closed = True

    def poll(self, context):
        md = context.smoke
        return md and (md.smoke_type == 'TYPE_DOMAIN')

    def draw(self, context):
        md = context.smoke.domain_settings
        cache = md.point_cache_low

        point_cache_ui(self, context, cache, cache.baked == False, 0, 1)


class PHYSICS_PT_smoke_highres(PhysicButtonsPanel):
    bl_label = "Smoke High Resolution"
    bl_default_closed = True

    def poll(self, context):
        md = context.smoke
        return md and (md.smoke_type == 'TYPE_DOMAIN')

    def draw_header(self, context):
        high = context.smoke.domain_settings

        self.layout.itemR(high, "highres", text="")

    def draw(self, context):
        layout = self.layout

        md = context.smoke.domain_settings
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.itemL(text="Resolution:")
        col.itemR(md, "amplify", text="Divisions")
        col.itemR(md, "viewhighres")

        if wide_ui:
            col = split.column()
        col.itemL(text="Noise Method:")
        col.row().itemR(md, "noise_type", text="")
        col.itemR(md, "strength")


class PHYSICS_PT_smoke_cache_highres(PhysicButtonsPanel):
    bl_label = "Smoke High Resolution Cache"
    bl_default_closed = True

    def poll(self, context):
        md = context.smoke
        return md and (md.smoke_type == 'TYPE_DOMAIN') and md.domain_settings.highres

    def draw(self, context):
        md = context.smoke.domain_settings
        cache = md.point_cache_high

        point_cache_ui(self, context, cache, cache.baked == False, 0, 1)


class PHYSICS_PT_smoke_field_weights(PhysicButtonsPanel):
    bl_label = "Smoke Field Weights"
    bl_default_closed = True

    def poll(self, context):
        smoke = context.smoke
        return (smoke and smoke.smoke_type == 'TYPE_DOMAIN')

    def draw(self, context):
        domain = context.smoke.domain_settings
        effector_weights_ui(self, context, domain.effector_weights)

bpy.types.register(PHYSICS_PT_smoke)
bpy.types.register(PHYSICS_PT_smoke_field_weights)
bpy.types.register(PHYSICS_PT_smoke_cache)
bpy.types.register(PHYSICS_PT_smoke_highres)
bpy.types.register(PHYSICS_PT_smoke_groups)
bpy.types.register(PHYSICS_PT_smoke_cache_highres)
