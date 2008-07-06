#!BPY

"""
Name: 'MD2 (.md2)'
Blender: 243
Group: 'Export'
Tooltip: 'Export to Quake file format (.md2).'
"""

__author__ = 'Bob Holcomb'
__version__ = '0.18.1 patch 1'
__url__ = ["Bob's site, http://bane.servebeer.com",
     "Support forum, http://bane.servebeer.com", "blender", "blenderartists.org"]
__email__ = ["Bob Holcomb, bob_holcomb:hotmail*com", "scripts"]
__bpydoc__ = """\
This script Exports a Quake 2 file (MD2).

 Additional help from: Shadwolf, Skandal, Rojo, Cambo<br>
 Thanks Guys!
"""

# This is a PATCHED VERSION, fixing the bug due to which animations would
# (almost) never work.  It is now also possible to output a MD2 model without
# texture.
# On: 23 january 2008
# By: Boris van Schooten (schooten@cs.utwente.nl)

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
g_filename=Create("tris.md2")
g_frame_filename=Create("default")

g_filename_search=Create("")
g_frame_search=Create("default")

g_texture_path=Create("")

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
	global g_texture_path

	########## Titles
	glClear(GL_COLOR_BUFFER_BIT)
	glRasterPos2d(10, 120)
	Text("MD2 Export")

	######### Parameters GUI Buttons
	######### MD2 Filename text entry
	g_filename = String("MD2 file to save: ", EVENT_NOEVENT, 10, 75, 210, 18,
                            g_filename.val, 255, "MD2 file to save")
	########## MD2 File Search Button
	Button("Browse",EVENT_CHOOSE_FILENAME,220,75,80,18)

	##########  MD2 Frame List Text entry
	g_frame_filename = String("Frame List file to load: ", EVENT_NOEVENT, 10, 55, 210, 18,
                                g_frame_filename.val, 255, "Frame List to load-overrides MD2 defaults")
	########## Frame List Search Button
	Button("Browse",EVENT_CHOOSE_FRAME,220,55,80,18)
	
	##########  Texture path to append
	g_texture_path=String("Texture Path: ", EVENT_NOEVENT, 10,35,210,18,
														g_texture_path.val,255, "Texture path to prepend")


	########## Scale slider-default is 1/8 which is a good scale for md2->blender
	g_scale= Slider("Scale Factor: ", EVENT_NOEVENT, 10, 95, 210, 18,
                    1.0, 0.001, 10.0, 1, "Scale factor for object Model");

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

	#move the object to the origin if it's not already there
	if object.getLocation('worldspace')!=(0.0, 0.0, 0.0):
		print "Model not centered at origin"
		result=Blender.Draw.PupMenu("Model not centered at origin%t|Center (will not work with animations!)|Do not center")
		if result==1:
			object.setLocation(0.0,0.0,0.0)

	#resize the object in case it is not the right size
	if object.getSize('worldspace')!=(1.0,1.0,1.0):
		print "Object is scaled-You should scale the mesh verts, not the object"
		result=Blender.Draw.PupMenu("Object is scaled-You should scale the mesh verts, not the object%t|Fix scale (will not work with animations!)|Do not scale")
		if result==1:
			object.setSize(1.0,1.0,1.0)
		
	if object.getEuler('worldspace')!=Blender.Mathutils.Euler(0.0,0.0,0.0):
		print "object.rot: ", object.getEuler('worldspace')
		print "Object is rotated-You should rotate the mesh verts, not the object"
		result=Blender.Draw.PupMenu("Object is rotated-You should rotate the mesh verts, not the object%t|Fix rotation (will not work with animations!)|Do not rotate")
		if result==1:
			object.setEuler([0.0,0.0,0.0])
	
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
			  result=Blender.Draw.PupMenu("Model not made entirely out of Triangles-Convert?%t|YES|Quit")
	
	#triangulate or quit
	if result==1:
		#selecting face mode
		Blender.Mesh.Mode(3)
		editmode = Window.EditMode()    # are we in edit mode?  If so ...
		if editmode: Window.EditMode(0) # leave edit mode before getting the mesh
		mesh.quadToTriangle(0) #use closest verticies in breaking a quad
	elif result==2:
		return False #user will fix (I guess)

	#check it has UV coordinates
	if mesh.vertexUV==True:
		print "Vertex UV not supported"
		result=Blender.Draw.PupMenu("Vertex UV not suppored-Use Sticky UV%t|Quit")
		return False
			
	elif mesh.faceUV==True:
		for face in mesh.faces:
			if(len(face.uv)==3):
				pass
			else:
				print "Model's vertices do not all have UV"
				result=Blender.Draw.PupMenu("Model's vertices do not all have UV%t|Quit")
				return False
	
	else:
		print "Model does not have UV (face or vertex)"
		result=Blender.Draw.PupMenu("Model does not have UV (face or vertex)%t|Output (0,0) as UV coordinates and do not generate GL commands|Quit")
		if result==2:
			return False

	#check it has an associated texture map
	last_face=""
	if mesh.faceUV:
		last_face=mesh.faces[0].image
		#check if each face uses the same texture map (only one allowed)
		for face in mesh.faces:
			mesh_image=face.image
			if not mesh_image:
				print "Model has a face without a texture Map"
				result=Blender.Draw.PupMenu("Model has a face without a texture Map%t|This should never happen!")
				#return False
			if mesh_image!=last_face:
				print "Model has more than 1 texture map assigned"
				result=Blender.Draw.PupMenu("Model has more than 1 texture map assigned%t|Quit")
				#return False
		if mesh_image:
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
	#global defines
	global user_frame_list
	global g_texture_path
	
	Blender.Window.DrawProgressBar(0.25,"Filling MD2 Data")
	
	#get a Mesh, not NMesh
	mesh=object.getData(False, True)	
	#don't forget to copy the data! -- Boris van Schooten
	mesh=mesh.__copy__();
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
	if mesh.faceUV:
		mesh_image=mesh.faces[0].image
		size=mesh_image.getSize()
		md2.skin_width=size[0]
		md2.skin_height=size[1]
		md2.num_skins=1
		#add a skin node to the md2 data structure
		md2.skins.append(md2_skin())
		md2.skins[0].name=g_texture_path.val+Blender.sys.basename(mesh_image.getFilename())
		if len(md2.skins[0].name)>64:
			print "Texture Path and name is more than 64 characters"
			result=Blender.Draw.PupMenu("Texture path and name is more than 64 characters-Quitting")
			return False

	#put texture information in the md2 structure
	#build UV coord dictionary (prevents double entries-saves space)
	for face in mesh.faces:
		for i in xrange(0,3):
			if mesh.faceUV:
				t=(face.uv[i])
			else:
				t=(0,0)
			tex_key=(t[0],t[1])
			if not tex_list.has_key(tex_key):
				tex_list[tex_key]=tex_count
				tex_count+=1
	md2.num_tex_coords=tex_count #each vert has its own UV coord

	for this_tex in xrange (0, md2.num_tex_coords):
		md2.tex_coords.append(md2_tex_coord())
	for coord, index in tex_list.iteritems():
		#md2.tex_coords.append(md2_tex_coord())
		md2.tex_coords[index].u=int(coord[0]*md2.skin_width)
		md2.tex_coords[index].v=int((1-coord[1])*md2.skin_height)

	#put faces in the md2 structure
	#for each face in the model
	for this_face in xrange(0, md2.num_faces):
		md2.faces.append(md2_face())
		for i in xrange(0,3):
			#blender uses indexed vertexes so this works very well
			md2.faces[this_face].vertex_index[i]=mesh.faces[this_face].verts[i].index
			#lookup texture index in dictionary
			if mesh.faceUV:
				uv_coord=(mesh.faces[this_face].uv[i])
			else:
				uv_coord=(0,0)
			tex_key=(uv_coord[0],uv_coord[1])
			tex_index=tex_list[tex_key]
			md2.faces[this_face].texture_index[i]=tex_index
	
	Blender.Window.DrawProgressBar(0.5, "Computing GL Commands")

	#compute GL commands
	md2.num_GL_commands=build_GL_commands(md2, mesh)

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
	

	progress=0.5
	progressIncrement=0.25/md2.num_frames

	#fill in each frame with frame info and all the vertex data for that frame
	for frame_counter in xrange(0,md2.num_frames):
		
		progress+=progressIncrement
		Blender.Window.DrawProgressBar(progress, "Calculating Frame: "+str(frame_counter))
			
		#add a frame
		md2.frames.append(md2_frame())
		#update the mesh objects vertex positions for the animation
		Blender.Set("curframe", frame_counter)  #set blender to the correct frame
		mesh.getFromObject(object.name)  #update the mesh to make verts current
		
