import bpy

# This class is used for bpy.ops
#

class bpy_ops(object):
	'''
	Fake module like class.
	
	 bpy.ops
	'''
	def add(self, pyop):
		bpy.__ops__.add(pyop)
	
	def remove(self, pyop):
		bpy.__ops__.remove(pyop)
	
	def __getattr__(self, module):
		'''
		gets a bpy.ops submodule
		'''
		return bpy_ops_submodule(module)
		
	def __dir__(self):
		
		submodules = set()
		
		for id_name in dir(bpy.__ops__):
			
			if id_name.startswith('__'):
				continue
				
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
		
		for id_name in dir(bpy.__ops__):
			
			if id_name.startswith('__'):
				continue
			
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
	
	def __call__(self, **kw):
		# submod.foo -> SUBMOD_OT_foo
		id_name = self.module.upper() + '_OT_' + self.func
		
		# Get the operator from 
		internal_op = getattr(bpy.__ops__, id_name)
		
		# Call the op
		return internal_op(**kw)
		
	def __repr__(self):
		return "<function bpy.ops.%s.%s at 0x%x'>" % (self.module, self.func, id(self))

bpy.ops = bpy_ops()
