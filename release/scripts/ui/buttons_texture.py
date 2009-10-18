
import bpy

def active_node_mat(mat):
	if mat:
		mat_node = mat.active_node_material
		if mat_node:
			return mat_node
		else:
			return mat

	return None
	
def context_tex_datablock(context):
		
		idblock = active_node_mat(context.material)
		if idblock: return idblock
		
		idblock =	context.lamp
		if idblock: return idblock
		
		idblock =	context.world
		if idblock: return idblock
		
		idblock =	context.brush
		return idblock

class TextureButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "texture"
	
	def poll(self, context):
		tex = context.texture
		return (tex and (tex.type != 'NONE' or tex.use_nodes))
		
class TEXTURE_PT_preview(TextureButtonsPanel):
	__label__ = "Preview"

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		slot = context.texture_slot
		
		idblock = context_tex_datablock(context)
		
		if idblock:
			layout.template_preview(tex, parent=idblock, slot=slot)
		else:
			layout.template_preview(tex, slot=slot)
			
class TEXTURE_PT_context_texture(TextureButtonsPanel):
	__show_header__ = False

	def poll(self, context):
		return (context.material or context.world or context.lamp or context.brush or context.texture)

	def draw(self, context):
		layout = self.layout

		tex = context.texture

		idblock = context_tex_datablock(context)
		
		space = context.space_data

		if idblock:
			row = layout.row()
			
			row.template_list(idblock, "textures", idblock, "active_texture_index", rows=2)
			
			col = row.column(align=True)
			col.item_enumO("texture.slot_move", "type", 'UP', text="", icon='ICON_TRIA_UP')
			col.item_enumO("texture.slot_move", "type", 'DOWN', text="", icon='ICON_TRIA_DOWN')
			
			
		split = layout.split(percentage=0.65)

		if idblock:
			split.template_ID(idblock, "active_texture", new="texture.new")
		elif tex:
			split.template_ID(space, "pin_id")

		if (not space.pin_id) and (
			context.sculpt_object or
			context.vertex_paint_object or
			context.weight_paint_object or
			context.texture_paint_object
		):
			split.itemR(space, "brush_texture", text="Brush", toggle=True)

		if tex:
			layout.itemR(tex, "use_nodes")
			
			split = layout.split(percentage=0.2)

			if tex.use_nodes:
				slot = context.texture_slot

				if slot:
					split.itemL(text="Output:")
					split.itemR(slot, "output_node", text="")

			else:
				split.itemL(text="Type:")
				split.itemR(tex, "type", text="")
			
class TEXTURE_PT_colors(TextureButtonsPanel):
	__label__ = "Colors"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture

		layout.itemR(tex, "use_color_ramp", text="Ramp")
		if tex.use_color_ramp:
			layout.template_color_ramp(tex, "color_ramp", expand=True)

		split = layout.split()

		col = split.column()
		col.itemL(text="RGB Multiply:")
		sub = col.column(align=True)
		sub.itemR(tex, "factor_red", text="R")
		sub.itemR(tex, "factor_green", text="G")
		sub.itemR(tex, "factor_blue", text="B")

		col = split.column()
		col.itemL(text="Adjust:")
		col.itemR(tex, "brightness")
		col.itemR(tex, "contrast")
			
# Texture Slot Panels #
			
class TextureSlotPanel(TextureButtonsPanel):
	def poll(self, context):
		return (
			context.texture_slot and 
			TextureButtonsPanel.poll(self, context)
		)
				