#each frame has a scale and transform value that gets the vertex value between 0-255
#since the scale and transform are the same for the all the verts in the frame, we only need
#to figure this out once per frame
		
		#we need to start with the bounding box
		#bounding_box=object.getBoundBox() #uses the object, not the mesh data
		#initialize with the first vertex for both min and max.  X and Y are swapped for MD2 format
	
		#initialize 
		frame_min_x=100000.0
		frame_max_x=-100000.0
		frame_min_y=100000.0
		frame_max_y=-100000.0
		frame_min_z=100000.0
		frame_max_z=-100000.0
	
		for face in mesh.faces:
			for vert in face.verts:					
				if frame_min_x>vert.co[1]: frame_min_x=vert.co[1]
				if frame_max_x<vert.co[1]: frame_max_x=vert.co[1]
				if frame_min_y>vert.co[0]: frame_min_y=vert.co[0]
				if frame_max_y<vert.co[0]: frame_max_y=vert.co[0]
				if frame_min_z>vert.co[2]: frame_min_z=vert.co[2]
				if frame_max_z<vert.co[2]: frame_max_z=vert.co[2]
		
		#the scale is the difference between the min and max (on that axis) / 255
		frame_scale_x=(frame_max_x-frame_min_x)/255
		frame_scale_y=(frame_max_y-frame_min_y)/255
		frame_scale_z=(frame_max_z-frame_min_z)/255
		
		if frame_scale_x == 0: frame_scale_x = 1.0
		if frame_scale_y == 0: frame_scale_y = 1.0
		if frame_scale_z == 0: frame_scale_z = 1.0
		
		#translate value of the mesh to center it on the origin
		frame_trans_x=frame_min_x
		frame_trans_y=frame_min_y
		frame_trans_z=frame_min_z
		
		#fill in the data
		md2.frames[frame_counter].scale=(-frame_scale_x, frame_scale_y, frame_scale_z)
		md2.frames[frame_counter].translate=(-frame_trans_x, frame_trans_y, frame_trans_z)
		
		#now for the vertices
		for vert_counter in xrange(0, md2.num_vertices):
			#add a vertex to the md2 structure
			md2.frames[frame_counter].vertices.append(md2_point())
			#figure out the new coords based on scale and transform
			#then translates the point so it's not less than 0
			#then scale it so it's between 0..255
			#print "frame scale : ", frame_scale_x, " ", frame_scale_y, " ", frame_scale_z
			new_x=int((mesh.verts[vert_counter].co[1]-frame_trans_x)/frame_scale_x)
			new_y=int((mesh.verts[vert_counter].co[0]-frame_trans_y)/frame_scale_y)
			new_z=int((mesh.verts[vert_counter].co[2]-frame_trans_z)/frame_scale_z)
			#put them in the structure
			md2.frames[frame_counter].vertices[vert_counter].vertices=(new_x, new_y, new_z)

			#need to add the lookup table check here
			maxdot = -999999.0;
			maxdotindex = -1;

			
			#swap y and x for difference in axis orientation 
			x1=-mesh.verts[vert_counter].no[1]
			y1=mesh.verts[vert_counter].no[0]
			z1=mesh.verts[vert_counter].no[2]
			for j in xrange(0,162):
				#dot = (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
				dot = (x1*MD2_NORMALS[j][0]+
				       y1*MD2_NORMALS[j][1]+
							 z1*MD2_NORMALS[j][2]);
				if (dot > maxdot):
					maxdot = dot;
					maxdotindex = j;
			
			md2.frames[frame_counter].vertices[vert_counter].lightnormalindex=maxdotindex+2
			
			del maxdot, maxdotindex
			del new_x, new_y, new_z
		del frame_max_x, frame_max_y, frame_max_z, frame_min_x, frame_min_y, frame_min_z
		del frame_scale_x, frame_scale_y, frame_scale_z, frame_trans_x, frame_trans_y, frame_trans_z			
			
			
	#output all the frame names-user_frame_list is loaded during the validation
	for frame_set in user_frame_list:
		for counter in xrange(frame_set[1]-1, frame_set[2]):
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
			if lines[0].strip() != "# MD2 Frame Name List":
				print "its not a valid file"
				result=Blender.Draw.PupMenu("This is not a valid frame definition file-using default%t|OK")
				return MD2_FRAME_NAME_LIST
			else:
				#read in the data
				num_frames=0
				for counter in xrange(1, len(lines)):
					current_line=lines[counter].strip()
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
# Globals for GL command list calculations
######################################################
used_tris=[]
edge_dict={}
strip_verts=[]
strip_st=[]
strip_tris=[]
strip_first_run=True
odd=False

