#!BPY
""" Registration info for Blender menus:
Name: 'OpenFlight (.flt)...'
Blender: 245
Group: 'Import'
Tip: 'Import OpenFlight (.flt)'
"""



__author__ = "Greg MacDonald, Campbell Barton, Geoffrey Bantle"
__version__ = "2.0 11/21/07"
__url__ = ("blender", "blenderartists.org", "Author's homepage, http://sourceforge.net/projects/blight/")
__bpydoc__ = """\
This script imports OpenFlight files into Blender. OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.

Feature overview and more availible at:
http://wiki.blender.org/index.php/Scripts/Manual/Import/openflight_flt

Note: This file is a grab-bag of old and new code. It needs some cleanup still.
"""

# flt_import.py is an OpenFlight importer for blender.
# Copyright (C) 2005 Greg MacDonald, 2007  Blender Foundation
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

import Blender
import os
import BPyMesh
import BPyImage
import flt_filewalker 
import flt_properties
reload(flt_properties)
from flt_properties import *

#Globals. Should Clean these up and minimize their usage.

typecodes = ['c','C','s','S','i','I','f','d','t']
records = dict()

FLTBaseLabel = None
FLTBaseString = None
FLTBaseChooser = None
FLTExport = None
FLTClose = None
FLTDoXRef = None
FLTScale = None
FLTShadeImport = None
FLTAttrib = None
FLTWarn = None

Vector= Blender.Mathutils.Vector
FLOAT_TOLERANCE = 0.01

FF = flt_filewalker.FileFinder()
current_layer = 0x01

global_prefs = dict()
global_prefs['verbose']= 4
global_prefs['get_texture'] = True
global_prefs['get_diffuse'] = True
global_prefs['get_specular'] = False
global_prefs['get_emissive'] = False
global_prefs['get_alpha'] = True
global_prefs['get_ambient'] = False
global_prefs['get_shininess'] = True
global_prefs['color_from_face'] = True
global_prefs['fltfile']= ''
global_prefs['smoothshading'] = 1
global_prefs['doxrefs'] = 1
global_prefs['scale'] = 1.0
global_prefs['attrib'] = 0
msg_once = False

reg = Blender.Registry.GetKey('flt_import',1)
if reg:
	for key in global_prefs:
		if reg.has_key(key):
			global_prefs[key] = reg[key]
		


throw_back_opcodes = [2, 73, 4, 11, 96, 14, 91, 98, 63,111] # Opcodes that indicate its time to return control to parent.
do_not_report_opcodes = [76, 78, 79, 80, 81, 82, 94, 83, 33, 112, 101, 102, 97, 31, 103, 104, 117, 118, 120, 121, 124, 125]

#Process FLT record definitions
for record in FLT_Records:
	props = dict()
	for prop in FLT_Records[record]:
		position = ''
		slice = 0
		(format,name) = prop.split('!')
		for i in format:
			if i not in typecodes:
				position = position + i
				slice = slice + 1
			else:
				break
		type = format[slice:]
		length = type[1:] 
		if len(length) == 0:
			length = 1
		else:
			type = type[0]
			length = int(length)
		
		props[int(position)] = (type,length,prop)
	records[record] = props

def col_to_gray(c):
	return 0.3*c[0] + 0.59*c[1] + 0.11*c[2]
class MaterialDesc:
	# Was going to use int(f*1000.0) instead of round(f,3), but for some reason
	# round produces better results, as in less dups.
	def make_key(self):
		key = list()
		if global_prefs['get_texture']:
			if self.tex0:
				key.append(self.tex0.getName())
			else:
				key.append(None)
		
		if global_prefs['get_alpha']:
			key.append(round(self.alpha, 3))
		else:
			key.append(None)
			
		if global_prefs['get_shininess']:
			key.append(round(self.shininess, 3))
		else:
			key.append(None)
		
		if global_prefs['get_emissive']:
			key.append(round(self.emissive, 3))
		else:
			key.append(None)
		
		if global_prefs['get_ambient']:
			key.append(round(self.ambient, 3))
		else:
			key.append(None)
		
		if global_prefs['get_specular']:
			for n in self.specular:
				key.append(round(n, 3))
		else:
			key.extend([None, None, None])
		
		if global_prefs['get_diffuse']:
			for n in self.diffuse:
				key.append(round(n, 3))
		else:
			key.extend([None, None, None])
		
#        key.extend(self.face_props.values())
		
		return tuple(key)

	def __init__(self):
		self.name = 'Material'
		# Colors, List of 3 floats.
		self.diffuse = [1.0, 1.0, 1.0]
		self.specular = [1.0, 1.0, 1.0]

		# Scalars
		self.ambient = 0.0 # [0.0, 1.0]
		self.emissive = 0.0 # [0.0, 1.0]
		self.shininess = 0.5 # Range is [0.0, 2.0]
		self.alpha = 1.0 # Range is [0.0, 1.0]

		self.tex0 = None
		
		# OpenFlight Face attributes
		self.face_props = dict.fromkeys(['comment', 'ir color', 'priority', 
							'draw type', 'texture white', 'template billboard',
							'smc', 'fid', 'ir material', 'lod generation control',
							'flags', 'light mode'])

class VertexDesc:
	def make_key(self):
		return round(self.x, 6), round(self.y, 6), round(self.z, 6)
		
	def __init__(self):
		
		# Assign later, save memory, all verts have a loc
		self.x = 0.0
		self.y = 0.0
		self.z = 0.0
		
		
		self.nx = 0.0
		self.ny = 0.0
		self.nz = 0.0
		
		self.uv= Vector(0,0)
		self.cindex = 127 #default/lowest
		self.cnorm = False        

class LightPointAppDesc:
	def make_key(self):
		d = dict(self.props)
		del d['id']
		del d['type']
		
		if d['directionality'] != 0: # not omni
			d['nx'] = 0.0
			d['ny'] = 0.0
			d['nz'] = 0.0
		
		return tuple(d.values())
		
	def __init__(self):
		self.props = dict()
		self.props.update({'type': 'LPA'})
		self.props.update({'id': 'ap'})
		# Attribs not found in inline lightpoint.
		self.props.update({'visibility range': 0.0})
		self.props.update({'fade range ratio': 0.0})
		self.props.update({'fade in duration': 0.0})
		self.props.update({'fade out duration': 0.0})
		self.props.update({'LOD range ratio': 0.0})
		self.props.update({'LOD scale': 0.0})

class GlobalResourceRepository:
	def request_lightpoint_app(self, desc, scene):
		match = self.light_point_app.get(desc.make_key())
		
		if match:
			return match.getName()
		else:
			# Create empty and fill with properties.
			name = desc.props['type'] + ': ' + desc.props['id']
			object = Blender.Object.New('Empty', name)
			scene.objects.link(object)
			object.Layers= current_layer
			object.sel= 1
			
			# Attach properties
			for name, value in desc.props.iteritems():
				object.addProperty(name, value)
			
			self.light_point_app.update({desc.make_key(): object})
			
			return object.getName()
	
	# Dont use request_vert - faster to make it from the vector direct.
	"""
	def request_vert(self, desc):
		match = self.vert_dict.get(desc.make_key())

		if match:
			return match
		else:
			vert = Blender.Mathutils.Vector(desc.x, desc.y, desc.z)
			''' IGNORE_NORMALS
			vert.no[0] = desc.nx
			vert.no[1] = desc.ny
			vert.no[2] = desc.nz
			'''
			self.vert_dict.update({desc.make_key(): vert})
			return vert
	"""
	def request_mat(self, mat_desc):
		match = self.mat_dict.get(mat_desc.make_key())
		if match: return match
		
		mat = Blender.Material.New(mat_desc.name)

		if mat_desc.tex0 != None:
			mat.setTexture(0, mat_desc.tex0, Blender.Texture.TexCo.UV)

		mat.setAlpha(mat_desc.alpha)
		mat.setSpec(mat_desc.shininess)
		mat.setHardness(255)
		mat.setEmit(mat_desc.emissive)
		mat.setAmb(mat_desc.ambient)
		mat.setSpecCol(mat_desc.specular)
		mat.setRGBCol(mat_desc.diffuse)
		
		# Create a text object to store openflight face attribs until
		# user properties can be set on materials.
#        t = Blender.Text.New('FACE: ' + mat.getName())
#
#        for name, value in mat_desc.face_props.items():
#            t.write(name + '\n' + str(value) + '\n\n')    
				
		self.mat_dict.update({mat_desc.make_key(): mat})

		return mat
		
	def request_image(self, filename_with_path):
		if not global_prefs['get_texture']: return None
		return BPyImage.comprehensiveImageLoad(filename_with_path, global_prefs['fltfile']) # Use join in case of spaces 
		
	def request_texture(self, image):
		if not global_prefs['get_texture']:
			return None

		tex = self.tex_dict.get(image.filename)
		if tex: return tex
		
		tex = Blender.Texture.New(Blender.sys.basename(image.filename))
		tex.setImage(image)
		tex.setType('Image')
		self.tex_dict.update({image.filename: tex})
		return tex
		
	def __init__(self):
		
		#list of scenes xrefs belong to.
		self.xrefs = dict()
		# material
		self.mat_dict = dict()
		mat_lst = Blender.Material.Get()
		for mat in mat_lst:
			mat_desc = MaterialDesc()
			mapto_lst = mat.getTextures()
			if mapto_lst[0]:
				mat_desc.tex0 = mapto_lst[0].tex
			else:
				mat_desc.tex0 = None
			mat_desc.alpha = mat.getAlpha()
			mat_desc.shininess = mat.getSpec()
			mat_desc.emissive = mat.getEmit()
			mat_desc.ambient = mat.getAmb()
			mat_desc.specular = mat.getSpecCol()
			mat_desc.diffuse = mat.getRGBCol()
			
			self.mat_dict.update({mat_desc.make_key(): mat})
			
		# texture
		self.tex_dict = dict()
		tex_lst = Blender.Texture.Get()
		
		for tex in tex_lst:
			img = tex.getImage()
			# Only interested in textures with images.
			if img:
				self.tex_dict.update({img.filename: tex})
			
		# vertex
		# self.vert_dict = dict()
		
		# light point
		self.light_point_app = dict()
		
