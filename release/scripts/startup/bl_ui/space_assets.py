# SPDX-License-Identifier: GPL-2.0-or-later

# <pep8 compliant>
import bpy
from bpy.types import Header, Menu, Panel, UIList


class ASSETBROWSER_HT_header(Header):
    bl_space_type = 'ASSET_BROWSER'

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        layout.template_header()

        ASSETBROWSER_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        layout.operator(
            "screen.region_toggle",
            text="",
            icon='PREFERENCES',
            depress=is_option_region_visible(context, space)
        ).region_type = 'UI'


def is_option_region_visible(context, space):
    for region in context.area.regions:
        if region.type == 'UI' and region.width <= 1:
            return False

    return True


class ASSETBROWSER_MT_editor_menus(Menu):
    bl_idname = "ASSETBROWSER_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout

        layout.menu("ASSETBROWSER_MT_view")
        layout.menu("ASSETBROWSER_MT_select")
        layout.menu("ASSETBROWSER_MT_edit")


class ASSETBROWSER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        layout.prop(st, "show_region_nav_bar", text="Navigation")

        layout.separator()

        layout.menu("INFO_MT_area")


class ASSETBROWSER_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout


class ASSETBROWSER_MT_edit(Menu):
    bl_label = "Edit"

    def draw(self, _context):
        layout = self.layout

        layout.operator("asset.catalog_undo", text="Undo")
        layout.operator("asset.catalog_redo", text="Redo")


class ASSETBROWSER_PT_metadata(Panel):
    bl_space_type = 'ASSET_BROWSER'
    bl_region_type = 'UI'
    bl_label = "Asset Metadata"
    bl_options = {'HIDE_HEADER'}
    bl_category = 'Metadata'

    def draw(self, context):
        layout = self.layout
        wm = context.window_manager
        asset_handle = context.asset_handle
        asset_file = asset_handle.file_data

        if asset_handle is None:
            layout.label(text="No active asset", icon='INFO')
            return

        asset_library_ref = context.asset_library_ref
        asset_lib_path = bpy.types.AssetHandle.get_full_library_path(asset_file, asset_library_ref)

        prefs = context.preferences
        show_asset_debug_info = prefs.view.show_developer_ui and prefs.experimental.show_asset_debug_info

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        if asset_handle.local_id:
            # If the active file is an ID, use its name directly so renaming is possible from right here.
            layout.prop(asset_handle.local_id, "name")

            if show_asset_debug_info:
                col = layout.column(align=True)
                col.label(text="Asset Catalog:")
                col.prop(asset_handle.local_id.asset_data, "catalog_id", text="UUID")
                col.prop(asset_handle.local_id.asset_data, "catalog_simple_name", text="Simple Name")
        else:
            layout.prop(asset_file, "name")

            if show_asset_debug_info:
                col = layout.column(align=True)
                col.enabled = False
                col.label(text="Asset Catalog:")
                col.prop(asset_file.asset_data, "catalog_id", text="UUID")
                col.prop(asset_file.asset_data, "catalog_simple_name", text="Simple Name")

        row = layout.row(align=True)
        row.prop(wm, "asset_path_dummy", text="Source")
        row.operator("asset.open_containing_blend_file", text="", icon='TOOL_SETTINGS')

        layout.prop(asset_file.asset_data, "description")
        layout.prop(asset_file.asset_data, "author")


class ASSETBROWSER_PT_metadata_preview(Panel):
    bl_space_type = 'ASSET_BROWSER'
    bl_region_type = 'UI'
    bl_label = "Preview"
    bl_category = 'Metadata'

    def draw(self, context):
        layout = self.layout
        asset_handle = context.asset_handle
        asset_file = asset_handle.file_data

        row = layout.row()
        box = row.box()
        box.template_icon(icon_value=asset_file.preview_icon_id, scale=5.0)

        col = row.column(align=True)
        col.operator("ed.lib_id_load_custom_preview", icon='FILEBROWSER', text="")
        col.separator()
        col.operator("ed.lib_id_generate_preview", icon='FILE_REFRESH', text="")
        col.menu("ASSETBROWSER_MT_metadata_preview_menu", icon='DOWNARROW_HLT', text="")


class ASSETBROWSER_MT_metadata_preview_menu(Menu):
    bl_label = "Preview"

    def draw(self, context):
        layout = self.layout
        layout.operator("ed.lib_id_generate_preview_from_object", text="Render Active Object")


class ASSETBROWSER_PT_metadata_tags(Panel):
    bl_space_type = 'ASSET_BROWSER'
    bl_region_type = 'UI'
    bl_label = "Tags"
    bl_category = 'Metadata'

    def draw(self, context):
        layout = self.layout
        asset = context.asset_handle
        asset_data = asset.file_data.asset_data

        row = layout.row()
        row.template_list("ASSETBROWSEROLD_UL_metadata_tags", "asset_tags", asset_data, "tags",
                          asset_data, "active_tag", rows=4)

        col = row.column(align=True)
        col.operator("asset.tag_add", icon='ADD', text="")
        col.operator("asset.tag_remove", icon='REMOVE', text="")


class ASSETBROWSER_UL_metadata_tags(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        tag = item

        row = layout.row(align=True)
        # Non-editable entries would show grayed-out, which is bad in this specific case, so switch to mere label.
        if tag.is_property_readonly("name"):
            row.label(text=tag.name, icon_value=icon)
        else:
            row.prop(tag, "name", text="", emboss=False, icon_value=icon)


classes = (
    ASSETBROWSER_HT_header,
    ASSETBROWSER_MT_editor_menus,
    ASSETBROWSER_MT_view,
    ASSETBROWSER_MT_select,
    ASSETBROWSER_MT_edit,
    ASSETBROWSER_PT_metadata,
    ASSETBROWSER_PT_metadata_preview,
    ASSETBROWSER_MT_metadata_preview_menu,
    ASSETBROWSER_PT_metadata_tags,
    ASSETBROWSER_UL_metadata_tags,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
