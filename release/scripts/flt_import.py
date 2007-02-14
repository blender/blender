#!BPY
""" Registration info for Blender menus:
Name: 'OpenFlight (.flt)...'
Blender: 238
Group: 'Import'
Tip: 'Import OpenFlight (.flt)'
"""

__author__ = "Greg MacDonald, Campbell Barton"
__version__ = "1.2 10/20/05"
__url__ = ("blender", "elysiun", "Author's homepage, http://sourceforge.net/projects/blight/")
__bpydoc__ = """\
This script imports OpenFlight files into Blender. OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.

Run from "File->Import" menu.

Options are available from Blender's "Scripts Config Editor," accessible through
the "Scripts->System" menu from the scripts window.

All global_prefs are toggle switches that let the user choose what is imported. Most
are straight-forward, but one option could be a source of confusion. The 
"Diffuse Color From Face" option when set pulls the diffuse color from the face
colors. Otherwise the diffuse color comes from the material. What may be
confusing is that this global_prefs only works if the "Diffuse Color" option is set.

New Features:<br>
* Importer is 14 times faster.<br>
* External triangle module is no longer required, but make sure the importer
has a 3d View screen open while its running or triangulation won't work.<br>
* Should be able to import all versions of flight files.

Features:<br>
* Heirarchy retained.<br>
* First texture imported.<br>
* Colors imported from face or material.<br>
* LOD seperated out into different layers.<br>
* Asks for location of unfound textures or external references.<br>
* Searches Blender's texture directory in the user preferences panel.<br>
* Triangles with more than 4 verts are triangulated if the Triangle python
module is installed.<br>
* Matrix transforms imported.<br>
* External references to whole files are imported.

Things To Be Aware Of:<br>
* Each new color and face attribute creates a new material and there are only a maximum of 16
materials per object.<br>
* For triangulated faces, normals must be recomputed outward manually by typing
CTRL+N in edit mode.<br>
* You can change global_prefs only after an initial import.<br>
* External references are imported as geometry and will be exported that way.<br>
* A work around for not using the Triangle python module is to simply to 
triangulate in Creator before importing. This is only necessary if your
model contains 5 or more vertices.<br>
* You have to manually blend the material color with the texture color.

What's Not Handled:<br>
* Special texture repeating modes.<br>
* Replications and instancing.<br>
* Comment and attribute fields.<br>
* Light points.<br>
* Animations.<br>
* External references to a node within a file.<br>
* Multitexturing.<br>
* Vetex colors.<br>
"""

# flt_import.py is an OpenFlight importer for blender.
# Copyright (C) 2005 Greg MacDonald
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

Vector= Blender.Mathutils.Vector

def col_to_gray(c):
	return 0.3*c[0] + 0.59*c[1] + 0.11*c[2]


global_prefs = dict()
global_prefs['verbose']= 1
global_prefs['get_texture'] = True
global_prefs['get_diffuse'] = True
global_prefs['get_specular'] = False
global_prefs['get_emissive'] = False
global_prefs['get_alpha'] = True
global_prefs['get_ambient'] = False
global_prefs['get_shininess'] = True
global_prefs['color_from_face'] = True
global_prefs['fltfile']= ''
msg_once = False

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
		
		''' # IGNORE_NORMALS
		self.nx = 0.0
		self.ny = 1.0
		self.nz = 0.0
		'''
		self.uv= Vector(0,0)
		self.r = 1.0
		self.g = 1.0
		self.b = 1.0
		self.a = 1.0        

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
	def request_lightpoint_app(self, desc):
		match = self.light_point_app.get(desc.make_key())
		
		if match:
			return match.getName()
		else:
			# Create empty and fill with properties.
			name = desc.props['type'] + ': ' + desc.props['id']
			object = Blender.Object.New('Empty', name)
			scene.link(object)
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
		
