# Default Doodad Set for Discombobulator
# by Evan J. Rosky, 2005
# GPL- http://www.gnu.org/copyleft/gpl.html
#
# $Id$
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


#Run discombobulator.py, not this.

import Blender
from Blender import NMesh,Object,Material
from Blender.NMesh import Vert,Face
from Blender.Mathutils import *
		
import BPyMathutils
from BPyMathutils import genrand
a = BPyMathutils.sgenrand(4859)

#Create random numbers
def randnum(low,high):
	num = genrand()
	num = num*(high-low)
	num = num+low
	return num

face = Face()
xmin = Vector([0,0,0])
xmax = Vector([0,0,0])
ymin = Vector([0,0,0])
ymax = Vector([0,0,0])
mxmin = Vector([0,0,0])
mxmax = Vector([0,0,0])
mymin = Vector([0,0,0])
mymax = Vector([0,0,0])
doodadCenter = Vector([0,0,0])
orientation = 0
center = Vector([0,0,0])
tosel = 0
seltopsonly = 0
tempx = []
doodadMesh = NMesh.GetRaw()

global materialArray
global reassignMats
global thereAreMats
global currmat
global doodSideMat
global doodTopMat

#face is the face to add the doodad to.
#sizeX and sizeY are values from 0.0 to 1.0 that represents a percentage the face that is covered by the doodad.
#height is how tall the doodad is.

def settings(seltops,matArr,reasMats,therMats,sidemat,topmat):
	global seltopsonly
	global materialArray
	global reassignMats
	global thereAreMats
	global currmat
	global doodSideMat
	global doodTopMat
	materialArray = matArr
	reassignMats = reasMats
	thereAreMats = therMats
	seltopsonly = seltops
	doodSideMat = sidemat
	doodTopMat = topmat

def setCurrMat(curma):
	global currmat
	currmat = curma

#Find center and orientation of doodad
def findDoodadCenter(sizeX, sizeY):
	#globalizing junk
	global face
	global xmin
	global xmax
	global ymin
	global ymax
	global orientation
	global doodadCenter
	global center
	global tosel
	global mxmin
	global mxmax
	global mymin
	global mymax
	global tempx
	global seltopsonly
	
	#Find the center of the face
	center = Vector([0,0,0])
	for pt in face.v:
		center = center + pt.co
	center = divideVectorByInt(center,len(face.v))
	
	#Find Temp Location Range by looking at the sizes
	txmin = ((divideVectorByInt((face.v[0].co + face.v[3].co),2)) - center)*(1-sizeX) + center
	txmax = ((divideVectorByInt((face.v[1].co + face.v[2].co),2)) - center)*(1-sizeX) + center
	tymin = ((divideVectorByInt((face.v[0].co + face.v[1].co),2)) - center)*(1-sizeY) + center
	tymax = ((divideVectorByInt((face.v[2].co + face.v[3].co),2)) - center)*(1-sizeY) + center
	
	#Find Center of doodad
	amtx = randnum(0.0,1.0)
	amty = randnum(0.0,1.0)
	thepoint = (((((txmin - txmax)*amtx + txmax) - ((tymin - tymax)*amty + tymax))*.5 + ((tymin - tymax)*amty + tymax)) - center)*2 + center
	doodadCenter = Vector([thepoint[0],thepoint[1],thepoint[2]])
	
	#Find Main Range by looking at the sizes
	mxmin = divideVectorByInt((face.v[0].co + face.v[3].co),2)
	mxmax = divideVectorByInt((face.v[1].co + face.v[2].co),2)
	mymin = divideVectorByInt((face.v[0].co + face.v[1].co),2)
	mymax = divideVectorByInt((face.v[2].co + face.v[3].co),2)
	
	#Find x/y equivs for whole face
	ve1 = (txmin - txmax)*amtx + txmax
	ve1 = ve1 - mxmax
	nax = ve1.length
	ve1 = (mxmin - mxmax)
	nax = nax/ve1.length
	
	ve1 = (tymin - tymax)*amty + tymax
	ve1 = ve1 - mymax
	nay = ve1.length
	ve1 = (mymin - mymax)
	nay = nay/ve1.length
	
	#Find new box thing
	tempx = []
	amtx = nax-sizeX/2
	amty = nay-sizeY/2
	tempx.append((((((mxmin - mxmax)*amtx + mxmax) - ((mymin - mymax)*amty + mymax))*.5 + ((mymin - mymax)*amty + mymax)) - center)*2 + center)
	
	amtx = nax-sizeX/2
	amty = nay+sizeY/2
	tempx.append((((((mxmin - mxmax)*amtx + mxmax) - ((mymin - mymax)*amty + mymax))*.5 + ((mymin - mymax)*amty + mymax)) - center)*2 + center)
	
	amtx = nax+sizeX/2
	amty = nay+sizeY/2
	tempx.append((((((mxmin - mxmax)*amtx + mxmax) - ((mymin - mymax)*amty + mymax))*.5 + ((mymin - mymax)*amty + mymax)) - center)*2 + center)
	
	amtx = nax+sizeX/2
	amty = nay-sizeY/2
	tempx.append((((((mxmin - mxmax)*amtx + mxmax) - ((mymin - mymax)*amty + mymax))*.5 + ((mymin - mymax)*amty + mymax)) - center)*2 + center)
	
	#Find New Location Range by looking at the sizes
	xmin = divideVectorByInt((tempx[0] + tempx[3]),2)
	xmax = divideVectorByInt((tempx[1] + tempx[2]),2)
	ymin = divideVectorByInt((tempx[0] + tempx[1]),2)
	ymax = divideVectorByInt((tempx[2] + tempx[3]),2)

