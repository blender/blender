#!BPY

"""
Name: 'Save UV Face Layout...'
Blender: 242
Group: 'UV'
Tooltip: 'Export the UV face layout of the selected object to a .TGA or .SVG file'
""" 

__author__ = "Martin 'theeth' Poirier"
__url__ = ("http://www.blender.org", "http://blenderartists.org/")
__version__ = "2.5"

__bpydoc__ = """\
This script exports the UV face layout of the selected mesh object to
a TGA image file or a SVG vector file.  Then you can, for example, paint details
in this image using an external 2d paint program of your choice and bring it back
to be used as a texture for the mesh.

Usage:

Open this script from UV/Image Editor's "UVs" menu, make sure there is a mesh
selected, define size and wire size parameters and push "Export" button.

There are more options to configure, like setting export path, if image should
use object's name and more.

Notes:<br>See change logs in scripts for a list of contributors.
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
#	 Version 2.0
# New interface using PupBlock and FileSelector
# Save/Load config to Registry
# Edit in external program
# --------------------------
#	 Version 2.1 Updates by Jarod from blenderartists.org
# New exportformat SVG
# simple memory optimations, 	two third less memory usage
# tga filewriting speed improvements, 3times faster now
# --------------------------
#	Version 2.2
# Cleanup code
# Filename handling enhancement and bug fixes
# --------------------------
#	Version 2.3
# Added check for excentric UVs (only affects TGA)
# --------------------------
#	Version 2.4
# Port from NMesh to Mesh by Daniel Salazar (zanqdo)
# --------------------------
#	Version 2.5
# Fixed some old off by one rasterizing errors (didn't render points at 1.0 in the UV scale properly).
# Fixed wire drawing for non 1 wire size (didn't wrap or stretch properly 
# and would often raise exceptions)
# --------------------------


FullPython = False

import Blender
import bpy
import BPyMessages

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
	conf["UVFORMATSVG"] = bSVG.val
	conf["SVGFILL"] = bSVGFill.val
	
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
		bSVG.val = conf["UVFORMATSVG"]
		bSVGFill.val = conf["SVGFILL"]
		if editor:
			bEditPath.val = editor
	except KeyError:
		# If one of the key is not in the dict, don't worry, it'll use the defaults
		pass
			
	
def PrintConfig():
	print
	print         "Imagesize: %ipx" % bSize.val
	print         "Wiresize : %ipx" % bWSize.val
	
	if bWrap.val:
		print "Wrap     : yes"
	else:
		print "Wrap     : no"
		
	if bAllFaces.val:
		print "AllFaces : yes"
	else:
		print "AllFaces : no"
		
	if bSVG.val:
		print "Format   : *.svg"
	else:
		print "Format   : *.tga"


def ExportCallback(f):
	obj = Blender.Scene.GetCurrent().objects.active
	
	time1= Blender.sys.time()

	if not obj or obj.type != "Mesh":
		BPyMessages.Error_NoMeshActive()
		return
	
	is_editmode = Blender.Window.EditMode()
	if is_editmode: Blender.Window.EditMode(0)
	
	mesh = obj.getData(mesh=1)
	if not mesh.faceUV:
		if is_editmode: Blender.Window.EditMode(1)
		BPyMessages.Error_NoMeshUvActive()
		return

	# just for information...
	PrintConfig()
	
	# taking care of filename
	if bObFile.val:
		name = AddExtension(f, obj.name)
	else:
		name = AddExtension(f, None)
		
	
	print "Target   :", name
	print
	
	UVFaces = ExtractUVFaces(mesh, bAllFaces.val)
	
	if is_editmode: Blender.Window.EditMode(1)
		
	if not bSVG.val:
		print "TGA export is running..."
		UV_Export_TGA(UVFaces, bSize.val, bWSize.val, bWrap.val, name)
	else:
		print "SVG export is running..."
		if bSVGFill.val:
			SVGFillColor="#F2DAF2"
		else:
			SVGFillColor="none"
		
		UV_Export_SVG(UVFaces, bSize.val, bWSize.val, bWrap.val, name, obj.name, SVGFillColor)
	
	print
	print "     ...finished exporting in %.4f sec." % (Blender.sys.time()-time1)
	
	if FullPython and bEdit.val and bEditPath.val:
		filepath = os.path.realpath(name)
		print filepath
		os.spawnl(os.P_NOWAIT, bEditPath.val, "", filepath)
	

def GetExtension():
	if bSVG.val:
		ext = "svg"
	else:
		ext = "tga"
	
	return ext
	
def AddExtension(filename, object_name):
	ext = "." + GetExtension()
	
	hasExtension = (ext in filename or ext.upper() in filename)
	
	if object_name and hasExtension:
		filename = filename.replace(ext, "")
		hasExtension = False
		
	if object_name:
		filename += "_" + object_name
		
	if not hasExtension:
		filename += ext
		
	return filename
	
def GetDefaultFilename():
	filename = Blender.Get("filename")
	
	filename = filename.replace(".blend", "")
	filename += "." + GetExtension()
	
	return filename

def ExtractUVFaces(mesh, allface):
	
	if allface: return [f.uv for f in mesh.faces]
	else:  return [f.uv for f in mesh.faces if f.sel]

def Buffer(height=16, width=16, profondeur=1,rvb=255 ):  
	"""  
	reserve l'espace memoire necessaire  
	"""  
	p=[rvb]  
	myb=height*width*profondeur
	print"Memory  : %ikB" % (myb/1024)
	b=p*myb
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

	# Origine_en_haut_a_gauche
	print"  ...writing tga..."
	for t in entete0:
	  f.write(t)
	
	redpx=chr(0) + chr(0) + chr(255)
	blackpx=chr(0) + chr(0) + chr(0)
	whitepx=chr(255) + chr(255) + chr(255)
	
	for t in bitmap:
		if t==255:
			f.write(whitepx)
		elif t==0:
			f.write(blackpx)
		else:
			f.write(redpx)
		
	f.close()
	
	
def UV_Export_TGA(vList, size, wsize, wrap, file):
	extreme_warning = False
	
	minx = 0
	miny = 0
	scale = 1.0
	
	step = 0

	img = Buffer(size,size)

	if wrap:
		wrapSize = size
	else:
		wrapSize = size
		maxx = -100000
		maxy = -100000
		for f in vList:
			for v in f:
				x = int(v[0] * size)
				maxx = max (x + wsize - 1, maxx)
				minx = min (x - wsize + 1, minx)
				
				y = int(v[1] * size)
				maxy = max (y + wsize - 1, maxy)
				miny = min (y - wsize + 1, miny)
		wrapSize = max (maxx - minx + 1, maxy - miny + 1)
		scale = float (size) / float (wrapSize)

	max_index = size - 1 # max index of the buffer (height or width)
	fnum = 0
	fcnt = len (vList)

	for f in vList:
		fnum = fnum + 1
		if not fnum % 100:
			print "%i of %i Faces completed" % (fnum, fcnt)
			
		for index in range(len(f)):
			co1 = f[index]
			if index < len(f) - 1:
				co2 = f[index + 1]
			else:
				co2 = f[0]

			step = int(ceil(size*sqrt((co1[0]-co2[0])**2+(co1[1]-co2[1])**2)))
			if step:
				try:
					for t in xrange(step):
							x = int(floor((co1[0] + t*(co2[0]-co1[0])/step) * max_index))
							y = int(floor((co1[1] + t*(co2[1]-co1[1])/step) * max_index))
		
							for dx in range(-1*wsize + 1, wsize):
								if wrap:
									wx = (x + dx) % wrapSize
								else:
									wx = int ((x - minx + dx) * scale)
									
								for dy in range(-1*wsize + 1, wsize):
									if wrap:
										wy = (y + dy) % wrapSize
									else:
										wy = int ((y - miny + dy) * scale)
									
									co = wx * 1 + wy * 1 * size
									img[co] = 0
				except OverflowError:
					if not extreme_warning:
						print "Skipping extremely long UV edges, check your layout for excentric values"
						extreme_warning = True
		
		for v in f:
			x = int(v[0] * max_index)
			y = int(v[1] * max_index)

			if wrap:
				x = x % wrapSize
				y = y % wrapSize
			else:
				x = int ((x - minx) * scale)
				y = int ((y - miny) * scale)

			co = x * 1 + y * 1 * size
			img[co] = 1
	
	
	write_tgafile(file,img,size,size,3)

def UV_Export_SVG(vList, size, wsize, wrap, file, objname, facesfillcolor):
	fl=open(file,'wb')	
	fl.write('<?xml version="1.0"?>\r\n<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\r\n')
	fl.write('<svg width="' + str(size) + 'px" height="' + str(size) + 'px" viewBox="0 0 ' + str(size) + ' ' + str(size) + '" xmlns="http://www.w3.org/2000/svg" version="1.1">\r\n')
	fl.write('<desc>UV-Map from Object: ' + str(objname) +'. Exported from Blender3D with UV Exportscript</desc>\r\n')
	fl.write('<rect x="0" y="0" width="' + str(size) + '" height="' + str(size) + '" fill="none" stroke="blue" stroke-width="' + str(wsize) + 'px" />\r\n')
	fl.write('<g style="fill:' + str(facesfillcolor) + '; stroke:black; stroke-width:' + str(wsize) + 'px;">\r\n')

	fnum = 0
	fcnt = len (vList)
	fnumv = (long) (fcnt/10)
	
	for f in vList:
		fnum = fnum + 1

		if fnum == fnumv:
			print ".",
			fnumv = fnumv + ((long) (fcnt/10))
			
		fl.write('<polygon points="')
		for index in range(len(f)):
			co = f[index]
			fl.write("%.3f,%.3f " % (co[0]*size, size-co[1]*size))
		fl.write('" />\r\n')
		
	print "%i Faces completed." % fnum
	fl.write('</g>\r\n')
	fl.write('</svg>')
	fl.close()
	
def SetEditorAndExportCallback(f):
	global bEditPath
	bEditPath.val = f
	
	ExportConfig()
	
	Export()
	
def Export():
	Blender.Window.FileSelector(ExportCallback, "Save UV (%s)" % GetExtension(), GetDefaultFilename())
	
def SetEditorAndExport():
	Blender.Window.FileSelector(SetEditorAndExportCallback, "Select Editor")
	
# ###################################### MAIN SCRIPT BODY ###############################

# Create user values and fill with defaults
bSize = Blender.Draw.Create(512)
bWSize = Blender.Draw.Create(1)
bObFile = Blender.Draw.Create(1)
bWrap = Blender.Draw.Create(1)
bAllFaces = Blender.Draw.Create(1)
bEdit = Blender.Draw.Create(0)
bEditPath = Blender.Draw.Create("")
bSVG = Blender.Draw.Create(0)
bSVGFill = Blender.Draw.Create(1)

# Import saved configurations
ImportConfig()


Block = []

Block.append(("Size: ", bSize, 64, 16384, "Size of the exported image"))
Block.append(("Wire: ", bWSize, 1, 9, "Size of the wire of the faces"))
Block.append(("Wrap", bWrap, "Wrap to image size, scale otherwise"))
Block.append(("All Faces", bAllFaces, "Export all or only selected faces"))
Block.append(("Object", bObFile, "Use object name in filename"))
Block.append(("SVG", bSVG, "save as *.svg instead of *.tga"))
Block.append(("Fill SVG faces", bSVGFill, "SVG faces will be filled, none filled otherwise"))

if FullPython:
	Block.append(("Edit", bEdit, "Edit resulting file in an external program"))
	Block.append(("Editor: ", bEditPath, 0, 399, "Path to external editor (leave blank to select a new one)"))

retval = Blender.Draw.PupBlock("UV Image Export", Block)

if retval:
	ExportConfig()
		
	if bEdit.val and not bEditPath.val:
		SetEditorAndExport()
	else:
		Export()