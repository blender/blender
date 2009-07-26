	
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

	def draw(self, context):
		layout = self.layout
		mat = context.material
		
		layout.template_preview(mat)
		
class MATERIAL_PT_context_material(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_context_material"
	__show_header__ = False

	def poll(self, context):
		return (context.object)

	def draw(self, context):
		layout = self.layout
		
		mat = context.material
		ob = context.object
		slot = context.material_slot
		space = context.space_data

		if ob:
			row = layout.row()

			row.template_list(ob, "materials", ob, "active_material_index", rows=2)

			col = row.column(align=True)
			col.itemO("object.material_slot_add", icon="ICON_ZOOMIN", text="")
			col.itemO("object.material_slot_remove", icon="ICON_ZOOMOUT", text="")

			if context.edit_object:
				row = layout.row(align=True)

				row.itemO("object.material_slot_assign", text="Assign")
				row.itemO("object.material_slot_select", text="Select")
				row.itemO("object.material_slot_deselect", text="Deselect")

		split = layout.split(percentage=0.65)

		if ob and slot:
			split.template_ID(slot, "material", new="material.new")
			row = split.row()
			row.itemR(slot, "link", expand=True)
		elif mat:
			split.template_ID(space, "pin_id")
			split.itemS()
	
class MATERIAL_PT_material(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_material"
	__label__ = "Shading"

	def draw(self, context):
		layout = self.layout
		
		mat = context.material
		ob = context.object
		slot = context.material_slot
		space = context.space_data

		if mat:
			layout.itemR(mat, "type", expand=True)

			if mat.type in ('SURFACE', 'WIRE', 'VOLUME'):
				split = layout.split()
	
				sub = split.column()
				sub.itemR(mat, "alpha", slider=True)
				sub.itemR(mat, "ambient", slider=True)
				sub.itemR(mat, "emit")
				sub.itemR(mat, "translucency", slider=True)
				
				sub = split.column()
				sub.itemR(mat, "z_transparency")
				sub.itemR(mat, "shadeless")	
				sub.itemR(mat, "tangent_shading")
				sub.itemR(mat, "cubic", slider=True)
				
			elif mat.type == 'HALO':
				layout.itemR(mat, "alpha", slider=True)
			
class MATERIAL_PT_strand(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_strand"
	__label__ = "Strand"
	__default_closed__ = True
	
	def draw(self, context):
		layout = self.layout
		tan = context.material.strand
		mat = context.material
		
		split = layout.split()
		
		sub = split.column()
		sub.itemL(text="Size:")
		sub.itemR(tan, "start_size", text="Root")
		sub.itemR(tan, "end_size", text="Tip")
		sub.itemR(tan, "min_size", text="Minimum")
		sub.itemR(tan, "blender_units")
		colsub = sub.column()
		colsub.active = mat.shadeless== False
		colsub.itemR(tan, "tangent_shading")
		
		sub = split.column()
		sub.itemR(tan, "shape")
		sub.itemR(tan, "width_fade")
		sub.itemR(tan, "uv_layer")
		colsub = sub.column()
		colsub.active = mat.shadeless== False
		colsub.itemR(tan, "surface_diffuse")
		colsubsub = colsub.column()
		colsubsub.active = tan.surface_diffuse
		colsubsub.itemR(tan, "blend_distance", text="Distance")
		
class MATERIAL_PT_options(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_options"
	__label__ = "Options"

	def draw(self, context):
		layout = self.layout
		mat = context.material
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(mat, "traceable")
		sub.itemR(mat, "full_oversampling")
		sub.itemR(mat, "sky")
		sub.itemR(mat, "exclude_mist")
		sub.itemR(mat, "invert_z")

		col = sub.column(align=True)
		col.itemL(text="Light Group:")
		col.itemR(mat, "light_group", text="")
		row = col.row()
		row.active = mat.light_group
		row.itemR(mat, "light_group_exclusive", text="Exclusive")

		sub = split.column()
		sub.itemR(mat, "face_texture")
		colsub = sub.column()
		colsub.active = mat.face_texture
		colsub.itemR(mat, "face_texture_alpha")
		sub.itemR(mat, "vertex_color_paint")
		sub.itemR(mat, "vertex_color_light")
		sub.itemR(mat, "object_color")

class MATERIAL_PT_shadows(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_shadows"
	__label__ = "Shadows"

	def draw(self, context):
		layout = self.layout
		mat = context.material
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(mat, "shadows", text="Receive")
		sub.itemR(mat, "transparent_shadows", text="Receive Transparent")
		sub.itemR(mat, "only_shadow", text="Shadows Only")
		sub.itemR(mat, "cast_shadows_only", text="Cast Only")
		sub.itemR(mat, "shadow_casting_alpha", text="Casting Alpha", slider=True)
		sub = split.column()
		sub.itemR(mat, "ray_shadow_bias", text="Auto Ray Bias")
		colsub = sub.column()
		colsub.active = not mat.ray_shadow_bias
		colsub.itemR(mat, "shadow_ray_bias", text="Ray Shadow Bias")
		sub.itemR(mat, "cast_buffer_shadows")
		sub.itemR(mat, "shadow_buffer_bias", text="Buffer Bias")


class MATERIAL_PT_diffuse(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_diffuse"
	__label__ = "Diffuse"

	def poll(self, context):
		mat = context.material
		return (mat and mat.type != 'HALO')

	def draw(self, context):
		layout = self.layout
		mat = context.material	
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(mat, "diffuse_color", text="")
		row = sub.row()
		row.active = mat.shadeless== False
		row.itemR(mat, "diffuse_reflection", text="Intensity", slider=True)
		
		sub = split.column()
		sub.active = mat.shadeless== False
		sub.itemR(mat, "diffuse_shader", text="")
		sub.itemR(mat, "use_diffuse_ramp", text="Ramp")
		
		split = layout.split()
		split.active = mat.shadeless== False
		sub = split.column()
		if mat.diffuse_shader == 'OREN_NAYAR':
			sub.itemR(mat, "roughness")
		if mat.diffuse_shader == 'MINNAERT':
			sub.itemR(mat, "darkness")
		if mat.diffuse_shader == 'TOON':
			sub.itemR(mat, "diffuse_toon_size", text="Size")
			sub = split.column()
			sub.itemR(mat, "diffuse_toon_smooth", text="Smooth")
		if mat.diffuse_shader == 'FRESNEL':
			sub.itemR(mat, "diffuse_fresnel", text="Fresnel")
			sub = split.column()
			sub.itemR(mat, "diffuse_fresnel_factor", text="Factor")
		
		if mat.use_diffuse_ramp:
			layout.template_color_ramp(mat.diffuse_ramp, expand=True)
		
class MATERIAL_PT_specular(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_specular"
	__label__ = "Specular"

	def poll(self, context):
		mat = context.material
		return (mat and mat.type != 'HALO')

	def draw(self, context):
		layout = self.layout
		mat = context.material
		
		layout.active = mat.shadeless== False
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(mat, "specular_color", text="")
		sub.itemR(mat, "specular_reflection", text="Intensity", slider=True)

		sub = split.column()
		sub.itemR(mat, "specular_shader", text="")
		sub.itemR(mat, "use_specular_ramp", text="Ramp")
		
		split = layout.split()
		
		sub = split.column()
		if mat.specular_shader in ('COOKTORR', 'PHONG'):
			sub.itemR(mat, "specular_hardness", text="Hardness")
		if mat.specular_shader == 'BLINN':
			sub.itemR(mat, "specular_hardness", text="Hardness")
			sub = split.column()
			sub.itemR(mat, "specular_ior", text="IOR")
		if mat.specular_shader == 'WARDISO':
			sub.itemR(mat, "specular_slope", text="Slope")
		if mat.specular_shader == 'TOON':
			sub.itemR(mat, "specular_toon_size", text="Size")
			sub = split.column()
			sub.itemR(mat, "specular_toon_smooth", text="Smooth")
		
		if mat.use_specular_ramp:
			layout.template_color_ramp(mat.specular_ramp, expand=True)
		
class MATERIAL_PT_sss(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_sss"
	__label__ = "Subsurface Scattering"
	__default_closed__ = True
	
	def poll(self, context):
		mat = context.material
		return (mat and (mat.type == 'SURFACE' or mat.type == 'WIRE'))

	def draw_header(self, context):
		layout = self.layout
		sss = context.material.subsurface_scattering

		layout.itemR(sss, "enabled", text="")
	
	def draw(self, context):
		layout = self.layout
		sss = context.material.subsurface_scattering
		mat = context.material
		layout.active = sss.enabled	
		
		split = layout.split()
		split.active = mat.shadeless== False
		
		sub = split.column()
		sub.itemR(sss, "color", text="")
		sub.itemL(text="Blend:")
		sub.itemR(sss, "color_factor", slider=True)
		sub.itemR(sss, "texture_factor", slider=True)
		sub.itemL(text="Scattering Weight:")
		sub.itemR(sss, "front")
		sub.itemR(sss, "back")
		
		sub = split.column()
		sub.itemR(sss, "ior")
		sub.itemR(sss, "scale")
		sub.itemR(sss, "radius", text="RGB Radius")
		sub.itemR(sss, "error_tolerance")

class MATERIAL_PT_raymir(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raymir"
	__label__ = "Ray Mirror"
	__default_closed__ = True
	
	def poll(self, context):
		mat = context.material
		return (mat and (mat.type == 'SURFACE' or mat.type == 'WIRE'))
	
	def draw_header(self, context):
		layout = self.layout
		raym = context.material.raytrace_mirror

		layout.itemR(raym, "enabled", text="")
	
	def draw(self, context):
		layout = self.layout
		raym = context.material.raytrace_mirror
		mat = context.material
		
		layout.active = raym.enabled
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(raym, "reflect", text="Reflectivity", slider=True)
		sub.itemR(mat, "mirror_color", text="")
		sub.itemR(raym, "fresnel")
		sub.itemR(raym, "fresnel_fac", text="Fac", slider=True)
		
		sub = split.column()
		sub.itemR(raym, "gloss", slider=True)
		colsub = sub.column()
		colsub.active = raym.gloss < 1
		colsub.itemR(raym, "gloss_threshold", slider=True, text="Threshold")
		colsub.itemR(raym, "gloss_samples", text="Samples")
		colsub.itemR(raym, "gloss_anisotropic", slider=True, text="Anisotropic")
		
		row = layout.row()
		row.itemR(raym, "distance", text="Max Dist")
		row.itemR(raym, "depth")
		
		layout.itemR(raym, "fade_to")
		
class MATERIAL_PT_raytransp(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_raytransp"
	__label__= "Ray Transparency"
	__default_closed__ = True
		
	def poll(self, context):
		mat = context.material
		return (mat and (mat.type == 'SURFACE' or mat.type == 'WIRE'))

	def draw_header(self, context):
		layout = self.layout
		rayt = context.material.raytrace_transparency

		layout.itemR(rayt, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		rayt = context.material.raytrace_transparency
		mat = context.material
		
		layout.active = rayt.enabled	
		
		split = layout.split()
		split.active = mat.shadeless== False
		
		sub = split.column()
		sub.itemR(rayt, "ior")
		sub.itemR(rayt, "fresnel")
		sub.itemR(rayt, "fresnel_fac", text="Fac", slider=True)
		
		sub = split.column()
		sub.itemR(rayt, "gloss", slider=True)
		colsub = sub.column()
		colsub.active = rayt.gloss < 1
		colsub.itemR(rayt, "gloss_threshold", slider=True, text="Threshold")
		colsub.itemR(rayt, "gloss_samples", text="Samples")
		
		flow = layout.column_flow()
		flow.active = mat.shadeless== False
		flow.itemR(rayt, "filter", slider=True)
		flow.itemR(rayt, "limit")
		flow.itemR(rayt, "falloff")
		flow.itemR(rayt, "specular_opacity", slider=True, text="Spec Opacity")
		flow.itemR(rayt, "depth")
		
class MATERIAL_PT_halo(MaterialButtonsPanel):
	__idname__= "MATERIAL_PT_halo"
	__label__= "Halo"
	
	def poll(self, context):
		mat = context.material
		return (mat and mat.type == 'HALO')
	
	def draw(self, context):
		layout = self.layout
		
		mat = context.material
		halo = mat.halo

		split = layout.split()
		
		col = split.column()
		col.itemR(mat, "diffuse_color", text="")
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
		col = col.column()
		col.itemR(halo, "ring")
		colsub = col.column()
		colsub.active = halo.ring
		colsub.itemR(halo, "rings")
		colsub.itemR(mat, "mirror_color", text="")
		col.itemR(halo, "lines")
		colsub = col.column()
		colsub.active = halo.lines
		colsub.itemR(halo, "line_number", text="Lines")
		colsub.itemR(mat, "specular_color", text="")
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


bpy.types.register(MATERIAL_PT_context_material)
bpy.types.register(MATERIAL_PT_preview)
bpy.types.register(MATERIAL_PT_material)
bpy.types.register(MATERIAL_PT_diffuse)
bpy.types.register(MATERIAL_PT_specular)
bpy.types.register(MATERIAL_PT_raymir)
bpy.types.register(MATERIAL_PT_raytransp)
bpy.types.register(MATERIAL_PT_sss)
bpy.types.register(MATERIAL_PT_halo)
bpy.types.register(MATERIAL_PT_strand)
bpy.types.register(MATERIAL_PT_options)
bpy.types.register(MATERIAL_PT_shadows)
