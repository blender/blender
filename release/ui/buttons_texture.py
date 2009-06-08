		
import bpy

class TextureButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "texture"
	
	def poll(self, context):
		return (context.texture != None)

class TEXTURE_PT_preview(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_preview"
	__label__ = "Preview"

	def poll(self, context):
		return (context.texture or context.material)

	def draw(self, context):
		layout = self.layout

		tex = context.texture
		layout.template_preview(tex)

class TEXTURE_PT_texture(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_texture"
	__label__ = "Texture"

	def poll(self, context):
		return (context.texture or context.material or context.world or context.lamp)

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		ma = context.material
		la = context.lamp
		wo = context.world
		space = context.space_data
		slot = context.texture_slot

		split = layout.split(percentage=0.65)

		if ma or la or wo:
			if slot:
				split.template_ID(context, slot, "texture", new="TEXTURE_OT_new")
			else:
				split.itemS()

			if ma:
				split.itemR(ma, "active_texture_index", text="Active")
			elif la:
				split.itemR(la, "active_texture_index", text="Active")
			elif wo:
				split.itemR(wo, "active_texture_index", text="Active")
		elif tex:
			split.template_ID(context, space, "pin_id")
			split.itemS()

		layout.itemS()
		
		if tex:
			split = layout.split(percentage=0.2)
		
			col = split.column()
			col.itemL(text="Type:")
			col = split.column()
			col.itemR(tex, "type", text="")

class TEXTURE_PT_map(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_map"
	__label__ = "Map"

	def draw(self, context):
		layout = self.layout
		tex = context.texture_slot

		split = layout.split(percentage=0.3)
		col = split.column()
		col.itemL(text="Coordinates:")
		col = split.column()
		col.itemR(tex, "texture_coordinates", text="")
		
		split = layout.split()
		col = split.column()
		if tex.texture_coordinates == 'UV':
			col.itemR(tex, "uv_layer")
		elif tex.texture_coordinates == 'OBJECT':
			col.itemR(tex, "object")
			
		col = split.column()
		col.itemR(tex, "from_dupli")
		
		split = layout.split()
		col = split.column()
		col.itemR(tex, "mapping")
		col = split.column()
		rowsub = col.row()
		rowsub.itemL(text="TODO:X")
		rowsub.itemL(text="TODO:Y")
		rowsub.itemL(text="TODO:Z")
		
		split = layout.split()
		col = split.column()
		col.itemR(tex, "offset")
		col = split.column()
		col.itemR(tex, "size")
	
		row = layout.row()
		row.itemL(text="Affect:")
		
		split = layout.split()
		col = split.column()
		col.itemL(text="TODO: Diffuse Color")
		col.itemR(tex, "color_factor")
		col.itemR(tex, "blend_type")
		col.itemR(tex, "no_rgb")
		colsub = col.column()
		colsub.active = tex.no_rgb
		colsub.itemR(tex, "color")
		col.itemL(text="TODO: Normal")
		col.itemR(tex, "normal_factor")
		col.itemR(tex, "normal_map_space")
		col.itemL(text="TODO: Warp")
		col.itemR(tex, "warp_factor")
		col.itemL(text="TODO: Specular Color")
		col.itemL(text="TODO: Displacement")
		col.itemR(tex, "displacement_factor")
		col = split.column()
		col.itemL(text="TODO: Mirror Color")
		col.itemL(text="TODO: Reflection")
		col.itemL(text="TODO: Specularity")
		col.itemL(text="TODO: Ambient")
		col.itemL(text="TODO: Hard")
		col.itemL(text="TODO: Ray Mirror")
		col.itemL(text="TODO: Alpha")
		col.itemL(text="TODO: Emit")
		col.itemL(text="TODO: Translucency")

		col.itemR(tex, "default_value")
		
		split = layout.split()
		col = split.column()
		col.itemR(tex, "stencil")
		col = split.column()
		col.itemR(tex, "negate", text="Negative")
		


class TEXTURE_PT_colors(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_colors"
	__label__ = "Colors"

	def draw(self, context):
		layout = self.layout
		tex = context.texture

		if tex.color_ramp:
			layout.template_color_ramp(tex.color_ramp, expand=True)
		else:
			split = layout.split()
			col = split.column()
			col.itemR(tex, "rgb_factor")

		col = split.column()
		col.itemL(text="Adjust:")
		col.itemR(tex, "brightness")
		col.itemR(tex, "contrast")

class TEXTURE_PT_clouds(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_clouds"
	__label__ = "Clouds"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'CLOUDS')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		layout.itemR(tex, "stype", expand=True)
		layout.itemL(text="Noise:")
		layout.itemR(tex, "noise_type", text="Type", expand=True)
		layout.itemR(tex, "noise_basis", text="Basis")
		
		col = layout.column_flow()
		col.itemR(tex, "noise_size", text="Size")
		col.itemR(tex, "noise_depth", text="Depth")
		col.itemR(tex, "nabla", text="Nabla")

class TEXTURE_PT_wood(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_wood"
	__label__ = "Wood"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'WOOD')

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
		
		col = layout.column_flow()
		col.active = tex.stype in ('RINGNOISE', 'BANDNOISE')
		col.itemR(tex, "noise_size", text="Size")
		col.itemR(tex, "turbulence")
		col.itemR(tex, "nabla")
		
class TEXTURE_PT_marble(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_marble"
	__label__ = "Marble"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'MARBLE')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		layout.itemR(tex, "stype", expand=True)
		layout.itemR(tex, "noisebasis2", expand=True)
		layout.itemL(text="Noise:")
		layout.itemR(tex, "noise_type", text="Type", expand=True)
		layout.itemR(tex, "noise_basis", text="Basis")
		
		col = layout.column_flow()	
		col.itemR(tex, "noise_size", text="Size")
		col.itemR(tex, "noise_depth", text="Depth")
		col.itemR(tex, "turbulence")
		col.itemR(tex, "nabla")

class TEXTURE_PT_magic(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_magic"
	__label__ = "Magic"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'MAGIC')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
			
		row = layout.row()
		row.itemR(tex, "noise_depth", text="Depth")
		row.itemR(tex, "turbulence")

class TEXTURE_PT_blend(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_blend"
	__label__ = "Blend"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'BLEND')

	def draw(self, context):
		layout = self.layout
		tex = context.texture

		layout.itemR(tex, "progression")
		layout.itemR(tex, "flip_axis")
			
class TEXTURE_PT_stucci(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_stucci"
	__label__ = "Stucci"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'STUCCI')

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
		
class TEXTURE_PT_image(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_image"
	__label__ = "Image/Movie"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'IMAGE')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		split = layout.split()
		
		sub = split.column()   		
		sub.itemR(tex, "flip_axis")
		sub.itemR(tex, "normal_map")
		sub.itemL(text="Filter:")
		sub.itemR(tex, "mipmap")
		sub.itemR(tex, "mipmap_gauss")
		sub.itemR(tex, "interpolation")
		sub = split.column() 
		sub.itemL(text="Alpha:")
		sub.itemR(tex, "use_alpha")
		sub.itemR(tex, "calculate_alpha")
		sub.itemR(tex, "invert_alpha")

class TEXTURE_PT_mapping(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_mapping"
	__label__ = "Mapping"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'IMAGE')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
				
		split = layout.split()
		
		sub = split.column()
		#sub.itemR(tex, "crop_rectangle")
		sub.itemL(text="Crop Minimum:")
		sub.itemR(tex, "crop_min_x", text="X")
		sub.itemR(tex, "crop_min_y", text="Y")
		sub = split.column()
		sub.itemL(text="Crop Maximum:")
		sub.itemR(tex, "crop_max_x", text="X")
		sub.itemR(tex, "crop_max_y", text="Y")
		
		layout.itemR(tex, "extension")
		
		split = layout.split()
		
		sub = split.column()
		if tex.extension == 'REPEAT': 
			sub.itemL(text="Repeat:")
			sub.itemR(tex, "repeat_x", text="X")
			sub.itemR(tex, "repeat_y", text="Y")
			sub = split.column()
			sub.itemL(text="Mirror:")
			sub.itemR(tex, "mirror_x", text="X")
			sub.itemR(tex, "mirror_y", text="Y")
		elif tex.extension == 'CHECKER': 
			sub.itemR(tex, "checker_even", text="Even")
			sub.itemR(tex, "checker_odd", text="Odd")
			sub = split.column()
			sub.itemR(tex, "checker_distance", text="Distance")
	
class TEXTURE_PT_plugin(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_plugin"
	__label__ = "Plugin"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'PLUGIN')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		layout.itemL(text="Nothing yet")
		
class TEXTURE_PT_envmap(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_envmap"
	__label__ = "Environment Map"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'ENVIRONMENT_MAP')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		layout.itemL(text="Nothing yet")
		
class TEXTURE_PT_musgrave(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_musgrave"
	__label__ = "Musgrave"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'MUSGRAVE')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		layout.itemR(tex, "musgrave_type")	
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(tex, "highest_dimension", text="Dimension")
		sub.itemR(tex, "lacunarity")
		sub.itemR(tex, "octaves")
		sub = split.column() 
		if (tex.musgrave_type in ('HETERO_TERRAIN', 'RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL')):
			sub.itemR(tex, "offset")
		if (tex.musgrave_type in ('RIDGED_MULTIFRACTAL', 'HYBRID_MULTIFRACTAL')):
			sub.itemR(tex, "gain")
			sub.itemR(tex, "noise_intensity", text="Intensity")
		
		layout.itemL(text="Noise:")
		
		layout.itemR(tex, "noise_basis", text="Basis")
		
		row = layout.row()
		row.itemR(tex, "noise_size", text="Size")
		row.itemR(tex, "nabla")

class TEXTURE_PT_voronoi(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_voronoi"
	__label__ = "Voronoi"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'VORONOI')

	def draw(self, context):
		layout = self.layout
		tex = context.texture
	
		layout.itemR(tex, "distance_metric")
		layout.itemR(tex, "coloring")
		
		split = layout.split()
		
		sub = split.column()   
		
		sub.itemR(tex, "noise_intensity", text="Intensity")
		if tex.distance_metric == 'MINKOVSKY':
			sub.itemR(tex, "minkovsky_exponent", text="Exponent")
		sub = split.column()
		sub.itemR(tex, "feature_weights", slider=True)
		
		layout.itemL(text="Noise:")
		
		row = layout.row()
		row.itemR(tex, "noise_size", text="Size")
		row.itemR(tex, "nabla")
			
class TEXTURE_PT_distortednoise(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_distortednoise"
	__label__ = "Distorted Noise"
	
	def poll(self, context):
		tex = context.texture
		return (tex and tex.type == 'DISTORTED_NOISE')

	def draw(self, context):
		layout = self.layout
		tex = context.texture

		layout.itemR(tex, "noise_distortion")
		layout.itemR(tex, "noise_basis", text="Basis")
		
		split = layout.split()
		
		sub = split.column()
		sub.itemR(tex, "distortion_amount", text="Amount")
		sub.itemR(tex, "noise_size", text="Size")
		
		sub = split.column()
		sub.itemR(tex, "nabla")	

bpy.types.register(TEXTURE_PT_preview)
bpy.types.register(TEXTURE_PT_texture)
bpy.types.register(TEXTURE_PT_clouds)
bpy.types.register(TEXTURE_PT_wood)
bpy.types.register(TEXTURE_PT_marble)
bpy.types.register(TEXTURE_PT_magic)
bpy.types.register(TEXTURE_PT_blend)
bpy.types.register(TEXTURE_PT_stucci)
bpy.types.register(TEXTURE_PT_image)
bpy.types.register(TEXTURE_PT_mapping)
bpy.types.register(TEXTURE_PT_plugin)
bpy.types.register(TEXTURE_PT_envmap)
bpy.types.register(TEXTURE_PT_musgrave)
bpy.types.register(TEXTURE_PT_voronoi)
bpy.types.register(TEXTURE_PT_distortednoise)
bpy.types.register(TEXTURE_PT_colors)
bpy.types.register(TEXTURE_PT_map)
		