# Globals
GRR = GlobalResourceRepository()
FF = flt_filewalker.FileFinder()
scene = Blender.Scene.GetCurrent() # just hope they dont chenge scenes once the file selector pops up.
current_layer = 0x01


# Opcodes that indicate its time to return control to parent.
throw_back_opcodes = [2, 73, 4, 11, 96, 14, 91, 98, 63]
do_not_report_opcodes = [76, 78, 79, 80, 81, 82, 94, 83, 33, 112, 100, 101, 102, 97, 31, 103, 104, 117, 118, 120, 121, 124, 125]

opcode_name = { 0: 'db',
				1: 'head',
				2: 'grp',
				4: 'obj',
				5: 'face',
				10: 'push',
				11: 'pop',
				14: 'dof',
				19: 'push sub',
				20: 'pop sub',
				21: 'push ext',
				22: 'pop ext',
				23: 'cont',
				31: 'comment',
				32: 'color pal',
				33: 'long id',
				49: 'matrix',
				50: 'vector',
				52: 'multi-tex',
				53: 'uv lst',
				55: 'bsp',
				60: 'rep',
				61: 'inst ref',
				62: 'inst def',
				63: 'ext ref',
				64: 'tex pal',
				67: 'vert pal',
				68: 'vert w col',
				69: 'vert w col & norm',
				70: 'vert w col, norm & uv',
				71: 'vert w col & uv',
				72: 'vert lst',
				73: 'lod',
				74: 'bndin box',
				76: 'rot edge',
				78: 'trans',
				79: 'scl',
				80: 'rot pnt',
				81: 'rot and/or scale pnt',
				82: 'put',
				83: 'eyepoint & trackplane pal',
				84: 'mesh',
				85: 'local vert pool',
				86: 'mesh prim',
				87: 'road seg',
				88: 'road zone',
				89: 'morph vert lst',
				90: 'link pal',
				91: 'snd',
				92: 'rd path',
				93: 'snd pal',
				94: 'gen matrix',
				95: 'txt',
				96: 'sw',
				97: 'line styl pal',
				98: 'clip reg',
				100: 'ext',
				101: 'light src',
				102: 'light src pal',
				103: 'reserved',
				104: 'reserved',
				105: 'bndin sph',
				106: 'bndin cyl',
				107: 'bndin hull',
				108: 'bndin vol cntr',
				109: 'bndin vol orient',
				110: 'rsrvd',
				111: 'light pnt',
				112: 'tex map pal',
				113: 'mat pal',
				114: 'name tab',
				115: 'cat',
				116: 'cat dat',
				117: 'rsrvd',
				118: 'rsrvd',
				119: 'bounding hist',
				120: 'rsrvd',
				121: 'rsrvd',
				122: 'push attrib',
				123: 'pop attrib',
				124: 'rsrvd',
				125: 'rsrvd',
				126: 'curv',
				127: 'road const',
				128: 'light pnt appear pal',
				129: 'light pnt anim pal',
				130: 'indexed lp',
				131: 'lp sys',
				132: 'indx str',
				133: 'shdr pal'}

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
						print opcode_name[opcode], 'not handled'                        
					
	def get_level(self):
		return self.level
		
	def parse_long_id(self):
		self.props['id'] = self.header.fw.read_string(self.header.fw.get_length()-4)
		return True

	def parse_comment(self):
		self.props['comment'] = self.header.fw.read_string(self.header.fw.get_length()-4)
		return True

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
		if not v.flags & 0x2000: # 0x2000 = no color
			if v.flags & 0x1000: # 0x1000 = packed color
				v.a = self.header.fw.read_uchar()
				v.b = self.header.fw.read_uchar()
				v.g = self.header.fw.read_uchar()
				v.r = self.header.fw.read_uchar()
			else:
				self.header.fw.read_ahead(4)
			
			color_index = self.header.fw.read_uint()
			v.r, v.g, v.b, v.a= self.header.get_color(color_index)
		
		self.vert_desc_lst.append(v)
		
		return True

	def parse_vertex_c(self):
		v = self.parse_vertex_common()

		self.parse_vertex_post_common(v)
		
		return True

	def parse_vertex_cn(self):
		v = self.parse_vertex_common()
		
		'''
		v.nx = self.header.fw.read_float()
		v.ny = self.header.fw.read_float()
		v.nz = self.header.fw.read_float()
		'''
		# Just to advance
		self.header.fw.read_float()
		self.header.fw.read_float()
		self.header.fw.read_float()
		
		self.parse_vertex_post_common(v)
		
		return True

	def parse_vertex_cuv(self):
		v = self.parse_vertex_common()

		v.uv[:] = self.header.fw.read_float(), self.header.fw.read_float()

		self.parse_vertex_post_common(v)
		
		return True

	def parse_vertex_cnuv(self):
		v = self.parse_vertex_common()
		'''
		v.nx = self.header.fw.read_float()
		v.ny = self.header.fw.read_float()
		v.nz = self.header.fw.read_float()
		'''
		# Just to advance
		self.header.fw.read_float()
		self.header.fw.read_float()
		self.header.fw.read_float()
		
		v.uv[:] = self.header.fw.read_float(), self.header.fw.read_float()

		self.parse_vertex_post_common(v)
		
		return True

	def parse(self): # Run once per import
		Node.parse(self)