class TEXTURE_PT_mapping(TextureSlotPanel):
	__label__ = "Mapping"
	
	def draw(self, context):
		layout = self.layout
		
		idblock = context_tex_datablock(context)
		
		tex = context.texture_slot
		textype = context.texture

		if type(idblock) != bpy.types.Brush:
			split = layout.split(percentage=0.3)
			col = split.column()
			col.itemL(text="Coordinates:")
			col = split.column()
			col.itemR(tex, "texture_coordinates", text="")

			if tex.texture_coordinates == 'ORCO':
				"""
				ob = context.object
				if ob and ob.type == 'MESH':
					split = layout.split(percentage=0.3)
					split.itemL(text="Mesh:")
					split.itemR(ob.data, "texco_mesh", text="")
				"""
			elif tex.texture_coordinates == 'UV':
				split = layout.split(percentage=0.3)
				split.itemL(text="Layer:")
				split.itemR(tex, "uv_layer", text="")
			elif tex.texture_coordinates == 'OBJECT':
				split = layout.split(percentage=0.3)
				split.itemL(text="Object:")
				split.itemR(tex, "object", text="")
			
		if type(idblock) == bpy.types.Brush:
			layout.itemR(tex, "map_mode", expand=True)
			
			row = layout.row()
			row.active = tex.map_mode in ('FIXED', 'TILED')
			row.itemR(tex, "angle")

			row = layout.row()
			row.active = tex.map_mode in ('TILED', '3D')
			row.column().itemR(tex, "size")
		else:
			if type(idblock) == bpy.types.Material:
				split = layout.split(percentage=0.3)
				split.itemL(text="Projection:")
				split.itemR(tex, "mapping", text="")

				split = layout.split()
				
				col = split.column()
				if tex.texture_coordinates in ('ORCO', 'UV'):
					col.itemR(tex, "from_dupli")
				elif tex.texture_coordinates == 'OBJECT':
					col.itemR(tex, "from_original")
				else:
					col.itemL()
				
				col = split.column()
				row = col.row()
				row.itemR(tex, "x_mapping", text="")
				row.itemR(tex, "y_mapping", text="")
				row.itemR(tex, "z_mapping", text="")
			
			# any non brush
			row = layout.row()
			row.column().itemR(tex, "offset")
			row.column().itemR(tex, "size")
			

class TEXTURE_PT_influence(TextureSlotPanel):
	__label__ = "Influence"
	
	def draw(self, context):
		layout = self.layout
		
		idblock = context_tex_datablock(context)
		
		textype = context.texture
		tex = context.texture_slot

		def factor_but(layout, active, toggle, factor, name):
			row = layout.row(align=True)
			row.itemR(tex, toggle, text="")
			sub = row.row()
			sub.active = active
			sub.itemR(tex, factor, text=name, slider=True)
		
		if type(idblock) == bpy.types.Material:
			if idblock.type in ('SURFACE', 'HALO', 'WIRE'):
				split = layout.split()
				
				col = split.column()
				col.itemL(text="Diffuse:")
				factor_but(col, tex.map_diffuse, "map_diffuse", "diffuse_factor", "Intensity")
				factor_but(col, tex.map_colordiff, "map_colordiff", "colordiff_factor", "Color")
				factor_but(col, tex.map_alpha, "map_alpha", "alpha_factor", "Alpha")
				factor_but(col, tex.map_translucency, "map_translucency", "translucency_factor", "Translucency")

				col.itemL(text="Specular:")
				factor_but(col, tex.map_specular, "map_specular", "specular_factor", "Intensity")
				factor_but(col, tex.map_colorspec, "map_colorspec", "colorspec_factor", "Color")
				factor_but(col, tex.map_hardness, "map_hardness", "hardness_factor", "Hardness")

				col = split.column()
				col.itemL(text="Shading:")
				factor_but(col, tex.map_ambient, "map_ambient", "ambient_factor", "Ambient")
				factor_but(col, tex.map_emit, "map_emit", "emit_factor", "Emit")
				factor_but(col, tex.map_mirror, "map_mirror", "mirror_factor", "Mirror")
				factor_but(col, tex.map_raymir, "map_raymir", "raymir_factor", "Ray Mirror")

				col.itemL(text="Geometry:")
				factor_but(col, tex.map_normal, "map_normal", "normal_factor", "Normal")
				factor_but(col, tex.map_warp, "map_warp", "warp_factor", "Warp")
				factor_but(col, tex.map_displacement, "map_displacement", "displacement_factor", "Displace")

				#sub = col.column()
				#sub.active = tex.map_translucency or tex.map_emit or tex.map_alpha or tex.map_raymir or tex.map_hardness or tex.map_ambient or tex.map_specularity or tex.map_reflection or tex.map_mirror
				#sub.itemR(tex, "default_value", text="Amount", slider=True)
			elif idblock.type == 'VOLUME':
				split = layout.split()
				
				col = split.column()
				factor_but(col, tex.map_density, "map_density", "density_factor", "Density")
				factor_but(col, tex.map_emission, "map_emission", "emission_factor", "Emission")
				factor_but(col, tex.map_scattering, "map_scattering", "scattering_factor", "Scattering")
				factor_but(col, tex.map_reflection, "map_reflection", "reflection_factor", "Reflection")
				
				col = split.column()
				col.itemL(text=" ")
				factor_but(col, tex.map_alpha, "map_coloremission", "coloremission_factor", "Emission Color")
				factor_but(col, tex.map_colortransmission, "map_colortransmission", "colortransmission_factor", "Transmission Color")
				factor_but(col, tex.map_colorreflection, "map_colorreflection", "colorreflection_factor", "Reflection Color")

		elif type(idblock) == bpy.types.Lamp:
			row = layout.row()
			factor_but(row, tex.map_color, "map_color", "color_factor", "Color")
			factor_but(row, tex.map_shadow, "map_shadow", "shadow_factor", "Shadow")
			
		elif type(idblock) == bpy.types.World:
			split = layout.split()
			
			col = split.column()
			factor_but(col, tex.map_blend, "map_blend", "blend_factor", "Blend")
			factor_but(col, tex.map_horizon, "map_horizon", "horizon_factor", "Horizon")

			col = split.column()
			factor_but(col, tex.map_zenith_up, "map_zenith_up", "zenith_up_factor", "Zenith Up")
			factor_but(col, tex.map_zenith_down, "map_zenith_down", "zenith_down_factor", "Zenith Down")

		layout.itemS()
		
		split = layout.split()

		col = split.column()
		col.itemR(tex, "blend_type", text="Blend")
		col.itemR(tex, "rgb_to_intensity")
		sub = col.column()
		sub.active = tex.rgb_to_intensity
		sub.itemR(tex, "color", text="")

		col = split.column()
		col.itemR(tex, "negate", text="Negative")
		col.itemR(tex, "stencil")
		
		if type(idblock) in (bpy.types.Material, bpy.types.World):
			col.itemR(tex, "default_value", text="DVar", slider=True)

