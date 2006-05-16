#!BPY

"""
Name: 'MD2 (.md2)'
Blender: 241
Group: 'Export'
Tooltip: 'Export to Quake file format (.md2).'
"""

__author__ = 'Bob Holcomb'
__version__ = '0.17'
__url__ = ["Bob's site, http://bane.servebeer.com",
     "Support forum, http://scourage.servebeer.com/phpbb/", "blender", "elysiun"]
__email__ = ["Bob Holcomb, bob_holcomb:hotmail*com", "scripts"]
__bpydoc__ = """\
This script Exports a Quake 2 file (MD2).

 Additional help from: Shadwolf, Skandal, Rojo, Cambo<br>
 Thanks Guys!
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C): Bob Holcomb
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
from Blender import *
from Blender.Draw import *
from Blender.BGL import *
from Blender.Window import *

import struct, string
from types import *



######################################################
# GUI Loader
######################################################

# Export globals
g_filename=Create("/home/bob/work/blender_scripts/md2/test-export.md2")
g_frame_filename=Create("default")

g_filename_search=Create("model")
g_frame_search=Create("default")

user_frame_list=[]

#Globals
g_scale=Create(1.0)

# Events
EVENT_NOEVENT=1
EVENT_SAVE_MD2=2
EVENT_CHOOSE_FILENAME=3
EVENT_CHOOSE_FRAME=4
EVENT_EXIT=100

######################################################
# Callbacks for Window functions
######################################################
def filename_callback(input_filename):
	global g_filename
	g_filename.val=input_filename

def frame_callback(input_frame):
	global g_frame_filename
	g_frame_filename.val=input_frame

def draw_gui():
	global g_scale
	global g_filename
	global g_frame_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2,EVENT_CHOOSE_FILENAME,EVENT_CHOOSE_FRAME,EVENT_EXIT

	########## Titles
	glClear(GL_COLOR_BUFFER_BIT)
	glRasterPos2d(8, 103)
	Text("MD2 Export")

	######### Parameters GUI Buttons
	g_filename = String("MD2 file to save: ", EVENT_NOEVENT, 10, 55, 210, 18,
                            g_filename.val, 255, "MD2 file to save")
	########## MD2 File Search Button
	Button("Search",EVENT_CHOOSE_FILENAME,220,55,80,18)

	g_frame_filename = String("Frame List file to load: ", EVENT_NOEVENT, 10, 35, 210, 18,
                                g_frame_filename.val, 255, "Frame List to load-overrides MD2 defaults")
	########## Texture Search Button
	Button("Search",EVENT_CHOOSE_FRAME,220,35,80,18)

	########## Scale slider-default is 1/8 which is a good scale for md2->blender
	g_scale= Slider("Scale Factor: ", EVENT_NOEVENT, 10, 75, 210, 18,
                    1.0, 0.001, 10.0, 1, "Scale factor for obj Model");

	######### Draw and Exit Buttons
	Button("Export",EVENT_SAVE_MD2 , 10, 10, 80, 18)
	Button("Exit",EVENT_EXIT , 170, 10, 80, 18)

def event(evt, val):	
	if (evt == QKEY and not val):
		Exit()

def bevent(evt):
	global g_filename
	global g_frame_filename
	global EVENT_NOEVENT,EVENT_SAVE_MD2,EVENT_EXIT

	######### Manages GUI events
	if (evt==EVENT_EXIT):
		Blender.Draw.Exit()
	elif (evt==EVENT_CHOOSE_FILENAME):
		FileSelector(filename_callback, "MD2 File Selection")
	elif (evt==EVENT_CHOOSE_FRAME):
		FileSelector(frame_callback, "Frame Selection")
	elif (evt==EVENT_SAVE_MD2):
		if (g_filename.val == "model"):
			save_md2("blender.md2")
			Blender.Draw.Exit()
			return
		else:
			save_md2(g_filename.val)
			Blender.Draw.Exit()
			return

Register(draw_gui, event, bevent)

######################################################
# MD2 Model Constants
######################################################
MD2_MAX_TRIANGLES=4096
MD2_MAX_VERTICES=2048
MD2_MAX_TEXCOORDS=2048
MD2_MAX_FRAMES=512
MD2_MAX_SKINS=32
MD2_MAX_FRAMESIZE=(MD2_MAX_VERTICES * 4 + 128)

MD2_FRAME_NAME_LIST=(("stand",1,40),
					("run",41,46),
					("attack",47,54),
					("pain1",55,58),
					("pain2",59,62),
					("pain3",63,66),
					("jump",67,72),
					("flip",73,84),
					("salute", 85,95),
					("taunt",96,112),
					("wave",113,123),
					("point",124,135),
					("crstnd",136,154),
					("crwalk",155,160),
					("crattack",161,169),
					("crpain",170,173),
					("crdeath",174,178),
					("death1",179,184),
					("death2",185,190),
					("death3",191,198))
					#198 frames
					
MD2_NORMALS=((-0.525731, 0.000000, 0.850651), 
			(-0.442863, 0.238856, 0.864188), 
			(-0.295242, 0.000000, 0.955423), 
			(-0.309017, 0.500000, 0.809017), 
			(-0.162460, 0.262866, 0.951056), 
			(0.000000, 0.000000, 1.000000), 
			(0.000000, 0.850651, 0.525731), 
			(-0.147621, 0.716567, 0.681718), 
			(0.147621, 0.716567, 0.681718), 
			(0.000000, 0.525731, 0.850651), 
			(0.309017, 0.500000, 0.809017), 
			(0.525731, 0.000000, 0.850651), 
			(0.295242, 0.000000, 0.955423), 
			(0.442863, 0.238856, 0.864188), 
			(0.162460, 0.262866, 0.951056), 
			(-0.681718, 0.147621, 0.716567), 
			(-0.809017, 0.309017, 0.500000), 
			(-0.587785, 0.425325, 0.688191), 
			(-0.850651, 0.525731, 0.000000), 
			(-0.864188, 0.442863, 0.238856), 
			(-0.716567, 0.681718, 0.147621), 
			(-0.688191, 0.587785, 0.425325), 
			(-0.500000, 0.809017, 0.309017), 
			(-0.238856, 0.864188, 0.442863), 
			(-0.425325, 0.688191, 0.587785), 
			(-0.716567, 0.681718, -0.147621), 
			(-0.500000, 0.809017, -0.309017), 
			(-0.525731, 0.850651, 0.000000), 
			(0.000000, 0.850651, -0.525731), 
			(-0.238856, 0.864188, -0.442863), 
			(0.000000, 0.955423, -0.295242), 
			(-0.262866, 0.951056, -0.162460), 
			(0.000000, 1.000000, 0.000000), 
			(0.000000, 0.955423, 0.295242), 
			(-0.262866, 0.951056, 0.162460), 
			(0.238856, 0.864188, 0.442863), 
			(0.262866, 0.951056, 0.162460), 
			(0.500000, 0.809017, 0.309017), 
			(0.238856, 0.864188, -0.442863), 
			(0.262866, 0.951056, -0.162460), 
			(0.500000, 0.809017, -0.309017), 
			(0.850651, 0.525731, 0.000000), 
			(0.716567, 0.681718, 0.147621), 
			(0.716567, 0.681718, -0.147621), 
			(0.525731, 0.850651, 0.000000), 
			(0.425325, 0.688191, 0.587785), 
			(0.864188, 0.442863, 0.238856), 
			(0.688191, 0.587785, 0.425325), 
			(0.809017, 0.309017, 0.500000), 
			(0.681718, 0.147621, 0.716567), 
			(0.587785, 0.425325, 0.688191), 
			(0.955423, 0.295242, 0.000000), 
			(1.000000, 0.000000, 0.000000), 
			(0.951056, 0.162460, 0.262866), 
			(0.850651, -0.525731, 0.000000), 
			(0.955423, -0.295242, 0.000000), 
			(0.864188, -0.442863, 0.238856), 
			(0.951056, -0.162460, 0.262866), 
			(0.809017, -0.309017, 0.500000), 
			(0.681718, -0.147621, 0.716567), 
			(0.850651, 0.000000, 0.525731), 
			(0.864188, 0.442863, -0.238856), 
			(0.809017, 0.309017, -0.500000), 
			(0.951056, 0.162460, -0.262866), 
			(0.525731, 0.000000, -0.850651), 
			(0.681718, 0.147621, -0.716567), 
			(0.681718, -0.147621, -0.716567), 
			(0.850651, 0.000000, -0.525731), 
			(0.809017, -0.309017, -0.500000), 
			(0.864188, -0.442863, -0.238856), 
			(0.951056, -0.162460, -0.262866), 
			(0.147621, 0.716567, -0.681718), 
			(0.309017, 0.500000, -0.809017), 
			(0.425325, 0.688191, -0.587785), 
			(0.442863, 0.238856, -0.864188), 
			(0.587785, 0.425325, -0.688191), 
			(0.688191, 0.587785, -0.425325), 
			(-0.147621, 0.716567, -0.681718), 
			(-0.309017, 0.500000, -0.809017), 
			(0.000000, 0.525731, -0.850651), 
			(-0.525731, 0.000000, -0.850651), 
			(-0.442863, 0.238856, -0.864188), 
			(-0.295242, 0.000000, -0.955423), 
			(-0.162460, 0.262866, -0.951056), 
			(0.000000, 0.000000, -1.000000), 
			(0.295242, 0.000000, -0.955423), 
			(0.162460, 0.262866, -0.951056), 
			(-0.442863, -0.238856, -0.864188), 
			(-0.309017, -0.500000, -0.809017), 
			(-0.162460, -0.262866, -0.951056), 
			(0.000000, -0.850651, -0.525731), 
			(-0.147621, -0.716567, -0.681718), 
			(0.147621, -0.716567, -0.681718), 
			(0.000000, -0.525731, -0.850651), 
			(0.309017, -0.500000, -0.809017), 
			(0.442863, -0.238856, -0.864188), 
			(0.162460, -0.262866, -0.951056), 
			(0.238856, -0.864188, -0.442863), 
			(0.500000, -0.809017, -0.309017), 
			(0.425325, -0.688191, -0.587785), 
			(0.716567, -0.681718, -0.147621), 
			(0.688191, -0.587785, -0.425325), 
			(0.587785, -0.425325, -0.688191), 
			(0.000000, -0.955423, -0.295242), 
			(0.000000, -1.000000, 0.000000), 
			(0.262866, -0.951056, -0.162460), 
			(0.000000, -0.850651, 0.525731), 
			(0.000000, -0.955423, 0.295242), 
			(0.238856, -0.864188, 0.442863), 
			(0.262866, -0.951056, 0.162460), 
			(0.500000, -0.809017, 0.309017), 
			(0.716567, -0.681718, 0.147621), 
			(0.525731, -0.850651, 0.000000), 
			(-0.238856, -0.864188, -0.442863), 
			(-0.500000, -0.809017, -0.309017), 
			(-0.262866, -0.951056, -0.162460), 
			(-0.850651, -0.525731, 0.000000), 
			(-0.716567, -0.681718, -0.147621), 
			(-0.716567, -0.681718, 0.147621), 
			(-0.525731, -0.850651, 0.000000), 
			(-0.500000, -0.809017, 0.309017), 
			(-0.238856, -0.864188, 0.442863), 
			(-0.262866, -0.951056, 0.162460), 
			(-0.864188, -0.442863, 0.238856), 
			(-0.809017, -0.309017, 0.500000), 
			(-0.688191, -0.587785, 0.425325), 
			(-0.681718, -0.147621, 0.716567), 
			(-0.442863, -0.238856, 0.864188), 
			(-0.587785, -0.425325, 0.688191), 
			(-0.309017, -0.500000, 0.809017), 
			(-0.147621, -0.716567, 0.681718), 
			(-0.425325, -0.688191, 0.587785), 
			(-0.162460, -0.262866, 0.951056), 
			(0.442863, -0.238856, 0.864188), 
			(0.162460, -0.262866, 0.951056), 
			(0.309017, -0.500000, 0.809017), 
			(0.147621, -0.716567, 0.681718), 
			(0.000000, -0.525731, 0.850651), 
			(0.425325, -0.688191, 0.587785), 
			(0.587785, -0.425325, 0.688191), 
			(0.688191, -0.587785, 0.425325), 
			(-0.955423, 0.295242, 0.000000), 
			(-0.951056, 0.162460, 0.262866), 
			(-1.000000, 0.000000, 0.000000), 
			(-0.850651, 0.000000, 0.525731), 
			(-0.955423, -0.295242, 0.000000), 
			(-0.951056, -0.162460, 0.262866), 
			(-0.864188, 0.442863, -0.238856), 
			(-0.951056, 0.162460, -0.262866), 
			(-0.809017, 0.309017, -0.500000), 
			(-0.864188, -0.442863, -0.238856), 
			(-0.951056, -0.162460, -0.262866), 
			(-0.809017, -0.309017, -0.500000), 
			(-0.681718, 0.147621, -0.716567), 
			(-0.681718, -0.147621, -0.716567), 
			(-0.850651, 0.000000, -0.525731), 
			(-0.688191, 0.587785, -0.425325), 
			(-0.587785, 0.425325, -0.688191), 
			(-0.425325, 0.688191, -0.587785), 
			(-0.425325, -0.688191, -0.587785), 
			(-0.587785, -0.425325, -0.688191), 
			(-0.688191, -0.587785, -0.425325))


######################################################
# MD2 data structures
######################################################
class md2_point:
	vertices=[]
	lightnormalindex=0
	binary_format="<3BB"
	def __init__(self):
		self.vertices=[0]*3
		self.lightnormalindex=0
	def save(self, file):
		temp_data=[0]*4
		temp_data[0]=self.vertices[0]
		temp_data[1]=self.vertices[1]
		temp_data[2]=self.vertices[2]
		temp_data[3]=self.lightnormalindex
		data=struct.pack(self.binary_format, temp_data[0], temp_data[1], temp_data[2], temp_data[3])
		file.write(data)
	def dump(self):
		print "MD2 Point Structure"
		print "vertex X: ", self.vertices[0]
		print "vertex Y: ", self.vertices[1]
		print "vertex Z: ", self.vertices[2]
		print "lightnormalindex: ",self.lightnormalindex
		print ""
		
class md2_face:
	vertex_index=[]
	texture_index=[]
	binary_format="<3h3h"
	def __init__(self):
		self.vertex_index = [ 0, 0, 0 ]
		self.texture_index = [ 0, 0, 0]
	def save(self, file):
		temp_data=[0]*6
		#swap vertices around so they draw right
		temp_data[0]=self.vertex_index[0]
		temp_data[1]=self.vertex_index[2]
		temp_data[2]=self.vertex_index[1]
		#swap texture vertices around so they draw right
		temp_data[3]=self.texture_index[0]
		temp_data[4]=self.texture_index[2]
		temp_data[5]=self.texture_index[1]
		data=struct.pack(self.binary_format,temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5])
		file.write(data)
	def dump (self):
		print "MD2 Face Structure"
		print "vertex 1 index: ", self.vertex_index[0]
		print "vertex 2 index: ", self.vertex_index[1]
		print "vertex 3 index: ", self.vertex_index[2]
		print "texture 1 index: ", self.texture_index[0]
		print "texture 2 index: ", self.texture_index[1]
		print "texture 3 index: ", self.texture_index[2]
		print ""
		
class md2_tex_coord:
	u=0
	v=0
	binary_format="<2h"
	def __init__(self):
		self.u=0
		self.v=0
	def save(self, file):
		temp_data=[0]*2
		temp_data[0]=self.u
		temp_data[1]=self.v
		data=struct.pack(self.binary_format, temp_data[0], temp_data[1])
		file.write(data)
	def dump (self):
		print "MD2 Texture Coordinate Structure"
		print "texture coordinate u: ",self.u
		print "texture coordinate v: ",self.v
		print ""
		
class md2_GL_command:
	s=0.0
	t=0.0
	vert_index=0
	binary_format="<2fi"
	
	def __init__(self):
		self.s=0.0
		self.t=0.0
		vert_index=0
	def save(self,file):
		temp_data=[0]*3
		temp_data[0]=float(self.s)
		temp_data[1]=float(self.t)
		temp_data[2]=self.vert_index
		data=struct.pack(self.binary_format, temp_data[0],temp_data[1],temp_data[2])
		file.write(data)
	def dump (self):
		print "MD2 OpenGL Command"
		print "s: ", self.s
		print "t: ", self.t
		print "Vertex Index: ", self.vert_index
		print ""

class md2_GL_cmd_list:
	num=0
	cmd_list=[]
	binary_format="<i"
	
	def __init__(self):
		self.num=0
		self.cmd_list=[]
	
	def save(self,file):
		data=struct.pack(self.binary_format, self.num)
		file.write(data)
		for cmd in self.cmd_list:
			cmd.save(file)
	def dump(self):
		print "MD2 OpenGL Command List"
		print "number: ", self.num
		for cmd in self.cmd_list:
			cmd.dump()
		print ""

class md2_skin:
	name=""
	binary_format="<64s"
	def __init__(self):
		self.name=""
	def save(self, file):
		temp_data=self.name
		data=struct.pack(self.binary_format, temp_data)
		file.write(data)
	def dump (self):
		print "MD2 Skin"
		print "skin name: ",self.name
		print ""
		
class md2_frame:
	scale=[]
	translate=[]
	name=[]
	vertices=[]
	binary_format="<3f3f16s"

	def __init__(self):
		self.scale=[0.0]*3
		self.translate=[0.0]*3
		self.name=""
		self.vertices=[]
	def save(self, file):
		temp_data=[0]*7
		temp_data[0]=float(self.scale[0])
		temp_data[1]=float(self.scale[1])
		temp_data[2]=float(self.scale[2])
		temp_data[3]=float(self.translate[0])
		temp_data[4]=float(self.translate[1])
		temp_data[5]=float(self.translate[2])
		temp_data[6]=self.name
		data=struct.pack(self.binary_format, temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5],temp_data[6])
		file.write(data)
	def dump (self):
		print "MD2 Frame"
		print "scale x: ",self.scale[0]
		print "scale y: ",self.scale[1]
		print "scale z: ",self.scale[2]
		print "translate x: ",self.translate[0]
		print "translate y: ",self.translate[1]
		print "translate z: ",self.translate[2]
		print "name: ",self.name
		print ""
		
class md2_obj:
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
	binary_format="<17i"  #little-endian (<), 17 integers (17i)
	#md2 data objects
	tex_coords=[]
	faces=[]
	frames=[]
	skins=[]
	GL_commands=[]
	
	def __init__ (self):
		self.tex_coords=[]
		self.faces=[]
		self.frames=[]
		self.skins=[]
	def save(self, file):
		temp_data=[0]*17
		temp_data[0]=self.ident
		temp_data[1]=self.version
		temp_data[2]=self.skin_width
		temp_data[3]=self.skin_height
		temp_data[4]=self.frame_size
		temp_data[5]=self.num_skins
		temp_data[6]=self.num_vertices
		temp_data[7]=self.num_tex_coords
		temp_data[8]=self.num_faces
		temp_data[9]=self.num_GL_commands
		temp_data[10]=self.num_frames
		temp_data[11]=self.offset_skins
		temp_data[12]=self.offset_tex_coords
		temp_data[13]=self.offset_faces
		temp_data[14]=self.offset_frames
		temp_data[15]=self.offset_GL_commands
		temp_data[16]=self.offset_end
		data=struct.pack(self.binary_format, temp_data[0],temp_data[1],temp_data[2],temp_data[3],temp_data[4],temp_data[5],temp_data[6],temp_data[7],temp_data[8],temp_data[9],temp_data[10],temp_data[11],temp_data[12],temp_data[13],temp_data[14],temp_data[15],temp_data[16])
		file.write(data)
		#write the skin data
		for skin in self.skins:
			skin.save(file)
		#save the texture coordinates
		for tex_coord in self.tex_coords:
			tex_coord.save(file)
		#save the face info
		for face in self.faces:
			face.save(file)
		#save the frames
		for frame in self.frames:
			frame.save(file)
			for vert in frame.vertices:
				vert.save(file)
		#save the GL command List
		for cmd in self.GL_commands:
			cmd.save(file)
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
		print "number of GL commands: ",self.num_GL_commands
		print "offset skins: ", self.offset_skins
		print "offset texture coordinates: ", self.offset_tex_coords
		print "offset faces: ", self.offset_faces
		print "offset frames: ",self.offset_frames
		print "offset GL Commands: ",self.offset_GL_commands
		print "offset end: ",self.offset_end
		print ""

######################################################
# Validation
######################################################
def validation(object):
	global user_frame_list
	
	#get access to the mesh data
	mesh=object.getData(False, True) #get the object (not just name) and the Mesh, not NMesh

	#check it's composed of only tri's	
	result=0
	for face in mesh.faces:
		if len(face.verts)!=3:
			#select the face for future triangulation
			face.sel=1
			if result==0:  #first time we have this problem, don't pop-up a window every time it finds a quad
			  print "Model not made entirely of triangles"
			  result=Blender.Draw.PupMenu("Model not made entirely out of Triangles-Convert?%t|YES|NO")
	
	print "result: ", result
	#triangulate or quit
	if result==1:
		#selecting face mode
		Blender.Mesh.Mode(3)
		mesh.quadToTriangle(0) #use closest verticies in breaking a quad
	elif result==2:
		return False #user will fix (I guess)

	#check it has UV coordinates
	if mesh.vertexUV==True:
		print "Vertex UV not supported"
		result=Blender.Draw.PupMenu("Vertex UV not suppored-Use Sticky UV%t|OK")
		return False
			
	elif mesh.faceUV==True:
		for face in mesh.faces:
			if(len(face.uv)==3):
				pass
			else:
				print "Models vertices do not all have UV"
				result=Blender.Draw.PupMenu("Models vertices do not all have UV%t|OK")
				return False
	
	else:
		print "Model does not have UV (face or vertex)"
		result=Blender.Draw.PupMenu("Model does not have UV (face or vertex)%t|OK")
		return False

	#check it has only 1 associated texture map
	last_face=""
	last_face=mesh.faces[0].image
	if last_face=="":
		print "Model does not have a texture Map"
		result=Blender.Draw.PupMenu("Model does not have a texture Map%t|OK")
		return False

	for face in mesh.faces:
		mesh_image=face.image
		if not mesh_image:
			print "Model has a face without a texture Map"
			result=Blender.Draw.PupMenu("Model has a face without a texture Map%t|OK")
			return False
		if mesh_image!=last_face:
			print "Model has more than 1 texture map assigned"
			result=Blender.Draw.PupMenu("Model has more than 1 texture map assigned%t|OK")
			return False
		
	size=mesh_image.getSize()
	#is this really what the user wants
	if (size[0]!=256 or size[1]!=256):
		print "Texture map size is non-standard (not 256x256), it is: ",size[0],"x",size[1]
		result=Blender.Draw.PupMenu("Texture map size is non-standard (not 256x256), it is: "+str(size[0])+"x"+str(size[1])+": Continue?%t|YES|NO")
		if(result==2):
			return False

	#verify frame list data
	user_frame_list=get_frame_list()	
	temp=user_frame_list[len(user_frame_list)-1]
	temp_num_frames=temp[2]
	
	#verify tri/vert/frame counts are within MD2 standard
	face_count=len(mesh.faces)
	vert_count=len(mesh.verts)	
	frame_count=temp_num_frames
	
	if face_count>MD2_MAX_TRIANGLES:
		print "Number of triangles exceeds MD2 standard: ", face_count,">",MD2_MAX_TRIANGLES
		result=Blender.Draw.PupMenu("Number of triangles exceeds MD2 standard: Continue?%t|YES|NO")
		if(result==2):
			return False
	if vert_count>MD2_MAX_VERTICES:
		print "Number of verticies exceeds MD2 standard",vert_count,">",MD2_MAX_VERTICES
		result=Blender.Draw.PupMenu("Number of verticies exceeds MD2 standard: Continue?%t|YES|NO")
		if(result==2):
			return False
	if frame_count>MD2_MAX_FRAMES:
		print "Number of frames exceeds MD2 standard of",frame_count,">",MD2_MAX_FRAMES
		result=Blender.Draw.PupMenu("Number of frames exceeds MD2 standard: Continue?%t|YES|NO")
		if(result==2):
			return False
	#model is OK
	return True

######################################################
# Fill MD2 data structure
######################################################
def fill_md2(md2, object):
	global user_frame_list
	#get a Mesh, not NMesh
	mesh=object.getData(False, True)	
	
	#load up some intermediate data structures
	tex_list={}
	tex_count=0
	#create the vertex list from the first frame
	Blender.Set("curframe", 1)
	
	#header information
	md2.ident=844121161
	md2.version=8	
	md2.num_vertices=len(mesh.verts)
	md2.num_faces=len(mesh.faces)

	#get the skin information
	#use the first faces' image for the texture information
	mesh_image=mesh.faces[0].image
	size=mesh_image.getSize()
	md2.skin_width=size[0]
	md2.skin_height=size[1]
	md2.num_skins=1
	#add a skin node to the md2 data structure
	md2.skins.append(md2_skin())
	md2.skins[0].name=Blender.sys.basename(mesh_image.getFilename())

	#put texture information in the md2 structure
	#build UV coord dictionary (prevents double entries-saves space)
	for face in mesh.faces:
		for i in range(0,3):
			t=(face.uv[i])
			tex_key=(t[0],t[1])
			if not tex_list.has_key(tex_key):
				tex_list[tex_key]=tex_count
				tex_count+=1
	md2.num_tex_coords=tex_count #each vert has its own UV coord

	for this_tex in range (0, md2.num_tex_coords):
		md2.tex_coords.append(md2_tex_coord())
	for coord, index in tex_list.iteritems():
		#md2.tex_coords.append(md2_tex_coord())
		md2.tex_coords[index].u=int(coord[0]*md2.skin_width)
		md2.tex_coords[index].v=int((1-coord[1])*md2.skin_height)

	#put faces in the md2 structure
	#for each face in the model
	for this_face in range(0, md2.num_faces):
		md2.faces.append(md2_face())
		for i in range(0,3):
			#blender uses indexed vertexes so this works very well
			md2.faces[this_face].vertex_index[i]=mesh.faces[this_face].verts[i].index
			#lookup texture index in dictionary
			uv_coord=(mesh.faces[this_face].uv[i])
			tex_key=(uv_coord[0],uv_coord[1])
			tex_index=tex_list[tex_key]
			md2.faces[this_face].texture_index[i]=tex_index

	#compute GL commands
	md2.num_GL_commands=build_GL_commands(md2)

	#get the frame data
	#calculate 1 frame size  + (1 vert size*num_verts)
	md2.frame_size=40+(md2.num_vertices*4) #in bytes
	
	#get the frame list
	user_frame_list=get_frame_list()
	if user_frame_list=="default":
		md2.num_frames=198
	else:
		temp=user_frame_list[len(user_frame_list)-1]  #last item
		md2.num_frames=temp[2] #last frame number
	
	#fill in each frame with frame info and all the vertex data for that frame
	for frame_counter in range(0,md2.num_frames):
		#add a frame
		md2.frames.append(md2_frame())
		#update the mesh objects vertex positions for the animation
		Blender.Set("curframe", frame_counter)  #set blender to the correct frame
		mesh.getFromObject(object.name)  #update the mesh to make verts current
		
#each frame has a scale and transform value that gets the vertex value between 0-255
#since the scale and transform are the same for the all the verts in the frame, we only need
#to figure this out once per frame
		
		#we need to start with the bounding box
		bounding_box=object.getBoundBox() #uses the object, not the mesh data
		#initialize with the first vertex for both min and max.  X and Y are swapped for MD2 format
		point=bounding_box[0]
		frame_min_x=point[1]
		frame_max_x=point[1]
		frame_min_y=point[0]
		frame_max_y=point[0]
		frame_min_z=point[2]
		frame_max_z=point[2]
		#find min/max values
		for point in bounding_box:
			if frame_min_x>point[1]: frame_min_x=point[1]
			if frame_max_x<point[1]: frame_max_x=point[1]
			if frame_min_y>point[0]: frame_min_y=point[0]
			if frame_max_y<point[0]: frame_max_y=point[0]
			if frame_min_z>point[2]: frame_min_z=point[2]
			if frame_max_z<point[2]: frame_max_z=point[2]

		#the scale is the difference between the min and max (on that axis) / 255
		frame_scale_x=(frame_max_x-frame_min_x)/255
		frame_scale_y=(frame_max_y-frame_min_y)/255
		frame_scale_z=(frame_max_z-frame_min_z)/255
		
		#translate value of the mesh to center it on the origin
		frame_trans_x=frame_min_x
		frame_trans_y=frame_min_y
		frame_trans_z=frame_min_z
		
		#fill in the data
		md2.frames[frame_counter].scale=(-frame_scale_x, frame_scale_y, frame_scale_z)
		md2.frames[frame_counter].translate=(-frame_trans_x, frame_trans_y, frame_trans_z)
		
		#now for the vertices
		for vert_counter in range(0, md2.num_vertices):
			#add a vertex to the md2 structure
			md2.frames[frame_counter].vertices.append(md2_point())
			#figure out the new coords based on scale and transform
			#then translates the point so it's not less than 0
			#then scale it so it's between 0..255
			new_x=int((mesh.verts[vert_counter].co[1]-frame_trans_x)/frame_scale_x)
			new_y=int((mesh.verts[vert_counter].co[0]-frame_trans_y)/frame_scale_y)
			new_z=int((mesh.verts[vert_counter].co[2]-frame_trans_z)/frame_scale_z)
			#put them in the structure
			md2.frames[frame_counter].vertices[vert_counter].vertices=(new_x, new_y, new_z)

			#need to add the lookup table check here
			maxdot = -999999.0;
			maxdotindex = -1;

			for j in range(0,162):
				#dot = (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
				x1=mesh.verts[vert_counter].no[1]
				y1=mesh.verts[vert_counter].no[0]
				z1=mesh.verts[vert_counter].no[2]
				dot = (x1*MD2_NORMALS[j][0]+
				       y1*MD2_NORMALS[j][1]+
							 z1*MD2_NORMALS[j][2]);
				if (dot > maxdot):
					maxdot = dot;
					maxdotindex = j;
			
			md2.frames[frame_counter].vertices[vert_counter].lightnormalindex=maxdotindex	
			
			
			
			
	#output all the frame names-user_frame_list is loaded during the validation
	for frame_set in user_frame_list:
		for counter in range(frame_set[1]-1, frame_set[2]):
			md2.frames[counter].name=frame_set[0]+"_"+str(counter-frame_set[1]+2)

	#compute these after everthing is loaded into a md2 structure
	header_size=17*4 #17 integers, and each integer is 4 bytes
	skin_size=64*md2.num_skins #64 char per skin * number of skins
	tex_coord_size=4*md2.num_tex_coords #2 short * number of texture coords
	face_size=12*md2.num_faces #3 shorts for vertex index, 3 shorts for tex index
	frames_size=(((12+12+16)+(4*md2.num_vertices)) * md2.num_frames) #frame info+verts per frame*num frames
	GL_command_size=md2.num_GL_commands*4 #each is an int or float, so 4 bytes per
	
	#fill in the info about offsets
	md2.offset_skins=0+header_size
	md2.offset_tex_coords=md2.offset_skins+skin_size
	md2.offset_faces=md2.offset_tex_coords+tex_coord_size
	md2.offset_frames=md2.offset_faces+face_size
	md2.offset_GL_commands=md2.offset_frames+frames_size
	md2.offset_end=md2.offset_GL_commands+GL_command_size

######################################################
# Get Frame List
######################################################
def get_frame_list():
	global g_frame_filename
	frame_list=[]

	if g_frame_filename.val=="default":
		return MD2_FRAME_NAME_LIST

	else:
	#check for file
		if (Blender.sys.exists(g_frame_filename.val)==1):
			#open file and read it in
			file=open(g_frame_filename.val,"r")
			lines=file.readlines()
			file.close()

			#check header (first line)
			if lines[0]<>"# MD2 Frame Name List\n":
				print "its not a valid file"
				result=Blender.Draw.PupMenu("This is not a valid frame definition file-using default%t|OK")
				return MD2_FRAME_NAME_LIST
			else:
				#read in the data
				num_frames=0
				for counter in range(1, len(lines)):
					current_line=lines[counter]
					if current_line[0]=="#":
						#found a comment
						pass
					else:
						data=current_line.split()
						frame_list.append([data[0],num_frames+1, num_frames+int(data[1])])
						num_frames+=int(data[1])
				return frame_list
		else:
			print "Cannot find file"
			result=Blender.Draw.PupMenu("Cannot find frame definion file-using default%t|OK")
			return MD2_FRAME_NAME_LIST

######################################################
# Tri-Strip/Tri-Fan functions
######################################################
def find_strip_length(md2, start_tri, start_vert):
	#variables shared between fan and strip functions
	global used
	global strip_vert
	global strip_st
	global strip_tris
	global strip_count

	m1=m2=0
	st1=st2=0
	
	used[start_tri]=2

	last=start_tri

	strip_vert[0]=md2.faces[last].vertex_index[start_vert%3]
	strip_vert[1]=md2.faces[last].vertex_index[(start_vert+1)%3]
	strip_vert[2]=md2.faces[last].vertex_index[(start_vert+2)%3]

	strip_st[0]=md2.faces[last].texture_index[start_vert%3]
	strip_st[1]=md2.faces[last].texture_index[(start_vert+1)%3]
	strip_st[2]=md2.faces[last].texture_index[(start_vert+2)%3]

	strip_tris[0]=start_tri
	strip_count=1

	m1=md2.faces[last].vertex_index[(start_vert+2)%3]
	st1=md2.faces[last].texture_index[(start_vert+2)%3]
	m2=md2.faces[last].vertex_index[(start_vert+1)%3]
	st2=md2.faces[last].texture_index[(start_vert+1)%3]
	
	#look for matching triangle
	check=start_tri+1
	
	for tri_counter in range(start_tri+1, md2.num_faces):
		
		for k in range(0,3):
			if md2.faces[check].vertex_index[k]!=m1:
				continue
			if md2.faces[check].texture_index[k]!=st1:
				continue
			if md2.faces[check].vertex_index[(k+1)%3]!=m2:
				continue
			if md2.faces[check].texture_index[(k+1)%3]!=st2:
				continue
			
			#if we can't use this triangle, this tri_strip is done
			if (used[tri_counter]!=0):
				for clear_counter in range(start_tri+1, md2.num_faces):
					if used[clear_counter]==2:
						used[clear_counter]=0
				return strip_count

			#new edge
			if (strip_count & 1):
				m2=md2.faces[check].vertex_index[(k+2)%3]
				st2=md2.faces[check].texture_index[(k+2)%3]
			else:
				m1=md2.faces[check].vertex_index[(k+2)%3]
				st1=md2.faces[check].texture_index[(k+2)%3]
			
			strip_vert[strip_count+2]=md2.faces[tri_counter].vertex_index[(k+2)%3]
			strip_st[strip_count+2]=md2.faces[tri_counter].texture_index[(k+2)%3]
			strip_tris[strip_count]=tri_counter
			strip_count+=1
	
			used[tri_counter]=2
		check+=1
	return strip_count

def find_fan_length(md2, start_tri, start_vert):
	#variables shared between fan and strip functions
	global used
	global strip_vert
	global strip_st
	global strip_tris
	global strip_count

	m1=m2=0
	st1=st2=0
	
	used[start_tri]=2

	last=start_tri

	strip_vert[0]=md2.faces[last].vertex_index[start_vert%3]
	strip_vert[1]=md2.faces[last].vertex_index[(start_vert+1)%3]
	strip_vert[2]=md2.faces[last].vertex_index[(start_vert+2)%3]
	
	strip_st[0]=md2.faces[last].texture_index[start_vert%3]
	strip_st[1]=md2.faces[last].texture_index[(start_vert+1)%3]
	strip_st[2]=md2.faces[last].texture_index[(start_vert+2)%3]

	strip_tris[0]=start_tri
	strip_count=1

	m1=md2.faces[last].vertex_index[(start_vert+0)%3]
	st1=md2.faces[last].texture_index[(start_vert+0)%3]
	m2=md2.faces[last].vertex_index[(start_vert+2)%3]
	st2=md2.faces[last].texture_index[(start_vert+2)%3]

	#look for matching triangle	
	check=start_tri+1
	for tri_counter in range(start_tri+1, md2.num_faces):
		for k in range(0,3):
			if md2.faces[check].vertex_index[k]!=m1:
				continue
			if md2.faces[check].texture_index[k]!=st1:
				continue
			if md2.faces[check].vertex_index[(k+1)%3]!=m2:
				continue
			if md2.faces[check].texture_index[(k+1)%3]!=st2:
				continue
			
			#if we can't use this triangle, this tri_strip is done
			if (used[tri_counter]!=0):
				for clear_counter in range(start_tri+1, md2.num_faces):
					if used[clear_counter]==2:
						used[clear_counter]=0
				return strip_count

			#new edge
			m2=md2.faces[check].vertex_index[(k+2)%3]
			st2=md2.faces[check].texture_index[(k+2)%3]
			
			strip_vert[strip_count+2]=m2
			strip_st[strip_count+2]=st2
			strip_tris[strip_count]=tri_counter
			strip_count+=1
	
			used[tri_counter]=2
		check+=1
	return strip_count


######################################################
# Globals for GL command list calculations
######################################################
used=[]
strip_vert=0
strip_st=0
strip_tris=0
strip_count=0

######################################################
# Build GL command List
######################################################
def build_GL_commands(md2):
	#variables shared between fan and strip functions
	global used
	used=[0]*md2.num_faces
	global strip_vert
	strip_vert=[0]*128
	global strip_st
	strip_st=[0]*128
	global strip_tris
	strip_tris=[0]*128
	global strip_count
	strip_count=0
	
	#variables
	num_commands=0
	start_vert=0
	fan_length=strip_length=0
	length=best_length=0
	best_type=0
	best_vert=[0]*1024
	best_st=[0]*1024
	best_tris=[0]*1024
	s=0.0
	t=0.0
	
	for face_counter in range(0,md2.num_faces):
		if used[face_counter]!=0: #don't evaluate a tri that's been used
			#print "found a used triangle: ", face_counter
			pass
		else:
			best_length=0 #restart the counter
			#for each vertex index in this face
			for start_vert in range(0,3):
				fan_length=find_fan_length(md2, face_counter, start_vert)
				if (fan_length>best_length):
					best_type=1
					best_length=fan_length
					for index in range (0, best_length+2):
						best_st[index]=strip_st[index]
						best_vert[index]=strip_vert[index]
					for index in range(0, best_length):
						best_tris[index]=strip_tris[index]
				
				strip_length=find_strip_length(md2, face_counter, start_vert)
				if (strip_length>best_length): 
					best_type=0
					best_length=strip_length
					for index in range (0, best_length+2):
						best_st[index]=strip_st[index]
						best_vert[index]=strip_vert[index]
					for index in range(0, best_length):
						best_tris[index]=strip_tris[index]
			
			#mark the tris on the best strip/fan as used
			for used_counter in range (0, best_length):
				used[best_tris[used_counter]]=1
	
			temp_cmdlist=md2_GL_cmd_list()
			#push the number of commands into the command stream
			if best_type==1:
				temp_cmdlist.num=best_length+2
				num_commands+=1
			else:	
				temp_cmdlist.num=(-(best_length+2))
				num_commands+=1
			for command_counter in range (0, best_length+2):
				#emit a vertex into the reorder buffer
				cmd=md2_GL_command()
				index=best_st[command_counter]
				#calc and put S/T coords in the structure
				s=md2.tex_coords[index].u
				t=md2.tex_coords[index].v
				s=(s+0.5)/md2.skin_width
				t=(t+0.5)/md2.skin_height
				cmd.s=s
				cmd.t=t
				cmd.vert_index=best_vert[command_counter]
				temp_cmdlist.cmd_list.append(cmd)
				num_commands+=3
			md2.GL_commands.append(temp_cmdlist)
	
	#end of list
	temp_cmdlist=md2_GL_cmd_list()	
	temp_cmdlist.num=0
	md2.GL_commands.append(temp_cmdlist)  
	num_commands+=1
	
	#cleanup and return
	used=best_vert=best_st=best_tris=strip_vert=strip_st=strip_tris=0
	return num_commands

######################################################
# Save MD2 Format
######################################################
def save_md2(filename):
	md2=md2_obj()  #blank md2 object to save

	#get the object
	mesh_objs = Blender.Object.GetSelected()

	#check there is a blender object selected
	if len(mesh_objs)==0:
		print "Fatal Error: Must select a mesh to output as MD2"
		print "Found nothing"
		result=Blender.Draw.PupMenu("Must select an object to export%t|OK")
		return

	mesh_obj=mesh_objs[0] #this gets the first object (should be only one)

	#check if it's a mesh object
	if mesh_obj.getType()!="Mesh":
		print "Fatal Error: Must select a mesh to output as MD2"
		print "Found: ", mesh_obj.getType()
		result=Blender.Draw.PupMenu("Selected Object must be a mesh to output as MD2%t|OK")
		return

	ok=validation(mesh_obj)
	if ok==False:
		return
	
	fill_md2(md2, mesh_obj)
	md2.dump()
	
	#actually write it to disk
	file=open(filename,"wb")
	md2.save(file)
	file.close()
	
	#cleanup
	md2=0
	
	print "Closed the file"
