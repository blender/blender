import bpy, sys
import __builtin__, tokenize
from Blender.sys import time
from tokenize import generate_tokens, TokenError

# TODO: Remove the dependency for a full Python installation.

# Context types
NORMAL = 0
SINGLE_QUOTE = 1
DOUBLE_QUOTE = 2
COMMENT = 3

# Python keywords
KEYWORDS = ['and', 'del', 'from', 'not', 'while', 'as', 'elif', 'global',
			'or', 'with', 'assert', 'else', 'if', 'pass', 'yield',
			'break', 'except', 'import', 'print', 'class', 'exec', 'in',
			'raise', 'continue', 'finally', 'is', 'return', 'def', 'for',
			'lambda', 'try' ]

# Used to cache the return value of generate_tokens
_token_cache = None
_cache_update = 0

ModuleType = type(__builtin__)
_modules = dict([(n, None) for n in sys.builtin_module_names])
_modules_updated = 0

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

def cached_generate_tokens(txt, since=1):
	"""A caching version of generate tokens for multiple parsing of the same
	document within a given timescale.
	"""
	
	global _token_cache, _cache_update
	
	t = time()
	if _cache_update < t - since:
		txt.reset()
		_token_cache = [g for g in generate_tokens(txt.readline)]
		_cache_update = t
	return _token_cache

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
	  NORMAL - Cursor is in a normal context
	  SINGLE_QUOTE - Cursor is inside a single quoted string
	  DOUBLE_QUOTE - Cursor is inside a double quoted string
	  COMMENT - Cursor is inside a comment
	
	"""
	
	l, cursor = txt.getCursorPos()
	lines = txt.asLines()[:l+1]
	
	# Detect context (in string or comment)
	in_str = 0			# 1-single quotes, 2-double quotes
	for line in lines:
		if l == 0:
			end = cursor
		else:
			end = len(line)
			l -= 1
		
		# Comments end at new lines
		if in_str == 3:
			in_str = 0
		
		for i in range(end):
			if in_str == 0:
				if line[i] == "'": in_str = 1
				elif line[i] == '"': in_str = 2
				elif line[i] == '#': in_str = 3
			else:
				if in_str == 1:
					if line[i] == "'":
						in_str = 0
						# In again if ' escaped, out again if \ escaped, and so on
						for a in range(i-1, -1, -1):
							if line[a] == '\\': in_str = 1-in_str
							else: break
				elif in_str == 2:
					if line[i] == '"':
						in_str = 0
						# In again if " escaped, out again if \ escaped, and so on
						for a in range(i-1, -1, -1):
							if line[i-a] == '\\': in_str = 2-in_str
							else: break
		
	return in_str

def current_line(txt):
	"""Extracts the Python script line at the cursor in the Blender Text object
	provided and cursor position within this line as the tuple pair (line,
	cursor)"""
	
	(lineindex, cursor) = txt.getCursorPos()
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
	returns them as a list in the same order."""
	
	targets = []
	i = cursor - 1
	while i >= 0 and (line[i].isalnum() or line[i] == '_' or line[i] == '.'):
		i -= 1
	
	pre = line[i+1:cursor]
	return pre.split('.')

def get_imports(txt):
	"""Returns a dictionary which maps symbol names in the source code to their
	respective modules.
	
	The line 'from Blender import Text as BText' for example, results in the
	mapping 'BText' : <module 'Blender.Text' (built-in)>
	
	Note that this method imports the modules to provide this mapping as as such
	will execute any initilization code found within.
	"""
	
	# Unfortunately, generate_tokens may fail if the script leaves brackets or
	# strings open or there are other syntax errors. For now we return an empty
	# dictionary until an alternative parse method is implemented.
	try:
		tokens = cached_generate_tokens(txt)
	except TokenError:
		return dict()
	
	imports = dict()
	step = 0
	
	for type, string, start, end, line in tokens:
		store = False
		
		# Default, look for 'from' or 'import' to start
		if step == 0:
			if string == 'from':
				tmp = []
				step = 1
			elif string == 'import':
				fromname = None
				tmp = []
				step = 2
		
		# Found a 'from', create fromname in form '???.???...'
		elif step == 1:
			if string == 'import':
				fromname = '.'.join(tmp)
				tmp = []
				step = 2
			elif type == tokenize.NAME:
				tmp.append(string)
			elif string != '.':
				step = 0 # Invalid syntax
		
		# Found 'import', fromname is populated or None, create impname
		elif step == 2:
			if string == 'as':
				impname = '.'.join(tmp)
				step = 3
			elif type == tokenize.NAME or string == '*':
				tmp.append(string)
			elif string != '.':
				impname = '.'.join(tmp)
				symbol = impname
				store = True
		
		# Found 'as', change symbol to this value and go back to step 2
		elif step == 3:
			if type == tokenize.NAME:
				symbol = string
			else:
				store = True
		
		# Both impname and symbol have now been populated so we can import
		if store:
			
			# Handle special case of 'import *'
			if impname == '*':
				parent = get_module(fromname)
				imports.update(parent.__dict__)
				
			else:
				# Try importing the name as a module
				try:
					if fromname:
						module = get_module(fromname +'.'+ impname)
					else:
						module = get_module(impname)
					imports[symbol] = module
				except (ImportError, ValueError, AttributeError, TypeError):
					# Try importing name as an attribute of the parent
					try:
						module = __import__(fromname, globals(), locals(), [impname])
						imports[symbol] = getattr(module, impname)
					except (ImportError, ValueError, AttributeError, TypeError):
						pass
			
			# More to import from the same module?
			if string == ',':
				tmp = []
				step = 2
			else:
				step = 0
	
	return imports

def get_builtins():
	"""Returns a dictionary of built-in modules, functions and variables."""
	
	return __builtin__.__dict__

def get_defs(txt):
	"""Returns a dictionary which maps definition names in the source code to
	a list of their parameter names.
	
	The line 'def doit(one, two, three): print one' for example, results in the
	mapping 'doit' : [ 'one', 'two', 'three' ]
	"""
	
	# See above for problems with generate_tokens
	try:
		tokens = cached_generate_tokens(txt)
	except TokenError:
		return dict()
	
	defs = dict()
	step = 0
	
	for type, string, start, end, line in tokens:
		
		# Look for 'def'
		if step == 0:
			if string == 'def':
				name = None
				step = 1
		
		# Found 'def', look for name followed by '('
		elif step == 1:
			if type == tokenize.NAME:
				name = string
				params = []
			elif name and string == '(':
				step = 2
		
		# Found 'def' name '(', now identify the parameters upto ')'
		# TODO: Handle ellipsis '...'
		elif step == 2:
			if type == tokenize.NAME:
				params.append(string)
			elif string == ')':
				defs[name] = params
				step = 0
		
	return defs

def get_vars(txt):
	"""Returns a dictionary of variable names found in the specified Text
	object. This method locates all names followed directly by an equal sign:
	'a = ???' or indirectly as part of a tuple/list assignment or inside a
	'for ??? in ???:' block.
	"""
	
	# See above for problems with generate_tokens
	try:
		tokens = cached_generate_tokens(txt)
	except TokenError:
		return []
	
	vars = []
	accum = [] # Used for tuple/list assignment
	foring = False
	
	for type, string, start, end, line in tokens:
		
		# Look for names
		if string == 'for':
			foring = True
		if string == '=' or (foring and string == 'in'):
			vars.extend(accum)
			accum = []
			foring = False
		elif type == tokenize.NAME:
			accum.append(string)
		elif not string in [',', '(', ')', '[', ']']:
			accum = []
			foring = False
		
	return vars