class Handler:
	def in_throw_back_lst(self, opcode):
		return opcode in self.throw_back_lst
		
	def handle(self, opcode):
		return self.handler[opcode]()
	
	def handles(self, opcode):
		return opcode in self.handler.iterkeys()
	
	def throws_back_all_unhandled(self):
		return self.throw_back_unhandled
		
	def set_throw_back_lst(self, a):
		self.throw_back_lst = a
		
	def set_throw_back_all_unhandled(self):
		self.throw_back_unhandled = True
		
	def set_only_throw_back_specified(self):
		self.throw_back_unhandled = False
		
	def set_handler(self, d):
		self.handler = d
		
	def __init__(self):
		# Dictionary of opcodes to handler methods.
		self.handler = dict()
		# Send all opcodes not handled to the parent node.
		self.throw_back_unhandled = False
		# If throw_back_unhandled is False then only throw back
		# if the opcodes in throw_back are encountered.
		self.throw_back_lst = list()
		
class Node:
	def blender_import(self):
		if self.opcode in opcode_name and global_prefs['verbose'] >= 2:
			for i in xrange(self.get_level()):
				print ' ',
			print opcode_name[self.opcode],
			print '-', self.props['id'],
			print '-', self.props['comment'],

			print
                
		for child in self.children:
			child.blender_import()
			
# Import comment.
#        if self.props['comment'] != '':
#            name = 'COMMENT: ' + self.props['id']
#            t = Blender.Text.New(name)
#            t.write(self.props['comment'])
#            self.props['comment'] = name
		
	# Always ignore extensions and anything in between them.
	def parse_push_extension(self):
		self.saved_handler = self.active_handler
		self.active_handler = self.extension_handler
		return True
	
	def parse_pop_extension(self):
		self.active_handler = self.saved_handler
		return True
	
	def parse_push(self):
		self.header.fw.up_level()
		# Ignore unknown children.
		self.ignore_unhandled = True
		# Don't do child records that might overwrite parent info. ex: longid
		self.active_handler = self.child_handler
		return True
		
	def parse_pop(self):
		self.header.fw.down_level()
		
		if self.header.fw.get_level() == self.level:
			return False
		
		return True
	
	def parse(self):
		while self.header.fw.begin_record():
			opcode = self.header.fw.get_opcode()

			# Print out info on opcode and tree level.
			if global_prefs['verbose'] >= 3:
				p = ''
				for i in xrange(self.header.fw.get_level()):
					p = p + '  '
				if opcode in opcode_name:
					p = p + opcode_name[opcode]
				else:
					if global_prefs['verbose'] >= 1:
						print 'undocumented opcode', opcode
					continue
							
			if self.global_handler.handles(opcode):
				if global_prefs['verbose'] >= 3:
					print p + ' handled globally'
				if self.global_handler.handle(opcode) == False:
					break
					
			elif self.active_handler.handles(opcode):
				if global_prefs['verbose'] >= 4:
					print p + ' handled'
				if self.active_handler.handle(opcode) == False:
					break
					
			else:
				if self.active_handler.throws_back_all_unhandled():
					if global_prefs['verbose'] >= 3:
						print p + ' handled elsewhere'              
					self.header.fw.repeat_record()
					break

				elif self.active_handler.in_throw_back_lst(opcode):
					if global_prefs['verbose'] >= 3:
						print p + ' handled elsewhere'              
					self.header.fw.repeat_record()
					break

				else:
					if global_prefs['verbose'] >= 3:
						print p + ' ignored'
					elif global_prefs['verbose'] >= 1 and not opcode in do_not_report_opcodes and opcode in opcode_name:   
						print 'not handled'
					
	def get_level(self):
		return self.level
		
	def parse_long_id(self):
		self.props['id'] = self.header.fw.read_string(self.header.fw.get_length()-4)
		return True

	def parse_comment(self):
		self.props['comment'] = self.header.fw.read_string(self.header.fw.get_length()-4)
		return True
	
	def parse_extension(self):
		extension = dict()
		props = records[100]
		propkeys = props.keys()
		propkeys.sort()
		for position in propkeys:
			(type,length,name) = props[position]
			extension[name] = read_prop(self.header.fw,type,length)
		#read extension data.
		dstring = list()
		for i in xrange(self.header.fw.get_length()-24):
			dstring.append(self.header.fw.read_char())
		extension['data'] = dstring
		self.extension = extension
	def parse_record(self):
		self.props['type'] = self.opcode
		props = records[self.opcode]
		propkeys = props.keys()
		propkeys.sort()
		for position in propkeys:
			(type,length,name) = props[position]
			self.props[name] = read_prop(self.header.fw,type,length)
		try: #remove me!
			self.props['id'] = self.props['3t8!id']
		except:
			pass
	def __init__(self, parent, header):
		self.root_handler = Handler()
		self.child_handler = Handler()
		self.extension_handler = Handler()
		self.global_handler = Handler()
		
		self.global_handler.set_handler({21: self.parse_push_extension})
		self.active_handler = self.root_handler
		
		# used by parse_*_extension
		self.extension_handler.set_handler({22: self.parse_pop_extension})
		self.saved_handler = None
		
		self.header = header
		self.children = list()

		self.parent = parent

		if parent:
			parent.children.append(self)

		self.level = self.header.fw.get_level()
		self.opcode = self.header.fw.get_opcode()

		self.props = {'id': 'unnamed', 'comment': '', 'type': 'untyped'}

class VertexPalette(Node):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		self.root_handler.set_handler({68: self.parse_vertex_c,
									   69: self.parse_vertex_cn,
									   70: self.parse_vertex_cnuv,
									   71: self.parse_vertex_cuv})
		self.root_handler.set_throw_back_all_unhandled()

		self.vert_desc_lst = list()
		self.blender_verts = list()
		self.offset = 8
		# Used to create a map from byte offset to vertex index.
		self.index = dict()
	
	
	def blender_import(self):
		self.blender_verts.extend([Vector(vert_desc.x, vert_desc.y, vert_desc.z) for vert_desc in self.vert_desc_lst ])

	def parse_vertex_common(self):
		# Add this vertex to an offset to index dictionary.
		#self.index_lst.append( (self.offset, self.next_index) )
		self.index[self.offset]= len(self.index)
		
		# Get ready for next record.
		self.offset += self.header.fw.get_length()

		v = VertexDesc()

		self.header.fw.read_ahead(2)
		v.flags = self.header.fw.read_short()

		v.x = self.header.fw.read_double()
		v.y = self.header.fw.read_double()
		v.z = self.header.fw.read_double()

		return v

	def parse_vertex_post_common(self, v):
		#if not v.flags & 0x2000: # 0x2000 = no color
			#if v.flags & 0x1000: # 0x1000 = packed color
			#	v.a = self.header.fw.read_uchar()
			#	v.b = self.header.fw.read_uchar()
			#	v.g = self.header.fw.read_uchar()
			#	v.r = self.header.fw.read_uchar()
			#else:
		self.header.fw.read_ahead(4) #skip packed color
		v.cindex = self.header.fw.read_uint()
		self.vert_desc_lst.append(v)
		return True

	def parse_vertex_c(self):
		v = self.parse_vertex_common()

		self.parse_vertex_post_common(v)
		
		return True

	def parse_vertex_cn(self):
		v = self.parse_vertex_common()
		v.cnorm = True
		v.nx = self.header.fw.read_float()
		v.ny = self.header.fw.read_float()
		v.nz = self.header.fw.read_float()
		
		self.parse_vertex_post_common(v)
		
		return True

	def parse_vertex_cuv(self):
		v = self.parse_vertex_common()

		v.uv[:] = self.header.fw.read_float(), self.header.fw.read_float()

		self.parse_vertex_post_common(v)
		
		return True

	def parse_vertex_cnuv(self):
		v = self.parse_vertex_common()
		v.cnorm = True
		v.nx = self.header.fw.read_float()
		v.ny = self.header.fw.read_float()
		v.nz = self.header.fw.read_float()
		
		v.uv[:] = self.header.fw.read_float(), self.header.fw.read_float()

		self.parse_vertex_post_common(v)
		
		return True

	def parse(self): # Run once per import
		Node.parse(self)


