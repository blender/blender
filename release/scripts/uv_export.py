#!BPY

"""
Name: 'Save UV Face Layout...'
Blender: 232
Group: 'UV'
Tooltip: 'Export the UV face layout of the selected object to a .TGA file'
""" 

__author__ = "Martin 'theeth' Poirier"
__url__ = ("http://www.blender.org", "http://www.elysiun.com")
__version__ = "1.4"

__bpydoc__ = """\
This script exports the UV face layout of the selected mesh object to
a TGA image file.  Then you can, for example, paint details in this image using
an external 2d paint program of your choice and bring it back to be used as a
texture for the mesh.

Usage:

Open this script from UV/Image Editor's "UVs" menu, make sure there is a mesh
selected, define size and wire size parameters and push "Export" button.

There are more options to configure, like setting export path, if image should
use object's name and more.

Notes:<br>
	 Jean-Michel Soler (jms) wrote TGA functions used by this script.<br>
	 Zaz added the default path code and Selected Face option.<br>
	 Macouno fixed a rounding error in the step calculations<br>
"""


# $Id$
#
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
# thanks to jms for the tga functions:
# Writetga and buffer functions
# (c) 2002-2004 J-M Soler released under GPL licence
# Official Page :
# http://jmsoler.free.fr/didacticiel/blender/tutor/write_tga_pic.htm
# Communicate problems and errors on:
# http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender 
# --------------------------
#	 Version 1.1		
# Clear a bug that crashed the script when UV coords overlapped in the same faces
# --------------------------
#	 Version 1.2
# Now with option to use the object's name as filename
# --------------------------
#	 Version 1.3 Updates by Zaz from Elysiun.com
# Default path is now the directory of the last saved .blend
# New options: Work on selected face only & Scale image when face wraps
# --------------------------
#	 Version 1.3a
# Corrected a minor typo and added the tga extension to both export call
# --------------------------
#	 Version 1.4 Updates by Macouno from Elysiun.com
# Fixed rounding error that can cause breaks in lines.
# --------------------------
#  Version 2.0
# New interface using PupBlock and FileSelector
# Save/Load config to Registry
# Edit in external program
# --------------------------

FullPython = False

import Blender

try:
	import os
	FullPython = True
except:
	pass

from math import *


def ExportConfig():
	conf = {}
	
	conf["SIZE"] = bSize.val
	conf["WSIZE"] = bWSize.val
	conf["OBFILE"] = bObFile.val
	conf["WRAP"] = bWrap.val
	conf["ALLFACES"] = bAllFaces.val
	conf["EDIT"] = bEdit.val
	conf["EXTERNALEDITOR"] = bEditPath.val
	
	Blender.Registry.SetKey("UVEXPORT", conf, True)

def ImportConfig():
	global bSize, bWSize, bObFile, bWrap, bAllFaces
	
	conf = Blender.Registry.GetKey("UVEXPORT", True)
	
	if not conf:
		return
	
	try:
		bSize.val = conf["SIZE"]
		bWSize.val = conf["WSIZE"]
		bObFile.val = conf["OBFILE"]
		bWrap.val = conf["WRAP"]
		bAllFaces.val = conf["ALLFACES"]
		bEdit.val = conf["EDIT"]
		editor = conf["EXTERNALEDITOR"]
		if editor:
			bEditPath.val = editor
	except KeyError:
		pass
			
	

def Export(f):
	obj = Blender.Scene.getCurrent().getActiveObject()

	if not obj:
		Blender.Draw.PupMenu("ERROR%t|No Active Object!")
		return

	if obj.getType() != "Mesh":
		Blender.Draw.PupMenu("ERROR%t|Not a Mesh!")
		return

	mesh = obj.getData()
	if not mesh.hasFaceUV():
		Blender.Draw.PupMenu("ERROR%t|No UV coordinates!")
		return
	
	if bObFile.val:
		name = f + obj.name + ".tga"
	else:
		name = f + ".tga"
		
	UV_Export(mesh, bSize.val, bWSize.val, bWrap.val, bAllFaces.val, name)
	
	if FullPython and bEdit.val and bEditPath.val:
		filepath = os.path.realpath(name)
		print filepath
		os.spawnl(os.P_NOWAIT, bEditPath.val, "", filepath)
	

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

def UV_Export(mesh, size, wsize, wrap, allface, file):
	vList = []
	faces = []

	minx = 0
	miny = 0
	scale = 1.0
	
	step = 0

	if allface:
		faces = mesh.faces
	else:
		faces = mesh.getSelectedFaces ()

	for f in faces:
		vList.append(f.uv)

	img = Buffer(size+1,size+1)

	if wrap:
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

			step = int(ceil(size*sqrt((co1[0]-co2[0])**2+(co1[1]-co2[1])**2)))
			if step:
				for t in range(step):
					x = int(floor((co1[0] + t*(co2[0]-co1[0])/step) * size))
					y = int(floor((co1[1] + t*(co2[1]-co1[1])/step) * size))

					if wrap:
						x = x % wrapSize
						y = y % wrapSize
					else:
						x = int ((x - minx) * scale)
						y = int ((y - miny) * scale)
						
					co = x * 3 + y * 3 * size;
					
					img[co] = 0
					img[co+1] = 0
					img[co+2] = 0
					if wsize > 1:
						for x in range(-1*wsize + 1,wsize):
							for y in range(-1*wsize,wsize):
								img[co + 3 * x + y * 3 * size] = 0
								img[co + 3 * x + y * 3 * size +1] = 0
								img[co + 3 * x + y * 3 * size +2] = 0
	
		for v in f:
			x = int(v[0] * size)
			y = int(v[1] * size)

			if wrap:
				x = x % wrapSize
				y = y % wrapSize
			else:
				x = int ((x - minx) * scale)
				y = int ((y - miny) * scale)

			co = x * 3 + y * 3 * size
			img[co] = 0
			img[co+1] = 0
			img[co+2] = 255 				
	
	
	write_tgafile(file,img,size,size,3)
	
def SetEditorAndExport(f):
	global bEditPath
	bEditPath.val = f
	
	ExportConfig()
	
	Blender.Window.FileSelector(Export, "Save UV Image")
	
# ###################################### MAIN SCRIPT BODY ###############################

bSize = Blender.Draw.Create(512)
bWSize = Blender.Draw.Create(1)
bObFile = Blender.Draw.Create(1)
bWrap = Blender.Draw.Create(1)
bAllFaces = Blender.Draw.Create(1)
bEdit = Blender.Draw.Create(0)
bEditPath = Blender.Draw.Create("")

ImportConfig()

Block = []

Block.append(("Size: ", bSize, 64, 8192, "Size of the exported image"))
Block.append(("Wire: ", bWSize, 1, 5, "Size of the wire of the faces"))
Block.append(("Wrap", bWrap, "Wrap to image size, scale otherwise"))
Block.append(("All Faces", bAllFaces, "Export all or only selected faces"))
Block.append(("Ob", bObFile, "Use object name in filename"))

if FullPython:
	Block.append(("Edit", bEdit, "Edit resulting file in an external program"))
	Block.append(("Editor: ", bEditPath, 0, 1024, "Path to external editor (leave blank to select a new one)"))

retval = Blender.Draw.PupBlock("UV Image Export", Block)

if retval:
	ExportConfig()
	
	if bEdit.val and not bEditPath.val:
		Blender.Window.FileSelector(SetEditorAndExport, "Select Editor")
	else:
		Blender.Window.FileSelector(Export, "Save UV Image")