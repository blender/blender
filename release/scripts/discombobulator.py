#!BPY

"""
Name: 'Discombobulator'
Blender: 236
Group: 'Mesh'
Tip: 'Adds random geometry to a mesh'
"""

__author__ = "Evan J. Rosky (syrux)"
__url__ = ("Script's homepage, http://evan.nerdsofparadise.com/programs/discombobulator/index.html")
__version__ = "236"
__bpydoc__ = """\
Discombobulator adds random geometry to a mesh.

As an example, this script can easily give a "high-tech"
look to walls and spaceships.

Definitions:<br>
  - Protrusions: extrusions of each original face on the mesh.
You may have from 1 to 4 protrusions on each face.<br>
  - Taper: The tips of each protrusion will be a percentage
smaller than the base.<br>
  - Doodads: small extruded blocks/shapes that are randomly placed
about the top of a protrusion or face.


Usage:<br>
  Input your settings, make sure the mesh you would like to modify
is selected (active) and then click on "Discombobulate".

Notes:<br>
  - Modifications can be restricted to selected faces
by setting "Only selected faces" for protrusions and/or
doodads.<br>
  - It's possible to restrict mesh generation to add only
protrusions or only doodads instead of both.<br>
  - You may also choose to have Discombobulator select the
tops of created protrusions by clicking the corresponding
number of protrusion buttons under "Select Tops". You may 
also do the same for doodads by choosing "Select Doodads" and
"Only Select Tops". You may choose to select the whole doodads 
by leaving "Only Select Tops" off.<br>
  - By selecting "Deselect Selected" you can have
discombobulator deselect everything but the selections it
makes.<br>
  - The "Face %" option will set the percentage of faces that
will be modified either for the doodads or the protrusions.<br>
  - "Copy Before Modifying" will create a new object with the
modifications where leaving it off will overwrite the original
mesh.<br>

You can find more information at the Link above.
"""


# $Id$
#
# --------------------------------------------------------------------------
# Discombobulator v5.3.5.406893.potato
# by Evan J. Rosky, 2005
# This plugin is protected by the GPL: Gnu Public Licence
# GPL - http://www.gnu.org/copyleft/gpl.html
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2005: Evan J. Rosky
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

#Hit Alt-P to run

import Blender
from Blender import NMesh,Object,Material,Window
from Blender.NMesh import Vert,Face
from Blender.Mathutils import *

import defaultdoodads
import BPyMathutils
from BPyMathutils import genrand
a = BPyMathutils.sgenrand(int(round(Rand(1000,99999),0)))

#Create random numbers
def randnum(low,high):
	num = genrand()
	num = num*(high-low)
	num = num+low
	return num


origmesh = NMesh.GetRaw()
newmesh = NMesh.GetRaw()
origobj = Object.Get()
newobj = Object.Get()

#Global Vars
makenewobj = 1
errortext = "Remember to select an object."
editmode = 0

#Protrusion Vars
makeprots = 1
faceschangedpercent = 1.0
minimumheight = 0.2
maximumheight = 0.4
subface1 = 1
subface2 = 1
subface3 = 1
subface4 = 1
subfaceArray = [1,2,3,4]
minsubfaces = 1
minimumtaperpercent = 0.15
maximumtaperpercent = 0.35
useselectedfaces = 0
selectface1 = 1
selectface2 = 1
selectface3 = 1
selectface4 = 1
deselface = 1
#vertselected = 0

#Doodad Vars
makedoodads = 1
doodadfacepercent = 1.0
selectdoodad = 0
onlyonprotrusions = 0
doodonselectedfaces = 0
selectdoodadtoponly = 0
doodad1 = 1
doodad2 = 1
doodad3 = 1
doodad4 = 1
doodad5 = 1
doodad6 = 1
doodadminperface = 2
doodadmaxperface = 6
doodadminsize = 0.15
doodadmaxsize = 0.45
doodadminheight = 0.0
doodadmaxheight = 0.1
doodadArray = [1,2,3,4,5,6]

SEL = NMesh.FaceFlags['SELECT']

def isselectedface(theface):
	for vertic in theface.v:
		if vertic.sel == 0:
			return 0
	return 1

def makeSubfaceArray():
	global subfaceArray
	global subface1
	global subface2
	global subface3
	global subface4
	
	subfaceArray = []
	if subface1 > 0:
		subfaceArray.append(1)
	if subface2 > 0:
		subfaceArray.append(2)
	if subface3 > 0:
		subfaceArray.append(3)
	if subface4 > 0:
		subfaceArray.append(4)