class InterNode(Node):
	def __init__(self):
		self.object = None
		self.mesh = None
		self.swapmesh = None
		self.hasMesh = False
		self.faceLs= []
		self.matrix = None
		self.vis = True
		self.hasmtex = False
		self.uvlayers = dict()
		self.blayernames = dict()
		self.subfacelevel = 0
		self.extension = None
		
		mask = 2147483648
		for i in xrange(7):
			self.uvlayers[mask] = False
			mask = mask / 2
		
	#######################################################
	##              Begin Remove Doubles Replacement     ##
	#######################################################
	def __xvertsort(self,__a,__b):
		(__vert, __x1) = __a
		(__vert2,__x2) = __b
		
		if __x1 > __x2:
			return 1
		elif __x1 < __x2:
			return -1
		return 0	
	def __calcFaceNorm(self,__face):
		if len(__face) == 3:
			return Blender.Mathutils.TriangleNormal(__face[0].co, __face[1].co, __face[2].co)
		elif len(__face) == 4:
			return Blender.Mathutils.QuadNormal(__face[0].co, __face[1].co, __face[2].co, __face[3].co)
			
	def __replaceFaceVert(self,__weldface, __oldvert, __newvert):
		__index = None
		for __i, __v in enumerate(__weldface):
			if __v == __oldvert:
				__index = __i
				break
		__weldface[__index] = __newvert
	
	def __matchEdge(self,__weldmesh, __edge1, __edge2):
		if __edge1[0] in __weldmesh['Vertex Disk'][__edge2[1]] and __edge1[1] in __weldmesh['Vertex Disk'][__edge2[0]]:
			return True
		return False
	#have to compare original faces!
	def __faceWinding(self, __weldmesh, __face1, __face2):
		
		__f1edges = list()
		__f2edges = list()
		
		__f1edges.append((__face1.verts[0], __face1.verts[1]))
		__f1edges.append((__face1.verts[1], __face1.verts[2]))
		if len(__face1.verts) == 3:
			__f1edges.append((__face1.verts[2], __face1.verts[0]))
		else:
			__f1edges.append((__face1.verts[2], __face1.verts[3]))
			__f1edges.append((__face1.verts[3], __face1.verts[0]))

		__f2edges.append((__face2.verts[0], __face2.verts[1]))
		__f2edges.append((__face2.verts[1], __face2.verts[2]))
		if len(__face2.verts) == 3:
			__f2edges.append((__face2.verts[2], __face2.verts[0]))
		else:
			__f2edges.append((__face2.verts[2], __face2.verts[3]))
			__f2edges.append((__face2.verts[3], __face2.verts[0]))

			
		#find a matching edge
		for __edge1 in __f1edges:
			for __edge2 in __f2edges:
				if self.__matchEdge(__weldmesh, __edge1, __edge2): #no more tests nessecary
					return True
		
		return False
		
	def __floatcompare(self, __f1, __f2):
		epsilon = 0.1
		if ((__f1 + epsilon) > __f2) and ((__f1 - epsilon) < __f2):
			return True
		return False
	def __testFace(self,__weldmesh,__v1face, __v2face, __v1bface, __v2bface):
		limit = 0.01
		__matchvert = None
		#frst test (for real this time!). Are the faces the same face?
		if __v1face == __v2face:
			return False
		
		#first test: Do the faces possibly geometrically share more than two vertices? we should be comparing original faces for this? - Yes.....
		__match = 0
		for __vert in __v1bface.verts:
			for __vert2 in __v2bface.verts:
				#if (abs(__vert.co[0] - __vert2.co[0]) <= limit) and (abs(__vert.co[1] - __vert2.co[1]) <= limit) and (abs(__vert.co[2] - __vert2.co[2]) <= limit): #this needs to be fixed!
				if __vert2 in __weldmesh['Vertex Disk'][__vert] or __vert == __vert2:
					__match += 1
					__matchvert = __vert2
		#avoid faces sharing more than two verts
		if __match > 2:
			return False

		#consistent winding for face normals
		if __match == 2:
			if not self.__faceWinding(__weldmesh, __v1bface, __v2bface):
				return False

		#second test: Compatible normals.Anything beyond almost exact opposite is 'ok'
		__v1facenorm = self.__calcFaceNorm(__v1face)
		__v2facenorm = self.__calcFaceNorm(__v2face)

		#dont even mess with zero length faces
		if __v1facenorm.length < limit:
			return False
		if __v2facenorm.length < limit:
			return False

		__v1facenorm.normalize()
		__v2facenorm.normalize()

		if __match == 1:
			#special case, look for comparison of normals angle
			__angle = Blender.Mathutils.AngleBetweenVecs(__v1facenorm, __v2facenorm)
			if __angle > 70.0:
				return False	



		__v2facenorm = __v2facenorm.negate()

		if self.__floatcompare(__v1facenorm[0], __v2facenorm[0]) and self.__floatcompare(__v1facenorm[1], __v2facenorm[1]) and self.__floatcompare(__v1facenorm[2], __v2facenorm[2]):
			return False

		#next test: dont weld a subface to a non-subface!
		if __v1bface.getProperty("FLT_SFLEVEL") != __v2bface.getProperty("FLT_SFLEVEL"):
			return False	
			
		#final test: edge test - We dont want to create a non-manifold edge through our weld operation	
	
		return True

	def __copyFaceData(self, __source, __target):
		#copy vcolor layers.
		__actColLayer = self.mesh.activeColorLayer
		for __colorlayer in self.mesh.getColorLayerNames():
			self.mesh.activeColorLayer = __colorlayer
			for __i, __col in enumerate(__source.col):
				__target.col[__i].r = __col.r
				__target.col[__i].g = __col.g
				__target.col[__i].b = __col.b			
			
		self.mesh.activeColorLayer = __actColLayer
		#copy uv layers.
		__actUVLayer = self.mesh.activeUVLayer
		for __uvlayer in self.mesh.getUVLayerNames():
			self.mesh.activeUVLayer = __uvlayer
			__target.image = __source.image
			__target.mode = __source.mode
			__target.smooth = __source.smooth
			__target.transp = __source.transp
			for __i, __uv in enumerate(__source.uv):
				__target.uv[__i][0] = __uv[0]
				__target.uv[__i][1] = __uv[1]
			
		self.mesh.activeUVLayer = __actUVLayer
		#copy property layers
		for __property in self.mesh.faces.properties:
			__target.setProperty(__property, __source.getProperty(__property))	

	def findDoubles(self):
		limit = 0.01
		sortblock = list()
		double = dict()
		for vert in self.mesh.verts:
			double[vert] = None
			sortblock.append((vert, vert.co[0] + vert.co[1] + vert.co[2]))
		sortblock.sort(self.__xvertsort)
		
		a = 0
		while a < len(self.mesh.verts):
			(vert,xsort) = sortblock[a]
			b = a+1
			if not double[vert]:
				while b < len(self.mesh.verts):
					(vert2, xsort2) = sortblock[b]
					if not double[vert2]:
						#first test, simple distance
						if (xsort2 - xsort) > limit: 
							break
						#second test, more expensive
						if (abs(vert.co[0] - vert2.co[0]) <= limit) and (abs(vert.co[1] - vert2.co[1]) <= limit) and (abs(vert.co[2] - vert2.co[2]) <= limit):
							double[vert2] = vert
					b+=1				
			a+=1
	
		return double

	def buildWeldMesh(self):
		
		weldmesh = dict()
		weldmesh['Vertex Disk'] = dict() #this is geometric adjacency
		weldmesh['Vertex Faces'] = dict() #topological adjacency
		
		#find the doubles for this mesh
		double = self.findDoubles()
		
		for vert in self.mesh.verts:
			weldmesh['Vertex Faces'][vert] = list()
	
		#create weld faces	
		weldfaces = list()
		originalfaces = list()
		for face in self.mesh.faces:
			weldface = list()
			for vert in face.verts:
				weldface.append(vert)
			weldfaces.append(weldface)
			originalfaces.append(face)
		for i, weldface in enumerate(weldfaces):
			for vert in weldface:
				weldmesh['Vertex Faces'][vert].append(i)
		weldmesh['Weld Faces'] = weldfaces
		weldmesh['Original Faces'] = originalfaces
		
		#Now we need to build the vertex disk data. first we do just the 'target' vertices
		for vert in self.mesh.verts:
			if not double[vert]: #its a target
				weldmesh['Vertex Disk'][vert] = list()
		for vert in self.mesh.verts:
			if double[vert]: #its a double
				weldmesh['Vertex Disk'][double[vert]].append(vert)
				
		#Now we need to create the disk information for the remaining vertices
		targets = weldmesh['Vertex Disk'].keys()
		for target in targets:
			for doublevert in weldmesh['Vertex Disk'][target]:
				weldmesh['Vertex Disk'][doublevert] = [target]
				for othervert in weldmesh['Vertex Disk'][target]:
					if othervert != doublevert:
						weldmesh['Vertex Disk'][doublevert].append(othervert) 		
		
		return weldmesh 		

	def weldFuseFaces(self,weldmesh):

		#retain original loose vertices
		looseverts = dict()
		for vert in self.mesh.verts:
			looseverts[vert] = 0
		for edge in self.mesh.edges:
			looseverts[edge.v1] += 1
			looseverts[edge.v2] += 1



		#slight modification here: we need to walk around the mesh as many times as it takes to have no more matches
		done = 0
		while not done:
			done = 1
			for windex, weldface in enumerate(weldmesh['Weld Faces']):
				for vertex in weldface:
					#we walk around the faces of the doubles of this vertex and if possible, we weld them.
					for doublevert in weldmesh['Vertex Disk'][vertex]:
						removeFaces = list() #list of faces to remove from doubleverts face list
						for doublefaceindex in weldmesh['Vertex Faces'][doublevert]:
							doubleface = weldmesh['Weld Faces'][doublefaceindex]
							oface1 = self.mesh.faces[windex]
							oface2 = self.mesh.faces[doublefaceindex]
							ok = self.__testFace(weldmesh, weldface, doubleface, oface1, oface2)
							if ok:
								done = 0
								removeFaces.append(doublefaceindex)
								self.__replaceFaceVert(doubleface, doublevert, vertex)
						for doublefaceindex in removeFaces:
							weldmesh['Vertex Faces'][doublevert].remove(doublefaceindex)
		#old faces first
		oldindices = list()
		for face in self.mesh.faces:
			oldindices.append(face.index)
		#make our new faces.
		newfaces = list()
		for weldface in weldmesh['Weld Faces']:
			newfaces.append(weldface)
		newindices = self.mesh.faces.extend(newfaces, indexList=True, ignoreDups=True)
		#copy custom data over
		for i, newindex in enumerate(newindices):
			try:
				self.__copyFaceData(self.mesh.faces[oldindices[i]], self.mesh.faces[newindex])
			except:
				print "warning, could not copy face data!"
		#delete the old faces
		self.mesh.faces.delete(1, oldindices)
		
		#Clean up stray vertices
		vertuse = dict()
		for vert in self.mesh.verts:
			vertuse[vert] = 0
		for face in self.mesh.faces:
			for vert in face.verts:
				vertuse[vert] += 1
		delverts = list()
		for vert in self.mesh.verts:
			if not vertuse[vert] and vert.index != 0 and looseverts[vert]:
				delverts.append(vert)
		
		self.mesh.verts.delete(delverts)	


	#######################################################
	##             End Remove Doubles Replacement        ##
	#######################################################

	def blender_import_my_faces(self):

		# Add the verts onto the mesh
		blender_verts= self.header.vert_pal.blender_verts
		vert_desc_lst= self.header.vert_pal.vert_desc_lst
		
		vert_list= [ i for flt_face in self.faceLs for i in flt_face.indices] #splitting faces apart. Is this a good thing?
		face_edges= []
		face_verts= []
		self.mesh.verts.extend([blender_verts[i] for i in vert_list])
		
		new_faces= []
		new_faces_props= []
		ngon= BPyMesh.ngon
		vert_index= 1
		
		#add vertex color layer for baked face colors.
		self.mesh.addColorLayer("FLT_Fcol")
		self.mesh.activeColorLayer = "FLT_Fcol"
		
		FLT_OrigIndex = 0
		for flt_face in self.faceLs:
			if flt_face.tex_index != -1:
				try:
					image= self.header.tex_pal[flt_face.tex_index][1]
				except KeyError:
					image= None
			else:
				image= None
			face_len= len(flt_face.indices)
			
			#create dummy uvert dicts
			if len(flt_face.uverts) == 0:
				for i in xrange(face_len):
					flt_face.uverts.append(dict())
			#May need to patch up MTex info
			if self.hasmtex:
				#For every layer in mesh, there should be corresponding layer in the face
				for mask in self.uvlayers.keys():
					if self.uvlayers[mask]:
						if not flt_face.uvlayers.has_key(mask): #Does the face have this layer?
							#Create Layer info for this face
							flt_face.uvlayers[mask] = dict()
							flt_face.uvlayers[mask]['texture index'] = -1
							flt_face.uvlayers[mask]['texture enviorment'] = 3
							flt_face.uvlayers[mask]['texture mapping'] = 0
							flt_face.uvlayers[mask]['texture data'] = 0
							
							#now go through and create dummy uvs for this layer
							for uvert in flt_face.uverts:
									uv = Vector(0.0,0.0)
									uvert[mask] = uv

			# Get the indicies in reference to the mesh.
			uvs= [vert_desc_lst[j].uv for j in flt_face.indices]
			if face_len == 1:
				pass
			elif face_len == 2:
				face_edges.append((vert_index, vert_index+1))
			elif flt_face.props['draw type'] == 2 or flt_face.props['draw type'] == 3:
				i = 0
				while i < (face_len-1):
					face_edges.append((vert_index + i, vert_index + i + 1))
					i = i + 1
				if flt_face.props['draw type'] == 2:
					face_edges.append((vert_index + i,vert_index)) 
			elif face_len == 3 or face_len == 4: # tri or quad
				#if face_len == 1:
				#	pass
				#if face_len == 2:
				#	face_edges.append((vert_index, vert_index+1))
				new_faces.append( [i+vert_index for i in xrange(face_len)] )
				new_faces_props.append((None, image, uvs, flt_face.uverts, flt_face.uvlayers, flt_face.color_index, flt_face.props,FLT_OrigIndex,0, flt_face.subfacelevel))
			
			else: # fgon
				mesh_face_indicies = [i+vert_index for i in xrange(face_len)]
				tri_ngons= ngon(self.mesh, mesh_face_indicies)
				new_faces.extend([ [mesh_face_indicies[t] for t in tri] for tri in tri_ngons])
				new_faces_props.extend( [ (None, image, (uvs[tri[0]], uvs[tri[1]], uvs[tri[2]]), [flt_face.uverts[tri[0]], flt_face.uverts[tri[1]], flt_face.uverts[tri[2]]], flt_face.uvlayers, flt_face.color_index, flt_face.props,FLT_OrigIndex,1, flt_face.subfacelevel) for tri in tri_ngons ])
			
			vert_index+= face_len
			FLT_OrigIndex+=1
		
		self.mesh.faces.extend(new_faces)
		self.mesh.edges.extend(face_edges)
		
		#add in the FLT_ORIGINDEX layer
		if len(self.mesh.faces):
			try:	self.mesh.faceUV= True
			except:	pass
		
			if self.mesh.faceUV == True:
				self.mesh.renameUVLayer(self.mesh.activeUVLayer, 'Layer0')
		
			#create name layer for faces
			self.mesh.faces.addPropertyLayer("FLT_ID",Blender.Mesh.PropertyTypes["STRING"])
			#create layer for face color indices
			self.mesh.faces.addPropertyLayer("FLT_COL",Blender.Mesh.PropertyTypes["INT"])
			#create index layer for faces. This is needed by both FGONs and subfaces
			self.mesh.faces.addPropertyLayer("FLT_ORIGINDEX",Blender.Mesh.PropertyTypes["INT"])
			#create temporary FGON flag layer. Delete after remove doubles
			self.mesh.faces.addPropertyLayer("FLT_FGON",Blender.Mesh.PropertyTypes["INT"])
			self.mesh.faces.addPropertyLayer("FLT_SFLEVEL", Blender.Mesh.PropertyTypes["INT"])
			
			for i, f in enumerate(self.mesh.faces):
				props = new_faces_props[i]
				if props[6]['template billboard'] > 0:
					f.transp |= Blender.Mesh.FaceTranspModes["ALPHA"]
					if props[6]['template billboard'] == 2:
						f.mode |=  Blender.Mesh.FaceModes["BILLBOARD"]
					f.mode |= Blender.Mesh.FaceModes["LIGHT"]
				if props[6]['draw type'] == 1:
					f.mode |= Blender.Mesh.FaceModes["TWOSIDE"]
				
				#f.mat = props[0]
				f.image = props[1]
				f.uv = props[2]
				#set vertex colors
				color = self.header.get_color(props[5])
				if not color:
					color = [255,255,255,255]
				for mcol in f.col:
					mcol.a = color[3]
					mcol.r = color[0]
					mcol.g = color[1]
					mcol.b = color[2]
				
				f.setProperty("FLT_SFLEVEL", props[9])
				f.setProperty("FLT_ORIGINDEX",i)
				f.setProperty("FLT_ID",props[6]['id'])
				#if props[5] > 13199:
				#	print "Warning, invalid color index read in! Using default!"
				#	f.setProperty("FLT_COL",127)
				#else:
				if(1):			#uh oh....
					value = struct.unpack('>i',struct.pack('>I',props[5]))[0]
					f.setProperty("FLT_COL",value)
				
				#if props[8]: 
				#	f.setProperty("FLT_FGON",1)
				#else:
				#	f.setProperty("FLT_FGON",0)
			
			
			#Create multitex layers, if present.
			actuvlayer = self.mesh.activeUVLayer
			if(self.hasmtex):
				#For every multi-tex layer, we have to add a new UV layer to the mesh
				for i,mask in enumerate(reversed(sorted(self.uvlayers))):
					if self.uvlayers[mask]:
						self.blayernames[mask] = "Layer" + str(i+1)
						self.mesh.addUVLayer(self.blayernames[mask])
				
				#Cycle through availible multi-tex layers and add face UVS
				for mask in self.uvlayers:
					if self.uvlayers[mask]:
						self.mesh.activeUVLayer = self.blayernames[mask]
						for j, f in enumerate(self.mesh.faces):
							if props[6]['draw type'] == 1:
								f.mode |= Blender.Mesh.FaceModes["TWOSIDE"]
							f.transp |= Blender.Mesh.FaceTranspModes["ALPHA"]
							f.mode |= Blender.Mesh.FaceModes["LIGHT"]
							props = new_faces_props[j]
							uvlayers = props[4]
							if uvlayers.has_key(mask): #redundant
								uverts = props[3]
								for k, uv in enumerate(f.uv):
									uv[0] = uverts[k][mask][0]
									uv[1] = uverts[k][mask][1]
				
								uvlayer = uvlayers[mask]
								tex_index = uvlayer['texture index']
								if tex_index != -1:
									try:
										f.image = self.header.tex_pal[tex_index][1]
									except KeyError:
										f.image = None
									
			if global_prefs['smoothshading'] == True and len(self.mesh.faces):
				#We need to store per-face vertex normals in the faces as UV layers and delete them later.
				self.mesh.addUVLayer("FLTNorm1")
				self.mesh.addUVLayer("FLTNorm2")
				self.mesh.activeUVLayer = "FLTNorm1"
				for f in self.mesh.faces:
					f.smooth = 1
					#grab the X and Y components of normal and store them in UV 
					for i, uv in enumerate(f.uv):
						vert = f.v[i].index
						vert_desc = vert_desc_lst[vert_list[vert-1]]
						if vert_desc.cnorm:
							uv[0] = vert_desc.nx
							uv[1] = vert_desc.ny
						else:
							uv[0] = 0.0
							uv[1] = 0.0
				
				#Now go through and populate the second UV Layer with the z component
				self.mesh.activeUVLayer = "FLTNorm2"
				for f in self.mesh.faces:
					for i, uv in enumerate(f.uv):
						vert = f.v[i].index
						vert_desc = vert_desc_lst[vert_list[vert-1]]
						if vert_desc.cnorm:
							uv[0] = vert_desc.nz
							uv[1] = 0.0
						else:
							uv[0] = 0.0
							uv[1] = 0.0
			
				
				
			#Finally, go through, remove dummy vertex, remove doubles and add edgesplit modifier.
			Blender.Mesh.Mode(Blender.Mesh.SelectModes['VERTEX'])
			self.mesh.sel= 1
			self.header.scene.update(1) #slow!
			
			#self.mesh.remDoubles(0.0001)
			weldmesh = self.buildWeldMesh()
			welded = self.weldFuseFaces(weldmesh)
			self.mesh.verts.delete(0) # remove the dummy vert
			
			edgeHash = dict()

			for edge in self.mesh.edges:
				edgeHash[edge.key] = edge.index


			if global_prefs['smoothshading'] == True and len(self.mesh.faces):
				
				#rip out the custom vertex normals from the mesh and place them in a face aligned list. Easier to compare this way.
				facenorms = []
				self.mesh.activeUVLayer = "FLTNorm1"
				for face in self.mesh.faces:
					facenorm = []
					for uv in face.uv:
						facenorm.append(Vector(uv[0],uv[1],0.0))
					facenorms.append(facenorm)
				self.mesh.activeUVLayer = "FLTNorm2"
				for i, face in enumerate(self.mesh.faces):
					facenorm = facenorms[i]
					for j, uv in enumerate(face.uv):
						facenorm[j][2] = uv[0]
				self.mesh.removeUVLayer("FLTNorm1")
				self.mesh.removeUVLayer("FLTNorm2")

				#find hard edges
				#store edge data for lookup by faces
				#edgeHash = dict()
				#for edge in self.mesh.edges:
				#	edgeHash[edge.key] = edge.index

				edgeNormHash = dict()
				#make sure to align the edgenormals to key value!
				for i, face in enumerate(self.mesh.faces):
					
					facenorm = facenorms[i]
					faceEdges = []
					faceEdges.append((face.v[0].index,face.v[1].index,facenorm[0],facenorm[1],face.edge_keys[0]))
					faceEdges.append((face.v[1].index,face.v[2].index,facenorm[1],facenorm[2],face.edge_keys[1]))
					if len(face.v) == 3:
						faceEdges.append((face.v[2].index,face.v[0].index,facenorm[2],facenorm[0],face.edge_keys[2]))
					elif len(face.v) == 4:
						faceEdges.append((face.v[2].index,face.v[3].index,facenorm[2],facenorm[3],face.edge_keys[2]))
						faceEdges.append((face.v[3].index,face.v[0].index,facenorm[3],facenorm[0],face.edge_keys[3]))
					
					#check to see if edgeNormal has been placed in the edgeNormHash yet
					#this is a redundant test, and should be optimized to not be called as often as it is.
					for j, faceEdge in enumerate(faceEdges):
						#the value we are looking for is (faceEdge[2],faceEdge[3])
						hashvalue = (faceEdge[2],faceEdge[3])
						if (faceEdge[0],faceEdge[1]) != faceEdge[4]:
							hashvalue = (hashvalue[1],hashvalue[0])
							assert (faceEdge[1],faceEdge[0]) == faceEdge[4]
						if edgeNormHash.has_key(faceEdge[4]):
							#compare value in the hash, if different, mark as sharp
							edgeNorm = edgeNormHash[faceEdge[4]]
							if\
							abs(hashvalue[0][0] - edgeNorm[0][0]) > FLOAT_TOLERANCE or\
							abs(hashvalue[0][1] - edgeNorm[0][1]) > FLOAT_TOLERANCE or\
							abs(hashvalue[0][2] - edgeNorm[0][2]) > FLOAT_TOLERANCE or\
							abs(hashvalue[1][0] - edgeNorm[1][0]) > FLOAT_TOLERANCE or\
							abs(hashvalue[1][1] - edgeNorm[1][1]) > FLOAT_TOLERANCE or\
							abs(hashvalue[1][2] - edgeNorm[1][2]) > FLOAT_TOLERANCE:
								edge = self.mesh.edges[edgeHash[faceEdge[4]]]
								edge.flag |= Blender.Mesh.EdgeFlags.SHARP
								
						else:
							edgeNormHash[faceEdge[4]] = hashvalue
				
				#add in edgesplit modifier
				mod = self.object.modifiers.append(Blender.Modifier.Types.EDGESPLIT)
				mod[Blender.Modifier.Settings.EDGESPLIT_FROM_SHARP] = True
				mod[Blender.Modifier.Settings.EDGESPLIT_FROM_ANGLE] = False

			if(actuvlayer):
				self.mesh.activeUVLayer = actuvlayer
 		
	def blender_import(self):
		if self.vis and self.parent.object:
			self.vis = self.parent.vis
		name = self.props['id']
		

		if self.hasMesh:
			self.mesh = Blender.Mesh.New()
			self.mesh.name = 'FLT_FaceList'
			self.mesh.fakeUser = True
			self.mesh.verts.extend( Vector()) #DUMMYVERT
			self.object = self.header.scene.objects.new(self.mesh)
		else:
			self.object = self.header.scene.objects.new('Empty')

		self.object.name = name
		self.header.group.objects.link(self.object)

		#id props import
		self.object.properties['FLT'] = dict()
		for key in self.props:
			try:
				self.object.properties['FLT'][key] = self.props[key]
			except: #horrible...
				pass
		

		if self.extension:
			self.object.properties['FLT']['EXT'] = dict()
			for key in self.extension:
				self.object.properties['FLT']['EXT'][key] = self.extension[key]
		
		if self.parent and self.parent.object and (self.header.scene == self.parent.header.scene):
				self.parent.object.makeParent([self.object],1)

		if self.matrix:
			self.object.setMatrix(self.matrix)
		
		if self.vis == False:
			self.object.restrictDisplay = True
			self.object.restrictRender = True
		
		else: #check for LOD children and set the proper flags
			lodlist = list()
			for child in self.children:
				if child.props.has_key('type') and child.props['type'] == 73:
					if child.props['6d!switch out'] != 0.0:
						child.vis = False
					#lodlist.append(child)
			
			#def LODmin(a,b):
			#	if a.props['5d!switch in'] < b.props['5d!switch in']:
			#		return a 
			#	return b
		
			#min= None
			#if len(lodlist) > 1:
			#	for lod in lodlist:
			#		lod.vis = False
			#	min = lodlist[0]
			#	for i in xrange(len(lodlist)):
			#		min= LODmin(min,lodlist[i])
			#	min.vis = True
				
			
		Node.blender_import(self) # Attach faces to self.faceLs
		
		if self.hasMesh:
			# Add all my faces into the mesh at once
			self.blender_import_my_faces()
			
	def parse_face(self):
		child = Face(self, self.subfacelevel)
		child.parse()
		return True

	def parse_group(self):
		child = Group(self)
		child.parse()
		return True

	def move_to_next_layer(self):
		global current_layer
		current_layer = current_layer << 1
		if current_layer > 0x80000:
			current_layer = 1

	def parse_lod(self):
		child = LOD(self)
		child.parse()
		return True

	def parse_unhandled(self):
		child = Unhandled(self)
		child.parse()
		return True

	def parse_object(self):
		child = Object(self)
		child.parse()
		return True
	
	def parse_xref(self):
		child = XRef(self)
		child.parse()
		return True

	def parse_dof(self):
		child = DOF(self)
		child.parse()
		return True

	def parse_indexed_light_point(self):
		child = IndexedLightPoint(self)
		child.parse()
		return True
		
	def parse_inline_light_point(self):
		child = InlineLightPoint(self)
		child.parse()
		return True
		
	def parse_matrix(self):
		m = list()
		for i in xrange(4):
			m.append([])
			for j in xrange(4):
				f = self.header.fw.read_float()
				m[i].append(f)
		self.matrix = Blender.Mathutils.Matrix(m[0], m[1], m[2], m[3])
		
	def parse_subpush(self):
		self.parse_push()
		self.subfacelevel+= 1
		return True
	def  parse_subpop(self):
		self.parse_pop()
		self.subfacelevel -= 1
		return True

		
		