######################################################
# Find Strip length function
######################################################
def find_strip_length(mesh, start_tri, edge_key):
	#print "Finding strip length"
	
	global used_tris
	global edge_dict
	global strip_tris
	global strip_st
	global strip_verts
	global strip_first_run
	global odd
	
	used_tris[start_tri]=2
	
	strip_tris.append(start_tri) #add this tri to the potential list of tri-strip						
	
	#print "I am face: ", start_tri
	#print "Using edge Key: ", edge_key
	
	faces=edge_dict[edge_key] #get list of face indexes that share this edge
	if (len(faces)==0):
		#print "Cant find edge with key: ", edge_key
		pass
		
	#print "Faces sharing this edge: ", faces
	for face_index in faces:
		face=mesh.faces[face_index]
		if face_index==start_tri: #don't want to check myself
			#print "I found myself, continuing"
			pass
		else:
			if used_tris[face_index]!=0: #found a used tri-move along
				#print "Found a used tri: ", face_index
				pass
			else:
				#find non-shared vert
				for vert_counter in xrange(0,3):
					if (face.verts[vert_counter].index!=edge_key[0] and face.verts[vert_counter].index!=edge_key[1]):
						next_vert=vert_counter
						
						if(odd==False):
							#print "Found a suitable even connecting tri: ", face_index			
							used_tris[face_index]=2 #mark as dirty for this rum
							odd=True
										
							#find the new edge
							if(face.verts[next_vert].index < face.verts[(next_vert+2)%3].index):
								temp_key=(face.verts[next_vert].index,face.verts[(next_vert+2)%3].index)
							else:
								temp_key=(face.verts[(next_vert+2)%3].index, face.verts[next_vert].index)
							
							#print "temp key: ", temp_key
							temp_faces=edge_dict[temp_key]
							
							if(len(temp_faces)==0):
								print "Can't find any other faces with key: ", temp_key
							else:
								#search the new edge	
								#print "found other faces, searching them"	
								find_strip_length(mesh, face_index, temp_key) #recursive greedy-takes first tri it finds as best 
								break;
						else:
							#print "Found a suitable odd connecting tri: ", face_index			
							used_tris[face_index]=2 #mark as dirty for this rum
							odd=False
								
							#find the new edge
							if(face.verts[next_vert].index < face.verts[(next_vert+1)%3].index):
								temp_key=(face.verts[next_vert].index,face.verts[(next_vert+1)%3].index)
							else:
								temp_key=(face.verts[(next_vert+1)%3].index, face.verts[next_vert].index)
							#print "temp key: ", temp_key
							temp_faces=edge_dict[temp_key]
							if(len(temp_faces)==0):
								print "Can't find any other faces with key: ", temp_key
							else:
								#search the new edge	
								#print "found other faces, searching them"	
								find_strip_length(mesh, face_index, temp_key) #recursive greedy-takes first tri it finds as best 
								break;

	return len(strip_tris)