# Texture Type Panels #

class TextureTypePanel(TextureButtonsPanel):
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == self.tex_type and not tex.use_nodes)

class TEXTURE_PT_clouds(TextureTypePanel):
	__label__ = "Clouds"
	tex_type = 'CLOUDS'

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemR(tex, "stype", expand=True)
		layout.itemL(text="Noise:")
		layout.itemR(tex, "noise_type", text="Type", expand=True)
		layout.itemR(tex, "noise_basis", text="Basis")
		
		flow = layout.column_flow()
		flow.itemR(tex, "noise_size", text="Size")
		flow.itemR(tex, "noise_depth", text="Depth")
		flow.itemR(tex, "nabla", text="Nabla")

class TEXTURE_PT_wood(TextureTypePanel):
	__label__ = "Wood"
	tex_type = 'WOOD'

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemR(tex, "noisebasis2", expand=True)
		layout.itemR(tex, "stype", expand=True)
		
		col = layout.column()
		col.active = tex.stype in ('RINGNOISE', 'BANDNOISE')
		col.itemL(text="Noise:")
		col.row().itemR(tex, "noise_type", text="Type", expand=True)
		col.itemR(tex, "noise_basis", text="Basis")
		
		flow = layout.column_flow()
		flow.active = tex.stype in ('RINGNOISE', 'BANDNOISE')
		flow.itemR(tex, "noise_size", text="Size")
		flow.itemR(tex, "turbulence")
		flow.itemR(tex, "nabla")
		