#Make a point
def makePoint(x,y,z=0):
	global xmin
	global xmax
	global ymin
	global ymax
	global doodadCenter
	global tosel
	global seltopsonly
	global face

	amtx = x
	amty = y
	thepoint = (((((xmin - xmax)*amtx + xmax) - ((ymin - ymax)*amty + ymax))*.5 + ((ymin - ymax)*amty + ymax)) - doodadCenter)*2 + doodadCenter
	thepoint = thepoint + z*Vector(face.no)
	tver = Vert(thepoint[0],thepoint[1],thepoint[2])
	if tosel == 1 and seltopsonly == 0 and z == 0:
		tver.sel = 1
	return tver

#extrude ground-plane(s)
def extrudedoodad(vArray,heig):
	global face
	global doodadMesh
	global tosel
	
	topVArray = []
	
	doodadMesh.verts.extend(vArray)
	
	#Create array for extruded verts
	for ind in range(0,(len(vArray))):
		point = vArray[ind].co + heig*Vector(face.no)
		ver = Vert(point[0],point[1],point[2])
		if tosel == 1:
			ver.sel = 1
		topVArray.append(ver)
		doodadMesh.verts.append(topVArray[ind])
	
	#make faces around sides
	for ind in range(0,(len(vArray) - 1)):
		face = Face()
		face.v.extend([vArray[ind],vArray[ind+1],topVArray[ind+1],topVArray[ind]])
		if tosel == 1 and seltopsonly == 0: face.sel = 1
		if thereAreMats == 1:
			if reassignMats == 0 or doodSideMat == 0:
				face.materialIndex = currmat
			else:
				face.materialIndex = doodSideMat-1
		doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vArray[len(vArray) - 1],vArray[0],topVArray[0],topVArray[len(topVArray) - 1]])
	if tosel == 1 and seltopsonly == 0: 
		face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodSideMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodSideMat-1
	doodadMesh.faces.append(face)
	
	return topVArray

#For switching face vertices
def fixvertindex(ind):
	if ind > 3:
		indx = ind - 4
	else:
		indx = ind
	return indx

#runs doodads
def createDoodad(indexArray,facec,minsi,maxsi,minhei,maxhei,selec,amtmin,amtmax,facpercent):
	global doodadMesh
	global seltopsonly
	global tosel
	
	doodadMesh = NMesh.GetRaw()
	
	theamt = round(randnum(amtmin,amtmax),0)
	theamt = int(theamt)
	tosel = selec
	
	for i in range(0,(theamt)):
		if randnum(0,1) <= facpercent:
			index = round(randnum(1,len(indexArray)),0)
			index = indexArray[(int(index) - 1)]
			
			Xsi = randnum(minsi,maxsi)
			Ysi = randnum(minsi,maxsi)
			hei = randnum(minhei,maxhei)
					
			#Determine orientation
			orient = int(round(randnum(0.0,3.0)))
			
			#face to use as range
			facer = Face()
			facer.v.extend([facec.v[orient],facec.v[fixvertindex(1+orient)],facec.v[fixvertindex(2+orient)],facec.v[fixvertindex(3+orient)]])
			
			if index == 1:
				singleBox(facer,Xsi,Ysi,hei)
			if index == 2:
				doubleBox(facer,Xsi,Ysi,hei)
			if index == 3:
				tripleBox(facer,Xsi,Ysi,hei)
			if index == 4:
				LShape(facer,Xsi,Ysi,hei)
			if index == 5:
				TShape(facer,Xsi,Ysi,hei)
			if index == 6:
				if randnum(0.0,1.0) > .5:
					SShape(facer,Xsi,Ysi,hei)
				else:
					ZShape(facer,Xsi,Ysi,hei)
	
	return doodadMesh

