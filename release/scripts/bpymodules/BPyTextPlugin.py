"""The BPyTextPlugin Module

Use get_cached_descriptor(txt) to retrieve information about the script held in
the txt Text object.

Use print_cache_for(txt) to print the information to the console.

Use line, cursor = current_line(txt) to get the logical line and cursor position

Use get_targets(line, cursor) to find out what precedes the cursor:
	aaa.bbb.cc|c.ddd -> ['aaa', 'bbb', 'cc']

Use resolve_targets(txt, targets) to turn a target list into a usable object if
one is found to match.
"""

import bpy, sys, os
import __builtin__, tokenize
from Blender.sys import time
from tokenize import generate_tokens, TokenError, \
		COMMENT, DEDENT, INDENT, NAME, NEWLINE, NL, STRING, NUMBER

class Definition:
	"""Describes a definition or defined object through its name, line number
	and docstring. This is the base class for definition based descriptors.
	"""
	
	def __init__(self, name, lineno, doc=''):
		self.name = name
		self.lineno = lineno
		self.doc = doc

class ScriptDesc:
	"""Describes a script through lists of further descriptor objects (classes,
	defs, vars) and dictionaries to built-in types (imports). If a script has
	not been fully parsed, its incomplete flag will be set. The time of the last
	parse is held by the time field and the name of the text object from which
	it was parsed, the name field.
	"""
	
	def __init__(self, name, imports, classes, defs, vars, incomplete=False):
		self.name = name
		self.imports = imports
		self.classes = classes
		self.defs = defs
		self.vars = vars
		self.incomplete = incomplete
		self.parse_due = 0
	
	def set_delay(self, delay):
		self.parse_due = time() + delay

class ClassDesc(Definition):
	"""Describes a class through lists of further descriptor objects (defs and
	vars). The name of the class is held by the name field and the line on
	which it is defined is held in lineno.
	"""
	
	def __init__(self, name, parents, defs, vars, lineno, doc=''):
		Definition.__init__(self, name, lineno, doc)
		self.parents = parents
		self.defs = defs
		self.vars = vars

class FunctionDesc(Definition):
	"""Describes a function through its name and list of parameters (name,
	params) and the line on which it is defined (lineno).
	"""
	
	def __init__(self, name, params, lineno, doc=''):
		Definition.__init__(self, name, lineno, doc)
		self.params = params

class VarDesc(Definition):
	"""Describes a variable through its name and type (if ascertainable) and the
	line on which it is defined (lineno). If no type can be determined, type
	will equal None.
	"""
	
	def __init__(self, name, type, lineno):
		Definition.__init__(self, name, lineno)
		self.type = type # None for unknown (supports: dict/list/str)

# Context types
CTX_UNSET = -1
CTX_NORMAL = 0
CTX_SINGLE_QUOTE = 1
CTX_DOUBLE_QUOTE = 2
CTX_COMMENT = 3

# Python keywords
KEYWORDS = ['and', 'del', 'from', 'not', 'while', 'as', 'elif', 'global',
			'or', 'with', 'assert', 'else', 'if', 'pass', 'yield',
			'break', 'except', 'import', 'print', 'class', 'exec', 'in',
			'raise', 'continue', 'finally', 'is', 'return', 'def', 'for',
			'lambda', 'try' ]

# Module file extensions
MODULE_EXTS = ['.py', '.pyc', '.pyo', '.pyw', '.pyd']

ModuleType = type(__builtin__)
NoneScriptDesc = ScriptDesc('', dict(), dict(), dict(), dict(), True)

_modules = {}
_modules_updated = 0
_parse_cache = dict()

def _load_module_names():
	"""Searches the sys.path for module files and lists them, along with
	sys.builtin_module_names, in the global dict _modules.
	"""
	
	global _modules
	
	for n in sys.builtin_module_names:
		_modules[n] = None
	for p in sys.path:
		if p == '': p = os.curdir
		if not os.path.isdir(p): continue
		for f in os.listdir(p):
			for ext in MODULE_EXTS:
				if f.endswith(ext):
					_modules[f[:-len(ext)]] = None
					break

