#!BPY

"""
Name: 'MD2 (.md2)'
Blender: 239
Group: 'Import'
Tooltip: 'Import from Quake file format (.md2).'
"""

__author__ = 'Bob Holcomb'
__version__ = '0.16'
__url__ = ["Bob's site, http://bane.servebeer.com",
     "Support forum, http://scourage.servebeer.com/phpbb/", "blender", "blenderartists.org"]
__email__ = ["Bob Holcomb, bob_holcomb:hotmail*com", "scripts"]
__bpydoc__ = """\
This script imports a Quake 2 file (MD2), textures, 
and animations into blender for editing.  Loader is based on MD2 loader from www.gametutorials.com-Thanks DigiBen! and the md3 blender loader by PhaethonH <phaethon@linux.ucla.edu><br>

 Additional help from: Shadwolf, Skandal, Rojo and Campbell Barton<br>
 Thanks Guys!
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Bob Holcomb
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


import Blender
from Blender import Mesh, Object, sys
from Blender.BGL import *
from Blender.Draw import *
from Blender.Window import *
from Blender.Mathutils import Vector
import struct
from types import *


######################################################
# Main Body
######################################################

#returns the string from a null terminated string
def asciiz (s):
  n = 0
  while (ord(s[n]) != 0):
    n = n + 1
  return s[0:n]


######################################################
# MD2 Model Constants
######################################################
MD2_MAX_TRIANGLES=4096
MD2_MAX_VERTICES=2048
MD2_MAX_TEXCOORDS=2048
MD2_MAX_FRAMES=512
MD2_MAX_SKINS=32
MD2_MAX_FRAMESIZE=(MD2_MAX_VERTICES * 4 + 128)

######################################################
# MD2 data structures
######################################################
class md2_alias_triangle(object):
	__slots__ = 'vertices', 'lightnormalindex'
	binary_format="<3BB" #little-endian (<), 3 Unsigned char
	
	def __init__(self):
		self.vertices=[0]*3
		self.lightnormalindex=0

	def load(self, file):
		temp_data = file.read(struct.calcsize(self.binary_format))
		data = struct.unpack(self.binary_format, temp_data)
		self.vertices[0]=data[0]
		self.vertices[1]=data[1]
		self.vertices[2]=data[2]
		self.lightnormalindex=data[3]
		return self

	def dump(self):
		print "MD2 Alias_Triangle Structure"
		print "vertex: ", self.vertices[0]
		print "vertex: ", self.vertices[1]
		print "vertex: ", self.vertices[2]
		print "lightnormalindex: ",self.lightnormalindex
		print ""

class md2_face(object):
	
	binary_format="<3h3h" #little-endian (<), 3 short, 3 short
	
	__slots__ = 'vertex_index', 'texture_index'
	
	def __init__(self):
		self.vertex_index = [ 0, 0, 0 ]
		self.texture_index = [ 0, 0, 0]

	def load (self, file):
		temp_data=file.read(struct.calcsize(self.binary_format))
		data=struct.unpack(self.binary_format, temp_data)
		self.vertex_index[0]=data[0]
		self.vertex_index[1]=data[1]
		self.vertex_index[2]=data[2]
		self.texture_index[0]=data[3]
		self.texture_index[1]=data[4]
		self.texture_index[2]=data[5]
		return self

	def dump (self):
		print "MD2 Face Structure"
		print "vertex index: ", self.vertex_index[0]
		print "vertex index: ", self.vertex_index[1]
		print "vertex index: ", self.vertex_index[2]
		print "texture index: ", self.texture_index[0]
		print "texture index: ", self.texture_index[1]
		print "texture index: ", self.texture_index[2]
		print ""

class md2_tex_coord(object):
	__slots__ = 'u', 'v'
	binary_format="<2h" #little-endian (<), 2 unsigned short
	
	def __init__(self):
		self.u=0
		self.v=0

	def load (self, file):
		temp_data=file.read(struct.calcsize(self.binary_format))
		data=struct.unpack(self.binary_format, temp_data)
		self.u=data[0]
		self.v=data[1]
		return self

	def dump (self):
		print "MD2 Texture Coordinate Structure"
		print "texture coordinate u: ",self.u
		print "texture coordinate v: ",self.v
		print ""


class md2_skin(object):
	__slots__ = 'name'
	binary_format="<64s" #little-endian (<), char[64]

	def __init__(self):
		self.name=""

	def load (self, file):
		temp_data=file.read(struct.calcsize(self.binary_format))
		data=struct.unpack(self.binary_format, temp_data)
		self.name=asciiz(data[0])
		return self

	def dump (self):
		print "MD2 Skin"
		print "skin name: ",self.name
		print ""

class md2_alias_frame(object):
	__slots__ = 'scale', 'translate', 'name', 'vertices'
	binary_format="<3f3f16s" #little-endian (<), 3 float, 3 float char[16]
	#did not add the "3bb" to the end of the binary format
	#because the alias_vertices will be read in through
	#thier own loader

	def __init__(self):
		self.scale=[0.0]*3
		self.translate=[0.0]*3
		self.name=""
		self.vertices=[]


	def load (self, file):
		temp_data=file.read(struct.calcsize(self.binary_format))
		data=struct.unpack(self.binary_format, temp_data)
		self.scale[0]=data[0]
		self.scale[1]=data[1]
		self.scale[2]=data[2]
		self.translate[0]=data[3]
		self.translate[1]=data[4]
		self.translate[2]=data[5]
		self.name=asciiz(data[6])
		return self

	def dump (self):
		print "MD2 Alias Frame"
		print "scale x: ",self.scale[0]
		print "scale y: ",self.scale[1]
		print "scale z: ",self.scale[2]
		print "translate x: ",self.translate[0]
		print "translate y: ",self.translate[1]
		print "translate z: ",self.translate[2]
		print "name: ",self.name
		print ""

class md2_obj(object):
	__slots__ =\
	'tex_coords', 'faces', 'frames',\
	'skins', 'ident', 'version',\
	'skin_width', 'skin_height',\
	'frame_size', 'num_skins', 'num_vertices',\
	'num_tex_coords', 'num_faces', 'num_GL_commands',\
	'num_frames', 'offset_skins', 'offset_tex_coords',\
	'offset_faces', 'offset_frames', 'offset_GL_commands'
	
	'''
	#Header Structure
	ident=0				#int 0	This is used to identify the file
	version=0			#int 1	The version number of the file (Must be 8)
	skin_width=0		#int 2	The skin width in pixels
	skin_height=0		#int 3	The skin height in pixels
	frame_size=0		#int 4	The size in bytes the frames are
	num_skins=0			#int 5	The number of skins associated with the model
	num_vertices=0		#int 6	The number of vertices (constant for each frame)
	num_tex_coords=0	#int 7	The number of texture coordinates
	num_faces=0			#int 8	The number of faces (polygons)
	num_GL_commands=0	#int 9	The number of gl commands
	num_frames=0		#int 10	The number of animation frames
	offset_skins=0		#int 11	The offset in the file for the skin data
	offset_tex_coords=0	#int 12	The offset in the file for the texture data
	offset_faces=0		#int 13	The offset in the file for the face data
	offset_frames=0		#int 14	The offset in the file for the frames data
	offset_GL_commands=0#int 15	The offset in the file for the gl commands data
	offset_end=0		#int 16	The end of the file offset
	'''
	binary_format="<17i"  #little-endian (<), 17 integers (17i)

	#md2 data objects

	def __init__ (self):
		self.tex_coords=[]
		self.faces=[]
		self.frames=[]
		self.skins=[]


	def load (self, file):
		temp_data = file.read(struct.calcsize(self.binary_format))
		data = struct.unpack(self.binary_format, temp_data)

		self.ident=data[0]
		self.version=data[1]

		if (self.ident!=844121161 or self.version!=8):
			print "Not a valid MD2 file"
			Exit()

		self.skin_width=data[2]
		self.skin_height=data[3]
		self.frame_size=data[4]

		#make the # of skin objects for model
		self.num_skins=data[5]
		for i in xrange(0,self.num_skins):
			self.skins.append(md2_skin())

		self.num_vertices=data[6]

		#make the # of texture coordinates for model
		self.num_tex_coords=data[7]
		for i in xrange(0,self.num_tex_coords):
			self.tex_coords.append(md2_tex_coord())

		#make the # of triangle faces for model
		self.num_faces=data[8]
		for i in xrange(0,self.num_faces):
			self.faces.append(md2_face())

		self.num_GL_commands=data[9]

		#make the # of frames for the model
		self.num_frames=data[10]
		for i in xrange(0,self.num_frames):
			self.frames.append(md2_alias_frame())
			#make the # of vertices for each frame
			for j in xrange(0,self.num_vertices):
				self.frames[i].vertices.append(md2_alias_triangle())

		self.offset_skins=data[11]
		self.offset_tex_coords=data[12]
		self.offset_faces=data[13]
		self.offset_frames=data[14]
		self.offset_GL_commands=data[15]

		#load the skin info
		file.seek(self.offset_skins,0)
		for i in xrange(0, self.num_skins):
			self.skins[i].load(file)
			#self.skins[i].dump()

		#load the texture coordinates
		file.seek(self.offset_tex_coords,0)
		for i in xrange(0, self.num_tex_coords):
			self.tex_coords[i].load(file)
			#self.tex_coords[i].dump()

		#load the face info
		file.seek(self.offset_faces,0)
		for i in xrange(0, self.num_faces):
			self.faces[i].load(file)
			#self.faces[i].dump()

		#load the frames
		file.seek(self.offset_frames,0)
		for i in xrange(0, self.num_frames):
			self.frames[i].load(file)
			#self.frames[i].dump()
			for j in xrange(0,self.num_vertices):
				self.frames[i].vertices[j].load(file)
				#self.frames[i].vertices[j].dump()
		return self

	def dump (self):
		print "Header Information"
		print "ident: ", self.ident
		print "version: ", self.version
		print "skin width: ", self.skin_width
		print "skin height: ", self.skin_height
		print "frame size: ", self.frame_size
		print "number of skins: ", self.num_skins
		print "number of texture coordinates: ", self.num_tex_coords
		print "number of faces: ", self.num_faces
		print "number of frames: ", self.num_frames
		print "number of vertices: ", self.num_vertices
		print "offset skins: ", self.offset_skins
		print "offset texture coordinates: ", self.offset_tex_coords
		print "offset faces: ", self.offset_faces
		print "offset frames: ",self.offset_frames
		print ""

######################################################
# Import functions
######################################################
def load_textures(md2, texture_filename):
	#did the user specify a texture they wanted to use?
	if texture_filename:
		if (Blender.sys.exists(texture_filename)):
			try:	return Blender.Image.Load(texture_filename)
			except:	return -1 # could not load?
			
	#does the model have textures specified with it?
	if int(md2.num_skins) > 0:
		for i in xrange(0,md2.num_skins):
			#md2.skins[i].dump()
			if (Blender.sys.exists(md2.skins[i].name)):
				try:	return Blender.Image.Load(md2.skins[i].name)
				except:	return -1
	

def animate_md2(md2, mesh):
	######### Animate the verts through keyframe animation
	
	# Fast access to the meshes vertex coords
	verts = [v.co for v in mesh.verts] 
	scale = g_scale.val
	
	for i in xrange(1, md2.num_frames):
		frame = md2.frames[i]
		#update the vertices
		for j in xrange(md2.num_vertices):
			x=(frame.scale[0] * frame.vertices[j].vertices[0] + frame.translate[0]) * scale
			y=(frame.scale[1] * frame.vertices[j].vertices[1] + frame.translate[1]) * scale
			z=(frame.scale[2] * frame.vertices[j].vertices[2] + frame.translate[2]) * scale
			
			#put the vertex in the right spot
			verts[j][:] = y,-x,z
			
		mesh.insertKey(i,"absolute")
		# mesh.insertKey(i)
		
		#not really necissary, but I like playing with the frame counter
		Blender.Set("curframe", i)
	
	
	# Make the keys animate in the 3d view.
	key = mesh.key
	key.relative = False
	
	# Add an IPO to teh Key
	ipo = Blender.Ipo.New('Key', 'md2')
	key.ipo = ipo
	# Add a curve to the IPO
	curve = ipo.addCurve('Basis')
	
	# Add 2 points to cycle through the frames.
	curve.append((1, 0))
	curve.append((md2.num_frames, (md2.num_frames-1)/10.0))
	curve.interpolation = Blender.IpoCurve.InterpTypes.LINEAR
	


def load_md2(md2_filename, texture_filename):
	#read the file in
	file=open(md2_filename,"rb")
	WaitCursor(1)
	DrawProgressBar(0.0, 'Loading MD2')
	md2=md2_obj()
	md2.load(file)
	#md2.dump()
	file.close()

	######### Creates a new mesh
	mesh = Mesh.New()

	uv_coord=[]
	#uv_list=[]
	verts_extend = []
	#load the textures to use later
	#-1 if there is no texture to load
	mesh_image=load_textures(md2, texture_filename)
	if mesh_image == -1 and texture_filename:
		print 'MD2 Import, Warning, texture "%s" could not load'

	######### Make the verts
	DrawProgressBar(0.25,"Loading Vertex Data")
	frame = md2.frames[0]
	scale = g_scale.val
	
	def tmp_get_vertex(i):
		#use the first frame for the mesh vertices
		x=(frame.scale[0]*frame.vertices[i].vertices[0]+frame.translate[0])*scale
		y=(frame.scale[1]*frame.vertices[i].vertices[1]+frame.translate[1])*scale
		z=(frame.scale[2]*frame.vertices[i].vertices[2]+frame.translate[2])*scale
		return y,-x,z
	
	mesh.verts.extend( [tmp_get_vertex(i) for i in xrange(0,md2.num_vertices)] )
	del tmp_get_vertex
	
	######## Make the UV list
	DrawProgressBar(0.50,"Loading UV Data")
	
	w = float(md2.skin_width)
	h = float(md2.skin_height)
	if w <= 0.0: w = 1.0
	if h <= 0.0: h = 1.0
	#for some reason quake2 texture maps are upside down, flip that
	uv_list = [Vector(co.u/w, 1-(co.v/h)) for co in md2.tex_coords]
	del w, h
	
	######### Make the faces
	DrawProgressBar(0.75,"Loading Face Data")
	faces = []
	face_uvs = []
	for md2_face in md2.faces:
		f = md2_face.vertex_index[0], md2_face.vertex_index[2], md2_face.vertex_index[1]
		uv = uv_list[md2_face.texture_index[0]], uv_list[md2_face.texture_index[2]], uv_list[md2_face.texture_index[1]]
		
		if f[2] == 0:
			# EEKADOODLE :/
			f= f[1], f[2], f[0]
			uv= uv[1], uv[2], uv[0]
		
		#ditto in reverse order with the texture verts
		faces.append(f)
		face_uvs.append(uv)
	
	
	face_mapping = mesh.faces.extend(faces, indexList=True)
	print len(faces)
	print len(mesh.faces)
	mesh.faceUV= True  #turn on face UV coordinates for this mesh
	mesh_faces = mesh.faces
	for i, uv in enumerate(face_uvs):
		if face_mapping[i] != None:
			f = mesh_faces[face_mapping[i]]
			f.uv = uv
			if (mesh_image!=-1):
				f.image=mesh_image
	
	scn= Blender.Scene.GetCurrent()
	mesh_obj= scn.objects.new(mesh)
	animate_md2(md2, mesh)
	DrawProgressBar(0.98,"Loading Animation Data")
	
	#locate the Object containing the mesh at the cursor location
	cursor_pos=Blender.Window.GetCursorPos()
	mesh_obj.setLocation(float(cursor_pos[0]),float(cursor_pos[1]),float(cursor_pos[2]))
	DrawProgressBar (1.0, "") 
	WaitCursor(0)

#***********************************************
# MAIN
#***********************************************

# Import globals
g_md2_filename=Create("*.md2")
#g_md2_filename=Create("/d/warvet/tris.md2")
g_texture_filename=Create('')
# g_texture_filename=Create("/d/warvet/warvet.jpg")

g_filename_search=Create("*.md2")
g_texture_search=Create('')
# g_texture_search=Create("/d/warvet/warvet.jpg")

#Globals
g_scale=Create(1.0)

# Events
EVENT_NOEVENT=1
EVENT_LOAD_MD2=2
EVENT_CHOOSE_FILENAME=3
EVENT_CHOOSE_TEXTURE=4
EVENT_SAVE_MD2=5
EVENT_EXIT=100

######################################################
# Callbacks for Window functions
######################################################
def filename_callback(input_filename):
	global g_md2_filename
	g_md2_filename.val=input_filename

def texture_callback(input_texture):
	global g_texture_filename
	g_texture_filename.val=input_texture

######################################################
# GUI Loader
######################################################


def draw_gui():
	global g_scale
	global g_md2_filename
	global g_texture_filename
	global EVENT_NOEVENT,EVENT_LOAD_MD2,EVENT_CHOOSE_FILENAME,EVENT_CHOOSE_TEXTURE,EVENT_EXIT

	########## Titles
	glClear(GL_COLOR_BUFFER_BIT)
	glRasterPos2d(8, 125)
	Text("MD2 loader")

	######### Parameters GUI Buttons
	BeginAlign()
	g_md2_filename = String("MD2 file to load: ", EVENT_NOEVENT, 10, 55, 210, 18,
							g_md2_filename.val, 255, "MD2 file to load")
	########## MD2 File Search Button
	Button("Browse",EVENT_CHOOSE_FILENAME,220,55,80,18)
	EndAlign()

	BeginAlign()
	g_texture_filename = String("Texture file to load: ", EVENT_NOEVENT, 10, 35, 210, 18,
								g_texture_filename.val, 255, "Texture file to load-overrides MD2 file")
	########## Texture Search Button
	Button("Browse",EVENT_CHOOSE_TEXTURE,220,35,80,18)
	EndAlign()

	########## Scale slider-default is 1/8 which is a good scale for md2->blender
	g_scale= Slider("Scale Factor: ", EVENT_NOEVENT, 10, 75, 210, 18,
					1.0, 0.001, 10.0, 1, "Scale factor for obj Model");

	######### Draw and Exit Buttons
	Button("Load",EVENT_LOAD_MD2 , 10, 10, 80, 18)
	Button("Exit",EVENT_EXIT , 170, 10, 80, 18)

def event(evt, val):	
	if (evt == QKEY and not val):
		Blender.Draw.Exit()

def bevent(evt):
	global g_md2_filename
	global g_texture_filename
	global EVENT_NOEVENT,EVENT_LOAD_MD2,EVENT_SAVE_MD2,EVENT_EXIT

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Blender.Draw.Exit()
	elif (evt==EVENT_CHOOSE_FILENAME):
		FileSelector(filename_callback, "MD2 File Selection")
	elif (evt==EVENT_CHOOSE_TEXTURE):
		FileSelector(texture_callback, "Texture Selection")
	elif (evt==EVENT_LOAD_MD2):
		if not Blender.sys.exists(g_md2_filename.val):
			PupMenu('Model file does not exist')
			return
		else:
			load_md2(g_md2_filename.val, g_texture_filename.val)
			Blender.Redraw()
			Blender.Draw.Exit()
			return

if __name__ == '__main__':
	Register(draw_gui, event, bevent)
