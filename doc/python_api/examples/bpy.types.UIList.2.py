"""
Advanced UIList Example - Filtering and Reordering
++++++++++++++++++++++++++++++++++++++++++++++++++

This script is an extended version of the ``UIList`` subclass used to show vertex groups. It is not used 'as is',
because iterating over all vertices in a 'draw' function is a very bad idea for UI performance! However, it's a good
example of how to create/use filtering/reordering callbacks.
"""
import bpy


class MESH_UL_vgroups_slow(bpy.types.UIList):
    # Constants (flags).
    # Be careful not to shadow FILTER_ITEM!
    VGROUP_EMPTY = 1 << 0

    # Custom properties, saved with `.blend` file.
    use_filter_empty: bpy.props.BoolProperty(
        name="Filter Empty",
        default=False,
        options=set(),
        description="Whether to filter empty vertex groups",
    )
    use_filter_empty_reverse: bpy.props.BoolProperty(
        name="Reverse Empty",
        default=False,
        options=set(),
        description="Reverse empty filtering",
    )
    use_filter_name_reverse: bpy.props.BoolProperty(
        name="Reverse Name",
        default=False,
        options=set(),
        description="Reverse name filtering",
    )
    use_filter_orderby_invert: bpy.props.BoolProperty(
        name="Reverse Order",
        default=False,
        options=set(),
        description="Reverse order filtering",
    )

    # This allows us to have mutually exclusive options, which are also all disable-able!
    def _gen_order_update(name1, name2):
        def _u(self, ctxt):
            if (getattr(self, name1)):
                setattr(self, name2, False)
        return _u
    use_order_name: bpy.props.BoolProperty(
        name="Name", default=False, options=set(),
        description="Sort groups by their name (case-insensitive)",
        update=_gen_order_update("use_order_name", "use_order_importance"),
    )
    use_order_importance: bpy.props.BoolProperty(
        name="Importance",
        default=False,
        options=set(),
        description="Sort groups by their average weight in the mesh",
        update=_gen_order_update("use_order_importance", "use_order_name"),
    )

    # Usual draw item function.
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index, flt_flag):
        # Just in case, we do not use it here!
        self.use_filter_invert = False

        # assert(isinstance(item, bpy.types.VertexGroup)
        vgroup = item
        # Here we use one feature of new filtering feature: it can pass data to draw_item, through flt_flag
        # parameter, which contains exactly what filter_items set in its filter list for this item!
        # In this case, we show empty groups grayed out.
        if flt_flag & self.VGROUP_EMPTY:
            col = layout.column()
            col.enabled = False
            col.alignment = 'LEFT'
            col.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
        else:
            layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
        icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
        layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)

    def draw_filter(self, context, layout):
        # Nothing much to say here, it's usual UI code...
        row = layout.row()

        subrow = row.row(align=True)
        subrow.prop(self, "filter_name", text="")
        icon = 'ZOOM_OUT' if self.use_filter_name_reverse else 'ZOOM_IN'
        subrow.prop(self, "use_filter_name_reverse", text="", icon=icon)

        subrow = row.row(align=True)
        subrow.prop(self, "use_filter_empty", toggle=True)
        icon = 'ZOOM_OUT' if self.use_filter_empty_reverse else 'ZOOM_IN'
        subrow.prop(self, "use_filter_empty_reverse", text="", icon=icon)

        row = layout.row(align=True)
        row.label(text="Order by:")
        row.prop(self, "use_order_name", toggle=True)
        row.prop(self, "use_order_importance", toggle=True)
        icon = 'TRIA_UP' if self.use_filter_orderby_invert else 'TRIA_DOWN'
        row.prop(self, "use_filter_orderby_invert", text="", icon=icon)

    def filter_items_empty_vgroups(self, context, vgroups):
        # This helper function checks vgroups to find out whether they are empty, and what's their average weights.
        # TODO: This should be RNA helper actually (a vgroup prop like `"raw_data: ((vidx, vweight), etc.)"`).
        #       Too slow for Python!
        obj_data = context.active_object.data
        ret = {vg.index: [True, 0.0] for vg in vgroups}
        if hasattr(obj_data, "vertices"):  # Mesh data
            if obj_data.is_editmode:
                import bmesh
                bm = bmesh.from_edit_mesh(obj_data)
                # only ever one deform weight layer
                dvert_lay = bm.verts.layers.deform.active
                fact = 1 / len(bm.verts)
                if dvert_lay:
                    for v in bm.verts:
                        for vg_idx, vg_weight in v[dvert_lay].items():
                            ret[vg_idx][0] = False
                            ret[vg_idx][1] += vg_weight * fact
            else:
                fact = 1 / len(obj_data.vertices)
                for v in obj_data.vertices:
                    for vg in v.groups:
                        ret[vg.group][0] = False
                        ret[vg.group][1] += vg.weight * fact
        elif hasattr(obj_data, "points"):  # Lattice data
            # XXX: no access to lattice edit-data?
            fact = 1 / len(obj_data.points)
            for v in obj_data.points:
                for vg in v.groups:
                    ret[vg.group][0] = False
                    ret[vg.group][1] += vg.weight * fact
        return ret

    def filter_items(self, context, data, propname):
        # This function gets the collection property (as the usual tuple (data, propname)), and must return two lists:
        # * The first one is for filtering, it must contain 32bit integers were self.bitflag_filter_item marks the
        #   matching item as filtered (i.e. to be shown). The upper 16 bits (including `self.bitflag_filter_item`) are
        #   reserved for internal use, the lower 16 bits are free for custom use. Here we use the first bit to mark
        #   VGROUP_EMPTY.
        # * The second one is for reordering, it must return a list containing the new indices of the items (which
        #   gives us a mapping `org_idx -> new_idx`).
        # Please note that the default UI_UL_list defines helper functions for common tasks (see its doc for more info).
        # If you do not make filtering and/or ordering, return empty list(s) (this will be more efficient than
        # returning full lists doing nothing!).
        vgroups = getattr(data, propname)
        helper_funcs = bpy.types.UI_UL_list

        # Default return values.
        flt_flags = []
        flt_neworder = []

        # Pre-compute of vertex-groups data, unfortunately this is CPU-intensive.
        vgroups_empty = self.filter_items_empty_vgroups(context, vgroups)

        # Filtering by name.
        if self.filter_name:
            flt_flags = helper_funcs.filter_items_by_name(self.filter_name, self.bitflag_filter_item, vgroups, "name",
                                                          reverse=self.use_filter_name_reverse)
        if not flt_flags:
            flt_flags = [self.bitflag_filter_item] * len(vgroups)

        # Filter by emptiness.
        for idx, vg in enumerate(vgroups):
            if vgroups_empty[vg.index][0]:
                flt_flags[idx] |= self.VGROUP_EMPTY
                if self.use_filter_empty and self.use_filter_empty_reverse:
                    flt_flags[idx] &= ~self.bitflag_filter_item
            elif self.use_filter_empty and not self.use_filter_empty_reverse:
                flt_flags[idx] &= ~self.bitflag_filter_item

        # Reorder by name or average weight.
        if self.use_order_name:
            flt_neworder = helper_funcs.sort_items_by_name(vgroups, "name")
            if self.use_filter_orderby_invert:
                flt_neworder.reverse()
        elif self.use_order_importance:
            _sort = [(idx, vgroups_empty[vg.index][1]) for idx, vg in enumerate(vgroups)]
            highest_first = not self.use_filter_orderby_invert
            flt_neworder = helper_funcs.sort_items_helper(_sort, lambda e: e[1], highest_first)

        return flt_flags, flt_neworder


# Minimal code to use above UIList...
class UIListPanelExample2(bpy.types.Panel):
    """Creates a Panel in the Object properties window"""
    bl_label = "UIList Example 2 Panel"
    bl_idname = "OBJECT_PT_ui_list_example_2"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        layout = self.layout
        obj = context.object

        # `template_list` now takes two new arguments.
        # The first one is the identifier of the registered UIList to use (if you want only the default list,
        # with no custom draw code, use "UI_UL_list").
        layout.template_list("MESH_UL_vgroups_slow", "", obj, "vertex_groups", obj.vertex_groups, "active_index")


def register():
    bpy.utils.register_class(MESH_UL_vgroups_slow)
    bpy.utils.register_class(UIListPanelExample2)


def unregister():
    bpy.utils.unregister_class(UIListPanelExample2)
    bpy.utils.unregister_class(MESH_UL_vgroups_slow)


if __name__ == "__main__":
    register()