class TEXTURE_PT_marble(TextureTypePanel):
	__label__ = "Marble"
	tex_type = 'MARBLE'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemR(tex, "stype", expand=True)
		layout.itemR(tex, "noisebasis2", expand=True)
		layout.itemL(text="Noise:")
		layout.itemR(tex, "noise_type", text="Type", expand=True)
		layout.itemR(tex, "noise_basis", text="Basis")
		
		flow = layout.column_flow()	
		flow.itemR(tex, "noise_size", text="Size")
		flow.itemR(tex, "noise_depth", text="Depth")
		flow.itemR(tex, "turbulence")
		flow.itemR(tex, "nabla")

class TEXTURE_PT_magic(TextureTypePanel):
	__label__ = "Magic"
	tex_type = 'MAGIC'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
			
		row = layout.row()
		row.itemR(tex, "noise_depth", text="Depth")
		row.itemR(tex, "turbulence")

class TEXTURE_PT_blend(TextureTypePanel):
	__label__ = "Blend"
	tex_type = 'BLEND'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture

		layout.itemR(tex, "progression")
		sub = layout.row()

		sub.active = (tex.progression in ('LINEAR', 'QUADRATIC', 'EASING', 'RADIAL'))
		sub.itemR(tex, "flip_axis", expand=True)
			
class TEXTURE_PT_stucci(TextureTypePanel):
	__label__ = "Stucci"
	tex_type = 'STUCCI'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemR(tex, "stype", expand=True)
		layout.itemL(text="Noise:")
		layout.itemR(tex, "noise_type", text="Type", expand=True)
		layout.itemR(tex, "noise_basis", text="Basis")
		
		row = layout.row()
		row.itemR(tex, "noise_size", text="Size")
		row.itemR(tex, "turbulence")
		
class TEXTURE_PT_image(TextureTypePanel):
	__label__ = "Image"
	tex_type = 'IMAGE'
	
	def draw(self, context):
		layout = self.layout

		tex = context.texture

		layout.template_image(tex, "image", tex.image_user)

class TEXTURE_PT_image_sampling(TextureTypePanel):
	__label__ = "Image Sampling"
	__default_closed__ = True
	tex_type = 'IMAGE'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		slot = context.texture_slot
		
		split = layout.split()
		
		"""
		col = split.column()   		
		col.itemR(tex, "flip_axis")
		col.itemR(tex, "normal_map")
		if slot:
			row = col.row()
			row.active = tex.normal_map
			row.itemR(slot, "normal_map_space", text="")
		"""

		col = split.column()
		col.itemL(text="Alpha:")
		col.itemR(tex, "use_alpha", text="Use")
		col.itemR(tex, "calculate_alpha", text="Calculate")
		col.itemR(tex, "invert_alpha", text="Invert")

		col.itemL(text="Flip:")
		col.itemR(tex, "flip_axis", text="X/Y Axis")

		col = split.column() 
		col.itemL(text="Filter:")
		col.itemR(tex, "filter", text="")
		col.itemR(tex, "mipmap")
		
		row = col.row()
		row.active = tex.mipmap
		row.itemR(tex, "mipmap_gauss", text="Gauss")
		
		col.itemR(tex, "interpolation")
		if tex.mipmap and tex.filter != 'DEFAULT':
			if tex.filter == 'FELINE':
				col.itemR(tex, "filter_probes", text="Probes")
			else:
				col.itemR(tex, "filter_eccentricity", text="Eccentricity")

class TEXTURE_PT_image_mapping(TextureTypePanel):
	__label__ = "Image Mapping"
	__default_closed__ = True
	tex_type = 'IMAGE'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemR(tex, "extension")
		
		split = layout.split()
		
		if tex.extension == 'REPEAT': 
			col = split.column(align=True)
			col.itemL(text="Repeat:")
			col.itemR(tex, "repeat_x", text="X")
			col.itemR(tex, "repeat_y", text="Y")
			
			col = split.column(align=True)
			col.itemL(text="Mirror:")
			col.itemR(tex, "mirror_x", text="X")
			col.itemR(tex, "mirror_y", text="Y")
		elif tex.extension == 'CHECKER': 
			col = split.column(align=True)
			row = col.row()
			row.itemR(tex, "checker_even", text="Even")
			row.itemR(tex, "checker_odd", text="Odd")

			split.itemR(tex, "checker_distance", text="Distance")

		layout.itemS()

		split = layout.split()
		
		col = split.column(align=True)
		#col.itemR(tex, "crop_rectangle")
		col.itemL(text="Crop Minimum:")
		col.itemR(tex, "crop_min_x", text="X")
		col.itemR(tex, "crop_min_y", text="Y")
		
		col = split.column(align=True)
		col.itemL(text="Crop Maximum:")
		col.itemR(tex, "crop_max_x", text="X")
		col.itemR(tex, "crop_max_y", text="Y")
	
