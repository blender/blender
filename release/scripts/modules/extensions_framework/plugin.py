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

from extensions_framework import init_properties
from extensions_framework import log

class plugin(object):
	"""Base class for plugins which wish to make use of utilities
	provided in extensions_framework. Using the property_groups
	attribute and the install() and uninstall() methods, a large number
	of custom scene properties can be easily defined, displayed and
	managed.
	
	TODO: Rename, 'extension' would be more appropriate than 'plugin'
	
	"""
	
	"""The property_groups defines a list of declarative_property_group
	types to create in specified types during the initialisation of the
	plugin.
	Item format:
	('bpy.type prototype to attach to', <declarative_property_group>)
	
	Example item:
	('Scene', myaddon_property_group)
	In this example, a new property group will be attached to
	bpy.types.Scene and all of the properties described in that group
	will be added to it.
	See extensions_framework.declarative_property_group.
	
	"""
	property_groups = []
	
	@classmethod
	def install(r_class):
		"""Initialise this plugin. So far, all this does is to create
		custom property groups specified in the property_groups
		attribute.
		
		"""
		for property_group_parent, property_group in r_class.property_groups:
			call_init = False
			if property_group_parent is not None:
				prototype = getattr(bpy.types, property_group_parent)
				if not hasattr(prototype, property_group.__name__):
					init_properties(prototype, [{
						'type': 'pointer',
						'attr': property_group.__name__,
						'ptype': property_group,
						'name': property_group.__name__,
						'description': property_group.__name__
					}])
					call_init = True
			else:
				call_init = True
			
			if call_init:
				init_properties(property_group, property_group.properties)
		
		log('Extension "%s" initialised' % r_class.bl_label)
	
	@classmethod
	def uninstall(r_class):
		"""Unregister property groups in reverse order"""
		reverse_property_groups = [p for p in r_class.property_groups]
		reverse_property_groups.reverse()
		for property_group_parent, property_group in reverse_property_groups:
			prototype = getattr(bpy.types, property_group_parent)
			prototype.RemoveProperty(property_group.__name__)
