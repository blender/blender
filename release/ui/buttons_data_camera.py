
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"

	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'CAMERA')
		
class DATA_PT_cameralens(DataButtonsPanel):
	__idname__ = "DATA_PT_camera"
	__label__ = "Lens"

	def draw(self, context):
		cam = context.main.cameras[0]
		layout = self.layout

		if not cam:
			return
		
		layout.row()
		layout.itemR(cam, "type", expand=True)
		
		layout.row()
		if (cam.type == 'PERSP'):
			layout.itemR(cam, "lens_unit")
			if (cam.lens_unit == 'MILLIMETERS'):
				layout.itemR(cam, "lens", text="Angle")
			if (cam.lens_unit == 'DEGREES'):
				layout.itemR(cam, "angle")
		if (cam.type == 'ORTHO'):
			layout.itemR(cam, "ortho_scale")
		
		layout.column_flow()
		layout.itemL(text="Shift:")
		layout.itemR(cam, "shift_x", text="X")
		layout.itemR(cam, "shift_y", text="Y")
		layout.itemL(text="Clipping:")
		layout.itemR(cam, "clip_start", text="Start")
		layout.itemR(cam, "clip_end", text="End")
		
		layout.row()
		layout.itemR(cam, "dof_object")
		layout.itemR(cam, "dof_distance")
		
class DATA_PT_cameradisplay(DataButtonsPanel):
	__idname__ = "DATA_PT_cameradisplay"
	__label__ = "Display"
	
	def draw(self, context):
		cam = context.main.cameras[0]
		layout = self.layout

		if not cam:
			return
			
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemR(cam, "show_limits", text="Limits")
		sub.itemR(cam, "show_mist", text="Mist")
		sub.itemR(cam, "show_title_safe", text="Title Safe")
		sub.itemR(cam, "show_name", text="Name")
		
		sub = layout.sub(1)
		subsub = sub.box()
		subsub.column()
		subsub.itemR(cam, "show_passepartout", text="Passepartout")
		if (cam.show_passepartout):
			subsub.itemR(cam, "passepartout_alpha", text="Alpha")
		sub.row()
		sub.itemR(cam, "draw_size", text="Size")
		
bpy.types.register(DATA_PT_cameralens)
bpy.types.register(DATA_PT_cameradisplay)