class InterNode(Node):
	def __init__(self):
		self.object = None
		self.mesh = None
		self.isMesh = False
		self.faceLs= []
		self.matrix = None
	
	def blender_import_my_faces(self):
		
		# Add the verts onto the mesh
		mesh = self.mesh
		blender_verts= self.header.vert_pal.blender_verts
		vert_desc_lst= self.header.vert_pal.vert_desc_lst
		
		vert_list= [ i for flt_face in self.faceLs for i in flt_face.indices]
		
		mesh.verts.extend([blender_verts[i] for i in vert_list])
		
		
		new_faces= []
		new_faces_props= []
		ngon= BPyMesh.ngon
		vert_index= 1
		for flt_face in self.faceLs:
			material_index= flt_face.blen_mat_idx
			image= flt_face.blen_image
			
			face_len= len(flt_face.indices)
			
			# Get the indicies in reference to the mesh.
			
			uvs= [vert_desc_lst[j].uv for j in flt_face.indices]
			if face_len <=4: # tri or quad
				new_faces.append( [i+vert_index for i in xrange(face_len)] )
				new_faces_props.append((material_index, image, uvs))
			
			else: # fgon
				mesh_face_indicies = [i+vert_index for i in xrange(face_len)]
				tri_ngons= ngon(mesh, mesh_face_indicies)
				new_faces.extend([ [mesh_face_indicies[t] for t in tri] for tri in tri_ngons])
				new_faces_props.extend( [ (material_index, image, (uvs[tri[0]], uvs[tri[1]], uvs[tri[2]]) ) for tri in tri_ngons ] )
			
			vert_index+= face_len
		
		mesh.faces.extend(new_faces)
		
		try:	mesh.faceUV= True
		except:	pass
		
		for i, f in enumerate(mesh.faces):
			f.mat, f.image, f.uv= new_faces_props[i]
	
	def blender_import(self):
#        name = self.props['type'] + ': ' + self.props['id']
		name = self.props['id']
		if self.isMesh:
			self.object = Blender.Object.New('Mesh', name)
			#self.mesh = self.object.getData()
			self.mesh = Blender.Mesh.New()
			self.mesh.verts.extend( Vector() ) # DUMMYVERT
			self.object.link(self.mesh)
		else:
			self.object = Blender.Object.New('Empty', name)

		if self.parent:
			self.parent.object.makeParent([self.object])

		scene.link(self.object)
		self.object.Layer = current_layer
		self.object.sel = 1
		
		Node.blender_import(self) # Attach faces to self.faceLs
		
		if self.isMesh:
			# Add all my faces into the mesh at once
			self.blender_import_my_faces()
		
		if self.matrix:
			self.object.setMatrix(self.matrix)
		
		# Attach properties
		#for name, value in self.props.items():
		#    self.object.addProperty(name, value)
		
	def parse_face(self):
		child = Face(self)
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
		