######################################################
# Tri-Stripify function
######################################################
def stripify_tri_list(mesh, edge_key):
	global edge_dict
	global strip_tris
	global strip_st
	global strip_verts
	
	shared_edge=[]
	key=[]
	
	#print "*****Stripify the triangle list*******"
	#print "strip tris: ", strip_tris
	#print "strip_tri length: ", len(strip_tris)
		
	for tri_counter in xrange(0, len(strip_tris)):
		face=mesh.faces[strip_tris[tri_counter]]
		if (tri_counter==0): #first one only 
			#find non-edge vert
			for vert_counter in xrange(0,3):
				if (face.verts[vert_counter].index!=edge_key[0] and face.verts[vert_counter].index!=edge_key[1]):
					start_vert=vert_counter
			strip_verts.append(face.verts[start_vert].index)
			strip_st.append(face.uv[start_vert])
			
			strip_verts.append(face.verts[(start_vert+2)%3].index)
			strip_st.append(face.uv[(start_vert+2)%3])

			strip_verts.append(face.verts[(start_vert+1)%3].index)
			strip_st.append(face.uv[(start_vert+1)%3])
		else:
			for vert_counter in xrange(0,3):
				if(face.verts[vert_counter].index!=strip_verts[-1] and face.verts[vert_counter].index!=strip_verts[-2]):
					strip_verts.append(face.verts[vert_counter].index)
					strip_st.append(face.uv[vert_counter])
					break
		
	

