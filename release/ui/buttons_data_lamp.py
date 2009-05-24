
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == 'LAMP')
	
class DATA_PT_lamp(DataButtonsPanel):
	__idname__ = "DATA_PT_lamp"
	__label__ = "Lamp"

	def draw(self, context):
		lamp = context.active_object.data
		layout = self.layout

		if not lamp:
			return
		
		row = layout.row()
		row.itemR(lamp, "type", expand=True)
		
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
			sub.itemR(lamp, "falloff_type")
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
				
class DATA_PT_sunsky(DataButtonsPanel):
	__idname__ = "DATA_PT_sunsky"
	__label__ = "Sun/Sky"
	
	def poll(self, context):
		ob = context.active_object
		return (ob.type == 'LAMP' and ob.data.type == 'SUN')

	def draw(self, context):
		lamp = context.active_object.data
		layout = self.layout

		if not lamp:
			return
		
		row = layout.row()
		row.itemR(lamp, "sky")
		row.itemR(lamp, "atmosphere")
		
		if lamp.sky or lamp.atmosphere:
			layout.itemR(lamp, "atmosphere_turbidity", text="Turbidity")
			
			split = layout.split()
			
			col = split.column()
			if lamp.sky:
				sub = col.column()
				sub.itemR(lamp, "sky_blend_type", text="Blend Type")
				sub.itemR(lamp, "sky_blend")
				sub.itemR(lamp, "sky_color_space", text="Color Space")
				sub.itemR(lamp, "sky_exposure")
				sub = col.column()
				sub.itemR(lamp, "horizon_brightness", text="Hor Bright")
				sub.itemR(lamp, "spread", text="Hor Spread")
				sub.itemR(lamp, "sun_brightness", text="Sun Bright")
				sub.itemR(lamp, "sun_size")
				sub.itemR(lamp, "backscattered_light", text="Back Light")
				
			sub = split.column()
			if lamp.atmosphere:
				sub.itemR(lamp, "sun_intensity", text="Sun Intens")
				sub.itemR(lamp, "atmosphere_inscattering", text="Inscattering")
				sub.itemR(lamp, "atmosphere_extinction", text="Extinction")
				sub.itemR(lamp, "atmosphere_distance_factor", text="Distance")
				
class DATA_PT_shadow(DataButtonsPanel):
	__idname__ = "DATA_PT_shadow"
	__label__ = "Shadow"
	
	def poll(self, context):
		ob = context.active_object
		return (ob.type == 'LAMP' and ob.data.type in ('POINT','SUN', 'SPOT', 'AREA'))

	def draw(self, context):
		lamp = context.active_object.data
		layout = self.layout

		if not lamp:
			return
		
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
		ob = context.active_object
		return (ob.type == 'LAMP' and ob.data.type == 'SPOT')

	def draw(self, context):
		lamp = context.active_object.data
		layout = self.layout

		if not lamp:
			return
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(lamp, "spot_size", text="Size")
		sub.itemR(lamp, "spot_blend", text="Blend")
		sub.itemR(lamp, "square")
		
		sub = split.column()
		sub.itemR(lamp, "halo")
		if lamp.halo:
			sub.itemR(lamp, "halo_intensity", text="Intensity")
			if lamp.shadow_method == 'BUFFER_SHADOW':
				sub.itemR(lamp, "halo_step", text="Step")

bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_sunsky)
bpy.types.register(DATA_PT_spot)