_load_module_names()

def _trim_doc(doc):
	"""Trims the quotes from a quoted STRING token (eg. "'''text'''" -> "text")
	"""
	
	l = len(doc)
	i = 0
	while i < l/2 and (doc[i] == "'" or doc[i] == '"'):
		i += 1
	return doc[i:-i]

def resolve_targets(txt, targets):
	"""Attempts to return a useful object for the locally or externally defined
	entity described by targets. If the object is local (defined in txt), a
	Definition instance is returned. If the object is external (imported or
	built in), the object itself is returned. If no object can be found, None is
	returned.
	"""
	
	count = len(targets)
	if count==0: return None
	
	obj = None
	local = None
	i = 1
	
	desc = get_cached_descriptor(txt)
	b = targets[0].find('(')
	if b==-1: b = None # Trick to let us use [:b] and get the whole string
	
	if desc.classes.has_key(targets[0][:b]):
		local = desc.classes[targets[0][:b]]
	elif desc.defs.has_key(targets[0]):
		local = desc.defs[targets[0]]
	elif desc.vars.has_key(targets[0]):
		obj = desc.vars[targets[0]].type
	
	if local:
		while i < count:
			b = targets[i].find('(')
			if b==-1: b = None
			if hasattr(local, 'classes') and local.classes.has_key(targets[i][:b]):
				local = local.classes[targets[i][:b]]
			elif hasattr(local, 'defs') and local.defs.has_key(targets[i]):
				local = local.defs[targets[i]]
			elif hasattr(local, 'vars') and local.vars.has_key(targets[i]):
				obj = local.vars[targets[i]].type
				local = None
				i += 1
				break
			else:
				local = None
				break
			i += 1
	
	if local: return local
	
	if not obj:
		if desc.imports.has_key(targets[0]):
			obj = desc.imports[targets[0]]
		else:
			builtins = get_builtins()
			if builtins.has_key(targets[0]):
				obj = builtins[targets[0]]
	
	while obj and i < count:
		if hasattr(obj, targets[i]):
			obj = getattr(obj, targets[i])
		else:
			obj = None
			break
		i += 1
	
	return obj

def get_cached_descriptor(txt, force_parse=0):
	"""Returns the cached ScriptDesc for the specified Text object 'txt'. If the
	script has not been parsed in the last 'period' seconds it will be reparsed
	to obtain this descriptor.
	
	Specifying TP_AUTO for the period (default) will choose a period based on the
	size of the Text object. Larger texts are parsed less often.
	"""
	
	global _parse_cache
	
	parse = True
	key = hash(txt)
	if not force_parse and _parse_cache.has_key(key):
		desc = _parse_cache[key]
		if desc.parse_due > time():
			parse = desc.incomplete
	
	if parse:
		desc = parse_text(txt)
	
	return desc