class TEXTURE_PT_plugin(TextureTypePanel):
	__label__ = "Plugin"
	tex_type = 'PLUGIN'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemL(text="Nothing yet")
		
class TEXTURE_PT_envmap(TextureTypePanel):
	__label__ = "Environment Map"
	tex_type = 'ENVIRONMENT_MAP'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemL(text="Nothing yet")
		
class TEXTURE_PT_musgrave(TextureTypePanel):
	__label__ = "Musgrave"
	tex_type = 'MUSGRAVE'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		layout.itemR(tex, "musgrave_type")	
		
		split = layout.split()
		
		col = split.column()
		col.itemR(tex, "highest_dimension", text="Dimension")
		col.itemR(tex, "lacunarity")
		col.itemR(tex, "octaves")
		
		col = split.column() 
		if (tex.musgrave_type in ('HETERO_TERRAIN', 'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL')):
			col.itemR(tex, "offset")
		if (tex.musgrave_type in ('RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL')):
			col.itemR(tex, "gain")
			col.itemR(tex, "noise_intensity", text="Intensity")
		
		layout.itemL(text="Noise:")
		
		layout.itemR(tex, "noise_basis", text="Basis")
		
		row = layout.row()
		row.itemR(tex, "noise_size", text="Size")
		row.itemR(tex, "nabla")

class TEXTURE_PT_voronoi(TextureTypePanel):
	__label__ = "Voronoi"
	tex_type = 'VORONOI'

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		
		split = layout.split()
		
		col = split.column()   
		col.itemL(text="Distance Metric:")
		col.itemR(tex, "distance_metric", text="")
		sub = col.column()
		sub.active = tex.distance_metric == 'MINKOVSKY'
		sub.itemR(tex, "minkovsky_exponent", text="Exponent")
		col.itemL(text="Coloring:")
		col.itemR(tex, "coloring", text="")
		col.itemR(tex, "noise_intensity", text="Intensity")
		
		col = split.column(align=True) 
		col.itemL(text="Feature Weights:")
		col.itemR(tex, "weight_1", text="1", slider=True)
		col.itemR(tex, "weight_2", text="2", slider=True)
		col.itemR(tex, "weight_3", text="3", slider=True)
		col.itemR(tex, "weight_4", text="4", slider=True)
		
		layout.itemL(text="Noise:")
		
		row = layout.row()
		row.itemR(tex, "noise_size", text="Size")
		row.itemR(tex, "nabla")
			
class TEXTURE_PT_distortednoise(TextureTypePanel):
	__label__ = "Distorted Noise"
	tex_type = 'DISTORTED_NOISE'
	
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture

		layout.itemR(tex, "noise_distortion")
		layout.itemR(tex, "noise_basis", text="Basis")
		
		flow = layout.column_flow()
		flow.itemR(tex, "distortion", text="Distortion")
		flow.itemR(tex, "noise_size", text="Size")
		flow.itemR(tex, "nabla")	
		
class TEXTURE_PT_voxeldata(TextureButtonsPanel):
	__label__ = "Voxel Data"

	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'VOXEL_DATA')

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		vd = tex.voxeldata

		layout.itemR(vd, "file_format")
		if vd.file_format in ['BLENDER_VOXEL', 'RAW_8BIT']:
			layout.itemR(vd, "source_path")
		if vd.file_format == 'RAW_8BIT':
			layout.itemR(vd, "resolution")
		elif vd.file_format == 'SMOKE':
			layout.itemR(vd, "domain_object")
		
		layout.itemR(vd, "still")
		row = layout.row()
		row.active = vd.still
		row.itemR(vd, "still_frame_number")
		
		layout.itemR(vd, "interpolation")
		layout.itemR(vd, "extension")
		layout.itemR(vd, "intensity")
		
