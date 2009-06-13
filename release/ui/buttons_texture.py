
import bpy

class TextureButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "texture"
	
	def poll(self, context):
		return (context.texture != None and context.texture.type != 'NONE')
		
class TEXTURE_PT_preview(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_preview"
	__label__ = "Preview"

	def draw(self, context):
		layout = self.layout
		tex = context.texture
		
		layout.template_preview(tex)

class TEXTURE_PT_texture(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_texture"
	__label__ = "Texture"

	def poll(self, context):
		return (context.material or context.world or context.lamp)

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

class TEXTURE_PT_mapping(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_mapping"
	__label__ = "Mapping"

	def draw(self, context):
		layout = self.layout
		tex = context.texture_slot
		textype = context.texture

		split = layout.split(percentage=0.3)
		col = split.column()
		col.itemL(text="Coordinates:")
		col = split.column()
		col.itemR(tex, "texture_coordinates", text="")

		if tex.texture_coordinates == 'UV':
			row = layout.row()
			row.itemR(tex, "uv_layer")
		elif tex.texture_coordinates == 'OBJECT':
			row = layout.row()
			row.itemR(tex, "object")
		
		if textype.type in ('IMAGE', 'ENVIRONMENT_MAP'):
			split = layout.split(percentage=0.3)
			col = split.column()
			col.itemL(text="Projection:")
			col = split.column()
			col.itemR(tex, "mapping", text="")

		split = layout.split()
		
		col = split.column()
		col.itemR(tex, "from_dupli")
		
		col = split.column()
		row = col.row()
		row.itemR(tex, "x_mapping", text="")
		row.itemR(tex, "y_mapping", text="")
		row.itemR(tex, "z_mapping", text="")

		row = layout.row()
		row.column().itemR(tex, "offset")
		row.column().itemR(tex, "size")

class TEXTURE_PT_influence(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_influence"
	__label__ = "Influence"

	def draw(self, context):
		layout = self.layout
		
		textype = context.texture
		tex = context.texture_slot
		
		split = layout.split()
		
		col = split.column()
		col.itemR(tex, "map_color", text="Diffuse Color")
		colsub = col.column()
		colsub.active = tex.map_color
		colsub.itemR(tex, "color_factor", text="Opacity", slider=True)
		colsub.itemR(tex, "blend_type")
		if textype.type == 'IMAGE':
			col.itemR(tex, "no_rgb")
			
			colsub = col.column()
			colsub.active = tex.no_rgb
			colsub.itemR(tex, "color")
		else:
			col.itemR(tex, "color")
			
		col.itemR(tex, "map_colorspec")
		col.itemR(tex, "map_normal")
		colsub = col.column()
		colsub.active = tex.map_normal
		colsub.itemR(tex, "normal_factor", text="Amount", slider=True)
		col.itemR(tex, "normal_map_space")
		col.itemR(tex, "map_warp")
		colsub = col.column()
		colsub.active = tex.map_warp
		colsub.itemR(tex, "warp_factor", text="Amount", slider=True)	
		col.itemR(tex, "map_displacement")
		colsub = col.column()
		colsub.active = tex.map_displacement
		colsub.itemR(tex, "displacement_factor", text="Amount", slider=True)
		col = split.column()
		col.itemR(tex, "map_mirror")
		col.itemR(tex, "map_reflection")
		col.itemR(tex, "map_specularity")
		col.itemR(tex, "map_ambient")
		col.itemR(tex, "map_hardness")
		col.itemR(tex, "map_raymir")
		col.itemR(tex, "map_alpha")
		col.itemR(tex, "map_emit")
		col.itemR(tex, "map_translucency")

		colsub = col.column()
		colsub.active = tex.map_translucency or tex.map_emit or tex.map_alpha or tex.map_raymir or tex.map_hardness or tex.map_ambient or tex.map_specularity or tex.map_reflection or tex.map_mirror
		colsub.itemR(tex, "default_value", text="Amount", slider=True)
		
		row = layout.row()
		row.itemR(tex, "stencil")
		row.itemR(tex, "negate", text="Negative")

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
			col.itemR(tex, "rgb_factor", text="Multiply RGB")

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

class TEXTURE_PT_crop(TextureButtonsPanel):
	__idname__= "TEXTURE_PT_crop"
	__label__ = "Crop"
	
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
		sub.itemR(tex, "distortion_amount", text="Distortion")
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
bpy.types.register(TEXTURE_PT_crop)
bpy.types.register(TEXTURE_PT_plugin)
bpy.types.register(TEXTURE_PT_envmap)
bpy.types.register(TEXTURE_PT_musgrave)
bpy.types.register(TEXTURE_PT_voronoi)
bpy.types.register(TEXTURE_PT_distortednoise)
bpy.types.register(TEXTURE_PT_colors)
bpy.types.register(TEXTURE_PT_mapping)
bpy.types.register(TEXTURE_PT_influence)