def divideVectorByInt(thevect,theint):
	thevect.x = thevect.x/theint
	thevect.y = thevect.y/theint
	thevect.z = thevect.z/theint
	return thevect

#Single Box Doodad
def singleBox(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	vertArray = []
	
	#place four points
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,1))
	vertArray.append(makePoint(1,1))
	vertArray.append(makePoint(1,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	face = Face()
	face.v.extend(vertArray)
	face.v.reverse()
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend(topVertArray)
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
#Double Box Doodad
def doubleBox(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	vertArray = []
	
	#place first box
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,1))
	vertArray.append(makePoint(0.45,1))
	vertArray.append(makePoint(0.45,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	face = Face()
	face.v.extend(vertArray)
	face.v.reverse()
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend(topVertArray)
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	vertArray = []
	
	#place second box
	vertArray.append(makePoint(0.55,0))
	vertArray.append(makePoint(0.55,1))
	vertArray.append(makePoint(1,1))
	vertArray.append(makePoint(1,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	face = Face()
	face.v.extend(vertArray)
	face.v.reverse()
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend(topVertArray)
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)

#Triple Box Doodad
def tripleBox(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	vertArray = []
	
	#place first box
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,1))
	vertArray.append(makePoint(0.3,1))
	vertArray.append(makePoint(0.3,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	face = Face()
	face.v.extend(vertArray)
	face.v.reverse()
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend(topVertArray)
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	vertArray = []
	
	#place second box
	vertArray.append(makePoint(0.35,0))
	vertArray.append(makePoint(0.35,1))
	vertArray.append(makePoint(0.65,1))
	vertArray.append(makePoint(0.65,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	face = Face()
	face.v.extend(vertArray)
	face.v.reverse()
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend(topVertArray)
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	vertArray = []
	
	#place third box
	vertArray.append(makePoint(0.7,0))
	vertArray.append(makePoint(0.7,1))
	vertArray.append(makePoint(1,1))
	vertArray.append(makePoint(1,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	face = Face()
	face.v.extend(vertArray)
	face.v.reverse()
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend(topVertArray)
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)

#The "L" Shape
def LShape(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	rcon1 = randnum(0.2,0.8)
	rcon2 = randnum(0.2,0.8)
	
	vertArray = []
	
	#place L shape
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,rcon1))
	vertArray.append(makePoint(0,1))
	vertArray.append(makePoint(rcon2,1))
	vertArray.append(makePoint(rcon2,rcon1))
	vertArray.append(makePoint(1,rcon1))
	vertArray.append(makePoint(1,0))
	vertArray.append(makePoint(rcon2,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	#This fills in the bottom of doodad with faceness
	face = Face()
	face.v.extend([vertArray[0],vertArray[1],vertArray[4],vertArray[7]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[1],vertArray[2],vertArray[3],vertArray[4]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[4],vertArray[5],vertArray[6],vertArray[7]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	#This fills in the top with faceness
	face = Face()
	face.v.extend([topVertArray[0],topVertArray[1],topVertArray[4],topVertArray[7]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[1],topVertArray[2],topVertArray[3],topVertArray[4]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[4],topVertArray[5],topVertArray[6],topVertArray[7]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
#The "T" Shape
def TShape(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	rcony = randnum(0.25,0.75)
	rconx1 = randnum(0.1,0.49)
	rconx2 = randnum(0.51,0.9)
	
	vertArray = []
	
	#place T shape
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,rcony))
	vertArray.append(makePoint(rconx1,rcony))
	vertArray.append(makePoint(rconx1,1))
	vertArray.append(makePoint(rconx2,1))
	vertArray.append(makePoint(rconx2,rcony))
	vertArray.append(makePoint(1,rcony))
	vertArray.append(makePoint(1,0))
	vertArray.append(makePoint(rconx2,0))
	vertArray.append(makePoint(rconx1,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	#fills bottom with faceness
	face = Face()
	face.v.extend([vertArray[0],vertArray[1],vertArray[2],vertArray[9]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[2],vertArray[3],vertArray[4],vertArray[5]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[5],vertArray[6],vertArray[7],vertArray[8]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[8],vertArray[9],vertArray[2],vertArray[5]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	#fills top with faceness
	face = Face()
	face.v.extend([topVertArray[0],topVertArray[1],topVertArray[2],topVertArray[9]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[2],topVertArray[3],topVertArray[4],topVertArray[5]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[5],topVertArray[6],topVertArray[7],topVertArray[8]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[8],topVertArray[9],topVertArray[2],topVertArray[5]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
#The "S" or "Z" Shapes
def SShape(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	rcony1 = randnum(0.1,0.49)
	rcony2 = randnum(0.51,0.9)
	rconx1 = randnum(0.1,0.49)
	rconx2 = randnum(0.51,0.9)
	
	vertArray = []
	
	#place S shape
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,rcony1))
	vertArray.append(makePoint(rconx1,rcony1))
	vertArray.append(makePoint(rconx1,rcony2))
	vertArray.append(makePoint(rconx1,1))
	vertArray.append(makePoint(rconx2,1))
	vertArray.append(makePoint(1,1))
	vertArray.append(makePoint(1,rcony2))
	vertArray.append(makePoint(rconx2,rcony2))
	vertArray.append(makePoint(rconx2,rcony1))
	vertArray.append(makePoint(rconx2,0))
	vertArray.append(makePoint(rconx1,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	#fills bottom with faceness
	face = Face()
	face.v.extend([vertArray[0],vertArray[1],vertArray[2],vertArray[11]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[2],vertArray[9],vertArray[10],vertArray[11]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[2],vertArray[3],vertArray[8],vertArray[9]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[3],vertArray[4],vertArray[5],vertArray[8]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[5],vertArray[6],vertArray[7],vertArray[8]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	#fills top with faceness
	face = Face()
	face.v.extend([topVertArray[0],topVertArray[1],topVertArray[2],topVertArray[11]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[2],topVertArray[9],topVertArray[10],topVertArray[11]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[2],topVertArray[3],topVertArray[8],topVertArray[9]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[3],topVertArray[4],topVertArray[5],topVertArray[8]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[5],topVertArray[6],topVertArray[7],topVertArray[8]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
def ZShape(facel, Xsize, Ysize, height):
	#globaling junk
	global face
	global tosel
	global doodadMesh
	
	face = Face()
	face = facel
	
	findDoodadCenter(Xsize, Ysize)
	
	rcony1 = randnum(0.1,0.49)
	rcony2 = randnum(0.51,0.9)
	rconx1 = randnum(0.1,0.49)
	rconx2 = randnum(0.51,0.9)
	
	vertArray = []
	
	#place Z shape
	vertArray.append(makePoint(0,0))
	vertArray.append(makePoint(0,rcony1))
	vertArray.append(makePoint(0,rcony2))
	vertArray.append(makePoint(rconx1,rcony2))
	vertArray.append(makePoint(rconx2,rcony2))
	vertArray.append(makePoint(rconx2,1))
	vertArray.append(makePoint(1,1))
	vertArray.append(makePoint(1,rcony2))
	vertArray.append(makePoint(1,rcony1))
	vertArray.append(makePoint(rconx2,rcony1))
	vertArray.append(makePoint(rconx1,rcony1))
	vertArray.append(makePoint(rconx1,0))
	topVertArray = extrudedoodad(vertArray,height)
	
	#fills bottom with faceness
	face = Face()
	face.v.extend([vertArray[0],vertArray[1],vertArray[10],vertArray[11]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[1],vertArray[2],vertArray[3],vertArray[10]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[3],vertArray[4],vertArray[9],vertArray[10]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[4],vertArray[7],vertArray[8],vertArray[9]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([vertArray[4],vertArray[5],vertArray[6],vertArray[7]])
	face.v.reverse()
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
	#fills top with faceness
	face = Face()
	face.v.extend([topVertArray[0],topVertArray[1],topVertArray[10],topVertArray[11]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[1],topVertArray[2],topVertArray[3],topVertArray[10]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[3],topVertArray[4],topVertArray[9],topVertArray[10]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[4],topVertArray[7],topVertArray[8],topVertArray[9]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	face = Face()
	face.v.extend([topVertArray[4],topVertArray[5],topVertArray[6],topVertArray[7]])
	if tosel == 1: 
			face.sel = 1
	if thereAreMats == 1:
		if reassignMats == 0 or doodTopMat == 0:
			face.materialIndex = currmat
		else:
			face.materialIndex = doodTopMat-1
	doodadMesh.faces.append(face)
	
