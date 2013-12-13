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
from bpy.app.translations import contexts as i18n_contexts


class PhysicButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.object) and (not rd.use_game_engine)


def physics_add(self, layout, md, name, type, typeicon, toggles):
    row = layout.row(align=True)
    if md:
        row.context_pointer_set("modifier", md)
        row.operator("object.modifier_remove", text=name, text_ctxt=i18n_contexts.default, icon='X')
        if toggles:
            row.prop(md, "show_render", text="")
            row.prop(md, "show_viewport", text="")
    else:
        row.operator("object.modifier_add", text=name, text_ctxt=i18n_contexts.default, icon=typeicon).type = type


def physics_add_special(self, layout, data, name, addop, removeop, typeicon):
    row = layout.row(align=True)
    if data:
        row.operator(removeop, text=name, text_ctxt=i18n_contexts.default, icon='X')
    else:
        row.operator(addop, text=name, text_ctxt=i18n_contexts.default, icon=typeicon)


class PHYSICS_PT_add(PhysicButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        obj = context.object

        layout = self.layout
        layout.label("Enable physics for:")
        split = layout.split()
        col = split.column()

        if obj.field.type == 'NONE':
            col.operator("object.forcefield_toggle", text="Force Field", icon='FORCE_FORCE')
        else:
            col.operator("object.forcefield_toggle", text="Force Field", icon='X')

        if obj.type == 'MESH':
            physics_add(self, col, context.collision, "Collision", 'COLLISION', 'MOD_PHYSICS', False)
            physics_add(self, col, context.cloth, "Cloth", 'CLOTH', 'MOD_CLOTH', True)
            physics_add(self, col, context.dynamic_paint, "Dynamic Paint", 'DYNAMIC_PAINT', 'MOD_DYNAMICPAINT', True)

        col = split.column()

        if obj.type in {'MESH', 'LATTICE', 'CURVE'}:
            physics_add(self, col, context.soft_body, "Soft Body", 'SOFT_BODY', 'MOD_SOFT', True)

        if obj.type == 'MESH':
            physics_add(self, col, context.fluid, "Fluid", 'FLUID_SIMULATION', 'MOD_FLUIDSIM', True)
            physics_add(self, col, context.smoke, "Smoke", 'SMOKE', 'MOD_SMOKE', True)

            physics_add_special(self, col, obj.rigid_body, "Rigid Body",
                                "rigidbody.object_add",
                                "rigidbody.object_remove",
                                'MESH_ICOSPHERE')  # XXX: need dedicated icon

        # all types of objects can have rigid body constraint
        physics_add_special(self, col, obj.rigid_body_constraint, "Rigid Body Constraint",
                            "rigidbody.constraint_add",
                            "rigidbody.constraint_remove",
                            'CONSTRAINT')  # RB_TODO needs better icon


# cache-type can be 'PSYS' 'HAIR' 'SMOKE' etc

def point_cache_ui(self, context, cache, enabled, cachetype):
    layout = self.layout

    layout.context_pointer_set("point_cache", cache)

    if not cachetype == 'RIGID_BODY':
        row = layout.row()
        row.template_list("UI_UL_list", "point_caches", cache, "point_caches",
                          cache.point_caches, "active_index", rows=1)
        col = row.column(align=True)
        col.operator("ptcache.add", icon='ZOOMIN', text="")
        col.operator("ptcache.remove", icon='ZOOMOUT', text="")

    row = layout.row()
    if cachetype in {'PSYS', 'HAIR', 'SMOKE'}:
        row.prop(cache, "use_external")

        if cachetype == 'SMOKE':
            row.prop(cache, "use_library_path", "Use Lib Path")

    if cache.use_external:
        split = layout.split(percentage=0.35)
        col = split.column()
        col.label(text="Index Number:")
        col.label(text="File Path:")

        col = split.column()
        col.prop(cache, "index", text="")
        col.prop(cache, "filepath", text="")

        cache_info = cache.info
        if cache_info:
            layout.label(text=cache_info)
    else:
        if cachetype in {'SMOKE', 'DYNAMIC_PAINT'}:
            if not bpy.data.is_saved:
                layout.label(text="Cache is disabled until the file is saved")
                layout.enabled = False

    if not cache.use_external or cachetype == 'SMOKE':
        row = layout.row(align=True)

        if cachetype not in {'PSYS', 'DYNAMIC_PAINT'}:
            row.enabled = enabled
            row.prop(cache, "frame_start")
            row.prop(cache, "frame_end")
        if cachetype not in {'SMOKE', 'CLOTH', 'DYNAMIC_PAINT', 'RIGID_BODY'}:
            row.prop(cache, "frame_step")

        if cachetype != 'SMOKE':
            layout.label(text=cache.info)

        can_bake = True

        if cachetype not in {'SMOKE', 'DYNAMIC_PAINT', 'RIGID_BODY'}:
            split = layout.split()
            split.enabled = enabled and bpy.data.is_saved

            col = split.column()
            col.prop(cache, "use_disk_cache")

            col = split.column()
            col.active = cache.use_disk_cache
            col.prop(cache, "use_library_path", "Use Lib Path")

            row = layout.row()
            row.enabled = enabled and bpy.data.is_saved
            row.active = cache.use_disk_cache
            row.label(text="Compression:")
            row.prop(cache, "compression", expand=True)

            layout.separator()

            if cache.id_data.library and not cache.use_disk_cache:
                can_bake = False

                col = layout.column(align=True)
                col.label(text="Linked object baking requires Disk Cache to be enabled", icon='INFO')
        else:
            layout.separator()

        split = layout.split()
        split.active = can_bake

        col = split.column()

        if cache.is_baked is True:
            col.operator("ptcache.free_bake", text="Free Bake")
        else:
            col.operator("ptcache.bake", text="Bake").bake = True

        sub = col.row()
        sub.enabled = (cache.frames_skipped or cache.is_outdated) and enabled
        sub.operator("ptcache.bake", text="Calculate To Frame").bake = False

        sub = col.column()
        sub.enabled = enabled
        sub.operator("ptcache.bake_from_cache", text="Current Cache to Bake")

        col = split.column()
        col.operator("ptcache.bake_all", text="Bake All Dynamics").bake = True
        col.operator("ptcache.free_bake_all", text="Free All Bakes")
        col.operator("ptcache.bake_all", text="Update All To Frame").bake = False


def effector_weights_ui(self, context, weights, weight_type):
    layout = self.layout

    layout.prop(weights, "group")

    split = layout.split()

    split.prop(weights, "gravity", slider=True)
    split.prop(weights, "all", slider=True)

    layout.separator()

    split = layout.split()

    col = split.column()
    col.prop(weights, "force", slider=True)
    col.prop(weights, "vortex", slider=True)
    col.prop(weights, "magnetic", slider=True)
    col.prop(weights, "wind", slider=True)
    col.prop(weights, "curve_guide", slider=True)
    col.prop(weights, "texture", slider=True)
    if weight_type != 'SMOKE':
        col.prop(weights, "smokeflow", slider=True)

    col = split.column()
    col.prop(weights, "harmonic", slider=True)
    col.prop(weights, "charge", slider=True)
    col.prop(weights, "lennardjones", slider=True)
    col.prop(weights, "turbulence", slider=True)
    col.prop(weights, "drag", slider=True)
    col.prop(weights, "boid", slider=True)


def basic_force_field_settings_ui(self, context, field):
    layout = self.layout

    split = layout.split()

    if not field or field.type == 'NONE':
        return

    col = split.column()

    if field.type == 'DRAG':
        col.prop(field, "linear_drag", text="Linear")
    else:
        col.prop(field, "strength")

    if field.type == 'TURBULENCE':
        col.prop(field, "size")
        col.prop(field, "flow")
    elif field.type == 'HARMONIC':
        col.prop(field, "harmonic_damping", text="Damping")
        col.prop(field, "rest_length")
    elif field.type == 'VORTEX' and field.shape != 'POINT':
        col.prop(field, "inflow")
    elif field.type == 'DRAG':
        col.prop(field, "quadratic_drag", text="Quadratic")
    else:
        col.prop(field, "flow")

    col = split.column()
    sub = col.column(align=True)
    sub.prop(field, "noise")
    sub.prop(field, "seed")
    if field.type == 'TURBULENCE':
        col.prop(field, "use_global_coords", text="Global")
    elif field.type == 'HARMONIC':
        col.prop(field, "use_multiple_springs")

    split = layout.split()

    col = split.column()
    col.label(text="Effect point:")
    col.prop(field, "apply_to_location")
    col.prop(field, "apply_to_rotation")

    col = split.column()
    col.label(text="Collision:")
    col.prop(field, "use_absorption")


def basic_force_field_falloff_ui(self, context, field):
    layout = self.layout

    split = layout.split(percentage=0.35)

    if not field or field.type == 'NONE':
        return

    col = split.column()
    col.prop(field, "z_direction", text="")

    col = split.column()
    col.prop(field, "falloff_power", text="Power")

    split = layout.split()
    col = split.column()
    row = col.row(align=True)
    row.prop(field, "use_min_distance", text="")
    sub = row.row(align=True)
    sub.active = field.use_min_distance
    sub.prop(field, "distance_min", text="Minimum")

    col = split.column()
    row = col.row(align=True)
    row.prop(field, "use_max_distance", text="")
    sub = row.row(align=True)
    sub.active = field.use_max_distance
    sub.prop(field, "distance_max", text="Maximum")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
