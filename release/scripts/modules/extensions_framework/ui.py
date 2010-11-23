# -*- coding: utf8 -*-
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# --------------------------------------------------------------------------
# Blender 2.5 Extensions Framework
# --------------------------------------------------------------------------
#
# Authors:
# Doug Hammond
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#
# ***** END GPL LICENCE BLOCK *****
#
import bpy

from extensions_framework.validate import Visibility

class EF_OT_msg(bpy.types.Operator):
	"""An operator to show simple messages in the UI"""
	bl_idname = 'ef.msg'
	bl_label = 'Show UI Message'
	msg_type = bpy.props.StringProperty(default='INFO')
	msg_text = bpy.props.StringProperty(default='')
	def execute(self, context):
		self.report({self.properties.msg_type}, self.properties.msg_text)
		return {'FINISHED'}

def _get_item_from_context(context, path):
	"""Utility to get an object when the path to it is known:
	_get_item_from_context(context, ['a','b','c']) returns
	context.a.b.c
	No error checking is performed other than checking that context
	is not None. Exceptions caused by invalid path should be caught in
	the calling code.
	
	"""
	
	if context is not None:
		for p in path:
			context = getattr(context, p)
	return context

class property_group_renderer(object):
	"""Mix-in class for sub-classes of bpy.types.Panel. This class
	will provide the draw() method which implements drawing one or
	more property groups derived from
	extensions_framework.declarative_propery_group.
	The display_property_groups list attribute describes which
	declarative_property_groups should be drawn in the Panel, and
	how to extract those groups from the context passed to draw().
	
	"""
	
	"""The display_property_groups list attribute specifies which
	custom declarative_property_groups this panel should draw, and
	where to find that property group in the active context.
	Example item:
		( ('scene',), 'myaddon_property_group')
	In this case, this renderer will look for properties in
	context.scene.myaddon_property_group to draw in the Panel.
	
	"""
	display_property_groups = []
	
	def draw(self, context):
		"""Sub-classes should override this if they need to display
		other (object-related) property groups. super().draw(context)
		can be a useful call in those cases.
		
		"""
		for property_group_path, property_group_name in \
			self.display_property_groups:
			ctx = _get_item_from_context(context, property_group_path)
			property_group = getattr(ctx, property_group_name)
			for p in property_group.controls:
				self.draw_column(p, self.layout, ctx, context,
					property_group=property_group)
			property_group.draw_callback(context)
	
	@staticmethod
	def property_reload():
		"""Override this in sub classes to force data refresh upon scene reload
		
		TODO: Remove, this is not used anywhere
		"""
		pass
	
	def check_visibility(self, lookup_property, property_group):
		"""Determine if the lookup_property should be drawn in the Panel"""
		vt = Visibility(property_group)
		if lookup_property in property_group.visibility.keys():
			if hasattr(property_group, lookup_property):
				member = getattr(property_group, lookup_property)
			else:
				member = None
			return vt.test_logic(member,
				property_group.visibility[lookup_property])
		else:
			return True
			
	def draw_column(self, control_list_item, layout, context,
					supercontext=None, property_group=None):
		"""Draw a column's worth of UI controls in this Panel"""
		if type(control_list_item) is list:
			do_split = False
			
			found_percent = None
			for sp in control_list_item:
				if type(sp) is float:
					found_percent = sp
				elif type(sp) is list:
					for ssp in sp:
						do_split = do_split and self.check_visibility(ssp,
							property_group)
				else:
					do_split = do_split or self.check_visibility(sp,
						property_group)
			
			if do_split:
				if found_percent is not None:
					splt = layout.split(percentage=found_percent)
				else:
					splt = layout.row(True)
				for sp in [s for s in control_list_item if type(s) in \
						[str, list]]:
					col2 = splt.column()
					self.draw_column(sp, col2, context, supercontext,
						property_group)
		else:
			if self.check_visibility(control_list_item, property_group):
				
				for current_property in property_group.properties:
					if current_property['attr'] == control_list_item:
						current_property_keys = current_property.keys() 
						if 'type' in current_property_keys:
							
							if current_property['type'] in ['int', 'float',
								'float_vector', 'enum', 'string']:
								layout.prop(
									property_group,
									control_list_item,
									text = current_property['name'],
									expand = current_property['expand'] \
										if 'expand' in current_property_keys \
										else False,
									slider = current_property['slider'] \
										if 'slider' in current_property_keys \
										else False,
									toggle = current_property['toggle'] \
										if 'toggle' in current_property_keys \
										else False,
									icon_only = current_property['icon_only'] \
										if 'icon_only' in current_property_keys \
										else False,
									event = current_property['event'] \
										if 'event' in current_property_keys \
										else False,
									full_event = current_property['full_event'] \
										if 'full_event' in current_property_keys \
										else False,
									emboss = current_property['emboss'] \
										if 'emboss' in current_property_keys \
										else True,
								)
							if current_property['type'] in ['bool']:
								layout.prop(
									property_group,
									control_list_item,
									text = current_property['name'],
									toggle = current_property['toggle'] \
										if 'toggle' in current_property_keys \
										else False,
									icon_only = current_property['icon_only'] \
										if 'icon_only' in current_property_keys \
										else False,
									event = current_property['event'] \
										if 'event' in current_property_keys \
										else False,
									full_event = current_property['full_event'] \
										if 'full_event' in current_property_keys \
										else False,
									emboss = current_property['emboss'] \
										if 'emboss' in current_property_keys \
										else True,
								)
							elif current_property['type'] in ['operator']:
								layout.operator(current_property['operator'],
									text = current_property['text'],
									icon = current_property['icon']
								)
							
							elif current_property['type'] in ['text']:
								layout.label(
									text = current_property['name']
								)
								
							elif current_property['type'] in ['template_list']:
								layout.template_list(
									current_property['src'](supercontext, context),
									current_property['src_attr'],
									current_property['trg'](supercontext, context),
									current_property['trg_attr'],
									rows = 4 \
										if not 'rows' in current_property_keys \
										else current_property['rows'],
									maxrows = 4 \
										if not 'rows' in current_property_keys \
										else current_property['rows'],
									type = 'DEFAULT' \
										if not 'list_type' in current_property_keys \
										else current_property['list_type']
								)
								
							elif current_property['type'] in ['prop_search']:
								layout.prop_search(
									current_property['trg'](supercontext,
										context),
									current_property['trg_attr'],
									current_property['src'](supercontext,
										context),
									current_property['src_attr'],
									text = current_property['name'],
								)
						else:
							layout.prop(property_group, control_list_item)
						
						# Fire a draw callback if specified
						if 'draw' in current_property_keys:
							current_property['draw'](supercontext, context)
						
						break