def makeDoodadArray():
	global doodadArray
	global doodad1
	global doodad2
	global doodad3
	global doodad4
	global doodad5
	global doodad6
	
	doodadArray = []
	if doodad1 > 0:
		doodadArray.append(1)
	if doodad2 > 0:
		doodadArray.append(2)
	if doodad3 > 0:
		doodadArray.append(3)
	if doodad4 > 0:
		doodadArray.append(4)
	if doodad5 > 0:
		doodadArray.append(5)
	if doodad6 > 0:
		doodadArray.append(6)

def copyObjStuff(startObj,endObj):
	endObj.setDeltaLocation(startObj.getDeltaLocation())
	endObj.setDrawMode(startObj.getDrawMode())
	endObj.setDrawType(startObj.getDrawType())
	endObj.setEuler(startObj.getEuler())
	if(startObj.getIpo() != None):
		endObj.setIpo(startObj.getIpo())
	endObj.setLocation(startObj.getLocation())
	endObj.setMaterials(startObj.getMaterials())
	endObj.setMatrix(startObj.getMatrix())
	endObj.setSize(startObj.getSize())
	endObj.setTimeOffset(startObj.getTimeOffset())
	

def extrude(mid,nor,protrusion,v1,v2,v3,v4,tosel=1,flipnor=0):
	taper = 1 - randnum(minimumtaperpercent,maximumtaperpercent)
	
	vert = newmesh.verts[v1]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh.verts.append(ver)
	vert = newmesh.verts[v2]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh.verts.append(ver)
	vert = newmesh.verts[v3]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh.verts.append(ver)
	vert = newmesh.verts[v4]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh.verts.append(ver)
	
	faceindex = len(newmesh.verts) - 4
	
	#face 1
	face = Face()
	face.v.append(newmesh.verts[v1])
	face.v.append(newmesh.verts[v2])
	face.v.append(newmesh.verts[faceindex+1])
	face.v.append(newmesh.verts[faceindex])
	if flipnor != 0:
		face.v.reverse()
	newmesh.faces.append(face)
	
	#face 2
	face = Face()
	face.v.append(newmesh.verts[v2])
	face.v.append(newmesh.verts[v3])
	face.v.append(newmesh.verts[faceindex+2])
	face.v.append(newmesh.verts[faceindex+1])
	if flipnor != 0:
		face.v.reverse()
	newmesh.faces.append(face)
	
	#face 3
	face = Face()
	face.v.append(newmesh.verts[v3])
	face.v.append(newmesh.verts[v4])
	face.v.append(newmesh.verts[faceindex+3])
	face.v.append(newmesh.verts[faceindex+2])
	if flipnor != 0:
		face.v.reverse()
	newmesh.faces.append(face)
	
	#face 4
	face = Face()
	face.v.append(newmesh.verts[v4])
	face.v.append(newmesh.verts[v1])
	face.v.append(newmesh.verts[faceindex])
	face.v.append(newmesh.verts[faceindex+3])
	if flipnor != 0:
		face.v.reverse()
	newmesh.faces.append(face)
		
	face = Face()
	face.v = newmesh.verts[-4:]
	if flipnor != 0:
		face.v.reverse()
	if tosel == 1:
		face.sel = 1
	newmesh.faces.append(face)
	return face

