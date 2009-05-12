
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
	
		layout.row()
		layout.itemR(mat, "diffuse_color")
		layout.itemR(mat, "specular_color")
		layout.itemR(mat, "mirror_color")
		
		layout.row()
		layout.itemR(mat, "color_model")
		layout.itemR(mat, "alpha")
		
		halo = context.active_object.active_material.halo
		
		layout.row()
		layout.itemR(halo, "enabled", text="Enable Halo")
		
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
		
		layout.row()
		layout.itemR(sss, "enabled", text="Enable")
		
		layout.column_flow()
		layout.itemR(sss, "error_tolerance")
		layout.itemR(sss, "ior")
		layout.itemR(sss, "scale")
		
		layout.row()
		layout.itemR(sss, "color")
		layout.itemR(sss, "radius")
		
		layout.column_flow()
		layout.itemR(sss, "color_factor")
		layout.itemR(sss, "texture_factor")
		layout.itemR(sss, "front")
		layout.itemR(sss, "back")
		
class MATERIAL_PT_raymir(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raymir"
	__label__ = "Ray Mirror"
	
	def draw(self, context):
		layout = self.layout
		try:		
			raym = context.active_object.active_material.raytrace_mirror
		except:	
			raym = None
		
		if not raym:
			return 
		
		layout.row()
		layout.itemR(raym, "enabled", text="Enable")
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column_flow()
		sub.itemR(raym, "reflect", text="RayMir")
		sub.itemR(raym, "fresnel")
		sub.itemR(raym, "fresnel_fac", text="Fac")
		
		sub = layout.sub(1)
		sub.column_flow()
		sub.itemR(raym, "gloss")
		sub.itemR(raym, "gloss_threshold")
		sub.itemR(raym, "gloss_samples")
		sub.itemR(raym, "gloss_anisotropic")
		
		layout.column_flow()
		layout.itemR(raym, "distance", text="Max Dist")
		layout.itemR(raym, "depth")
		layout.itemR(raym, "fade_to")
		
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

		layout.row()
		layout.itemR(rayt, "enabled", text="Enable")
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemR(rayt, "ior")
		sub.itemR(rayt, "fresnel")
		sub.itemR(rayt, "fresnel_fac", text="Fac")
		
		sub = layout.sub(1)
		sub.column()
		sub.itemR(rayt, "gloss")
		sub.itemR(rayt, "gloss_threshold")
		sub.itemR(rayt, "gloss_samples")
		
		layout.column_flow()
		layout.itemR(rayt, "filter")
		layout.itemR(rayt, "limit")
		layout.itemR(rayt, "falloff")
		layout.itemR(rayt, "specular_opacity")
		layout.itemR(rayt, "depth")
		
class MATERIAL_PT_halo(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_halo"
	__label__= "Halo"
	
	def poll(self, context):
		ob = context.active_object
		halo = context.active_object.active_material.halo
		return (ob and halo.enabled)

	def draw(self, context):
		layout = self.layout
		try:		
			halo = context.active_object.active_material.halo
		except:	
			halo = None

		if not halo:
			return
		
		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemL(text="General Settings:")
		sub.itemR(halo, "size")
		sub.itemR(halo, "hardness")
		sub.itemR(halo, "add")
		
		sub = layout.sub(1)
		sub.column()
		sub.itemL(text="Elements:")
		sub.itemR(halo, "ring")
		sub.itemR(halo, "lines")
		sub.itemR(halo, "star")
		sub.itemR(halo, "flare_mode")

		layout.split(number=2)
		
		sub = layout.sub(0)
		sub.column()
		sub.itemL(text="Options:")
		sub.itemR(halo, "use_texture", text="Texture")
		sub.itemR(halo, "use_vertex_normal", text="Vertex Normal")
		sub.itemR(halo, "xalpha")
		sub.itemR(halo, "shaded")
		sub.itemR(halo, "soft")
	
		sub = layout.sub(1)
		sub.column()
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