EDGE_FGON= Blender.Mesh.EdgeFlags['FGON']
FACE_TEX= Blender.Mesh.FaceModes['TEX']

class Face(Node):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		self.root_handler.set_handler({31: self.parse_comment,
									   10: self.parse_push})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({72: self.parse_vertex_list,
										10: self.parse_push,
										11: self.parse_pop})
		
		if parent:
			parent.isMesh = True

		self.indices =  list() # face verts here
		
		self.comment = ''
		self.props = dict.fromkeys(['ir color', 'priority', 
									'draw type', 'texture white', 'template billboard',
									'smc', 'fid', 'ir material', 'lod generation control',
									'flags', 'light mode'])
		
		self.header.fw.read_ahead(8) # face id
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
	
	
	"""
	def blender_import_face(self, material_index, image):
		
		
		mesh = self.parent.mesh
		face_len= len(self.indices)
		
		mesh_vert_len_orig= len(mesh.verts)
		mesh.verts.extend([ self.header.vert_pal.blender_verts[i] for i in self.indices])
		
		# Exception for an edge
		if face_len==2:
			mesh.edges.extend((mesh.verts[-1], mesh.verts[-2]))
			return
		
		
		mesh_face_indicies = range(mesh_vert_len_orig, mesh_vert_len_orig+face_len)
		
		#print mesh_face_indicies , 'mesh_face_indicies '
		
		# First we need to triangulate NGONS
		if face_len>4:
			tri_indicies = [[i+mesh_vert_len_orig for i in t] for t in BPyMesh.ngon(mesh, mesh_face_indicies) ]   # use range because the verts are in order.
		else:
			tri_indicies= [mesh_face_indicies] # can be a quad but thats ok
		
		# Extend face or ngon
		
		mesh.faces.extend(tri_indicies)
		#print mesh.faces, 'mesh.faces'
		mesh.faceUV= True
		
		# Now set UVs
		for i in xrange(len(mesh.faces)-len(tri_indicies), len(mesh.faces)):
			f= mesh.faces[i]
			f_v= f.v
			for j, uv in enumerate(f.uv):
				vertex_index_flt= self.indices[f_v[j].index - mesh_vert_len_orig]
				
				vert_desc = self.header.vert_pal.vert_desc_lst[vertex_index_flt]
				uv.x, uv.y= vert_desc.u, vert_desc.v
			
			# Only a bug in 2.42, fixed in cvs
			for c in f.col:
				c.r=c.g=c.b= 255
			
			f.mat = material_index
			if image:
				f.image = image
			else:
				f.mode &= ~FACE_TEX
		
		# FGon
		
		if face_len>4:
			# Add edges we know are not fgon
			end_index= len(mesh.verts)
			start_index= end_index - len(self.indices)
			edge_dict= dict([ ((i, i+1), None) for i in xrange(start_index, end_index-1)])
			edge_dict[(start_index, end_index)]= None # wish this was a set
			
			fgon_edges= {}
			for tri in tri_indicies:
				for i in (0,1,2):
					i1= tri[i]
					i2= tri[i-1]
					
					# Sort
					if i1>i2:
						i1,i2= i2,i1
					
					if not edge_dict.has_key( (i1,i2) ): # if this works its an edge vert
						fgon_edges[i1,i2]= None
			
			
			# Now set fgon flags
			for ed in mesh.edges:
				i1= ed.v1.index
				i2= ed.v2.index
				if i1>i2:
					i1,i2= i2,i1
				
				if fgon_edges.has_key( (i1,i2) ):
					# This is an edge tagged for fgonning?
					fgon_edges[i1, i2]
					ed.flag |= EDGE_FGON
					del fgon_edges[i1, i2] # make later searches faster?
				
				if not fgon_edges:
					break
	"""
	
	def parse_comment(self):
		self.comment = self.header.fw.read_string(self.header.fw.get_length()-4)
		return True
		
	# returns a tuple (material, image) where material is the blender material and
	# image is the blender image or None.
	def create_blender_material(self):
		 # Create face material.
		mat_desc = MaterialDesc()
		
		if self.mat_index != -1:
			if not self.mat_index in self.header.mat_desc_pal:
				if global_prefs['verbose'] >= 1:
					#print 'Warning: Material index', self.mat_index, 'not in material palette.'
					pass
			else:
				mat_pal_desc = self.header.mat_desc_pal[self.mat_index]
				mat_desc.alpha = mat_pal_desc.alpha * self.alpha # combine face and mat alphas
				mat_desc.ambient = mat_pal_desc.ambient
				mat_desc.diffuse = mat_pal_desc.diffuse
				mat_desc.specular = mat_pal_desc.specular
				mat_desc.emissive = mat_pal_desc.emissive
				mat_desc.shininess = mat_pal_desc.shininess
		else:
			# if no material get alpha from just face.
			mat_desc.alpha = self.alpha

		# Color.
		if global_prefs['color_from_face']:
			color = None
			if not self.props['flags'] & 0x40000000:
				if self.props['flags'] & 0x10000000: # packed color
					color = self.packed_color
				else:
					color = self.header.get_color(self.color_index)

			if color:
				r = float(color[0])/255.0
				g = float(color[1])/255.0
				b = float(color[2])/255.0
				mat_desc.diffuse = [r, g, b]

		# Texture
		image = None
		if self.tex_index != -1 and self.tex_index in self.header.bl_tex_pal:
			mat_desc.tex0 = self.header.bl_tex_pal[self.tex_index]
			if mat_desc.tex0:
				mat_desc.name = FF.strip_path(self.header.tex_pal[self.tex_index])
				image = mat_desc.tex0.image

		# OpenFlight Face Attributes
		mat_desc.face_props = self.props
		
		# Get material.
		mat = GRR.request_mat(mat_desc)
		
		# Add material to mesh.
		mesh = self.parent.mesh
		
		# Return where it is in the mesh for faces.
		mesh_materials= mesh.materials
		
		material_index= -1
		for i,m in enumerate(mesh_materials):
			if m.name==mat.name:
				material_index= i
				break
		
		if material_index==-1:
			material_index= len(mesh_materials)
			if material_index==16:
				material_index= 15
				if global_prefs['verbose'] >= 1:
					print 'Warning: Too many materials per mesh object. Only a maximum of 16 ' + \
						  'allowed. Using 16th material instead.'
			
			else:
				mesh_materials.append(mat)
				mesh.materials= mesh_materials
		
		return (material_index, image)
	
	
	def blender_import(self):
		vert_count = len(self.indices)
		if vert_count < 3:
			if global_prefs['verbose'] >= 2:
				print 'Warning: Ignoring face with no vertices.'
			return
		
		# Assign material and image
		
		self.parent.faceLs.append(self)
		self.blen_mat_idx, self.blen_image= self.create_blender_material()
		
		
		
		
		# Store comment info in parent.
		if self.comment != '':
			if self.parent.props['comment'] != '':
				self.parent.props['comment'] += '\n\nFrom Face:\n' + self.comment
			else:
				self.parent.props['comment'] = self.comment
		
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


