
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.type == "LAMP")
	
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

bpy.types.register(DATA_PT_lamp)
bpy.types.register(DATA_PT_sunsky)
