#!BPY

"""
Name: 'UV Face Layout'
Blender: 232
Group: 'Export'
Tooltip: 'Export the UV Faces layout of the  selected object to tga'
""" 

# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003: Martin Poirier, theeth@yahoo.com
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
#    thanks to jms for the tga functions
# --------------------------
#    Version 1.1      
# Clear a bug that crashed the script when UV coords overlapped in the same faces
# --------------------------
#    Version 1.2
# Now with option to use the object's name as filename
# --------------------------
#    Version 1.3 Updates by Zaz from Elysiun.com
# Default path is now the directory of the last saved .blend
# New options: Work on selected face only & Scale image when face wraps
# --------------------------

import Blender
from math import *

default_file = Blender.sys.dirname(Blender.Get ("filename")) + Blender.sys.sep

bSize = Blender.Draw.Create(500)
bWSize = Blender.Draw.Create(1)
bFile = Blender.Draw.Create(default_file)
bObFile = Blender.Draw.Create(1)
bWrap = Blender.Draw.Create(1)
bAllFaces = Blender.Draw.Create(1)

def draw():
	global bSize, bWSize, bFile, bObFile, bWrap, bAllFaces
	# clearing screen
	Blender.BGL.glClearColor(0.5, 0.5, 0.5, 1)
	Blender.BGL.glColor3f(1.,1.,1.)
	Blender.BGL.glClear(Blender.BGL.GL_COLOR_BUFFER_BIT)

	#Title
	Blender.BGL.glColor3f(1, 1, 1)
	Blender.BGL.glRasterPos2d(8, 183)
	Blender.Draw.Text("Blender UF Faces Export")
	Blender.BGL.glRasterPos2d(8, 163)
	Blender.Draw.Text("""(C) Feb. 2003 Martin Poirier (aka "theeth")""")
  
	# Instructions
	Blender.BGL.glRasterPos2d(8, 83)
	Blender.Draw.Text("1 - Select the mesh you want to export")
	Blender.BGL.glRasterPos2d(8, 63)
	Blender.Draw.Text("2 - Define the Size and WireSize parameters")
	Blender.BGL.glRasterPos2d(8, 43)
	Blender.Draw.Text("3 - Push the EXPORT button!!!")

	# Buttons
	Blender.Draw.Button("EXPORT", 3, 10, 10, 100, 25)
	Blender.Draw.Button("Exit", 1, 200, 177, 40, 18)
	bSize = Blender.Draw.Number("Size", 4, 10, 130, 90, 18, bSize.val, 100, 10000, "Size of the exported image")
	bWSize = Blender.Draw.Number("Wire Size", 4, 120, 130, 90, 18, bWSize.val, 1, 5, "Size of the wire of the faces")
	bWrap = Blender.Draw.Toggle("Wrap", 5, 220, 130, 50, 20, bWrap.val, "Wrap to image size, scale otherwise")
	bAllFaces = Blender.Draw.Toggle("AllFaces", 6, 280, 130, 60, 20, bAllFaces.val, "Export All or only selected faces")

	bFile = Blender.Draw.String("Path: ", 4, 10, 100, 200, 20, bFile.val, 100, "Filename path")
	bObFile = Blender.Draw.Toggle("Ob", 4, 212, 100, 30, 20, bObFile.val, "Use object name in filename")

def event(evt, val):
	if evt == Blender.Draw.ESCKEY and not val: Blender.Draw.Exit()

def bevent(evt):
	bSize, bWSize, bFile, bObFile
	if evt == 1: Blender.Draw.Exit()
	if evt == 3:
		if bObFile.val:
			UV_Export(bSize.val, bWSize.val, bFile.val + Blender.Object.GetSelected()[0].name + ".tga")
		else:
			UV_Export(bSize.val, bWSize.val, bFile.val)

def Buffer(height=16, width=16, profondeur=3,rvb=255 ):  
   """  
   reserve l'espace memoire necessaire  
   """  
   p=[rvb]  
   b=p*height*width*profondeur  
   return b  

