
import bpy

class MaterialButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "material"

	def poll(self, context):
		return (context.material != None)

class MATERIAL_PT_preview(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_preview"
	__label__ = "Preview"

	def poll(self, context):
		return (context.material or context.object)

	def draw(self, context):
		layout = self.layout

		mat = context.material
		layout.template_preview(mat)
	
class MATERIAL_PT_material(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_material"
	__label__ = "Material"

	def poll(self, context):
		return (context.material or context.object)

	def draw(self, context):
		layout = self.layout
		mat = context.material
		ob = context.object
		slot = context.material_slot
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob and slot:
			split.template_ID(context, slot, "material", new="MATERIAL_OT_new")
			split.itemR(ob, "active_material_index", text="Active")
		elif mat:
			split.template_ID(context, space, "pin_id")
			split.itemS()

		if mat:
			layout.itemS()
		
			layout.itemR(mat, "type", expand=True)

			row = layout.row()
			row.column().itemR(mat, "diffuse_color")
			row.column().itemR(mat, "specular_color")
			row.column().itemR(mat, "mirror_color")
			
			layout.itemR(mat, "alpha", slider=True)
			
class MATERIAL_PT_sss(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_sss"
	__label__ = "Subsurface Scattering"

	def poll(self, context):
		mat = context.material
		return (mat and mat.type == "SURFACE")

	def draw_header(self, context):
		sss = context.material.subsurface_scattering

		layout = self.layout
		layout.itemR(sss, "enabled", text="")
	
	def draw(self, context):
		layout = self.layout
		sss = context.material.subsurface_scattering
		layout.active = sss.enabled	
		
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
		mat = context.material
		return (mat and mat.type == "SURFACE")
	
	def draw_header(self, context):
		raym = context.material.raytrace_mirror

		layout = self.layout
		layout.itemR(raym, "enabled", text="")
	
	def draw(self, context):
		layout = self.layout
		raym = context.material.raytrace_mirror
		layout.active = raym.enabled	
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
		
		row = layout.row()
		row.itemR(raym, "distance", text="Max Dist")
		row.itemR(raym, "depth")
		
		layout.itemR(raym, "fade_to")
		
class MATERIAL_PT_raytransp(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raytransp"
	__label__= "Ray Transparency"
	
	def poll(self, context):
		mat = context.material
		return (mat and mat.type == "SURFACE")

	def draw_header(self, context):
		rayt = context.material.raytrace_transparency

		layout = self.layout
		layout.itemR(rayt, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		rayt = context.material.raytrace_transparency
		layout.active = rayt.enabled	
		
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
		mat = context.material
		return (mat and mat.type == "HALO")
	
	def draw(self, context):
		layout = self.layout
		mat = context.material
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
		col = col.column(align=True)
		col.itemR(halo, "ring")
		colsub = col.column()
		colsub.active = halo.ring
		colsub.itemR(halo, "rings")
		col.itemR(halo, "lines")
		colsub = col.column()
		colsub.active = halo.lines
		colsub.itemR(halo, "line_number", text="Lines")
		col.itemR(halo, "star")
		colsub = col.column()
		colsub.active = halo.star
		colsub.itemR(halo, "star_tips")
		col.itemR(halo, "flare_mode")
		colsub = col.column()
		colsub.active = halo.flare_mode
		colsub.itemR(halo, "flare_size", text="Size")
		colsub.itemR(halo, "flare_subsize", text="Subsize")
		colsub.itemR(halo, "flare_boost", text="Boost")
		colsub.itemR(halo, "flare_seed", text="Seed")
		colsub.itemR(halo, "flares_sub", text="Sub")

bpy.types.register(MATERIAL_PT_preview)
bpy.types.register(MATERIAL_PT_material)
bpy.types.register(MATERIAL_PT_raymir)
bpy.types.register(MATERIAL_PT_raytransp)
bpy.types.register(MATERIAL_PT_sss)
bpy.types.register(MATERIAL_PT_halo)