def parse_text(txt):
	"""Parses an entire script's text and returns a ScriptDesc instance
	containing information about the script.
	
	If the text is not a valid Python script (for example if brackets are left
	open), parsing may fail to complete. However, if this occurs, no exception
	is thrown. Instead the returned ScriptDesc instance will have its incomplete
	flag set and information processed up to this point will still be accessible.
	"""
	
	start_time = time()
	txt.reset()
	tokens = generate_tokens(txt.readline) # Throws TokenError
	
	curl, cursor = txt.getCursorPos()
	linen = curl + 1 # Token line numbers are one-based
	
	imports = dict()
	imp_step = 0
	
	classes = dict()
	cls_step = 0
	
	defs = dict()
	def_step = 0
	
	vars = dict()
	var1_step = 0
	var2_step = 0
	var3_step = 0
	var_accum = dict()
	var_forflag = False
	
	indent = 0
	prev_type = -1
	prev_text = ''
	incomplete = False
	
	while True:
		try:
			type, text, start, end, line = tokens.next()
		except StopIteration:
			break
		except (TokenError, IndentationError):
			incomplete = True
			break
		
		# Skip all comments and line joining characters
		if type == COMMENT or type == NL:
			continue
		
		#################
		## Indentation ##
		#################
		
		if type == INDENT:
			indent += 1
		elif type == DEDENT:
			indent -= 1
		
		#########################
		## Module importing... ##
		#########################
		
		imp_store = False
		
		# Default, look for 'from' or 'import' to start
		if imp_step == 0:
			if text == 'from':
				imp_tmp = []
				imp_step = 1
			elif text == 'import':
				imp_from = None
				imp_tmp = []
				imp_step = 2
		
		# Found a 'from', create imp_from in form '???.???...'
		elif imp_step == 1:
			if text == 'import':
				imp_from = '.'.join(imp_tmp)
				imp_tmp = []
				imp_step = 2
			elif type == NAME:
				imp_tmp.append(text)
			elif text != '.':
				imp_step = 0 # Invalid syntax
		
		# Found 'import', imp_from is populated or None, create imp_name
		elif imp_step == 2:
			if text == 'as':
				imp_name = '.'.join(imp_tmp)
				imp_step = 3
			elif type == NAME or text == '*':
				imp_tmp.append(text)
			elif text != '.':
				imp_name = '.'.join(imp_tmp)
				imp_symb = imp_name
				imp_store = True
		
		# Found 'as', change imp_symb to this value and go back to step 2
		elif imp_step == 3:
			if type == NAME:
				imp_symb = text
			else:
				imp_store = True
		
		# Both imp_name and imp_symb have now been populated so we can import
		if imp_store:
			
			# Handle special case of 'import *'
			if imp_name == '*':
				parent = get_module(imp_from)
				imports.update(parent.__dict__)
				
			else:
				# Try importing the name as a module
				try:
					if imp_from:
						module = get_module(imp_from +'.'+ imp_name)
					else:
						module = get_module(imp_name)
				except (ImportError, ValueError, AttributeError, TypeError):
					# Try importing name as an attribute of the parent
					try:
						module = __import__(imp_from, globals(), locals(), [imp_name])
						imports[imp_symb] = getattr(module, imp_name)
					except (ImportError, ValueError, AttributeError, TypeError):
						pass
				else:
					imports[imp_symb] = module
			
			# More to import from the same module?
			if text == ',':
				imp_tmp = []
				imp_step = 2
			else:
				imp_step = 0
		
		###################
		## Class parsing ##
		###################
		
		# If we are inside a class then def and variable parsing should be done
		# for the class. Otherwise the definitions are considered global
		
		# Look for 'class'
		if cls_step == 0:
			if text == 'class':
				cls_name = None
				cls_lineno = start[0]
				cls_indent = indent
				cls_step = 1
		
		# Found 'class', look for cls_name followed by '(' parents ')'
		elif cls_step == 1:
			if not cls_name:
				if type == NAME:
					cls_name = text
					cls_sline = False
					cls_parents = dict()
					cls_defs = dict()
					cls_vars = dict()
			elif type == NAME:
				if classes.has_key(text):
					parent = classes[text]
					cls_parents[text] = parent
					cls_defs.update(parent.defs)
					cls_vars.update(parent.vars)
			elif text == ':':
				cls_step = 2
		
		# Found 'class' name ... ':', now check if it's a single line statement
		elif cls_step == 2:
			if type == NEWLINE:
				cls_sline = False
			else:
				cls_sline = True
			cls_doc = ''
			cls_step = 3
		
		elif cls_step == 3:
			if not cls_doc and type == STRING:
				cls_doc = _trim_doc(text)
			if cls_sline:
				if type == NEWLINE:
					classes[cls_name] = ClassDesc(cls_name, cls_parents, cls_defs, cls_vars, cls_lineno, cls_doc)
					cls_step = 0
			else:
				if type == DEDENT and indent <= cls_indent:
					classes[cls_name] = ClassDesc(cls_name, cls_parents, cls_defs, cls_vars, cls_lineno, cls_doc)
					cls_step = 0
		
		#################
		## Def parsing ##
		#################
		
		# Look for 'def'
		if def_step == 0:
			if text == 'def':
				def_name = None
				def_lineno = start[0]
				def_step = 1
		
		# Found 'def', look for def_name followed by '('
		elif def_step == 1:
			if type == NAME:
				def_name = text
				def_params = []
			elif def_name and text == '(':
				def_step = 2
		
		# Found 'def' name '(', now identify the parameters upto ')'
		# TODO: Handle ellipsis '...'
		elif def_step == 2:
			if type == NAME:
				def_params.append(text)
			elif text == ':':
				def_step = 3
		
		# Found 'def' ... ':', now check if it's a single line statement
		elif def_step == 3:
			if type == NEWLINE:
				def_sline = False
			else:
				def_sline = True
			def_doc = ''
			def_step = 4
		
		elif def_step == 4:
			if type == STRING:
				def_doc = _trim_doc(text)
			newdef = None
			if def_sline:
				if type == NEWLINE:
					newdef = FunctionDesc(def_name, def_params, def_lineno, def_doc)
			else:
				if type == NAME:
					newdef = FunctionDesc(def_name, def_params, def_lineno, def_doc)
			if newdef:
				if cls_step > 0: # Parsing a class
					cls_defs[def_name] = newdef
				else:
					defs[def_name] = newdef
				def_step = 0
		
		##########################
		## Variable assignation ##
		##########################
		
		if cls_step > 0: # Parsing a class
			# Look for 'self.???'
			if var1_step == 0:
				if text == 'self':
					var1_step = 1
			elif var1_step == 1:
				if text == '.':
					var_name = None
					var1_step = 2
				else:
					var1_step = 0
			elif var1_step == 2:
				if type == NAME:
					var_name = text
					if cls_vars.has_key(var_name):
						var_step = 0
					else:
						var1_step = 3
			elif var1_step == 3:
				if text == '=':
					var1_step = 4
				elif text != ',':
					var1_step = 0
			elif var1_step == 4:
				var_type = None
				if type == NUMBER:
					close = end[1]
					if text.find('.') != -1: var_type = float
					else: var_type = int
				elif type == STRING:
					close = end[1]
					var_type = str
				elif text == '[':
					close = line.find(']', end[1])
					var_type = list
				elif text == '(':
					close = line.find(')', end[1])
					var_type = tuple
				elif text == '{':
					close = line.find('}', end[1])
					var_type = dict
				elif text == 'dict':
					close = line.find(')', end[1])
					var_type = dict
				if var_type and close+1 < len(line):
					if line[close+1] != ' ' and line[close+1] != '\t':
						var_type = None
				cls_vars[var_name] = VarDesc(var_name, var_type, start[0])
				var1_step = 0
		
		elif def_step > 0: # Parsing a def
			# Look for 'global ???[,???]'
			if var2_step == 0:
				if text == 'global':
					var2_step = 1
			elif var2_step == 1:
				if type == NAME:
					if not vars.has_key(text):
						vars[text] = VarDesc(text, None, start[0])
				elif text != ',' and type != NL:
					var2_step == 0
		
		else: # In global scope
			if var3_step == 0:
				# Look for names
				if text == 'for':
					var_accum = dict()
					var_forflag = True
				elif text == '=' or (var_forflag and text == 'in'):
					var_forflag = False
					var3_step = 1
				elif type == NAME:
					if prev_text != '.' and not vars.has_key(text):
						var_accum[text] = VarDesc(text, None, start[0])
				elif not text in [',', '(', ')', '[', ']']:
					var_accum = dict()
					var_forflag = False
			elif var3_step == 1:
				if len(var_accum) != 1:
					var_type = None
					vars.update(var_accum)
				else:
					var_name = var_accum.keys()[0]
					var_type = None
					if type == NUMBER:
						if text.find('.') != -1: var_type = float
						else: var_type = int
					elif type == STRING: var_type = str
					elif text == '[': var_type = list
					elif text == '(': var_type = tuple
					elif text == '{': var_type = dict
					vars[var_name] = VarDesc(var_name, var_type, start[0])
				var3_step = 0
		
		#######################
		## General utilities ##
		#######################
		
		prev_type = type
		prev_text = text
	
	desc = ScriptDesc(txt.name, imports, classes, defs, vars, incomplete)
	desc.set_delay(10 * (time()-start_time) + 0.05)
	
	global _parse_cache
	_parse_cache[hash(txt)] = desc
	return desc