class Object(InterNode):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({5: self.parse_face,
										#130: self.parse_indexed_light_point,
										#111: self.parse_inline_light_point,
										10: self.parse_push,
										11: self.parse_pop})

		self.props['type'] = 'Object'
		self.props['id'] = self.header.fw.read_string(8)



class Group(InterNode):
	def __init__(self, parent):
		Node.__init__(self, parent, parent.header)
		InterNode.__init__(self)
		
		self.root_handler.set_handler({33: self.parse_long_id,
									   31: self.parse_comment,
									   10: self.parse_push,
									   49: self.parse_matrix})
		self.root_handler.set_throw_back_lst(throw_back_opcodes)
		
		self.child_handler.set_handler({5: self.parse_face,
										#130: self.parse_indexed_light_point,
										#111: self.parse_inline_light_point,
										2: self.parse_group,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled,
										14: self.parse_unhandled,
										91: self.parse_unhandled,
										98: self.parse_unhandled,
										63: self.parse_xref})
		self.props = dict.fromkeys(['type', 'id', 'comment', 'priority', 'flags', 'special1',
									'special2', 'significance', 'layer code', 'loop count',
									'loop duration', 'last frame duration'])
		
		self.props['type'] = 'Group'
		self.props['comment'] = ''
		self.props['id'] = self.header.fw.read_string(8)
		self.props['priority'] = self.header.fw.read_short()
		self.header.fw.read_ahead(2)
		self.props['flags'] = self.header.fw.read_int()
		self.props['special1'] = self.header.fw.read_short()
		self.props['special2'] = self.header.fw.read_short()
		self.props['significance'] = self.header.fw.read_short()
		self.props['layer code'] = self.header.fw.read_char()
		self.header.fw.read_ahead(5)
		self.props['loop count'] = self.header.fw.read_int()
		self.props['loop duration'] = self.header.fw.read_float()
		self.props['last frame duration'] = self.header.fw.read_float()               

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
		
		xref_filename = self.header.fw.read_string(200)
		filename = FF.find(xref_filename)

		self.props['type'] = 'XRef'
		
		if filename != None:
			self.xref = Database(filename, self)
			self.props['id'] = 'X: ' + Blender.sys.splitext(Blender.sys.basename(filename))[0]
		else:
			self.xref = None
			self.props['id'] = 'X: broken'

