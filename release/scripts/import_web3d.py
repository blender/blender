#!BPY
"""
Name: 'X3D & VRML97 (.x3d / wrl)...'
Blender: 248
Group: 'Import'
Tooltip: 'Load a VRML97 File'
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# (C) Copyright 2008 Paravizion
# Written by Campbell Barton aka Ideasman42
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
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

__author__ = "Campbell Barton"
__url__ = ['www.blender.org', 'blenderartists.org', 'http://wiki.blender.org/index.php/Scripts/Manual/Import/X3D_VRML97']
__version__ = "0.1"

__bpydoc__ = """\
This script is an importer for the X3D and VRML97 file formats.
"""

def vrmlFormat(data):
	'''
	Keep this as a valid vrml file, but format in a way we can pradict.
	'''
	# Strip all commends - # not in strings - warning multiline strings are ignored.
	def strip_comment(l):
		#l = ' '.join(l.split())
		l = l.strip()
		
		if l.startswith('#'):
			return ''
		
		i = l.find('#')
		
		if i==-1:
			return l
		
		# Most cases accounted for! if we have a comment at the end of the line do this...
		j = l.find('"')
		
		if j == -1: # simple no strings
			return l[:i].strip()
		
		q = False
		for i,c in enumerate(l):
			if c == '"':
				q = not q # invert
			
			elif c == '#':
				if q==False:
					return l[:i-1]
		
		return l
	
	data = '\n'.join([strip_comment(l) for l in data.split('\n') ]) # remove all whitespace
	
		
	# Bad, dont take strings into account
	'''
	data = data.replace('#', '\n#')
	data = '\n'.join([ll for l in data.split('\n') for ll in (l.strip(),) if not ll.startswith('#')]) # remove all whitespace
	'''
	data = data.replace('{', '\n{\n')
	data = data.replace('}', '\n}\n')
	data = data.replace('[', '\n[\n')
	data = data.replace(']', '\n]\n')
	data = data.replace(',', ' , ') # make sure comma's seperate
	
	# More annoying obscure cases where USE or DEF are placed on a newline
	# data = data.replace('\nDEF ', ' DEF ')
	# data = data.replace('\nUSE ', ' USE ')
	
	data = '\n'.join([' '.join(l.split()) for l in data.split('\n')]) # remove all whitespace
	
	# Better to parse the file accounting for multiline arrays
	'''
	data = data.replace(',\n', ' , ') # remove line endings with commas
	data = data.replace(']', '\n]\n') # very very annoying - but some comma's are at the end of the list, must run this again.
	'''
	
	return [l for l in data.split('\n') if l]


# This should work without a blender at all
try:
	from Blender.sys import exists
except:
	from os.path import exists

def baseName(path):
	return path.split('/')[-1].split('\\')[-1]

def dirName(path):
	return path[:-len(baseName(path))]

# notes
# transform are relative 
# order dosnt matter for loc/size/rot
# right handed rotation
# angles are in radians
# rotation first defines axis then ammount in deg



# =============================== VRML Spesific

NODE_NORMAL = 1 # {}
NODE_ARRAY = 2 # []
NODE_REFERENCE = 3 # USE foobar

lines = []

def getNodePreText(i, words):
	# print lines[i]
	use_node = False
	while len(words) < 5:
		
		if i>=len(lines):
			break
		elif lines[i]=='{':
			# words.append(lines[i]) # no need
			# print "OK"
			return NODE_NORMAL, i+1
		elif lines[i].count('"') % 2 != 0: # odd number of quotes? - part of a string.
			# print 'ISSTRING'
			break
		else:
			new_words = lines[i].split()
			if 'USE' in new_words:
				use_node = True
			
			words.extend(new_words)
			i += 1
		
		# Check for USE node - no {
		# USE #id - should always be on the same line.
		if use_node:
			# print 'LINE', i, words[:words.index('USE')+2]
			words[:] = words[:words.index('USE')+2]
			if lines[i] == '{' and lines[i+1] == '}':
				# USE sometimes has {} after it anyway
				i+=2 
			return NODE_REFERENCE, i
		
	# print "error value!!!", words
	return 0, -1

def is_nodeline(i, words):
	
	if not lines[i][0].isalpha():
		return 0, 0
	
	# Simple "var [" type
	if lines[i+1] == '[':
		if lines[i].count('"') % 2 == 0:
			words[:] = lines[i].split()
			return NODE_ARRAY, i+2
	
	node_type, new_i = getNodePreText(i, words)
	
	if not node_type:
		return 0, 0
	
	# Ok, we have a { after some values
	# Check the values are not fields
	for i, val in enumerate(words):
		if i != 0 and words[i-1] in ('DEF', 'USE'):
			# ignore anything after DEF, it is a ID and can contain any chars.
			pass
		elif val[0].isalpha() and val not in ('TRUE', 'FALSE'):
			pass
		else:
			# There is a number in one of the values, therefor we are not a node.
			return 0, 0
	
	#if node_type==NODE_REFERENCE:
	#	print words, "REF_!!!!!!!"
	return node_type, new_i

def is_numline(i):
	'''
	Does this line start with a number?
	'''
	l = lines[i]
	line_end = len(l)-1
	line_end_new = l.find(' ') # comma's always have a space before them
	
	if line_end_new != -1:
		line_end = line_end_new
	
	try:
		float(l[:line_end]) # works for a float or int
		return True
	except:
		return False

class vrmlNode(object):
	__slots__ = 'id', 'fields', 'node_type', 'parent', 'children', 'parent', 'array_data', 'reference', 'lineno', 'filename', 'blendObject', 'DEF_NAMESPACE', 'FIELD_NAMESPACE', 'x3dNode'
	def __init__(self, parent, node_type, lineno):
		self.id = None
		self.node_type = node_type
		self.parent = parent
		self.blendObject = None
		self.x3dNode = None # for x3d import only
		if parent:
			parent.children.append(self)
		
		self.lineno = lineno
		
		# This is only set from the root nodes.
		# Having a filename also denotes a root node
		self.filename = None
		
		# Store in the root node because each inline file needs its own root node and its own namespace
		self.DEF_NAMESPACE = None 
		self.FIELD_NAMESPACE = None
		
		self.reference = None
		
		if node_type==NODE_REFERENCE:
			# For references, only the parent and ID are needed
			# the reference its self is assigned on parsing
			return 
		
		self.fields = [] # fields have no order, in some cases rool level values are not unique so dont use a dict
		self.children = []
		self.array_data = [] # use for arrays of data - should only be for NODE_ARRAY types
		
	
	# Only available from the root node
	def getFieldDict(self):
		if self.FIELD_NAMESPACE != None:
			return self.FIELD_NAMESPACE
		else:
			return self.parent.getFieldDict()
	
	def getDefDict(self):
		if self.DEF_NAMESPACE != None:
			return self.DEF_NAMESPACE
		else:
			return self.parent.getDefDict()
	
	def setRoot(self, filename):
		self.filename = filename
		self.FIELD_NAMESPACE =	{}
		self.DEF_NAMESPACE=		{}
		
	def getFilename(self):
		if self.filename:
			return self.filename
		elif self.parent:
			return self.parent.getFilename()
		else:
			return None
	
	def getRealNode(self):
		if self.reference:
			return self.reference
		else:
			return self
	
	def getSpec(self):
		self_real = self.getRealNode()
		try:
			return self_real.id[-1] # its possible this node has no spec
		except:
			return None
	
	def getDefName(self):
		self_real = self.getRealNode()
		
		if 'DEF' in self_real.id:
			# print self_real.id
			return self_real.id[ list(self_real.id).index('DEF')+1 ]
		else:
			return None
		
	
	def getChildrenBySpec(self, node_spec): # spec could be Transform, Shape, Appearance
		self_real = self.getRealNode()
		# using getSpec functions allows us to use the spec of USE children that dont have their spec in their ID
		if type(node_spec) == str:
			return [child for child in self_real.children if child.getSpec()==node_spec]
		else:
			# Check inside a list of optional types
			return [child for child in self_real.children if child.getSpec() in node_spec]
	
	def getChildBySpec(self, node_spec): # spec could be Transform, Shape, Appearance
		# Use in cases where there is only ever 1 child of this type
		ls = self.getChildrenBySpec(node_spec)
		if ls: return ls[0]
		else: return None
	
	def getChildrenByName(self, node_name): # type could be geometry, children, appearance
		self_real = self.getRealNode()
		return [child for child in self_real.children if child.id if child.id[0]==node_name]
	
	def getChildByName(self, node_name):
		self_real = self.getRealNode()
		for child in self_real.children:
			if child.id and child.id[0]==node_name: # and child.id[-1]==node_spec:
				return child
	
	def getSerialized(self, results, ancestry):
		'''	Return this node and all its children in a flat list '''
		ancestry = ancestry[:] # always use a copy
		
		# self_real = self.getRealNode()
		
		results.append((self, tuple(ancestry)))
		ancestry.append(self)
		for child in self.getRealNode().children:
			if child not in ancestry:
				child.getSerialized(results, ancestry)
		
		return results
		
	def searchNodeTypeID(self, node_spec, results):
		self_real = self.getRealNode()
		# print self.lineno, self.id
		if self_real.id and self_real.id[-1]==node_spec: # use last element, could also be only element
			results.append(self_real)
		for child in self_real.children:
			child.searchNodeTypeID(node_spec, results)
		return results
	
	def getFieldName(self, field):
		self_real = self.getRealNode() # incase we're an instance
		
		for f in self_real.fields:
			# print f
			if f and f[0] == field:
				# print '\tfound field', f
				
				return f[1:]
		# print '\tfield not found', field
		return None
	
	def getFieldAsInt(self, field, default):
		self_real = self.getRealNode() # incase we're an instance
		
		f = self_real.getFieldName(field)
		if f==None:	return default
		if ',' in f: f = f[:f.index(',')] # strip after the comma
		
		if len(f) != 1:
			print '\t"%s" wrong length for int conversion for field "%s"' % (f, field)
			return default
		
		try:
			return int(f[0])
		except:
			print '\tvalue "%s" could not be used as an int for field "%s"' % (f[0], field)
			return default
	
	def getFieldAsFloat(self, field, default):
		self_real = self.getRealNode() # incase we're an instance
		
		f = self_real.getFieldName(field)
		if f==None:	return default
		if ',' in f: f = f[:f.index(',')] # strip after the comma
		
		if len(f) != 1:
			print '\t"%s" wrong length for float conversion for field "%s"' % (f, field)
			return default
		
		try:
			return float(f[0])
		except:
			print '\tvalue "%s" could not be used as a float for field "%s"' % (f[0], field)
			return default
	
	def getFieldAsFloatTuple(self, field, default):
		self_real = self.getRealNode() # incase we're an instance
		
		f = self_real.getFieldName(field)
		if f==None:	return default
		# if ',' in f: f = f[:f.index(',')] # strip after the comma
		
		if len(f) < 1:
			print '"%s" wrong length for float tuple conversion for field "%s"' % (f, field)
			return default
		
		ret = []
		for v in f:
			if v != ',':
				try:		ret.append(float(v))
				except:		break # quit of first non float, perhaps its a new field name on the same line? - if so we are going to ignore it :/ TODO
		# print ret
		
		if ret:
			return ret
		if not ret:
			print '\tvalue "%s" could not be used as a float tuple for field "%s"' % (f, field)
			return default
	
	def getFieldAsBool(self, field, default):
		self_real = self.getRealNode() # incase we're an instance
		
		f = self_real.getFieldName(field)
		if f==None:	return default
		if ',' in f: f = f[:f.index(',')] # strip after the comma
		
		if len(f) != 1:
			print '\t"%s" wrong length for bool conversion for field "%s"' % (f, field)
			return default
		
		if f[0].upper()=='"TRUE"' or f[0].upper()=='TRUE':
			return True
		elif f[0].upper()=='"FALSE"' or f[0].upper()=='FALSE':
			return False
		else:
			print '\t"%s" could not be used as a bool for field "%s"' % (f[1], field)
			return default
	
	def getFieldAsString(self, field, default=None):
		self_real = self.getRealNode() # incase we're an instance
		
		f = self_real.getFieldName(field)
		if f==None:	return default
		if len(f) < 1:
			print '\t"%s" wrong length for string conversion for field "%s"' % (f, field)
			return default
		
		if len(f) > 1:
			# String may contain spaces
			st = ' '.join(f)
		else:
			st = f[0]
		
		# X3D HACK 
		if self.x3dNode: 
			return st
			
		if st[0]=='"' and st[-1]=='"':
			return st[1:-1]
		else:
			print '\tvalue "%s" could not be used as a string for field "%s"' % (f[0], field)
			return default
	
	def getFieldAsArray(self, field, group):
		'''
		For this parser arrays are children
		'''
		self_real = self.getRealNode() # incase we're an instance
		
		child_array = None
		for child in self_real.children:
			if child.id and len(child.id) == 1 and child.id[0] == field:
				child_array = child
				break
		
		if child_array==None:
			# For x3d, should work ok with vrml too
			# for x3d arrays are fields, vrml they are nodes, annoying but not tooo bad.
			data_split = self.getFieldName(field)
			if not data_split:
				return []
			array_data = ' '.join(data_split)
			if array_data == None:
				return []
			
			array_data = array_data.replace(',', ' ')
			data_split = array_data.split()
			try:
				array_data = [int(val) for val in data_split]
			except:
				try:
					array_data = [float(val) for val in data_split]
				except:
					print '\tWarning, could not parse array data from field'
					array_data = []
		else:
			
			# Normal vrml
			array_data = child_array.array_data
		
		if group==-1 or len(array_data)==0:
			return array_data
		
		# We want a flat list
		flat = True
		for item in array_data:
			if type(item) == list:
				flat = False
				break
		
		# make a flat array
		if flat:
			flat_array = array_data # we are alredy flat.
		else:
			flat_array = []
			
			def extend_flat(ls):
				for item in ls:
					if type(item)==list:	extend_flat(item)
					else:					flat_array.append(item)
			
			extend_flat(array_data)
		
		
		# We requested a flat array
		if group == 0:
			return flat_array
			
			
		
		new_array = []
		sub_array = []
		
		for item in flat_array:
			sub_array.append(item)
			if len(sub_array)==group:
				new_array.append(sub_array)
				sub_array = []
		
		if sub_array:
			print '\twarning, array was not aligned to requested grouping', group, 'remaining value', sub_array
		
		return new_array
	
	def getLevel(self):
		# Ignore self_real
		level = 0
		p = self.parent
		while p:
			level +=1
			p = p.parent
			if not p: break
			
		return level
	
	def __repr__(self):
		level = self.getLevel()
		ind = '  ' * level
		
		if self.node_type==NODE_REFERENCE:
			brackets = ''
		elif self.node_type==NODE_NORMAL:
			brackets = '{}'
		else:
			brackets = '[]'
		
		if brackets:
			text = ind + brackets[0] + '\n'
		else:
			text = ''
		
		text += ind + 'ID: ' + str(self.id) + ' ' + str(level) + ('lineno %d\n' % self.lineno)
		
		if self.node_type==NODE_REFERENCE:
			return text
		
		for item in self.fields:
			text += ind + str(item) +'\n'
		
		#text += ind + 'ARRAY: ' + str(len(self.array_data)) + ' ' + str(self.array_data) + '\n'
		text += ind + 'ARRAY: ' + str(len(self.array_data)) + '[...] \n'
		
		text += ind + 'CHILDREN: ' + str(len(self.children)) + '\n'
		for child in self.children:
			text += str(child)
		
		text += '\n' + ind + brackets[1]
		
		return text
	
	def parse(self, i):
		new_i = self.__parse(i)
		
		# print self.id, self.getFilename()
		
		# If we were an inline then try load the file
		if self.node_type == NODE_NORMAL and self.getSpec() == 'Inline':
			url = self.getFieldAsString('url', None)
			
			if url != None:
				if not exists(url):
					url = dirName(self.getFilename()) + baseName(url)
				if not exists(url):
					print '\tWarning: Inline URL could not be found:', url
				else:
					if url==self.getFilename(): 
						print '\tWarning: cant Inline yourself recursively:', url
					else:
						
						try:
							f = open(url, 'rU')
						except:
							print '\tWarning: cant open the file:', url
							f = None
						
						if f:
							# Tricky - inline another VRML
							print '\tLoading Inline:"%s"...' % url
							
							# Watch it! - backup lines
							lines_old = lines[:]
							
							
							lines[:] = vrmlFormat( f.read() )
							f.close()
							
							lines.insert(0, '{')
							lines.insert(0, 'root_node____')
							lines.append('}')
							
							child = vrmlNode(self, NODE_NORMAL, -1)
							child.setRoot(url) # initialized dicts
							child.parse(0)
							
							# Watch it! - restore lines
							lines[:] = lines_old
					
		
		return new_i
	
	def __parse(self, i):
		# print 'parsing at', i,
		# print i, self.id, self.lineno
		l = lines[i]
		
		if l=='[':
			# An anonymous list
			self.id = None
			i+=1
		else:
			words = []
			node_type, new_i = is_nodeline(i, words)
			if not node_type: # fail for parsing new node.
				raise "error"
			
			if self.node_type==NODE_REFERENCE:
				# Only assign the reference and quit
				key = words[words.index('USE')+1]
				self.id = (words[0],)
				
				self.reference = self.getDefDict()[key]
				return new_i
			
			self.id = tuple(words)
			
			# fill in DEF/USE
			key = self.getDefName()
			
			if key != None:
				self.getDefDict()[ key ] = self
			
			i = new_i
		
		# print self.id
		ok = True
		while ok:
			l = lines[i]
			# print '\t', i, l
			if l=='':
				i+=1
				continue 
			
			if l=='}':
				if self.node_type != NODE_NORMAL:
					print 'wrong node ending, expected an } ' + str(i)
					raise ""
				### print "returning", i
				return i+1
			if l==']':
				if self.node_type != NODE_ARRAY:
					print 'wrong node ending, expected a ] ' + str(i)
					raise ""
				### print "returning", i
				return i+1
				
			node_type, new_i = is_nodeline(i, [])
			if node_type: # check text\n{
				### print '\t\tgroup', i
				child = vrmlNode(self, node_type, i)
				i = child.parse(i)
				# print child.id, 'YYY'
				
			elif l=='[': # some files have these anonymous lists
				child = vrmlNode(self, NODE_ARRAY, i)
				i = child.parse(i)
				
			elif is_numline(i):
				l_split = l.split(',')
				
				values = None
				# See if each item is a float?
				
				for num_type in (int, float):
					try:
						values = [num_type(v) for v in l_split ]
						break
					except:
						pass
					
					
					try:
						values = [[num_type(v) for v in segment.split()] for segment in l_split ]
						break
					except:
						pass
				
				if values == None: # dont parse
					values = l_split
				
				# This should not extend over multiple lines however it is possible
				self.array_data.extend( values )
				i+=1
			else:
				words = l.split()
				if len(words) > 2 and words[1] == 'USE':
					vrmlNode(self, NODE_REFERENCE, i)
				else:
					
					# print "FIELD", i, l
					# 
					#words = l.split()
					### print '\t\ttag', i
					# this is a tag/
					# print words, i, l
					value = l
					# print i
					# javastrips can exist as values.
					quote_count = l.count('"')
					if quote_count % 2: # odd number?
						# print 'MULTILINE'
						while 1:
							i+=1
							l = lines[i]
							quote_count = l.count('"')
							if quote_count % 2: # odd number?
								value += '\n'+ l[:l.rfind('"')]
								break # assume
							else:
								value += '\n'+ l
					
					value_all = value.split()
					
					def iskey(k):
						if k[0] != '"' and k[0].isalpha() and k.upper() not in ('TRUE', 'FALSE'):
							return True
						return False
					
					def split_fields(value):
						'''
						key 0.0 otherkey 1,2,3 opt1 opt1 0.0
							-> [key 0.0], [otherkey 1,2,3], [opt1 opt1 0.0]
						'''
						field_list = []
						field_context = []
						
						for j in xrange(len(value)):
							if iskey(value[j]):
								if field_context:
									# this IS a key but the previous value was not a key, ot it was a defined field.
									if (not iskey(field_context[-1])) or ((len(field_context)==3 and field_context[1]=='IS')):
										field_list.append(field_context)
										field_context = [value[j]]
									else:
										# The last item was not a value, multiple keys are needed in some cases.
										field_context.append(value[j])
								else:
									# Is empty, just add this on
									field_context.append(value[j])
							else:
								# Add a value to the list
								field_context.append(value[j])
						
						if field_context:
							field_list.append(field_context)
						
						return field_list
					
					
					for value in split_fields(value_all):
						# Split 
						
						if value[0]=='field':
							# field SFFloat creaseAngle 4
							self.getFieldDict()[value[2]] = value[3:] # skip the first 3 values
						else:
							# Get referenced field
							if len(value) >= 3 and value[1]=='IS':
								try:
									value = [ value[0] ] + self.getFieldDict()[ value[2] ]
								except:
									print '\tWarning, field could not be found:', value, 'TODO add support for exposedField'
									print '\t', self.getFieldDict()
									self.fields.append(value)
							else:
								self.fields.append(value)
				i+=1

def vrml_parse(path):
	'''
	Sets up the root node and returns it so load_web3d() can deal with the blender side of things.
	Return root (vrmlNode, '') or (None, 'Error String')
	'''
	try:	f = open(path, 'rU')
	except:	return None, 'Failed to open file: ' + path
	
	# Stripped above
	lines[:] = vrmlFormat( f.read() )
	f.close()
	lines.insert(0, '{')
	lines.insert(0, 'dymmy_node')
	lines.append('}')
	
	# Use for testing our parsed output, so we can check on line numbers.
	
	## ff = open('m:\\test.txt', 'w')
	## ff.writelines([l+'\n' for l in lines])
	
	
	# Now evaluate it
	node_type, new_i = is_nodeline(0, [])
	if not node_type:
		return None, 'Error: VRML file has no starting Node'
	
	# Trick to make sure we get all root nodes.
	lines.insert(0, '{')
	lines.insert(0, 'root_node____') # important the name starts with an ascii char
	lines.append('}')
	
	root = vrmlNode(None, NODE_NORMAL, -1)
	root.setRoot(path) # we need to set the root so we have a namespace and know the path incase of inlineing
	
	# Parse recursively
	root.parse(0)
	
	# print root
	return root, ''

# ====================== END VRML 



# ====================== X3d Support

# Sane as vrml but replace the parser
class x3dNode(vrmlNode):
	def __init__(self, parent, node_type, x3dNode):
		vrmlNode.__init__(self, parent, node_type, -1)
		self.x3dNode = x3dNode
		
	def parse(self):
		# print self.x3dNode.tagName
		
		define = self.x3dNode.getAttributeNode('DEF')
		if define:
			self.getDefDict()[define.value] = self
		else:
			use = self.x3dNode.getAttributeNode('USE')
			if use:
				try:
					self.reference = self.getDefDict()[use.value]
					self.node_type = NODE_REFERENCE
				except:
					print '\tWarning: reference', use.value, 'not found'
					self.parent.children.remove(self)
				
				return
		
		for x3dChildNode in self.x3dNode.childNodes:
			if x3dChildNode.nodeType in (x3dChildNode.TEXT_NODE, x3dChildNode.COMMENT_NODE, x3dChildNode.CDATA_SECTION_NODE):
				continue
			
			node_type = NODE_NORMAL
			# print x3dChildNode, dir(x3dChildNode)
			if x3dChildNode.getAttributeNode('USE'):
				node_type = NODE_REFERENCE
			
			child = x3dNode(self, node_type, x3dChildNode)
			child.parse()
		
		# TODO - x3d Inline
		
	def getSpec(self):
		return self.x3dNode.tagName # should match vrml spec
	
	def getDefName(self):
		data = self.x3dNode.getAttributeNode('DEF')
		if data: data.value
		return None
	
	# Other funcs operate from vrml, but this means we can wrap XML fields, still use nice utility funcs
	# getFieldAsArray getFieldAsBool etc
	def getFieldName(self, field):
		self_real = self.getRealNode() # incase we're an instance
		field_xml = self.x3dNode.getAttributeNode(field)
		if field_xml:
			value = field_xml.value
			
			# We may want to edit. for x3d spesific stuff
			# Sucks a bit to return the field name in the list but vrml excepts this :/
			return value.split()
		else:
			return None

def x3d_parse(path):
	'''
	Sets up the root node and returns it so load_web3d() can deal with the blender side of things.
	Return root (x3dNode, '') or (None, 'Error String')
	'''
	
	try:
		import xml.dom.minidom
	except:
		return None, 'Error, import XML parsing module (xml.dom.minidom) failed, install python'
	
	'''
	try:	doc = xml.dom.minidom.parse(path)
	except:	return None, 'Could not parse this X3D file, XML error'
	'''
	
	# Could add a try/except here, but a console error is more useful.
	doc = xml.dom.minidom.parse(path)
	
	
	try:
		x3dnode = doc.getElementsByTagName('X3D')[0]
	except:
		return None, 'Not a valid x3d document, cannot import'
	
	root = x3dNode(None, NODE_NORMAL, x3dnode)
	root.setRoot(path) # so images and Inline's we load have a relative path
	root.parse()
	
	return root, ''



## f = open('/_Cylinder.wrl', 'r')
# f = open('/fe/wrl/Vrml/EGS/TOUCHSN.WRL', 'r')
# vrml_parse('/fe/wrl/Vrml/EGS/TOUCHSN.WRL')
#vrml_parse('/fe/wrl/Vrml/EGS/SCRIPT.WRL')
'''

import os
files = os.popen('find /fe/wrl -iname "*.wrl"').readlines()
files.sort()
tot = len(files)
for i, f in enumerate(files):
	#if i < 801:
	#	continue
	
	f = f.strip()
	print f, i, tot
	vrml_parse(f)
'''

# NO BLENDER CODE ABOVE THIS LINE.
# -----------------------------------------------------------------------------------
import bpy
import BPyImage
import Blender
from Blender import Texture, Material, Mathutils, Mesh, Types, Window
from Blender.Mathutils import TranslationMatrix
from Blender.Mathutils import RotationMatrix
from Blender.Mathutils import Vector
from Blender.Mathutils import Matrix

RAD_TO_DEG = 57.29578

GLOBALS = {'CIRCLE_DETAIL':16}

def translateRotation(rot):
	'''	axis, angle	'''
	return RotationMatrix(rot[3]*RAD_TO_DEG, 4, 'r', Vector(rot[:3]))

def translateScale(sca):
	mat = Matrix() # 4x4 default
	mat[0][0] = sca[0]
	mat[1][1] = sca[1]
	mat[2][2] = sca[2]
	return mat

def translateTransform(node):
	cent =		node.getFieldAsFloatTuple('center', None) # (0.0, 0.0, 0.0)
	rot =		node.getFieldAsFloatTuple('rotation', None) # (0.0, 0.0, 1.0, 0.0)
	sca =		node.getFieldAsFloatTuple('scale', None) # (1.0, 1.0, 1.0)
	scaori =	node.getFieldAsFloatTuple('scaleOrientation', None) # (0.0, 0.0, 1.0, 0.0)
	tx =		node.getFieldAsFloatTuple('translation', None) # (0.0, 0.0, 0.0)
	
	if cent:
		cent_mat = TranslationMatrix(Vector(cent)).resize4x4()
		cent_imat = cent_mat.copy().invert()
	else:
		cent_mat = cent_imat = None
	
	if rot:		rot_mat = translateRotation(rot)
	else:		rot_mat = None
	
	if sca:		sca_mat = translateScale(sca)
	else:		sca_mat = None
	
	if scaori:
		scaori_mat = translateRotation(scaori)
		scaori_imat = scaori_mat.copy().invert()
	else:
		scaori_mat = scaori_imat = None
	
	if tx:		tx_mat = TranslationMatrix(Vector(tx)).resize4x4()
	else:		tx_mat = None
	
	new_mat = Matrix()
	
	mats = [tx_mat, cent_mat, rot_mat, scaori_mat, sca_mat, scaori_imat, cent_imat]
	for mtx in mats:
		if mtx:
			new_mat = mtx * new_mat
	
	return new_mat

def translateTexTransform(node):
	cent =		node.getFieldAsFloatTuple('center', None) # (0.0, 0.0)
	rot =		node.getFieldAsFloat('rotation', None) # 0.0
	sca =		node.getFieldAsFloatTuple('scale', None) # (1.0, 1.0)
	tx =		node.getFieldAsFloatTuple('translation', None) # (0.0, 0.0)
	
	
	if cent:
		# cent is at a corner by default
		cent_mat = TranslationMatrix(Vector(cent).resize3D()).resize4x4()
		cent_imat = cent_mat.copy().invert()
	else:
		cent_mat = cent_imat = None
	
	if rot:		rot_mat = RotationMatrix(rot*RAD_TO_DEG, 4, 'z') # translateRotation(rot)
	else:		rot_mat = None
	
	if sca:		sca_mat = translateScale((sca[0], sca[1], 0.0))
	else:		sca_mat = None
	
	if tx:		tx_mat = TranslationMatrix(Vector(tx).resize3D()).resize4x4()
	else:		tx_mat = None
	
	new_mat = Matrix()
	
	# as specified in VRML97 docs
	mats = [cent_imat, sca_mat, rot_mat, cent_mat, tx_mat]

	for mtx in mats:
		if mtx:
			new_mat = mtx * new_mat
	
	return new_mat


def getFinalMatrix(node, mtx, ancestry):
	
	transform_nodes = [node_tx for node_tx in ancestry if node_tx.getSpec() == 'Transform']
	if node.getSpec()=='Transform':
		transform_nodes.append(node)
	transform_nodes.reverse()
	
	if mtx==None:
		mtx = Matrix()
	
	for node_tx in transform_nodes:
		mat = translateTransform(node_tx)
		mtx = mtx * mat
	
	return mtx

def importMesh_IndexedFaceSet(geom, bpyima):
	# print geom.lineno, geom.id, vrmlNode.DEF_NAMESPACE.keys()
	
	ccw =				geom.getFieldAsBool('ccw', True)
	ifs_colorPerVertex =	geom.getFieldAsBool('colorPerVertex', True) # per vertex or per face
	ifs_normalPerVertex =	geom.getFieldAsBool('normalPerVertex', True)
	
	# This is odd how point is inside Coordinate
	
	# VRML not x3d
	#coord = geom.getChildByName('coord') # 'Coordinate'
	
	coord = geom.getChildBySpec('Coordinate') # 'Coordinate'
	
	if coord:	ifs_points = coord.getFieldAsArray('point', 3)
	else:		coord = []
	
	if not coord:
		print '\tWarnint: IndexedFaceSet has no points'
		return None, ccw
	
	ifs_faces = geom.getFieldAsArray('coordIndex', 0)
	
	coords_tex = None
	if ifs_faces: # In rare cases this causes problems - no faces but UVs???
		
		# WORKS - VRML ONLY
		# coords_tex = geom.getChildByName('texCoord')
		coords_tex = geom.getChildBySpec('TextureCoordinate')
		
		if coords_tex:
			ifs_texpoints = coords_tex.getFieldAsArray('point', 2)
			ifs_texfaces = geom.getFieldAsArray('texCoordIndex', 0)
			
			if not ifs_texpoints:
				# IF we have no coords, then dont bother
				coords_tex = None
		
		
	# WORKS - VRML ONLY
	# vcolor = geom.getChildByName('color')
	vcolor = geom.getChildBySpec('Color')
	vcolor_spot = None # spot color when we dont have an array of colors
	if vcolor:
		# float to char
		ifs_vcol = [[int(c*256) for c in col] for col in vcolor.getFieldAsArray('color', 3)]
		ifs_color_index = geom.getFieldAsArray('colorIndex', 0)
		
		if not ifs_vcol:
			vcolor_spot = [int(c*256) for c in vcolor.getFieldAsFloatTuple('color', [])]
	
	# Convert faces into somthing blender can use
	edges = []
	
	# All lists are aligned!
	faces = []
	faces_uv = [] # if ifs_texfaces is empty then the faces_uv will match faces exactly.
	faces_orig_index = [] # for ngons, we need to know our original index
	
	if coords_tex and ifs_texfaces:
		do_uvmap = True
	else:
		do_uvmap = False
	
	# current_face = [0] # pointer anyone
	
	def add_face(face, fuvs, orig_index):
		l = len(face)
		if l==3 or l==4:
			faces.append(face)
			# faces_orig_index.append(current_face[0])
			if do_uvmap:
				faces_uv.append(fuvs)
				
			faces_orig_index.append(orig_index)
		elif l==2:			edges.append(face)
		elif l>4:
			for i in xrange(2, len(face)):
				faces.append([face[0], face[i-1], face[i]])
				if do_uvmap:
					faces_uv.append([fuvs[0], fuvs[i-1], fuvs[i]])
				faces_orig_index.append(orig_index)
		else:
			# faces with 1 verts? pfft!
			# still will affect index ordering
			pass
		
	
	face = []
	fuvs = []
	orig_index = 0
	for i, fi in enumerate(ifs_faces):
		# ifs_texfaces and ifs_faces should be aligned
		if fi != -1:
			face.append(int(fi)) # in rare cases this is a float
			
			if do_uvmap:
				if i >= len(ifs_texfaces):
					print '\tWarning: UV Texface index out of range'
					fuvs.append(ifs_texfaces[0])
				else:
					fuvs.append(ifs_texfaces[i])
		else:
			add_face(face, fuvs, orig_index)
			face = []
			if do_uvmap:
				fuvs = []
			orig_index += 1
	
	add_face(face, fuvs, orig_index)
	del add_face # dont need this func anymore
	
	bpymesh = bpy.data.meshes.new()
	
	bpymesh.verts.extend(ifs_points)
	
	# print len(ifs_points), faces, edges, ngons
	
	try:
		bpymesh.faces.extend(faces, smooth=True, ignoreDups=True)
	except KeyError:
		print "one or more vert indicies out of range. corrupt file?"
		#for f in faces:
		#	bpymesh.faces.extend(faces, smooth=True)
	
	bpymesh.calcNormals()
	
	if len(bpymesh.faces) != len(faces):
		print '\tWarning: adding faces did not work! file is invalid, not adding UVs or vcolors'
		return bpymesh, ccw
	
	# Apply UVs if we have them
	if not do_uvmap:
		faces_uv = faces # fallback, we didnt need a uvmap in the first place, fallback to the face/vert mapping.
	if coords_tex:
		#print ifs_texpoints
		# print geom
		bpymesh.faceUV = True
		for i,f in enumerate(bpymesh.faces):
			f.image = bpyima
			fuv = faces_uv[i] # uv indicies
			for j,uv in enumerate(f.uv):
				# print fuv, j, len(ifs_texpoints)
				try:
					uv[:] = ifs_texpoints[fuv[j]]
				except:
					print '\tWarning: UV Index out of range'
					uv[:] = ifs_texpoints[0]
	
	elif bpyima and len(bpymesh.faces):
		# Oh Bugger! - we cant really use blenders ORCO for for texture space since texspace dosnt rotate.
		# we have to create VRML's coords as UVs instead.
		
		# VRML docs
		'''
		If the texCoord field is NULL, a default texture coordinate mapping is calculated using the local
		coordinate system bounding box of the shape. The longest dimension of the bounding box defines the S coordinates,
		and the next longest defines the T coordinates. If two or all three dimensions of the bounding box are equal,
		ties shall be broken by choosing the X, Y, or Z dimension in that order of preference.
		The value of the S coordinate ranges from 0 to 1, from one end of the bounding box to the other.
		The T coordinate ranges between 0 and the ratio of the second greatest dimension of the bounding box to the greatest dimension.
		'''
		
		# Note, S,T == U,V
		# U gets longest, V gets second longest
		xmin, ymin, zmin = ifs_points[0]
		xmax, ymax, zmax = ifs_points[0]
		for co in ifs_points:
			x,y,z = co
			if x < xmin: xmin = x
			if y < ymin: ymin = y
			if z < zmin: zmin = z
			
			if x > xmax: xmax = x
			if y > ymax: ymax = y
			if z > zmax: zmax = z
			
		xlen = xmax - xmin
		ylen = ymax - ymin
		zlen = zmax - zmin
		
		depth_min = xmin, ymin, zmin
		depth_list = [xlen, ylen, zlen]
		depth_sort = depth_list[:]
		depth_sort.sort()
		
		depth_idx = [depth_list.index(val) for val in depth_sort]
		
		axis_u = depth_idx[-1]
		axis_v = depth_idx[-2] # second longest
		
		# Hack, swap these !!! TODO - Why swap??? - it seems to work correctly but should not.
		# axis_u,axis_v = axis_v,axis_u
		
		min_u = depth_min[axis_u]
		min_v = depth_min[axis_v]
		depth_u = depth_list[axis_u]
		depth_v = depth_list[axis_v]
		
		depth_list[axis_u]
		
		if axis_u == axis_v:
			# This should be safe because when 2 axies have the same length, the lower index will be used.
			axis_v += 1
		
		bpymesh.faceUV = True
		
		# HACK !!! - seems to be compatible with Cosmo though.
		depth_v = depth_u = max(depth_v, depth_u)
		
		for f in bpymesh.faces:
			f.image = bpyima
			fuv = f.uv
			
			for i,v in enumerate(f):
				co = v.co
				fuv[i][:] = (co[axis_u]-min_u) / depth_u, (co[axis_v]-min_v) / depth_v
	
	# Add vcote 
	if vcolor:
		# print ifs_vcol
		bpymesh.vertexColors = True
		
		for f in bpymesh.faces:
			fcol = f.col
			if ifs_colorPerVertex:
				fv = f.verts
				for i,c in enumerate(fcol):
					color_index = fv[i].index # color index is vert index
					if ifs_color_index: color_index = ifs_color_index[color_index]
					
					if len(ifs_vcol) < color_index:
						c.r, c.g, c.b = ifs_vcol[color_index]
					else:
						print '\tWarning: per face color index out of range'
			else:
				if vcolor_spot: # use 1 color, when ifs_vcol is []
					for c in fcol:
						c.r, c.g, c.b = vcolor_spot
				else:
					color_index = faces_orig_index[f.index] # color index is face index
					#print color_index, ifs_color_index
					if ifs_color_index:
						if color_index <= len(ifs_color_index):
							print '\tWarning: per face color index out of range'
							color_index = 0
						else:
							color_index = ifs_color_index[color_index]
						
					
					col = ifs_vcol[color_index]
					for i,c in enumerate(fcol):
						c.r, c.g, c.b = col
	
	return bpymesh, ccw

def importMesh_IndexedLineSet(geom):
	coord = geom.getChildByName('coord') # 'Coordinate'
	if coord:	points = coord.getFieldAsArray('point', 3)
	else:		points = []
	
	if not points:
		print '\tWarning: IndexedLineSet had no points'
		return None
	
	ils_lines = geom.getFieldAsArray('coordIndex', 0)
	
	lines = []
	line = []
	
	for il in ils_lines:
		if il==-1:
			lines.append(line)
			line = []
		else:
			line.append(int(il))
	lines.append(line)
	
	# vcolor = geom.getChildByName('color') # blender dosnt have per vertex color
	
	bpycurve = bpy.data.curves.new('IndexedCurve', 'Curve')
	bpycurve.setFlag(1)
	
	w=t=1
	
	curve_index = 0
	
	for line in lines:
		if not line:
			continue
		co = points[line[0]]
		bpycurve.appendNurb([co[0], co[1], co[2], w, t])
		bpycurve[curve_index].type= 0 # Poly Line
		
		for il in line[1:]:
			co = points[il]
			bpycurve.appendPoint(curve_index, [co[0], co[1], co[2], w])
		
		
		curve_index += 1
	
	return bpycurve


def importMesh_PointSet(geom):
	coord = geom.getChildByName('coord') # 'Coordinate'
	if coord:	points = coord.getFieldAsArray('point', 3)
	else:		points = []
	
	# vcolor = geom.getChildByName('color') # blender dosnt have per vertex color
	
	bpymesh = bpy.data.meshes.new()
	bpymesh.verts.extend(points)
	bpymesh.calcNormals() # will just be dummy normals
	return bpymesh

GLOBALS['CIRCLE_DETAIL'] = 12

MATRIX_Z_TO_Y = RotationMatrix(90, 4, 'x')

def importMesh_Sphere(geom):
	# bpymesh = bpy.data.meshes.new()
	diameter = geom.getFieldAsFloat('radius', 0.5) * 2 # * 2 for the diameter
	bpymesh = Mesh.Primitives.UVsphere(GLOBALS['CIRCLE_DETAIL'], GLOBALS['CIRCLE_DETAIL'], diameter)  
	bpymesh.transform(MATRIX_Z_TO_Y)
	return bpymesh

def importMesh_Cylinder(geom):
	# bpymesh = bpy.data.meshes.new()
	diameter = geom.getFieldAsFloat('radius', 1.0) * 2 # * 2 for the diameter
	height = geom.getFieldAsFloat('height', 2)
	bpymesh = Mesh.Primitives.Cylinder(GLOBALS['CIRCLE_DETAIL'], diameter, height) 
	bpymesh.transform(MATRIX_Z_TO_Y)
	
	# Warning - Rely in the order Blender adds verts
	# not nice design but wont change soon.
	
	bottom = geom.getFieldAsBool('bottom', True)
	side = geom.getFieldAsBool('side', True)
	top = geom.getFieldAsBool('top', True)
	
	if not top: # last vert is top center of tri fan.
		bpymesh.verts.delete([(GLOBALS['CIRCLE_DETAIL']+GLOBALS['CIRCLE_DETAIL'])+1])
	
	if not bottom: # second last vert is bottom of triangle fan
		bpymesh.verts.delete([GLOBALS['CIRCLE_DETAIL']+GLOBALS['CIRCLE_DETAIL']])
	
	if not side:
		# remove all quads
		bpymesh.faces.delete(1, [f for f in bpymesh.faces if len(f)==4])
	
	return bpymesh

def importMesh_Cone(geom):
	# bpymesh = bpy.data.meshes.new()
	diameter = geom.getFieldAsFloat('bottomRadius', 1.0) * 2 # * 2 for the diameter
	height = geom.getFieldAsFloat('height', 2)
	bpymesh = Mesh.Primitives.Cone(GLOBALS['CIRCLE_DETAIL'], diameter, height) 
	bpymesh.transform(MATRIX_Z_TO_Y)
	
	# Warning - Rely in the order Blender adds verts
	# not nice design but wont change soon.
	
	bottom = geom.getFieldAsBool('bottom', True)
	side = geom.getFieldAsBool('side', True)
	
	if not bottom: # last vert is on the bottom
		bpymesh.verts.delete([GLOBALS['CIRCLE_DETAIL']+1])
	if not side: # second last vert is on the pointy bit of the cone
		bpymesh.verts.delete([GLOBALS['CIRCLE_DETAIL']])
	
	return bpymesh

def importMesh_Box(geom):
	# bpymesh = bpy.data.meshes.new()
	
	size = geom.getFieldAsFloatTuple('size', (2.0, 2.0, 2.0))
	bpymesh = Mesh.Primitives.Cube(1.0) 

	# Scale the box to the size set
	scale_mat = Matrix([size[0],0,0], [0, size[1], 0], [0, 0, size[2]])
	bpymesh.transform(scale_mat.resize4x4())
	
	return bpymesh

def importShape(node, ancestry):
	vrmlname = node.getDefName()
	if not vrmlname: vrmlname = 'Shape'
	
	# works 100% in vrml, but not x3d
	#appr = node.getChildByName('appearance') # , 'Appearance'
	#geom = node.getChildByName('geometry') # , 'IndexedFaceSet'
	
	# Works in vrml and x3d
	appr = node.getChildBySpec('Appearance')
	geom = node.getChildBySpec(['IndexedFaceSet', 'IndexedLineSet', 'PointSet', 'Sphere', 'Box', 'Cylinder', 'Cone'])
	
	# For now only import IndexedFaceSet's
	if geom:
		bpymat = None
		bpyima = None
		texmtx = None
		if appr:
			
			#mat = appr.getChildByName('material') # 'Material'
			#ima = appr.getChildByName('texture') # , 'ImageTexture'
			#if ima and ima.getSpec() != 'ImageTexture':
			#	print '\tWarning: texture type "%s" is not supported' % ima.getSpec() 
			#	ima = None
			# textx = appr.getChildByName('textureTransform')
			
			mat = appr.getChildBySpec('Material')
			ima = appr.getChildBySpec('ImageTexture')
			
			textx = appr.getChildBySpec('TextureTransform')
			
			if textx:
				texmtx = translateTexTransform(textx)
			

			
			# print mat, ima
			if mat or ima:
				
				if not mat:
					mat = ima # This is a bit dumb, but just means we use default values for all
				
				# all values between 0.0 and 1.0, defaults from VRML docs
				bpymat = bpy.data.materials.new()
				bpymat.amb =		mat.getFieldAsFloat('ambientIntensity', 0.2)
				bpymat.rgbCol =		mat.getFieldAsFloatTuple('diffuseColor', [0.8, 0.8, 0.8])
				
				# NOTE - blender dosnt support emmisive color
				# Store in mirror color and approximate with emit.
				emit =				mat.getFieldAsFloatTuple('emissiveColor', [0.0, 0.0, 0.0])
				bpymat.mirCol =		emit
				bpymat.emit = 		(emit[0]+emit[1]+emit[2])/3.0
				
				bpymat.hard =		int(1+(510*mat.getFieldAsFloat('shininess', 0.2))) # 0-1 -> 1-511
				bpymat.specCol =	mat.getFieldAsFloatTuple('specularColor', [0.0, 0.0, 0.0])
				bpymat.alpha =		1.0 - mat.getFieldAsFloat('transparency', 0.0)
				if bpymat.alpha < 0.999:
					bpymat.mode |= Material.Modes.ZTRANSP
			
			
			if ima:
				# print ima
				ima_url =			ima.getFieldAsString('url')
				if ima_url==None:
					print "\twarning, image with no URL, this is odd"
				else:
					bpyima= BPyImage.comprehensiveImageLoad(ima_url, dirName(node.getFilename()), PLACE_HOLDER= False, RECURSIVE= False)
					if bpyima:
						texture= bpy.data.textures.new()
						texture.setType('Image')
						texture.image = bpyima
						
						# Adds textures for materials (rendering)
						try:	depth = bpyima.depth
						except:	depth = -1
						
						if depth == 32:
							# Image has alpha
							bpymat.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.COL | Texture.MapTo.ALPHA)
							texture.setImageFlags('MipMap', 'InterPol', 'UseAlpha')
							bpymat.mode |= Material.Modes.ZTRANSP
							bpymat.alpha = 0.0
						else:
							bpymat.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.COL)
							
						ima_repS =			ima.getFieldAsBool('repeatS', True)
						ima_repT =			ima.getFieldAsBool('repeatT', True)
						
						texture.repeat =	max(1, ima_repS * 512), max(1, ima_repT * 512)
						
						if not ima_repS: bpyima.clampX = True
						if not ima_repT: bpyima.clampY = True
		
		bpydata = None
		geom_spec = geom.getSpec()
		ccw = True
		if geom_spec == 'IndexedFaceSet':
			bpydata, ccw = importMesh_IndexedFaceSet(geom, bpyima)
		elif geom_spec == 'IndexedLineSet':
			bpydata = importMesh_IndexedLineSet(geom)
		elif geom_spec == 'PointSet':
			bpydata = importMesh_PointSet(geom)
		elif geom_spec == 'Sphere':
			bpydata = importMesh_Sphere(geom)
		elif geom_spec == 'Box':
			bpydata = importMesh_Box(geom)
		elif geom_spec == 'Cylinder':
			bpydata = importMesh_Cylinder(geom)
		elif geom_spec == 'Cone':
			bpydata = importMesh_Cone(geom)
		else:
			print '\tWarning: unsupported type "%s"' % geom_spec
			return
		
		if bpydata:
			vrmlname = vrmlname + geom_spec
			
			bpydata.name = vrmlname
			
			bpyob  = node.blendObject = bpy.data.scenes.active.objects.new(bpydata)
			
			if type(bpydata) == Types.MeshType:
				is_solid =			geom.getFieldAsBool('solid', True)
				creaseAngle =		geom.getFieldAsFloat('creaseAngle', None)
				
				if creaseAngle != None:
					bpydata.maxSmoothAngle = 1+int(min(79, creaseAngle * RAD_TO_DEG))
					bpydata.mode |= Mesh.Modes.AUTOSMOOTH
				
				# Only ever 1 material per shape
				if bpymat:	bpydata.materials = [bpymat]
				
				if bpydata.faceUV and texmtx:
					# Apply texture transform?
					uv_copy = Vector()
					for f in bpydata.faces:
						for uv in f.uv:
							uv_copy.x = uv.x
							uv_copy.y = uv.y
							
							uv.x, uv.y = (uv_copy * texmtx)[0:2]
				# Done transforming the texture
				
				
				# Must be here and not in IndexedFaceSet because it needs an object for the flip func. Messy :/
				if not ccw: bpydata.flipNormals()
				
				
			# else could be a curve for example
			
			
			
			# Can transform data or object, better the object so we can instance the data
			#bpymesh.transform(getFinalMatrix(node))
			bpyob.setMatrix( getFinalMatrix(node, None, ancestry) )


def importLamp_PointLight(node):
	vrmlname = node.getDefName()
	if not vrmlname: vrmlname = 'PointLight'
	
	# ambientIntensity = node.getFieldAsFloat('ambientIntensity', 0.0) # TODO
	# attenuation = node.getFieldAsFloatTuple('attenuation', (1.0, 0.0, 0.0)) # TODO
	color = node.getFieldAsFloatTuple('color', (1.0, 1.0, 1.0))
	intensity = node.getFieldAsFloat('intensity', 1.0) # max is documented to be 1.0 but some files have higher.
	location = node.getFieldAsFloatTuple('location', (0.0, 0.0, 0.0))
	# is_on = node.getFieldAsBool('on', True) # TODO
	radius = node.getFieldAsFloat('radius', 100.0)
	
	bpylamp = bpy.data.lamps.new()
	bpylamp.setType('Lamp')
	bpylamp.energy = intensity
	bpylamp.dist = radius
	bpylamp.col = color
	
	mtx = TranslationMatrix(Vector(location))
	
	return bpylamp, mtx

def importLamp_DirectionalLight(node):
	vrmlname = node.getDefName()
	if not vrmlname: vrmlname = 'DirectLight'
	
	# ambientIntensity = node.getFieldAsFloat('ambientIntensity', 0.0) # TODO
	color = node.getFieldAsFloatTuple('color', (1.0, 1.0, 1.0))
	direction = node.getFieldAsFloatTuple('direction', (0.0, 0.0, -1.0))
	intensity = node.getFieldAsFloat('intensity', 1.0) # max is documented to be 1.0 but some files have higher.
	# is_on = node.getFieldAsBool('on', True) # TODO
	
	bpylamp = bpy.data.lamps.new(vrmlname)
	bpylamp.setType('Sun')
	bpylamp.energy = intensity
	bpylamp.col = color
	
	# lamps have their direction as -z, yup
	mtx = Vector(direction).toTrackQuat('-z', 'y').toMatrix().resize4x4()
	
	return bpylamp, mtx

# looks like default values for beamWidth and cutOffAngle were swapped in VRML docs.

def importLamp_SpotLight(node):
	vrmlname = node.getDefName()
	if not vrmlname: vrmlname = 'SpotLight'
	
	# ambientIntensity = geom.getFieldAsFloat('ambientIntensity', 0.0) # TODO
	# attenuation = geom.getFieldAsFloatTuple('attenuation', (1.0, 0.0, 0.0)) # TODO
	beamWidth = node.getFieldAsFloat('beamWidth', 1.570796) * RAD_TO_DEG # max is documented to be 1.0 but some files have higher.
	color = node.getFieldAsFloatTuple('color', (1.0, 1.0, 1.0))
	cutOffAngle = node.getFieldAsFloat('cutOffAngle', 0.785398) * RAD_TO_DEG # max is documented to be 1.0 but some files have higher.
	direction = node.getFieldAsFloatTuple('direction', (0.0, 0.0, -1.0))
	intensity = node.getFieldAsFloat('intensity', 1.0) # max is documented to be 1.0 but some files have higher.
	location = node.getFieldAsFloatTuple('location', (0.0, 0.0, 0.0))
	# is_on = node.getFieldAsBool('on', True) # TODO
	radius = node.getFieldAsFloat('radius', 100.0)
	
	bpylamp = bpy.data.lamps.new(vrmlname)
	bpylamp.setType('Spot')
	bpylamp.energy = intensity
	bpylamp.dist = radius
	bpylamp.col = color
	bpylamp.spotSize = cutOffAngle
	if beamWidth > cutOffAngle:
		bpylamp.spotBlend = 0.0
	else:
		if cutOffAngle==0.0: #@#$%^&*(!!! - this should never happen
			bpylamp.spotBlend = 0.5
		else:
			bpylamp.spotBlend = beamWidth / cutOffAngle
	
	# Convert 
	
	# lamps have their direction as -z, y==up
	mtx = TranslationMatrix(Vector(location)) * Vector(direction).toTrackQuat('-z', 'y').toMatrix().resize4x4()
	
	return bpylamp, mtx


def importLamp(node, spec, ancestry):
	if spec=='PointLight':
		bpylamp,mtx = importLamp_PointLight(node)
	elif spec=='DirectionalLight':
		bpylamp,mtx = importLamp_DirectionalLight(node)
	elif spec=='SpotLight':
		bpylamp,mtx = importLamp_SpotLight(node)
	else:
		print "Error, not a lamp"
		raise ""
	
	bpyob = node.blendObject = bpy.data.scenes.active.objects.new(bpylamp)
	bpyob.setMatrix( getFinalMatrix(node, mtx, ancestry) )


def importViewpoint(node, ancestry):
	name = node.getDefName()
	if not name: name = 'Viewpoint'
	
	fieldOfView = node.getFieldAsFloat('fieldOfView', 0.785398) * RAD_TO_DEG # max is documented to be 1.0 but some files have higher.
	# jump = node.getFieldAsBool('jump', True)
	orientation = node.getFieldAsFloatTuple('orientation', (0.0, 0.0, 1.0, 0.0))
	position = node.getFieldAsFloatTuple('position', (0.0, 0.0, 10.0))
	description = node.getFieldAsString('description', '')
	
	bpycam = bpy.data.cameras.new(name)
	
	bpycam.angle = fieldOfView
	
	mtx = TranslationMatrix(Vector(position)) * translateRotation(orientation) * MATRIX_Z_TO_Y
	
	
	bpyob = node.blendObject = bpy.data.scenes.active.objects.new(bpycam)
	bpyob.setMatrix( getFinalMatrix(node, mtx, ancestry) )


def importTransform(node, ancestry):
	name = node.getDefName()
	if not name: name = 'Transform'
	
	bpyob = node.blendObject = bpy.data.scenes.active.objects.new('Empty', name) # , name)
	bpyob.setMatrix( getFinalMatrix(node, None, ancestry) )


def load_web3d(path, PREF_FLAT=False, PREF_CIRCLE_DIV=16):
	
	# Used when adding blender primitives
	GLOBALS['CIRCLE_DETAIL'] = PREF_CIRCLE_DIV
	
	#root_node = vrml_parse('/_Cylinder.wrl')
	if path.lower().endswith('.x3d'):
		root_node, msg = x3d_parse(path)
	else:
		root_node, msg = vrml_parse(path)
	
	if not root_node:
		if Blender.mode == 'background':
			print msg
		else:
			Blender.Draw.PupMenu(msg)
		return
	
	
	# fill with tuples - (node, [parents-parent, parent])
	all_nodes = root_node.getSerialized([], [])
	
	for node, ancestry in all_nodes:
		#if 'castle.wrl' not in node.getFilename():
		#	continue
		
		spec = node.getSpec()
		if spec=='Shape':
			importShape(node, ancestry)
		elif spec in ('PointLight', 'DirectionalLight', 'SpotLight'):
			importLamp(node, spec, ancestry)
		elif spec=='Viewpoint':
			importViewpoint(node, ancestry)
		elif spec=='Transform':
			# Only use transform nodes when we are not importing a flat object hierarchy
			if PREF_FLAT==False:
				importTransform(node, ancestry)
	
	# Add in hierarchy
	if PREF_FLAT==False:
		child_dict = {}
		for node, ancestry in all_nodes:
			if node.blendObject:
				blendObject = None
				
				# Get the last parent
				i = len(ancestry)
				while i:
					i-=1
					blendObject = ancestry[i].blendObject
					if blendObject:
						break
				
				if blendObject:
					# Parent Slow, - 1 liner but works
					# blendObject.makeParent([node.blendObject], 0, 1)
					
					# Parent FAST
					try:	child_dict[blendObject].append(node.blendObject)
					except:	child_dict[blendObject] = [node.blendObject]
		
		# Parent FAST
		for parent, children in child_dict.iteritems():
			parent.makeParent(children, 0, 1)
		
		# update deps
		bpy.data.scenes.active.update(1)
		del child_dict


def load_ui(path):
	Draw = Blender.Draw
	PREF_HIERARCHY= Draw.Create(0)
	PREF_CIRCLE_DIV= Draw.Create(16)
	
	# Get USER Options
	pup_block= [\
	'Import...',\
	('Hierarchy', PREF_HIERARCHY, 'Import transform nodes as empties to create a parent/child hierarchy'),\
	('Circle Div:', PREF_CIRCLE_DIV, 3, 128, 'Number of divisions to use for circular primitives')
	]
	
	if not Draw.PupBlock('Import X3D/VRML...', pup_block):
		return
	
	Window.WaitCursor(1)
	
	load_web3d(path,\
	  (not PREF_HIERARCHY.val),\
	  PREF_CIRCLE_DIV.val,\
	)
	
	Window.WaitCursor(0)
	

if __name__ == '__main__':
	Window.FileSelector(load_ui, 'Import X3D/VRML97')
	
	
# Testing stuff

# load_web3d('/test.x3d')
# load_web3d('/_Cylinder.x3d')

# Testing below
# load_web3d('m:\\root\\Desktop\\_Cylinder.wrl')
# load_web3d('/_Cylinder.wrl')
# load_web3d('/fe/wrl/Vrml/EGS/BCKGD.WRL')

# load_web3d('/fe/wrl/Vrml/EGS/GRNDPLNE.WRL')
# load_web3d('/fe/wrl/Vrml/EGS/INDEXFST.WRL')
# load_web3d('/fe/wrl/panel1c.wrl')
# load_web3d('/test.wrl')
# load_web3d('/fe/wrl/dulcimer.wrl')
# load_web3d('/fe/wrl/rccad/Ju-52.wrl') # Face index out of range
# load_web3d('/fe/wrl/16lat.wrl') # spotlight
# load_web3d('/fe/wrl/Vrml/EGS/FOG.WRL') # spotlight
# load_web3d('/fe/wrl/Vrml/EGS/LOD.WRL') # vcolor per face

# load_web3d('/fe/wrl/new/daybreak_final.wrl') # no faces in mesh, face duplicate error
# load_web3d('/fe/wrl/new/earth.wrl')
# load_web3d('/fe/wrl/new/hendrix.ei.dtu.dk/vrml/talairach/fourd/TalaDruryRight.wrl') # define/use fields
# load_web3d('/fe/wrl/new/imac.wrl') # extrusion and define/use fields, face index is a float somehow
# load_web3d('/fe/wrl/new/www.igs.net/~mascott/vrml/vrml2/mcastle.wrl') 
# load_web3d('/fe/wrl/new/www.igs.net/~mascott/vrml/vrml2/tower.wrl') 
# load_web3d('/fe/wrl/new/www.igs.net/~mascott/vrml/vrml2/temple.wrl') 
# load_web3d('/fe/wrl/brain.wrl')  # field define test 'a IS b'
# load_web3d('/fe/wrl/new/coaster.wrl')  # fields that are confusing to read.

# X3D 

# load_web3d('/fe/x3d/www.web3d.org/x3d/content/examples/Basic/StudentProjects/PlayRoom.x3d') # invalid UVs

'''
import os
# files = os.popen('find /fe/wrl -iname "*.wrl"').readlines()
# files = os.popen('find /fe/x3d -iname "*.x3d"').readlines()
files = os.popen('find   /fe/x3d/X3dExamplesSavage   -iname "*.x3d"').readlines()

files.sort()
tot = len(files)
for i, f in enumerate(files):
	if i < 12803 or i > 1000000:
		continue
	#if i != 12686:
	#	continue
	
	f = f.strip()
	print f, i, tot
	sce = bpy.data.scenes.new(f.split('/')[-1])
	bpy.data.scenes.active = sce
	# Window.
	load_web3d(f, PREF_FLAT=True)
'''