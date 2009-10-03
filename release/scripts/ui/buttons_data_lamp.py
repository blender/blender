
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "data"
	
	def poll(self, context):
		return context.lamp
		
class DATA_PT_preview(DataButtonsPanel):
	__label__ = "Preview"

	def draw(self, context):
		self.layout.template_preview(context.lamp)
	
class DATA_PT_context_lamp(DataButtonsPanel):
	__show_header__ = False
	
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
	__label__ = "Lamp"

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp
		
		layout.itemR(lamp, "type", expand=True)
		
		split = layout.split()
		
		col = split.column()
		sub = col.column()
		sub.itemR(lamp, "color", text="")
		sub.itemR(lamp, "energy")

		if lamp.type in ('POINT', 'SPOT'):
			sub.itemL(text="Falloff:")
			sub.itemR(lamp, "falloff_type", text="")
			sub.itemR(lamp, "distance")

			if lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED':
				col.itemL(text="Attenuation Factors:")
				sub = col.column(align=True)
				sub.itemR(lamp, "linear_attenuation", slider=True, text="Linear")
				sub.itemR(lamp, "quadratic_attenuation", slider=True, text="Quadratic")
			
			col.itemR(lamp, "sphere")
			
		if lamp.type == 'AREA':
			col.itemR(lamp, "distance")
			col.itemR(lamp, "gamma")
	
		col = split.column()
		col.itemR(lamp, "negative")
		col.itemR(lamp, "layer", text="This Layer Only")
		col.itemR(lamp, "specular")
		col.itemR(lamp, "diffuse")	

class DATA_PT_sunsky(DataButtonsPanel):
	__label__ = "Sky & Atmosphere"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SUN')

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp.sky

		layout.itemR(lamp, "sky")

		row = layout.row()
		row.active = lamp.sky or lamp.atmosphere
		row.itemR(lamp, "atmosphere_turbidity", text="Turbidity")
			
		split = layout.split()
		
		col = split.column()
		col.active = lamp.sky
		col.itemL(text="Blending:")
		sub = col.column()
		sub.itemR(lamp, "sky_blend_type", text="")
		sub.itemR(lamp, "sky_blend", text="Factor")
		
		col.itemL(text="Color Space:")
		sub = col.column()
		sub.row().itemR(lamp, "sky_color_space", expand=True)
		sub.itemR(lamp, "sky_exposure", text="Exposure")
			
		col = split.column()
		col.active = lamp.sky
		col.itemL(text="Horizon:")
		sub = col.column()
		sub.itemR(lamp, "horizon_brightness", text="Brightness")
		sub.itemR(lamp, "spread", text="Spread")
		
		col.itemL(text="Sun:")
		sub = col.column()
		sub.itemR(lamp, "sun_brightness", text="Brightness")
		sub.itemR(lamp, "sun_size", text="Size")
		sub.itemR(lamp, "backscattered_light", slider=True,text="Back Light")
		
		layout.itemS()
		
		layout.itemR(lamp, "atmosphere")
		
		split = layout.split()
		
		col = split.column()
		col.active = lamp.atmosphere
		col.itemL(text="Intensity:")
		col.itemR(lamp, "sun_intensity", text="Sun")
		col.itemR(lamp, "atmosphere_distance_factor", text="Distance")
			
		col = split.column()
		col.active = lamp.atmosphere
		col.itemL(text="Scattering:")
		sub = col.column(align=True)
		sub.itemR(lamp, "atmosphere_inscattering", slider=True, text="Inscattering")
		sub.itemR(lamp, "atmosphere_extinction", slider=True ,text="Extinction")
		