class Face(Node):
	def __init__(self, parent,subfacelevel):
		Node.__init__(self, parent, parent.header)
		self.root_handler.set_handler({31: self.parse_comment,
									   10: self.parse_push,
									   52: self.parse_multitex})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({72: self.parse_vertex_list,
										10: self.parse_push,
										11: self.parse_pop,
										53: self.parse_uvlist})
		
		if parent:
			parent.hasMesh = True

		self.subfacelevel = subfacelevel
		self.indices =  list()	# face verts here
		self.uvlayers = dict()	# MultiTexture layers keyed to layer bitmask.
		self.uverts = list()	# Vertex aligned list of dictionaries keyed to layer bitmask.
		self.uvmask = 0			# Bitfield read from MTex record
		
		self.comment = ''
		self.props = dict()		
		self.props['id'] = self.header.fw.read_string(8)
		# Load face.
		self.props['ir color'] = self.header.fw.read_int()
		self.props['priority'] = self.header.fw.read_short()
		self.props['draw type'] = self.header.fw.read_char()
		self.props['texture white'] = self.header.fw.read_char()
		self.header.fw.read_ahead(4) # color name indices
		self.header.fw.read_ahead(1) # reserved
		self.props['template billboard'] = self.header.fw.read_uchar()
		self.detail_tex_index = self.header.fw.read_short()
		self.tex_index = self.header.fw.read_short()
		self.mat_index = self.header.fw.read_short()
		self.props['smc'] = self.header.fw.read_short()
		self.props['fid'] = self.header.fw.read_short()
		self.props['ir material'] = self.header.fw.read_int()
		self.alpha = 1.0 - float(self.header.fw.read_ushort()) / 65535.0
		self.props['lod generation control'] = self.header.fw.read_uchar()
		self.header.fw.read_ahead(1) # line style index
		self.props['flags'] = self.header.fw.read_int()
		self.props['light mode'] = self.header.fw.read_uchar()
		self.header.fw.read_ahead(7)
		a = self.header.fw.read_uchar()
		b = self.header.fw.read_uchar()
		g = self.header.fw.read_uchar()
		r = self.header.fw.read_uchar()
		self.packed_color = [r, g, b, a]
		a = self.header.fw.read_uchar()
		b = self.header.fw.read_uchar()
		g = self.header.fw.read_uchar()
		r = self.header.fw.read_uchar()
		self.alt_packed_color = [r, g, b, a]
		self.tex_map_index = self.header.fw.read_short()
		self.header.fw.read_ahead(2)
		self.color_index = self.header.fw.read_uint()
		self.alt_color_index = self.header.fw.read_uint()
		#self.header.fw.read_ahead(2)
		#self.shader_index = self.header.fw.read_short()

	def parse_comment(self):
		self.comment = self.header.fw.read_string(self.header.fw.get_length()-4)
		return True
		
	def blender_import(self):
		vert_count = len(self.indices)
		if vert_count < 1:
			if global_prefs['verbose'] >= 2:
				print 'Warning: Ignoring face with no vertices.'
			return
		
		# Assign material and image
		
		self.parent.faceLs.append(self)
		#need to store comment in mesh prop layer!
		
		# Store comment info in parent.
		#if self.comment != '':
		#	if self.parent.props['comment'] != '':
		#		self.parent.props['comment'] += '\n\nFrom Face:\n' + self.comment
		#	else:
		#		self.parent.props['comment'] = self.comment
		
		if self.uvlayers:
			#Make sure that the mesh knows about the layers that this face uses
			self.parent.hasmtex = True
			for mask in self.uvlayers.keys():
				self.parent.uvlayers[mask] = True
			
	def parse_vertex_list(self):
		length = self.header.fw.get_length()
		fw = self.header.fw
		vert_pal = self.header.vert_pal

		count = (length-4)/4
		
		# If this ever fails the chunk below does error checking
		self.indices= [vert_pal.index[fw.read_int()] for i in xrange(count)]
		'''
		for i in xrange(count):
			byte_offset = fw.read_int()
			if byte_offset in vert_pal.index:
				index = vert_pal.index[byte_offset]
				self.indices.append(index)
			elif global_prefs['verbose'] >= 1:
				print 'Warning: Unable to map byte offset %s' + \
					  ' to vertex index.' % byte_offset
		'''
		return True
	
	def parse_multitex(self):
		#Parse  MultiTex Record.
		length = self.header.fw.get_length()
		fw = self.header.fw
		#num layers == (length - 8) / 4
		uvmask = fw.read_uint()
		mask = 2147483648
		for i in xrange(7):
			if mask & uvmask:
				uvlayer = dict()
				self.uvlayers[mask] = uvlayer
			mask = mask / 2
		
		#read in record for each individual layer.
		for key in reversed(sorted(self.uvlayers)):
			uvlayer = self.uvlayers[key]
			uvlayer['texture index'] = fw.read_ushort()
			uvlayer['texture enviorment'] = fw.read_ushort()
			uvlayer['texture mapping'] = fw.read_ushort()
			uvlayer['texture data'] = fw.read_ushort()
		
			self.uvmask = uvmask
		
	def parse_uvlist(self):
		#for each uvlayer, add uv vertices
		length = self.header.fw.get_length()
		fw = self.header.fw
		uvmask = fw.read_uint()
		if uvmask != self.uvmask: #This should never happen!
			fw.read_ahead(self.length -  4) #potentially unnessecary?
		else:	
			#need to store in uvverts dictionary for each vertex.
			totverts = len(self.indices)
			for i in xrange(totverts):
				uvert = dict()
				for key in reversed(sorted(self.uvlayers)):
					uv = Vector(0.0,0.0)
					uv[0] = fw.read_float()
					uv[1] = fw.read_float()
					uvert[key] = uv
				self.uverts.append(uvert)
				
