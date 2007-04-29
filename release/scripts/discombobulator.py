#!BPY

"""
Name: 'Discombobulator'
Blender: 237
Group: 'Mesh'
Tip: 'Adds random geometry to a mesh'
"""

__author__ = "Evan J. Rosky (syrux)"
__url__ = ("Script's homepage, http://evan.nerdsofparadise.com/programs/discombobulator/index.html")
__version__ = "237"
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
is selected (active) and then click on "Discombobulate".<br>
  See the scripts tutorial page (on the homepage) for more info.


New Features:<br>
  - Will use existing materials if there are any.<br>
  - Clicking "Assign materials by part" will allow assigning
of different material indices to Protrusion or Doodad Sides
and Tops in the gui element below it.<br>
  - Setting a material index to 0 will use whatever material
is assigned to the face that is discombobulated.
  - You can now scroll using the arrow keys.


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
# Updated 2006-09-26
# Changes since last version: 
#     > Works with Blender CVS and hopefully with Blender 2.40.
#     > Swaps min/max values when min>max rather than complaining.
#     > Will keep previously assigned materials.
#     > Now allows user to assign custom material indices to
#            Protrusion and Doodad Sides and Tops.
#     > The initial Gui Layout will change depending on the aspect
#            ratio of the window it is in.
#     > Using the arrow keys will scroll the gui.
# 
# --------------------------------------------------------------------------
# Discombobulator v2.1b
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
from Blender import NMesh,Object,Material,Window,Types,Scene
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

#Object Vars
newmesh = NMesh.GetRaw()
materialArray = [0]

#Material Vars
reassignMats = 0
protSideMat = 1
protTopMat = 2
doodSideMat = 3
doodTopMat = 4
thereAreMats = 0
currmat = 0

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

def extrude(mid,nor,protrusion,v1,v2,v3,v4,tosel=1,flipnor=0):
	taper = 1 - randnum(minimumtaperpercent,maximumtaperpercent)
	newmesh_verts = newmesh.verts
	newmesh_faces = newmesh.faces
	
	vert = newmesh_verts[v1]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh_verts.append(ver)
	vert = newmesh_verts[v2]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh_verts.append(ver)
	vert = newmesh_verts[v3]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh_verts.append(ver)
	vert = newmesh_verts[v4]
	point = (vert.co - mid)*taper + mid + protrusion*Vector(nor)
	ver = Vert(point[0],point[1],point[2])
	ver.sel = tosel
	newmesh_verts.append(ver)
	
	faceindex = len(newmesh_verts) - 4
	
	#side face 1
	face = Face([newmesh_verts[v1], newmesh_verts[v2], newmesh_verts[faceindex+1], newmesh_verts[faceindex]])
	if flipnor != 0:
		face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or protSideMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = protSideMat-1
	newmesh_faces.append(face)
	
	#side face 2
	face = Face([newmesh_verts[v2], newmesh_verts[v3], newmesh_verts[faceindex+2], newmesh_verts[faceindex+1]])
	if flipnor != 0:
		face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or protSideMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = protSideMat-1
	newmesh_faces.append(face)
	
	#side face 3
	face = Face([newmesh_verts[v3], newmesh_verts[v4], newmesh_verts[faceindex+3], newmesh_verts[faceindex+2]])
	if flipnor != 0:
		face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or protSideMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = protSideMat-1
	newmesh_faces.append(face)
	
	#side face 4
	face = Face([newmesh_verts[v4], newmesh_verts[v1], newmesh_verts[faceindex], newmesh_verts[faceindex+3]])
	if flipnor != 0:
		face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or protSideMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = protSideMat-1
	newmesh_faces.append(face)
	
	#top face	
	face = Face(newmesh_verts[-4:])
	if flipnor != 0:
		face.v.reverse()
	if tosel == 1:
		face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or protTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = protTopMat-1
	newmesh_faces.append(face)
	return face

#Sets the global protrusion values
def setProtrusionValues(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10,p11,p12,p13,p14,p15):
	
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
	global subfaceArray
	
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
	
	if minimumheight > maximumheight:
		a = maximumheight
		maximimheight = minimumheight
		minimumheight = a
	elif minimumtaperpercent > maximumtaperpercent:
		a = maximumtaperpercent
		maximimtaperpercent = minimumtaperpercent
		minimumtaperpercent = a

