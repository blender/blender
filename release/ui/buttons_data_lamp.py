
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
		lamp = context.main.lamps[0]
		layout = self.layout

		if not lamp:
			return
		
		layout.row()
		layout.itemR(lamp, "type", expand=True)
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemL(text="LAMP DATABLOCKS")
		sub.itemR(lamp, "energy")
		sub.itemR(lamp, "distance")
	
		sub = layout.sub(1)
		sub.column()
		sub.itemR(lamp, "color")
		
		layout.split(number=2)	
		
		sub = layout.sub(0)
		sub.column()
		sub.itemL(text="Illumination:")
		sub.itemR(lamp, "layer")
		sub.itemR(lamp, "negative")
		sub.itemR(lamp, "specular")
		sub.itemR(lamp, "diffuse")
		
		sub = layout.sub(1)
		if (lamp.type in ('LOCAL', 'SPOT')):
			sub.column()
			sub.itemR(lamp, "falloff_type")
			sub.itemR(lamp, "sphere")
			
			if (lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED'):
				sub.itemR(lamp, "linear_attenuation")
				sub.itemR(lamp, "quadratic_attenuation")
			
		if (lamp.type == 'AREA'):
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
		lamp = context.main.lamps[0]
		return (ob.type == 'LAMP' and lamp.type == 'SUN')

	def draw(self, context):
		lamp = context.main.lamps[0].sky
		layout = self.layout

		if not lamp:
			return
		
		layout.row()
		layout.itemR(lamp, "sky")
		layout.itemR(lamp, "atmosphere")
		
		if (lamp.sky or lamp.atmosphere):
			layout.row()
			layout.itemR(lamp, "atmosphere_turbidity", text="Turbidity")
			
			layout.split(number=2)
			
			if (lamp.sky):
				sub = layout.sub(0)
				sub.column()
				sub.itemR(lamp, "sky_blend_type", text="Blend Type")
				sub.itemR(lamp, "sky_blend")
				sub.itemR(lamp, "sky_color_space", text="Color Space")
				sub.itemR(lamp, "sky_exposure")
				sub.column()
				sub.itemR(lamp, "horizon_brightness", text="Hor Bright")
				sub.itemR(lamp, "spread", text="Hor Spread")
				sub.itemR(lamp, "sun_brightness", text="Sun Bright")
				sub.itemR(lamp, "sun_size")
				sub.itemR(lamp, "backscattered_light", text="Back Light")
				
			if (lamp.atmosphere):
				sub = layout.sub(1)
				sub.column()
				sub.itemR(lamp, "sun_intensity", text="Sun Intens")
				sub.itemR(lamp, "atmosphere_inscattering", text="Inscattering")
				sub.itemR(lamp, "atmosphere_extinction", text="Extinction")
				sub.itemR(lamp, "atmosphere_distance_factor", text="Distance")
				
class DATA_PT_shadow(DataButtonsPanel):
	__idname__ = "DATA_PT_shadow"
	__label__ = "Shadow"
	
	def poll(self, context):
		ob = context.active_object
		lamp = context.main.lamps[0]
		return (ob.type == 'LAMP' and lamp.type in ('LOCAL','SUN', 'SPOT', 'AREA'))

	def draw(self, context):
		lamp = context.main.lamps[0]
		layout = self.layout

		if not lamp:
			return
		
		layout.row()
		layout.itemR(lamp, "shadow_method", expand=True)
		
		if (lamp.shadow_method in ('BUFFER_SHADOW', 'RAY_SHADOW')):
		
			layout.split(number=2)
			
			sub = layout.sub(0)
			sub.column()
			sub.itemL(text="Options:")
			sub.itemR(lamp, "only_shadow")
			sub.itemR(lamp, "shadow_layer")
			
			sub = layout.sub(1)
			sub.column()
			sub.itemR(lamp, "shadow_color")
		
		if (lamp.shadow_method == 'RAY_SHADOW'):
		
			layout.column()
			layout.itemL(text="Sampling:")
			layout.itemR(lamp, "shadow_ray_sampling_method", expand=True)
				
			if (lamp.type in ('LOCAL', 'SUN', 'SPOT') and lamp.shadow_ray_sampling_method in ('CONSTANT_QMC', 'ADAPTIVE_QMC')):
				layout.column_flow()
				layout.itemR(lamp, "shadow_soft_size", text="Soft Size")
				layout.itemR(lamp, "shadow_ray_samples", text="Samples")
				if (lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC'):
					layout.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
						
			if (lamp.type == 'AREA'):
				layout.column_flow()
				layout.itemR(lamp, "shadow_ray_samples_x", text="Samples")
				if (lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC'):
					layout.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
				if (lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED'):
					layout.itemR(lamp, "umbra")
					layout.itemR(lamp, "dither")
					layout.itemR(lamp, "jitter")	
		
		if (lamp.shadow_method == 'BUFFER_SHADOW'):
			layout.column()
			layout.itemL(text="Buffer Type:")
			layout.itemR(lamp, "shadow_buffer_type", expand=True)

			if (lamp.shadow_buffer_type in ('REGULAR', 'HALFWAY')):
				layout.column_flow()
				layout.itemL(text="Sample Buffers:")
				layout.itemR(lamp, "shadow_sample_buffers", expand=True)
				layout.itemL(text="Filter Type:")
				layout.itemR(lamp, "shadow_filter_type", expand=True)
				layout.column_flow()
				layout.itemR(lamp, "shadow_buffer_size", text="Size")
				layout.itemR(lamp, "shadow_buffer_samples", text="Samples")
				layout.itemR(lamp, "shadow_buffer_bias", text="Bias")
				layout.itemR(lamp, "shadow_buffer_soft", text="Soft")
				
			if (lamp.shadow_buffer_type == 'IRREGULAR'):
				layout.row()
				layout.itemR(lamp, "shadow_buffer_bias", text="Bias")
			
			layout.row()
			layout.itemR(lamp, "auto_clip_start", text="Autoclip Start")
			if not (lamp.auto_clip_start):
				layout.itemR(lamp, "shadow_buffer_clip_start", text="Clip Start")
			layout.row()
			layout.itemR(lamp, "auto_clip_end", text="Autoclip End")
			if not (lamp.auto_clip_end):
				layout.itemR(lamp, "shadow_buffer_clip_end", text=" Clip End")

class DATA_PT_spot(DataButtonsPanel):
	__idname__ = "DATA_PT_spot"
	__label__ = "Spot"
	
	def poll(self, context):
		ob = context.active_object
		lamp = context.main.lamps[0]
		return (ob.type == 'LAMP' and lamp.type == 'SPOT')

	def draw(self, context):
		lamp = context.main.lamps[0]
		layout = self.layout

		if not lamp:
			return
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemR(lamp, "square")
		sub.itemR(lamp, "spot_size")
		sub.itemR(lamp, "spot_blend")
		
		sub = layout.sub(1)
		sub.column()
		sub.itemR(lamp, "halo")
		if (lamp.halo):
			sub.itemR(lamp, "halo_intensity")
			if (lamp.shadow_method == 'BUFFER_SHADOW'):
				sub.itemR(lamp, "halo_step")

bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_sunsky)
bpy.types.register(DATA_PT_spot)
