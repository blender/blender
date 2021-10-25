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
from bpy.types import Header, Panel, Menu


class FILEBROWSER_HT_header(Header):
    bl_space_type = 'FILE_BROWSER'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        layout.template_header()

        row = layout.row()
        row.separator()

        row = layout.row(align=True)
        row.operator("file.previous", text="", icon='BACK')
        row.operator("file.next", text="", icon='FORWARD')
        row.operator("file.parent", text="", icon='FILE_PARENT')
        row.operator("file.refresh", text="", icon='FILE_REFRESH')

        layout.separator()
        layout.operator_context = 'EXEC_DEFAULT'
        layout.operator("file.directory_new", icon='NEWFOLDER', text="")
        layout.separator()

        layout.operator_context = 'INVOKE_DEFAULT'
        params = st.params

        # can be None when save/reload with a file selector open
        if params:
            is_lib_browser = params.use_library_browsing

            layout.prop(params, "recursion_level", text="")

            layout.prop(params, "display_type", expand=True, text="")

            layout.prop(params, "display_size", text="")

            layout.prop(params, "sort_method", expand=True, text="")

            layout.prop(params, "show_hidden", text="", icon='FILE_HIDDEN')
            layout.prop(params, "use_filter", text="", icon='FILTER')

            row = layout.row(align=True)
            row.active = params.use_filter

            row.prop(params, "use_filter_folder", text="")

            if params.filter_glob:
                # if st.active_operator and hasattr(st.active_operator, "filter_glob"):
                #     row.prop(params, "filter_glob", text="")
                row.label(params.filter_glob)
            else:
                row.prop(params, "use_filter_blender", text="")
                row.prop(params, "use_filter_backup", text="")
                row.prop(params, "use_filter_image", text="")
                row.prop(params, "use_filter_movie", text="")
                row.prop(params, "use_filter_script", text="")
                row.prop(params, "use_filter_font", text="")
                row.prop(params, "use_filter_sound", text="")
                row.prop(params, "use_filter_text", text="")

            if is_lib_browser:
                row.prop(params, "use_filter_blendid", text="")
                if params.use_filter_blendid:
                    row.separator()
                    row.prop(params, "filter_id_category", text="")

            row.separator()
            row.prop(params, "filter_search", text="", icon='VIEWZOOM')

        layout.template_running_jobs()


class FILEBROWSER_UL_dir(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        direntry = item
        # space = context.space_data
        icon = 'NONE'
        if active_propname == "system_folders_active":
            icon = 'DISK_DRIVE'
        if active_propname == "system_bookmarks_active":
            icon = 'BOOKMARKS'
        if active_propname == "bookmarks_active":
            icon = 'BOOKMARKS'
        if active_propname == "recent_folders_active":
            icon = 'FILE_FOLDER'

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)
            row.enabled = direntry.is_valid
            # Non-editable entries would show grayed-out, which is bad in this specific case, so switch to mere label.
            if direntry.is_property_readonly("name"):
                row.label(text=direntry.name, icon=icon)
            else:
                row.prop(direntry, "name", text="", emboss=False, icon=icon)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.prop(direntry, "path", text="")


class FILEBROWSER_PT_system_folders(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "System"

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.system_folders:
            row = layout.row()
            row.template_list("FILEBROWSER_UL_dir", "system_folders", space, "system_folders",
                              space, "system_folders_active", item_dyntip_propname="path", rows=1, maxrows=10)


class FILEBROWSER_PT_system_bookmarks(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "System Bookmarks"

    @classmethod
    def poll(cls, context):
        return not context.user_preferences.filepaths.hide_system_bookmarks

    def draw(self, context):
        layout = self.layout
        space = context.space_data

        if space.system_bookmarks:
            row = layout.row()
            row.template_list("FILEBROWSER_UL_dir", "system_bookmarks", space, "system_bookmarks",
                              space, "system_bookmarks_active", item_dyntip_propname="path", rows=1, maxrows=10)


class FILEBROWSER_MT_bookmarks_specials(Menu):
    bl_label = "Bookmarks Specials"

    def draw(self, context):
        layout = self.layout
        layout.operator("file.bookmark_cleanup", icon='X', text="Cleanup")

        layout.separator()
        layout.operator("file.bookmark_move", icon='TRIA_UP_BAR', text="Move To Top").direction = 'TOP'
        layout.operator("file.bookmark_move", icon='TRIA_DOWN_BAR', text="Move To Bottom").direction = 'BOTTOM'


class FILEBROWSER_PT_bookmarks(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "Bookmarks"

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
            col.operator("file.bookmark_add", icon='ZOOMIN', text="")
            col.operator("file.bookmark_delete", icon='ZOOMOUT', text="")
            col.menu("FILEBROWSER_MT_bookmarks_specials", icon='DOWNARROW_HLT', text="")

            if num_rows > 1:
                col.separator()
                col.operator("file.bookmark_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("file.bookmark_move", icon='TRIA_DOWN', text="").direction = 'DOWN'
        else:
            layout.operator("file.bookmark_add", icon='ZOOMIN')


class FILEBROWSER_PT_recent_folders(Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOLS'
    bl_category = "Bookmarks"
    bl_label = "Recent"

    @classmethod
    def poll(cls, context):
        return not context.user_preferences.filepaths.hide_recent_locations

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
        return context.space_data.params.use_library_browsing

    def draw(self, context):
        layout = self.layout
        space = context.space_data
        params = space.params

        if params and params.use_library_browsing:
            layout.prop(params, "use_filter_blendid")
            if params.use_filter_blendid:
                layout.separator()
                col = layout.column()
                col.prop(params, "filter_id")


classes = (
    FILEBROWSER_HT_header,
    FILEBROWSER_UL_dir,
    FILEBROWSER_PT_system_folders,
    FILEBROWSER_PT_system_bookmarks,
    FILEBROWSER_MT_bookmarks_specials,
    FILEBROWSER_PT_bookmarks,
    FILEBROWSER_PT_recent_folders,
    FILEBROWSER_PT_advanced_filter,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
