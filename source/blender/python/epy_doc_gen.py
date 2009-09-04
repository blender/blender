 # ***** BEGIN GPL LICENSE BLOCK *****
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
 # along with this program; if not, write to the Free Software Foundation,
 # Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 #
 # Contributor(s): Campbell Barton
 #
 # #**** END GPL LICENSE BLOCK #****

script_help_msg = '''
Usage,
run this script from blenders root path once you have compiled blender
	./blender.bin -P source/blender/python/epy_doc_gen.py

This will generate rna.py and bpyoperator.py in "./source/blender/python/doc/"
Generate html docs  by running...

	epydoc source/blender/python/doc/*.py -v \\
			-o source/blender/python/doc/html \\
			--inheritance=included \\
			--no-sourcecode \\
			--graph=classtree \\
			--graph-font-size=8

'''

# if you dont have graphvis installed ommit the --graph arg.

# GLOBALS['BASEDIR'] = './source/blender/python/doc'

import os

SUBMODULES = {}
INIT_SUBMODULES = {}			# store initialized files

INIT_SUBMODULES_IMPORTS = {}	# dont import the same module twice

def append_package(package_path, mod_name):
	
	init_path =	os.path.join(os.path.dirname(package_path), "__init__.py")
	
	# avoid double ups
	if mod_name:
		imports = INIT_SUBMODULES_IMPORTS.setdefault(init_path, [])
		if mod_name in imports:
			return
		imports.append(mod_name)
		
	try:
		os.makedirs(os.path.dirname(init_path)) # make the dirs if they are not there
	except:
		pass
	
	# Open the new file for the first time, otherwise keep it open.
	f = INIT_SUBMODULES.get(init_path)
	if f == None:
		f = INIT_SUBMODULES[init_path] = open(init_path, 'w')
	
	if mod_name:
		f.write("import %s\n" % mod_name)
		
	return f

def append_package_recursive(package_path, BASEPATH):
	'''
	assume the last item of package_path will be a file (not a dir thats created)
	'''
	
	package_path = os.path.splitext(package_path)[0] # incase of .py
	
	try:
		os.makedirs(os.path.join(BASEPATH, os.path.dirname(package_path))) # make the dirs if they are not there
	except:
		pass
	
	new_path = BASEPATH
	
	for mod_name in package_path.split(os.sep):
		init_path =	os.path.join(new_path, "__init__.py")
		new_path = os.path.join(new_path, mod_name)
		append_package(init_path, mod_name)


def open_submodule(subpath, BASEPATH):
	'''
	This is a utility function that lets us quickly add submodules
	'''
	
	# create all the package paths leading up to this module
	append_package_recursive(subpath, BASEPATH)
	
	module_name =		os.path.basename( os.path.splitext(subpath)[0] )
	mod_path =	os.path.join(BASEPATH, subpath)
	
	# Open the new file for the first time, otherwise keep it open.
	f = SUBMODULES.get(mod_path)
	if f == None:
		f = SUBMODULES[mod_path] = open(mod_path, 'w')

	f = open(mod_path, 'w')
	return f

def close_all():
	for files in (INIT_SUBMODULES.values(), SUBMODULES.values()):
		for f in files:
			if f.name.endswith('.py'):
				f_name = f.name 
				f.close()
				
				f = open(f_name, 'a')
				f.write("\ndel __package__\n") # annoying, no need do show this
			
			
			f.close()


def range_str(val):
	if val < -10000000:	return '-inf'
	if val >  10000000:	return 'inf'
	if type(val)==float:
		return '%g'  % val
	else:
		return str(val)	

def get_array_str(length):
	if length > 0:	return ' array of %d items' % length
	else:		return ''

def full_rna_struct_path(rna_struct):
	'''
	Needed when referencing one struct from another
	'''
	nested = rna_struct.nested
	if nested:
		return "%s.%s" % (full_rna_struct_path(nested), rna_struct.identifier)
	else:
		return rna_struct.identifier

