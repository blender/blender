
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.lamp != None)
		
class DATA_PT_preview(DataButtonsPanel):
	__idname__= "DATA_PT_preview"
	__label__ = "Preview"

	def draw(self, context):
		layout = self.layout

		lamp = context.lamp
		layout.template_preview(lamp)
	
class DATA_PT_context_lamp(DataButtonsPanel):
	__idname__ = "DATA_PT_context_lamp"
	__no_header__ = True
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		lamp = context.lamp
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif lamp:
			split.template_ID(space, "pin_id")
			split.itemS()

class DATA_PT_lamp(DataButtonsPanel):
	__idname__ = "DATA_PT_lamp"
	__label__ = "Lamp"

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp
		
		split = layout.split(percentage=0.2)
		split.itemL(text="Type:")
		split.itemR(lamp, "type", text="")
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(lamp, "color")
		sub.itemR(lamp, "energy")
		sub.itemR(lamp, "distance")
		sub.itemR(lamp, "negative")
	
		sub = split.column()
		sub.itemR(lamp, "layer", text="This Layer Only")
		sub.itemR(lamp, "specular")
		sub.itemR(lamp, "diffuse")
		
		if lamp.type in ('POINT', 'SPOT'):
			split = sub.split(percentage=0.3)
			split.itemL(text="Falloff:")
			split.itemR(lamp, "falloff_type", text="")
			sub.itemR(lamp, "sphere")
			
			if (lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED'):
				sub.itemR(lamp, "linear_attenuation")
				sub.itemR(lamp, "quadratic_attenuation")
			
		if lamp.type == 'AREA':
			sub.column()
			sub.itemR(lamp, "gamma")
			sub.itemR(lamp, "shape")
			if (lamp.shape == 'SQUARE'):
				sub.itemR(lamp, "size")
			if (lamp.shape == 'RECTANGLE'):
				sub.itemR(lamp, "size", text="Size X")
				sub.itemR(lamp, "size_y")
				
class DATA_PT_sky(DataButtonsPanel):
	__idname__ = "DATA_PT_sky"
	__label__ = "Sky"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SUN')
		
	def draw_header(self, context):
		layout = self.layout
		lamp = context.lamp.sky

		layout.itemR(lamp, "sky", text="")

	def draw(self, context):
		layout = self.layout
		lamp = context.lamp.sky

		layout.active = lamp.sky
		
		split = layout.split()
		col = split.column()

		col.itemL(text="Colors:")
		col.itemR(lamp, "sky_blend_type", text="Blend Type")
		col.itemR(lamp, "sky_blend")
		col.itemR(lamp, "sky_color_space", text="Color Space")
		col.itemR(lamp, "sky_exposure", text="Exposure")
		
		col = split.column()
		col.itemL(text="Horizon:")
		col.itemR(lamp, "horizon_brightness", text="Brightness")
		col.itemR(lamp, "spread", text="Spread")
		col.itemL(text="Sun:")
		col.itemR(lamp, "sun_brightness", text="Brightness")
		col.itemR(lamp, "sun_size", text="Size")
		col.itemR(lamp, "backscattered_light", text="Back Light")
				

		
		
class DATA_PT_atmosphere(DataButtonsPanel):
	__idname__ = "DATA_PT_atmosphere"
	__label__ = "Atmosphere"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SUN')

	def draw_header(self, context):
		layout = self.layout
		lamp = context.lamp.sky

		layout.itemR(lamp, "atmosphere", text="")

	def draw(self, context):
		layout = self.layout
		lamp = context.lamp.sky
	
		layout.active = lamp.atmosphere
		
		split = layout.split()
		sub = split.column()
		sub.itemR(lamp, "atmosphere_turbidity", text="Turbidity")
		sub.itemR(lamp, "sun_intensity", text="Sun Intensity")
		sub = split.column()
		sub.itemR(lamp, "atmosphere_inscattering", text="Inscattering", slider=True)
		sub.itemR(lamp, "atmosphere_extinction", text="Extinction", slider=True)
		sub.itemR(lamp, "atmosphere_distance_factor", text="Distance")		
				
class DATA_PT_shadow(DataButtonsPanel):
	__idname__ = "DATA_PT_shadow"
	__label__ = "Shadow"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type in ('POINT','SUN', 'SPOT', 'AREA'))

	def draw(self, context):
		layout = self.layout
		lamp = context.lamp

		layout.itemR(lamp, "shadow_method", expand=True)
		
		if lamp.shadow_method in ('BUFFER_SHADOW', 'RAY_SHADOW'):
		
			split = layout.split()
			
			sub = split.column()
			sub.itemR(lamp, "shadow_color")
			
			sub = split.column()
			sub.itemR(lamp, "shadow_layer", text="This Layer Only")
			sub.itemR(lamp, "only_shadow")
		
		if lamp.shadow_method == 'RAY_SHADOW':
		
			col = layout.column()
			col.itemL(text="Sampling:")
			col.row().itemR(lamp, "shadow_ray_sampling_method", expand=True)
				
			if lamp.type in ('POINT', 'SUN', 'SPOT'):
				flow = layout.column_flow()
				flow.itemR(lamp, "shadow_soft_size", text="Soft Size")
				flow.itemR(lamp, "shadow_ray_samples", text="Samples")
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					flow.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
						
			if lamp.type == 'AREA':
				flow = layout.column_flow()
				flow.itemR(lamp, "shadow_ray_samples_x", text="Samples")
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					flow.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
				if lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
					flow.itemR(lamp, "umbra")
					flow.itemR(lamp, "dither")
					flow.itemR(lamp, "jitter")	
	
		if lamp.shadow_method == 'BUFFER_SHADOW':
			col = layout.column()
			col.itemL(text="Buffer Type:")
			col.row().itemR(lamp, "shadow_buffer_type", expand=True)

			if lamp.shadow_buffer_type in ('REGULAR', 'HALFWAY'):
				flow = layout.column_flow()
				flow.itemL(text="Sample Buffers:")
				flow.itemR(lamp, "shadow_sample_buffers", text="")
				flow.itemL(text="Filter Type:")
				flow.itemR(lamp, "shadow_filter_type", text="")
				
				flow = layout.column_flow()
				flow.itemR(lamp, "shadow_buffer_size", text="Size")
				flow.itemR(lamp, "shadow_buffer_samples", text="Samples")
				flow.itemR(lamp, "shadow_buffer_bias", text="Bias")
				flow.itemR(lamp, "shadow_buffer_soft", text="Soft")
				
			if (lamp.shadow_buffer_type == 'IRREGULAR'):
				row = layout.row()
				row.itemR(lamp, "shadow_buffer_bias", text="Bias")
			
			row = layout.row()
			row.itemR(lamp, "auto_clip_start", text="Autoclip Start")
			if not (lamp.auto_clip_start):
				row.itemR(lamp, "shadow_buffer_clip_start", text="Clip Start")
			row = layout.row()
			row.itemR(lamp, "auto_clip_end", text="Autoclip End")
			if not (lamp.auto_clip_end):
				row.itemR(lamp, "shadow_buffer_clip_end", text=" Clip End")

