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

from bpy.types import Header, Panel, Menu, UIList

from bpy_extras import (
    asset_utils,
)


class FILEBROWSER_HT_header(Header):
    bl_space_type = 'FILE_BROWSER'

    def draw_asset_browser_buttons(self, context):
        layout = self.layout

        space_data = context.space_data
        params = space_data.params

        row = layout.row(align=True)
        row.prop(params, "asset_library", text="")
        # External libraries don't auto-refresh, add refresh button.
        if params.asset_library != 'LOCAL':
            row.operator("file.refresh", text="", icon='FILE_REFRESH')

        layout.separator_spacer()

        # Uses prop_with_popover() as popover() only adds the triangle icon in headers.
        layout.prop_with_popover(
            params,
            "display_type",
            panel="FILEBROWSER_PT_display",
            text="",
            icon_only=True,
        )
        layout.prop_with_popover(
            params,
            "display_type",
            panel="FILEBROWSER_PT_filter",
            text="",
            icon='FILTER',
            icon_only=True,
        )

        layout.prop(params, "filter_search", text="", icon='VIEWZOOM')

        layout.operator(
            "screen.region_toggle",
            text="",
            icon='PREFERENCES',
            depress=is_option_region_visible(context, space_data)
        ).region_type = 'TOOL_PROPS'

    def draw(self, context):
        from bpy_extras.asset_utils import SpaceAssetInfo

        layout = self.layout

        space_data = context.space_data

        if space_data.active_operator is None:
            layout.template_header()

        FILEBROWSER_MT_editor_menus.draw_collapsible(context, layout)

        if SpaceAssetInfo.is_asset_browser(space_data):
            layout.separator()
            self.draw_asset_browser_buttons(context)
        else:
            layout.separator_spacer()

        if not context.screen.show_statusbar:
            layout.template_running_jobs()


