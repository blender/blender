#!BPY

"""
Name: 'Skin Two Vert-loops / Loft Multiple'
Blender: 234
Group: 'Mesh'
Submenu: 'Loft-loop - shortest edge method' A1
Submenu: 'Loft-loop - even method' A2
Submenu: 'Loft-segment - shortest edge' B1
Submenu: 'Loft-segment - even method' B2
Tooltip: 'Select 2 or more vert loops, then run this script'
"""

# $Id$
#
# -------------------------------------------------------------------------- 
# Skin Selected edges 1.0 By Campbell Barton (AKA Ideasman)
# -------------------------------------------------------------------------- 
# ***** BEGIN GPL LICENSE BLOCK ***** 
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



# Made by Ideasman/Campbell 2004/04/25 - ideasman@linuxmail.org

import Blender
from Blender import *
import math
from math import *
arg = __script__['arg']


#================#
# Math functions #
#================#

# Measure 2 points
def measure(v1, v2):
  return Mathutils.Vector([v1[0]-v2[0], v1[1] - v2[1], v1[2] - v2[2]]).length
  
# Clamp
def clamp(max, number):
	while number >= max:
		number = number - max
	return number

#=============================================================#
# List func that takes the last item and adds it to the front #
#=============================================================#
def listRotate(ls):
	return [ls[-1]] + ls[:-1]

#=================================================================#
# Recieve a list of locs: [x,y,z] and return the average location #
#=================================================================#
def averageLocation(locList):
	avLoc = [0,0,0]
	
	# Loop through x/y/z
	for coordIdx in [0,1,2]:
		
		# Add all the values from 1 of the 3 coords at the avLoc.
		for loc in locList:
			avLoc[coordIdx] += loc[coordIdx]
		
		avLoc[coordIdx] = avLoc[coordIdx] / len(locList)	
	return avLoc



#=============================#
# Blender functions/shortcuts #
#=============================#
def error(str):
	Draw.PupMenu('ERROR%t|'+str)

# Returns a new face that has the same properties as the origional face
# With no verts though
def copyFace(face):
  newFace = NMesh.Face()
  # Copy some generic properties
  newFace.mode = face.mode
  if face.image != None:
    newFace.image = face.image
  newFace.flag = face.flag
  newFace.mat = face.mat
  newFace.smooth = face.smooth
  return newFace

#=============================================#
# Find a selected vert that 2 faces share.    #
#=============================================#
def selVertBetween2Faces(face1, face2):
	for v1 in face1.v:
		if v1.sel:
			for v2 in face2.v:
				if v1 == v2:
					return v1
	
	
#=======================================================#
# Measure the total distance between all the edges in   #
# 2 vertex loops                                        #
#=======================================================#
def measureVloop(mesh, v1loop, v2loop, surplusFaces):
	totalDist = 0
	
	# Rotate the vertloops to cycle through each pair.
	# of faces to compate the distance between the 2 poins
	for ii in range(len(v1loop)):
		if ii not in surplusFaces:
			V1 = selVertBetween2Faces(mesh.faces[v1loop[0]], mesh.faces[v1loop[1]])
			V2 = selVertBetween2Faces(mesh.faces[v2loop[0]], mesh.faces[v2loop[1]])
			
			P1 = (V1[0],V1[1],V1[2])
			P2 = (V2[0],V2[1],V2[2])
	
			totalDist += measure(P1,P2)
			v1loop = listRotate(v1loop)
			v2loop = listRotate(v2loop)
	
	#selVertBetween2Faces(mesh.faces[v2loop[0]], mesh.faces[v2loop[1]])
	return totalDist

# Remove the shortest edge from a vert loop
def removeSmallestFace(mesh, vloop):
	bestDistSoFar = None
	bestFIdxSoFar = None
	for fIdx in vloop:
		vSelLs = []
		for v in mesh.faces[fIdx].v:
			if v.sel:
				vSelLs.append(v)
		
		dist = measure(vSelLs[0].co, vSelLs[1].co)
		
		if bestDistSoFar == None:
			bestDistSoFar = dist
			bestFIdxSoFar = fIdx 
		elif dist < bestDistSoFar:
			bestDistSoFar = dist
			bestFIdxSoFar = fIdx
	
	# Return the smallest face index of the vloop that was sent
	return bestFIdxSoFar


