
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "modifier"
	
class DATA_PT_modifiers(DataButtonsPanel):
	__idname__ = "DATA_PT_modifiers"
	__label__ = "Modifiers"

	def draw(self, context):
		ob = context.object
		layout = self.layout

		row = layout.row()
		row.item_menu_enumO("OBJECT_OT_modifier_add", "type")
		row.itemL();

		for md in ob.modifiers:
			box = layout.template_modifier(md)

			if box:
				if md.type == 'ARMATURE':
					self.armature(box, md)
				if md.type == 'ARRAY':
					self.array(box, md)
				if md.type == 'BEVEL':
					self.bevel(box, md)
				if md.type == 'BOOLEAN':
					self.boolean(box, md)
				if md.type == 'BUILD':
					self.build(box, md)
				if md.type == 'CAST':
					self.cast(box, md)
				if md.type == 'CLOTH':
					self.cloth(box, md)
				if md.type == 'COLLISION':
					self.collision(box, md)
				if md.type == 'CURVE':
					self.curve(box, md)
				if md.type == 'DECIMATE':
					self.decimate(box, md)
				if md.type == 'DISPLACE':
					self.displace(box, md)
				if md.type == 'EDGE_SPLIT':
					self.edgesplit(box, md)
				if md.type == 'EXPLODE':
					self.explode(box, md)
				if md.type == 'FLUID_SIMULATION':
					self.fluid(box, md)
				if md.type == 'HOOK':
					self.hook(box, md)
				if md.type == 'LATTICE':
					self.lattice(box, md)
				if md.type == 'MASK':
					self.mask(box, md)
				if md.type == 'MESH_DEFORM':
					self.mesh_deform(box, md)
				if md.type == 'MIRROR':
					self.mirror(box, md)
				if md.type == 'MULTIRES':
					self.multires(box, md)
				if md.type == 'PARTICLE_INSTANCE':
					self.particleinstance(box, md)
				if md.type == 'PARTICLE_SYSTEM':
					self.particlesystem(box, md)
				if md.type == 'SHRINKWRAP':
					self.shrinkwrap(box, md)
				if md.type == 'SIMPLE_DEFORM':
					self.simpledeform(box, md)
				if md.type == 'SMOOTH':
					self.smooth(box, md)
				if md.type == 'SOFTBODY':
					self.softbody(box, md)
				if md.type == 'SUBSURF':
					self.subsurf(box, md)
				if md.type == 'UV_PROJECT':
					self.uvproject(box, md)
				if md.type == 'WAVE':
					self.wave(box, md)
							
	def armature(self, layout, md):
		layout.itemR(md, "object")
		row = layout.row()
		row.itemR(md, "vertex_group")
		row.itemR(md, "invert")
		flow = layout.column_flow()
		flow.itemR(md, "use_vertex_groups", text="Vertex Groups")
		flow.itemR(md, "use_bone_envelopes", text="Bone Envelopes")
		flow.itemR(md, "quaternion")
		flow.itemR(md, "multi_modifier")
		
	def array(self, layout, md):
		layout.itemR(md, "fit_type")
		if md.fit_type == 'FIXED_COUNT':
			layout.itemR(md, "count")
		if md.fit_type == 'FIT_LENGTH':
			layout.itemR(md, "length")
		if md.fit_type == 'FIT_CURVE':
			layout.itemR(md, "curve")

		layout.itemS()
		
		split = layout.split()
		
		col = split.column()
		col = col.column()
		col.itemR(md, "constant_offset")
		colsub = col.column()
		colsub.active = md.constant_offset
		colsub.itemR(md, "constant_offset_displacement", text="")

		col.itemS()

		sub = col.row().itemR(md, "merge_adjacent_vertices", text="Merge")
		colsub = col.column()
		colsub.active = md.merge_adjacent_vertices
		colsub.itemR(md, "merge_end_vertices", text="First Last")
		colsub.itemR(md, "merge_distance", text="Distance")
		
		col = split.column()
		col = col.column()
		col.itemR(md, "relative_offset")
		colsub = col.column()
		colsub.active = md.relative_offset
		colsub.itemR(md, "relative_offset_displacement", text="")

		col.itemS()

		col = col.column()
		col.itemR(md, "add_offset_object")
		colsub = col.column()
		colsub.active = md.add_offset_object
		colsub.itemR(md, "offset_object", text="")

		layout.itemS()
		
		col = layout.column()
		col.itemR(md, "start_cap")
		col.itemR(md, "end_cap")
	
	def bevel(self, layout, md):
		row = layout.row()
		row.itemR(md, "width")
		row.itemR(md, "only_vertices")
		
		layout.itemL(text="Limit Method:")
		row = layout.row()
		row.itemR(md, "limit_method", expand=True)
		if md.limit_method == 'ANGLE':
			row = layout.row()
			row.itemR(md, "angle")
		if md.limit_method == 'WEIGHT':
			row = layout.row()
			row.itemR(md, "edge_weight_method", expand=True)
			
	def boolean(self, layout, md):
		layout.itemR(md, "operation")
		layout.itemR(md, "object")
		
	def build(self, layout, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "start")
		col.itemR(md, "length")

		col = split.column()
		col.itemR(md, "randomize")
		colsub = col.column()
		colsub.active = md.randomize
		colsub.itemR(md, "seed")
			
		
			
	def cast(self, layout, md):
		layout.itemR(md, "cast_type")
		col = layout.column_flow()
		col.itemR(md, "x")
		col.itemR(md, "y")
		col.itemR(md, "z")
		col.itemR(md, "factor")
		col.itemR(md, "radius")
		col.itemR(md, "size")
		layout.itemR(md, "vertex_group")
		#Missing: "OB" and "From Radius"
		
	def cloth(self, layout, md):
		layout.itemL(text="See Cloth panel.")
		
	def collision(self, layout, md):
		layout.itemL(text="See Collision panel.")
		
	def curve(self, layout, md):
		layout.itemR(md, "object")
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "deform_axis")
		
	def decimate(self, layout, md):
		layout.itemR(md, "ratio")
		layout.itemR(md, "face_count")
		
	def displace(self, layout, md):
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "texture")
		layout.itemR(md, "midlevel")
		layout.itemR(md, "strength")
		layout.itemR(md, "direction")
		layout.itemR(md, "texture_coordinates")
		if md.texture_coordinates == 'OBJECT':
			layout.itemR(md, "texture_coordinate_object", text="Object")
		if md.texture_coordinates == 'UV':
			layout.itemR(md, "uv_layer")
	
	def edgesplit(self, layout, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "use_edge_angle", text="Edge Angle")
		colsub = col.column()
		colsub.active = md.use_edge_angle
		colsub.itemR(md, "split_angle")
		col = split.column()
		col.itemR(md, "use_sharp", text="Sharp Edges")
		
	def explode(self, layout, md):
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "protect")
		layout.itemR(md, "split_edges")
		layout.itemR(md, "unborn")
		layout.itemR(md, "alive")
		layout.itemR(md, "dead")
		# Missing: "Refresh" and "Clear Vertex Group" ?
		
	def fluid(self, layout, md):
		layout.itemL(text="See Fluidsim panel.")
		
	def hook(self, layout, md):
		layout.itemR(md, "falloff")
		layout.itemR(md, "force", slider=True)
		layout.itemR(md, "object")
		layout.itemR(md, "vertex_group")
		# Missing: "Reset" and "Recenter"
		
	def lattice(self, layout, md):
		layout.itemR(md, "object")
		layout.itemR(md, "vertex_group")
		
	def mask(self, layout, md):
		layout.itemR(md, "mode")
		if md.mode == 'ARMATURE':
			layout.itemR(md, "armature")
		if md.mode == 'VERTEX_GROUP':
			layout.itemR(md, "vertex_group")
		layout.itemR(md, "inverse")
		
	def mesh_deform(self, layout, md):
		layout.itemR(md, "object")
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "invert")

		layout.itemS()
		layout.itemO("OBJECT_OT_modifier_mdef_bind", text="Bind")
		row = layout.row()
		row.itemR(md, "precision")
		row.itemR(md, "dynamic")
		
	def mirror(self, layout, md):
		layout.itemR(md, "merge_limit")
		split = layout.split()
		
		sub = split.column()
		sub.itemR(md, "x")
		sub.itemR(md, "y")
		sub.itemR(md, "z")
		sub = split.column()
		sub.itemL(text="Textures:")
		sub.itemR(md, "mirror_u")
		sub.itemR(md, "mirror_v")
		sub = split.column()
		sub.itemR(md, "clip", text="Do Clipping")
		sub.itemR(md, "mirror_vertex_groups", text="Vertex Group")
		
		layout.itemR(md, "mirror_object")
		
	def multires(self, layout, md):
		layout.itemR(md, "subdivision_type")
		layout.itemO("OBJECT_OT_multires_subdivide", text="Subdivide")
		layout.itemR(md, "level")
	
	def particleinstance(self, layout, md):
		layout.itemR(md, "object")
		layout.itemR(md, "particle_system_number")
		
		col = layout.column_flow()
		col.itemR(md, "normal")
		col.itemR(md, "children")
		col.itemR(md, "path")
		col.itemR(md, "unborn")
		col.itemR(md, "alive")
		col.itemR(md, "dead")
		
	def particlesystem(self, layout, md):
		layout.itemL(text="See Particle panel.")
		
	def shrinkwrap(self, layout, md):
		layout.itemR(md, "target")
		layout.itemR(md, "vertex_group")
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
		
			col = layout.column_flow()
			col.itemR(md, "negative")
			col.itemR(md, "positive")
			col.itemR(md, "cull_front_faces")
			col.itemR(md, "cull_back_faces")
		if md.mode == 'NEAREST_SURFACEPOINT':
			layout.itemR(md, "keep_above_surface")
		# To-Do: Validate if structs
		
	def simpledeform(self, layout, md):
		layout.itemR(md, "mode")
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "origin")
		layout.itemR(md, "relative")
		layout.itemR(md, "factor")
		layout.itemR(md, "limits")
		if md.mode in ('TAPER', 'STRETCH'):
			layout.itemR(md, "lock_x_axis")
			layout.itemR(md, "lock_y_axis")
	
	def smooth(self, layout, md):
		split = layout.split()
		sub = split.column()
		sub.itemR(md, "x")
		sub.itemR(md, "y")
		sub.itemR(md, "z")
		sub = split.column()
		sub.itemR(md, "factor")
		sub.itemR(md, "repeat")
		
		layout.itemR(md, "vertex_group")
		
	def softbody(self, layout, md):
		layout.itemL(text="See Softbody panel.")
	
	def subsurf(self, layout, md):
		layout.itemR(md, "subdivision_type")
		col = layout.column_flow()
		col.itemR(md, "levels", text="Preview")
		col.itemR(md, "render_levels", text="Render")
		col.itemR(md, "optimal_draw", text="Optimal Display")
		col.itemR(md, "subsurf_uv")
	
	def uvproject(self, layout, md):
		layout.itemR(md, "uv_layer")
		layout.itemR(md, "projectors")
		layout.itemR(md, "image")
		layout.itemR(md, "horizontal_aspect_ratio")
		layout.itemR(md, "vertical_aspect_ratio")
		layout.itemR(md, "override_image")
		#"Projectors" don't work.
		
	def wave(self, layout, md):
		split = layout.split()
		
		sub = split.column()
		sub.itemL(text="Motion:")
		sub.itemR(md, "x")
		sub.itemR(md, "y")
		sub.itemR(md, "cyclic")
		
		sub = split.column()
		sub.itemR(md, "normals")
		row = sub.row(align=True)
		row.active = md.normals
		row.itemR(md, "x_normal", text="X", toggle=True)
		row.itemR(md, "y_normal", text="Y", toggle=True)
		row.itemR(md, "z_normal", text="Z", toggle=True)
		
		col = layout.column_flow()
		col.itemR(md, "time_offset")
		col.itemR(md, "lifetime")
		col.itemR(md, "damping_time")
		col.itemR(md, "falloff_radius")
		col.itemR(md, "start_position_x")
		col.itemR(md, "start_position_y")
		
		layout.itemR(md, "start_position_object")
		layout.itemR(md, "vertex_group")
		layout.itemR(md, "texture")
		layout.itemR(md, "texture_coordinates")
		if md.texture_coordinates == 'MAP_UV':
			layout.itemR(md, "uv_layer")
		if md.texture_coordinates == 'OBJECT':
			layout.itemR(md, "texture_coordinates_object")
		
		col = layout.column_flow()
		col.itemR(md, "speed", slider=True)
		col.itemR(md, "height", slider=True)
		col.itemR(md, "width", slider=True)
		col.itemR(md, "narrowness", slider=True)

bpy.types.register(DATA_PT_modifiers)