class DATA_PT_spot(DataButtonsPanel):
	__idname__ = "DATA_PT_spot"
	__label__ = "Spot"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SPOT')

	def draw(self, context):
		layout = self.layout
		lamp = context.lamp

		split = layout.split()
		
		sub = split.column()
		sub.itemR(lamp, "spot_size", text="Size")
		sub.itemR(lamp, "spot_blend", text="Blend")
		sub.itemR(lamp, "square")
		
		col = split.column()
		col.itemR(lamp, "halo")
		colsub = col.column()
		colsub.active = lamp.halo
		colsub.itemR(lamp, "halo_intensity", text="Intensity")
		if lamp.shadow_method == 'BUFFER_SHADOW':
			colsub.itemR(lamp, "halo_step", text="Step")

class DATA_PT_falloff_curve(DataButtonsPanel):
	__idname__ = "DATA_PT_falloff_curve"
	__label__ = "Falloff Curve"
	
	def poll(self, context):
		lamp = context.lamp

		if lamp and lamp.type in ('POINT', 'SPOT'):
			if lamp.falloff_type == 'CUSTOM_CURVE':
				return True

		return False

	def draw(self, context):
		layout = self.layout
		lamp = context.lamp

		layout.template_curve_mapping(lamp.falloff_curve)

bpy.types.register(DATA_PT_context_lamp)
bpy.types.register(DATA_PT_preview)
bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_sky)
bpy.types.register(DATA_PT_atmosphere)
bpy.types.register(DATA_PT_spot)
bpy.types.register(DATA_PT_falloff_curve)

