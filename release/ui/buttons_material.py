
import bpy

class MaterialButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "material"

	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_material)
	
class MATERIAL_PT_material(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_material"
	__label__ = "Material"

	def draw(self, context):
		layout = self.layout
		mat = context.active_object.active_material
	
		row = layout.row()
		row.itemR(mat, "type", expand=True)

		row = layout.row()
		row.column().itemR(mat, "diffuse_color")
		row.column().itemR(mat, "specular_color")
		row.column().itemR(mat, "mirror_color")
		
		row = layout.row()
		row.itemR(mat, "alpha", slider=True)
			
class MATERIAL_PT_sss(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_sss"
	__label__ = "Subsurface Scattering"

	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_material and ob.active_material.type == "SURFACE")

	def draw_header(self, context):
		sss = context.active_object.active_material.subsurface_scattering

		layout = self.layout
		layout.itemR(sss, "enabled", text="")
	
	def draw(self, context):
		layout = self.layout
		sss = context.active_object.active_material.subsurface_scattering
		
		flow = layout.column_flow()
		flow.itemR(sss, "error_tolerance")
		flow.itemR(sss, "ior")
		flow.itemR(sss, "scale")
		
		row = layout.row()
		row.column().itemR(sss, "color")
		row.column().itemR(sss, "radius")
		
		flow = layout.column_flow()
		flow.itemR(sss, "color_factor", slider=True)
		flow.itemR(sss, "texture_factor", slider=True)
		flow.itemR(sss, "front")
		flow.itemR(sss, "back")
		
class MATERIAL_PT_raymir(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raymir"
	__label__ = "Ray Mirror"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_material and ob.active_material.type == "SURFACE")
	
	def draw_header(self, context):
		raym = context.active_object.active_material.raytrace_mirror

		layout = self.layout
		layout.itemR(raym, "enabled", text="")
	
	def draw(self, context):
		layout = self.layout
		raym = context.active_object.active_material.raytrace_mirror

		split = layout.split()
		
		sub = split.column()
		sub.itemR(raym, "reflect", text="RayMir", slider=True)
		sub.itemR(raym, "fresnel")
		sub.itemR(raym, "fresnel_fac", text="Fac", slider=True)
		
		sub = split.column()
		sub.itemR(raym, "gloss", slider=True)
		sub.itemR(raym, "gloss_threshold", slider=True)
		sub.itemR(raym, "gloss_samples")
		sub.itemR(raym, "gloss_anisotropic", slider=True)
		
		flow = layout.column_flow()
		flow.itemR(raym, "distance", text="Max Dist")
		flow.itemR(raym, "depth")
		flow.itemR(raym, "fade_to")
		
class MATERIAL_PT_raytransp(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raytransp"
	__label__= "Ray Transparency"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_material and ob.active_material.type == "SURFACE")

	def draw_header(self, context):
		rayt = context.active_object.active_material.raytrace_transparency

		layout = self.layout
		layout.itemR(rayt, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		rayt = context.active_object.active_material.raytrace_transparency
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(rayt, "ior")
		sub.itemR(rayt, "fresnel")
		sub.itemR(rayt, "fresnel_fac", text="Fac", slider=True)
		
		sub = split.column()
		sub.itemR(rayt, "gloss", slider=True)
		sub.itemR(rayt, "gloss_threshold", slider=True)
		sub.itemR(rayt, "gloss_samples")
		
		flow = layout.column_flow()
		flow.itemR(rayt, "filter", slider=True)
		flow.itemR(rayt, "limit")
		flow.itemR(rayt, "falloff")
		flow.itemR(rayt, "specular_opacity", slider=True)
		flow.itemR(rayt, "depth")
		
class MATERIAL_PT_halo(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_halo"
	__label__= "Halo"
	
	def poll(self, context):
		ob = context.active_object
		return (ob and ob.active_material and ob.active_material.type == "HALO")
	
	def draw(self, context):
		layout = self.layout
		mat = context.active_object.active_material
		halo = mat.halo

		split = layout.split()
		
		col = split.column(align=True)
		col.itemL(text="General Settings:")
		col.itemR(halo, "size")
		col.itemR(halo, "hardness")
		col.itemR(halo, "add", slider=True)
		
		col.itemL(text="Options:")
		col.itemR(halo, "use_texture", text="Texture")
		col.itemR(halo, "use_vertex_normal", text="Vertex Normal")
		col.itemR(halo, "xalpha")
		col.itemR(halo, "shaded")
		col.itemR(halo, "soft")

		col = split.column()
		sub = col.column(align=True)
		sub.itemL(text="Elements:")
		sub.itemR(halo, "ring")
		sub.itemR(halo, "lines")
		sub.itemR(halo, "star")
		sub.itemR(halo, "flare_mode")
		
		sub = col.column()
		if (halo.ring):
			sub.itemR(halo, "rings")
		if (halo.lines):
			sub.itemR(halo, "line_number")
		if (halo.ring or halo.lines):
			sub.itemR(halo, "seed")
		if (halo.star):
			sub.itemR(halo, "star_tips")
		if (halo.flare_mode):
			sub = col.column(align=True)
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
