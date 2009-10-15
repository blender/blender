
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "data"
	
	def poll(self, context):
		return context.mesh

class DATA_PT_context_mesh(DataButtonsPanel):
	__show_header__ = False
	
	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		mesh = context.mesh
		space = context.space_data

		split = layout.split(percentage=0.65)

		if ob:
			split.template_ID(ob, "data")
			split.itemS()
		elif mesh:
			split.template_ID(space, "pin_id")
			split.itemS()

class DATA_PT_normals(DataButtonsPanel):
	__label__ = "Normals"

	def draw(self, context):
		layout = self.layout
		
		mesh = context.mesh
		
		split = layout.split()
		
		col = split.column()
		col.itemR(mesh, "autosmooth")
		sub = col.column()
		sub.active = mesh.autosmooth
		sub.itemR(mesh, "autosmooth_angle", text="Angle")
		
		col = split.column()
		col.itemR(mesh, "vertex_normal_flip")
		col.itemR(mesh, "double_sided")

class DATA_PT_settings(DataButtonsPanel):
	__label__ = "Settings"

	def draw(self, context):
		layout = self.layout
		
		mesh = context.mesh
		
		split = layout.split()
		
		col = split.column()
		col.itemR(mesh, "texture_mesh")
		
		col = split.column()
		col.itemR(mesh, "use_mirror_x")

class DATA_PT_vertex_groups(DataButtonsPanel):
	__label__ = "Vertex Groups"
	
	def poll(self, context):
		return (context.object and context.object.type in ('MESH', 'LATTICE'))

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		row = layout.row()
		row.template_list(ob, "vertex_groups", ob, "active_vertex_group_index", rows=2)

		col = row.column(align=True)
		col.itemO("object.vertex_group_add", icon='ICON_ZOOMIN', text="")
		col.itemO("object.vertex_group_remove", icon='ICON_ZOOMOUT', text="")

		col.itemO("object.vertex_group_copy", icon='ICON_COPY_ID', text="")
		if ob.data.users > 1:
			col.itemO("object.vertex_group_copy_to_linked", icon='ICON_LINK_AREA', text="")

		group = ob.active_vertex_group
		if group:
			row = layout.row()
			row.itemR(group, "name")

		if ob.mode == 'EDIT':
			row = layout.row()
			
			sub = row.row(align=True)
			sub.itemO("object.vertex_group_assign", text="Assign")
			sub.itemO("object.vertex_group_remove_from", text="Remove")
			
			sub = row.row(align=True)
			sub.itemO("object.vertex_group_select", text="Select")
			sub.itemO("object.vertex_group_deselect", text="Deselect")

			layout.itemR(context.tool_settings, "vertex_group_weight", text="Weight")

class DATA_PT_shape_keys(DataButtonsPanel):
	__label__ = "Shape Keys"
	
	def poll(self, context):
		return (context.object and context.object.type in ('MESH', 'LATTICE'))

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		key = ob.data.shape_keys
		kb = ob.active_shape_key

		row = layout.row()
		row.template_list(key, "keys", ob, "active_shape_key_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("object.shape_key_add", icon='ICON_ZOOMIN', text="")
		subcol.itemO("object.shape_key_remove", icon='ICON_ZOOMOUT', text="")

		if kb:
			col.itemS()

			subcol = col.column(align=True)
			subcol.itemR(ob, "shape_key_lock", icon='ICON_UNPINNED', text="")
			subcol.itemR(kb, "mute", icon='ICON_MUTE_IPO_OFF', text="")

			if key.relative:
				row = layout.row()
				row.itemR(key, "relative")
				row.itemL()

				row = layout.row()
				row.itemR(kb, "name")

				if ob.active_shape_key_index != 0:
					
					row = layout.row()
					row.enabled = ob.shape_key_lock == False
					row.itemR(kb, "value", slider=True)
					
					split = layout.split()
					sub = split.column(align=True)
					sub.enabled = ob.shape_key_lock == False
					sub.itemL(text="Range:")
					sub.itemR(kb, "slider_min", text="Min")
					sub.itemR(kb, "slider_max", text="Max")
					
					sub = split.column()
					sub.itemL(text="Blend:")
					sub.item_pointerR(kb, "vertex_group", ob, "vertex_groups", text="")
					sub.item_pointerR(kb, "relative_key", key, "keys", text="")
					
			else:
				row = layout.row()
				row.itemR(key, "relative")
				row.itemR(key, "slurph")

				layout.itemR(kb, "name")

		if ob.mode == 'EDIT':
			layout.enabled = False

class DATA_PT_uv_texture(DataButtonsPanel):
	__label__ = "UV Texture"
	
	def draw(self, context):
		layout = self.layout
		
		me = context.mesh

		row = layout.row()
		col = row.column()
		
		col.template_list(me, "uv_textures", me, "active_uv_texture_index", rows=2)

		col = row.column(align=True)
		col.itemO("mesh.uv_texture_add", icon='ICON_ZOOMIN', text="")
		col.itemO("mesh.uv_texture_remove", icon='ICON_ZOOMOUT', text="")

		lay = me.active_uv_texture
		if lay:
			layout.itemR(lay, "name")

class DATA_PT_vertex_colors(DataButtonsPanel):
	__label__ = "Vertex Colors"
	
	def draw(self, context):
		layout = self.layout
		
		me = context.mesh

		row = layout.row()
		col = row.column()

		col.template_list(me, "vertex_colors", me, "active_vertex_color_index", rows=2)

		col = row.column(align=True)
		col.itemO("mesh.vertex_color_add", icon='ICON_ZOOMIN', text="")
		col.itemO("mesh.vertex_color_remove", icon='ICON_ZOOMOUT', text="")

		lay = me.active_vertex_color
		if lay:
			layout.itemR(lay, "name")

bpy.types.register(DATA_PT_context_mesh)
bpy.types.register(DATA_PT_normals)
bpy.types.register(DATA_PT_settings)
bpy.types.register(DATA_PT_vertex_groups)
bpy.types.register(DATA_PT_shape_keys)
bpy.types.register(DATA_PT_uv_texture)
bpy.types.register(DATA_PT_vertex_colors)