class Object(InterNode):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({33: self.parse_long_id,
									21: self.parse_push_extension,
									31: self.parse_comment,
									10: self.parse_push,
									49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({5: self.parse_face,
										19: self.parse_subpush,
										20: self.parse_subpop,
										111: self.parse_inline_light_point,
										10: self.parse_push,
										11: self.parse_pop})
		self.extension_handler.set_handler({22: self.parse_pop_extension,
								100: self.parse_extension})
		
		self.extension = dict()
		self.props = dict()		
		self.props['comment'] = ''
		self.parse_record()

class Group(InterNode):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix,
									   21: self.parse_push_extension})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({5: self.parse_face,
										19: self.parse_subpush,
										20: self.parse_subpop,
										111: self.parse_inline_light_point,
										2: self.parse_group,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled,
										14: self.parse_dof,
										91: self.parse_unhandled,
										98: self.parse_unhandled,
										63: self.parse_xref})
										
		self.extension_handler.set_handler({22: self.parse_pop_extension,
								100: self.parse_extension})
								
		self.props = dict.fromkeys(['type', 'id', 'comment', 'priority', 'flags', 'special1',
									'special2', 'significance', 'layer code', 'loop count',
									'loop duration', 'last frame duration'])
		
		self.props['comment'] = ''
		self.parse_record()
		
		#self.props['type'] = str(self.opcode) + ':' + opcode_name[self.opcode]
		#props = records[self.opcode]
		#propkeys = props.keys()
		#propkeys.sort()
		#for position in propkeys:
		#	(type,length,name) = props[position]
		#	self.props[name] = read_prop(self.header.fw,type,length)
		#self.props['id'] = self.props['3t8!id']