#=============================================#
# Take 2 vert loops and skin them             #
#=============================================#
def skinVertLoops(mesh, v1loop, v2loop):
	
	
	#=============================================#
	# Handle uneven vert loops, this is tricky    #
	#=============================================#
	# Reorder so v1loop is always the biggest
	if len(v1loop) < len(v2loop):
		v1loop, v2loop = v2loop, v1loop
	
	# Work out if the vert loops are equel or not, if not remove the extra faces from the larger
	surplusFaces = []
	tempv1loop = eval(str(v1loop)) # strip faces off this one, use it to keep track of which we have taken faces from.
	if len(v1loop) > len(v2loop):
		
		# Even face method.
		if arg[1] == '2':
			remIdx = 0
			faceStepping = len(	v1loop) / len(v2loop)
			while len(v1loop) - len(surplusFaces) > len(v2loop):
				remIdx += faceStepping
				surplusFaces.append(tempv1loop[ clamp(len(tempv1loop),remIdx) ]) 
				tempv1loop.remove(surplusFaces[-1])
		
		# Shortest face
		elif arg[1] == '1':
			while len(v1loop) - len(surplusFaces) > len(v2loop):
				surplusFaces.append(removeSmallestFace(mesh, tempv1loop)) 
				tempv1loop.remove(surplusFaces[-1])
			
	
	tempv1loop = None
	
	v2loop = optimizeLoopOrdedShortEdge(mesh, v1loop, v2loop, surplusFaces)
	
	# make Faces from 
	lenVloop = len(v1loop)
	lenSupFaces = len(surplusFaces)
	fIdx = 0
	offset = 0
	while fIdx < lenVloop:
		
		face = copyFace( mesh.faces[v1loop[clamp(lenVloop, fIdx+1)]] )
		
		if v1loop[fIdx] in surplusFaces:
			# Draw a try, this face does not catch with an edge.
			# So we must draw a tri and wedge it in.
			
			# Copy old faces properties
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v1loop[clamp(lenVloop, fIdx)]],\
			mesh.faces[v1loop[clamp(lenVloop, fIdx+1)]]) )
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v1loop[clamp(lenVloop, fIdx+1)]],\
			mesh.faces[v1loop[clamp(lenVloop, fIdx+2)]]) )
			
			#face.v.append( selVertBetween2Faces(\
			#mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, (fIdx - offset +1 ))]],\
			#mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, (fIdx - offset + 2))]]) )
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, (fIdx - offset))]],\
			mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, fIdx - offset + 1)]]) )
			
			mesh.faces.append(face)			
			
			# We need offset to work out how much smaller v2loop is at this current index.
			offset+=1		
			

		else:	
			# Draw a normal quad between the 2 edges/faces
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v1loop[clamp(lenVloop, fIdx)]],\
			mesh.faces[v1loop[clamp(lenVloop, fIdx+1)]]) )
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v1loop[clamp(lenVloop, fIdx+1)]],\
			mesh.faces[v1loop[clamp(lenVloop, fIdx+2)]]) )
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, (fIdx - offset +1 ))]],\
			mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, (fIdx - offset + 2))]]) )
			
			face.v.append( selVertBetween2Faces(\
			mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, (fIdx - offset))]],\
			mesh.faces[v2loop[clamp(lenVloop - lenSupFaces, fIdx - offset + 1)]]) )
			
			mesh.faces.append(face)
			
		fIdx +=1
		
	return mesh



#=======================================================#
# Takes a face and returns the number of selected verts #
#=======================================================#
def faceVSel(face):
	vSel = 0
	for v in face.v:
		if v.sel:
			vSel +=1
	return vSel




#================================================================#
# This function takes a face and returns its selected vert loop  #
# it returns a list of face indicies
#================================================================#
def vertLoop(mesh, startFaceIdx, fIgLs): # fIgLs is a list of faces to ignore.
	# Here we store the faces indicies that
	# are a part of the first vertex loop
	vertLoopLs = [startFaceIdx]

	restart = 0
	while restart == 0:
		# this keeps the face loop going until its told to stop,
		# If the face loop does not find an adjacent face then the vert loop has been compleated
		restart = 1 
		
		# Get my selected verts for the active face/edge.
		selVerts = []
		for v in mesh.faces[vertLoopLs[-1]].v:
			selVerts.append(v)
		
		fIdx = 0
		while fIdx < len(mesh.faces) and restart:
			# Not already added to the vert list
			if fIdx not in fIgLs + vertLoopLs:
				# Has 2 verts selected
				if faceVSel(mesh.faces[fIdx]) > 1:
					# Now we need to find if any of the selected verts
					# are shared with our active face. (are we next to ActiveFace)
					for v in mesh.faces[fIdx].v:
						if v in selVerts:
							vertLoopLs.append(fIdx)
							restart = 0 # restart the face loop.
							break
					
			fIdx +=1
			
	return vertLoopLs




