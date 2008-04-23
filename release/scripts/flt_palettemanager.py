#!BPY

"""
Name: 'FLT Palette Manager'
Blender: 240
Group: 'Misc'
Tooltip: 'Manage FLT colors'
"""

__author__ = "Geoffrey Bantle"
__version__ = "1.0 11/21/2007"
__email__ = ('scripts', 'Author, ')
__url__ = ('blender', 'blenderartists.org')

__bpydoc__ ="""\

This script manages colors in OpenFlight databases. OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.

Todo:
-Figure out whats causing the PC speaker to beep when initializing...

Feature overview and more availible at:
http://wiki.blender.org/index.php/Scripts/Manual/FLTools
"""

# --------------------------------------------------------------------------
# flt_palettemanager.py version 0.1 2005/04/08
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2007: Blender Foundation
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

import Blender.Draw as Draw
from Blender.BGL import *
import Blender
import flt_properties
import flt_defaultp as defaultp
from flt_properties import *


palette_size = 12
palette_x = 0
palette_y = 0

colors = list()
curint = 1.0
curswatch = 0
#make a default palette, not very useful.
cinc = 1.0 / 1024.0
cstep = 0.0
picker = None
ptt = ""
for i in xrange(1024):
	colors.append([cstep,cstep,cstep])
	cstep = cstep + cinc
def update_state():
	state = dict()
	state["activeScene"] = Blender.Scene.getCurrent()
	state["activeObject"] = state["activeScene"].getActiveObject()
	state["activeMesh"] = None
	if state["activeObject"] and state["activeObject"].type == 'Mesh':
		state["activeMesh"] = state["activeObject"].getData(mesh=True)
	
	state["activeFace"] = None
	if state["activeMesh"]:
		if state["activeMesh"].faceUV and state["activeMesh"].activeFace != None:
			state["activeFace"] = state["activeMesh"].faces[state["activeMesh"].activeFace]
		
	return state
	
def pack_face_index(index, intensity):
	return ((127*intensity)+(128*index))
def unpack_face_index(face_index):
	index = face_index / 128
	intensity = float(face_index - 128.0 * index) / 127.0
	return(index,intensity)