def write_tgafile(loc2,bitmap,width,height,profondeur):  
   f=open(loc2,'wb')  

   Origine_en_haut_a_gauche=32  
   Origine_en_bas_a_gauche=0  

   Data_Type_2=2  
   RVB=profondeur*8  
   RVBA=32  
   entete0=[]  
   for t in range(18):  
     entete0.append(chr(0))  

   entete0[2]=chr(Data_Type_2)  
   entete0[13]=chr(width/256)  
   entete0[12]=chr(width % 256)  
   entete0[15]=chr(height/256)  
   entete0[14]=chr(height % 256)  
   entete0[16]=chr(RVB)  
   entete0[17]=chr(Origine_en_bas_a_gauche)  

   #Origine_en_haut_a_gauche  

   for t in entete0:  
     f.write(t)  

   for t in bitmap:  
     f.write(chr(t))  
   f.close()  

def UV_Export(size, wsize, file):
	obj = Blender.Object.GetSelected()
	if not obj:
		Blender.Draw.PupMenu("ERROR%t|No Active Object!")
		return
	obj = obj[0];
	if obj.getType() != "Mesh":
		Blender.Draw.PupMenu("ERROR%t|Not a Mesh!")
		return
	mesh = obj.getData()
	if not mesh.hasFaceUV():
		Blender.Draw.PupMenu("ERROR%t|No UV coordinates!")
		return
	

	vList = []
	faces = []

	minx = 0
	miny = 0
	scale = 1.0
	
	step = 0

	if bAllFaces.val:
		faces = mesh.faces

	else:
		faces = mesh.getSelectedFaces ()

	for f in faces:
		vList.append(f.uv)

	img = Buffer(size+1,size+1)

	if bWrap.val:
		wrapSize = size
	else:
		wrapSize = size
		maxx = -100000
		maxy = -100000
		for f in vList:
			for v in f:
				x = int(v[0] * size)
				maxx = max (x, maxx)
				minx = min (x, minx)
				
				y = int(v[1] * size)
				maxy = max (y, maxy)
				miny = min (y, miny)
		wrapSize = max (maxx - minx + 1, maxy - miny + 1)
		scale = float (size) / float (wrapSize)

	fnum = 0
	fcnt = len (vList)

	for f in vList:
		fnum = fnum + 1
		if not fnum % 100:
			print "Face " + str (fnum) + " of " + str (fcnt)
			
		for index in range(len(f)):
			co1 = f[index]
			if index < len(f) - 1:
				co2 = f[index + 1]
			else:
				co2 = f[0]
			step = int(size*sqrt((co1[0]-co2[0])**2+(co1[1]-co2[1])**2))
			if step:
				for t in range(step + 1):
					x = int((co1[0] + t*(co2[0]-co1[0])/step) * size)
					y = int((co1[1] + t*(co2[1]-co1[1])/step) * size)

					if bWrap.val:
						x = x % wrapSize
						y = y % wrapSize
					else:
						x = int ((x - minx) * scale)
						y = int ((y - miny) * scale)
						
					co = x * 3 + y * 3 * size
					img[co] = 0
					img[co+1] = 0
					img[co+2] = 255
					if wsize > 1:
						for x in range(-1*wsize + 1,wsize):
							for y in range(-1*wsize,wsize):
								img[co + 3 * x + y * 3 * size] = 0
								img[co + 3 * x + y * 3 * size +1] = 0
								img[co + 3 * x + y * 3 * size +2] = 255
	
		for v in f:
			x = int(v[0] * size)
			y = int(v[1] * size)

			if bWrap.val:
				x = x % wrapSize
				y = y % wrapSize
			else:
				x = int ((x - minx) * scale)
				y = int ((y - miny) * scale)

			co = x * 3 + y * 3 * size
			img[co] = 0
			img[co+1] = 0
			img[co+2] = 0
				
	
	
	write_tgafile(file,img,size,size,3)

Blender.Draw.Register(draw,event,bevent)