class FILEBROWSER_PT_display(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'HEADER'
    bl_label = "Display Settings"  # Shows as tooltip in popover
    bl_ui_units_x = 10

    @classmethod
    def poll(cls, context):
        # can be None when save/reload with a file selector open
        return context.space_data.params is not None

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        params = space.params

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        if params.display_type == 'THUMBNAIL':
            layout.prop(params, "display_size", text="Size")
        else:
            col = layout.column(heading="Columns", align=True)
            col.prop(params, "show_details_size", text="Size")
            col.prop(params, "show_details_datetime", text="Date")

        layout.prop(params, "recursion_level", text="Recursions")

        layout.column().prop(params, "sort_method", text="Sort By", expand=True)
        layout.prop(params, "use_sort_invert")


class FILEBROWSER_PT_filter(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'HEADER'
    bl_label = "Filter Settings"  # Shows as tooltip in popover
    bl_ui_units_x = 8

    @classmethod
    def poll(cls, context):
        # can be None when save/reload with a file selector open
        return context.space_data.params is not None

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        params = space.params
        is_lib_browser = params.use_library_browsing

        col = layout.column()
        col.active = params.use_filter

        row = col.row()
        row.label(icon='FILE_FOLDER')
        row.prop(params, "use_filter_folder", text="Folders", toggle=False)

        if params.filter_glob:
            col.label(text=params.filter_glob)
        else:
            row = col.row()
            row.label(icon='FILE_BLEND')
            row.prop(params, "use_filter_blender",
                     text=".blend Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_BACKUP')
            row.prop(params, "use_filter_backup",
                     text="Backup .blend Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_IMAGE')
            row.prop(params, "use_filter_image", text="Image Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_MOVIE')
            row.prop(params, "use_filter_movie", text="Movie Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_SCRIPT')
            row.prop(params, "use_filter_script",
                     text="Script Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_FONT')
            row.prop(params, "use_filter_font", text="Font Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_SOUND')
            row.prop(params, "use_filter_sound", text="Sound Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_TEXT')
            row.prop(params, "use_filter_text", text="Text Files", toggle=False)
            row = col.row()
            row.label(icon='FILE_VOLUME')
            row.prop(params, "use_filter_volume", text="Volume Files", toggle=False)

        col.separator()

        if is_lib_browser:
            row = col.row()
            row.label(icon='BLANK1')  # Indentation
            row.prop(params, "use_filter_blendid",
                     text="Blender IDs", toggle=False)
            if params.use_filter_blendid:
                row = col.row()
                row.label(icon='BLANK1')  # Indentation

                sub = row.column(align=True)

                if context.preferences.experimental.use_asset_browser:
                    sub.prop(params, "use_filter_asset_only")

                filter_id = params.filter_id
                for identifier in dir(filter_id):
                    if identifier.startswith("category_"):
                        sub.prop(filter_id, identifier, toggle=True)

                col.separator()

        layout.prop(params, "show_hidden")


def panel_poll_is_upper_region(region):
    # The upper region is left-aligned, the lower is split into it then.
    # Note that after "Flip Regions" it's right-aligned.
    return region.alignment in {'LEFT', 'RIGHT'}


def panel_poll_is_asset_browsing(context):
    from bpy_extras.asset_utils import SpaceAssetInfo
    return SpaceAssetInfo.is_asset_browser_poll(context)


class FILEBROWSER_UL_dir(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        direntry = item
        # space = context.space_data

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            row.enabled = direntry.is_valid
            # Non-editable entries would show grayed-out, which is bad in this specific case, so switch to mere label.
            if direntry.is_property_readonly("name"):
                row.label(text=direntry.name, icon_value=icon)
            else:
                row.prop(direntry, "name", text="", emboss=False, icon_value=icon)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.prop(direntry, "path", text="")


class FILEBROWSER_PT_bookmarks_volumes(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "Volumes"

    @classmethod
    def poll(cls, context):
        return panel_poll_is_upper_region(context.region) and not panel_poll_is_asset_browsing(context)

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.system_folders:
            row = layout.row()
            row.template_list("FILEBROWSER_UL_dir", "system_folders", space, "system_folders",
                              space, "system_folders_active", item_dyntip_propname="path", rows=1, maxrows=10)


class FILEBROWSER_PT_bookmarks_system(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "System"

    @classmethod
    def poll(cls, context):
        return (
            not context.preferences.filepaths.hide_system_bookmarks and
            panel_poll_is_upper_region(context.region) and
            not panel_poll_is_asset_browsing(context)
        )

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.system_bookmarks:
            row = layout.row()
            row.template_list("FILEBROWSER_UL_dir", "system_bookmarks", space, "system_bookmarks",
                              space, "system_bookmarks_active", item_dyntip_propname="path", rows=1, maxrows=10)


class FILEBROWSER_MT_bookmarks_context_menu(Menu):
    bl_label = "Bookmarks Specials"

    def draw(self, _context):
        layout = self.layout
        layout.operator("file.bookmark_cleanup", icon='X', text="Cleanup")

        layout.separator()
        layout.operator("file.bookmark_move", icon='TRIA_UP_BAR',
                        text="Move to Top").direction = 'TOP'
        layout.operator("file.bookmark_move", icon='TRIA_DOWN_BAR',
                        text="Move to Bottom").direction = 'BOTTOM'


class FILEBROWSER_PT_bookmarks_favorites(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "Favorites"

    @classmethod
    def poll(cls, context):
        return (
            panel_poll_is_upper_region(context.region) and
            not panel_poll_is_asset_browsing(context)
        )

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.bookmarks:
            row = layout.row()
            num_rows = len(space.bookmarks)
            row.template_list("FILEBROWSER_UL_dir", "bookmarks", space, "bookmarks",
                              space, "bookmarks_active", item_dyntip_propname="path",
                              rows=(2 if num_rows < 2 else 4), maxrows=10)

            col = row.column(align=True)
            col.operator("file.bookmark_add", icon='ADD', text="")
            col.operator("file.bookmark_delete", icon='REMOVE', text="")
            col.menu("FILEBROWSER_MT_bookmarks_context_menu",
                     icon='DOWNARROW_HLT', text="")

            if num_rows > 1:
                col.separator()
                col.operator("file.bookmark_move", icon='TRIA_UP',
                             text="").direction = 'UP'
                col.operator("file.bookmark_move", icon='TRIA_DOWN',
                             text="").direction = 'DOWN'
        else:
            layout.operator("file.bookmark_add", icon='ADD')


class FILEBROWSER_PT_bookmarks_recents(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "Recent"

    @classmethod
    def poll(cls, context):
        return (
            not context.preferences.filepaths.hide_recent_locations and
            panel_poll_is_upper_region(context.region) and
            not panel_poll_is_asset_browsing(context)
        )

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.recent_folders:
            row = layout.row()
            row.template_list("FILEBROWSER_UL_dir", "recent_folders", space, "recent_folders",
                              space, "recent_folders_active", item_dyntip_propname="path", rows=1, maxrows=10)

            col = row.column(align=True)
            col.operator("file.reset_recent", icon='X', text="")


class FILEBROWSER_PT_advanced_filter(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Filter"
    bl_label = "Advanced Filter"

    @classmethod
    def poll(cls, context):
        # only useful in append/link (library) context currently...
        return (
            context.space_data.params.use_library_browsing and
            panel_poll_is_upper_region(context.region) and
            not panel_poll_is_asset_browsing(context)
        )

    def draw(self, context):
        layout = self.layout
        space = context.space_data
        params = space.params

        if params and params.use_library_browsing:
            layout.prop(params, "use_filter_blendid")
            if params.use_filter_blendid:
                layout.separator()
                col = layout.column(align=True)

                if context.preferences.experimental.use_asset_browser:
                    col.prop(params, "use_filter_asset_only")

                filter_id = params.filter_id
                for identifier in dir(filter_id):
                    if identifier.startswith("filter_"):
                        col.prop(filter_id, identifier, toggle=True)


def is_option_region_visible(context, space):
    if not space.active_operator:
        return False

    for region in context.area.regions:
        if region.type == 'TOOL_PROPS' and region.width <= 1:
            return False

    return True


class FILEBROWSER_PT_directory_path(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'UI'
    bl_label = "Directory Path"
    bl_category = "Attributes"
    bl_options = {'HIDE_HEADER'}

    def is_header_visible(self, context):
        for region in context.area.regions:
            if region.type == 'HEADER' and region.height <= 1:
                return False

        return True

    def draw(self, context):
        layout = self.layout
        space = context.space_data
        params = space.params

        layout.scale_x = 1.3
        layout.scale_y = 1.3

        row = layout.row()
        flow = row.grid_flow(row_major=True, columns=0, even_columns=False, even_rows=False, align=False)

        subrow = flow.row()

        subsubrow = subrow.row(align=True)
        subsubrow.operator("file.previous", text="", icon='BACK')
        subsubrow.operator("file.next", text="", icon='FORWARD')
        subsubrow.operator("file.parent", text="", icon='FILE_PARENT')
        subsubrow.operator("file.refresh", text="", icon='FILE_REFRESH')

        subsubrow = subrow.row()
        subsubrow.operator_context = 'EXEC_DEFAULT'
        subsubrow.operator("file.directory_new", icon='NEWFOLDER', text="")

        subrow.template_file_select_path(params)

        subrow = flow.row()

        subsubrow = subrow.row()
        subsubrow.scale_x = 0.6
        subsubrow.prop(params, "filter_search", text="", icon='VIEWZOOM')

        subsubrow = subrow.row(align=True)
        subsubrow.prop(params, "display_type", expand=True, icon_only=True)
        subsubrow.popover("FILEBROWSER_PT_display", text="")

        subsubrow = subrow.row(align=True)
        subsubrow.prop(params, "use_filter", toggle=True, icon='FILTER', icon_only=True)
        subsubrow.popover("FILEBROWSER_PT_filter", text="")

        if space.active_operator:
            subrow.operator(
                "screen.region_toggle",
                text="",
                icon='PREFERENCES',
                depress=is_option_region_visible(context, space)
            ).region_type = 'TOOL_PROPS'


class FILEBROWSER_MT_editor_menus(Menu):
    bl_idname = "FILEBROWSER_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout

        layout.menu("FILEBROWSER_MT_view")
        layout.menu("FILEBROWSER_MT_select")


class FILEBROWSER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        params = st.params

        layout.prop(st, "show_region_toolbar", text="Source List")
        layout.prop(st, "show_region_ui", text="File Path")
        layout.operator("file.view_selected")

        layout.separator()

        layout.prop_menu_enum(params, "display_size")
        layout.prop_menu_enum(params, "recursion_level")

        layout.separator()

        layout.menu("INFO_MT_area")


class FILEBROWSER_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("file.select_all", text="All").action = 'SELECT'
        layout.operator("file.select_all", text="None").action = 'DESELECT'
        layout.operator("file.select_all", text="Inverse").action = 'INVERT'

        layout.separator()

        layout.operator("file.select_box")


class FILEBROWSER_MT_context_menu(Menu):
    bl_label = "Files Context Menu"

    def draw(self, context):
        layout = self.layout
        st = context.space_data
        params = st.params

        layout.operator("file.previous", text="Back")
        layout.operator("file.next", text="Forward")
        layout.operator("file.parent", text="Go to Parent")
        layout.operator("file.refresh", text="Refresh")

        layout.separator()

        layout.operator("file.filenum", text="Increase Number",
                        icon='ADD').increment = 1
        layout.operator("file.filenum", text="Decrease Number",
                        icon='REMOVE').increment = -1

        layout.separator()

        layout.operator("file.rename", text="Rename")
        sub = layout.row()
        sub.operator_context = 'EXEC_DEFAULT'
        sub.operator("file.delete", text="Delete")

        layout.separator()

        sub = layout.row()
        sub.operator_context = 'EXEC_DEFAULT'
        sub.operator("file.directory_new", text="New Folder")
        layout.operator("file.bookmark_add", text="Add Bookmark")

        layout.separator()

        layout.prop_menu_enum(params, "display_type")
        if params.display_type == 'THUMBNAIL':
            layout.prop_menu_enum(params, "display_size")
        layout.prop_menu_enum(params, "recursion_level", text="Recursions")
        layout.prop_menu_enum(params, "sort_method")


class ASSETBROWSER_PT_navigation_bar(asset_utils.AssetBrowserPanel, Panel):
    bl_label = "Asset Navigation"
    bl_region_type = 'TOOLS'
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        space_file = context.space_data

        col = layout.column()

        col.scale_x = 1.3
        col.scale_y = 1.3
        col.prop(space_file.params, "asset_category", expand=True)


class ASSETBROWSER_PT_metadata(asset_utils.AssetBrowserPanel, Panel):
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Asset Metadata"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        active_file = context.active_file
        active_asset = asset_utils.SpaceAssetInfo.get_active_asset(context)

        if not active_file or not active_asset:
            layout.label(text="No asset selected", icon='INFO')
            return

        # If the active file is an ID, use its name directly so renaming is possible from right here.
        layout.prop(context.id if context.id is not None else active_file, "name", text="")


class ASSETBROWSER_PT_metadata_preview(asset_utils.AssetMetaDataPanel, Panel):
    bl_label = "Preview"

    def draw(self, context):
        layout = self.layout
        active_file = context.active_file

        row = layout.row()
        box = row.box()
        box.template_icon(icon_value=active_file.preview_icon_id, scale=5.0)
        if bpy.ops.ed.lib_id_load_custom_preview.poll():
            col = row.column(align=True)
            col.operator("ed.lib_id_load_custom_preview", icon='FILEBROWSER', text="")
            col.separator()
            col.operator("ed.lib_id_generate_preview", icon='FILE_REFRESH', text="")


class ASSETBROWSER_PT_metadata_details(asset_utils.AssetMetaDataPanel, Panel):
    bl_label = "Details"

    def draw(self, context):
        layout = self.layout
        active_asset = asset_utils.SpaceAssetInfo.get_active_asset(context)

        layout.use_property_split = True

        if active_asset:
            layout.prop(active_asset, "description")


class ASSETBROWSER_PT_metadata_tags(asset_utils.AssetMetaDataPanel, Panel):
    bl_label = "Tags"

    def draw(self, context):
        layout = self.layout
        asset_data = asset_utils.SpaceAssetInfo.get_active_asset(context)

        row = layout.row()
        row.template_list("ASSETBROWSER_UL_metadata_tags", "asset_tags", asset_data, "tags",
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
    FILEBROWSER_HT_header,
    FILEBROWSER_PT_display,
    FILEBROWSER_PT_filter,
    FILEBROWSER_UL_dir,
    FILEBROWSER_PT_bookmarks_volumes,
    FILEBROWSER_PT_bookmarks_system,
    FILEBROWSER_MT_bookmarks_context_menu,
    FILEBROWSER_PT_bookmarks_favorites,
    FILEBROWSER_PT_bookmarks_recents,
    FILEBROWSER_PT_advanced_filter,
    FILEBROWSER_PT_directory_path,
    FILEBROWSER_MT_editor_menus,
    FILEBROWSER_MT_view,
    FILEBROWSER_MT_select,
    FILEBROWSER_MT_context_menu,
    ASSETBROWSER_PT_navigation_bar,
    ASSETBROWSER_PT_metadata,
    ASSETBROWSER_PT_metadata_preview,
    ASSETBROWSER_PT_metadata_details,
    ASSETBROWSER_PT_metadata_tags,
    ASSETBROWSER_UL_metadata_tags,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
