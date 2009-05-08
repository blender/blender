
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
		sub.itemR(lamp, "color")
	
		sub = layout.sub(1)
		
		sub.column()
		sub.itemL(text="Illumination:")
		sub.itemR(lamp, "layer")
		sub.itemR(lamp, "negative")
		sub.itemR(lamp, "specular")
		sub.itemR(lamp, "diffuse")
		
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
				sub.itemR(lamp, "horizon_brightness", text="Hor Bright")
				sub.itemR(lamp, "spread", text="Hor Spread")
				sub.itemR(lamp, "sun_brightness", text="Sun Bright")
				sub.itemR(lamp, "sun_size")
				sub.itemR(lamp, "backscattered_light", text="Back Light")
				sub.column()
				sub.itemR(lamp, "sky_blend_type", text="Blend Type")
				sub.itemR(lamp, "sky_blend")
				sub.itemR(lamp, "sky_color_space", text="Color Space")
				sub.itemR(lamp, "sky_exposure")
			
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
		
		layout.row()
		layout.itemR(lamp, "only_shadow")
		layout.itemR(lamp, "shadow_layer", text="Layer")
		if (lamp.shadow_method == 'RAY_SHADOW'):
			if (lamp.type in ('LOCAL', 'SUN', 'SPOT', 'AREA')):

				layout.split(number=2)

				sub = layout.sub(0)
				sub.column()
				sub.itemL(text="Display:")
				sub.itemR(lamp, "shadow_color")
		
				sub = layout.sub(1)
				sub.column()
				sub.itemL(text="Sampling:")
				sub.itemR(lamp, "shadow_ray_sampling_method", text="")
				
				if (lamp.type in ('LOCAL', 'SUN', 'SPOT') and lamp.shadow_ray_sampling_method in ('CONSTANT_QMC', 'ADAPTIVE_QMC')):
					sub.itemR(lamp, "shadow_soft_size", text="Soft Size")
					sub.itemR(lamp, "shadow_ray_samples", text="Samples")
					if (lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC'):
						sub.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
						
				if (lamp.type == 'AREA'):
					sub.itemR(lamp, "shadow_ray_samples_x", text="Samples")
					if (lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC'):
						sub.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
					if (lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED'):
						sub.itemR(lamp, "umbra")
						sub.itemR(lamp, "dither")
						sub.itemR(lamp, "jitter")
						
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
		sub.itemR(lamp, "halo")
		
		sub = layout.sub(1)
		sub.column()
		sub.itemR(lamp, "spot_size")
		sub.itemR(lamp, "spot_blend")
		sub.itemR(lamp, "halo_intensity")

bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_sunsky)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_spot)