def event(evt,val):
	global palette_size
	global palette_x
	global palette_y
	global colors
	global curint
	global curswatch
	
	areas = Blender.Window.GetScreenInfo()
	curarea = Blender.Window.GetAreaID()
	curRect = None
	editmode = 0

	for area in areas:
		if area['id'] == curarea:
			curRect = area['vertices']
			break
		
	if evt == Draw.LEFTMOUSE:
		mval = Blender.Window.GetMouseCoords()
		rastx = mval[0] - curRect[0]
		rasty = mval[1] - curRect[1]
		
		swatchx = (rastx -palette_x) / palette_size #+state["palette_x"]
		swatchy = (rasty -palette_y) / palette_size #+state["palette_y"]
                if rastx > palette_x and rastx < (palette_x + palette_size * 32) and rasty > palette_y and rasty < (palette_y+ palette_size* 32):
                        if swatchx < 32 and swatchy < 32:
                                curswatch = (swatchx * 32) + swatchy
                                Draw.Redraw(1)
		
                elif swatchy < 34 and swatchx < 32:
                        curint = 1.0 - (float(rastx-palette_x)/(palette_size*32.0))
                        Draw.Redraw(1)
	
	#copy current color and intensity to selected faces.
	elif evt == Draw.CKEY:
		
		if Blender.Window.EditMode():
			Blender.Window.EditMode(0)
			editmode = 1
		state = update_state()
		
		#retrieve color from palette
		color = struct.unpack('>BBBB',struct.pack('>I',colors[curswatch]))
		actmesh = state["activeMesh"]
		if actmesh: 
			if(Blender.Window.GetKeyQualifiers() != Blender.Window.Qual["CTRL"]):
				selfaces = list()
				for face in actmesh.faces:
					if face.sel:
						selfaces.append(face)
				
				if not "FLT_COL" in actmesh.faces.properties:
					actmesh.faces.addPropertyLayer("FLT_COL",Blender.Mesh.PropertyTypes["INT"])
					for face in actmesh.faces:
						face.setProperty("FLT_COL",127) #default
				try:
					actmesh.activeColorLayer = "FLT_Fcol"
				except:
					actmesh.addColorLayer("FLT_Fcol")
					actmesh.activeColorLayer = "FLT_Fcol"
				

				for face in selfaces:
					#First append packed index + color and store in face property
					face.setProperty("FLT_COL",int(pack_face_index(curswatch,curint))) 
					#Save baked color to face vertex colors
					for col in face.col:
						col.r = int(color[0] * curint)
						col.g = int(color[1] * curint)
						col.b = int(color[2] * curint)
						col.a = int(color[3] * curint)
			else:
				if Blender.Mesh.Mode() == Blender.Mesh.SelectModes['VERTEX']:
					if not 'FLT_VCOL' in actmesh.verts.properties:
						actmesh.verts.addPropertyLayer("FLT_VCOL",Blender.Mesh.PropertyTypes["INT"])
						for vert in actmesh.verts:
							vert.setProperty("FLT_VCOL",127)
					else:
						for vert in actmesh.verts:
							if vert.sel:
								vert.setProperty("FLT_VCOL",int(pack_face_index(curswatch,curint)))
			
			if editmode:
				Blender.Window.EditMode(1)
			
			Blender.Window.RedrawAll()
	
	#grab color and intensity from active face
	elif evt == Draw.VKEY:
		if Blender.Window.EditMode():
			Blender.Window.EditMode(0)
			editmode = 1
		state = update_state()
		
		actmesh = state["activeMesh"]
		activeFace = state["activeFace"]
		
		
		if activeFace:
			if not "FLT_COL" in actmesh.faces.properties:
				actmesh.faces.addPropertyLayer("FLT_COL",Blender.Mesh.PropertyTypes["INT"])
				for face in actmesh.faces:
					face.setProperty("FLT_COL",127) #default
			try:
				actmesh.activeColorLayer = "FLT_Fcol"
			except:
				actmesh.addColorLayer("FLT_Fcol")
				actmesh.activeColorLayer = "FLT_Fcol"
			tcol = activeFace.getProperty("FLT_COL")
			(index,intensity) = unpack_face_index(tcol)
			curswatch = index
			curint = intensity
			
		if editmode:
			Blender.Window.EditMode(1)
		
		Blender.Window.RedrawAll()
			
	elif evt == Draw.ESCKEY:
		Draw.Exit()
	
	if editmode:
		Blender.Window.EditMode(1)

def update_all():
	global colors
	state = update_state()
	#update the baked FLT colors for all meshes.
	for object in state["activeScene"].objects:
		if object.type == "Mesh":
			mesh = object.getData(mesh=True)
			if 'FLT_COL' in mesh.faces.properties:
				mesh.activeColorLayer = "FLT_Fcol"
				for face in mesh.faces:
					(index,intensity) = unpack_face_index(face.getProperty('FLT_COL'))
					color = struct.unpack('>BBBB',struct.pack('>I',colors[index]))
					#update the vertex colors for this face
					for col in face.col:
						col.r = int(color[0] * intensity)
						col.g = int(color[1] * intensity)
						col.b = int(color[2] * intensity)
						col.a = 255


def but_event(evt):
	global palette_size
	global palette_x
	global palette_y
	global colors
	global curint
	global curswatch
	global picker
	state = update_state()

	if evt == 1:
 		if picker.val:
			rval = (int(picker.val[0]*255),int(picker.val[1]*255),int(picker.val[2]*255),255)
			rval = struct.pack('>BBBB',rval[0],rval[1],rval[2],rval[3])
			rval = struct.unpack('>i',rval)
			colors[curswatch] = rval[0]	
			#go cd through all meshes and update their FLT colors
			update_all()

	Draw.Redraw(1)
def init_pal():
	global palette_size
	global palette_x
	global palette_y
	global colors
	global curint
	global curswatch
	
	state = update_state()

	if not state["activeScene"].properties.has_key('FLT'):
		state["activeScene"].properties['FLT'] = dict()

	try:
		colors = state["activeScene"].properties['FLT']['Color Palette']
	except:
		state["activeScene"].properties['FLT']['Color Palette'] = defaultp.pal
		colors = state["activeScene"].properties['FLT']['Color Palette']