def get_modules(since=1):
	"""Returns the set of built-in modules and any modules that have been
	imported into the system upto 'since' seconds ago.
	"""
	
	global _modules, _modules_updated
	
	t = time()
	if _modules_updated < t - since:
		_modules.update(sys.modules)
		_modules_updated = t
	return _modules.keys()

def suggest_cmp(x, y):
	"""Use this method when sorting a list of suggestions.
	"""
	
	return cmp(x[0].upper(), y[0].upper())

def get_module(name):
	"""Returns the module specified by its name. The module itself is imported
	by this method and, as such, any initialization code will be executed.
	"""
	
	mod = __import__(name)
	components = name.split('.')
	for comp in components[1:]:
		mod = getattr(mod, comp)
	return mod

def type_char(v):
	"""Returns the character used to signify the type of a variable. Use this
	method to identify the type character for an item in a suggestion list.
	
	The following values are returned:
	  'm' if the parameter is a module
	  'f' if the parameter is callable
	  'v' if the parameter is variable or otherwise indeterminable
	
	"""
	
	if isinstance(v, ModuleType):
		return 'm'
	elif callable(v):
		return 'f'
	else: 
		return 'v'

def get_context(txt):
	"""Establishes the context of the cursor in the given Blender Text object
	
	Returns one of:
	  CTX_NORMAL - Cursor is in a normal context
	  CTX_SINGLE_QUOTE - Cursor is inside a single quoted string
	  CTX_DOUBLE_QUOTE - Cursor is inside a double quoted string
	  CTX_COMMENT - Cursor is inside a comment
	
	"""
	
	l, cursor = txt.getCursorPos()
	lines = txt.asLines(0, l+1)
	
	# FIXME: This method is too slow in large files for it to be called as often
	# as it is. So for lines below the 1000th line we do this... (quorn)
	if l > 1000: return CTX_NORMAL
	
	# Detect context (in string or comment)
	in_str = CTX_NORMAL
	for line in lines:
		if l == 0:
			end = cursor
		else:
			end = len(line)
			l -= 1
		
		# Comments end at new lines
		if in_str == CTX_COMMENT:
			in_str = CTX_NORMAL
		
		for i in range(end):
			if in_str == 0:
				if line[i] == "'": in_str = CTX_SINGLE_QUOTE
				elif line[i] == '"': in_str = CTX_DOUBLE_QUOTE
				elif line[i] == '#': in_str = CTX_COMMENT
			else:
				if in_str == CTX_SINGLE_QUOTE:
					if line[i] == "'":
						in_str = CTX_NORMAL
						# In again if ' escaped, out again if \ escaped, and so on
						for a in range(i-1, -1, -1):
							if line[a] == '\\': in_str = 1-in_str
							else: break
				elif in_str == CTX_DOUBLE_QUOTE:
					if line[i] == '"':
						in_str = CTX_NORMAL
						# In again if " escaped, out again if \ escaped, and so on
						for a in range(i-1, -1, -1):
							if line[i-a] == '\\': in_str = 2-in_str
							else: break
		
	return in_str