class DOF(InterNode):
	def blender_import(self):
		InterNode.blender_import(self)

	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix,
									   21: self.parse_push_extension})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({#130: self.parse_indexed_light_point,
										111: self.parse_inline_light_point,
										2: self.parse_group,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled,
										14: self.parse_dof,
										91: self.parse_unhandled,
										98: self.parse_unhandled,
										63: self.parse_xref})
		self.extension_handler.set_handler({22: self.parse_pop_extension,
										100: self.parse_extension})
		self.props = dict()		
		self.props['comment'] = ''
		self.parse_record()


class XRef(InterNode):
	def parse(self):
		if self.xref:
			self.xref.parse()
		Node.parse(self)

	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.props = dict()		
		self.props['comment'] = ''
		self.parse_record()

		xref_filename = self.props['3t200!filename'] #I dont even think there is a reason to keep this around...
		
		if not os.path.isabs(xref_filename):
			absname = os.path.join(os.path.dirname(self.header.filename), xref_filename) 
		else:
			absname = xref_filename	
		
		self.props['id'] = 'X: ' + Blender.sys.splitext(Blender.sys.basename(xref_filename))[0] #this is really wrong as well....
		
		if global_prefs['doxrefs'] and os.path.exists(absname) and not self.header.grr.xrefs.has_key(xref_filename):
			self.xref = Database(absname, self.header.grr, self)
			self.header.grr.xrefs[xref_filename] = self.xref
		else:
			self.xref = None
		

	def blender_import(self):
		#name = self.props['type'] + ': ' + self.props['id']
		name = self.props['id']
		self.object = self.header.scene.objects.new('Empty')
		self.object.name = name
		self.object.enableDupGroup = True
		self.header.group.objects.link(self.object)
		
		#for broken links its ok to leave this empty! they purely for visual purposes anyway.....
		try:
			self.object.DupGroup = self.header.grr.xrefs[self.props['3t200!filename']].group
		except:
			pass
			



		if self.parent and self.parent.object:
			self.parent.object.makeParent([self.object],1)

		if self.matrix:
			self.object.setMatrix(self.matrix)


		#id props import
		self.object.properties['FLT'] = dict()
		for key in self.props:
			try:
				self.object.properties['FLT'][key] = self.props[key]
			except: #horrible...
				pass

		self.object.Layer = current_layer
		self.object.sel = 1

		Node.blender_import(self)
		
		
class LOD(InterNode):
	def blender_import(self):
		#self.move_to_next_layer()
		InterNode.blender_import(self)
		#self.object.properties['FLT'] = self.props.copy()
		
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)

		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix,
									   21: self.parse_push_extension})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({2: self.parse_group,
										111: self.parse_inline_light_point,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled, # switch
										14: self.parse_dof, # DOF
										91: self.parse_unhandled, # sound
										98: self.parse_unhandled, # clip
										63: self.parse_xref})
		self.extension_handler.set_handler({22: self.parse_pop_extension,
								100: self.parse_extension})


		self.props = dict()		
		self.props['comment'] = ''
		self.parse_record()

class InlineLightPoint(InterNode):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   21: self.parse_push_extension,
									   49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({72: self.parse_vertex_list,
										10: self.parse_push,
										11: self.parse_pop})
		self.extension_handler.set_handler({22: self.parse_pop_extension,
								100: self.parse_extension})
		
		self.indices = list()
		self.props = dict()		
		self.props['comment'] = ''
		self.parse_record()

		
	def blender_import(self):
		

		name = self.props['id']
		self.mesh= Blender.Mesh.New()
		self.mesh.name = 'FLT_LP'
		self.object = self.header.scene.objects.new(self.mesh)
		self.object.name = name
		#self.mesh.verts.extend(Vector() ) # DUMMYVERT
		self.object.Layer = current_layer
		self.object.sel= 1
	
		self.object.properties['FLT'] = dict()
		for key in self.props:
			try:
				self.object.properties['FLT'][key] = self.props[key]
			except: #horrible...
				pass

		if self.extension:
			self.object.properties['FLT']['EXT'] = dict()
			for key in self.extension:
				self.object.properties['FLT']['EXT'][key] = self.extension[key]

		if self.parent and self.parent.object and self.header.scene == self.parent.header.scene:
			self.parent.object.makeParent([self.object])

		if self.matrix:
			self.object.setMatrix(self.matrix)

		self.mesh.verts.extend([self.header.vert_pal.blender_verts[i] for i in self.indices])
		
		#add color index information.
		self.mesh.verts.addPropertyLayer("FLT_VCOL",Blender.Mesh.PropertyTypes["INT"])
		for i, vindex in enumerate(self.indices):
			vdesc = self.header.vert_pal.vert_desc_lst[vindex]
			v = self.mesh.verts[i]
			v.setProperty("FLT_VCOL",vdesc.cindex)
		#for i, v in enumerate(self.mesh.verts):
		#	vdesc = self.header.vert_pal.vert_desc_lst[i]
		#	v.setProperty("FLT_VCOL",vdesc.cindex)
		self.mesh.update()
				
	def parse_vertex_list(self):
		length = self.header.fw.get_length()
		fw = self.header.fw
		vert_pal = self.header.vert_pal

		count = (length-4)/4
		
		# If this ever fails the chunk below does error checking
		self.indices= [vert_pal.index[fw.read_int()] for i in xrange(count)]
		
		'''
		for i in xrange(count):
			byte_offset = fw.read_int()
			if byte_offset in vert_pal.index:
				index = vert_pal.index[byte_offset]
				self.indices.append(index)
			elif global_prefs['verbose'] >= 1:
				print 'Warning: Unable to map byte offset %s' + \
					  ' to vertex index.' % byte_offset
		'''
		
		return True
		


class IndexedLightPoint(InterNode):
	# return dictionary: lp_app name => index list
	def group_points(self, props):
		
		name_to_indices = {}
		
		for i in self.indices:
			vert_desc = self.header.vert_pal.vert_desc_lst[i]
			app_desc = LightPointAppDesc()
			app_desc.props.update(props)
			# add vertex normal and color
			app_desc.props.update({'nx': vert_desc.nx})
			app_desc.props.update({'ny': vert_desc.ny})
			app_desc.props.update({'nz': vert_desc.nz})
			
			app_desc.props.update({'r': vert_desc.r})
			app_desc.props.update({'g': vert_desc.g})
			app_desc.props.update({'b': vert_desc.b})
			app_desc.props.update({'a': vert_desc.a})
			
			app_name = self.header.grr.request_lightpoint_app(app_desc, self.header.scene)

			if name_to_indices.get(app_name):
				name_to_indices[app_name].append(i)
			else:
				name_to_indices.update({app_name: [i]})
			
		return name_to_indices
		
	def blender_import(self):
		name = self.props['type'] + ': ' + self.props['id']
		
		name_to_indices = self.group_points(self.header.lightpoint_appearance_pal[self.index])
		
		for app_name, indices in name_to_indices.iteritems():        
			self.object = Blender.Object.New('Mesh', name)
			self.mesh= Blender.Mesh.New()
			self.mesh.verts.extend( Vector() ) # DUMMYVERT
			self.object.link(self.mesh)
			
			if self.parent:
				self.parent.object.makeParent([self.object])
				
			for i in indices:
				vert = self.header.vert_pal.blender_verts[i]
				self.mesh.verts.append(vert)
			
			self.header.scene.objects.link(self.object)
	
			self.object.Layer = current_layer
			
			if self.matrix:
				self.object.setMatrix(self.matrix)
				
			# Import comment.
			if self.props['comment'] != '':
				name = 'COMMENT: ' + self.props['id']
				t = Blender.Text.New(name)
				t.write(self.props['comment'])
				self.props['comment'] = name
				
			# Attach properties.
			self.props.update({'appearance': app_name})
			for name, value in self.props.iteritems():
				self.object.addProperty(name, value)
			
			self.mesh.update()
			
	def parse_vertex_list(self):
		length = self.header.fw.get_length()
		fw = self.header.fw
		vert_pal = self.header.vert_pal

		count = (length-4)/4
		
		# If this ever fails the chunk below does error checking
		self.indices= [vert_pal.index[fw.read_int()] for i in xrange(count)]
		
		'''
		for i in xrange(count):
			byte_offset = fw.read_int()
			if byte_offset in vert_pal.index:
				index = vert_pal.index[byte_offset]
				self.indices.append(index)
			elif global_prefs['verbose'] >= 1:
				print 'Warning: Unable to map byte offset %s' + \
					  ' to vertex index.' % byte_offset
		'''
		return True
		
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({72: self.parse_vertex_list,
										10: self.parse_push,
										11: self.parse_pop})

		self.indices = list()
		
		self.props = dict.fromkeys(['id', 'type', 'comment', 'draw order', 'appearance'])
		self.props['comment'] = ''
		self.props['type'] = 'Light Point'
		self.props['id'] = self.header.fw.read_string(8)
		self.index = self.header.fw.read_int()
		self.header.fw.read_ahead(4) # animation index
		self.props['draw order'] = self.header.fw.read_int()        