def rna_id_ignore(rna_id):
	if rna_id == "rna_type":
		return True
	
	if "_OT_" in rna_id:
		return True
	if "_MT_" in rna_id:
		return True
	if "_PT_" in rna_id:
		return True
	
	return False

def write_func(rna, ident, out, func_type):
	# Keyword attributes
	kw_args = [] # "foo = 1", "bar=0.5", "spam='ENUM'"
	kw_arg_attrs = [] # "@type mode: int"
	
	rna_struct= rna.rna_type
	
	# Operators and functions work differently
	if func_type=='OPERATOR':
		rna_func_name = rna_struct.identifier.split("_OT_")[-1]
		rna_func_desc = rna_struct.description.strip()
		items = rna_struct.properties.items()
	else:
		rna_func_name = rna.identifier
		rna_func_desc = rna.description.strip()
		items = rna.parameters.items()
	
	
	for rna_prop_identifier, rna_prop in items:
		if rna_id_ignore(rna_prop_identifier):
			continue
		
		# clear vars			
		val = val_error = val_str = rna_prop_type = None
		
		# ['rna_type', 'name', 'array_length', 'description', 'hard_max', 'hard_min', 'identifier', 'precision', 'readonly', 'soft_max', 'soft_min', 'step', 'subtype', 'type']
		#rna_prop=  op_rna.rna_type.properties[attr]
		rna_prop_type = rna_prop.type.lower() # enum, float, int, boolean
		
		
		# only for rna functions, operators should not get pointers as args
		if rna_prop_type=='pointer':
			rna_prop_type_refine = "L{%s}" % rna_prop.fixed_type.identifier
		else:
			rna_prop_type_refine = rna_prop_type
		
		
		try:		length = rna_prop.array_length
		except:	length = 0
		
		array_str = get_array_str(length)
		
		if rna_prop.use_return:
			kw_type_str= "@rtype: %s%s" % (rna_prop_type_refine, array_str)
			kw_param_str= "@return: %s" % (rna_prop.description.strip())
		else:
			kw_type_str= "@type %s: %s%s" % (rna_prop_identifier, rna_prop_type_refine, array_str)
			kw_param_str= "@param %s: %s" % (rna_prop_identifier, rna_prop.description.strip())
		
		kw_param_set = False
		
		if func_type=='OPERATOR':
			try:
				val = getattr(rna, rna_prop_identifier)
				val_error = False
			except:
				val = "'<UNDEFINED>'"
				val_error = True
			
				
			if val_error:
				val_str = val
			elif rna_prop_type=='float':
				if length==0:
					val_str= '%g' % val
					if '.' not in val_str:
						val_str += '.0'
				else:
					# array
					val_str = str(tuple(val))
				
				kw_param_str += (' in (%s, %s)' % (range_str(rna_prop.hard_min), range_str(rna_prop.hard_max)))
				kw_param_set= True
				
			elif rna_prop_type=='int':
				if length==0:
					val_str='%d' % val
				else:
					val_str = str(tuple(val))
				
				# print(dir(rna_prop))
				kw_param_str += (' in (%s, %s)' % (range_str(rna_prop.hard_min), range_str(rna_prop.hard_max)))
				# These strings dont have a max length???
				#kw_param_str += ' (maximum length of %s)' %  (rna_prop.max_length)
				kw_param_set= True
				
			elif rna_prop_type=='boolean':
				if length==0:
					if val:	val_str='True'
					else:	val_str='False'
				else:
					val_str = str(tuple(val))
			
			elif rna_prop_type=='enum':
				# no array here?
				val_str="'%s'" % val
				# Too cramped
				kw_param_str += (' in (%s)' % ', '.join(rna_prop.items.keys()))
				
				kw_param_set= True
				
			elif rna_prop_type=='string':
				# no array here?
				val_str='"%s"' % val
			
			# todo - collection - array
			# print (rna_prop.type)
			
			kw_args.append('%s = %s' % (rna_prop_identifier, val_str))
			
			# stora 
		else:
			# currently functions dont have a default value
			if not rna_prop.use_return:
				kw_args.append('%s' % (rna_prop_identifier))
			else:
				kw_param_set = True

		
		# Same for operators and functions
		kw_arg_attrs.append(kw_type_str)
		if kw_param_set:
			kw_arg_attrs.append(kw_param_str)
		
	
	
	out.write(ident+'def %s(%s):\n' % (rna_func_name, ', '.join(kw_args)))
	out.write(ident+'\t"""\n')
	out.write(ident+'\t%s\n' % rna_func_desc)
	for desc in kw_arg_attrs:
		out.write(ident+'\t%s\n' % desc)
		
	# out.write(ident+'\t@rtype: None\n') # implicit
	out.write(ident+'\t"""\n')
	


