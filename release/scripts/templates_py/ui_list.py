import bpy


class MESH_UL_mylist(bpy.types.UIList):
    # Constants (flags)
    # Be careful not to shadow FILTER_ITEM (i.e. UIList().bitflag_filter_item)!
    # E.g. VGROUP_EMPTY = 1 << 0

    # Custom properties, saved with .blend file. E.g.
    # use_filter_empty = bpy.props.BoolProperty(name="Filter Empty", default=False, options=set(),
    #                                           description="Whether to filter empty vertex groups")

    # Called for each drawn item.
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index, flt_flag):
        # 'DEFAULT' and 'COMPACT' layout types should usually use the same draw code.
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            pass
        # 'GRID' layout type should be as compact as possible (typically a single icon!).
        elif self.layout_type in {'GRID'}:
            pass

    # Called once to draw filtering/reordering options.
    def draw_filter(self, context, layout):
        # Nothing much to say here, it's usual UI code...
        pass

    # Called once to filter/reorder items.
    def filter_items(self, context, data, propname):
        # This function gets the collection property (as the usual tuple (data, propname)), and must return two lists:
        # * The first one is for filtering, it must contain 32bit integers were self.bitflag_filter_item marks the
        #   matching item as filtered (i.e. to be shown), and 31 other bits are free for custom needs. Here we use the
        #   first one to mark VGROUP_EMPTY.
        # * The second one is for reordering, it must return a list containing the new indices of the items (which
        #   gives us a mapping org_idx -> new_idx).
        # Please note that the default UI_UL_list defines helper functions for common tasks (see its doc for more info).
        # If you do not make filtering and/or ordering, return empty list(s) (this will be more efficient than
        # returning full lists doing nothing!).

        # Default return values.
        flt_flags = []
        flt_neworder = []

        # Do filtering/reordering here...

        return flt_flags, flt_neworder