def discombobulate(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15,g0,d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16,d17):
	
	#Global
	global origmesh
	global newmesh
	global makenewobj
	global origobj
	global newobj
	global messagetext
	global errortext
	global editmode
	
	#Protrusions
	global makeprots
	global minimumtaperpercent
	global maximumtaperpercent
	global faceschangedpercent
	global minimumheight
	global maximumheight
	global subface1
	global subface2
	global subface3
	global subface4
	global useselectedfaces
	global selectface1
	global selectface2
	global selectface3
	global selectface4
	global deselface
	#global vertselected
	global subfaceArray
	
	#Doodads
	global makedoodads
	global doodadfacepercent
	global selectdoodad
	global onlyonprotrusions
	global doodad1
	global doodad2
	global doodad3
	global doodad4
	global doodad5
	global doodad6
	global doodadminperface
	global doodadmaxperface
	global doodadminsize
	global doodadmaxsize
	global doodadminheight
	global doodadmaxheight
	global doodadArray
	global doodonselectedfaces
	global selectdoodadtoponly
	
	#Global
	try:
		origobj = Object.GetSelected()[0]
	except:
		glRasterPos2d(10,50)
		errortext = "YOU MUST SELECT AN OBJECT!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return

	#Leave Editmode
	editmode = Window.EditMode()
	if editmode: Window.EditMode(0)

	newobj = Object.Get()
	origmesh = origobj.getData()
	newmesh = NMesh.GetRaw()
	newmesh.verts = []
	makenewobj = g0
	
	#Protrusions
	makeprots = p0
	faceschangedpercent = p1
	minimumheight = p2
	maximumheight = p3
	subface1 = p4
	subface2 = p5
	subface3 = p6
	subface4 = p7
	minimumtaperpercent = p8
	maximumtaperpercent = p9
	useselectedfaces = p10
	selectface1 = p11
	selectface2 = p12
	selectface3 = p13
	selectface4 = p14
	deselface = p15
	makeSubfaceArray()
	if len(subfaceArray) == 0:
		makeprots = 0
	
	
	#Doodads
	makedoodads = d0
	doodadfacepercent = d1
	selectdoodad = d2
	onlyonprotrusions = d3
	doodad1 = d4
	doodad2 = d5
	doodad3 = d6
	doodad4 = d7
	doodad5 = d8
	doodad6 = d9
	doodadminperface = d10
	doodadmaxperface = d11
	doodadminsize = d12
	doodadmaxsize = d13
	doodadminheight = d14
	doodadmaxheight = d15
	doodonselectedfaces = d16
	selectdoodadtoponly = d17
	makeDoodadArray()
	if len(doodadArray) == 0:
		makedoodads = 0
	defaultdoodads.topsonly(selectdoodadtoponly)
	
	if minimumheight > maximumheight:
		glColor3f(1.0,0.0,0.0)
		glRasterPos2d(10,50)
		errortext = "MIN HEIGHT MUST BE LESS THAN OR EQUAL TO MAX HEIGHT!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return
	elif minimumtaperpercent > maximumtaperpercent:
		glColor3f(1.0,0.0,0.0)
		glRasterPos2d(10,50)
		errortext = "MIN TAPER MUST BE LESS THAN OR EQUAL TO MAX TAPER!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return
	elif doodadminperface > doodadmaxperface:
		glColor3f(1.0,0.0,0.0)
		glRasterPos2d(10,50)
		errortext = "MIN NUMBER OF DOODADS MUST BE LESS THAN OR EQUAL TO MAX!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return
	elif doodadminsize > doodadmaxsize:
		glColor3f(1.0,0.0,0.0)
		glRasterPos2d(10,50)
		errortext = "MIN DOODAD SIZE MUST BE LESS THAN OR EQUAL TO MAX!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return
	elif doodadminheight > doodadmaxheight:
		glColor3f(1.0,0.0,0.0)
		glRasterPos2d(10,50)
		errortext = "MIN DOODAD HEIGHT MUST BE LESS THAN OR EQUAL TO MAX!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return
	
	newmesh.verts.extend(origmesh.verts)
	
	for currface in origmesh.faces:
		
		#Check if it is a triangle
		if len(currface.v)<4:
			face = Face()
			face.v.extend([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index]])
			newmesh.faces.append(face)
			continue
		
		#Check whether or not to make protrusions
		if makeprots == 0:
			face = Face()
			face.v.extend([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
			newmesh.faces.append(face)
			if makedoodads == 1 and onlyonprotrusions == 0:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray,face, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray,face, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			continue
		
		#Check if only changing selected faces
		if useselectedfaces == 1:
			#check if currface is selected
			if currface.sel:
				a = 1
			else:
				face = Face()
				face.v.extend([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
				newmesh.faces.append(face)
				if makedoodads == 1 and onlyonprotrusions == 0:
					if doodonselectedfaces != 1:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray,face, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				continue
		#Check if face should be modified by random chance
		if randnum(0,1)>faceschangedpercent: 
			face = Face()
			face.v.extend([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
			newmesh.faces.append(face)
			if makedoodads == 1 and onlyonprotrusions == 0:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray,face, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray,face, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			continue
		
		center = Vector([0,0,0])
		for pt in currface.v:
			center = center + pt.co
		center = center/len(currface.v)
		
		#Determine amount of subfaces
		subfaces = round(randnum(1,len(subfaceArray)),0)
		subfaces = subfaceArray[(int(subfaces) - 1)]
		
		if subfaces == 1:
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,currface.v[0].index,currface.v[1].index,currface.v[2].index,currface.v[3].index,selectface1)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
		
		elif subfaces == 2:
			orientation = int(round(randnum(0,1)))
			p1 = currface.v[orientation]
			p2 = currface.v[orientation + 1]
			p3 = (p2.co - p1.co)/2 + p1.co
			ve1 = Vert(p3[0],p3[1],p3[2])
			ve1.sel = 0
			p1 = currface.v[2 + orientation]
			if orientation < 0.5:
				p2 = currface.v[3]
			else:
				p2 = currface.v[0]
			p3 = (p2.co - p1.co)/2 + p1.co
			ve2 = Vert(p3[0],p3[1],p3[2])
			ve2.sel = 0
			if orientation < 0.5:
				verti = currface.v[3]
				p3 = verti.index
				v1 = p3
				verti = currface.v[0]
				p0 = verti.index
				v2 = p0
			else:
				verti = currface.v[0]
				p0 = verti.index
				v1 = p0
				verti = currface.v[1]
				p1 = verti.index
				v2 = p1
			newmesh.verts.append(ve1)
			newmesh.verts.append(ve2)
			index = len(newmesh.verts) - 2
			v4 = index + 1
			v3 = index
			center = Vector([0, 0, 0])
			for pt in [newmesh.verts[v1],newmesh.verts[v2],newmesh.verts[v3],newmesh.verts[v4]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,v1,v2,v3,v4,selectface2)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			if orientation < 0.5:
				verti = currface.v[1]
				p1 = verti.index
				v1 = p1
				verti = currface.v[2]
				p2 = verti.index
				v2 = p2
			else:
				verti = currface.v[2]
				p2 = verti.index
				v1 = p2
				verti = currface.v[3]
				p3 = verti.index
				v2 = p3
			center = Vector([0]*3)
			for pt in [newmesh.verts[v1],newmesh.verts[v2],newmesh.verts[v3],newmesh.verts[v4]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,v1,v2,v4,v3,selectface2)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			if orientation < 0.5:
				face = Face()
				face.v.extend([newmesh.verts[p0],newmesh.verts[p1],newmesh.verts[v3]])
				newmesh.faces.append(face)
				face = Face()
				face.v.extend([newmesh.verts[p2],newmesh.verts[p3],newmesh.verts[v4]])
				newmesh.faces.append(face)
			else:
				face = Face()
				face.v.extend([newmesh.verts[p1],newmesh.verts[p2],newmesh.verts[v3]])
				newmesh.faces.append(face)
				face = Face()
				face.v.extend([newmesh.verts[p3],newmesh.verts[p0],newmesh.verts[v4]])
				newmesh.faces.append(face)
			
		elif subfaces == 3:
			layer2inds = []
			layer2verts = []
			orientation = int(round(randnum(0,1)))
			rotation = int(round(randnum(0,1)))
			p1 = currface.v[orientation]
			p2 = currface.v[orientation + 1]
			p3 = (p2.co - p1.co)/2 + p1.co
			ve1 = Vert(p3[0],p3[1],p3[2])
			ve1.sel = 0
			p1 = currface.v[2 + orientation]
			if orientation < 0.5:
				p2 = currface.v[3]
			else:
				p2 = currface.v[0]
			p3 = (p2.co - p1.co)/2 + p1.co
			ve2 = Vert(p3[0],p3[1],p3[2])
			ve2.sel = 0
			fp = []
	
			#make first protrusion
			if rotation < 0.5:
				if orientation < 0.5:
					verti = currface.v[3]
					fp.append(verti.index)
					v1 = verti.index
					verti = currface.v[0]
					fp.append(verti.index)
					v2 = verti.index
					layer2verts.extend([newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index]])
				else:
					verti = currface.v[0]
					fp.append(verti.index)
					v1 = verti.index
					verti = currface.v[1]
					fp.append(verti.index)
					v2 = verti.index
					layer2verts.extend([newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
				newmesh.verts.append(ve1)
				newmesh.verts.append(ve2)
				index = len(newmesh.verts) - 2
				v4 = index + 1
				v3 = index
				center = Vector([0]*3)
				for pt in [newmesh.verts[v1],newmesh.verts[v2],newmesh.verts[v3],newmesh.verts[v4]]:
					center = center + pt.co
				center = center/4
				prot = randnum(minimumheight,maximumheight)
				layer2inds.extend([v3,v4])
				tempface = extrude(center,currface.no,prot,v1,v2,v3,v4,selectface3)
				if makedoodads == 1:
					if doodonselectedfaces == 1:
						if currface.sel:
							tempmesh = NMesh.GetRaw()
							tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
							newmesh.verts.extend(tempmesh.verts)
							newmesh.faces.extend(tempmesh.faces)
					else:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
			#Still first protrusion
			else:
				if orientation < 0.5:
					verti = currface.v[1]
					fp.append(verti.index)
					v1 = verti.index
					verti = currface.v[2]
					fp.append(verti.index)
					v2 = verti.index
					layer2verts.extend([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[3].index]])
				else:
					verti = currface.v[2]
					fp.append(verti.index)
					v1 = verti.index
					verti = currface.v[3]
					fp.append(verti.index)
					v2 = verti.index
					layer2verts.extend([newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[0].index]])
				newmesh.verts.append(ve2)
				newmesh.verts.append(ve1)
				index = len(newmesh.verts) - 2
				v4 = index
				v3 = index + 1
				center = Vector([0]*3)
				for pt in [newmesh.verts[v1],newmesh.verts[v2],newmesh.verts[v3],newmesh.verts[v4]]:
					center = center + pt.co
				center = center/4
				prot = randnum(minimumheight,maximumheight)
				layer2inds.extend([index, index +1])
				tempface = extrude(center,currface.no,prot,v1,v2,v4,v3,selectface3)
				if makedoodads == 1:
					if doodonselectedfaces == 1:
						if currface.sel:
							tempmesh = NMesh.GetRaw()
							tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
							newmesh.verts.extend(tempmesh.verts)
							newmesh.faces.extend(tempmesh.faces)
					else:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
			
			#split next rect(pre-arranged, no orientation crud)--make flag in extruder for only one existing vert in mesh
			p1 = newmesh.verts[layer2inds[0]]
			p2 = newmesh.verts[layer2inds[1]]
			p3 = (p2.co - p1.co)/2 + p1.co
			ve3 = Vert(p3[0],p3[1],p3[2])
			ve3.sel = 0
			p1 = layer2verts[0]
			p2 = layer2verts[1]
			p3 = (p2.co - p1.co)/2 + p1.co
			ve4 = Vert(p3[0],p3[1],p3[2])
			ve4.sel = 0
			newmesh.verts.append(ve3)
			newmesh.verts.append(ve4)
			tempindex = len(newmesh.verts) - 2
			v5 = tempindex
			v6 = tempindex + 1
			verti = layer2verts[0]
			t0 = verti.index
			center = Vector([0]*3)
			for pt in [newmesh.verts[v5],newmesh.verts[v6],newmesh.verts[t0],newmesh.verts[v3]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			if rotation < 0.5: flino = 1
			else: flino = 0
			tempface = extrude(center,currface.no,prot,v3,v5,v6,t0,selectface3,flino)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			if rotation < 0.5:
				face = Face()
				fpt = t0
				face.v.extend([newmesh.verts[fp[1]],newmesh.verts[fpt],newmesh.verts[v3]])
				newmesh.faces.append(face)
			else:
				face = Face()
				fpt = t0
				face.v.extend([newmesh.verts[fp[0]],newmesh.verts[v3],newmesh.verts[fpt]])
				newmesh.faces.append(face)
			verti = layer2verts[1]
			tempindex = verti.index
			center = Vector([0]*3)
			for pt in [newmesh.verts[v5],newmesh.verts[v6],newmesh.verts[tempindex],newmesh.verts[v4]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,v6,v5,v4,tempindex,selectface3,flino)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			if rotation < 0.5:
				face = Face()
				face.v.extend([newmesh.verts[tempindex],newmesh.verts[fp[0]],newmesh.verts[v4]])
				newmesh.faces.append(face)
				face = Face()
				face.v.extend([newmesh.verts[fpt],newmesh.verts[tempindex],newmesh.verts[v6]])
				newmesh.faces.append(face)
			else:
				face = Face()
				face.v.extend([newmesh.verts[tempindex],newmesh.verts[v4],newmesh.verts[fp[1]]])
				newmesh.faces.append(face)
				face = Face()
				face.v.extend([newmesh.verts[tempindex],newmesh.verts[fpt],newmesh.verts[v6]])
				newmesh.faces.append(face)
			
		else:
			#get all points
			verti = currface.v[0]
			p0 = verti.index
			
			verti = currface.v[1]
			p1 = verti.index
		
			pt = (newmesh.verts[p1].co - newmesh.verts[p0].co)/2 + newmesh.verts[p0].co
			v1 = Vert(pt[0],pt[1],pt[2])
			v1.sel = 0
	
			verti = currface.v[2]
			p2 = verti.index

			pt = (newmesh.verts[p2].co - newmesh.verts[p1].co)/2 + newmesh.verts[p1].co
			v2 = Vert(pt[0],pt[1],pt[2])
			v2.sel = 0

			verti = currface.v[3]
			p3 = verti.index

			pt = (newmesh.verts[p3].co - newmesh.verts[p2].co)/2 + newmesh.verts[p2].co
			v3 = Vert(pt[0],pt[1],pt[2])
			v3.sel = 0

			pt = (newmesh.verts[p0].co - newmesh.verts[p3].co)/2 + newmesh.verts[p3].co
			v4 = Vert(pt[0],pt[1],pt[2])
			v4.sel = 0

			pt = (v3.co - v1.co)/2 + v1.co
			m = Vert(pt[0],pt[1],pt[2])
			m.sel = 0
			
			#extrusion 1
			newmesh.verts.extend([v1,m,v4])
			index = len(newmesh.verts) - 3
			v1 = index
			m = index + 1
			v4 = index + 2
			center = Vector([0]*3)
			for pt in [newmesh.verts[p0],newmesh.verts[v1],newmesh.verts[m],newmesh.verts[v4]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,p0,v1,m,v4,selectface4)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			
			#extrusion 2
			newmesh.verts.extend([v2])
			index = len(newmesh.verts) - 1
			v2 = index
			center = Vector([0]*3)
			for pt in [newmesh.verts[m],newmesh.verts[v1],newmesh.verts[p1],newmesh.verts[v2]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,m,v1,p1,v2,selectface4)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			
			#extrusion 3
			newmesh.verts.extend([v3])
			index = len(newmesh.verts) - 1
			v3 = index
			center = Vector([0]*3)
			for pt in [newmesh.verts[m],newmesh.verts[v2],newmesh.verts[p2],newmesh.verts[v3]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,m,v2,p2,v3,selectface4)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			
			#extrusion 4
			center = Vector([0]*3)
			for pt in [newmesh.verts[m],newmesh.verts[v3],newmesh.verts[p3],newmesh.verts[v4]]:
				center = center + pt.co
			center = center/4
			prot = randnum(minimumheight,maximumheight)
			tempface = extrude(center,currface.no,prot,v4,m,v3,p3,selectface4)
			if makedoodads == 1:
				if doodonselectedfaces == 1:
					if currface.sel:
						tempmesh = NMesh.GetRaw()
						tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
						newmesh.verts.extend(tempmesh.verts)
						newmesh.faces.extend(tempmesh.faces)
				else:
					tempmesh = NMesh.GetRaw()
					tempmesh = defaultdoodads.createDoodad(doodadArray, tempface, doodadminsize, doodadmaxsize, doodadminheight,doodadmaxheight, selectdoodad, doodadminperface, doodadmaxperface, doodadfacepercent)
					newmesh.verts.extend(tempmesh.verts)
					newmesh.faces.extend(tempmesh.faces)
			
			face = Face()
			face.v.extend([newmesh.verts[p0],newmesh.verts[p1],newmesh.verts[v1]])
			newmesh.faces.append(face)
			face = Face()
			face.v.extend([newmesh.verts[p1],newmesh.verts[p2],newmesh.verts[v2]])
			newmesh.faces.append(face)
			face = Face()
			face.v.extend([newmesh.verts[p2],newmesh.verts[p3],newmesh.verts[v3]])
			newmesh.faces.append(face)
			face = Face()
			face.v.extend([newmesh.verts[p3],newmesh.verts[p0],newmesh.verts[v4]])
			newmesh.faces.append(face)
			
	#NMesh.PutRaw(newmesh)
	if deselface == 1:
		for unvert in origmesh.verts:
			newmesh.verts[unvert.index].sel = 0
	if makenewobj == 1:
		newobj = Object.New('Mesh')
		copyObjStuff(origobj,newobj)
		newobj.link(newmesh)
		scene = Blender.Scene.getCurrent()
		scene.link(newobj)
		origobj.select(0)
		newobj.select(1)
	else:
		origobj.link(newmesh)
	
	#Return to Editmode if previously in it
	if editmode: Window.EditMode(1)


####################### gui ######################
from Blender.BGL import *
from Blender.Draw import *

def ErrorText(errortext):
	Window.WaitCursor(0)
	Text(errortext)
	PupMenu("ERROR: %s" % errortext.lower())

#Global Buttons
makenewobject = Create(makenewobj)
messagetext = Create(errortext)

#Protrusion Buttons
doprots = Create(makeprots)
facechange = Create(faceschangedpercent*100)
minheight = Create(minimumheight)
maxheight = Create(maximumheight)
sub1 = Create(subface1)
sub2 = Create(subface2)
sub3 = Create(subface3)
sub4 = Create(subface4)
mintaper = Create(minimumtaperpercent*100)
maxtaper = Create(maximumtaperpercent*100)
useselected = Create(useselectedfaces)
selface1 = Create(selectface1)
selface2 = Create(selectface2)
selface3 = Create(selectface3)
selface4 = Create(selectface4)
deselectvertices = Create(deselface)
#selectbyverts = Create(vertselected)

#Doodad Buttons
dodoodads = Create(makedoodads)
doodadfacechange = Create(doodadfacepercent*100)
seldoodad = Create(selectdoodad)
onprot = Create(onlyonprotrusions)
dood1 = Create(doodad1)
dood2 = Create(doodad2)
dood3 = Create(doodad3)
dood4 = Create(doodad4)
dood5 = Create(doodad5)
dood6 = Create(doodad6)
doodadminamount = Create(doodadminperface)
doodadmaxamount = Create(doodadmaxperface)
doodsizemin = Create(doodadminsize*100)
doodsizemax = Create(doodadmaxsize*100)
doodheightmin = Create(doodadminheight)
doodheightmax = Create(doodadmaxheight)
doodonselface = Create(doodonselectedfaces)
seldoodtop = Create(selectdoodadtoponly)


# Events
EVENT_NONE = 1
EVENT_DISCOMBOBULATE = 2
EVENT_EXIT = 3

def colorbox(x,y,xright,bottom):
   glColor3f(0.75, 0.75, 0.75)
   glRecti(x + 1, y + 1, xright - 1, bottom - 1)

def draw():
	
	#Protrusions
	global doprots
	global facechange
	global minheight
	global maxheight
	global sub1
	global sub2
	global sub3
	global sub4
	global mintaper
	global maxtaper
	global useselected
	global selface1
	global selface2
	global selface3
	global selface4
	global deselectvertices
	#global selectbyverts
	
	#Doodads
	global dodoodads
	global doodadfacechange
	global seldoodad
	global onprot
	global dood1
	global dood2
	global dood3
	global dood4
	global dood5
	global dood6
	global doodadminamount
	global doodadmaxamount
	global doodsizemin
	global doodsizemax
	global doodheightmin
	global doodheightmax
	global doodonselface
	global seldoodtop
	
	#Global Settings
	global makenewobject
	global messagetext
	global errortext
	global EVENT_NONE,EVENT_DRAW,EVENT_EXIT
	
	#Height Addition, this is for changing the gui
	hadd = 10
	
	#Title :420high
	glClearColor(0.6, 0.6, 0.6, 1.0)
	glClear(GL_COLOR_BUFFER_BIT)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(8, 400+hadd)
	Text("Discombobulator v5.3.5.406893.potato")
	
	#Protrusion
	colorbox(8,385+hadd,312,230+hadd)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12, 375+hadd)
	Text("Protrusions:")
	doprots = Toggle("Make Protrusions",EVENT_NONE,12,352+hadd,145,18,doprots.val,"Make Protrusions?")
	facechange = Number("Face %: ",EVENT_NONE,162,352+hadd,145,18,facechange.val,0,100,"Percentage of faces that will grow protrusions")
	useselected = Toggle("Only selected faces",EVENT_NONE,12,332+hadd,145,18,useselected.val,"If on, only selected faces will be modified")
	deselectvertices = Toggle("Deselect Selected",EVENT_NONE,162,332+hadd,145,18,deselectvertices.val,"Deselects any selected vertex except for ones selected by \"Select Tops\"")
	
	#Protrusion properties
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12, 315+hadd)
	Text("Protrusion Properties:")
	minheight = Number("Min Height: ",EVENT_NONE,12,292+hadd,145,18,minheight.val,-100.0,100.0,"Minimum height of any protrusion")
	maxheight = Number("Max Height: ",EVENT_NONE,162,292+hadd,145,18,maxheight.val,-100.0,100.0,"Maximum height of any protrusion")
	mintaper = Number("Min Taper %: ",EVENT_NONE,12,272+hadd,145,18,mintaper.val,0,100,"Minimum taper percentage of protrusion")
	maxtaper = Number("Max Taper %: ",EVENT_NONE,162,272+hadd,145,18,maxtaper.val,0,100,"Maximum taper percentage of protrusion")
	glRasterPos2d(19, 257+hadd)
	Text("Number of protrusions:")
	sub1 = Toggle("1",EVENT_NONE,12,235+hadd,34,18,sub1.val,"One Protrusion")
	sub2 = Toggle("2",EVENT_NONE,48,235+hadd,34,18,sub2.val,"Two Protrusions")
	sub3 = Toggle("3",EVENT_NONE,84,235+hadd,34,18,sub3.val,"Three Protrusions")
	sub4 = Toggle("4",EVENT_NONE,120,235+hadd,34,18,sub4.val,"Four Protrusions")
	glRasterPos2d(195, 257+hadd)
	Text("Select tops of:")
	selface1 = Toggle("1",EVENT_NONE,165,235+hadd,34,18,selface1.val,"Select the tip of the protrusion when it is created")
	selface2 = Toggle("2",EVENT_NONE,201,235+hadd,34,18,selface2.val,"Select the tips of each protrusion when they are created")
	selface3 = Toggle("3",EVENT_NONE,237,235+hadd,34,18,selface3.val,"Select the tips of each protrusion when they are created")
	selface4 = Toggle("4",EVENT_NONE,273,235+hadd,34,18,selface4.val,"Select the tips of each protrusion when they are created")
	
	#Doodad
	colorbox(8,220+hadd,312,40+hadd)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12, 210+hadd)
	Text("Doodads:")
	dood1 = Toggle("1 Box",EVENT_NONE,12,207+hadd-20,45,18,dood1.val,"Creates a rectangular box")
	dood2 = Toggle("2 Box",EVENT_NONE,61,207+hadd-20,45,18,dood2.val,"Creates 2 side-by-side rectangular boxes")
	dood3 = Toggle("3 Box",EVENT_NONE,110,207+hadd-20,45,18,dood3.val,"Creates 3 side-by-side rectangular boxes")
	dood4 = Toggle("\"L\"",EVENT_NONE,164,207+hadd-20,45,18,dood4.val,"Creates a Tetris-style \"L\" shape")
	dood5 = Toggle("\"T\"",EVENT_NONE,213,207+hadd-20,45,18,dood5.val,"Creates a Tetris-style \"T\" shape")
	dood6 = Toggle("\"S\"",EVENT_NONE,262,207+hadd-20,45,18,dood6.val,"Creates a sort-of \"S\" or \"Z\" shape")
	dodoodads = Toggle("Make Doodads",EVENT_NONE,12,165+hadd,145,18,dodoodads.val,"Make Doodads?")
	doodadfacechange = Number("Face %: ",EVENT_NONE,162,165+hadd,145,18,doodadfacechange.val,0,100,"Percentage of faces that will gain doodads")
	seldoodad = Toggle("Select Doodads",EVENT_NONE,12,145+hadd,145,18,seldoodad.val,"Selects doodads when they are created")
	seldoodtop = Toggle("Only Select Tops",EVENT_NONE,162,145+hadd,145,18,seldoodtop.val,"Only Selects tops of doodads when\"Select Doodads\" is on")
	doodonselface = Toggle("Only selected faces",EVENT_NONE,12,125+hadd,145,18,doodonselface.val,"Only create doodads on selected faces")
	onprot = Toggle("Only on Protrusions",EVENT_NONE,162,125+hadd,145,18,onprot.val,"Only place doodads on protrusions")
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12, 108+hadd)
	Text("Doodad Properties:")
	doodadminamount = Number("Min Amount: ",EVENT_NONE,12,85+hadd,145,18,doodadminamount.val,0,100,"Minimum number of doodads per face")
	doodadmaxamount = Number("Max Amount: ",EVENT_NONE,162,85+hadd,145,18,doodadmaxamount.val,0,100,"Maximum number of doodads per face")
	doodheightmin = Number("Min Height: ",EVENT_NONE,12,65+hadd,145,18,doodheightmin.val,0.0,100.0,"Minimum height of any doodad")
	doodheightmax = Number("Max Height: ",EVENT_NONE,162,65+hadd,145,18,doodheightmax.val,0.0,100.0,"Maximum height of any doodad")
	doodsizemin = Number("Min Size %: ",EVENT_NONE,12,45+hadd,145,18,doodsizemin.val,0.0,100.0,"Minimum size of any doodad in percentage of face")
	doodsizemax = Number("Max Size %: ",EVENT_NONE,162,45+hadd,145,18,doodsizemax.val,0.0,100.0,"Maximum size of any doodad in percentage of face")
	
	#Global Parts
	glColor3f(1.0,0.0,0.0)
	glRasterPos2d(10,35)
	messagetext = Text(errortext)
	glColor3f(0.0,0.0,0.0)
	makenewobject = Toggle("Copy Before Modifying",EVENT_NONE,162,10,145,18,makenewobject.val,"If selected, the original object will be copied before it is changed")
	Button("Discombobulate",EVENT_DISCOMBOBULATE,12,10,100,18)
	Button("Exit",EVENT_EXIT,120,10,30,18)

def event(evt, val):	
	if (evt == QKEY and not val): 
		Exit()

def bevent(evt):
	
	#Protrusions
	global doprots
	global facechange
	global minheight
	global maxheight
	global sub1
	global sub2
	global sub3
	global sub4
	global mintaper
	global maxtaper
	global useselected
	global selface1
	global selface2
	global selface3
	global selface4
	global deselectvertices
	#global selectbyverts
	
	#Doodads
	global dodoodads
	global doodadfacechange
	global seldoodad
	global onprot
	global dood1
	global dood2
	global dood3
	global dood4
	global dood5
	global dood6
	global doodadminamount
	global doodadmaxamount
	global doodsizemin
	global doodsizemax
	global doodheightmin
	global doodheightmax
	global doodonselface
	global seldoodtop
	
	#Global Settings
	global makenewobject
	global messagetext
	global errortext
	global EVENT_NONE,EVENT_DRAW,EVENT_EXIT

	######### Manages GUI events
	if evt==EVENT_EXIT: 
		Exit()
	elif evt==EVENT_DISCOMBOBULATE:
		Window.WaitCursor(1)
		discombobulate(doprots.val,facechange.val/100,minheight.val,maxheight.val,sub1.val,sub2.val,sub3.val,sub4.val,mintaper.val/100,maxtaper.val/100,useselected.val,selface1.val,selface2.val,selface3.val,selface4.val,deselectvertices.val,makenewobject.val,dodoodads.val,doodadfacechange.val/100,seldoodad.val,onprot.val,dood1.val,dood2.val,dood3.val,dood4.val,dood5.val,dood6.val,doodadminamount.val,doodadmaxamount.val,doodsizemin.val/100,doodsizemax.val/100,doodheightmin.val,doodheightmax.val,doodonselface.val,seldoodtop.val)
		Window.WaitCursor(0)
		Blender.Redraw()

Register(draw, event, bevent)
