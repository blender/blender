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

narrowui = 180


def point_cache_ui(self, context, cache, enabled, particles, smoke):
    layout = self.layout
    
    wide_ui = context.region.width > narrowui
    layout.set_context_pointer("PointCache", cache)

    row = layout.row()
    row.template_list(cache, "point_cache_list", cache, "active_point_cache_index", rows=2)
    col = row.column(align=True)
    col.itemO("ptcache.add_new", icon='ICON_ZOOMIN', text="")
    col.itemO("ptcache.remove", icon='ICON_ZOOMOUT', text="")

    row = layout.row()
    row.itemL(text="File Name:")
    if particles:
        row.itemR(cache, "external")

    if cache.external:
        split = layout.split(percentage=0.80)
        split.itemR(cache, "name", text="")
        split.itemR(cache, "index", text="")

        layout.itemL(text="File Path:")
        layout.itemR(cache, "filepath", text="")

        layout.itemL(text=cache.info)
    else:
        layout.itemR(cache, "name", text="")

        if not particles:
            row = layout.row()
            row.enabled = enabled
            row.itemR(cache, "start_frame")
            row.itemR(cache, "end_frame")

        row = layout.row()

        if cache.baked == True:
            row.itemO("ptcache.free_bake", text="Free Bake")
        else:
            row.item_booleanO("ptcache.bake", "bake", True, text="Bake")

        sub = row.row()
        sub.enabled = (cache.frames_skipped or cache.outdated) and enabled
        sub.itemO("ptcache.bake", "bake", False, text="Calculate to Current Frame")

        row = layout.row()
        row.enabled = enabled
        row.itemO("ptcache.bake_from_cache", text="Current Cache to Bake")
        if not smoke:
            row.itemR(cache, "step")

        if not smoke:
            row = layout.row()
            sub = row.row()
            sub.enabled = enabled
            sub.itemR(cache, "quick_cache")
            row.itemR(cache, "disk_cache")

        layout.itemL(text=cache.info)

        layout.itemS()

        row = layout.row()
        row.item_booleanO("ptcache.bake_all", "bake", True, text="Bake All Dynamics")
        row.itemO("ptcache.free_bake_all", text="Free All Bakes")
        layout.itemO("ptcache.bake_all", "bake", False, text="Update All Dynamics to current frame")


def effector_weights_ui(self, context, weights):
    layout = self.layout
    
    wide_ui = context.region.width > narrowui

    layout.itemR(weights, "group")

    split = layout.split()
    
    col = split.column()
    col.itemR(weights, "gravity", slider=True)
    
    if wide_ui:
        col = split.column()
    col.itemR(weights, "all", slider=True)

    layout.itemS()
    
    split = layout.split()

    col = split.column()
    col.itemR(weights, "force", slider=True)
    col.itemR(weights, "vortex", slider=True)
    col.itemR(weights, "magnetic", slider=True)
    col.itemR(weights, "wind", slider=True)
    col.itemR(weights, "curveguide", slider=True)
    col.itemR(weights, "texture", slider=True)

    if wide_ui:
        col = split.column()
    col.itemR(weights, "harmonic", slider=True)
    col.itemR(weights, "charge", slider=True)
    col.itemR(weights, "lennardjones", slider=True)
    col.itemR(weights, "turbulence", slider=True)
    col.itemR(weights, "drag", slider=True)
    col.itemR(weights, "boid", slider=True)


def basic_force_field_settings_ui(self, context, field):
    layout = self.layout
    
    wide_ui = context.region.width > narrowui
    
    split = layout.split()

    if not field or field.type == 'NONE':
        return

    col = split.column()

    if field.type == 'DRAG':
        col.itemR(field, "linear_drag", text="Linear")
    else:
        col.itemR(field, "strength")

    if field.type == 'TURBULENCE':
        col.itemR(field, "size")
        col.itemR(field, "flow")
    elif field.type == 'HARMONIC':
        col.itemR(field, "harmonic_damping", text="Damping")
    elif field.type == 'VORTEX' and field.shape != 'POINT':
        col.itemR(field, "inflow")
    elif field.type == 'DRAG':
        col.itemR(field, "quadratic_drag", text="Quadratic")
    else:
        col.itemR(field, "flow")
    
    if wide_ui:
        col = split.column()
    col.itemR(field, "noise")
    col.itemR(field, "seed")
    if field.type == 'TURBULENCE':
        col.itemR(field, "global_coordinates", text="Global")

    split = layout.split()

    col = split.column()
    col.itemL(text="Effect point:")
    col.itemR(field, "do_location")
    col.itemR(field, "do_rotation")
    
    if wide_ui:
        col = split.column()
    col.itemL(text="Collision:")
    col.itemR(field, "do_absorption")


def basic_force_field_falloff_ui(self, context, field):
    layout = self.layout
    
    wide_ui = context.region.width > narrowui
    
    # XXX: This doesn't update for some reason. 
    #if wide_ui:
    #    split = layout.split()
    #else:
    split = layout.split(percentage=0.35)

    if not field or field.type == 'NONE':
        return

    col = split.column()
    col.itemR(field, "z_direction", text="")
    col.itemR(field, "use_min_distance", text="Use Minimum")
    col.itemR(field, "use_max_distance", text="Use Maximum")

    if wide_ui:
        col = split.column()
    col.itemR(field, "falloff_power", text="Power")

    sub = col.column()
    sub.active = field.use_min_distance
    sub.itemR(field, "minimum_distance", text="Distance")

    sub = col.column()
    sub.active = field.use_max_distance
    sub.itemR(field, "maximum_distance", text="Distance")
