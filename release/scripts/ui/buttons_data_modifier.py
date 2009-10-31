
import bpy

class DataButtonsPanel(bpy.types.Panel):
	bl_space_type = 'PROPERTIES'
	bl_region_type = 'WINDOW'
	bl_context = "modifier"
	
class DATA_PT_modifiers(DataButtonsPanel):
	bl_label = "Modifiers"

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		row = layout.row()
		row.item_menu_enumO("object.modifier_add", "type")
		row.itemL()

		for md in ob.modifiers:
			box = layout.template_modifier(md)
			if box:
				# match enum type to our functions, avoids a lookup table.
				getattr(self, md.type)(box, ob, md)
	
	# the mt.type enum is (ab)used for a lookup on function names
	# ...to avoid lengthy if statements
	# so each type must have a function here.

	def ARMATURE(self, layout, ob, md):
		layout.itemR(md, "object")
		
		split = layout.split(percentage=0.5)
		split.itemL(text="Vertex Group:")
		sub = split.split(percentage=0.7)
		sub.item_pointerR(md, "vertex_group", ob, "vertex_groups", text="")
		subsub = sub.row()
		subsub.active = md.vertex_group
		subsub.itemR(md, "invert")
		
		layout.itemS()
		
		split = layout.split()

		col = split.column()
		col.itemL(text="Bind To:")
		col.itemR(md, "use_vertex_groups", text="Vertex Groups")
		col.itemR(md, "use_bone_envelopes", text="Bone Envelopes")
		
		col = split.column()
		col.itemL(text="Deformation:")
		col.itemR(md, "quaternion")
		col.itemR(md, "multi_modifier")
		
	def ARRAY(self, layout, ob, md):
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
	
	def BEVEL(self, layout, ob, md):
		row = layout.row()
		row.itemR(md, "width")
		row.itemR(md, "only_vertices")
		
		layout.itemL(text="Limit Method:")
		layout.row().itemR(md, "limit_method", expand=True)
		if md.limit_method == 'ANGLE':
			layout.itemR(md, "angle")
		elif md.limit_method == 'WEIGHT':
			layout.row().itemR(md, "edge_weight_method", expand=True)
			
	def BOOLEAN(self, layout, ob, md):
		layout.itemR(md, "operation")
		layout.itemR(md, "object")
		
	def BUILD(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "start")
		col.itemR(md, "length")

		col = split.column()
		col.itemR(md, "randomize")
		sub = col.column()
		sub.active = md.randomize
		sub.itemR(md, "seed")

	def CAST(self, layout, ob, md):
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
		
	def CLOTH(self, layout, ob, md):
		layout.itemL(text="See Cloth panel.")
		
	def COLLISION(self, layout, ob, md):
		layout.itemL(text="See Collision panel.")
		
	def CURVE(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "deform_axis")
		
	def DECIMATE(self, layout, ob, md):
		layout.itemR(md, "ratio")
		layout.itemR(md, "face_count")
		
	def DISPLACE(self, layout, ob, md):
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "texture")
		layout.itemR(md, "midlevel")
		layout.itemR(md, "strength")
		layout.itemR(md, "direction")
		layout.itemR(md, "texture_coordinates")
		if md.texture_coordinates == 'OBJECT':
			layout.itemR(md, "texture_coordinate_object", text="Object")
		elif md.texture_coordinates == 'UV' and ob.type == 'MESH':
			layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")
	
	def EDGE_SPLIT(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "use_edge_angle", text="Edge Angle")
		sub = col.column()
		sub.active = md.use_edge_angle
		sub.itemR(md, "split_angle")
		
		col = split.column()
		col.itemR(md, "use_sharp", text="Sharp Edges")
		
	def EXPLODE(self, layout, ob, md):
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "protect")

		flow = layout.column_flow(2)
		flow.itemR(md, "split_edges")
		flow.itemR(md, "unborn")
		flow.itemR(md, "alive")
		flow.itemR(md, "dead")

		layout.itemO("object.explode_refresh", text="Refresh");
		
	def FLUID_SIMULATION(self, layout, ob, md):
		layout.itemL(text="See Fluid panel.")
		
	def HOOK(self, layout, ob, md):
		col = layout.column()
		col.itemR(md, "object")
		if md.object and md.object.type == 'ARMATURE':
			layout.item_pointerR(md, "subtarget", md.object.data, "bones", text="Bone")
		
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")

		split = layout.split()
		split.itemR(md, "falloff")
		split.itemR(md, "force", slider=True)

		layout.itemS()

		row = layout.row()
		row.itemO("object.hook_reset", text="Reset")
		row.itemO("object.hook_recenter", text="Recenter")

		if ob.mode == 'EDIT':
			row = layout.row()
			row.itemO("object.hook_select", text="Select")
			row.itemO("object.hook_assign", text="Assign")
		
	def LATTICE(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		
	def MASK(self, layout, ob, md):
		layout.itemR(md, "mode")
		if md.mode == 'ARMATURE':
			layout.itemR(md, "armature")
		elif md.mode == 'VERTEX_GROUP':
			layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "inverse")
		
	def MESH_DEFORM(self, layout, ob, md):
		layout.itemR(md, "object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "invert")

		layout.itemS()
		
		if md.is_bound:
			layout.itemO("object.meshdeform_bind", text="Unbind")
		else:
			layout.itemO("object.meshdeform_bind", text="Bind")
			row = layout.row()
			row.itemR(md, "precision")
			row.itemR(md, "dynamic")
		
	def MIRROR(self, layout, ob, md):
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
		
	def MULTIRES(self, layout, ob, md):
		layout.itemR(md, "subdivision_type")
		
		row = layout.row()
		row.itemO("object.multires_subdivide", text="Subdivide")
		row.itemO("object.multires_higher_levels_delete", text="Delete Higher")

		layout.itemR(md, "level")
	
	def PARTICLE_INSTANCE(self, layout, ob, md):
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
		
	def PARTICLE_SYSTEM(self, layout, ob, md):
		layout.itemL(text="See Particle panel.")
		
	def SHRINKWRAP(self, layout, ob, md):
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
		
	def SIMPLE_DEFORM(self, layout, ob, md):
		layout.itemR(md, "mode")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "origin")
		layout.itemR(md, "relative")
		layout.itemR(md, "factor")
		layout.itemR(md, "limits")
		if md.mode in ('TAPER', 'STRETCH'):
			layout.itemR(md, "lock_x_axis")
			layout.itemR(md, "lock_y_axis")
			
	def SMOKE(self, layout, ob, md):
		layout.itemL(text="See Smoke panel.")
	
	def SMOOTH(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemR(md, "x")
		col.itemR(md, "y")
		col.itemR(md, "z")
		
		col = split.column()
		col.itemR(md, "factor")
		col.itemR(md, "repeat")
		
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		
	def SOFT_BODY(self, layout, ob, md):
		layout.itemL(text="See Soft Body panel.")
	
	def SUBSURF(self, layout, ob, md):
		layout.row().itemR(md, "subdivision_type", expand=True)
		
		flow = layout.column_flow()
		flow.itemR(md, "levels", text="Preview")
		flow.itemR(md, "render_levels", text="Render")
		flow.itemR(md, "optimal_draw", text="Optimal Display")
		flow.itemR(md, "subsurf_uv")

	def SURFACE(self, layout, ob, md):
		layout.itemL(text="See Fields panel.")
	
	def UV_PROJECT(self, layout, ob, md):
		if ob.type == 'MESH':
			layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")
			layout.itemR(md, "image")
			layout.itemR(md, "override_image")

			split = layout.split()

			col = split.column()
			col.itemL(text="Aspect Ratio:")

			sub = col.column(align=True)
			sub.itemR(md, "horizontal_aspect_ratio", text="Horizontal")
			sub.itemR(md, "vertical_aspect_ratio", text="Vertical")

			col = split.column()
			col.itemL(text="Projectors:")

			sub = col.column(align=True)
			sub.itemR(md, "num_projectors", text="Number")
			for proj in md.projectors:
				sub.itemR(proj, "object", text="")
		
	def WAVE(self, layout, ob, md):
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Motion:")
		col.itemR(md, "x")
		col.itemR(md, "y")
		col.itemR(md, "cyclic")
		
		col = split.column()
		col.itemR(md, "normals")
		sub = col.column()
		sub.active = md.normals
		sub.itemR(md, "x_normal", text="X")
		sub.itemR(md, "y_normal", text="Y")
		sub.itemR(md, "z_normal", text="Z")
		
		split = layout.split()

		col = split.column()
		col.itemL(text="Time:")
		sub = col.column(align=True)
		sub.itemR(md, "time_offset", text="Offset")
		sub.itemR(md, "lifetime", text="Life")
		col.itemR(md, "damping_time", text="Damping")
		
		col = split.column()
		col.itemL(text="Position:")
		sub = col.column(align=True)
		sub.itemR(md, "start_position_x", text="X")
		sub.itemR(md, "start_position_y", text="Y")
		col.itemR(md, "falloff_radius", text="Falloff")
		
		layout.itemS()
		
		layout.itemR(md, "start_position_object")
		layout.item_pointerR(md, "vertex_group", ob, "vertex_groups")
		layout.itemR(md, "texture")
		layout.itemR(md, "texture_coordinates")
		if md.texture_coordinates == 'MAP_UV' and ob.type == 'MESH':
			layout.item_pointerR(md, "uv_layer", ob.data, "uv_textures")
		elif md.texture_coordinates == 'OBJECT':
			layout.itemR(md, "texture_coordinates_object")
		
		layout.itemS()
		
		flow = layout.column_flow()
		flow.itemR(md, "speed", slider=True)
		flow.itemR(md, "height", slider=True)
		flow.itemR(md, "width", slider=True)
		flow.itemR(md, "narrowness", slider=True)

bpy.types.register(DATA_PT_modifiers)
