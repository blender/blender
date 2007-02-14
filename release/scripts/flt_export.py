#!BPY
""" Registration info for Blender menus:
Name: 'OpenFlight (.flt)...'
Blender: 237
Group: 'Export'
Tip: 'Export to OpenFlight v16.0 (.flt)'
"""

__author__ = "Greg MacDonald"
__version__ = "1.2 10/20/05"
__url__ = ("blender", "elysiun", "Author's homepage, http://sourceforge.net/projects/blight/")
__bpydoc__ = """\
This script exports v16.0 OpenFlight files.  OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.

Run from "File->Export" menu. 

Options are available from Blender's "Scripts Config Editor," accessible through
the "Scripts->System" menu from the scripts window.

Features:<br>
* Heirarchy retained.<br>
* Normals retained.<br>
* First texture exported.<br>
* Diffuse material color is exported as the face color, material color, or both
depending on the option settings.<br>
* Double sided faces are exported as two faces.<br>
* Object transforms exported.

Things To Be Aware Of:<br>
* Object names are exported, not mesh or data names.
* Material indices that don't have a material associated with them will confuse the
exporter. If a warning appears about this, correct it by deleting the offending
material indices in Blender.

What's Not Handled:<br>
* Animations.<br>
* Vetex colors.<br>
"""

# flt_export.py is an OpenFlight exporter for blender.
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
from flt_filewalker import FltOut

class ExporterOptions:
	def __init__(self):
		self.defaults = { 'Diffuse Color To OpenFlight Material': False,
						  'Diffuse Color To OpenFlight Face': True}
		
		d = Blender.Registry.GetKey('flt_export', True)
		
		if d == None or d.keys() != self.defaults.keys():
			d = self.defaults
			Blender.Registry.SetKey('flt_export', d, True)
		
		self.verbose = 1
		self.tolerance = 0.001
		self.use_mat_color = d['Diffuse Color To OpenFlight Material']
		self.use_face_color = d['Diffuse Color To OpenFlight Face']
		
options = ExporterOptions()

FLOAT_TOLERANCE = options.tolerance

identity_matrix = [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]

def is_identity(m):
	for i in xrange(4):
		for j in xrange(4):
			if abs(m[i][j] - identity_matrix[i][j]) > FLOAT_TOLERANCE:
				return False
	return True

class MaterialDesc:
	def __init__(self):
		self.name = 'Blender'

		# Colors, List of 3 floats.
		self.diffuse = [1.0, 1.0, 1.0]
		self.specular = [1.0, 1.0, 1.0]

		# Scalars
		self.ambient = 0.1 # [0.0, 1.0]
		self.emissive = 0.0 # [0.0, 1.0]
		self.shininess = 32.0 # Range is [0.0, 128.0]
		self.alpha = 1.0 # Range is [0.0, 1.0]

class VertexDesc:
	def __init__(self, co=None, no=None, uv=None):
		if co: self.x, self.y, self.z = tuple(co)
		else: self.x = self.y = self.z = 0.0
		if no: self.nx, self.ny, self.nz = tuple(no)
		else: self.nx = self.ny = self.nz = 0.0
		if uv: self.u, self.v = tuple(uv)
		else: self.u = self.v = 0.0