class Unhandled(InterNode):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({2: self.parse_group,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled, # switch
										14: self.parse_dof, # DOF
										91: self.parse_unhandled, # sound
										98: self.parse_unhandled, # clip
										63: self.parse_xref})

		self.props['id'] = self.header.fw.read_string(8)

class Database(InterNode):
	def blender_import(self):
		for key in self.tex_pal.keys():
			path_filename= FF.find(self.tex_pal[key][0])
			if path_filename != None:
				img = self.grr.request_image(path_filename)
				if img:
					self.tex_pal[key][1] = img
			elif global_prefs['verbose'] >= 1:
				print 'Warning: Unable to find', self.tex_pal[key][0]
		
		self.scene.properties['FLT'] = dict()
		for key in self.props:
			try:
				self.scene.properties['FLT'][key] = self.props[key]
			except: #horrible...
				pass
		
		self.scene.properties['FLT']['Main'] = 0
		self.scene.properties['FLT']['Filename'] = self.bname
		
		for child in self.children:
			if child.props.has_key('type') and child.props['type'] == 73:
				if child.props['6d!switch out'] != 0.0:
						child.vis = False
		
		#import color palette
		carray = list()
		for color in self.col_pal:
			carray.append(struct.unpack('>i',struct.pack('>BBBB',color[0],color[1],color[2],color[3]))[0])
		self.scene.properties['FLT']['Color Palette'] = carray
		Node.blender_import(self)

	def parse_appearance_palette(self):
		props = dict()
		self.fw.read_ahead(4) # reserved
		props.update({'id': self.fw.read_string(256)})
		index = self.fw.read_int()
		props.update({'smc': self.fw.read_short()})
		props.update({'fid': self.fw.read_short()})
		props.update({'back color: a': self.fw.read_uchar()})
		props.update({'back color: b': self.fw.read_uchar()})
		props.update({'back color: g': self.fw.read_uchar()})
		props.update({'back color: r': self.fw.read_uchar()})
		props.update({'display mode': self.fw.read_int()})
		props.update({'intensity': self.fw.read_float()})
		props.update({'back intensity': self.fw.read_float()})
		props.update({'minimum defocus': self.fw.read_float()})
		props.update({'maximum defocus': self.fw.read_float()})
		props.update({'fading mode': self.fw.read_int()})
		props.update({'fog punch mode': self.fw.read_int()})
		props.update({'directional mode': self.fw.read_int()})
		props.update({'range mode': self.fw.read_int()})
		props.update({'min pixel size': self.fw.read_float()})
		props.update({'max pixel size': self.fw.read_float()})
		props.update({'actual size': self.fw.read_float()})
		props.update({'trans falloff pixel size': self.fw.read_float()})
		props.update({'trans falloff exponent': self.fw.read_float()})
		props.update({'trans falloff scalar': self.fw.read_float()})
		props.update({'trans falloff clamp': self.fw.read_float()})
		props.update({'fog scalar': self.fw.read_float()})
		props.update({'fog intensity': self.fw.read_float()})
		props.update({'size threshold': self.fw.read_float()})
		props.update({'directionality': self.fw.read_int()})
		props.update({'horizontal lobe angle': self.fw.read_float()})
		props.update({'vertical lobe angle': self.fw.read_float()})
		props.update({'lobe roll angle': self.fw.read_float()})
		props.update({'dir falloff exponent': self.fw.read_float()})
		props.update({'dir ambient intensity': self.fw.read_float()})
		props.update({'significance': self.fw.read_float()})
		props.update({'flags': self.fw.read_int()})
		props.update({'visibility range': self.fw.read_float()})
		props.update({'fade range ratio': self.fw.read_float()})
		props.update({'fade in duration': self.fw.read_float()})
		props.update({'fade out duration': self.fw.read_float()})
		props.update({'LOD range ratio': self.fw.read_float()})
		props.update({'LOD scale': self.fw.read_float()})
		
		self.lightpoint_appearance_pal.update({index: props})
		
	def parse_header(self):
		self.props['type'] = 'Header'
		self.props['comment'] = ''
		self.props['id'] = self.fw.read_string(8)
		self.props['version'] = self.fw.read_int()
		self.fw.read_ahead(46)
		self.props['units'] = self.fw.read_char()
		self.props['set white'] = bool(self.fw.read_char())
		self.props['flags'] = self.fw.read_int()
		self.fw.read_ahead(24)
		self.props['projection type'] = self.fw.read_int()
		self.fw.read_ahead(36)
		self.props['sw x'] = self.fw.read_double()
		self.props['sw y'] = self.fw.read_double()
		self.props['dx'] = self.fw.read_double()
		self.props['dy'] = self.fw.read_double()
		self.fw.read_ahead(24)
		self.props['sw lat'] = self.fw.read_double()
		self.props['sw lon'] = self.fw.read_double()
		self.props['ne lat'] = self.fw.read_double()
		self.props['ne lon'] = self.fw.read_double()
		self.props['origin lat'] = self.fw.read_double()
		self.props['origin lon'] = self.fw.read_double()
		self.props['lambert lat1'] = self.fw.read_double()
		self.props['lambert lat2'] = self.fw.read_double()
		self.fw.read_ahead(16)
		self.props['ellipsoid model'] = self.fw.read_int()
		self.fw.read_ahead(4)
		self.props['utm zone'] = self.fw.read_short()
		self.fw.read_ahead(6)
		self.props['dz'] = self.fw.read_double()
		self.props['radius'] = self.fw.read_double()
		self.fw.read_ahead(8)
		self.props['major axis'] = self.fw.read_double()
		self.props['minor axis'] = self.fw.read_double()
		
		if global_prefs['verbose'] >= 1:
			print 'OpenFlight Version:', float(self.props['version']) / 100.0
			print
			
		return True

	def parse_mat_palette(self):
		mat_desc = MaterialDesc()
		index = self.fw.read_int()

		name = self.fw.read_string(12)
		if len(mat_desc.name) > 0:
			mat_desc.name = name

		flag = self.fw.read_int()
		# skip material if not used
		if not flag & 0x80000000:
			return True

		ambient_col = [self.fw.read_float(), self.fw.read_float(), self.fw.read_float()]
		mat_desc.diffuse = [self.fw.read_float(), self.fw.read_float(), self.fw.read_float()]
		mat_desc.specular = [self.fw.read_float(), self.fw.read_float(), self.fw.read_float()]
		emissive_col = [self.fw.read_float(), self.fw.read_float(), self.fw.read_float()]

		mat_desc.shininess = self.fw.read_float() / 64.0 # [0.0, 128.0] => [0.0, 2.0]
		mat_desc.alpha = self.fw.read_float()

		# Convert ambient and emissive colors into intensitities.
		mat_desc.ambient = col_to_gray(ambient_col)
		mat_desc.emissive = col_to_gray(emissive_col)

		self.mat_desc_pal_lst.append( (index, mat_desc) )
		
		return True
	
	def get_color(self, color_index):
		color = None
		index = color_index / 128
		intensity = float(color_index - 128.0 * index) / 127.0
		
		if index >= 0 and index <= 1023:
			brightest = self.col_pal[index]
			r = int(brightest[0] * intensity)
			g = int(brightest[1] * intensity)
			b = int(brightest[2] * intensity)
			a = int(brightest[3])
			
			color = [r, g, b, a]
		
		return color
	
	def parse_color_palette(self):
		self.header.fw.read_ahead(128)
		for i in xrange(1024):
			a = self.header.fw.read_uchar()
			b = self.header.fw.read_uchar()
			g = self.header.fw.read_uchar()
			r = self.header.fw.read_uchar()
			self.col_pal.append((r, g, b, a))
		return True
		
	def parse_vertex_palette(self):
		self.vert_pal = VertexPalette(self)
		self.vert_pal.parse()
		return True
		
	def parse_texture_palette(self):
		name = self.fw.read_string(200)
		index = self.fw.read_int()
		self.tex_pal[index]= [name, None]
		return True
	
	def read_attribute_files(self):
		for tex in self.tex_pal.keys():
			[name,image] = self.tex_pal[tex]
			basename = os.path.basename(name)
			if(image):
				basename = basename + ".attr"
				dirname = os.path.dirname(Blender.sys.expandpath(image.getFilename())) #can't rely on original info stored in pallette since it might be relative link
				newpath = os.path.join(dirname, basename)
				if os.path.exists(newpath) and not image.properties.has_key('FLT'):
					fw = flt_filewalker.FltIn(newpath)
					fw.read_ahead(8) #We dont care what the attribute file says about x/y dimensions
					image.properties['FLT']={}
					
					#need to steal code from parse records....
					props = records['Image']
					propkeys = props.keys()
					propkeys.sort()
					for position in propkeys:
						(type,length,name) = props[position]
						image.properties['FLT'][name] = read_prop(fw,type,length)
					fw.close_file()
					
					#copy clamp settings
					wrap = image.properties['FLT']['10i!Wrap']
					wrapu = image.properties['FLT']['11i!WrapU']
					wrapv = image.properties['FLT']['12i!WrapV']
					
					if wrapu == 3 or wrapv == 3:
						wrapuv = (wrap,wrap)
					else:
						wrapuv = (wrapu, wrapv)
					image.clampX = wrapuv[0]
					image.clampY = wrapuv[1]
					
				elif not os.path.exists(newpath):
					print "Cannot read attribute file:" + newpath
					
	def __init__(self, filename, grr, parent=None):
		if global_prefs['verbose'] >= 1:
			print 'Parsing:', filename
			print
		
		#check to see if filename is a relative path
		#filename = os.path.abspath(filename)
		
		self.fw = flt_filewalker.FltIn(filename)
		self.filename = filename
		self.bname = os.path.splitext(os.path.basename(filename))[0]
		self.grr = grr
		
		Node.__init__(self, parent, self)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({1: self.parse_header,
									   67: self.parse_vertex_palette,
									   33: self.parse_long_id,
									   31: self.parse_comment,
									   64: self.parse_texture_palette,
									   32: self.parse_color_palette,
									   113: self.parse_mat_palette,
									   128: self.parse_appearance_palette,
									   10: self.parse_push})
		if parent:
			self.root_handler.set_throw_back_lst(throw_back_opcodes)

		self.child_handler.set_handler({#130: self.parse_indexed_light_point,
										111: self.parse_inline_light_point,
										2: self.parse_group,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled,
										14: self.parse_dof,
										91: self.parse_unhandled,
										98: self.parse_unhandled,
										63: self.parse_xref})
		
		self.scene = Blender.Scene.New(self.bname)
		self.group = Blender.Group.New(self.bname)

		self.vert_pal = None
		self.lightpoint_appearance_pal = dict()
		self.tex_pal = dict()
		#self.tex_pal_lst = list()
		#self.bl_tex_pal = dict()
		self.col_pal = list()
		self.mat_desc_pal_lst = list()
		self.mat_desc_pal = dict()
		self.props = dict.fromkeys(['id', 'type', 'comment', 'version', 'units', 'set white',
			'flags', 'projection type', 'sw x', 'sw y', 'dx', 'dy', 'dz', 'sw lat',
			'sw lon', 'ne lat', 'ne lon', 'origin lat', 'origin lon', 'lambert lat1',
			'lambert lat2', 'ellipsoid model', 'utm zone', 'radius', 'major axis', 'minor axis'])


