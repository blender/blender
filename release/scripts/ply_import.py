#!BPY

"""
Name: 'Stanford PLY (*.ply)...'
Blender: 241
Group: 'Import'
Tip: 'Import a Stanford PLY file'
"""

__author__ = 'Bruce Merry'
__version__ = '0.92'
__bpydoc__ = """\
This script imports Stanford PLY files into Blender. It supports per-vertex
normals, and per-face colours and texture coordinates.

Usage:

Run this script from "File->Import" and select the desired PLY file.
"""

# Copyright (C) 2004, 2005: Bruce Merry, bmerry@cs.uct.ac.za
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


# Updated by Campbell Barton AKA Ideasman, 10% faster code.

# Portions of this code are taken from mod_meshtools.py in Blender
# 2.32.

import Blender
try:
	import re, struct, StringIO
except:
	struct= None

class element_spec:
	name = ''
	count = 0
	def __init__(self, name, count):
		self.name = name
		self.count = count
		self.properties = []

	def load(self, format, stream):
		if format == 'ascii':
			stream = re.split('\s+', stream.readline())
		return map(lambda x: x.load(format, stream), self.properties)

	def index(self, name):
		for i, p in enumerate(self.properties):
			if p.name == name: return i
		return -1

class property_spec:
	name = ''
	list_type = ''
	numeric_type = ''
	def __init__(self, name, list_type, numeric_type):
		self.name = name
		self.list_type = list_type
		self.numeric_type = numeric_type

	def read_format(self, format, count, num_type, stream):
		if format == 'ascii':
			if (num_type == 's'):
				ans = []
				for i in xrange(count):
					s = stream[i]
					if len(s) < 2 or s[0] != '"' or s[-1] != '"':
						print 'Invalid string', s
						print 'Note: ply_import.py does not handle whitespace in strings'
						return None
					ans.append(s[1:-1])
				stream[:count] = []
				return ans
			if (num_type == 'f' or num_type == 'd'):
				mapper = float
			else:
				mapper = int
			ans = map(lambda x: mapper(x), stream[:count])
			stream[:count] = []
			return ans
		else:
			if (num_type == 's'):
				ans = []
				for i in xrange(count):
					fmt = format + 'i'
					data = stream.read(struct.calcsize(fmt))
					length = struct.unpack(fmt, data)[0]
					fmt = '%s%is' % (format, length)
					data = stream.read(struct.calcsize(fmt))
					s = struct.unpack(fmt, data)[0]
					ans.append(s[:-1]) # strip the NULL
				return ans
			else:
				fmt = '%s%i%s' % (format, count, num_type)
				data = stream.read(struct.calcsize(fmt));
				return struct.unpack(fmt, data)

	def load(self, format, stream):
		if (self.list_type != None):
			count = int(self.read_format(format, 1, self.list_type, stream)[0])
			return self.read_format(format, count, self.numeric_type, stream)
		else:
			return self.read_format(format, 1, self.numeric_type, stream)[0]

class object_spec:
	'A list of element_specs'
	specs = []

	def load(self, format, stream):
		return dict([(i.name,[i.load(format, stream) for j in xrange(i.count) ]) for i in self.specs])
		
		'''
		answer = {}
		for i in self.specs:
			answer[i.name] = []
			for j in xrange(i.count):
				if not j % 100 and meshtools.show_progress:
					Blender.Window.DrawProgressBar(float(j) / i.count, 'Loading ' + i.name)
				answer[i.name].append(i.load(format, stream))
		return answer
			'''
		

def read(filename):
	format = ''
	version = '1.0'
	format_specs = {'binary_little_endian': '<',
			'binary_big_endian': '>',
			'ascii': 'ascii'}
	type_specs = {'char': 'b',
		      'uchar': 'B',
		      'int8': 'b',
		      'uint8': 'B',
		      'int16': 'h',
		      'uint16': 'H',
		      'int': 'i',
		      'int32': 'i',
		      'uint': 'I',
		      'uint32': 'I',
		      'float': 'f',
		      'float32': 'f',
		      'float64': 'd',
		      'string': 's'}
	obj_spec = object_spec()

	try:
		file = open(filename, 'rb')
		signature = file.readline()
		if (signature != 'ply\n'):
			print 'Signature line was invalid'
			return None
		while 1:
			tokens = re.split(r'[ \n]+', file.readline())
			if (len(tokens) == 0):
				continue
			if (tokens[0] == 'end_header'):
				break
			elif (tokens[0] == 'comment' or tokens[0] == 'obj_info'):
				continue
			elif (tokens[0] == 'format'):
				if (len(tokens) < 3):
					print 'Invalid format line'
					return None
				if (tokens[1] not in format_specs.keys()):
					print 'Unknown format', tokens[1]
					return None
				if (tokens[2] != version):
					print 'Unknown version', tokens[2]
					return None
				format = tokens[1]
			elif (tokens[0] == 'element'):
				if (len(tokens) < 3):
					print 'Invalid element line'
					return None
				obj_spec.specs.append(element_spec(tokens[1], int(tokens[2])))
			elif (tokens[0] == 'property'):
				if (not len(obj_spec.specs)):
					print 'Property without element'
					return None
				if (tokens[1] == 'list'):
					obj_spec.specs[-1].properties.append(property_spec(tokens[4], type_specs[tokens[2]], type_specs[tokens[3]]))
				else:
					obj_spec.specs[-1].properties.append(property_spec(tokens[2], None, type_specs[tokens[1]]))
		obj = obj_spec.load(format_specs[format], file)

	except IOError, (errno, strerror):
		try:	file.close()
		except:	pass
		
		return None

	try:	file.close()
	except:	pass
	
	return (obj_spec, obj);


