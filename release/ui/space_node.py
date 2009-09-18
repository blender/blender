
import bpy

class NODE_HT_header(bpy.types.Header):
	__space_type__ = 'NODE_EDITOR'

	def draw(self, context):
		layout = self.layout
		
		snode = context.space_data

		row = layout.row(align=True)
		row.template_header()

		if context.area.show_menus:
			sub = row.row(align=True)
			sub.itemM("NODE_MT_view")
			sub.itemM("NODE_MT_select")
			sub.itemM("NODE_MT_add")
			sub.itemM("NODE_MT_node")

		row = layout.row()
		row.itemR(snode, "tree_type", text="", expand=True)

		if snode.tree_type == 'MATERIAL':
			ob = snode.id_from
			id = snode.id
			if ob:
				layout.template_ID(ob, "active_material", new="material.new")
			if id:
				layout.itemR(id, "use_nodes")

		elif snode.tree_type == 'TEXTURE':
			row.itemR(snode, "texture_type", text="", expand=True)

			id = snode.id
			id_from = snode.id_from
			if id_from:
				layout.template_ID(id_from, "active_texture", new="texture.new")
			if id:
				layout.itemR(id, "use_nodes")

		elif snode.tree_type == 'COMPOSITING':
			id = snode.id

			layout.itemR(id, "use_nodes")
			layout.itemR(id.render_data, "free_unused_nodes", text="Free Unused")
			layout.itemR(snode, "backdrop")

class NODE_MT_view(bpy.types.Menu):
	__space_type__ = 'NODE_EDITOR'
	__label__ = "View"

	def draw(self, context):
		layout = self.layout

		# layout.itemO("grease_pencil..")
		# layout.itemS()

		layout.itemO("view2d.zoom_in")
		layout.itemO("view2d.zoom_out")

		layout.itemS()

		layout.itemO("node.view_all")
		layout.itemO("screen.screen_full_area")

class NODE_MT_select(bpy.types.Menu):
	__space_type__ = 'NODE_EDITOR'
	__label__ = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("node.select_border")

		layout.itemS()
		layout.itemO("node.select_all")
		layout.itemO("node.select_linked_from")
		layout.itemO("node.select_linked_to")

class NODE_MT_node(bpy.types.Menu):
	__space_type__ = 'NODE_EDITOR'
	__label__ = "Node"

	def draw(self, context):
		layout = self.layout

		layout.itemO("tfm.translate")
		layout.itemO("tfm.resize")
		layout.itemO("tfm.rotate")

		layout.itemS()

		layout.itemO("node.duplicate")
		layout.itemO("node.delete")

		# XXX
		# layout.itemS()
		# layout.itemO("node.make_link")
		layout.itemS()
		layout.itemO("node.group_edit")
		layout.itemO("node.group_ungroup")
		layout.itemO("node.group_make")

		layout.itemS()

		layout.itemO("node.visibility_toggle")

		# XXX
		# layout.itemO("node.rename")
		# layout.itemS()
		# layout.itemO("node.show_cyclic_dependencies")

bpy.types.register(NODE_HT_header)
bpy.types.register(NODE_MT_view)
bpy.types.register(NODE_MT_select)
bpy.types.register(NODE_MT_node)
