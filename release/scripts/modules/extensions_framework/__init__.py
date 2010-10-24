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

import os, time
import bpy

#---------------------------------------------------------------------------------------------------------------------- 

def log(str, popup=False, module_name='EF'):
	print("[%s %s] %s" % (module_name, time.strftime('%Y-%b-%d %H:%M:%S'), str))
	if popup:
		bpy.ops.ef.msg(
			msg_type='WARNING',
			msg_text=str
		)

#---------------------------------------------------------------------------------------------------------------------- 

from .ui import EF_OT_msg

bpy.types.register(EF_OT_msg)
ef_path = os.path.realpath( os.path.dirname(__file__) )
# log('Extensions_Framework detected and loaded from %s'%ef_path)

del EF_OT_msg, os

#---------------------------------------------------------------------------------------------------------------------- 

class ef(object):
	'''
	Extensions Framework base class
	'''
	
	added_property_cache = {}


def init_properties(obj, props, cache=True):
	if not obj in ef.added_property_cache.keys():
		ef.added_property_cache[obj] = []
	
	for prop in props:
		if cache and prop['attr'] in ef.added_property_cache[obj]:
			continue
		try:
			if prop['type'] == 'bool':
				t = bpy.props.BoolProperty
				a = {k: v for k,v in prop.items() if k in ['name','description','default']}
			elif prop['type'] == 'collection':
				t = bpy.props.CollectionProperty
				a = {k: v for k,v in prop.items() if k in ["ptype", "name", "description"]}
				a['type'] = a['ptype']
				del a['ptype']
			elif prop['type'] == 'enum':
				t = bpy.props.EnumProperty
				a = {k: v for k,v in prop.items() if k in ["items", "name", "description", "default"]}
			elif prop['type'] == 'float':
				t = bpy.props.FloatProperty
				a = {k: v for k,v in prop.items() if k in ["name", "description", "min", "max", "soft_min", "soft_max", "default", "precision"]}
			elif prop['type'] == 'float_vector':
				t = bpy.props.FloatVectorProperty
				a = {k: v for k,v in prop.items() if k in ["name", "description", "min", "max", "soft_min", "soft_max", "default", "precision", "size", "subtype"]}
			elif prop['type'] == 'int':
				t = bpy.props.IntProperty
				a = {k: v for k,v in prop.items() if k in ["name", "description", "min", "max", "soft_min", "soft_max", "default"]}
			elif prop['type'] == 'pointer':
				t = bpy.props.PointerProperty
				a = {k: v for k,v in prop.items() if k in ["ptype", "name", "description"]}
				a['type'] = a['ptype']
				del a['ptype']
			elif prop['type'] == 'string':
				t = bpy.props.StringProperty
				a = {k: v for k,v in prop.items() if k in ["name", "description", "maxlen", "default", "subtype"]}
			else:
				#ef.log('Property type not recognised: %s' % prop['type'])
				continue
			
			setattr(obj, prop['attr'], t(**a))
			
			ef.added_property_cache[obj].append(prop['attr'])
			#log('Created property %s.%s' % (obj, prop['attr']))
		except KeyError:
			continue

class declarative_property_group(bpy.types.IDPropertyGroup):
	
	controls = [
		# this list controls the order of property
		# layout when rendered by a property_group_renderer.
		# This can be a nested list, where each list
		# becomes a row in the panel layout.
		# nesting may be to any depth
	]
	
	# Include some properties in display based on values of others
	visibility = {
		# See ef.validate for test syntax
	}
	
	# engine-specific properties to create in the scene.
	# Each item should be a dict of args to pass to a 
	# bpy.types.Scene.<?>Property function, with the exception
	# of 'type' which is used and stripped by ef
	properties = [
		# example:
		#{
		#	'type': 'int',
		#	'attr': 'threads',
		#	'name': 'Render Threads',
		#	'description': 'Number of threads to use',
		#	'default': 1,
		#	'min': 1,
		#	'soft_min': 1,
		#	'max': 64,
		#	'soft_max': 64
		#},
	]
	
	def draw_callback(self, context):
		'''
		Sub-classes can override this to get a callback
		when rendered by a property_group_renderer class
		'''
		
		pass
	
	@classmethod
	def get_exportable_properties(cls):
		out = []
		for prop in cls.properties:
			if 'save_in_preset' in prop.keys() and prop['save_in_preset']:
				out.append(prop)
		return out
