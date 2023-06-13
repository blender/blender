# SPDX-License-Identifier: GPL-2.0-or-later
from bpy.types import Header, Panel


class PROPERTIES_HT_header(Header):
    bl_space_type = 'PROPERTIES'

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        region = context.region
        ui_scale = context.preferences.system.ui_scale

        layout.template_header()

        layout.separator_spacer()

        # The following is an ugly attempt to make the search button center-align better visually.
        # A dummy icon is inserted that has to be scaled as the available width changes.
        content_size_est = 160 * ui_scale
        layout_scale = min(1, max(0, (region.width / content_size_est) - 1))
        if layout_scale > 0:
            row = layout.row()
            row.scale_x = layout_scale
            row.label(icon='BLANK1')

        layout.prop(view, "search_filter", icon='VIEWZOOM', text="")

        layout.separator_spacer()

        layout.popover(panel="PROPERTIES_PT_options", text="")


class PROPERTIES_PT_navigation_bar(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'NAVIGATION_BAR'
    bl_label = "Navigation Bar"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        layout.scale_x = 1.4
        layout.scale_y = 1.4
        if view.search_filter:
            layout.prop_tabs_enum(
                view, "context", data_highlight=view,
                property_highlight="tab_search_results", icon_only=True,
            )
        else:
            layout.prop_tabs_enum(view, "context", icon_only=True)


class PROPERTIES_PT_options(Panel):
    """Show options for the properties editor"""
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'HEADER'
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        space = context.space_data

        col = layout.column()
        col.label(text="Sync with Outliner")
        col.row().prop(space, "outliner_sync", expand=True)


classes = (
    PROPERTIES_HT_header,
    PROPERTIES_PT_navigation_bar,
    PROPERTIES_PT_options,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