class GlobalResourceRepository:
	def new_face_name(self):
		self.face_name += 1
		return 'f%i' % (self.face_name-1)

	def vertex_count(self):
		return len(self.vertex_lst)

	def request_vertex_desc(self, i):
		return self.vertex_lst[i]

	def request_vertex_index(self, desc):
		match = None
		for i, v in enumerate(self.vertex_lst):
			if\
			abs(v.x - desc.x) > FLOAT_TOLERANCE or\
			abs(v.y - desc.y) > FLOAT_TOLERANCE or\
			abs(v.z - desc.z) > FLOAT_TOLERANCE or\
			abs(v.nx - desc.nx) > FLOAT_TOLERANCE or\
			abs(v.ny - desc.ny) > FLOAT_TOLERANCE or\
			abs(v.nz - desc.nz) > FLOAT_TOLERANCE or\
			abs(v.u - desc.u) > FLOAT_TOLERANCE or\
			abs(v.v - desc.v) > FLOAT_TOLERANCE:
				pass
			else:
				match = i
				break

		if match != None:
			return match
		else:
			self.vertex_lst.append(desc)
			return len(self.vertex_lst) - 1

	def request_texture_index(self, filename):
		match = None
		for i in xrange(len(self.texture_lst)):
			if self.texture_lst[i] != filename:
				continue
			match = i
			break
		if match != None:
			return match
		else:
			self.texture_lst.append(filename)
			return len(self.texture_lst) - 1

	def request_texture_filename(self, index):
		return self.texture_lst[index]

	def texture_count(self):
		return len(self.texture_lst)

	def request_material_index(self, desc):
		match = None
		for i in xrange(len(self.material_lst)):
			if self.material_lst[i].diffuse != desc.diffuse:
				continue
			if self.material_lst[i].specular != desc.specular:
				continue
			if self.material_lst[i].ambient != desc.ambient:
				continue
			if self.material_lst[i].emissive != desc.emissive:
				continue
			if self.material_lst[i].shininess != desc.shininess:
				continue
			if self.material_lst[i].alpha != desc.alpha:
				continue
			match = i
			break

		if match != None:
			return i
		else:
			self.material_lst.append(desc)
			return len(self.material_lst) - 1

	def request_material_desc(self, index):
		return self.material_lst[index]

	def material_count(self):
		return len(self.material_lst)

	# Returns not actual index but one that includes intensity information.
	# color_index = 127*intensity + 128*actual_index
	def request_color_index(self, col):
		r,g,b = tuple(col)
		m = max(r, g, b)
		if m > 0.0:
			intensity = m / 1.0
			r = int(round(r/m * 255.0))
			g = int(round(g/m * 255.0))
			b = int(round(b/m * 255.0))
			brightest = [r, g, b]
		else:
			brightest = [255, 255, 255]
			intensity = 0.0

		match = None
		for i in xrange(len(self.color_lst)):
			if self.color_lst[i] != brightest:
				continue

			match = i
			break

		if match != None:
			index = match
		else:
			length = len(self.color_lst)
			if length <= 1024:
				self.color_lst.append(brightest)
				index = length
			else:
				if options.verbose >= 1:
					print 'Warning: Exceeded max color limit.'
				index = 0

		color_index = int(round(127.0*intensity)) + 128*index
		return color_index

	# Returns color from actual index.
	def request_max_color(self, index):
		return self.color_lst[index]

	def color_count(self):
		return len(self.color_lst)

	def __init__(self):
		self.vertex_lst = []
		self.texture_lst = []
		self.material_lst = []
		self.color_lst = [[255, 255, 255]]
		self.face_name = 0

class Node:
	# Gathers info from blender needed for export.
	# The =[0] is a trick to emulate c-like static function variables
	# that are persistant between calls.
	def blender_export(self, level=[0]):
		if self.object:
			if options.verbose >= 2:
				print '\t' * level[0], self.name, self.object.type

		level[0] += 1
		
		for child in self.children:
			child.blender_export()
			
		level[0] -= 1

	# Exports this node's info to file.
	def write(self):
		pass

	def write_matrix(self):
		if self.matrix and not is_identity(self.matrix):
			self.header.fw.write_short(49)   # Matrix opcode
			self.header.fw.write_ushort(68)  # Length of record
			for i in xrange(4):
				for j in xrange(4):
					self.header.fw.write_float(self.matrix[i][j])

	def write_push(self):
		self.header.fw.write_short(10)
		self.header.fw.write_ushort(4)

	def write_pop(self):
		self.header.fw.write_short(11)
		self.header.fw.write_ushort(4)

	def write_longid(self, name):
		length = len(name)
		if length >= 8:
			self.header.fw.write_short(33)              # Long ID opcode
			self.header.fw.write_ushort(length+5)       # Length of record
			self.header.fw.write_string(name, length+1) # name + zero terminator

	# Initialization sets up basic tree structure.
	def __init__(self, parent, header, object, object_lst):
		self.header = header
		self.object = object
		if object:
			self.name = self.object.name
			self.matrix = self.object.getMatrix('localspace')
		else:
			self.name = 'no name'
			self.matrix = None

		self.children = []
		self.parent = parent
		if parent:
			parent.children.append(self)

		left_over = object_lst[:]
		self.child_objects = []

		# Add children to child list and remove from left_over list.
		
		# Pop is faster then remove
		i = len(object_lst)
		while i:
			i-=1
			if object_lst[i].parent == object:
				self.child_objects.append(left_over.pop(i))
			
		# Spawn children.
		self.has_object_child = False # For Database class.
		for child in self.child_objects:			
			if child.type == 'Mesh':
				BlenderMesh(self, header, child, left_over)
				self.has_object_child = True
			else: # Treat all non meshes as emptys
				BlenderEmpty(self, header, child, left_over)

