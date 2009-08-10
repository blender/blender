# for slightly faster access
from bpy.__ops__ import add		as op_add
from bpy.__ops__ import remove		as op_remove
from bpy.__ops__ import dir		as op_dir
from bpy.__ops__ import call		as op_call
from bpy.__ops__ import get_rna	as op_get_rna

class bpy_ops(object):
	'''
	Fake module like class.
	
	 bpy.ops
	'''
	def add(self, pyop):
		op_add(pyop)
	
	def remove(self, pyop):
		op_remove(pyop)
	
	def __getattr__(self, module):
		'''
		gets a bpy.ops submodule
		'''
		return bpy_ops_submodule(module)
		
	def __dir__(self):
		
		submodules = set()
		
		# add this classes functions
		for id_name in dir(self.__class__):
			if not id_name.startswith('__'):
				submodules.add(id_name)
		
		for id_name in op_dir():
			id_split = id_name.split('_OT_', 1)
			
			if len(id_split) == 2:
				submodules.add(id_split[0].lower())
			else:
				submodules.add(id_split[0])
		
		return list(submodules)
		
	def __repr__(self):
		return "<module like class 'bpy.ops'>"


class bpy_ops_submodule(object):
	'''
	Utility class to fake submodules.
	
	eg. bpy.ops.object
	'''
	__keys__ = ('module',)
	
	def __init__(self, module):
		self.module = module
		
	def __getattr__(self, func):
		'''
		gets a bpy.ops.submodule function
		'''
		return bpy_ops_submodule_op(self.module, func)
		
	def __dir__(self):
		
		functions = set()
		
		module_upper = self.module.upper()
		
		for id_name in op_dir():
			id_split = id_name.split('_OT_', 1)
			if len(id_split) == 2 and module_upper == id_split[0]:
				functions.add(id_split[1])
		
		return list(functions)
	
	def __repr__(self):
		return "<module like class 'bpy.ops.%s'>" % self.module

class bpy_ops_submodule_op(object):
	'''
	Utility class to fake submodule operators.
	
	eg. bpy.ops.object.somefunc
	'''
	__keys__ = ('module', 'func')
	def __init__(self, module, func):
		self.module = module
		self.func = func
	
	def idname(self):
		# submod.foo -> SUBMOD_OT_foo
		return self.module.upper() + '_OT_' + self.func
	
	def __call__(self, **kw):
		
		# Get the operator from blender
		return op_call(self.idname(), kw)
	
	def get_rna(self):
		'''
		currently only used for '__rna__'
		'''
		return op_get_rna(self.idname())
			
	
	def __repr__(self):
		return "<function bpy.ops.%s.%s at 0x%x'>" % (self.module, self.func, id(self))

import bpy
bpy.ops = bpy_ops()




# A bit out of place but add button conversion code here
module_type = type(__builtins__)
import types
mod = types.ModuleType('button_convert')

import sys
sys.modules['button_convert'] = mod

def convert(expr):
	
	def replace(string, unit, scaler):
		# in need of regex
		change = True
		while change:
			change = False
			i = string.find(unit)
			if i != -1:
				if i>0 and not string[i-1].isalpha():
					i_end = i+len(unit)
					if i_end+1 >= len(string) or (not string[i_end+1].isalpha()):
						string = string.replace(unit, scaler)
						change = True
		# print(string)
		return string
	
	#imperial
	expr = replace(expr, 'mi', '*1609.344')
	expr = replace(expr, 'yd', '*0.9144')
	expr = replace(expr, 'ft', '*0.3048')
	expr = replace(expr, 'in', '*0.0254')
	
	# metric
	expr = replace(expr, 'km', '*1000')
	expr = replace(expr, 'm', '')
	expr = replace(expr, 'cm', '*0.01')
	expr = replace(expr, 'mm', '*0.001')
	
	return expr

mod.convert = convert