class LOD(InterNode):
	def blender_import(self):
		self.move_to_next_layer()
		InterNode.blender_import(self)

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
										14: self.parse_unhandled, # DOF
										91: self.parse_unhandled, # sound
										98: self.parse_unhandled, # clip
										63: self.parse_xref})

		self.props['type'] = 'LOD'
		self.props['id'] = self.header.fw.read_string(8)

class InlineLightPoint(InterNode):
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
		self.app_props = dict()
		
		self.props['comment'] = ''
		self.props['type'] = 'Light Point'
		self.props['id'] = self.header.fw.read_string(8)
		
		self.app_props.update({'smc': self.header.fw.read_short()})
		self.app_props.update({'fid': self.header.fw.read_short()})
		self.app_props.update({'back color: a': self.header.fw.read_uchar()})
		self.app_props.update({'back color: b': self.header.fw.read_uchar()})
		self.app_props.update({'back color: g': self.header.fw.read_uchar()})
		self.app_props.update({'back color: r': self.header.fw.read_uchar()})
		self.app_props.update({'display mode': self.header.fw.read_int()})
		self.app_props.update({'intensity': self.header.fw.read_float()})
		self.app_props.update({'back intensity': self.header.fw.read_float()})
		self.app_props.update({'minimum defocus': self.header.fw.read_float()})
		self.app_props.update({'maximum defocus': self.header.fw.read_float()})
		self.app_props.update({'fading mode': self.header.fw.read_int()})
		self.app_props.update({'fog punch mode': self.header.fw.read_int()})
		self.app_props.update({'directional mode': self.header.fw.read_int()})
		self.app_props.update({'range mode': self.header.fw.read_int()})
		self.app_props.update({'min pixel size': self.header.fw.read_float()})
		self.app_props.update({'max pixel size': self.header.fw.read_float()})
		self.app_props.update({'actual size': self.header.fw.read_float()})
		self.app_props.update({'trans falloff pixel size': self.header.fw.read_float()})
		self.app_props.update({'trans falloff exponent': self.header.fw.read_float()})
		self.app_props.update({'trans falloff scalar': self.header.fw.read_float()})
		self.app_props.update({'trans falloff clamp': self.header.fw.read_float()})
		self.app_props.update({'fog scalar': self.header.fw.read_float()})
		self.app_props.update({'fog intensity': self.header.fw.read_float()})
		self.app_props.update({'size threshold': self.header.fw.read_float()})
		self.app_props.update({'directionality': self.header.fw.read_int()})
		self.app_props.update({'horizontal lobe angle': self.header.fw.read_float()})
		self.app_props.update({'vertical lobe angle': self.header.fw.read_float()})
		self.app_props.update({'lobe roll angle': self.header.fw.read_float()})
		self.app_props.update({'dir falloff exponent': self.header.fw.read_float()})
		self.app_props.update({'dir ambient intensity': self.header.fw.read_float()})
		self.header.fw.read_ahead(12) # Animation settings.        
		self.app_props.update({'significance': self.header.fw.read_float()})
		self.props['draw order'] = self.header.fw.read_int()
		self.app_props.update({'flags': self.header.fw.read_int()})
		#self.fw.read_ahead(12) # More animation settings.  
	
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
			
			app_name = GRR.request_lightpoint_app(app_desc)

			if name_to_indices.get(app_name):
				name_to_indices[app_name].append(i)
			else:
				name_to_indices.update({app_name: [i]})
			
		return name_to_indices
		
	def blender_import(self):
		name = '%s: %s' % (self.props['type'], self.props['id'])

		name_to_indices = self.group_points(self.app_props)

		for app_name, indices in name_to_indices.iteritems():
			self.object = Blender.Object.New('Mesh', name)
			#self.mesh = self.object.getData()
			self.mesh= Blender.Mesh.New()
			self.mesh.verts.extend( Vector() ) # DUMMYVERT
			self.object.link(self.mesh)

			if self.parent:
				self.parent.object.makeParent([self.object])
				
			for i in indices:
				vert = self.header.vert_pal.blender_verts[i]
				self.mesh.verts.append(vert)
			
			scene.link(self.object)
			self.object.Layer = current_layer
			self.object.sel= 1
			
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
			
			app_name = GRR.request_lightpoint_app(app_desc)

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
			#self.mesh = self.object.getData()
			self.mesh= Blender.Mesh.New()
			self.mesh.verts.extend( Vector() ) # DUMMYVERT
			self.object.link(self.mesh)
			
			if self.parent:
				self.parent.object.makeParent([self.object])
				
			for i in indices:
				vert = self.header.vert_pal.blender_verts[i]
				self.mesh.verts.append(vert)
			
			scene.link(self.object)
	
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
										14: self.parse_unhandled, # DOF
										91: self.parse_unhandled, # sound
										98: self.parse_unhandled, # clip
										63: self.parse_xref})

		self.props['id'] = self.header.fw.read_string(8)

