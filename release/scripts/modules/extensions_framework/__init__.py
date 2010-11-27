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
import time

import bpy

from extensions_framework.ui import EF_OT_msg

bpy.types.register(EF_OT_msg)
del EF_OT_msg


def log(str, popup=False, module_name='EF'):
	"""Print a message to the console, prefixed with the module_name
	and the current time. If the popup flag is True, the message will
	be raised in the UI as a warning using the operator bpy.ops.ef.msg.
	
	"""
	print("[%s %s] %s" %
		(module_name, time.strftime('%Y-%b-%d %H:%M:%S'), str))
	if popup:
		bpy.ops.ef.msg(
			msg_type='WARNING',
			msg_text=str
		)


added_property_cache = {}

def init_properties(obj, props, cache=True):
	"""Initialise custom properties in the given object or type.
	The props list is described in the declarative_property_group
	class definition. If the cache flag is False, this function
	will attempt to redefine properties even if they have already been
	added.
	
	"""
	
	if not obj in added_property_cache.keys():
		added_property_cache[obj] = []
	
	for prop in props:
		try:
			if cache and prop['attr'] in added_property_cache[obj]:
				continue
			
			if prop['type'] == 'bool':
				t = bpy.props.BoolProperty
				a = {k: v for k,v in prop.items() if k in ['name',
					'description','default']}
			elif prop['type'] == 'collection':
				t = bpy.props.CollectionProperty
				a = {k: v for k,v in prop.items() if k in ["ptype", "name",
					"description"]}
				a['type'] = a['ptype']
				del a['ptype']
			elif prop['type'] == 'enum':
				t = bpy.props.EnumProperty
				a = {k: v for k,v in prop.items() if k in ["items", "name",
					"description", "default"]}
			elif prop['type'] == 'float':
				t = bpy.props.FloatProperty
				a = {k: v for k,v in prop.items() if k in ["name",
					"description", "min", "max", "soft_min", "soft_max",
					"default", "precision"]}
			elif prop['type'] == 'float_vector':
				t = bpy.props.FloatVectorProperty
				a = {k: v for k,v in prop.items() if k in ["name",
					"description", "min", "max", "soft_min", "soft_max",
					"default", "precision", "size", "subtype"]}
			elif prop['type'] == 'int':
				t = bpy.props.IntProperty
				a = {k: v for k,v in prop.items() if k in ["name",
					"description", "min", "max", "soft_min", "soft_max",
					"default"]}
			elif prop['type'] == 'pointer':
				t = bpy.props.PointerProperty
				a = {k: v for k,v in prop.items() if k in ["ptype", "name",
					"description"]}
				a['type'] = a['ptype']
				del a['ptype']
			elif prop['type'] == 'string':
				t = bpy.props.StringProperty
				a = {k: v for k,v in prop.items() if k in ["name",
					"description", "maxlen", "default", "subtype"]}
			else:
				continue
			
			setattr(obj, prop['attr'], t(**a))
			
			added_property_cache[obj].append(prop['attr'])
		except KeyError:
			# Silently skip invalid entries in props
			continue


class declarative_property_group(bpy.types.IDPropertyGroup):
	"""A declarative_property_group describes a set of logically
	related properties, using a declarative style to list each
	property type, name, values, and other relevant information.
	The information provided for each property depends on the
	property's type.
	
	The properties list attribute in this class describes the
	properties present in this group.
	
	Some additional information about the properties in this group
	can be specified, so that a UI can be generated to display them.
	To that end, the controls list attribute and the visibility dict
	attribute are present here, to be read and interpreted by a
	property_group_renderer object.
	See extensions_framework.ui.property_group_renderer.
	
	"""
	
	"""This list controls the order of property layout when rendered
	by a property_group_renderer. This can be a nested list, where each
	list becomes a row in the panel layout. Nesting may be to any depth.
	
	"""
	controls = []
	
	"""The visibility dict controls the display of properties based on
	the value of other properties. See extensions_framework.validate
	for test syntax.
	
	"""
	visibility = {}
	
	"""The properties list describes each property to be created. Each
	item should be a dict of args to pass to a
	bpy.props.<?>Property function, with the exception of 'type'
	which is used and stripped by extensions_framework in order to
	determine which Property creation function to call.
	
	Example item:
	{
		'type': 'int',								# bpy.props.IntProperty
		'attr': 'threads',							# bpy.types.<type>.threads
		'name': 'Render Threads',					# Rendered next to the UI
		'description': 'Number of threads to use',	# Tooltip text in the UI
		'default': 1,
		'min': 1,
		'soft_min': 1,
		'max': 64,
		'soft_max': 64
	}
	
	"""
	properties = []
	
	def draw_callback(self, context):
		"""Sub-classes can override this to get a callback when
		rendering is completed by a property_group_renderer sub-class.
		
		"""
		
		pass
	
	@classmethod
	def get_exportable_properties(cls):
		"""Return a list of properties which have the 'save_in_preset' key
		set to True, and hence should be saved into preset files.
		
		"""
		
		out = []
		for prop in cls.properties:
			if 'save_in_preset' in prop.keys() and prop['save_in_preset']:
				out.append(prop)
		return out
