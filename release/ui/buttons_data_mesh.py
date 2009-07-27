
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.mesh != None)

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
		
		sub = split.column()
		sub.itemR(mesh, "vertex_normal_flip")
		sub.itemR(mesh, "double_sided")

class DATA_PT_vertex_groups(DataButtonsPanel):
	__label__ = "Vertex Groups"
	
	def poll(self, context):
		return (context.object and context.object.type in ('MESH', 'LATTICE'))

	def draw(self, context):
		layout = self.layout
		
		ob = context.object

		row = layout.row()
		row.template_list(ob, "vertex_groups", ob, "active_vertex_group_index")

		col = row.column(align=True)
		col.itemO("object.vertex_group_add", icon="ICON_ZOOMIN", text="")
		col.itemO("object.vertex_group_remove", icon="ICON_ZOOMOUT", text="")

		col.itemO("object.vertex_group_copy", icon="ICON_BLANK1", text="")
		if ob.data.users > 1:
			col.itemO("object.vertex_group_copy_to_linked", icon="ICON_BLANK1", text="")

		group = ob.active_vertex_group
		if group:
			row = layout.row()
			row.itemR(group, "name")

		if context.edit_object:
			row = layout.row(align=True)

			row.itemO("object.vertex_group_assign", text="Assign")
			row.itemO("object.vertex_group_remove_from", text="Remove")
			row.itemO("object.vertex_group_select", text="Select")
			row.itemO("object.vertex_group_deselect", text="Deselect")

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
		row.template_list(key, "keys", ob, "active_shape_key_index")

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("object.shape_key_add", icon="ICON_ZOOMIN", text="")
		subcol.itemO("object.shape_key_remove", icon="ICON_ZOOMOUT", text="")

		if kb:
			col.itemS()

			subcol = col.column(align=True)
			subcol.itemR(ob, "shape_key_lock", icon="ICON_UNPINNED", text="")
			subcol.itemR(kb, "mute", icon="ICON_MUTE_IPO_ON", text="")

			if key.relative:
				row = layout.row()
				row.itemR(key, "relative")
				row.itemL()

				row = layout.row()
				row.itemR(kb, "name")

				if ob.active_shape_key_index != 0:
					if not ob.shape_key_lock:
						row = layout.row(align=True)
						row.itemR(kb, "value", text="")
						row.itemR(kb, "slider_min", text="Min")
						row.itemR(kb, "slider_max", text="Max")

					row = layout.row()
					row.item_pointerR(kb, "vertex_group", ob, "vertex_groups", text="")
					row.item_pointerR(kb, "relative_key", key, "keys", text="")
			else:
				row = layout.row()
				row.itemR(key, "relative")
				row.itemR(key, "slurph")

				layout.itemR(kb, "name")

		if context.edit_object:
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
		col.itemO("mesh.uv_texture_add", icon="ICON_ZOOMIN", text="")
		col.itemO("mesh.uv_texture_remove", icon="ICON_ZOOMOUT", text="")

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
		col.itemO("mesh.vertex_color_add", icon="ICON_ZOOMIN", text="")
		col.itemO("mesh.vertex_color_remove", icon="ICON_ZOOMOUT", text="")

		lay = me.active_vertex_color
		if lay:
			layout.itemR(lay, "name")

bpy.types.register(DATA_PT_context_mesh)
bpy.types.register(DATA_PT_normals)
bpy.types.register(DATA_PT_vertex_groups)
bpy.types.register(DATA_PT_shape_keys)
bpy.types.register(DATA_PT_uv_texture)
bpy.types.register(DATA_PT_vertex_colors)