#================================================================#
# Now we work out the optimum order to 'skin' the 2 vert loops   #
# by measuring the total distance of all edges created,          #
# test this for every possible series of joins                   # 
# and find the shortest, Once this is done the                   #
# shortest dist can be skinned.                                  #
# returns only the 2nd-reordered vert loop                       #
#================================================================#
def optimizeLoopOrded(mesh, v1loop, v2loop):
	bestSoFar = None
	
	# Measure the dist, ii is just a counter
	for ii in range(len(v1loop)):
		
		# Loop twice , Once for the forward test, and another for the revearsed
		for iii in [0, 0]:
			dist = measureVloop(mesh, v1loop, v2loop)
			# Initialize the Best distance recorded
			if bestSoFar == None:
				bestSoFar = dist
				bestv2Loop = eval(str(v2loop))
				
			elif dist < bestSoFar: # Update the info if a better vloop rotation is found.
				bestSoFar = dist
				bestv2Loop = eval(str(v2loop))
			
			# We might have got the vert loop backwards, try the other way
			v2loop.reverse()
		v2loop = listRotate(v2loop)
	return bestv2Loop
	
	
	
#================================================================#
# Now we work out the optimum order to 'skin' the 2 vert loops   #
# by measuring the total distance of all edges created,          #
# test this for every possible series of joins                   # 
# and find the shortest, Once this is done the                   #
# shortest dist can be skinned.                                  #
# returns only the 2nd-reordered vert loop                       #
#================================================================#
def optimizeLoopOrdedShortEdge(mesh, v1loop, v2loop, surplusFaces):
	bestSoFar = None
	
	# Measure the dist, ii is just a counter
	for ii in range(len(v2loop)):
		
		# Loop twice , Once for the forward test, and another for the revearsed
		for iii in [0, 0]:
			dist = measureVloop(mesh, v1loop, v2loop, surplusFaces)
			print 'dist', dist 
			# Initialize the Best distance recorded
			if bestSoFar == None:
				bestSoFar = dist
				bestv2Loop = eval(str(v2loop))
				
			elif dist < bestSoFar: # Update the info if a better vloop rotation is found.
				bestSoFar = dist
				bestv2Loop = eval(str(v2loop))
			
			# We might have got the vert loop backwards, try the other way
			v2loop.reverse()
		v2loop = listRotate(v2loop)
	print 'best so far ', bestSoFar
	return bestv2Loop	
	
	
	
	


#==============================#
#  Find our     vert loop list #
#==============================#
# Find a face with 2 verts selected,
#this will be the first face in out vert loop
def findVertLoop(mesh, fIgLs): # fIgLs is a list of faces to ignore.
	
	startFaceIdx = None
	
	fIdx = 0
	while fIdx < len(mesh.faces):	
		if fIdx not in fIgLs:
			# Do we have an edge?
			if faceVSel(mesh.faces[fIdx]) > 1:
				# THIS IS THE STARTING FACE.
				startFaceIdx = fIdx
				break
		fIdx+=1
	
	# Here we access the function that generates the real vert loop
	if startFaceIdx != None:
		return vertLoop(mesh, startFaceIdx, fIgLs)
	else:
		# We are out'a vert loops, return a None,
		return None

#===================================#
# Get the average loc of a vertloop #
# This is used when working out the #
# order to loft an object           #
#===================================#
def vLoopAverageLoc(mesh, vertLoop):
	locList = [] # List of vert locations
		
	fIdx = 0
	while fIdx < len(mesh.faces):	
		if fIdx in vertLoop:
			for v in mesh.faces[fIdx].v:
				if v.sel:
					locList.append(v.co)
		fIdx+=1
	
	return averageLocation(locList)



#=================================================#
# Vert loop group functions

