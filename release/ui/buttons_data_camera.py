
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "data"

	def poll(self, context):
		return (context.camera)
		
class DATA_PT_context_camera(DataButtonsPanel):
	__show_header__ = False
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		cam = context.camera
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif cam:
			split.template_ID(space, "pin_id")
			split.itemS()

class DATA_PT_camera(DataButtonsPanel):
	__label__ = "Lens"

	def draw(self, context):
		layout = self.layout
		
		cam = context.camera

		layout.itemR(cam, "type", expand=True)
			
		row = layout.row(align=True)
		if cam.type == 'PERSP':
			row.itemR(cam, "lens_unit", text="")
			if cam.lens_unit == 'MILLIMETERS':
				row.itemR(cam, "lens", text="Angle")
			elif cam.lens_unit == 'DEGREES':
				row.itemR(cam, "angle")

		elif cam.type == 'ORTHO':
			row.itemR(cam, "ortho_scale")

		layout.itemR(cam, "panorama")
				
		split = layout.split()
			
		col = split.column(align=True)
		col.itemL(text="Shift:")
		col.itemR(cam, "shift_x", text="X")
		col.itemR(cam, "shift_y", text="Y")
			
		col = split.column(align=True)
		col.itemL(text="Clipping:")
		col.itemR(cam, "clip_start", text="Start")
		col.itemR(cam, "clip_end", text="End")
			
		layout.itemL(text="Depth of Field:")
		
		row = layout.row()
		row.itemR(cam, "dof_object", text="")
		row.itemR(cam, "dof_distance", text="Distance")
		
class DATA_PT_camera_display(DataButtonsPanel):
	__label__ = "Display"

	def draw(self, context):
		layout = self.layout
		
		cam = context.camera

		split = layout.split()
		
		col = split.column()
		col.itemR(cam, "show_limits", text="Limits")
		col.itemR(cam, "show_mist", text="Mist")
		col.itemR(cam, "show_title_safe", text="Title Safe")
		col.itemR(cam, "show_name", text="Name")
			
		col = split.column()
		col.itemR(cam, "show_passepartout", text="Passepartout")
		sub = col.column()
		sub.active = cam.show_passepartout
		sub.itemR(cam, "passepartout_alpha", text="Alpha", slider=True)
		col.itemR(cam, "draw_size", text="Size")
		
bpy.types.register(DATA_PT_context_camera)
bpy.types.register(DATA_PT_camera)
bpy.types.register(DATA_PT_camera_display)
