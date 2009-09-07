
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "data"
	
	def poll(self, context):
		return (context.object and context.object.type in ('CURVE', 'SURFACE') and context.curve)
		
class DataButtonsPanelCurve(DataButtonsPanel):
	'''
	Same as above but for curves only
	'''
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
		is_surf = (ob.type == 'SURFACE')

		row = layout.row()
		row.itemR(curve, "curve_2d")			
		row.itemR(curve, "use_twist_correction")
		
							
		split = layout.split()
		
		col = split.column()
		
		if not is_surf:
			sub = col.column()
			sub.active = curve.curve_2d
			sub.itemL(text="Caps:")
			row = sub.row()
			row.itemR(curve, "front")
			row.itemR(curve, "back")
			
		col.itemL(text="Textures:")
#		col.itemR(curve, "uv_orco")
		col.itemR(curve, "auto_texspace")
			
		col = split.column()	
		col.itemL(text="Resolution:")
		sub = col.column(align=True)
		sub.itemR(curve, "resolution_u", text="Preview U")
		sub.itemR(curve, "render_resolution_u", text="Render U")
		
		if is_surf:
			sub = col.column(align=True)
			sub.itemR(curve, "resolution_v", text="Preview V")
			sub.itemR(curve, "render_resolution_v", text="Render V")
		

#		col.itemL(text="Display:")
#		col.itemL(text="HANDLES")
#		col.itemL(text="NORMALS")
#		col.itemR(curve, "vertex_normal_flip")

class DATA_PT_geometry_curve(DataButtonsPanelCurve):
	__label__ = "Geometry "

	def draw(self, context):
		layout = self.layout
		
		curve = context.curve
		
		split = layout.split()
	
		col = split.column()
		col.itemL(text="Modification:")
		col.itemR(curve, "width")
		col.itemR(curve, "extrude")
		col.itemR(curve, "taper_object", icon='ICON_OUTLINER_OB_CURVE')
		
		col = split.column()
		col.itemL(text="Bevel:")
		col.itemR(curve, "bevel_depth", text="Depth")
		col.itemR(curve, "bevel_resolution", text="Resolution")
		col.itemR(curve, "bevel_object", icon='ICON_OUTLINER_OB_CURVE')

	
class DATA_PT_pathanim(DataButtonsPanelCurve):
	__label__ = "Path Animation"
	
	def draw_header(self, context):
		curve = context.curve

		self.layout.itemR(curve, "use_path", text="")

	def draw(self, context):
		layout = self.layout
		
		curve = context.curve
		
		layout.active = curve.use_path	
		
		split = layout.split()		
		
		col = split.column()
		col.itemR(curve, "path_length", text="Frames")
		col.itemR(curve, "use_path_follow")

		col = split.column()
		col.itemR(curve, "use_stretch")
		col.itemR(curve, "use_time_offset", text="Offset Children")
	
class DATA_PT_current_curve(DataButtonsPanel):
	__label__ = "Current Curve"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		currentcurve = context.curve.curves[0] # XXX
		is_surf = (ob.type == 'SURFACE')
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Cyclic:")
		if currentcurve.type == 'NURBS':
			col.itemL(text="Bezier:")
			col.itemL(text="Endpoint:")
			col.itemL(text="Order:")
		col.itemL(text="Resolution:")
		
		
		col = split.column()
		col.itemR(currentcurve, "cyclic_u", text="U")
		
		if currentcurve.type == 'NURBS':
			sub = col.column()
			sub.active = (not currentcurve.cyclic_u)
			sub.itemR(currentcurve, "bezier_u", text="U")
			sub.itemR(currentcurve, "endpoint_u", text="U")
			
			sub = col.column()
			sub.itemR(currentcurve, "order_u", text="U")
		col.itemR(currentcurve, "resolution_u", text="U")
		
		if is_surf:
			col = split.column()
			col.itemR(currentcurve, "cyclic_v", text="V")
			
			# its a surface, assume its a nurb.
			sub = col.column()
			sub.active = (not currentcurve.cyclic_v)
			sub.itemR(currentcurve, "bezier_v", text="V")
			sub.itemR(currentcurve, "endpoint_v", text="V")
			sub = col.column()
			sub.itemR(currentcurve, "resolution_v", text="V")
			sub.itemR(currentcurve, "order_v", text="V")
		
		
		split = layout.split()
		col = split.column()
		col.itemL(text="Interpolation:")
		col.itemR(currentcurve, "tilt_interpolation", text="Tilt")
		col.itemR(currentcurve, "radius_interpolation", text="Radius")
		col.itemR(currentcurve, "smooth")
		
		
bpy.types.register(DATA_PT_context_curve)
bpy.types.register(DATA_PT_shape_curve)
bpy.types.register(DATA_PT_geometry_curve)
bpy.types.register(DATA_PT_pathanim)
bpy.types.register(DATA_PT_current_curve)