class TEXTURE_PT_pointdensity(TextureButtonsPanel):
	__label__ = "Point Density"

	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'POINT_DENSITY')

	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		pd = tex.pointdensity
		
		layout.itemR(pd, "point_source", expand=True)

		split = layout.split()
		
		col = split.column()
		if pd.point_source == 'PARTICLE_SYSTEM':
			col.itemL(text="Object:")
			col.itemR(pd, "object", text="")
			
			sub = col.column()
			sub.enabled = pd.object
			if pd.object:
				sub.itemL(text="System:")
				sub.item_pointerR(pd, "particle_system", pd.object, "particle_systems", text="")
			sub.itemL(text="Cache:")
			sub.itemR(pd, "particle_cache", text="")
		else:
			col.itemL(text="Object:")
			col.itemR(pd, "object", text="")
			col.itemL(text="Cache:")
			col.itemR(pd, "vertices_cache", text="")
		
		col.itemS()
		
		col.itemL(text="Color Source:")	
		col.itemR(pd, "color_source", text="")
		if pd.color_source in ('PARTICLE_SPEED', 'PARTICLE_VELOCITY'):
			col.itemR(pd, "speed_scale")
		if pd.color_source in ('PARTICLE_SPEED', 'PARTICLE_AGE'):
			layout.template_color_ramp(pd, "color_ramp", expand=True)

		col = split.column()
		col.itemL()
		col.itemR(pd, "radius")
		col.itemL(text="Falloff:")
		col.itemR(pd, "falloff", text="")
		if pd.falloff == 'SOFT':
			col.itemR(pd, "falloff_softness")

class TEXTURE_PT_pointdensity_turbulence(TextureButtonsPanel):
	__label__ = "Turbulence"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'POINT_DENSITY')
		
	def draw_header(self, context):
		layout = self.layout
		
		tex = context.texture
		pd = tex.pointdensity
		
		layout.itemR(pd, "turbulence", text="")
		
	def draw(self, context):
		layout = self.layout
		
		tex = context.texture
		pd = tex.pointdensity
		layout.active = pd.turbulence

		split = layout.split()
		
		col = split.column()
		col.itemL(text="Influence:")
		col.itemR(pd, "turbulence_influence", text="")
		col.itemL(text="Noise Basis:")
		col.itemR(pd, "noise_basis", text="")
		
		col = split.column()
		col.itemL()		
		col.itemR(pd, "turbulence_size")
		col.itemR(pd, "turbulence_depth")
		col.itemR(pd, "turbulence_strength")

bpy.types.register(TEXTURE_PT_context_texture)
bpy.types.register(TEXTURE_PT_preview)

bpy.types.register(TEXTURE_PT_clouds) # Texture Type Panels
bpy.types.register(TEXTURE_PT_wood)
bpy.types.register(TEXTURE_PT_marble)
bpy.types.register(TEXTURE_PT_magic)
bpy.types.register(TEXTURE_PT_blend)
bpy.types.register(TEXTURE_PT_stucci)
bpy.types.register(TEXTURE_PT_image)
bpy.types.register(TEXTURE_PT_image_sampling)
bpy.types.register(TEXTURE_PT_image_mapping)
bpy.types.register(TEXTURE_PT_plugin)
bpy.types.register(TEXTURE_PT_envmap)
bpy.types.register(TEXTURE_PT_musgrave)
bpy.types.register(TEXTURE_PT_voronoi)
bpy.types.register(TEXTURE_PT_distortednoise)
bpy.types.register(TEXTURE_PT_voxeldata)
bpy.types.register(TEXTURE_PT_pointdensity)
bpy.types.register(TEXTURE_PT_pointdensity_turbulence)

bpy.types.register(TEXTURE_PT_colors)
bpy.types.register(TEXTURE_PT_mapping)
bpy.types.register(TEXTURE_PT_influence)