class DATA_PT_shadow(DataButtonsPanel):
	__label__ = "Shadow"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type in ('POINT','SUN', 'SPOT', 'AREA'))

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp

		layout.itemR(lamp, "shadow_method", expand=True)
		
		if lamp.shadow_method != 'NOSHADOW':
			split = layout.split()
			
			col = split.column()
			col.itemR(lamp, "shadow_color", text="")
			
			col = split.column()
			col.itemR(lamp, "shadow_layer", text="This Layer Only")
			col.itemR(lamp, "only_shadow")
		
		if lamp.shadow_method == 'RAY_SHADOW':
			col = layout.column()
			col.itemL(text="Sampling:")
			col.row().itemR(lamp, "shadow_ray_sampling_method", expand=True)
				
			if lamp.type in ('POINT', 'SUN', 'SPOT'):
				split = layout.split()
				
				col = split.column()
				col.itemR(lamp, "shadow_soft_size", text="Soft Size")
				
				col.itemR(lamp, "shadow_ray_samples", text="Samples")
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
				col = split.column()
						
			elif lamp.type == 'AREA':
				split = layout.split()
				
				col = split.column()
				sub = split.column(align=True)
				if lamp.shape == 'SQUARE':
					col.itemR(lamp, "shadow_ray_samples_x", text="Samples")
				elif lamp.shape == 'RECTANGLE':
					col.itemR(lamp, "shadow_ray_samples_x", text="Samples X")
					col.itemR(lamp, "shadow_ray_samples_y", text="Samples Y")
					
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
					
				elif lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
					sub.itemR(lamp, "umbra")
					sub.itemR(lamp, "dither")
					sub.itemR(lamp, "jitter")	

		elif lamp.shadow_method == 'BUFFER_SHADOW':
			col = layout.column()
			col.itemL(text="Buffer Type:")
			col.row().itemR(lamp, "shadow_buffer_type", expand=True)

			if lamp.shadow_buffer_type in ('REGULAR', 'HALFWAY'):
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Filter Type:")
				col.itemR(lamp, "shadow_filter_type", text="")
				sub = col.column(align=True)
				sub.itemR(lamp, "shadow_buffer_soft", text="Soft")
				sub.itemR(lamp, "shadow_buffer_bias", text="Bias")
				
				col = split.column()
				col.itemL(text="Sample Buffers:")
				col.itemR(lamp, "shadow_sample_buffers", text="")
				sub = col.column(align=True)
				sub.itemR(lamp, "shadow_buffer_size", text="Size")
				sub.itemR(lamp, "shadow_buffer_samples", text="Samples")
				
			elif lamp.shadow_buffer_type == 'IRREGULAR':
				layout.itemR(lamp, "shadow_buffer_bias", text="Bias")
			
			row = layout.row()
			row.itemR(lamp, "auto_clip_start", text="Autoclip Start")
			sub = row.row()
			sub.active = not lamp.auto_clip_start
			sub.itemR(lamp, "shadow_buffer_clip_start", text="Clip Start")

			row = layout.row()
			row.itemR(lamp, "auto_clip_end", text="Autoclip End")
			sub = row.row()
			sub.active = not lamp.auto_clip_end
			sub.itemR(lamp, "shadow_buffer_clip_end", text=" Clip End")

class DATA_PT_area(DataButtonsPanel):
	__label__ = "Area Shape"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'AREA')

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp

		split = layout.split()
		
		col = split.column()
		col.row().itemR(lamp, "shape", expand=True)
		
		sub = col.column(align=True)
		if (lamp.shape == 'SQUARE'):
			sub.itemR(lamp, "size")
		elif (lamp.shape == 'RECTANGLE'):
			sub.itemR(lamp, "size", text="Size X")
			sub.itemR(lamp, "size_y", text="Size Y")

class DATA_PT_spot(DataButtonsPanel):
	__label__ = "Spot Shape"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SPOT')

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp

		split = layout.split()
		
		col = split.column()
		sub = col.column()
		sub.itemR(lamp, "spot_size", text="Size")
		sub.itemR(lamp, "spot_blend", text="Blend", slider=True)
		col.itemR(lamp, "square")
		
		col = split.column()
		col.itemR(lamp, "halo")
		sub = col.column(align=True)
		sub.active = lamp.halo
		sub.itemR(lamp, "halo_intensity", text="Intensity")
		if lamp.shadow_method == 'BUFFER_SHADOW':
			sub.itemR(lamp, "halo_step", text="Step")

class DATA_PT_falloff_curve(DataButtonsPanel):
	__label__ = "Falloff Curve"
	__default_closed__ = True
	
	def poll(self, context):
		lamp = context.lamp

		return (lamp and lamp.type in ('POINT', 'SPOT') and lamp.falloff_type == 'CUSTOM_CURVE')

	def draw(self, context):
		lamp = context.lamp

		self.layout.template_curve_mapping(lamp, "falloff_curve")

bpy.types.register(DATA_PT_context_lamp)
bpy.types.register(DATA_PT_preview)
bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_falloff_curve)
bpy.types.register(DATA_PT_area)
bpy.types.register(DATA_PT_spot)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_sunsky)