######################################################
# Build GL command List
######################################################
def build_GL_commands(md2, mesh):
	# we can't output gl command structure without uv
	if not mesh.faceUV:
		print "No UV: not building GL Commands"
		return 0

	print "Building GL Commands"

	global used_tris
	global edge_dict
	global strip_verts
	global strip_tris
	global strip_st
	
	#globals initialization
	used_tris=[0]*len(mesh.faces)
	#print "Used: ", used_tris
	num_commands=0
	
	#edge dictionary generation
	edge_dict=dict([(ed.key,[]) for ed in mesh.edges])
	for face in (mesh.faces):
		for key in face.edge_keys:
			edge_dict[key].append(face.index)
	
	#print "edge Dict: ", edge_dict

	for tri_counter in xrange(0,len(mesh.faces)):
		if used_tris[tri_counter]!=0: 
			#print "Found a used triangle: ", tri_counter
			pass
		else:
			#print "Found an unused triangle: ", tri_counter

			#intialization
			strip_tris=[0]*0
			strip_verts=[0]*0
			strip_st=[0]*0
			strip_first_run=True
			odd=True
			
			#find the strip length
			strip_length=find_strip_length(mesh, tri_counter, mesh.faces[tri_counter].edge_keys[0])

			#mark tris as used
			for used_counter in xrange(0,strip_length):
				used_tris[strip_tris[used_counter]]=1
				
			stripify_tri_list(mesh, mesh.faces[tri_counter].edge_keys[0])

			#create command list
			cmd_list=md2_GL_cmd_list()
			#number of commands in this list			
			print "strip length: ", strip_length
			cmd_list.num=(len(strip_tris)+2) #positive for strips, fans would be negative, but not supported yet
			num_commands+=1
			
			#add s,t,vert for this command list
			for command_counter in xrange(0, len(strip_tris)+2):
				cmd=md2_GL_command()
				cmd.s=strip_st[command_counter][0]
				cmd.t=1.0-strip_st[command_counter][1] #flip upside down
				cmd.vert_index=strip_verts[command_counter]
				num_commands+=3
				cmd_list.cmd_list.append(cmd)
			print "Cmd List length: ", len(cmd_list.cmd_list)
			print "Cmd list num: ", cmd_list.num
			print "Cmd List: ", cmd_list.dump()
			md2.GL_commands.append(cmd_list)		

	#add the null command at the end
	temp_cmdlist=md2_GL_cmd_list()	
	temp_cmdlist.num=0
	md2.GL_commands.append(temp_cmdlist)  
	num_commands+=1		

	#cleanup and return
	used=strip_vert=strip_st=strip_tris=0
	return num_commands
		



######################################################
# Save MD2 Format
######################################################
def save_md2(filename):
	print ""
	print "***********************************"
	print "MD2 Export"
	print "***********************************"
	print ""
	
	Blender.Window.DrawProgressBar(0.0,"Begining MD2 Export")
	
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
	
	Blender.Window.DrawProgressBar(1.0, "Writing to Disk")
	
	#actually write it to disk
	file=open(filename,"wb")
	md2.save(file)
	file.close()
	
	#cleanup
	md2=0
	
	print "Closed the file"