def clearparent(root,childhash):
	for child in childhash[root]:
		clearparent(child,childhash)
	root.clrParent(2,0)

def fixscale(root,childhash):	
	for child in childhash[root]:
		fixscale(child,childhash)
	location = Blender.Mathutils.Vector(root.getLocation('worldspace'))
	if location[0] != 0.0 or location[1] != 0.0 or location[2] != 0.0:
		#direction = Blender.Mathutils.Vector(0-location[0],0-location[1],0-location[2]) #reverse vector
		smat = Blender.Mathutils.ScaleMatrix(global_prefs['scale'],4)
		root.setLocation(location * smat)
	#if its a mesh, we need to scale all of its vertices too
	if root.type == 'Mesh':
		smat = Blender.Mathutils.ScaleMatrix(global_prefs['scale'],4)
		rmesh = root.getData(mesh=True)
		for v in rmesh.verts:
			v.co = v.co * smat
	
	
def reparent(root,childhash,sce):
	for child in childhash[root]:
		reparent(child,childhash,sce)
	
	root.makeParent(childhash[root])
	sce.update(1)
	
def update_scene(root,sdone):
	for object in root.objects:
		if object.DupGroup:
			try:
				child = Blender.Scene.Get(object.DupGroup.name)
			except:
				child = None
			if child and child not in sdone:
				update_scene(child,sdone)
	root.makeCurrent()
	#create a list of children for each object
	childhash = dict()
	for object in root.objects:
		childhash[object] = list()
		
	for object in root.objects:
		if object.parent:
			childhash[object.parent].append(object)
	
	for object in root.objects:
		if not object.parent:
			#recursivley go through and clear all the children of their transformation, starting at deepest level first.
			clearparent(object,childhash)
			#now fix the location of everything
			fixscale(object,childhash)
			#now fix the parenting
			reparent(object,childhash,root)
	
	for object in root.objects:
		object.makeDisplayList()
	root.update(1)
	sdone.append(root)


def select_file(filename, grr):
	if not Blender.sys.exists(filename):
		msg = 'Error: File ' + filename + ' does not exist.'
		Blender.Draw.PupMenu(msg)
		return
	
	if not filename.lower().endswith('.flt'):
		msg = 'Error: Not a flight file.'
		Blender.Draw.PupMenu(msg)
		print msg
		print
		return
	
	global_prefs['fltfile']= filename
	global_prefs['verbose']= 1
	global_prefs['get_texture'] = True
	global_prefs['get_diffuse'] = True
	global_prefs['get_specular'] = False
	global_prefs['get_emissive'] = False
	global_prefs['get_alpha'] = True
	global_prefs['get_ambient'] = False
	global_prefs['get_shininess'] = True
	global_prefs['color_from_face'] = True
	global_prefs['log to blender'] = True
	
	
	
	Blender.Window.WaitCursor(True)
	Blender.Window.EditMode(0)
	
	
	FF.add_file_to_search_path(filename)
	
	if global_prefs['verbose'] >= 1:
		print 'Pass 1: Loading.'
		print

	load_time = Blender.sys.time()    
	db = Database(filename,grr)
	db.parse()
	load_time = Blender.sys.time() - load_time

	if global_prefs['verbose'] >= 1:
		print
		print 'Pass 2: Importing to Blender.'
		print

	import_time = Blender.sys.time()
	db.blender_import()
	
	if global_prefs['attrib']:
		print "reading attribute files"
		db.read_attribute_files()
	
	Blender.Window.ViewLayer(range(1,21))
	
	update_scene(db.scene,[])
	import_time = Blender.sys.time() - import_time
	if global_prefs['verbose'] >= 1:
		print 'Done.'
		print
		print 'Time to parse file: %.3f seconds' % load_time
		print 'Time to import to blender: %.3f seconds' % import_time
		print 'Total time: %.3f seconds' % (load_time + import_time)
	
	Blender.Window.WaitCursor(False)

def setimportscale(ID,val):
	global global_prefs
	global_prefs['scale'] = val
def setBpath(fname):
	global_prefs['fltfile'] = fname
	d = dict()
	for key in global_prefs:
		d[key] = global_prefs[key]
		Blender.Registry.SetKey('flt_import', d, 1) 

def event(evt,val):
	pass

from Blender.BGL import *
from Blender import Draw

def but_event(evt):
	
	global FLTBaseLabel
	global FLTBaseString
	global FLTBaseChooser

	global FLTExport
	global FLTClose
	
	global FLTDoXRef
	global FLTShadeImport
	global FLTAttrib
	
	global FLTWarn
	
	#Import DB
	if evt == 1:
		if global_prefs['verbose'] >= 1:
			print
			print 'OpenFlight Importer'
			print 'Version:', __version__
			print 'Author: Greg MacDonald, Campbell Barton, Geoffrey Bantle'
			print __url__[2]
			print
		
		GRR = GlobalResourceRepository()
		
		try:
			select_file(global_prefs['fltfile'], GRR)
		except:
			import traceback
			FLTWarn = Draw.PupBlock("Export Error", ["See console for output!"])
			traceback.print_exception(sys.exc_type, sys.exc_value, sys.exc_traceback)
	
	#choose base path for export
	if evt == 4:
		Blender.Window.FileSelector(setBpath, "DB Root", global_prefs['fltfile'])
	#Import custom shading?
	if evt == 9:
		global_prefs['smoothshading'] = FLTShadeImport.val
	#Import Image attribute files
	if evt == 10:
		global_prefs['attrib'] = FLTAttrib.val
	#export XRefs
	if evt == 13:
		global_prefs['doxrefs'] = FLTDoXRef.val
	
	if evt == 2:
		Draw.Exit()
	
	d = dict()
	for key in global_prefs:
		d[key] = global_prefs[key]
		Blender.Registry.SetKey('flt_import', d, 1) 

def gui():
	
	global FLTBaseLabel
	global FLTBaseString
	global FLTBaseChooser

	global FLTExport
	global FLTClose
	
	global FLTDoXRef
	global FLTShadeImport
	
	global FLTAttrib
	
	
	glClearColor(0.772,0.832,0.847,1.0)
	glClear(GL_COLOR_BUFFER_BIT)
	
	areas = Blender.Window.GetScreenInfo()
	curarea = Blender.Window.GetAreaID()
	curRect = None
	
	for area in areas:
		if area['id'] == curarea:
			curRect = area['vertices']
			break
	
	width = curRect[2] - curRect[0]
	height = curRect[3] - curRect[1]
	cx = 50
	cy = height - 80

	FLTBaseLabel = Draw.Label("Base file:",cx,cy,100,20)
	FLTBaseString = Draw.String("",3,cx+100,cy,300,20,global_prefs['fltfile'],255,"Root DB file")
	FLTBaseChooser = Draw.PushButton("...",4,cx+400,cy,20,20,"Choose Folder")
	
	cy = cy-40
	FLTScale = Draw.Number("Import Scale",14,cx,cy,220,20,global_prefs['scale'],0.0,100.0,"Export scaleing factor",setimportscale)
	
	cy = cy-40
	FLTDoXRef = Draw.Toggle("Import XRefs", 13,cx,cy,220,20,global_prefs['doxrefs'],"Import External references")
	
	cy = cy-40
	FLTShadeImport = Draw.Toggle("Import Custom Shading",9,cx,cy,220,20,global_prefs['smoothshading'],"Import custom shading via edgesplit modifiers")
	
	cy = cy-40
	FLTAttrib = Draw.Toggle("Import Attribute Files", 10,cx,cy,220,20,global_prefs['attrib'],"Import Image Attribute files")
	
	cy = cy - 40
	FLTExport = Draw.PushButton("Import",1,cx,20,100,20,"Import FLT Database")
	FLTClose = Draw.PushButton("Close",2,cx+120,20,100,20,"Close Window")

	
	
Draw.Register(gui,event,but_event)