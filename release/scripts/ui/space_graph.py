# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy


class GRAPH_HT_header(bpy.types.Header):
	bl_space_type = 'GRAPH_EDITOR'

	def draw(self, context):
		layout = self.layout

		st = context.space_data

		row = layout.row(align=True)
		row.template_header()

		if context.area.show_menus:
			sub = row.row(align=True)

			sub.menu("GRAPH_MT_view")
			sub.menu("GRAPH_MT_select")
			sub.menu("GRAPH_MT_channel")
			sub.menu("GRAPH_MT_key")

		layout.prop(st, "mode", text="")

		layout.template_dopesheet_filter(st.dopesheet)

		layout.prop(st, "autosnap", text="")
		layout.prop(st, "pivot_point", text="", icon_only=True)

		row = layout.row(align=True)
		row.operator("graph.copy", text="", icon='ICON_COPYDOWN')
		row.operator("graph.paste", text="", icon='ICON_PASTEDOWN')

		row = layout.row(align=True)
		if st.has_ghost_curves:
			row.operator("graph.ghost_curves_clear", text="", icon='ICON_GHOST_DISABLED')
		else:
			row.operator("graph.ghost_curves_create", text="", icon='ICON_GHOST_ENABLED')


class GRAPH_MT_view(bpy.types.Menu):
	bl_label = "View"

	def draw(self, context):
		layout = self.layout

		st = context.space_data

		layout.column()

		layout.separator()
		layout.operator("graph.properties", icon="ICON_MENU_PANEL")

		layout.prop(st, "show_cframe_indicator")
		layout.prop(st, "show_cursor")
		layout.prop(st, "show_sliders")
		layout.prop(st, "automerge_keyframes")

		layout.separator()
		if st.show_handles:
			layout.operator("graph.handles_view_toggle", icon="ICON_CHECKBOX_HLT", text="Show All Handles")
		else:
			layout.operator("graph.handles_view_toggle", icon="ICON_CHECKBOX_DEHLT", text="Show All Handles")
		layout.prop(st, "only_selected_curves_handles")
		layout.prop(st, "only_selected_keyframe_handles")
		layout.operator("anim.time_toggle")

		layout.separator()
		layout.operator("anim.previewrange_set")
		layout.operator("anim.previewrange_clear")
		layout.operator("graph.previewrange_set")

		layout.separator()
		layout.operator("graph.frame_jump")
		layout.operator("graph.view_all")

		layout.separator()
		layout.operator("screen.area_dupli")
		layout.operator("screen.screen_full_area")


class GRAPH_MT_select(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.column()
		# This is a bit misleading as the operator's default text is "Select All" while it actually *toggles* All/None
		layout.operator("graph.select_all_toggle")
		layout.operator("graph.select_all_toggle", text="Invert Selection").invert = True

		layout.separator()
		layout.operator("graph.select_border")
		layout.operator("graph.select_border", text="Border Axis Range").axis_range = True

		layout.separator()
		layout.operator("graph.select_column", text="Columns on Selected Keys").mode = 'KEYS'
		layout.operator("graph.select_column", text="Column on Current Frame").mode = 'CFRA'

		layout.operator("graph.select_column", text="Columns on Selected Markers").mode = 'MARKERS_COLUMN'
		layout.operator("graph.select_column", text="Between Selected Markers").mode = 'MARKERS_BETWEEN'


class GRAPH_MT_channel(bpy.types.Menu):
	bl_label = "Channel"

	def draw(self, context):
		layout = self.layout

		layout.column()
		layout.operator("anim.channels_setting_toggle")
		layout.operator("anim.channels_setting_enable")
		layout.operator("anim.channels_setting_disable")

		layout.separator()
		layout.operator("anim.channels_editable_toggle")

		layout.separator()
		layout.operator("anim.channels_expand")
		layout.operator("anim.channels_collapse")


class GRAPH_MT_key(bpy.types.Menu):
	bl_label = "Key"

	def draw(self, context):
		layout = self.layout

		layout.column()
		layout.menu("GRAPH_MT_key_transform", text="Transform")

		layout.operator_menu_enum("graph.snap", property="type", text="Snap")
		layout.operator_menu_enum("graph.mirror", property="type", text="Mirror")

		layout.separator()
		layout.operator("graph.keyframe_insert")
		layout.operator("graph.fmodifier_add")

		layout.separator()
		layout.operator("graph.duplicate")
		layout.operator("graph.delete")

		layout.separator()
		layout.operator_menu_enum("graph.handle_type", property="type", text="Handle Type")
		layout.operator_menu_enum("graph.interpolation_type", property="type", text="Interpolation Mode")
		layout.operator_menu_enum("graph.extrapolation_type", property="type", text="Extrapolation Mode")

		layout.separator()
		layout.operator("graph.clean")
		layout.operator("graph.sample")
		layout.operator("graph.bake")

		layout.separator()
		layout.operator("graph.copy")
		layout.operator("graph.paste")


class GRAPH_MT_key_transform(bpy.types.Menu):
	bl_label = "Transform"

	def draw(self, context):
		layout = self.layout

		layout.column()
		layout.operator("tfm.translate", text="Grab/Move")
		layout.operator("tfm.transform", text="Extend").mode = 'TIME_EXTEND'
		layout.operator("tfm.rotate", text="Rotate")
		layout.operator("tfm.resize", text="Scale")


bpy.types.register(GRAPH_HT_header) # header/menu classes
bpy.types.register(GRAPH_MT_view)
bpy.types.register(GRAPH_MT_select)
bpy.types.register(GRAPH_MT_channel)
bpy.types.register(GRAPH_MT_key)
bpy.types.register(GRAPH_MT_key_transform)
