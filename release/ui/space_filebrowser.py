
import bpy


class FILEBROWSER_HT_header(bpy.types.Header):
	__space_type__ = "FILE_BROWSER"
	__idname__ = "FILEBROWSER_HT_header"

	def draw(self, context):
		st = context.space_data
		layout = self.layout
		
		params = st.params 
		layout.template_header(context)

		row = layout.row(align=True)
		row.itemO("FILE_OT_parent", text="", icon='ICON_FILE_PARENT')
		row.itemO("FILE_OT_refresh", text="", icon='ICON_FILE_REFRESH')

		layout.itemR(params, "display", expand=True, text="")
		layout.itemR(params, "sort", expand=True, text="")
		
		layout.itemR(params, "hide_dot")
		layout.itemR(params, "do_filter")
		
		row = layout.row(align=True)
		row.itemR(params, "filter_folder", text="");
		row.itemR(params, "filter_blender", text="");
		row.itemR(params, "filter_image", text="");
		row.itemR(params, "filter_movie", text="");
		row.itemR(params, "filter_script", text="");
		row.itemR(params, "filter_font", text="");
		row.itemR(params, "filter_sound", text="");
		row.itemR(params, "filter_text", text="");

		if params.do_filter:
			row.active = True
		else: 
			row.active = False
			
bpy.types.register(FILEBROWSER_HT_header)


