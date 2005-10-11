#!BPY

"""
Name: 'Bridge Faces/Edge-Loops'
Blender: 237
Group: 'Mesh'
Tooltip: 'Select 2 vert loops, then run this script.'
"""

__author__ = "Campbell Barton AKA Ideasman"
__url__ = ["http://members.iinet.net.au/~cpbarton/ideasman/", "blender", "elysiun"]
__version__ = "1.0 2004/04/25"

__bpydoc__ = """\
With this script vertex loops can be skinned: faces are created to connect the
selected loops of vertices.

Usage:

In mesh Edit mode select the vertices of the loops (closed paths / curves of
vertices: circles, for example) that should be skinned, then run this script.
A pop-up will provide further options, if the results of a method are not adequate try one of the others.
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

# Made by Ideasman/Campbell 2005/06/15 - ideasman@linuxmail.org

import Blender
from Blender import *

BIG_NUM = 1<<30

global CULL_METHOD
CULL_METHOD = 0

class edge:
	def __init__(self, v1,v2):
		self.v1 = v1
		self.v2 = v2
		
		# uv1 uv2 vcol1 vcol2 # Add later
		self.length = (v1.co - v2.co).length
		
		self.removed = 0 # Have we been culled from the eloop
		self.match = None # The other edge were making a face with


class edgeLoop:
	def __init__(self, loop): # Vert loop
		# Use next and prev, nextDist, prevDist
		
		# Get Loops centre.
		self.centre = Mathutils.Vector()
		f = 1.0/len(loop)
		for v in loop:
			self.centre += v.co * f
		
		
		
		
		# Convert Vert loop to Edges.
		self.edges = []
		vIdx = 0
		while vIdx < len(loop):
			self.edges.append( edge(loop[vIdx-1], loop[vIdx]) )
			vIdx += 1
		
		# Assign linked list
		for eIdx in range(len(self.edges)-1):
			self.edges[eIdx].next = self.edges[eIdx+1]
			self.edges[eIdx].prev = self.edges[eIdx-1]
		# Now last
		self.edges[-1].next = self.edges[0]
		self.edges[-1].prev = self.edges[-2]
		
		
		
		# GENERATE AN AVERAGE NORMAL FOR THE WHOLE LOOP.
		self.normal = Mathutils.Vector()
		for e in self.edges:
			n = Mathutils.CrossVecs(self.centre-e.v1.co, self.centre-e.v2.co)
			# Do we realy need tot normalize?
			n.normalize()
			self.normal += n
		self.normal.normalize()
		
		
		# Generate a normal for each edge.
		for e in self.edges:
			
			n1 = e.v1.co
			n2 = e.v2.co
			n3 = e.prev.v1.co
			
			a = n1-n2
			b = n1-n3
			normal1 = Mathutils.CrossVecs(a,b)
			normal1.normalize()
			
			n1 = e.v2.co
			n3 = e.next.v2.co
			n2 = e.v1.co
			
			a = n1-n2
			b = n1-n3
			
			normal2 = Mathutils.CrossVecs(a,b)
			normal2.normalize()
			
			# Reuse normal1 var
			normal1 += normal1 + normal2
			normal1.normalize()
			
			e.normal = normal1
			#print e.normal
		


		
	def backup(self):
		# Keep a backup of the edges
		self.backupEdges = self.edges[:]
			
	def restore(self):
		self.edges = self.backupEdges[:]
		for e in self.edges:
			e.removed = 0
		
	def reverse(self):
		self.edges.reverse()
		for e in self.edges:
			e.normal = -e.normal
			e.v1, e.v2 = e.v2, e.v1
		self.normal = -self.normal
	
	# Removes N Smallest edges and backs up
	def removeSmallest(self, cullNum, otherLoopLen):
		global CULL_METHOD
		if CULL_METHOD == 0: # Shortest edge
			
			eloopCopy = self.edges[:]
			eloopCopy.sort(lambda e1, e2: cmp(e1.length, e2.length )) # Length sort, smallest first
			eloopCopy = eloopCopy[:cullNum]
			for e in eloopCopy:
				e.removed = 1
				self.edges.remove( e ) # Remove from own list, still in linked list.
			
		else: # CULL METHOD is even
				
			culled = 0
			
			step = int(otherLoopLen / float(cullNum))
			
			currentEdge = self.edges[0]
			while culled < cullNum:
				
				# Get the shortest face in the next STEP
				while currentEdge.removed == 1:
					# Bug here!
					currentEdge = currentEdge.next
				smallestEdge = currentEdge
				
				for i in range(step):
					currentEdge = currentEdge.next
					while currentEdge.removed == 1:
						currentEdge = currentEdge.next
					if smallestEdge.length > currentEdge.length:
						smallestEdge = currentEdge
				
				# In that stepping length we have the smallest edge.remove it
				smallestEdge.removed = 1
				self.edges.remove(smallestEdge)
				
				culled+=1
	

# Returns face edges.
# face must have edge data.
def faceEdges(me, f):
	if len(f) == 3:
		return [\
		 me.findEdge(f[0], f[1]),\
		 me.findEdge(f[1], f[2]),\
		 me.findEdge(f[2], f[0])\
		]
	elif len(f) == 4:
		return [\
		 me.findEdge(f[0], f[1]),\
		 me.findEdge(f[1], f[2]),\
		 me.findEdge(f[2], f[3]),\
		 me.findEdge(f[3], f[0])\
		]


def getSelectedEdges(me, ob):	
	SEL_FLAG = NMesh.EdgeFlags['SELECT']
	FGON_FLAG = NMesh.EdgeFlags['FGON']	
	
	edges = [e for e in me.edges if e.flag & SEL_FLAG if (e.flag & FGON_FLAG) == 0 ]
	
	# Now remove edges that face 2 or more selected faces usoing them
	edgeFromSelFaces = []
	for f in me.faces:
		if len(f) >2 and f.sel:
			edgeFromSelFaces.extend(faceEdges(me, f))
	
	# Remove all edges with 2 or more selected faces as uses.
	for e in edgeFromSelFaces:
		if edgeFromSelFaces.count(e) > 1:
			me.removeEdge(e.v1, e.v2)
	
	# Remove selected faces?
	fIdx = len(me.faces)
	while fIdx:
		fIdx-=1
		if len(me.faces[fIdx]) > 2:
			if me.faces[fIdx].sel:
				me.faces.pop(fIdx)
	return [e for e in edges if edgeFromSelFaces.count(e) < 2]
	
	
# Like vert loops 
def getVertLoops(selEdges):
	mainVertLoops = []
	while selEdges:
		e = selEdges.pop()
		contextVertLoop= [e.v1, e.v2] # start the vert loop
		
		eIdx = 1 # Get us into the loop. dummy var.
		
		# if eIdx == 0 then it means we searched and found no matches... 
		# time for a new vert loop,
		while eIdx:
			eIdx = len(selEdges)
			while eIdx:
				eIdx-=1
				
				# Check for edge attached at the head of the loop.
				if contextVertLoop[0] == selEdges[eIdx].v1:
					contextVertLoop.insert(0, selEdges.pop(eIdx).v2)
				elif contextVertLoop[0] == selEdges[eIdx].v2:
					contextVertLoop.insert(0, selEdges.pop(eIdx).v1)
					
				# Chech for edge vert at the tail.
				elif contextVertLoop[-1] == selEdges[eIdx].v1:
					contextVertLoop.append(selEdges.pop(eIdx).v2)
				elif contextVertLoop[-1] == selEdges[eIdx].v2:
					contextVertLoop.append(selEdges.pop(eIdx).v1)
				else:
					# None found? Keep looking
					continue
				
				# Once found we.
				break
		
		# Is this a loop? if so then its forst and last vert must be teh same.
		if contextVertLoop[0].index == contextVertLoop[-1].index:
			contextVertLoop.pop() # remove double vert
			mainVertLoops.append(contextVertLoop)
		
		# Build context vert loops
	return mainVertLoops


def skin2EdgeLoops(eloop1, eloop2, me, ob, MODE):
	# Make sure e1 loops is bigger then e2
	if len(eloop1.edges) != len(eloop2.edges):
		if len(eloop1.edges) < len(eloop2.edges):
			eloop1, eloop2 = eloop2, eloop1
		
		eloop1.backup() # were about to cull faces
		CULL_FACES = len(eloop1.edges) - len(eloop2.edges)
		eloop1.removeSmallest(CULL_FACES, len(eloop1.edges))
	else:
		CULL_FACES = 0
	# First make sure poly vert loops are in sync with eachother.
	
	# The vector allong which we are skinning.
	skinVector = eloop1.centre - eloop2.centre
	
	loopDist = skinVector.length
	
	
	# IS THE LOOP FLIPPED, IF SO FLIP BACK.
	angleBetweenLoopNormals = Mathutils.AngleBetweenVecs(eloop1.normal, eloop2.normal)
	
	if angleBetweenLoopNormals > 90:
		eloop2.reverse()
	
	
	bestEloopDist = BIG_NUM
	bestOffset = 0
	# Loop rotation offset to test.1
	eLoopIdxs = range(len(eloop1.edges))
	for offset in range(len(eloop1.edges)):
		totEloopDist = 0 # Measure this total distance for thsi loop.
		
		offsetIndexLs = eLoopIdxs[offset:] + eLoopIdxs[:offset] # Make offset index list
		
		# e1Idx is always from 0 to N, e2Idx is offset.
		for e1Idx, e2Idx in enumerate(offsetIndexLs):
			# Measure the vloop distance ===============
			totEloopDist += ((eloop1.edges[e1Idx].v1.co - eloop2.edges[e2Idx].v1.co).length / loopDist) #/ nangle1
			totEloopDist += ((eloop1.edges[e1Idx].v2.co - eloop2.edges[e2Idx].v2.co).length / loopDist) #/ nangle1
			
			# Premeture break if where no better off
			if totEloopDist > bestEloopDist:
				break
		
		if totEloopDist < bestEloopDist:
			bestOffset = offset
			bestEloopDist = totEloopDist
	
	# Modify V2 LS for Best offset
	eloop2.edges = eloop2.edges[bestOffset:] + eloop2.edges[:bestOffset]
	
	
	
	for loopIdx in range(len(eloop2.edges)):
		e1 = eloop1.edges[loopIdx]
		e2 = eloop2.edges[loopIdx]
		
		# Remember the pairs for fan filling culled edges.
		e1.match = e2; e2.match = e1
		
		# need some smart face flipping code here.
		f = NMesh.Face([e1.v1, e1.v2, e2.v2, e2.v1])
		
		f.sel = 1
		me.faces.append(f)
	
	# FAN FILL MISSING FACES.
	if CULL_FACES:
		# Culled edges will be in eloop1.
		FAN_FILLED_FACES = 0
		
		contextEdge = eloop1.edges[0] # The larger of teh 2
		while FAN_FILLED_FACES < CULL_FACES:
			while contextEdge.next.removed == 0:
				contextEdge = contextEdge.next
			
			vertFanPivot = contextEdge.match.v2
			
			while contextEdge.next.removed == 1:
				
				f = NMesh.Face([contextEdge.next.v1, contextEdge.next.v2, vertFanPivot] )
				
				
				f.sel = 1
				me.faces.append(f)
				
				# Should we use another var?, this will work for now.
				contextEdge.next.removed = 1
				
				contextEdge = contextEdge.next
				FAN_FILLED_FACES += 1
		
		eloop1.restore() # Add culled back into the list.
	#if angleBetweenLoopNormals > 90:
	#	eloop2.reverse()


def main():
	global CULL_METHOD
	
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	ob = Scene.GetCurrent().getActiveObject()
	if ob == None or ob.getType() != 'Mesh':
		return
	
	me = ob.getData()
	if not me.edges:
		Draw.PupMenu('Error, add edge data first')
		if is_editmode: Window.EditMode(1)
		return
	
	# BAD BLENDER PYTHON API, NEED TO ENTER EXIT EDIT MODE FOR ADDING EDGE DATA.
	# ADD EDGE DATA HERE, Python API CANT DO IT YET, LOOSES SELECTION
	
	selEdges = getSelectedEdges(me, ob)
	vertLoops = getVertLoops(selEdges) # list of lists of edges.
	
	if len(vertLoops) > 2:
		choice = Draw.PupMenu('Loft '+str(len(vertLoops))+' edge loops%t|loop|segment')
		if choice == -1:
			if is_editmode: Window.EditMode(1)
			return
	elif len(vertLoops) < 2:
		Draw.PupMenu('Error, No Vertloops found%t|if you have a valid selection, go in and out of face edit mode to update the selection state.')
		if is_editmode: Window.EditMode(1)	
		return
	else:
		choice = 2
	
	
	# The line below checks if any of the vert loops are differenyt in length.
	if False in [len(v) == len(vertLoops[0]) for v in vertLoops]:
		CULL_METHOD = Draw.PupMenu('Small to large edge loop distrobution method%t|remove edges evenly|remove smallest edges edges')
		if CULL_METHOD == -1:
			if is_editmode: Window.EditMode(1)
			return
		
		if CULL_METHOD ==1: # RESET CULL_METHOD
			CULL_METHOD = 0 # shortest
		else:
			CULL_METHOD = 1 # even
	
	
	time1 = sys.time()
	# Convert to special edge data.
	edgeLoops = []
	for vloop in vertLoops:
		edgeLoops.append(edgeLoop(vloop))
		
	
	# VERT LOOP ORDERING CODE
	# Build a worm list - grow from Both ends
	edgeOrderedList = [edgeLoops.pop()]
	
	# Find the closest.
	bestSoFar = BIG_NUM
	bestIdxSoFar = None
	for edLoopIdx, edLoop in enumerate(edgeLoops):
		l =(edgeOrderedList[-1].centre - edLoop.centre).length 
		if l < bestSoFar:
			bestIdxSoFar = edLoopIdx
			bestSoFar = l
			
	edgeOrderedList.append( edgeLoops.pop(bestIdxSoFar) )
	
	# Now we have the 2 closest, append to either end-
	# Find the closest.
	while edgeLoops:
		bestSoFar = BIG_NUM
		bestIdxSoFar = None
		first_or_last = 0 # Zero is first
		for edLoopIdx, edLoop in enumerate(edgeLoops):
			l1 =(edgeOrderedList[-1].centre - edLoop.centre).length 
			
			if l1 < bestSoFar:
				bestIdxSoFar = edLoopIdx
				bestSoFar = l1
				first_or_last = 1 # last
			
			l2 =(edgeOrderedList[0].centre - edLoop.centre).length 
			if l2 < bestSoFar:
				bestIdxSoFar = edLoopIdx
				bestSoFar = l2
				first_or_last = 0 # last
		
		if first_or_last: # add closest Last
			edgeOrderedList.append( edgeLoops.pop(bestIdxSoFar) )	
		else: # Add closest First
			edgeOrderedList.insert(0, edgeLoops.pop(bestIdxSoFar) )	 # First
	
	for i in range(len(edgeOrderedList)-1):
		skin2EdgeLoops(edgeOrderedList[i], edgeOrderedList[i+1], me, ob, 0)	
	if choice == 1 and len(edgeOrderedList) > 2: # Loop
		skin2EdgeLoops(edgeOrderedList[0], edgeOrderedList[-1], me, ob, 0)	
	
	me.update(1, 1, 0)
	if is_editmode: Window.EditMode(1)

main()