def current_line(txt):
	"""Extracts the Python script line at the cursor in the Blender Text object
	provided and cursor position within this line as the tuple pair (line,
	cursor).
	"""
	
	lineindex, cursor = txt.getCursorPos()
	lines = txt.asLines()
	line = lines[lineindex]
	
	# Join previous lines to this line if spanning
	i = lineindex - 1
	while i > 0:
		earlier = lines[i].rstrip()
		if earlier.endswith('\\'):
			line = earlier[:-1] + ' ' + line
			cursor += len(earlier)
		i -= 1
	
	# Join later lines while there is an explicit joining character
	i = lineindex
	while i < len(lines)-1 and lines[i].rstrip().endswith('\\'):
		later = lines[i+1].strip()
		line = line + ' ' + later[:-1]
		i += 1
	
	return line, cursor

def get_targets(line, cursor):
	"""Parses a period separated string of valid names preceding the cursor and
	returns them as a list in the same order.
	"""
	
	brk = 0
	targets = []
	j = cursor
	i = j-1
	while i >= 0:
		if line[i] == ')': brk += 1
		elif brk:
			if line[i] == '(': brk -= 1
		else:
			if line[i] == '.':
				targets.insert(0, line[i+1:j]); j=i
			elif not (line[i].isalnum() or line[i] == '_' or line[i] == '.'):
				break
		i -= 1
	targets.insert(0, line[i+1:j])
	return targets