class Database(InterNode):
	def blender_import(self):
		self.tex_pal = dict(self.tex_pal_lst)
		del self.tex_pal_lst

		# Setup Textures
		bl_tex_pal_lst = list()
		for i in self.tex_pal.iterkeys():
			path_filename = FF.find(self.tex_pal[i])
			if path_filename != None:
				img = GRR.request_image(path_filename)
				if img:
					tex = GRR.request_texture(img)
					tex.setName(FF.strip_path(self.tex_pal[i]))
					bl_tex_pal_lst.append( (i, tex) )
				else:
					bl_tex_pal_lst.append( (i, None) )
			elif global_prefs['verbose'] >= 1:
				print 'Warning: Unable to find', self.tex_pal[i]

		self.bl_tex_pal = dict(bl_tex_pal_lst)

		# Setup Materials
		self.mat_desc_pal = dict(self.mat_desc_pal_lst)

		InterNode.blender_import(self)

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
		self.tex_pal_lst.append( (index, name) )
		return True
		
	def __init__(self, filename, parent=None):
		if global_prefs['verbose'] >= 1:
			print 'Parsing:', filename
			print
		
		self.fw = flt_filewalker.FltIn(filename)
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
										#111: self.parse_inline_light_point,
										2: self.parse_group,
										73: self.parse_lod,
										4: self.parse_object,
										10: self.parse_push,
										11: self.parse_pop,
										96: self.parse_unhandled,
										14: self.parse_unhandled,
										91: self.parse_unhandled,
										98: self.parse_unhandled,
										63: self.parse_xref})
		
		self.vert_pal = None
		self.lightpoint_appearance_pal = dict()
		self.tex_pal = dict()
		self.tex_pal_lst = list()
		self.bl_tex_pal = dict()
		self.col_pal = list()
		self.mat_desc_pal_lst = list()
		self.mat_desc_pal = dict()
		self.props = dict.fromkeys(['id', 'type', 'comment', 'version', 'units', 'set white',
			'flags', 'projection type', 'sw x', 'sw y', 'dx', 'dy', 'dz', 'sw lat',
			'sw lon', 'ne lat', 'ne lon', 'origin lat', 'origin lon', 'lambert lat1',
			'lambert lat2', 'ellipsoid model', 'utm zone', 'radius', 'major axis', 'minor axis'])