def rna2epy(BASEPATH):
	
	# Use for faster lookups
	# use rna_struct.identifier as the key for each dict
	rna_struct_dict =		{}  # store identifier:rna lookups
	rna_full_path_dict =	{}	# store the result of full_rna_struct_path(rna_struct)
	rna_children_dict =		{}	# store all rna_structs nested from here
	rna_references_dict =	{}	# store a list of rna path strings that reference this type
	rna_functions_dict =	{}	# store all functions directly in this type (not inherited)
	rna_words = set()
	
	# def write_func(rna_func, ident):
	
	
	def write_struct(rna_struct, ident):
		identifier = rna_struct.identifier
		
		rna_base = rna_struct.base
		
		if rna_base:
			out.write(ident+ 'class %s(%s):\n' % (identifier, rna_base.identifier))
			rna_base_prop_keys = rna_base.properties.keys() # could be cached
			rna_base_func_keys = [f.identifier for f in rna_base.functions]
		else:
			out.write(ident+ 'class %s:\n' % identifier)
			rna_base_prop_keys = []
			rna_base_func_keys = []
		
		out.write(ident+ '\t"""\n')
		
		title = 'The %s Object' % rna_struct.name
		description = rna_struct.description.strip()
		out.write(ident+ '\t%s\n' %  title)
		out.write(ident+ '\t%s\n' %  ('=' * len(title)))
		out.write(ident+ '\t\t%s\n' %  description)
		rna_words.update(description.split())
		
		
		# For convenience, give a list of all places were used.
		rna_refs= rna_references_dict[identifier]
		
		if rna_refs:
			out.write(ident+ '\t\t\n')
			out.write(ident+ '\t\tReferences\n')
			out.write(ident+ '\t\t==========\n')
			
			for rna_ref_string in rna_refs:
				out.write(ident+ '\t\t\t- L{%s}\n' % rna_ref_string)
			
			out.write(ident+ '\t\t\n')
		
		else:
			out.write(ident+ '\t\t\n')
			out.write(ident+ '\t\t(no references to this struct found)\n')
			out.write(ident+ '\t\t\n')
		
		for rna_prop_identifier, rna_prop in rna_struct.properties.items():
			
			if rna_prop_identifier=='RNA':					continue
			if rna_id_ignore(rna_prop_identifier):			continue
			if rna_prop_identifier in rna_base_prop_keys:	continue # does this prop exist in our parent class, if so skip
			
			rna_desc = rna_prop.description.strip()
			
			if rna_desc: rna_words.update(rna_desc.split())
			if not rna_desc: rna_desc = rna_prop.name
			if not rna_desc: rna_desc = 'Note - No documentation for this property!'
			
			rna_prop_type = rna_prop.type.lower()
			
			if rna_prop_type=='collection':	collection_str = 'Collection of '
			else:							collection_str = ''
			
			try:		rna_prop_ptr = rna_prop.fixed_type
			except:	rna_prop_ptr = None
			
			try:		length = rna_prop.array_length
			except:	length = 0
			
			array_str = get_array_str(length)
			
			if rna_prop.editable:	readonly_str = ''
			else:				readonly_str = ' (readonly)'
			
			if rna_prop_ptr: # Use the pointer type
				out.write(ident+ '\t@ivar %s: %s\n' %  (rna_prop_identifier, rna_desc))
				out.write(ident+ '\t@type %s: %sL{%s}%s%s\n' %  (rna_prop_identifier, collection_str, rna_prop_ptr.identifier, array_str, readonly_str))
			else:
				if rna_prop_type == 'enum':
					if 0:
						out.write(ident+ '\t@ivar %s: %s in (%s)\n' %  (rna_prop_identifier, rna_desc, ', '.join(rna_prop.items.keys())))
					else:
						out.write(ident+ '\t@ivar %s: %s in...\n' %  (rna_prop_identifier, rna_desc))
						for e, e_rna_prop in rna_prop.items.items():
							#out.write(ident+ '\t\t- %s: %s\n' % (e, e_rna_prop.description)) # XXX - segfaults, FIXME
							out.write(ident+ '\t\t- %s\n' % e)
						
					out.write(ident+ '\t@type %s: %s%s%s\n' %  (rna_prop_identifier, rna_prop_type,  array_str, readonly_str))
				elif rna_prop_type == 'int' or rna_prop_type == 'float':
					out.write(ident+ '\t@ivar %s: %s\n' %  (rna_prop_identifier, rna_desc))
					out.write(ident+ '\t@type %s: %s%s%s in [%s, %s]\n' %  (rna_prop_identifier, rna_prop_type, array_str, readonly_str, range_str(rna_prop.hard_min), range_str(rna_prop.hard_max) ))
				elif rna_prop_type == 'string':
					out.write(ident+ '\t@ivar %s: %s (maximum length of %s)\n' %  (rna_prop_identifier, rna_desc, rna_prop.max_length))
					out.write(ident+ '\t@type %s: %s%s%s\n' %  (rna_prop_identifier, rna_prop_type, array_str, readonly_str))
				else:
					out.write(ident+ '\t@ivar %s: %s\n' %  (rna_prop_identifier, rna_desc))
					out.write(ident+ '\t@type %s: %s%s%s\n' %  (rna_prop_identifier, rna_prop_type, array_str, readonly_str))
				
			
		out.write(ident+ '\t"""\n\n')
		
		
		# Write functions 
		# for rna_func in rna_struct.functions: # Better ignore inherited (line below)
		for rna_func in rna_functions_dict[identifier]:
			if rna_func not in rna_base_func_keys:
				write_func(rna_func, ident+'\t', out, 'FUNCTION')
		
		out.write('\n')
		
		# Now write children recursively
		for child in rna_children_dict[identifier]:
			write_struct(child, ident + '\t')
	
	
	
	# out = open(target_path, 'w')
	out = open_submodule("types.py", BASEPATH) # bpy.types

	def base_id(rna_struct):
		try:		return rna_struct.base.identifier
		except:	return '' # invalid id

	#structs = [(base_id(rna_struct), rna_struct.identifier, rna_struct) for rna_struct in bpy.doc.structs.values()]
	'''
	structs = []
	for rna_struct in bpy.doc.structs.values():
		structs.append( (base_id(rna_struct), rna_struct.identifier, rna_struct) )
	'''
	structs = []
	for rna_type_name in dir(bpy.types):
		rna_type = getattr(bpy.types, rna_type_name)
		
		try:		rna_struct = rna_type.__rna__
		except:	rna_struct = None
		
		if rna_struct:
			#if not rna_type_name.startswith('__'):
			
			identifier = rna_struct.identifier
			
			if not rna_id_ignore(identifier):
				structs.append( (base_id(rna_struct), identifier, rna_struct) )	
				
				# Simple lookup
				rna_struct_dict[identifier] = rna_struct
				
				# Store full rna path 'GameObjectSettings' -> 'Object.GameObjectSettings'
				rna_full_path_dict[identifier] = full_rna_struct_path(rna_struct)
				
				# Store a list of functions, remove inherited later
				rna_functions_dict[identifier]= list(rna_struct.functions)
				
				
				# fill in these later
				rna_children_dict[identifier]= []
				rna_references_dict[identifier]= []
			
			
		else:
			print("Ignoring", rna_type_name)
	
	
	# Sucks but we need to copy this so we can check original parent functions
	rna_functions_dict__copy = {}
	for key, val in rna_functions_dict.items():
		rna_functions_dict__copy[key] = val[:]
	
	
	structs.sort() # not needed but speeds up sort below, setting items without an inheritance first
	
	# Arrange so classes are always defined in the correct order
	deps_ok = False
	while deps_ok == False:
		deps_ok = True
		rna_done = set()
		
		for i, (rna_base, identifier, rna_struct) in enumerate(structs):
			
			rna_done.add(identifier)
			
			if rna_base and rna_base not in rna_done:
				deps_ok = False
				data = structs.pop(i)
				ok = False
				while i < len(structs):
					if structs[i][1]==rna_base:
						structs.insert(i+1, data) # insert after the item we depend on.
						ok = True
						break
					i+=1
					
				if not ok:
					print('Dependancy "%s" could not be found for "%s"' % (identifier, rna_base))
				
				break
	
	# Done ordering structs
	
	
	# precalc vars to avoid a lot of looping
	for (rna_base, identifier, rna_struct) in structs:
		
		if rna_base:
			rna_base_prop_keys = rna_struct_dict[rna_base].properties.keys() # could cache
			rna_base_func_keys = [f.identifier for f in rna_struct_dict[rna_base].functions]
		else:
			rna_base_prop_keys = []
			rna_base_func_keys= []
		
		# rna_struct_path = full_rna_struct_path(rna_struct)
		rna_struct_path = rna_full_path_dict[identifier]
		
		for rna_prop_identifier, rna_prop in rna_struct.properties.items():
			
			if rna_prop_identifier=='RNA':					continue
			if rna_id_ignore(rna_prop_identifier):			continue
			if rna_prop_identifier in rna_base_prop_keys:	continue
			
			try:		rna_prop_ptr = rna_prop.fixed_type
			except:	rna_prop_ptr = None
			
			# Does this property point to me?
			if rna_prop_ptr:
				rna_references_dict[rna_prop_ptr.identifier].append( "%s.%s" % (rna_struct_path, rna_prop_identifier) )
		
		for rna_func in rna_struct.functions:
			for rna_prop_identifier, rna_prop in rna_func.parameters.items():
				
				if rna_prop_identifier=='RNA':					continue
				if rna_id_ignore(rna_prop_identifier):			continue
				if rna_prop_identifier in rna_base_func_keys:	continue
					
				
				try:		rna_prop_ptr = rna_prop.fixed_type
				except:	rna_prop_ptr = None
				
				# Does this property point to me?
				if rna_prop_ptr:
					rna_references_dict[rna_prop_ptr.identifier].append( "%s.%s" % (rna_struct_path, rna_func.identifier) )
			
		
		# Store nested children
		nested = rna_struct.nested
		if nested:
			rna_children_dict[nested.identifier].append(rna_struct)
		
		
		if rna_base:
			rna_funcs =			rna_functions_dict[identifier]
			if rna_funcs:
				# Remove inherited functions if we have any
				rna_base_funcs =	rna_functions_dict__copy[rna_base]
				rna_funcs[:] =		[f for f in rna_funcs if f not in rna_base_funcs]
	
	rna_functions_dict__copy.clear()
	del rna_functions_dict__copy
	
	# Sort the refs, just reads nicer
	for rna_refs in rna_references_dict.values():
		rna_refs.sort()
	
	for (rna_base, identifier, rna_struct) in structs:
		if rna_struct.nested:
			continue
		
		write_struct(rna_struct, '')
		
		
	out.write('\n')
	out.close()
	
	# # We could also just run....
	# os.system('epydoc source/blender/python/doc/rna.py -o ./source/blender/python/doc/html -v')
	
	target_path = os.path.join(BASEPATH, "dump.py") # XXX - used for other funcs
	
	# Write graphviz
	out= open(target_path.replace('.py', '.dot'), 'w')
	out.write('digraph "rna data api" {\n')
	out.write('\tnode [style=filled, shape = "box"];\n')
	out.write('\toverlap=false;\n')
	out.write('\trankdir = LR;\n')
	out.write('\tsplines=true;\n')
	out.write('\tratio=auto;\n')
	
	
	
	out.write('\tsize="10,10"\n')
	#out.write('\tpage="8.5,11"\n')
	#out.write('\tcenter=""\n')
	
	def isop(rna_struct):
		return '_OT_' in rna_struct.identifier
	
	
	for (rna_base, identifier, rna_struct) in structs:
		if isop(rna_struct):
			continue
		
		base = rna_struct.base
		
		
		out.write('\t"%s";\n' % identifier)
	
	for (rna_base, identifier, rna_struct) in structs:
		
		if isop(rna_struct):
			continue
			
		base = rna_struct.base
		
		if base and not isop(base):
			out.write('\t"%s" -> "%s" [label="(base)" weight=1.0];\n' % (base.identifier, identifier))
		
		nested = rna_struct.nested
		if nested and not isop(nested):
			out.write('\t"%s" -> "%s" [label="(nested)"  weight=1.0];\n' % (nested.identifier, identifier))
		
		
		
		rna_refs= rna_references_dict[identifier]
		
		for rna_ref_string in rna_refs:
			
			if '_OT_' in rna_ref_string:
				continue
			
			ref = rna_ref_string.split('.')[-2]
			out.write('\t"%s" -> "%s" [label="%s" weight=0.01];\n' % (ref, identifier, rna_ref_string))
	
	
	out.write('}\n')
	out.close()
	
	# # We could also just run....
	# os.system('dot source/blender/python/doc/rna.dot -Tsvg -o ./source/blender/python/doc/rna.svg')
	
	
	out= open(target_path.replace('.py', '.words'), 'w')
	rna_words = list(rna_words)
	rna_words.sort()
	for w in rna_words:
		out.write('%s\n' % w)
	