class FaceDesc:
	def __init__(self):
		self.vertex_index_lst = []
		self.texture_index = -1
		self.material_index = -1
		self.color_index = 127
	
class BlenderMesh(Node):
	def blender_export(self):
		Node.blender_export(self)

		mesh = self.object.getData()
		mesh_hasuv = mesh.hasFaceUV()
		# Gather materials and textures.
		tex_index_lst = []
		mat_index_lst = []
		color_index_lst = []
		materials = mesh.getMaterials()
		
		if not materials:
			materials = [Blender.Material.New()]
		
		for mat in materials:
			# Gather Color.
			if options.use_face_color:
				color_index_lst.append(self.header.GRR.request_color_index(mat.getRGBCol()))
			else:
				color_index_lst.append(127) # white
			# Gather Texture.
			mtex_lst = mat.getTextures()

			index = -1
			mtex = mtex_lst[0] # Not doing multi-texturing at the moment.
			if mtex != None:
				tex = mtex_lst[0].tex
				if tex != None:
					image = tex.getImage()
					if image != None:
						filename = image.getFilename()
						index = self.header.GRR.request_texture_index(filename)

			tex_index_lst.append(index)

			# Gather Material
			mat_desc = MaterialDesc()
			mat_desc.name = mat.name
			mat_desc.alpha = mat.getAlpha()
			mat_desc.shininess = mat.getSpec() * 64.0   # 2.0 => 128.0
			if options.use_mat_color:
				mat_desc.diffuse = mat.getRGBCol()
			else:
				mat_desc.diffuse = [1.0, 1.0, 1.0]

			mat_desc.specular = mat.getSpecCol()
			amb = mat.getAmb()
			mat_desc.ambient = [amb, amb, amb]
			emit = mat.getEmit()
			mat_desc.emissive = [emit, emit, emit]

			mat_index_lst.append(self.header.GRR.request_material_index(mat_desc))

		# Faces described as lists of indices into the GRR's vertex_lst.
		for face in mesh.faces:
			
			face_v = face.v # Faster access
			
			# Create vertex description list for each face.
			if mesh_hasuv:
				vertex_lst = [VertexDesc(v.co, v.no, face.uv[i]) for i, v in enumerate(face_v)]
			else:
				vertex_lst = [VertexDesc(v.co, v.no) for i, v in enumerate(face_v)]
			
			index_lst = []
			for vert_desc in vertex_lst:
				index_lst.append(self.header.GRR.request_vertex_index(vert_desc))
			
			face_desc = FaceDesc()
			face_desc.vertex_index_lst = index_lst
			
			if face.materialIndex < len(materials):
				face_desc.color_index    = color_index_lst[face.materialIndex]
				face_desc.texture_index  = tex_index_lst[face.materialIndex]
				face_desc.material_index = mat_index_lst[face.materialIndex]
			else:
				if options.verbose >=1:
					print 'Warning: Missing material for material index. Materials will not be imported correctly. Fix by deleting abandoned material indices in Blender.'

			self.face_lst.append(face_desc)
			
			# Export double sided face as 2 faces with opposite orientations.
			if mesh_hasuv and face.mode & Blender.NMesh.FaceModes['TWOSIDE']:
				# Create vertex description list for each face. they have a face mode, so we know they have a UV too.
				vertex_lst = [VertexDesc(v.co, -v.no, face.uv[i]) for i, v in enumerate(face_v)]
				vertex_lst.reverse() # Reversing flips the face.
				
				index_lst = []
				for vert_desc in vertex_lst:
					index_lst.append(self.header.GRR.request_vertex_index(vert_desc))
				
				face_desc = FaceDesc()
				face_desc.vertex_index_lst = index_lst
				if face.materialIndex < len(materials):
					face_desc.color_index = color_index_lst[face.materialIndex]
					face_desc.texture_index = tex_index_lst[face.materialIndex]
					face_desc.material_index = mat_index_lst[face.materialIndex]
				else:
					if options.verbose >=1:
						print 'Error: No material for material index. Delete abandoned material indices in Blender.'
	
				self.face_lst.append(face_desc)

	def write_faces(self):
		for face_desc in self.face_lst:
			face_name = self.header.GRR.new_face_name()
			
			self.header.fw.write_short(5)                                   # Face opcode
			self.header.fw.write_ushort(80)                                 # Length of record
			self.header.fw.write_string(face_name, 8)                       # ASCII ID
			self.header.fw.write_int(-1)                                    # IR color code
			self.header.fw.write_short(0)                                   # Relative priority
			self.header.fw.write_char(0)                                    # Draw type
			self.header.fw.write_char(0)                                    # Draw textured white.
			self.header.fw.write_ushort(0)                                  # Color name index
			self.header.fw.write_ushort(0)                                  # Alt color name index
			self.header.fw.write_char(0)                                    # Reserved
			self.header.fw.write_char(1)                                    # Template
			self.header.fw.write_short(-1)                                  # Detail tex pat index
			self.header.fw.write_short(face_desc.texture_index)             # Tex pattern index
			self.header.fw.write_short(face_desc.material_index)            # material index
			self.header.fw.write_short(0)                                   # SMC code
			self.header.fw.write_short(0)                                   # Feature code
			self.header.fw.write_int(0)                                     # IR material code
			self.header.fw.write_ushort(0)                                  # transparency 0 = opaque
			self.header.fw.write_uchar(0)                                   # LOD generation control
			self.header.fw.write_uchar(0)                                   # line style index
			self.header.fw.write_int(0x00000000)                            # Flags
			self.header.fw.write_uchar(2)                                   # Light mode
			self.header.fw.pad(7)                                           # Reserved
			self.header.fw.write_uint(-1)                                   # Packed color
			self.header.fw.write_uint(-1)                                   # Packed alt color
			self.header.fw.write_short(-1)                                  # Tex map index
			self.header.fw.write_short(0)                                   # Reserved
			self.header.fw.write_uint(face_desc.color_index)                # Color index
			self.header.fw.write_uint(127)                                  # Alt color index
			self.header.fw.write_short(0)                                   # Reserved
			self.header.fw.write_short(-1)                                  # Shader index

			self.write_longid(face_name)

			self.write_push()

			# Vertex list record
			self.header.fw.write_short(72)                        # Vertex list opcode
			num_verts = len(face_desc.vertex_index_lst)
			self.header.fw.write_ushort(4*num_verts+4)            # Length of record

			for vert_index in face_desc.vertex_index_lst:
				# Offset into vertex palette
				self.header.fw.write_int(vert_index*64+8)

			self.write_pop()

	def write(self):
		if self.open_flight_type == 'Object':
			self.header.fw.write_short(4)               # Object opcode
			self.header.fw.write_ushort(28)             # Length of record
			self.header.fw.write_string(self.name, 8)   # ASCII ID
			self.header.fw.pad(16)
	
			self.write_longid(self.name)
			
			self.write_matrix()
			
			if self.face_lst != []:
				self.write_push()
		
				self.write_faces()
		
				self.write_pop()
		else:
			self.header.fw.write_short(2)               # Group opcode
			self.header.fw.write_ushort(44)             # Length of record
			self.header.fw.write_string(self.name, 8)   # ASCII ID
			self.header.fw.pad(32)
	
			self.write_longid(self.name)
			
			# Because a group can contain faces as well as children.
			self.write_push() 
			
			self.write_faces()
			
			for child in self.children:
				child.write()
			
			self.write_pop()

	def __init__(self, parent, header, object, object_lst):
		Node.__init__(self, parent, header, object, object_lst)
		self.face_lst = []
		
		if self.children:
			self.open_flight_type= 'Group'
		else: # Empty list.
			self.open_flight_type = 'Object'
			

