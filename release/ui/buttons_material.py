
import bpy

class MaterialButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "material"
	
class MATERIAL_PT_material(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_material"
	__label__ = "Material"
	
	def draw(self, context):
		layout = self.layout
		try:		
			mat = context.active_object.active_material
		except:	
			mat = None
		
		if not mat:
			return
	
		row = layout.row()
		row.column().itemR(mat, "diffuse_color")
		row.column().itemR(mat, "specular_color")
		row.column().itemR(mat, "mirror_color")
		
		row = layout.row()
		row.itemR(mat, "color_model")
		row.itemR(mat, "alpha")
			
class MATERIAL_PT_sss(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_sss"
	__label__ = "Subsurface Scattering"
	
	def draw(self, context):
		layout = self.layout
		try:		
			sss = context.active_object.active_material.subsurface_scattering
		except:	
			sss = None
		
		if not sss:
			return
		
		layout.itemR(sss, "enabled", text="Enable")
		
		flow = layout.column_flow()
		flow.itemR(sss, "error_tolerance")
		flow.itemR(sss, "ior")
		flow.itemR(sss, "scale")
		
		row = layout.row()
		row.column().itemR(sss, "color")
		row.column().itemR(sss, "radius")
		
		flow = layout.column_flow()
		flow.itemR(sss, "color_factor")
		flow.itemR(sss, "texture_factor")
		flow.itemR(sss, "front")
		flow.itemR(sss, "back")
		
class MATERIAL_PT_raymir(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raymir"
	__label__ = "Ray Mirror"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_material)
	
	def draw_header(self, context):
		raym = context.active_object.active_material.raytrace_mirror

		layout = self.layout
		layout.itemR(raym, "enabled", text=self.__label__)
	
	def draw(self, context):
		layout = self.layout
		raym = context.active_object.active_material.raytrace_mirror

		split = layout.split()
		
		sub = split.column()
		sub.itemR(raym, "reflect", text="RayMir")
		sub.itemR(raym, "fresnel")
		sub.itemR(raym, "fresnel_fac", text="Fac")
		
		sub = split.column()
		sub.itemR(raym, "gloss")
		sub.itemR(raym, "gloss_threshold")
		sub.itemR(raym, "gloss_samples")
		sub.itemR(raym, "gloss_anisotropic")
		
		flow = layout.column_flow()
		flow.itemR(raym, "distance", text="Max Dist")
		flow.itemR(raym, "depth")
		flow.itemR(raym, "fade_to")
		
class MATERIAL_PT_raytransp(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raytransp"
	__label__= "Ray Transparency"

	def draw(self, context):
		layout = self.layout
		try:		
			rayt = context.active_object.active_material.raytrace_transparency
		except:	
			rayt = None

		if not rayt:
			return

		layout.itemR(rayt, "enabled", text="Enable")
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(rayt, "ior")
		sub.itemR(rayt, "fresnel")
		sub.itemR(rayt, "fresnel_fac", text="Fac")
		
		sub = split.column()
		sub.itemR(rayt, "gloss")
		sub.itemR(rayt, "gloss_threshold")
		sub.itemR(rayt, "gloss_samples")
		
		flow = layout.column_flow()
		flow.itemR(rayt, "filter")
		flow.itemR(rayt, "limit")
		flow.itemR(rayt, "falloff")
		flow.itemR(rayt, "specular_opacity")
		flow.itemR(rayt, "depth")
		
class MATERIAL_PT_halo(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_halo"
	__label__= "Halo"
	
	def draw(self, context):
		layout = self.layout
		try:		
			halo = context.active_object.active_material.halo
		except:	
			halo = None

		if not halo:
			return
	
		layout.itemR(halo, "enabled", text="Enable Halo")

		if halo.enabled:
			
			split = layout.split()
			
			sub = split.column()
			sub.itemL(text="General Settings:")
			sub.itemR(halo, "size")
			sub.itemR(halo, "hardness")
			sub.itemR(halo, "add")
			
			sub = split.column()
			sub.itemL(text="Elements:")
			sub.itemR(halo, "ring")
			sub.itemR(halo, "lines")
			sub.itemR(halo, "star")
			sub.itemR(halo, "flare_mode")

			row = layout.row()
			
			sub = row.column()
			sub.itemL(text="Options:")
			sub.itemR(halo, "use_texture", text="Texture")
			sub.itemR(halo, "use_vertex_normal", text="Vertex Normal")
			sub.itemR(halo, "xalpha")
			sub.itemR(halo, "shaded")
			sub.itemR(halo, "soft")
		
			sub = row.column()
			if (halo.ring):
				sub.itemR(halo, "rings")
			if (halo.lines):
				sub.itemR(halo, "line_number")
			if (halo.ring or halo.lines):
				sub.itemR(halo, "seed")
			if (halo.star):
				sub.itemR(halo, "star_tips")
			if (halo.flare_mode):
				sub.itemL(text="Flare:")
				sub.itemR(halo, "flare_size", text="Size")
				sub.itemR(halo, "flare_subsize", text="Subsize")
				sub.itemR(halo, "flare_boost", text="Boost")
				sub.itemR(halo, "flare_seed", text="Seed")
				sub.itemR(halo, "flares_sub", text="Sub")
				
bpy.types.register(MATERIAL_PT_material)
bpy.types.register(MATERIAL_PT_raymir)
bpy.types.register(MATERIAL_PT_raytransp)
bpy.types.register(MATERIAL_PT_sss)
bpy.types.register(MATERIAL_PT_halo)
