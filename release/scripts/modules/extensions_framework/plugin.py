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
from . import init_properties
from . import log

import bpy

class plugin(object):
	
	# List of IDPropertyGroup types to create in the scene
	property_groups = [
		# ('bpy.type prototype to attach to. eg. Scene', <declarative_property_group type>)
	]
	
	@classmethod
	def install(r_class):
		# create custom property groups
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
					#print('Created IDPropertyGroup %s.%s' % (prototype, property_group.__name__))
			else:
				call_init = True
			
			if call_init:
				init_properties(property_group, property_group.properties)
				#print('Initialised IDPropertyGroup %s' % property_group.__name__)
		
		log('Extension "%s" initialised' % r_class.bl_label)
	
	@classmethod
	def uninstall(r_class):
		# unregister property groups in reverse order
		reverse_property_groups = [p for p in r_class.property_groups]
		reverse_property_groups.reverse()
		for property_group_parent, property_group in reverse_property_groups:
			prototype = getattr(bpy.types, property_group_parent)
			prototype.RemoveProperty(property_group.__name__)