class BlenderEmpty(Node):
	def write(self):
		self.header.fw.write_short(2)               # Group opcode
		self.header.fw.write_ushort(44)             # Length of record
		self.header.fw.write_string(self.name, 8)   # ASCII ID
		self.header.fw.pad(32)

		self.write_longid(self.name)
		
		self.write_matrix()
		
		if self.children: # != []
			self.write_push()
	
			for child in self.children:
				child.write()
				
			self.write_pop()

class Database(Node):
	def write_header(self):
		if options.verbose >= 2:
			print 'Writing header.'
		self.fw.write_short(1)          # Header opcode
		self.fw.write_ushort(324)       # Length of record
		self.fw.write_string('db', 8)   # ASCII ID
		self.fw.write_int(1600)         # Revision Number
		self.fw.pad(44)
		self.fw.write_short(1)          # Unit multiplier.
		self.fw.write_char(0)           # Units, 0 = meters
		self.fw.write_char(0)           # texwhite on new faces 0 = false
		self.fw.write_uint(0x80000000)  # misc flags set to saving vertex normals
		self.fw.pad(24)
		self.fw.write_int(0)            # projection type, 0 = flat earth
		self.fw.pad(30)
		self.fw.write_short(1)          # double precision
		self.fw.pad(140)
		self.fw.write_int(0)            # ellipsoid model, 0 = WSG 1984
		self.fw.pad(52)

	def write_vert_pal(self):
		if options.verbose >= 2:
			print 'Writing vertex palette.'
		# Write record for vertex palette
		self.fw.write_short(67)                             # Vertex palette opcode.
		self.fw.write_short(8)                              # Length of record
		self.fw.write_int(self.GRR.vertex_count() * 64 + 8) # Length of everything.

		# Write records for individual vertices.
		for i in xrange(self.GRR.vertex_count()):
			desc = self.GRR.request_vertex_desc(i)
			self.fw.write_short(70)                         # Vertex with color normal and uv opcode.
			self.fw.write_ushort(64)                        # Length of record
			self.fw.write_ushort(0)                         # Color name index
			self.fw.write_short(0x2000)                     # Flags set to no color
			self.fw.write_double(desc.x)
			self.fw.write_double(desc.y)
			self.fw.write_double(desc.z)
			self.fw.write_float(desc.nx)
			self.fw.write_float(desc.ny)
			self.fw.write_float(desc.nz)
			self.fw.write_float(desc.u)
			self.fw.write_float(desc.v)
			self.fw.pad(12)

	def write_tex_pal(self):
		if options.verbose >= 2:
			print 'Writing texture palette.'
		# Write record for texture palette
		for i in xrange(self.GRR.texture_count()):
			self.fw.write_short(64)                                         # Texture palette opcode.
			self.fw.write_short(216)                                        # Length of record
			self.fw.write_string(self.GRR.request_texture_filename(i), 200) # Filename
			self.fw.write_int(i)                                            # Texture index
			self.fw.write_int(0)                                            # X
			self.fw.write_int(0)                                            # Y

	def write_mat_pal(self):
		if options.verbose >= 2:
			print 'Writing material palette.'
		for i in xrange(self.GRR.material_count()):
			desc = self.GRR.request_material_desc(i)
			self.fw.write_short(113)                # Material palette opcode.
			self.fw.write_short(84)                 # Length of record
			self.fw.write_int(i)                    # Material index
			self.fw.write_string(desc.name, 12)     # Material name
			self.fw.write_uint(0x80000000)          # Flags
			self.fw.write_float(desc.ambient[0])    # Ambient color.
			self.fw.write_float(desc.ambient[1])    # Ambient color.
			self.fw.write_float(desc.ambient[2])    # Ambient color.
			self.fw.write_float(desc.diffuse[0])    # Diffuse color.
			self.fw.write_float(desc.diffuse[1])    # Diffuse color.
			self.fw.write_float(desc.diffuse[2])    # Diffuse color.
			self.fw.write_float(desc.specular[0])   # Specular color.
			self.fw.write_float(desc.specular[1])   # Specular color.
			self.fw.write_float(desc.specular[2])   # Specular color.
			self.fw.write_float(desc.emissive[0])   # Emissive color.
			self.fw.write_float(desc.emissive[1])   # Emissive color.
			self.fw.write_float(desc.emissive[2])   # Emissive color.
			self.fw.write_float(desc.shininess)
			self.fw.write_float(desc.alpha)
			self.fw.write_int(0)                    # Reserved

	def write_col_pal(self):
		if options.verbose >= 2:
			print 'Writing color palette.'
		self.fw.write_short(32)                     # Color palette opcode.
		self.fw.write_short(4228)                   # Length of record
		self.fw.pad(128)
		count = self.GRR.color_count()
		for i in xrange(count):
			col = self.GRR.request_max_color(i)
			self.fw.write_uchar(255)                  # alpha
			self.fw.write_uchar(col[2])               # b
			self.fw.write_uchar(col[1])               # g
			self.fw.write_uchar(col[0])               # r
		self.fw.pad(max(4096-count*4, 0))

	def write(self):
		self.write_header()
		self.write_vert_pal()
		self.write_tex_pal()
		self.write_mat_pal()
		self.write_col_pal()

		# Wrap everything in a group if it has an object child.
		if self.has_object_child:
			self.header.fw.write_short(2)          # Group opcode
			self.header.fw.write_ushort(44)        # Length of record
			self.header.fw.write_string('g1', 8)   # ASCII ID
			self.header.fw.pad(32)
		
		self.write_push()

		for child in self.children:
			child.write()

		self.write_pop()

	def __init__(self, scene, fw):
		self.fw = fw
		self.scene = scene
		self.all_objects = list(scene.objects)
		self.GRR = GlobalResourceRepository()

		Node.__init__(self, None, self, None, self.all_objects)

def fs_callback(filename):
	Blender.Window.WaitCursor(True)
	
	if Blender.sys.exists(filename):
		r = Blender.Draw.PupMenu('Overwrite ' + filename + '?%t|Yes|No')
		if r != 1:
			if options.verbose >= 1:
				print 'Export cancelled.'
			return
	
	time1 = Blender.sys.time() # Start timing
	
	fw = FltOut(filename)

	db = Database(Blender.Scene.GetCurrent(), fw)
	
	if options.verbose >= 1:
		print 'Pass 1: Exporting from Blender.\n'
	
	db.blender_export()
	
	if options.verbose >= 1:
		print 'Pass 2: Writing %s\n' % filename
		
	db.write()

	fw.close_file()
	if options.verbose >= 1:
		print 'Done in %.4f sec.\n' % (Blender.sys.time() - time1)
		
	Blender.Window.WaitCursor(False)

if options.verbose >= 1:
	print '\nOpenFlight Exporter'
	print 'Version:', __version__
	print 'Author: Greg MacDonald'
	print __url__[2]
	print
	
fname = Blender.sys.makename(ext=".flt")
Blender.Window.FileSelector(fs_callback, "Export OpenFlight v16.0", fname)
