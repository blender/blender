
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'CURVE')

class DATA_PT_shape_curve(DataButtonsPanel):
		__idname__ = "DATA_PT_shape_curve"
		__label__ = "Shape"

		def draw(self, context):
			curve = context.active_object.data
			layout = self.layout

			layout.itemR(curve, "curve_2d")			
							
			split = layout.split()
		
			sub = split.column()
			sub.itemL(text="Caps:")
			sub.itemR(curve, "front")
			sub.itemR(curve, "back")
			
			sub.itemL(text="Textures:")
			sub.itemR(curve, "uv_orco")
			sub.itemR(curve, "auto_texspace")
			
			sub = split.column()	
			sub.itemL(text="Resolution:")
			sub.itemR(curve, "resolution_u", text="Preview U")
			sub.itemR(curve, "resolution_v", text="Preview V")
			sub.itemR(curve, "render_resolution_u", text="Render U")
			sub.itemR(curve, "render_resolution_v", text="Render V")

			sub.itemL(text="Display:")
			sub.itemL(text="HANDLES")
			sub.itemL(text="NORMALS")
			sub.itemR(curve, "vertex_normal_flip")

class DATA_PT_geometry(DataButtonsPanel):
		__idname__ = "DATA_PT_geometry"
		__label__ = "Geometry"

		def draw(self, context):
			curve = context.active_object.data
			layout = self.layout

			split = layout.split()
		
			sub = split.column()
			sub.itemL(text="Modification:")
			sub.itemR(curve, "width")
			sub.itemR(curve, "extrude")
			sub.itemR(curve, "taper_object")
			
			sub = split.column()
			sub.itemL(text="Bevel:")
			sub.itemR(curve, "bevel_depth", text="Depth")
			sub.itemR(curve, "bevel_resolution", text="Resolution")
			sub.itemR(curve, "bevel_object")
	
class DATA_PT_pathanim(DataButtonsPanel):
		__idname__ = "DATA_PT_pathanim"
		__label__ = "Path Animation"

		def draw(self, context):
			curve = context.active_object.data
			layout = self.layout

			layout.itemR(curve, "path", text="Enable")
			
			split = layout.split()		
			
			sub = split.column()
			sub.itemR(curve, "path_length", text="Frames")
			sub.itemR(curve, "follow")

			sub = split.column()
			sub.itemR(curve, "stretch")
			sub.itemR(curve, "offset_path_distance", text="Offset Children")
	
class DATA_PT_current_curve(DataButtonsPanel):
		__idname__ = "DATA_PT_current_curve"
		__label__ = "Current Curve"

		def draw(self, context):
			currentcurve = context.active_object.data.curves[0]
			layout = self.layout

			split = layout.split()
		
			sub = split.column()
			sub.itemL(text="Cyclic:")
			sub.itemR(currentcurve, "cyclic_u", text="U")
			sub.itemR(currentcurve, "cyclic_v", text="V")
			sub.itemL(text="Order:")
			sub.itemR(currentcurve, "order_u", text="U")
			sub.itemR(currentcurve, "order_v", text="V")
			sub.itemL(text="Point Count:")			
			sub.itemR(currentcurve, "point_count_u", text="U")
			sub.itemR(currentcurve, "point_count_v", text="V")
			sub.itemL(text="Endpoints:")
			sub.itemR(currentcurve, "endpoint_u", text="U")
			sub.itemR(currentcurve, "endpoint_v", text="V")
			
			sub = split.column()
			sub.itemL(text="Bezier:")
			sub.itemR(currentcurve, "bezier_u", text="U")
			sub.itemR(currentcurve, "bezier_v", text="V")
			sub.itemL(text="Resolution:")
			sub.itemR(currentcurve, "resolution_u", text="U")
			sub.itemR(currentcurve, "resolution_v", text="V")
			sub.itemL(text="Interpolation:")
			sub.itemR(currentcurve, "tilt_interpolation", text="Tilt")
			sub.itemR(currentcurve, "radius_interpolation", text="Tilt")
			sub.itemR(currentcurve, "smooth")
			
bpy.types.register(DATA_PT_shape_curve)
bpy.types.register(DATA_PT_geometry)
bpy.types.register(DATA_PT_pathanim)
bpy.types.register(DATA_PT_current_curve)