def draw_palette():
	global palette_size
	global palette_x
	global palette_y
	global colors
	global curint
	global curswatch
        global picker

	state = update_state()
	init_pal()

	ssize = palette_size
	xpos = palette_x
	cid = 0

	highlight = [(palette_x,palette_y),(palette_x+palette_size,palette_y),(palette_x+palette_size,palette_y+palette_size),(palette_x,palette_y+palette_size)]
	for x in xrange(32):
		ypos = palette_y
		for y in xrange(32):
			color = struct.unpack('>BBBB',struct.pack('>I',colors[cid]))
			glColor3f(color[0]/255.0,color[1]/255.0,color[2]/255.0)
			glBegin(GL_POLYGON)
			glVertex2i(xpos,ypos)
			glVertex2i(xpos+ssize,ypos)
			glVertex2i(xpos+ssize,ypos+ssize)
			glVertex2i(xpos,ypos+ssize)				
			glEnd()
			
			if curswatch == cid:
				highlight[0] = (xpos,ypos)
				highlight[1] = (xpos+ssize,ypos)
				highlight[2] = (xpos+ssize,ypos+ssize)
				highlight[3] = (xpos,ypos+ssize)
		
			glColor3f(0.0,0.0,0.0)
			glBegin(GL_LINE_LOOP)
			glVertex2i(xpos,ypos)
			glVertex2i(xpos+ssize,ypos)
			glVertex2i(xpos+ssize,ypos+ssize)
			glVertex2i(xpos,ypos+ssize)
			glVertex2i(xpos,ypos)				
			glEnd()			
				
			
			cid = cid + 1
			ypos = ypos + ssize
		
		xpos = xpos + ssize
	
	#draw intensity gradient
	color = struct.unpack('>BBBB',struct.pack('>I',colors[curswatch]))
	color = [color[0]/255.0,color[1]/255.0,color[2]/255.0]
	colsteps = [color[0]/255.0,color[1]/255.0,color[2]/255.0]
	stripwidth = (palette_size * 32.0) / 256
	strippad = palette_size / 2.0
	
	xpos = palette_x
	grady = (palette_y + (palette_size * 32.0)) + strippad
	for x in xrange(256):
		color[0] = color[0] - colsteps[0]
		color[1] = color[1] - colsteps[1] 
		color[2] = color[2] - colsteps[2]

		glColor3f(color[0], color[1] ,color[2])
		glBegin(GL_POLYGON)
		glVertex2f(xpos,grady)
		glVertex2f(xpos+stripwidth,grady)
		glVertex2f(xpos+stripwidth,grady+palette_size)
		glVertex2f(xpos,grady+palette_size)
		glEnd()
		xpos = xpos + stripwidth
		
	#draw intensity slider bar
	#xposition ==  512 - ((curint) * 512)
	xpos = ((palette_size*32) * (1.0 - curint)) + palette_x
	glColor3f(1.0,1.0,1.0)
	glBegin(GL_LINE_LOOP)
	glVertex2i(xpos-6,grady-1)
	glVertex2i(xpos+6,grady-1)
	glVertex2i(xpos+6,grady+palette_size+1)
	glVertex2i(xpos-6,grady+palette_size+1)
	#glVertex2i(xpos-6,grady+7)
	glEnd()

	#draw color picker
	color = struct.unpack('>BBBB',struct.pack('>I',colors[curswatch]))
	pickcol = (color[0]/255.0,color[1]/255.0,color[2]/255.0)
	picker = Blender.Draw.ColorPicker(1,highlight[0][0]+1,highlight[0][1]+1,ssize-2,ssize-2,pickcol,ptt)

	#draw highlight swatch
	glColor3f(1.0,1.0,1.0)
	glBegin(GL_LINE_LOOP)
	glVertex2i(highlight[0][0],highlight[0][1])
	glVertex2i(highlight[1][0],highlight[1][1])
	glVertex2i(highlight[2][0],highlight[2][1])
	glVertex2i(highlight[3][0],highlight[3][1])
	glVertex2i(highlight[0][0],highlight[0][1])
	glEnd()			

def gui():
	glClearColor(0.5,0.5,0.5,1.0)
	glClear(GL_COLOR_BUFFER_BIT)
	draw_palette()


init_pal()
Draw.Register(gui,event,but_event)
	
