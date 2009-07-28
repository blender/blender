
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.lamp)
		
class DATA_PT_preview(DataButtonsPanel):
	__label__ = "Preview"

	def draw(self, context):
		layout = self.layout

		layout.template_preview(context.lamp)
	
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
		col.itemR(lamp, "color", text="")
		col.itemR(lamp, "energy")
		col.itemR(lamp, "negative")
		col.itemR(lamp, "distance")
	
		col = split.column()
		col.itemR(lamp, "layer", text="This Layer Only")
		col.itemR(lamp, "specular")
		col.itemR(lamp, "diffuse")
		
		split = layout.split()
		
		if lamp.type in ('POINT', 'SPOT'):
			col = split.column()
			col.itemL(text="Falloff:")
			sub = col.column(align=True)
			sub.itemR(lamp, "falloff_type", text="")
			sub.itemR(lamp, "distance")
			sub.itemR(lamp, "sphere")
			
			if lamp.falloff_type == 'LINEAR_QUADRATIC_WEIGHTED':
				col = split.column()
				col.itemL(text="Attenuation Distance:")
				sub = col.column(align=True)
				sub.itemR(lamp, "linear_attenuation", slider=True, text="Linear")
				sub.itemR(lamp, "quadratic_attenuation", slider=True, text="Quadratic")
			else:
				split.column()
			
		if lamp.type == 'AREA':
			col = split.column()
			col.itemL(text="Shape:")
			sub = col.column(align=True)
			sub.itemR(lamp, "shape", text="")
			if (lamp.shape == 'SQUARE'):
				sub.itemR(lamp, "size")
			elif (lamp.shape == 'RECTANGLE'):
				sub.itemR(lamp, "size", text="Size X")
				sub.itemR(lamp, "size_y", text="Size Y")
			
			col = split.column()
			col.itemL(text="Gamma:")
			col.itemR(lamp, "gamma", text="Value")

class DATA_PT_sunsky(DataButtonsPanel):
	__label__ = "Sun/Sky"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SUN')

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp.sky

		row = layout.row()
		row.itemR(lamp, "sky")
		row.itemR(lamp, "atmosphere")
		
		row = layout.row()
		row.active = lamp.sky or lamp.atmosphere
		row.itemR(lamp, "atmosphere_turbidity", text="Turbidity")
			
		split = layout.split()
		
		col = split.column()
		col.active = lamp.sky
		col.itemL(text="Blend Mode:")
		sub = col.column(align=True)
		sub.itemR(lamp, "sky_blend_type", text="")
		sub.itemR(lamp, "sky_blend", text="Factor")
		
		col.itemL(text="Color Space:")
		sub = col.column(align=True)
		sub.itemR(lamp, "sky_color_space", text="")
		sub.itemR(lamp, "sky_exposure", text="Exposure")
			
		col = split.column()
		col.active = lamp.sky
		col.itemL(text="Horizon:")
		sub = col.column(align=True)
		sub.itemR(lamp, "horizon_brightness", text="Brightness")
		sub.itemR(lamp, "spread", text="Spread")
		
		col.itemL(text="Sun:")
		sub = col.column(align=True)
		sub.itemR(lamp, "sun_brightness", text="Brightness")
		sub.itemR(lamp, "sun_size", text="Size")
		sub.itemR(lamp, "backscattered_light", slider=True,text="Back Light")
		
		layout.itemS()
		
		split = layout.split()
		
		col = split.column()
		col.active = lamp.atmosphere
		col.itemL(text="Sun:")
		col.itemR(lamp, "sun_intensity", text="Intensity")
		col.itemL(text="Scale Distance:")
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
				
				col = split.column(align=True)
				col.itemR(lamp, "shadow_soft_size", text="Soft Size")
				
				col = split.column(align=True)
				col.itemR(lamp, "shadow_ray_samples", text="Samples")
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
						
			elif lamp.type == 'AREA':
				split = layout.split()
				
				col = split.column(align=True)
				if lamp.shape == 'SQUARE':
					col.itemR(lamp, "shadow_ray_samples_x", text="Samples")
				elif lamp.shape == 'RECTANGLE':
					col.itemR(lamp, "shadow_ray_samples_x", text="Samples X")
					col.itemR(lamp, "shadow_ray_samples_y", text="Samples Y")
					
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
				
				elif lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
					col = split.column()
					col.itemR(lamp, "umbra")
					col.itemR(lamp, "dither")
					col.itemR(lamp, "jitter")	
				else:
					split.column()

		if lamp.shadow_method == 'BUFFER_SHADOW':
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

class DATA_PT_spot(DataButtonsPanel):
	__label__ = "Spot"
	
	def poll(self, context):
		lamp = context.lamp
		return (lamp and lamp.type == 'SPOT')

	def draw(self, context):
		layout = self.layout
		
		lamp = context.lamp

		split = layout.split()
		
		col = split.column()
		sub = col.column(align=True)
		sub.itemR(lamp, "spot_size", text="Size")
		sub.itemR(lamp, "spot_blend", text="Blend")
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
bpy.types.register(DATA_PT_falloff_curve)
bpy.types.register(DATA_PT_spot)
bpy.types.register(DATA_PT_shadow)
bpy.types.register(DATA_PT_sunsky)
