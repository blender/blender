#!BPY
"""
Name: 'Suggest'
Blender: 243
Group: 'TextPlugin'
Tooltip: 'Suggests completions for the word at the cursor in a python script'
"""

import bpy, __builtin__, token
from Blender  import Text
from StringIO import StringIO
from inspect  import *
from tokenize import generate_tokens

TK_TYPE  = 0
TK_TOKEN = 1
TK_START = 2 #(srow, scol)
TK_END   = 3 #(erow, ecol)
TK_LINE  = 4
TK_ROW = 0
TK_COL = 1

execs = [] # Used to establish the same import context across defs

keywords = ['and', 'del', 'from', 'not', 'while', 'as', 'elif', 'global',
			'or', 'with', 'assert', 'else', 'if', 'pass', 'yield',
			'break', 'except', 'import', 'print', 'class', 'exec', 'in',
			'raise', 'continue', 'finally', 'is', 'return', 'def', 'for',
			'lambda', 'try' ]


def getBuiltins():
	builtins = []
	bi = dir(__builtin__)
	for k in bi:
		v = getattr(__builtin__, k)
		if ismodule(v): t='m'
		elif callable(v): t='f'
		else: t='v'
		builtins.append((k, t))
	return builtins


def getKeywords():
	global keywords
	return [(k, 'k') for k in keywords]


def getTokens(txt):
	txt.reset()
	g = generate_tokens(txt.readline)
	tokens = []
	for t in g: tokens.append(t)
	return tokens


def isNameChar(s):
	return (s.isalnum() or s == '_')


# Returns words preceding the cursor that are separated by periods as a list in
# the same order
def getCompletionSymbols(txt):
	(l, c)= txt.getCursorPos()
	lines = txt.asLines()
	line = lines[l]
	a=0
	for a in range(1, c+1):
		if not isNameChar(line[c-a]) and line[c-a]!='.':
			a -= 1
			break
	return line[c-a:c].split('.')


# Returns a list of tuples of symbol names and their types (name, type) where
# type is one of:
#   m (module/class)  Has its own members (includes classes)
#   v (variable)      Has a type which may have its own members
#   f (function)      Callable and may have a return type (with its own members)
# It also updates the global import context (via execs)
def getGlobals(txt):
	global execs
	
	# Unfortunately, tokenize may fail if the script leaves brackets or strings
	# open. For now we return an empty list, leaving builtins and keywords as
	# the only globals. (on the TODO list)
	try:
		tokens = getTokens(txt)
	except:
		return []
	
	globals = dict()
	for i in range(len(tokens)):
		
		# Handle all import statements
		if i>=1 and tokens[i-1][TK_TOKEN]=='import':
			
			# Find 'from' if it exists
			fr= -1
			for a in range(1, i):
				if tokens[i-a][TK_TYPE]==token.NEWLINE: break
				if tokens[i-a][TK_TOKEN]=='from':
					fr=i-a
					break
			
			# Handle: import ___[,___]
			if fr<0:
				
				while True:
					if tokens[i][TK_TYPE]==token.NAME:
						# Add the import to the execs list
						x = tokens[i][TK_LINE].strip()
						k = tokens[i][TK_TOKEN]
						execs.append(x)
						exec 'try: '+x+'\nexcept: pass'
						
						# Add the symbol name to the return list
						globals[k] = 'm'
					elif tokens[i][TK_TOKEN]!=',':
						break
					i += 1
			
			# Handle statement: from ___[.___] import ___[,___]
			else: # fr>=0:
				
				# Add the import to the execs list
				x = tokens[i][TK_LINE].strip()
				execs.append(x)
				exec 'try: '+x+'\nexcept: pass'
				
				# Import parent module so we can process it for sub modules
				parent = ''.join([t[TK_TOKEN] for t in tokens[fr+1:i-1]])
				exec 'try: import '+parent+'\nexcept: pass'
				
				# All submodules, functions, etc.
				if tokens[i][TK_TOKEN]=='*':
					
					# Add each symbol name to the return list
					d = eval(parent).__dict__.items()
					for k,v in d:
						if not globals.has_key(k) or not globals[k]:
							t='v'
							if ismodule(v): t='m'
							elif callable(v): t='f'
							globals[k] = t
				
				# Specific function, submodule, etc.
				else:
					while True:
						if tokens[i][TK_TYPE]==token.NAME:
							k = tokens[i][TK_TOKEN]
							if not globals.has_key(k) or not globals[k]:
								t='v'
								try:
									v = eval(parent+'.'+k)
									if ismodule(v): t='m'
									elif callable(v): t='f'
								except: pass
								globals[k] = t
						elif tokens[i][TK_TOKEN]!=',':
							break
						i += 1
					
		elif tokens[i][TK_TYPE]==token.NAME and tokens[i][TK_TOKEN] not in keywords and (i==0 or tokens[i-1][TK_TOKEN]!='.'):
			k = tokens[i][TK_TOKEN]
			if not globals.has_key(k) or not globals[k]:
				t=None
				if (i>0 and tokens[i-1][TK_TOKEN]=='def'):
					t='f'
				else:
					t='v'
				globals[k] = t
	
	return globals.items()


def globalSuggest(txt, cs):
	globals = getGlobals(txt)
	return globals


# Only works for 'static' members (eg. Text.Get)
def memberSuggest(txt, cs):
	global execs
	
	# Populate the execs for imports
	getGlobals(txt)
	
	# Sometimes we have conditional includes which will fail if the module
	# cannot be found. So we protect outselves in a try block
	for x in execs:
		exec 'try: '+x+'\nexcept: pass'
	
	suggestions = dict()
	(row, col) = txt.getCursorPos()
	
	sub = cs[len(cs)-1]
	
	m=None
	pre='.'.join(cs[:-1])
	try:
		m = eval(pre)
	except:
		print pre+ ' not found or not imported.'
	
	if m!=None:
		for k,v in m.__dict__.items():
			if ismodule(v): t='m'
			elif callable(v): t='f'
			else: t='v'
			suggestions[k] = t
	
	return suggestions.items()


def cmp0(x, y):
	return cmp(x[0], y[0])


def main():
	txt = bpy.data.texts.active
	if txt==None: return
	
	cs = getCompletionSymbols(txt)
	
	if len(cs)<=1:
		l = globalSuggest(txt, cs)
		l.extend(getBuiltins())
		l.extend(getKeywords())
	else:
		l = memberSuggest(txt, cs)
	
	l.sort(cmp=cmp0)
	txt.suggest(l, cs[len(cs)-1])

main()
