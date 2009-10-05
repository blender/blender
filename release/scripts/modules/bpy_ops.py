# for slightly faster access
from bpy.__ops__ import add		as op_add
from bpy.__ops__ import remove		as op_remove
from bpy.__ops__ import dir		as op_dir
from bpy.__ops__ import call		as op_call
from bpy.__ops__ import as_string	as op_as_string
from bpy.__ops__ import get_rna	as op_get_rna

# Keep in sync with WM_types.h
context_dict = {
	'INVOKE_DEFAULT':0,
	'INVOKE_REGION_WIN':1,
	'INVOKE_AREA':2,
	'INVOKE_SCREEN':3,
	'EXEC_DEFAULT':4,
	'EXEC_REGION_WIN':5,
	'EXEC_AREA':6,
	'EXEC_SCREEN':7,
}

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
	
	def __call__(self, *args, **kw):
		
		# Get the operator from blender
		if len(args) > 1:
			raise ValueError("only one argument for the execution context is supported ")
		
		if args:
			try:
				context = context_dict[args[0]]
			except:
				raise ValueError("Expected a single context argument in: " + str(list(context_dict.keys())))
			
			return op_call(self.idname(), kw, context)
		
		else:
			return op_call(self.idname(), kw)
	
	def get_rna(self):
		'''
		currently only used for '__rna__'
		'''
		return op_get_rna(self.idname())
			
	
	def __repr__(self): # useful display, repr(op)
		return op_as_string(self.idname())
	
	def __str__(self): # used for print(...)
		return "<function bpy.ops.%s.%s at 0x%x'>" % (self.module, self.func, id(self))

import bpy
bpy.ops = bpy_ops()

# TODO, C macro's cant define settings :|

class MESH_OT_delete_edgeloop(bpy.types.Operator):
	'''Export a single object as a stanford PLY with normals, colours and texture coordinates.'''
	__idname__ = "mesh.delete_edgeloop"
	__label__ = "Export PLY"
	
	def execute(self, context):
		bpy.ops.tfm.edge_slide(value=1.0)
		bpy.ops.mesh.select_more()
		bpy.ops.mesh.remove_doubles()
		return ('FINISHED',)


bpy.ops.add(MESH_OT_delete_edgeloop)