def select_file(filename):
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
	
	# Start loading the file,
	# first set the context
	Blender.Window.WaitCursor(True)
	Blender.Window.EditMode(0)
	for ob in scene.objects:
		ob.sel=0
	
	
	FF.add_file_to_search_path(filename)
	
	if global_prefs['verbose'] >= 1:
		print 'Pass 1: Loading.'
		print

	load_time = Blender.sys.time()    
	db = Database(filename)
	db.parse()
	load_time = Blender.sys.time() - load_time

	if global_prefs['verbose'] >= 1:
		print
		print 'Pass 2: Importing to Blender.'
		print

	import_time = Blender.sys.time()
	db.blender_import()
	import_time = Blender.sys.time() - import_time
	
	Blender.Window.ViewLayer(range(1,21))
	
	# FIX UP AFTER DUMMY VERT AND REMOVE DOUBLES
	Blender.Mesh.Mode(Blender.Mesh.SelectModes['VERTEX'])
	for ob in scene.objects.context:
		if ob.type=='Mesh':
			me=ob.getData(mesh=1)
			me.verts.delete(0) # remove the dummy vert
			me.sel= 1
			me.remDoubles(0.0001)
	
	
	
	Blender.Window.RedrawAll()
		
	if global_prefs['verbose'] >= 1:
		print 'Done.'
		print
		print 'Time to parse file: %.3f seconds' % load_time
		print 'Time to import to blender: %.3f seconds' % import_time
		print 'Total time: %.3f seconds' % (load_time + import_time)
	
	Blender.Window.WaitCursor(False)


if global_prefs['verbose'] >= 1:
	print
	print 'OpenFlight Importer'
	print 'Version:', __version__
	print 'Author: Greg MacDonald'
	print __url__[2]
	print


if __name__ == '__main__':
	Blender.Window.FileSelector(select_file, "Import OpenFlight", "*.flt")
	#select_file('/fe/flt/helnwsflt/helnws.flt')
	#select_file('/fe/flt/Container_006.flt')
	#select_file('/fe/flt/NaplesORIGINALmesh.flt')
	#select_file('/Anti_tank_D30.flt')
	#select_file('/metavr/file_examples/flt/cherrypoint/CherryPoint_liter_runway.flt')


"""
TIME= Blender.sys.time()
import os
PATH= 'c:\\flt_test'
for FNAME in os.listdir(PATH):
	if FNAME.lower().endswith('.flt'):
		FPATH= os.path.join(PATH, FNAME)
		newScn= Blender.Scene.New(FNAME)
		newScn.makeCurrent()
		scene= newScn
		select_file(FPATH)

print 'TOTAL TIME: %.6f' % (Blender.sys.time() - TIME)
"""
	