def add_face(vertices, varr, indices, uvindices, colindices):
	face = Blender.NMesh.Face([varr[i] for i in indices])
	for index in indices:
		vertex = vertices[index];
		
		if uvindices:
			face.uv.append((vertex[uvindices[0]], 1.0 - vertex[uvindices[1]]))
			face.mode &= ~Blender.NMesh.FaceModes.TEX
		if colindices:
			if not uvindices: face.uv.append((0, 0)) # Force faceUV
			face.col.append(Blender.NMesh.Col(vertex[colindices[0]], vertex[colindices[1]], vertex[colindices[2]], 255))
			face.mode &= ~Blender.NMesh.FaceModes.TEX
	return face

def filesel_callback(filename):
	t = Blender.sys.time()
	(obj_spec, obj) = read(filename)
	if obj == None:
		print 'Invalid file'
		return
	vmap = {}
	varr = []
	uvindices = None
	noindices = None
	colindices = None
	for el in obj_spec.specs:
		if el.name == 'vertex':
			vindices = vindices_x, vindices_y, vindices_z = (el.index('x'), el.index('y'), el.index('z'))
			if el.index('nx') >= 0 and el.index('ny') >= 0 and el.index('nz') >= 0:
				noindices = (el.index('nx'), el.index('ny'), el.index('nz'))
			if el.index('s') >= 0 and el.index('t') >= 0:
				uvindices = (el.index('s'), el.index('t'))
			if el.index('red') >= 0 and el.index('green') and el.index('blue') >= 0:
				colindices = (el.index('red'), el.index('green'), el.index('blue'))
		elif el.name == 'face':
			findex = el.index('vertex_indices')
		

	mesh = Blender.NMesh.GetRaw()
	NMVert = Blender.NMesh.Vert
	for v in obj['vertex']:
		
		if noindices > 0:
			x,y,z,nx,ny,nz = vkey =\
			(v[vindices_x], v[vindices_y], v[vindices_z],\
			v[noindices[0]], v[noindices[1]], v[noindices[2]])
		else:
			x,y,z = vkey = (v[vindices_x], v[vindices_y], v[vindices_z])
		#if not vmap.has_key(vkey):
		try: # try uses 1 less dict lookup
			varr.append(vmap[vkey])
		except:
			nmv = NMVert(vkey[0], vkey[1], vkey[2])
			mesh.verts.append(nmv)
			if noindices > 0:
				nmv.no[0] = vkey[3]
				nmv.no[1] = vkey[4]
				nmv.no[2] = vkey[5]
			vmap[vkey] = nmv
			varr.append(vmap[vkey])
	
	verts = obj['vertex']
	
	if 'face' in obj:
		for f in obj['face']:
			ind = f[findex]
			nind = len(ind)
			if nind <= 4:
				mesh.faces.append(add_face(verts, varr, ind, uvindices, colindices))
			else:
				for j in xrange(nind - 2):
					mesh.faces.append(add_face(verts, varr, (ind[0], ind[j + 1], ind[j + 2]), uvindices, colindices))

	
	del obj # Reclaim memory
	
	'''
	if noindices:
		normals = 1
	else:
		normals = 0
	'''
	
	objname = Blender.sys.splitext(Blender.sys.basename(filename))[0]
	scn= Blender.Scene.GetCurrent()
	scn.objects.selected = []
	
	mesh.name= objname
	scn.objects.new(mesh)
	
	Blender.Redraw()
	Blender.Window.DrawProgressBar(1.0, '')
	print '\nSuccessfully imported ' + Blender.sys.basename(filename) + ' ' + str(Blender.sys.time()-t)
	



def main():
	if not struct:
		Blender.Draw.PupMenu('This importer requires a full python install')
		return
	
	Blender.Window.FileSelector(filesel_callback, 'Import PLY', '*.ply')

if __name__=='__main__':
	main()





