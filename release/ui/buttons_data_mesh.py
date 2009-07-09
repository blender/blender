
import bpy

class DataButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "data"
	
	def poll(self, context):
		return (context.mesh != None)


class DATA_PT_context_mesh(DataButtonsPanel):
	__idname__ = "DATA_PT_context_mesh"
	__label__ = " "
	
	def poll(self, context):
		return (context.object and context.object.type == 'MESH')

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




class DATA_PT_mesh(DataButtonsPanel):
	__idname__ = "DATA_PT_mesh"
	__label__ = "Mesh"
	
	def poll(self, context):
		return (context.object and context.object.type == 'MESH')

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		mesh = context.mesh
		space = context.space_data

		if mesh:
			split = layout.split()
		
			col = split.column()
			col.itemR(mesh, "autosmooth")
			colsub = col.column()
			colsub.active = mesh.autosmooth
			colsub.itemR(mesh, "autosmooth_angle", text="Angle")
			sub = split.column()
			sub.itemR(mesh, "vertex_normal_flip")
			sub.itemR(mesh, "double_sided")
			
			layout.itemS()
			layout.itemR(mesh, "texco_mesh")


class DATA_PT_materials(DataButtonsPanel):
	__idname__ = "DATA_PT_materials"
	__label__ = "Materials"
	
	def poll(self, context):
		return (context.object and context.object.type in ('MESH', 'CURVE', 'FONT', 'SURFACE'))

	def draw(self, context):
		layout = self.layout
		ob = context.object

		row = layout.row()

		row.template_list(ob, "materials", ob, "active_material_index")

		col = row.column(align=True)
		col.itemO("OBJECT_OT_material_slot_add", icon="ICON_ZOOMIN", text="")
		col.itemO("OBJECT_OT_material_slot_remove", icon="ICON_ZOOMOUT", text="")

		if context.edit_object:
			row = layout.row(align=True)

			row.itemO("OBJECT_OT_material_slot_assign", text="Assign")
			row.itemO("OBJECT_OT_material_slot_select", text="Select")
			row.itemO("OBJECT_OT_material_slot_deselect", text="Deselect")

		"""
		layout.itemS()

		box= layout.box()

		row = box.row()
		row.template_list(ob, "materials", ob, "active_material_index", compact=True)

		subrow = row.row(align=True)
		subrow.itemO("OBJECT_OT_material_slot_add", icon="ICON_ZOOMIN", text="")
		subrow.itemO("OBJECT_OT_material_slot_remove", icon="ICON_ZOOMOUT", text="")
		"""

class DATA_PT_vertex_groups(DataButtonsPanel):
	__idname__ = "DATA_PT_vertex_groups"
	__label__ = "Vertex Groups"
	
	def poll(self, context):
		return (context.object and context.object.type in ('MESH', 'LATTICE'))

	def draw(self, context):
		layout = self.layout
		ob = context.object

		row = layout.row()

		row.template_list(ob, "vertex_groups", ob, "active_vertex_group_index")

		col = row.column(align=True)
		col.itemO("OBJECT_OT_vertex_group_add", icon="ICON_ZOOMIN", text="")
		col.itemO("OBJECT_OT_vertex_group_remove", icon="ICON_ZOOMOUT", text="")

		col.itemO("OBJECT_OT_vertex_group_copy", icon="ICON_BLANK1", text="")
		if ob.data.users > 1:
			col.itemO("OBJECT_OT_vertex_group_copy_to_linked", icon="ICON_BLANK1", text="")

		if context.edit_object:
			row = layout.row(align=True)

			row.itemO("OBJECT_OT_vertex_group_assign", text="Assign")
			row.itemO("OBJECT_OT_vertex_group_remove_from", text="Remove")
			row.itemO("OBJECT_OT_vertex_group_select", text="Select")
			row.itemO("OBJECT_OT_vertex_group_deselect", text="Deselect")

			layout.itemR(context.tool_settings, "vertex_group_weight", text="Weight")

class DATA_PT_shape_keys(DataButtonsPanel):
	__idname__ = "DATA_PT_shape_keys"
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
		subcol.itemO("OBJECT_OT_shape_key_add", icon="ICON_ZOOMIN", text="")
		subcol.itemO("OBJECT_OT_shape_key_remove", icon="ICON_ZOOMOUT", text="")

		if kb:
			col.itemS()

			subcol = col.column(align=True)
			subcol.itemR(ob, "shape_key_lock", icon="ICON_PINNED", text="")
			subcol.itemR(kb, "mute", icon="ICON_MUTE_IPO_ON", text="")

			if key.relative:
				row = layout.row()
				row.itemR(key, "relative")
				row.itemL()

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

		if context.edit_object:
			layout.enabled = False

class DATA_PT_uv_texture(DataButtonsPanel):
	__idname__ = "DATA_PT_uv_texture"
	__label__ = "UV Texture"
	
	def draw(self, context):
		layout = self.layout
		me = context.mesh

		row = layout.row()

		row.template_list(me, "uv_textures", me, "active_uv_texture_index")

		col = row.column(align=True)
		col.itemO("MESH_OT_uv_texture_add", icon="ICON_ZOOMIN", text="")
		col.itemO("MESH_OT_uv_texture_remove", icon="ICON_ZOOMOUT", text="")

class DATA_PT_vertex_colors(DataButtonsPanel):
	__idname__ = "DATA_PT_vertex_colors"
	__label__ = "Vertex Colors"
	
	def draw(self, context):
		layout = self.layout
		me = context.mesh

		row = layout.row()

		row.template_list(me, "vertex_colors", me, "active_vertex_color_index")

		col = row.column(align=True)
		col.itemO("MESH_OT_vertex_color_add", icon="ICON_ZOOMIN", text="")
		col.itemO("MESH_OT_vertex_color_remove", icon="ICON_ZOOMOUT", text="")

bpy.types.register(DATA_PT_context_mesh)
bpy.types.register(DATA_PT_mesh)
bpy.types.register(DATA_PT_materials)
bpy.types.register(DATA_PT_vertex_groups)
bpy.types.register(DATA_PT_shape_keys)
bpy.types.register(DATA_PT_uv_texture)
bpy.types.register(DATA_PT_vertex_colors)

