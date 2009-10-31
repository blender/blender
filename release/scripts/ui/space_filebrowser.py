# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

# <pep8 compliant>
import bpy


class FILEBROWSER_HT_header(bpy.types.Header):
    bl_space_type = 'FILE_BROWSER'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        params = st.params

        layout.template_header(menus=False)

        row = layout.row()
        row.itemS()

        row = layout.row(align=True)
        row.itemO("file.previous", text="", icon='ICON_BACK')
        row.itemO("file.next", text="", icon='ICON_FORWARD')
        row.itemO("file.parent", text="", icon='ICON_FILE_PARENT')
        row.itemO("file.refresh", text="", icon='ICON_FILE_REFRESH')

        row = layout.row()
        row.itemS()

        row = layout.row(align=True)
        row.itemO("file.directory_new", text="", icon='ICON_NEWFOLDER')

        layout.itemR(params, "display", expand=True, text="")
        layout.itemR(params, "sort", expand=True, text="")

        layout.itemR(params, "hide_dot", text="Hide Invisible")
        layout.itemR(params, "do_filter", text="", icon='ICON_FILTER')

        row = layout.row(align=True)
        row.active = params.do_filter

        row.itemR(params, "filter_folder", text="")
        row.itemR(params, "filter_blender", text="")
        row.itemR(params, "filter_image", text="")
        row.itemR(params, "filter_movie", text="")
        row.itemR(params, "filter_script", text="")
        row.itemR(params, "filter_font", text="")
        row.itemR(params, "filter_sound", text="")
        row.itemR(params, "filter_text", text="")

bpy.types.register(FILEBROWSER_HT_header)
