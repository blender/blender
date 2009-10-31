# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

# <pep8 compliant>
import bpy


def point_cache_ui(self, cache, enabled, particles, smoke):
    layout = self.layout
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


def effector_weights_ui(self, weights):
        layout = self.layout

        layout.itemR(weights, "group")

        split = layout.split()
        split.itemR(weights, "gravity", slider=True)
        split.itemR(weights, "all", slider=True)

        layout.itemS()

        flow = layout.column_flow()
        flow.itemR(weights, "force", slider=True)
        flow.itemR(weights, "vortex", slider=True)
        flow.itemR(weights, "magnetic", slider=True)
        flow.itemR(weights, "wind", slider=True)
        flow.itemR(weights, "curveguide", slider=True)
        flow.itemR(weights, "texture", slider=True)
        flow.itemR(weights, "harmonic", slider=True)
        flow.itemR(weights, "charge", slider=True)
        flow.itemR(weights, "lennardjones", slider=True)
        flow.itemR(weights, "turbulence", slider=True)
        flow.itemR(weights, "drag", slider=True)
        flow.itemR(weights, "boid", slider=True)


def basic_force_field_settings_ui(self, field):
    layout = self.layout
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

    sub = split.column()
    sub.itemL(text="Collision:")
    sub.itemR(field, "do_absorption")


def basic_force_field_falloff_ui(self, field):
    layout = self.layout
    split = layout.split(percentage=0.35)

    if not field or field.type == 'NONE':
        return

    col = split.column()
    col.itemR(field, "z_direction", text="")
    col.itemR(field, "use_min_distance", text="Use Minimum")
    col.itemR(field, "use_max_distance", text="Use Maximum")

    col = split.column()
    col.itemR(field, "falloff_power", text="Power")

    sub = col.column()
    sub.active = field.use_min_distance
    sub.itemR(field, "minimum_distance", text="Distance")

    sub = col.column()
    sub.active = field.use_max_distance
    sub.itemR(field, "maximum_distance", text="Distance")
