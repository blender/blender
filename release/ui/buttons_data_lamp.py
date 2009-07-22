
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
		
		layout.itemR(lamp, "type", expand=True)
		
		split = layout.split()
		col = split.column()
		#col.itemL(text="Type:")
		#col.itemR(lamp, "type", text="")
		colsub = col.column(align=True)
		colsub.itemR(lamp, "color", text="")
		colsub.itemR(lamp, "energy")
		
		col.itemR(lamp, "negative")
		#col.itemR(lamp, "distance")
	
		sub = split.column()
		#sub.itemL(text="Influence:")
		sub.itemR(lamp, "layer", text="This Layer Only")
		sub.itemR(lamp, "specular")
		sub.itemR(lamp, "diffuse")
		#sub.itemR(lamp, "negative")
		
		if lamp.type in ('POINT', 'SPOT'):
			split = layout.split()
			col = split.column()
			col.itemL(text="Falloff:")
			col = col.column(align=True)
			col.itemR(lamp, "falloff_type", text="")
			col.itemR(lamp, "distance")
			col.itemR(lamp, "sphere")
			
			if lamp.falloff_type != 'LINEAR_QUADRATIC_WEIGHTED':
				col = split.column()
			
			else:
				sub = split.column()
				sub.itemL(text="Attenuation Distance:")
				sub = sub.column(align=True)
				sub.itemR(lamp, "linear_attenuation", slider=True, text="Linear")
				sub.itemR(lamp, "quadratic_attenuation", slider=True, text="Quadratic")
			
		if lamp.type == 'AREA':
			split = layout.split()
			col = split.column()
			col.itemL(text="Shape:")
			col = col.column(align=True)
			col.itemR(lamp, "shape", text="")
			if (lamp.shape == 'SQUARE'):
				col.itemR(lamp, "size")
			if (lamp.shape == 'RECTANGLE'):
				col.itemR(lamp, "size", text="Size X")
				col.itemR(lamp, "size_y", text="Size Y")
			
			sub = split.column()
			sub.itemL(text="Gamma:")
			sub.itemR(lamp, "gamma", text="Value")
				
class DATA_PT_sunsky(DataButtonsPanel):
	__idname__ = "DATA_PT_sunsky"
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
		colsub = col.column(align=True)
		colsub.itemR(lamp, "sky_blend_type", text="")
		colsub.itemR(lamp, "sky_blend", text="Factor")
		
		col.itemL(text="Color Space:")
		colsub = col.column(align=True)
		colsub.itemR(lamp, "sky_color_space", text="")
		colsub.itemR(lamp, "sky_exposure", text="Exposure")
			
		col = split.column()
		col.active = lamp.sky
		col.itemL(text="Horizon:")
		colsub = col.column(align=True)
		colsub.itemR(lamp, "horizon_brightness", text="Brightness")
		colsub.itemR(lamp, "spread", text="Spread")
		
		col.itemL(text="Sun:")
		colsub = col.column(align=True)
		colsub.itemR(lamp, "sun_brightness", text="Brightness")
		colsub.itemR(lamp, "sun_size", text="Size")
		colsub.itemR(lamp, "backscattered_light", slider=True,text="Back Light")
		
		row = layout.row()
		row.itemS()
		
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
	__idname__ = "DATA_PT_shadow"
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
			col.itemR(lamp, "shadow_color")
			
			sub = split.column()
			sub.itemR(lamp, "shadow_layer", text="This Layer Only")
			sub.itemR(lamp, "only_shadow")
		
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
						
			if lamp.type == 'AREA':
				split = layout.split()
				col = split.column()

				if lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
					col.itemR(lamp, "umbra")
					col.itemR(lamp, "dither")
					col.itemR(lamp, "jitter")	
				else:
					col.itemL()

				col = split.column(align=True)
				col.itemR(lamp, "shadow_ray_samples_x", text="Samples")
				if lamp.shadow_ray_sampling_method == 'ADAPTIVE_QMC':
					col.itemR(lamp, "shadow_adaptive_threshold", text="Threshold")
	
		if lamp.shadow_method == 'BUFFER_SHADOW':
			col = layout.column()
			col.itemL(text="Buffer Type:")
			col.row().itemR(lamp, "shadow_buffer_type", expand=True)

			if lamp.shadow_buffer_type in ('REGULAR', 'HALFWAY'):
				
				split = layout.split()
				col = split.column()
				col.itemL(text="Filter Type:")
				col.itemR(lamp, "shadow_filter_type", text="")
				
				colsub = col.column(align=True)
				colsub.itemR(lamp, "shadow_buffer_soft", text="Soft")
				colsub.itemR(lamp, "shadow_buffer_bias", text="Bias")
				
				col = split.column()
				col.itemL(text="Sample Buffers:")
				col.itemR(lamp, "shadow_sample_buffers", text="")
				
				colsub = col.column(align=True)
				colsub.itemR(lamp, "shadow_buffer_size", text="Size")
				colsub.itemR(lamp, "shadow_buffer_samples", text="Samples")
				
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
		col = split.column()
		sub = col.column(align=True)
		sub.itemR(lamp, "spot_size", text="Size")
		sub.itemR(lamp, "spot_blend", text="Blend")
		col.itemR(lamp, "square")
		
		col = split.column()
		col.itemR(lamp, "halo")
		colsub = col.column(align=True)
		colsub.active = lamp.halo
		colsub.itemR(lamp, "halo_intensity", text="Intensity")
		if lamp.shadow_method == 'BUFFER_SHADOW':
			colsub.itemR(lamp, "halo_step", text="Step")

class DATA_PT_falloff_curve(DataButtonsPanel):
	__idname__ = "DATA_PT_falloff_curve"
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