#Sets the global Doodad values
def setDoodadValues(d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16,d17):
	
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
	
	elif doodadminperface > doodadmaxperface:
		a = doodadmaxperface
		doodadmaxperface = doodadminperface
		doodadminperface = a
	elif doodadminsize > doodadmaxsize:
		a = doodadmaxsize
		doodadmaxsize = doodadminsize
		doodadminsize = a
	elif doodadminheight > doodadmaxheight:
		a = doodadmaxheight
		doodadmaxheight = doodadminheight
		doodadminheight = a

#Sets other global values
def setOtherValues(g0,m0,m1,m2,m3,m4):
	
	#Global
	global reassignMats
	global makenewobj
	global protSideMat
	global protTopMat
	global doodSideMat
	global doodTopMat
	
	#Get Misc Variables
	makenewobj = g0
	reassignMats = m0
	protSideMat = m1
	protTopMat = m2
	doodSideMat = m3
	doodTopMat = m4

def discombobulate():
	
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
	global materialArray
	global reassignMats
	global protSideMat
	global protTopMat
	global doodSideMat
	global doodTopMat
	global thereAreMats
	global currmat
	
	origobj = Scene.GetCurrent().objects.active
	if not origobj:
		glRasterPos2d(10,50)
		errortext = "YOU MUST SELECT AN OBJECT!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return

	#Leave Editmode
	editmode = Window.EditMode()
	if editmode: Window.EditMode(0)

	#Get Major Variables
	
	origmesh = origobj.getData()
	
	if origobj.type != 'Mesh':
		glRasterPos2d(10,50)
		errortext = "OBJECT MUST BE MESH!"
		messagetext = ErrorText(errortext)
		Blender.Redraw()
		return
	
	newmesh = NMesh.GetRaw()
	materialArray = origmesh.getMaterials()
	if len(materialArray) < 1:
		thereAreMats = 0
	else:
		thereAreMats = 1
	
	#add material indices if necessary (only up to 4)
	if thereAreMats == 1 and reassignMats == 1:
		if len(materialArray) < 4:
			if protSideMat > 4: protSideMat = 4
			if protTopMat > 4: protTopMat = 4
			if doodSideMat > 4: doodSideMat = 4
			if doodTopMat > 4: doodTopMat = 4
		else:
			if protSideMat > len(materialArray): protSideMat = len(materialArray)
			if protTopMat > len(materialArray): protTopMat = len(materialArray)
			if doodSideMat > len(materialArray): doodSideMat = len(materialArray)
			if doodTopMat > len(materialArray): doodTopMat = len(materialArray)
		
		#This only does something if there are less than 4 verts
		for matind in [protSideMat,protTopMat,doodSideMat,doodTopMat]:
			if matind > len(materialArray) and matind <= 4:
				for i in xrange(len(materialArray),matind+1):
					materialArray.append(Material.New("AddedMat " + str(i)))
					
	#Sets the materials
	newmesh.setMaterials(materialArray)
	
	#Set the doodad settings
	defaultdoodads.settings(selectdoodadtoponly,materialArray,reassignMats,thereAreMats,doodSideMat,doodTopMat)
	#defaultdoodads.settings(selectdoodadtoponly,materialArray,reassignMats,thereAreMats,currmat)
	
	newmesh.verts.extend(origmesh.verts)
	
	#Start modifying faces
	for currface in origmesh.faces:
		
		currmat = currface.materialIndex
		defaultdoodads.setCurrMat(currmat)
		
		#Check if it is a triangle
		if len(currface.v)<4:
			face = Face([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index]])
			if thereAreMats == 1:
				face.materialIndex = currmat
			newmesh.faces.append(face)
			continue
		
		#Check whether or not to make protrusions
		if makeprots == 0:
			face = Face([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
			if thereAreMats == 1:
				face.materialIndex = currmat
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
				face = Face([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
				if thereAreMats == 1:
					face.materialIndex = currmat
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
			face = Face([newmesh.verts[currface.v[0].index],newmesh.verts[currface.v[1].index],newmesh.verts[currface.v[2].index],newmesh.verts[currface.v[3].index]])
			if thereAreMats == 1:
				face.materialIndex = currmat
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
		center = center / len(currface.v)
		
		#Determine amount of subfaces
		subfaces = round(randnum(1,len(subfaceArray)),0)
		subfaces = subfaceArray[(int(subfaces) - 1)]
		
		######################## START DEALING WITH PROTRUSIONS #####################
		
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
			p3 = ((p2.co - p1.co)/2) + p1.co
			ve1 = Vert(p3[0],p3[1],p3[2])
			ve1.sel = 0
			p1 = currface.v[2 + orientation]
			if orientation < 0.5:
				p2 = currface.v[3]
			else:
				p2 = currface.v[0]
			p3 = ((p2.co - p1.co)/2) + p1.co
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
				center += pt.co
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
				center += pt.co
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
				face = Face([newmesh.verts[p0],newmesh.verts[p1],newmesh.verts[v3]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
				face = Face([newmesh.verts[p2],newmesh.verts[p3],newmesh.verts[v4]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
			else:
				face = Face([newmesh.verts[p1],newmesh.verts[p2],newmesh.verts[v3]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
				face = Face([newmesh.verts[p3],newmesh.verts[p0],newmesh.verts[v4]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
			
		elif subfaces == 3:
			layer2inds = []
			layer2verts = []
			orientation = int(round(randnum(0,1)))
			rotation = int(round(randnum(0,1)))
			p1 = currface.v[orientation]
			p2 = currface.v[orientation + 1]
			p3 = ((p2.co - p1.co)/2) + p1.co
			ve1 = Vert(p3[0],p3[1],p3[2])
			ve1.sel = 0
			p1 = currface.v[2 + orientation]
			if orientation < 0.5:
				p2 = currface.v[3]
			else:
				p2 = currface.v[0]
			p3 = ((p2.co - p1.co)/2) + p1.co
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
					center += pt.co
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
					center += pt.co
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
			p3 = ((p2.co - p1.co)/2) + p1.co
			ve3 = Vert(p3[0],p3[1],p3[2])
			ve3.sel = 0
			p1 = layer2verts[0]
			p2 = layer2verts[1]
			p3 = ((p2.co - p1.co)/2) + p1.co
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
				center += pt.co
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
				fpt = t0
				face = Face([newmesh.verts[fp[1]],newmesh.verts[fpt],newmesh.verts[v3]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
			else:
				fpt = t0
				face = Face([newmesh.verts[fp[0]],newmesh.verts[v3],newmesh.verts[fpt]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
			verti = layer2verts[1]
			tempindex = verti.index
			center = Vector([0]*3)
			for pt in [newmesh.verts[v5],newmesh.verts[v6],newmesh.verts[tempindex],newmesh.verts[v4]]:
				center += pt.co
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
				face = Face([newmesh.verts[tempindex],newmesh.verts[fp[0]],newmesh.verts[v4]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
				face = Face([newmesh.verts[fpt],newmesh.verts[tempindex],newmesh.verts[v6]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
			else:
				face = Face([newmesh.verts[tempindex],newmesh.verts[v4],newmesh.verts[fp[1]]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
				face = Face([newmesh.verts[tempindex],newmesh.verts[fpt],newmesh.verts[v6]])
				if thereAreMats == 1:
					if reassignMats == 0 or protSideMat == 0:
						face.materialIndex = currmat
					else:
						face.materialIndex = protSideMat-1
				newmesh.faces.append(face)
			
		else:
			#get all points
			verti = currface.v[0]
			p0 = verti.index
			
			verti = currface.v[1]
			p1 = verti.index
		
			pt = ((newmesh.verts[p1].co - newmesh.verts[p0].co)/2) + newmesh.verts[p0].co
			v1 = Vert(pt[0],pt[1],pt[2])
			v1.sel = 0
	
			verti = currface.v[2]
			p2 = verti.index

			pt =  ((newmesh.verts[p2].co - newmesh.verts[p1].co)/2) + newmesh.verts[p1].co
			v2 = Vert(pt[0],pt[1],pt[2])
			v2.sel = 0

			verti = currface.v[3]
			p3 = verti.index

			pt =  ((newmesh.verts[p3].co - newmesh.verts[p2].co)/2) + newmesh.verts[p2].co
			v3 = Vert(pt[0],pt[1],pt[2])
			v3.sel = 0

			pt =  ((newmesh.verts[p0].co - newmesh.verts[p3].co)/2) + newmesh.verts[p3].co
			v4 = Vert(pt[0],pt[1],pt[2])
			v4.sel = 0

			pt =  ((v3.co - v1.co)/2) + v1.co
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
				center += pt.co
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
				center += pt.co
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
				center += pt.co
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
				center += pt.co
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
			
			face = Face([newmesh.verts[p0],newmesh.verts[p1],newmesh.verts[v1]])
			if thereAreMats == 1:
				if reassignMats == 0 or protSideMat == 0:
					face.materialIndex = currmat
				else:
					face.materialIndex = protSideMat-1
			newmesh.faces.append(face)
			face = Face([newmesh.verts[p1],newmesh.verts[p2],newmesh.verts[v2]])
			if thereAreMats == 1:
				if reassignMats == 0 or protSideMat == 0:
					face.materialIndex = currmat
				else:
					face.materialIndex = protSideMat-1
			newmesh.faces.append(face)
			face = Face([newmesh.verts[p2],newmesh.verts[p3],newmesh.verts[v3]])
			if thereAreMats == 1:
				if reassignMats == 0 or protSideMat == 0:
					face.materialIndex = currmat
				else:
					face.materialIndex = protSideMat-1
			newmesh.faces.append(face)
			face = Face([newmesh.verts[p3],newmesh.verts[p0],newmesh.verts[v4]])
			if thereAreMats == 1:
				if reassignMats == 0 or protSideMat == 0:
					face.materialIndex = currmat
				else:
					face.materialIndex = protSideMat-1
			newmesh.faces.append(face)
			
	#NMesh.PutRaw(newmesh)
	if deselface == 1:
		for unvert in origmesh.verts:
			newmesh.verts[unvert.index].sel = 0
	if makenewobj == 1:
		newobj = origobj.__copy__()
		newobj.link(newmesh)
		scene = Blender.Scene.GetCurrent()
		scene.objects.link(newobj)
		origobj.sel = 0
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

#Material Buttons
assignNewMats = Create(reassignMats)
replProtSideIndex = Create(protSideMat)
replProtTopIndex = Create(protTopMat)
replDoodSideIndex = Create(doodSideMat)
replDoodTopIndex = Create(doodTopMat)

# Events
EVENT_NONE = 1
EVENT_DISCOMBOBULATE = 2
EVENT_EXIT = 3

# Additions for moving gui
hadd = 0
wadd = 0
thadd = 410
phadd = 245
pwadd = 0
dhadd = 55
dwadd = 0
ghadd = 10
gwadd = 0
mhadd = 55
mwadd = 312

def colorbox(x,y,xright,bottom):
   glColor3f(0.75, 0.75, 0.75)
   glRecti(x + 1, y + 1, xright - 1, bottom - 1)

firstDraw = 1

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
	
	#Materials
	global assignNewMats
	global replProtSideIndex
	global replProtTopIndex
	global replDoodSideIndex
	global replDoodTopIndex
	
	#Global Settings
	global makenewobject
	global messagetext
	global errortext
	global EVENT_NONE,EVENT_DRAW,EVENT_EXIT,EVENT_UP,EVENT_DOWN,EVENT_LEFT,EVENT_RIGHT
	
	# Additions for moving gui
	global hadd
	global wadd
	global thadd
	global phadd
	global pwadd
	global dhadd
	global dwadd
	global ghadd
	global gwadd
	global mhadd
	global mwadd
	
	#This is for creating the initial layout
	global firstDraw
	if(firstDraw == 1):
		if(((Window.GetAreaSize()[1])*1.7) < Window.GetAreaSize()[0]):
			thadd = 180
			phadd = 10
			dhadd = 10
			mhadd = 55
			ghadd = 10
			pwadd = 0
			dwadd = 305
			mwadd = 610
			gwadd = 610
		else:
			thadd = 505
			phadd = 346
			dhadd = 160
			mhadd = 56
			ghadd = 10
			pwadd = 0
			dwadd = 0
			mwadd = 0
			gwadd = 0
		firstDraw = 0
	
	
	#Title :420high
	glClearColor(0.6, 0.6, 0.6, 1.0)
	glClear(GL_COLOR_BUFFER_BIT)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(8+wadd, thadd+hadd)
	Text("Discombobulator v2.1b")
	
	#Protrusion
	colorbox(8+pwadd+wadd,150+phadd+hadd,312+pwadd+wadd,phadd-5+hadd)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12+pwadd+wadd, 140+phadd+hadd)
	Text("Protrusions:")
	doprots = Toggle("Make Protrusions",EVENT_NONE,12+pwadd+wadd,117+phadd+hadd,145,18,doprots.val,"Make Protrusions?")
	facechange = Number("Face %: ",EVENT_NONE,162+pwadd+wadd,117+phadd+hadd,145,18,facechange.val,0,100,"Percentage of faces that will grow protrusions")
	useselected = Toggle("Only selected faces",EVENT_NONE,12+pwadd+wadd,97+phadd+hadd,145,18,useselected.val,"If on, only selected faces will be modified")
	deselectvertices = Toggle("Deselect Selected",EVENT_NONE,162+pwadd+wadd,97+phadd+hadd,145,18,deselectvertices.val,"Deselects any selected vertex except for ones selected by \"Select Tops\"")
	
	#Protrusion properties
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12+pwadd+wadd, 80+phadd+hadd)
	Text("Protrusion Properties:")
	BeginAlign()
	minheight = Number("Min Height: ",EVENT_NONE,12+pwadd+wadd,57+phadd+hadd,145,18,minheight.val,-100.0,100.0,"Minimum height of any protrusion")
	maxheight = Number("Max Height: ",EVENT_NONE,162+pwadd+wadd,57+phadd+hadd,145,18,maxheight.val,-100.0,100.0,"Maximum height of any protrusion")
	EndAlign()
	BeginAlign()
	mintaper = Number("Min Taper %: ",EVENT_NONE,12+pwadd+wadd,37+phadd+hadd,145,18,mintaper.val,0,100,"Minimum taper percentage of protrusion")
	maxtaper = Number("Max Taper %: ",EVENT_NONE,162+pwadd+wadd,37+phadd+hadd,145,18,maxtaper.val,0,100,"Maximum taper percentage of protrusion")
	EndAlign()
	glRasterPos2d(19+pwadd+wadd, 22+phadd+hadd)
	Text("Number of protrusions:")
	BeginAlign()
	sub1 = Toggle("1",EVENT_NONE,12+pwadd+wadd,phadd+hadd,34,18,sub1.val,"One Protrusion")
	sub2 = Toggle("2",EVENT_NONE,48+pwadd+wadd,phadd+hadd,34,18,sub2.val,"Two Protrusions")
	sub3 = Toggle("3",EVENT_NONE,84+pwadd+wadd,phadd+hadd,34,18,sub3.val,"Three Protrusions")
	sub4 = Toggle("4",EVENT_NONE,120+pwadd+wadd,phadd+hadd,34,18,sub4.val,"Four Protrusions")
	EndAlign()
	glRasterPos2d(195+pwadd+wadd, 22+phadd+hadd)
	Text("Select tops of:")
	BeginAlign()
	selface1 = Toggle("1",EVENT_NONE,165+pwadd+wadd,phadd+hadd,34,18,selface1.val,"Select the tip of the protrusion when it is created")
	selface2 = Toggle("2",EVENT_NONE,201+pwadd+wadd,phadd+hadd,34,18,selface2.val,"Select the tips of each protrusion when they are created")
	selface3 = Toggle("3",EVENT_NONE,237+pwadd+wadd,phadd+hadd,34,18,selface3.val,"Select the tips of each protrusion when they are created")
	selface4 = Toggle("4",EVENT_NONE,273+pwadd+wadd,phadd+hadd,34,18,selface4.val,"Select the tips of each protrusion when they are created")
	EndAlign()
	#Doodads
	colorbox(8+dwadd+wadd,175+dhadd+hadd,312+dwadd+wadd,dhadd-5+hadd)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12+dwadd+wadd, 165+dhadd+hadd)
	Text("Doodads:")
	BeginAlign()
	dood1 = Toggle("1 Box",EVENT_NONE,12+dwadd+wadd,142+dhadd+hadd,45,18,dood1.val,"Creates a rectangular box")
	dood2 = Toggle("2 Box",EVENT_NONE,61+dwadd+wadd,142+dhadd+hadd,45,18,dood2.val,"Creates 2 side-by-side rectangular boxes")
	dood3 = Toggle("3 Box",EVENT_NONE,110+dwadd+wadd,142+dhadd+hadd,45,18,dood3.val,"Creates 3 side-by-side rectangular boxes")
	EndAlign()
	BeginAlign()
	dood4 = Toggle("\"L\"",EVENT_NONE,164+dwadd+wadd,142+dhadd+hadd,45,18,dood4.val,"Creates a Tetris-style \"L\" shape")
	dood5 = Toggle("\"T\"",EVENT_NONE,213+dwadd+wadd,142+dhadd+hadd,45,18,dood5.val,"Creates a Tetris-style \"T\" shape")
	dood6 = Toggle("\"S\"",EVENT_NONE,262+dwadd+wadd,142+dhadd+hadd,45,18,dood6.val,"Creates a sort-of \"S\" or \"Z\" shape")
	EndAlign()
	dodoodads = Toggle("Make Doodads",EVENT_NONE,12+dwadd+wadd,120+dhadd+hadd,145,18,dodoodads.val,"Make Doodads?")
	doodadfacechange = Number("Face %: ",EVENT_NONE,162+dwadd+wadd,120+dhadd+hadd,145,18,doodadfacechange.val,0,100,"Percentage of faces that will gain doodads")
	seldoodad = Toggle("Select Doodads",EVENT_NONE,12+dwadd+wadd,100+dhadd+hadd,145,18,seldoodad.val,"Selects doodads when they are created")
	seldoodtop = Toggle("Only Select Tops",EVENT_NONE,162+dwadd+wadd,100+dhadd+hadd,145,18,seldoodtop.val,"Only Selects tops of doodads when\"Select Doodads\" is on")
	doodonselface = Toggle("Only selected faces",EVENT_NONE,12+dwadd+wadd,80+dhadd+hadd,145,18,doodonselface.val,"Only create doodads on selected faces")
	onprot = Toggle("Only on Protrusions",EVENT_NONE,162+dwadd+wadd,80+dhadd+hadd,145,18,onprot.val,"Only place doodads on protrusions")
	
	#Doodad Properties
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12+dwadd+wadd, 63+dhadd+hadd)
	Text("Doodad Properties:")
	BeginAlign()
	doodadminamount = Number("Min Amount: ",EVENT_NONE,12+dwadd+wadd,40+dhadd+hadd,145,18,doodadminamount.val,0,100,"Minimum number of doodads per face")
	doodadmaxamount = Number("Max Amount: ",EVENT_NONE,162+dwadd+wadd,40+dhadd+hadd,145,18,doodadmaxamount.val,0,100,"Maximum number of doodads per face")
	EndAlign()
	BeginAlign()
	doodheightmin = Number("Min Height: ",EVENT_NONE,12+dwadd+wadd,20+dhadd+hadd,145,18,doodheightmin.val,0.0,100.0,"Minimum height of any doodad")
	doodheightmax = Number("Max Height: ",EVENT_NONE,162+dwadd+wadd,20+dhadd+hadd,145,18,doodheightmax.val,0.0,100.0,"Maximum height of any doodad")
	EndAlign()
	BeginAlign()
	doodsizemin = Number("Min Size %: ",EVENT_NONE,12+dwadd+wadd,dhadd+hadd,145,18,doodsizemin.val,0.0,100.0,"Minimum size of any doodad in percentage of face")
	doodsizemax = Number("Max Size %: ",EVENT_NONE,162+dwadd+wadd,dhadd+hadd,145,18,doodsizemax.val,0.0,100.0,"Maximum size of any doodad in percentage of face")
	EndAlign()
	
	#Materials
	colorbox(8+mwadd+wadd,93+mhadd+hadd,312+mwadd+wadd,mhadd-5+hadd)
	glColor3f(0.0,0.0,0.0)
	glRasterPos2d(12+mwadd+wadd, 83+mhadd+hadd)
	Text("Materials:")
	glRasterPos2d(12+mwadd+wadd, 43+mhadd+hadd)
	Text("Assigned Material Indices:")
	assignNewMats = Toggle("Assign materials by part",EVENT_NONE,32+mwadd+wadd,60+mhadd+hadd,256,18,assignNewMats.val,"Otherwise, previous materials will be preserved")
	replProtSideIndex = Number("Protrusion Sides:",EVENT_NONE,12+mwadd+wadd,20+mhadd+hadd,145,18,replProtSideIndex.val,0,16,"Material index assigned to sides of protrusions")
	replProtTopIndex = Number("Protrusion Tops:",EVENT_NONE,162+mwadd+wadd,20+mhadd+hadd,145,18,replProtTopIndex.val,0,16,"Material index assigned to tops of protrusions")
	replDoodSideIndex = Number("Doodad Sides:",EVENT_NONE,12+mwadd+wadd,mhadd+hadd,145,18,replDoodSideIndex.val,0,16,"Material index assigned to sides of doodads")
	replDoodTopIndex = Number("Doodad Tops:",EVENT_NONE,162+mwadd+wadd,mhadd+hadd,145,18,replDoodTopIndex.val,0,16,"Material index assigned to tops and bottoms of doodads")
	
	#Global Parts
	colorbox(8+gwadd+wadd,35+ghadd+hadd,312+gwadd+wadd,ghadd-5+hadd)
	glColor3f(1.0,0.0,0.0)
	glRasterPos2d(12+gwadd+wadd,25+ghadd+hadd)
	messagetext = Text(errortext)
	glColor3f(0.0,0.0,0.0)
	makenewobject = Toggle("Copy Before Modifying",EVENT_NONE,162+gwadd+wadd,ghadd+hadd,145,18,makenewobject.val,"If selected, the original object will be copied before it is changed")
	Button("Discombobulate",EVENT_DISCOMBOBULATE,12+gwadd+wadd,ghadd+hadd,100,18)
	Button("Exit",EVENT_EXIT,120+gwadd+wadd,ghadd+hadd,30,18)

def event(evt, val):
	global wadd
	global hadd
	
	if (evt == RIGHTARROWKEY and val):
		wadd = wadd + 20
		Redraw(1)
	if (evt == LEFTARROWKEY and val):
		wadd = wadd - 20
		Redraw(1)
	if (evt == UPARROWKEY and val):
		hadd = hadd + 20
		Redraw(1)
	if (evt == DOWNARROWKEY and val):
		hadd = hadd - 20
		Redraw(1)
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
	
	#Materials
	global assignNewMats
	global replProtSideIndex
	global replProtTopIndex
	global replDoodSideIndex
	global replDoodTopIndex
	
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
		setProtrusionValues(doprots.val,facechange.val/100,minheight.val,maxheight.val,sub1.val,sub2.val,sub3.val,sub4.val,mintaper.val/100,maxtaper.val/100,useselected.val,selface1.val,selface2.val,selface3.val,selface4.val,deselectvertices.val)
		setDoodadValues(dodoodads.val,doodadfacechange.val/100,seldoodad.val,onprot.val,dood1.val,dood2.val,dood3.val,dood4.val,dood5.val,dood6.val,doodadminamount.val,doodadmaxamount.val,doodsizemin.val/100,doodsizemax.val/100,doodheightmin.val,doodheightmax.val,doodonselface.val,seldoodtop.val)
		setOtherValues(makenewobject.val,assignNewMats.val,replProtSideIndex.val,replProtTopIndex.val,replDoodSideIndex.val,replDoodTopIndex.val)
		discombobulate()
		Window.WaitCursor(0)
		Blender.Redraw()

Register(draw, event, bevent)