def getAllVertLoops(mesh):
	# Make a chain of vert loops.
	fIgLs = [] # List of faces to ignore 
	allVLoops = [findVertLoop(mesh, fIgLs)]
	while allVLoops[-1] != None:
		
		# In future ignore all faces in this vert loop
		fIgLs += allVLoops[-1]		
		
		# Add the new vert loop to the list
		allVLoops.append( findVertLoop(mesh, fIgLs) )
	
	return allVLoops[:-1] # Remove the last Value- None.
	
	
def reorderCircularVLoops(mesh, allVLoops):
	# Now get a location for each vert loop.
	allVertLoopLocs = []
	for vLoop in allVLoops:
		allVertLoopLocs.append( vLoopAverageLoc(mesh, vLoop) )

	# We need to find the longest distance between 2 vert loops so we can 
	reorderedVLoopLocs = []

	# Start with this one, then find the next closest.
	# in doing this make a new list called reorderedVloop
	currentVLoop = 0
	reorderedVloopIdx = [currentVLoop]
	newOrderVLoops = [allVLoops[0]] # This is a re-ordered allVLoops
	while len(reorderedVloopIdx) != len(allVLoops):
		bestSoFar = None
		bestVIdxSoFar = None
		for vLoopIdx in range(len(allVLoops)):
			if vLoopIdx not in reorderedVloopIdx + [currentVLoop]:
				if bestSoFar == None:
					bestSoFar = measure( allVertLoopLocs[vLoopIdx], allVertLoopLocs[currentVLoop] )
					bestVIdxSoFar = vLoopIdx
				else:
					newDist = measure( allVertLoopLocs[vLoopIdx], allVertLoopLocs[currentVLoop] )
					if newDist < bestSoFar:
						bestSoFar = newDist
						bestVIdxSoFar = vLoopIdx
		
		reorderedVloopIdx.append(bestVIdxSoFar)
		reorderedVLoopLocs.append(allVertLoopLocs[bestVIdxSoFar])
		newOrderVLoops.append( allVLoops[bestVIdxSoFar] ) 
		
		# Start looking for the next best fit
		currentVLoop = bestVIdxSoFar
	
	# This is not the locicle place to put this but its convieneint.
	# Here we find the 2 vert loops that are most far apart
	# We use this to work out which 2 vert loops not to skin when making an open loft.
	vLoopIdx = 0
	# Longest measured so far - 0 dummy.
	bestSoFar = 0
	while vLoopIdx < len(reorderedVLoopLocs):
		
		
		# Skin back to the start if needs be, becuase this is a crcular loft
		toSkin2 = vLoopIdx + 1
		if toSkin2 == len(reorderedVLoopLocs):
			toSkin2 = 0
			
		
		newDist  = measure( reorderedVLoopLocs[vLoopIdx], reorderedVLoopLocs[toSkin2] )
		
		if newDist >= bestSoFar:
			bestSoFar = newDist
			vLoopIdxNotToSkin = vLoopIdx + 1	
				
		vLoopIdx +=1 
	
	return newOrderVLoops, vLoopIdxNotToSkin


is_editmode = Window.EditMode()
if is_editmode: Window.EditMode(0)

# Get a mesh and raise errors if we cant
mesh = None
if len(Object.GetSelected()) > 0:
  if Object.GetSelected()[0].getType() == 'Mesh':
    mesh = Object.GetSelected()[0].getData()
  else:
    error('please select a mesh')
else:
  error('no mesh object selected')


if mesh != None:
  allVLoops = getAllVertLoops(mesh)
  
  # Re order the vert loops
  allVLoops, vLoopIdxNotToSkin = reorderCircularVLoops(mesh, allVLoops)	
  
  vloopIdx = 0
  while vloopIdx < len(allVLoops):
    #print range(len(allVLoops) )
    #print vloopIdx
    #print allVLoops[vloopIdx]
    
    # Skin back to the start if needs be, becuase this is a crcular loft
    toSkin2 = vloopIdx + 1
    if toSkin2 == len(allVLoops):
      toSkin2 = 0
    
    # Circular loft or not?
    if arg[0] == 'B': # B for open
      if vloopIdx != vLoopIdxNotToSkin:
        mesh = skinVertLoops(mesh, allVLoops[vloopIdx], allVLoops[toSkin2])
    elif arg[0] == 'A': # A for closed
      mesh = skinVertLoops(mesh, allVLoops[vloopIdx], allVLoops[toSkin2])
    
    vloopIdx +=1	
  
  mesh.update()

if is_editmode: Window.EditMode(1)