def get_defs(txt):
	"""Returns a dictionary which maps definition names in the source code to
	a list of their parameter names.
	
	The line 'def doit(one, two, three): print one' for example, results in the
	mapping 'doit' : [ 'one', 'two', 'three' ]
	"""
	
	return get_cached_descriptor(txt).defs

def get_vars(txt):
	"""Returns a dictionary of variable names found in the specified Text
	object. This method locates all names followed directly by an equal sign:
	'a = ???' or indirectly as part of a tuple/list assignment or inside a
	'for ??? in ???:' block.
	"""
	
	return get_cached_descriptor(txt).vars

def get_imports(txt):
	"""Returns a dictionary which maps symbol names in the source code to their
	respective modules.
	
	The line 'from Blender import Text as BText' for example, results in the
	mapping 'BText' : <module 'Blender.Text' (built-in)>
	
	Note that this method imports the modules to provide this mapping as as such
	will execute any initilization code found within.
	"""
	
	return get_cached_descriptor(txt).imports

def get_builtins():
	"""Returns a dictionary of built-in modules, functions and variables."""
	
	return __builtin__.__dict__


#################################
## Debugging utility functions ##
#################################

def print_cache_for(txt, period=sys.maxint):
	"""Prints out the data cached for a given Text object. If no period is
	given the text will not be reparsed and the cached version will be returned.
	Otherwise if the period has expired the text will be reparsed.
	"""
	
	desc = get_cached_descriptor(txt, period)
	print '================================================'
	print 'Name:', desc.name, '('+str(hash(txt))+')'
	print '------------------------------------------------'
	print 'Defs:'
	for name, ddesc in desc.defs.items():
		print ' ', name, ddesc.params, ddesc.lineno
		print '   ', ddesc.doc
	print '------------------------------------------------'
	print 'Vars:'
	for name, vdesc in desc.vars.items():
		print ' ', name, vdesc.type, vdesc.lineno
	print '------------------------------------------------'
	print 'Imports:'
	for name, item in desc.imports.items():
		print ' ', name.ljust(15), item
	print '------------------------------------------------'
	print 'Classes:'
	for clsnme, clsdsc in desc.classes.items():
		print '  *********************************'
		print '  Name:', clsnme
		print ' ', clsdsc.doc
		print '  ---------------------------------'
		print '  Defs:'
		for name, ddesc in clsdsc.defs.items():
			print '   ', name, ddesc.params, ddesc.lineno
			print '     ', ddesc.doc
		print '  ---------------------------------'
		print '  Vars:'
		for name, vdesc in clsdsc.vars.items():
			print '   ', name, vdesc.type, vdesc.lineno
		print '  *********************************'
	print '================================================'