def op2epy(BASEPATH):
	# out = open(target_path, 'w')
	
	op_mods = dir(bpy.ops)
	op_mods.remove('add')
	op_mods.remove('remove')
	
	for op_mod_name in sorted(op_mods):
		if op_mod_name.startswith('__'):
			continue
		
		# open the submodule
		mod_path = os.path.join("ops", op_mod_name + ".py")
		out = open_submodule(mod_path, BASEPATH)


		op_mod = getattr(bpy.ops, op_mod_name)
		operators = dir(op_mod)
		for op in sorted(operators):
			# rna = getattr(bpy.types, op).__rna__
			rna = getattr(op_mod, op).get_rna()
			write_func(rna, '', out, 'OPERATOR')
		
		out.write('\n')
		out.close()

def misc2epy(BASEPATH):
	'''
	Hard coded modules, try to avoid adding stuff here
	'''
	
	f = append_package(os.path.join(BASEPATH, ""), ""); # add a slash on the end of the base path
	f.write('''
"""
@type data: L{bpy.types.Main}
@var  data: blender data is accessed from here
"""
''')

	f = open_submodule("props.py", BASEPATH)
	f.write('''
MAX_INT= 2**31
MAX_FLOAT= 1e+37
def BoolProperty(attr, name="", description="", default=False):
	"""
	return a new bool property
	"""
def IntProperty(attr, name="", description="", min=-MAX_INT, max=MAX_INT, soft_min=-MAX_INT, soft_max=MAX_INT, default=0):
	"""
	return a new int property
	"""
def FloatProperty(attr, name="", description="", min=-MAX_FLOAT, max=MAX_FLOAT, soft_min=-MAX_FLOAT, soft_max=MAX_FLOAT, default=0.0):
	"""
	return a new float property
	"""
def StringProperty(attr, name="", description="", maxlen=0, default=""):
	"""
	return a new string property
	"""
def EnumProperty(attr, items, name="", description="", default=""):
	"""
	return a new enum property
	"""	
''')


if __name__ == '__main__':
	if 'bpy' not in dir():
		print("\nError, this script must run from inside blender2.5")
		print(script_help_msg)
	else:
		misc2epy('source/blender/python/doc/bpy') # first to write in info in some of the modules.
		rna2epy('source/blender/python/doc/bpy')
		op2epy('source/blender/python/doc/bpy')
	
	
	close_all()
	
	import sys
	sys.exit()
