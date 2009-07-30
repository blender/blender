
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "modifier"
	
class DATA_PT_modifiers(DataButtonsPanel):
	__label__ = "Modifiers"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		row = layout.row()
		row.item_menu_enumO("object.modifier_add", "type")
		row.itemL();

		for md in ob.modifiers:
			box = layout.template_modifier(md)

			if box:
				if md.type == 'ARMATURE':
					self.armature(box, ob, md)
				elif md.type == 'ARRAY':
					self.array(box, ob, md)
				elif md.type == 'BEVEL':
					self.bevel(box, ob, md)
				elif md.type == 'BOOLEAN':
					self.boolean(box, ob, md)
				elif md.type == 'BUILD':
					self.build(box, ob, md)
				elif md.type == 'CAST':
					self.cast(box, ob, md)
				elif md.type == 'CLOTH':
					self.cloth(box, ob, md)
				elif md.type == 'COLLISION':
					self.collision(box, ob, md)
				elif md.type == 'CURVE':
					self.curve(box, ob, md)
				elif md.type == 'DECIMATE':
					self.decimate(box, ob, md)
				elif md.type == 'DISPLACE':
					self.displace(box, ob, md)
				elif md.type == 'EDGE_SPLIT':
					self.edgesplit(box, ob, md)
				elif md.type == 'EXPLODE':
					self.explode(box, ob, md)
				elif md.type == 'FLUID_SIMULATION':
					self.fluid(box, ob, md)
				elif md.type == 'HOOK':
					self.hook(box, ob, md)
				elif md.type == 'LATTICE':
					self.lattice(box, ob, md)
				elif md.type == 'MASK':
					self.mask(box, ob, md)
				elif md.type == 'MESH_DEFORM':
					self.mesh_deform(box, ob, md)
				elif md.type == 'MIRROR':
					self.mirror(box, ob, md)
				elif md.type == 'MULTIRES':
					self.multires(box, ob, md)
				elif md.type == 'PARTICLE_INSTANCE':
					self.particleinstance(box, ob, md)
				elif md.type == 'PARTICLE_SYSTEM':
					self.particlesystem(box, ob, md)
				elif md.type == 'SHRINKWRAP':
					self.shrinkwrap(box, ob, md)
				elif md.type == 'SIMPLE_DEFORM':
					self.simpledeform(box, ob, md)
				elif md.type == 'SMOOTH':
					self.smooth(box, ob, md)
				elif md.type == 'SOFTBODY':
					self.softbody(box, ob, md)
				elif md.type == 'SUBSURF':
					self.subsurf(box, ob, md)
				elif md.type == 'SURFACE':
					self.surface(box, ob, md)
				elif md.type == 'UV_PROJECT':
					self.uvproject(box, ob, md)
				elif md.type == 'WAVE':
					self.wave(box, ob, md)
				if md.type == 'SMOKE':
					self.smoke(box, ob, md)
							
	def armature(self, layout, ob, md):
		layout.itemR(md, "object")
		
		row = layout.row()
		row.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		row.itemR(md, "invert")
		
		flow = layout.column_flow()
		flow.itemR(md, "use_vertex_groups", text="Vertex Groups")
		flow.itemR(md, "use_bone_envelopes", text="Bone Envelopes")
		flow.itemR(md, "quaternion")
		flow.itemR(md, "multi_modifier")
		
	def array(self, layout, ob, md):
		layout.itemR(md, "fit_type")
		if md.fit_type == 'FIXED_COUNT':
			layout.itemR(md, "count")
		elif md.fit_type == 'FIT_LENGTH':
			layout.itemR(md, "length")
		elif md.fit_type == 'FIT_CURVE':
			layout.itemR(md, "curve")

		layout.itemS()
		
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "constant_offset")
		sub = col.column()
		sub.active = md.constant_offset
		sub.itemR(md, "constant_offset_displacement", text="")

		col.itemS()

		col.itemR(md, "merge_adjacent_vertices", text="Merge")
		sub = col.column()
		sub.active = md.merge_adjacent_vertices
		sub.itemR(md, "merge_end_vertices", text="First Last")
		sub.itemR(md, "merge_distance", text="Distance")
		
		col = split.column()
		col.itemR(md, "relative_offset")
		sub = col.column()
		sub.active = md.relative_offset
		sub.itemR(md, "relative_offset_displacement", text="")

		col.itemS()

		col.itemR(md, "add_offset_object")
		sub = col.column()
		sub.active = md.add_offset_object
		sub.itemR(md, "offset_object", text="")

		layout.itemS()
		
		col = layout.column()
		col.itemR(md, "start_cap")
		col.itemR(md, "end_cap")
	
	def bevel(self, layout, ob, md):
		row = layout.row()
		row.itemR(md, "width")
		row.itemR(md, "only_vertices")
		
		layout.itemL(text="Limit Method:")
		layout.row().itemR(md, "limit_method", expand=True)
		if md.limit_method == 'ANGLE':
			layout.itemR(md, "angle")
		elif md.limit_method == 'WEIGHT':
			layout.row().itemR(md, "edge_weight_method", expand=True)
			
	def boolean(self, layout, ob, md):
		layout.itemR(md, "operation")
		layout.itemR(md, "object")
		
	def build(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "start")
		col.itemR(md, "length")

		col = split.column()
		col.itemR(md, "randomize")
		sub = col.column()
		sub.active = md.randomize
		sub.itemR(md, "seed")

	def cast(self, layout, ob, md):
		layout.itemR(md, "cast_type")
		layout.itemR(md, "object")
		if md.object:
			layout.itemR(md, "use_transform")
		
		flow = layout.column_flow()
		flow.itemR(md, "x")
		flow.itemR(md, "y")
		flow.itemR(md, "z")
		flow.itemR(md, "factor")
		flow.itemR(md, "radius")
		flow.itemR(md, "size")

		layout.itemR(md, "from_radius")
		
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		
	def cloth(self, layout, ob, md):
		layout.itemL(text="See Cloth panel.")
		
	def collision(self, layout, ob, md):
		layout.itemL(text="See Collision panel.")
		
	def curve(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "deform_axis")
		
	def decimate(self, layout, ob, md):
		layout.itemR(md, "ratio")
		layout.itemR(md, "face_count")
		
	def displace(self, layout, ob, md):
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "texture")
		layout.itemR(md, "midlevel")
		layout.itemR(md, "strength")
		layout.itemR(md, "direction")
		layout.itemR(md, "texture_coordinates")
		if md.texture_coordinates == 'OBJECT':
			layout.itemR(md, "texture_coordinate_object", text="Object")
		elif md.texture_coordinates == 'UV' and ob.type == 'MESH':
			layout.item_pointerR(md, "uv_layer", ob.data, "uv_layers")
	
	def edgesplit(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "use_edge_angle", text="Edge Angle")
		sub = col.column()
		sub.active = md.use_edge_angle
		sub.itemR(md, "split_angle")
		
		col = split.column()
		col.itemR(md, "use_sharp", text="Sharp Edges")
		
	def explode(self, layout, ob, md):
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "protect")
		layout.itemR(md, "split_edges")
		layout.itemR(md, "unborn")
		layout.itemR(md, "alive")
		layout.itemR(md, "dead")
		# Missing: "Refresh" and "Clear Vertex Group" Operator
		
	def fluid(self, layout, ob, md):
		layout.itemL(text="See Fluid panel.")
		
	def hook(self, layout, ob, md):
		layout.itemR(md, "falloff")
		layout.itemR(md, "force", slider=True)
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		# Missing: "Reset" and "Recenter" Operator
		
	def lattice(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		
	def mask(self, layout, ob, md):
		layout.itemR(md, "mode")
		if md.mode == 'ARMATURE':
			layout.itemR(md, "armature")
		elif md.mode == 'VERTEX_GROUP':
			layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "inverse")
		
	def mesh_deform(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "invert")

		layout.itemS()
		
		layout.itemO("object.modifier_mdef_bind", text="Bind")
		row = layout.row()
		row.itemR(md, "precision")
		row.itemR(md, "dynamic")
		
	def mirror(self, layout, ob, md):
		layout.itemR(md, "merge_limit")
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "x")
		col.itemR(md, "y")
		col.itemR(md, "z")
		
		col = split.column()
		col.itemL(text="Textures:")
		col.itemR(md, "mirror_u")
		col.itemR(md, "mirror_v")
		
		col = split.column()
		col.itemR(md, "clip", text="Do Clipping")
		col.itemR(md, "mirror_vertex_groups", text="Vertex Group")
		
		layout.itemR(md, "mirror_object")
		
	def multires(self, layout, ob, md):
		layout.itemR(md, "subdivision_type")
		layout.itemO("object.multires_subdivide", text="Subdivide")
		layout.itemR(md, "level")
	
	def particleinstance(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.itemR(md, "particle_system_number")
		
		flow = layout.column_flow()
		flow.itemR(md, "normal")
		flow.itemR(md, "children")
		flow.itemR(md, "size")
		flow.itemR(md, "path")
		if md.path:
			flow.itemR(md, "keep_shape")
		flow.itemR(md, "unborn")
		flow.itemR(md, "alive")
		flow.itemR(md, "dead")
		flow.itemL(md, "")
		if md.path:
			flow.itemR(md, "axis", text="")
		
		if md.path:
			row = layout.row()
			row.itemR(md, "position", slider=True)
			row.itemR(md, "random_position", text = "Random", slider=True)
		
	def particlesystem(self, layout, ob, md):
		layout.itemL(text="See Particle panel.")
		
	def shrinkwrap(self, layout, ob, md):
		layout.itemR(md, "target")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "offset")
		layout.itemR(md, "subsurf_levels")
		layout.itemR(md, "mode")
		if md.mode == 'PROJECT':
			layout.itemR(md, "subsurf_levels")
			layout.itemR(md, "auxiliary_target")
		
			row = layout.row()
			row.itemR(md, "x")
			row.itemR(md, "y")
			row.itemR(md, "z")
		
			flow = layout.column_flow()
			flow.itemR(md, "negative")
			flow.itemR(md, "positive")
			flow.itemR(md, "cull_front_faces")
			flow.itemR(md, "cull_back_faces")
		elif md.mode == 'NEAREST_SURFACEPOINT':
			layout.itemR(md, "keep_above_surface")
		
	def simpledeform(self, layout, ob, md):
		layout.itemR(md, "mode")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "origin")
		layout.itemR(md, "relative")
		layout.itemR(md, "factor")
		layout.itemR(md, "limits")
		if md.mode in ('TAPER', 'STRETCH'):
			layout.itemR(md, "lock_x_axis")
			layout.itemR(md, "lock_y_axis")
	
	def smooth(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "x")
		col.itemR(md, "y")
		col.itemR(md, "z")
		
		col = split.column()
		col.itemR(md, "factor")
		col.itemR(md, "repeat")
		
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		
	def softbody(self, layout, ob, md):
		layout.itemL(text="See Soft Body panel.")
	
	def subsurf(self, layout, ob, md):
		layout.itemR(md, "subdivision_type")
		
		flow = layout.column_flow()
		flow.itemR(md, "levels", text="Preview")
		flow.itemR(md, "render_levels", text="Render")
		flow.itemR(md, "optimal_draw", text="Optimal Display")
		flow.itemR(md, "subsurf_uv")

	def surface(self, layout, ob, md):
		layout.itemL(text="See Fields panel.")
	
	def uvproject(self, layout, ob, md):
		if ob.type == 'MESH':
			layout.item_pointerR(md, "uv_layer", ob.data, "uv_layers")
			#layout.itemR(md, "projectors")
			layout.itemR(md, "image")
			layout.itemR(md, "horizontal_aspect_ratio")
			layout.itemR(md, "vertical_aspect_ratio")
			layout.itemR(md, "override_image")
			#"Projectors" don't work.
		
	def wave(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Motion:")
		col.itemR(md, "x")
		col.itemR(md, "y")
		col.itemR(md, "cyclic")
		
		col = split.column()
		col.itemR(md, "normals")
		sub = col.row(align=True)
		sub.active = md.normals
		sub.itemR(md, "x_normal", text="X", toggle=True)
		sub.itemR(md, "y_normal", text="Y", toggle=True)
		sub.itemR(md, "z_normal", text="Z", toggle=True)
		
		flow = layout.column_flow()
		flow.itemR(md, "time_offset")
		flow.itemR(md, "lifetime")
		flow.itemR(md, "damping_time")
		flow.itemR(md, "falloff_radius")
		flow.itemR(md, "start_position_x")
		flow.itemR(md, "start_position_y")
		
		layout.itemR(md, "start_position_object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "texture")
		layout.itemR(md, "texture_coordinates")
		if md.texture_coordinates == 'MAP_UV' and ob.type == 'MESH':
			layout.item_pointerR(md, "uv_layer", ob.data, "uv_layers")
		elif md.texture_coordinates == 'OBJECT':
			layout.itemR(md, "texture_coordinates_object")
		
		flow = layout.column_flow()
		flow.itemR(md, "speed", slider=True)
		flow.itemR(md, "height", slider=True)
		flow.itemR(md, "width", slider=True)
		flow.itemR(md, "narrowness", slider=True)
		
	def smoke(self, layout, ob, md):
		layout.itemR(md, "fluid_type")
		if md.fluid_type == 'TYPE_DOMAIN':
			layout.itemS()
			layout.itemR(md.domain_settings, "maxres")
			layout.itemR(md.domain_settings, "color")
			layout.itemR(md.domain_settings, "amplify")
			layout.itemR(md.domain_settings, "highres")
			layout.itemR(md.domain_settings, "noise_type")
			layout.itemR(md.domain_settings, "visibility")
			layout.itemR(md.domain_settings, "alpha")
			layout.itemR(md.domain_settings, "beta")
			layout.itemR(md.domain_settings, "fluid_group")
			layout.itemR(md.domain_settings, "eff_group")
			layout.itemR(md.domain_settings, "coll_group")
		if md.fluid_type == 'TYPE_FLOW':
			layout.itemS()
			layout.itemR(md.flow_settings, "density")
			layout.itemR(md.flow_settings, "temperature")
			layout.itemL(text="Velocity")
			layout.row().itemR(md.flow_settings, "velocity", text="")
			layout.item_pointerR(md.flow_settings, "psys", ob, "particle_systems")
		if md.fluid_type == 'TYPE_FLUID':
			layout.itemS()

bpy.types.register(DATA_PT_modifiers)
