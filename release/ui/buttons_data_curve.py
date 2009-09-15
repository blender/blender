
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "PROPERTIES"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.object and context.object.type == 'CURVE' and context.curve)

class DATA_PT_context_curve(DataButtonsPanel):
	__show_header__ = False
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		curve = context.curve
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif curve:
			split.template_ID(space, "pin_id")
			split.itemS()

class DATA_PT_shape_curve(DataButtonsPanel):
	__label__ = "Shape"
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		curve = context.curve
		space = context.space_data

		if curve:
			layout.itemR(curve, "curve_2d")			
							
			split = layout.split()
		
			col = split.column()
			colsub = col.column()
			colsub.active = curve.curve_2d
			colsub.itemL(text="Caps:")
			colsub.itemR(curve, "front")
			colsub.itemR(curve, "back")
			
			col.itemL(text="Textures:")
#			col.itemR(curve, "uv_orco")
			col.itemR(curve, "auto_texspace")
			
			sub = split.column()	
			sub.itemL(text="Resolution:")
			sub.itemR(curve, "resolution_u", text="Preview U")
			sub.itemR(curve, "resolution_v", text="Preview V")
			sub.itemR(curve, "render_resolution_u", text="Render U")
			sub.itemR(curve, "render_resolution_v", text="Render V")

#			sub.itemL(text="Display:")
#			sub.itemL(text="HANDLES")
#			sub.itemL(text="NORMALS")
#			sub.itemR(curve, "vertex_normal_flip")

class DATA_PT_geometry_curve(DataButtonsPanel):
	__label__ = "Geometry "

	def draw(self, context):
		layout = self.layout
		
		curve = context.curve

		split = layout.split()
	
		sub = split.column()
		sub.itemL(text="Modification:")
		sub.itemR(curve, "width")
		sub.itemR(curve, "extrude")
		sub.itemR(curve, "taper_object", icon="ICON_OUTLINER_OB_CURVE")
		
		sub = split.column()
		sub.itemL(text="Bevel:")
		sub.itemR(curve, "bevel_depth", text="Depth")
		sub.itemR(curve, "bevel_resolution", text="Resolution")
		sub.itemR(curve, "bevel_object", icon="ICON_OUTLINER_OB_CURVE")
	
class DATA_PT_pathanim(DataButtonsPanel):
	__label__ = "Path Animation"
	
	def draw_header(self, context):
		layout = self.layout
		
		curve = context.curve

		layout.itemR(curve, "path", text="")

	def draw(self, context):
		layout = self.layout
		
		curve = context.curve
		
		layout.active = curve.path	
		
		split = layout.split()		
		
		col = split.column()
		col.itemR(curve, "path_length", text="Frames")
		col.itemR(curve, "follow")

		col = split.column()
		col.itemR(curve, "stretch")
		col.itemR(curve, "offset_path_distance", text="Offset Children")
	
class DATA_PT_current_curve(DataButtonsPanel):
	__label__ = "Current Curve"

	def draw(self, context):
		layout = self.layout
		
		currentcurve = context.curve.curves[0] # XXX

		split = layout.split()
	
		col = split.column()
		col.itemL(text="Cyclic:")
		col.itemR(currentcurve, "cyclic_u", text="U")
		col.itemR(currentcurve, "cyclic_v", text="V")
		col.itemL(text="Order:")
		col.itemR(currentcurve, "order_u", text="U")
		col.itemR(currentcurve, "order_v", text="V")
		col.itemL(text="Endpoints:")
		col.itemR(currentcurve, "endpoint_u", text="U")
		col.itemR(currentcurve, "endpoint_v", text="V")
		
		col = split.column()
		col.itemL(text="Bezier:")
		col.itemR(currentcurve, "bezier_u", text="U")
		col.itemR(currentcurve, "bezier_v", text="V")
		col.itemL(text="Resolution:")
		col.itemR(currentcurve, "resolution_u", text="U")
		col.itemR(currentcurve, "resolution_v", text="V")
		col.itemL(text="Interpolation:")
		col.itemR(currentcurve, "tilt_interpolation", text="Tilt")
		col.itemR(currentcurve, "radius_interpolation", text="Tilt")
		col.itemR(currentcurve, "smooth")
		
bpy.types.register(DATA_PT_context_curve)
bpy.types.register(DATA_PT_shape_curve)
bpy.types.register(DATA_PT_geometry_curve)
bpy.types.register(DATA_PT_pathanim)
bpy.types.register(DATA_PT_current_curve)
