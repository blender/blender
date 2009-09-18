#!BPY

"""
Name: 'Skin Faces/Edge-Loops'
Blender: 243
Group: 'MeshFaceKey'
Tooltip: 'Select 2 vert loops, then run this script.'
"""

__author__ = "Campbell Barton AKA Ideasman"
__url__ = ["blenderartists.org", "www.blender.org"]
__version__ = "1.1 2006/12/26"

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

# Made by Ideasman/Campbell 2005/06/15 - cbarton@metavr.com

import Blender
import bpy
from Blender import Window
from Blender.Mathutils import MidpointVecs, Vector
from Blender.Mathutils import AngleBetweenVecs as _AngleBetweenVecs_
import BPyMessages

from Blender.Draw import PupMenu

BIG_NUM = 1<<30

global CULL_METHOD
CULL_METHOD = 0

def AngleBetweenVecs(a1,a2):
	try:
		return _AngleBetweenVecs_(a1,a2)
	except:
		return 180.0

class edge(object):
	__slots__ = 'v1', 'v2', 'co1', 'co2', 'length', 'removed', 'match', 'cent', 'angle', 'next', 'prev', 'normal', 'fake'
	def __init__(self, v1,v2):
		self.v1 = v1
		self.v2 = v2
		co1, co2= v1.co, v2.co
		self.co1= co1
		self.co2= co2
		
		# uv1 uv2 vcol1 vcol2 # Add later
		self.length = (co1 - co2).length
		self.removed = 0	# Have we been culled from the eloop
		self.match = None	# The other edge were making a face with
		
		self.cent= MidpointVecs(co1, co2)
		self.angle= 0.0
		self.fake= False

class edgeLoop(object):
	__slots__ = 'centre', 'edges', 'normal', 'closed', 'backup_edges'
	def __init__(self, loop, me, closed): # Vert loop
		# Use next and prev, nextDist, prevDist
		
		# Get Loops centre.
		fac= len(loop)
		verts = me.verts
		self.centre= reduce(lambda a,b: a+verts[b].co/fac, loop, Vector())
		
		# Convert Vert loop to Edges.
		self.edges = [edge(verts[loop[vIdx-1]], verts[loop[vIdx]]) for vIdx in xrange(len(loop))]
		
		if not closed:
			self.edges[0].fake = True # fake edge option
			
		self.closed = closed
			
		
		# Assign linked list
		for eIdx in xrange(len(self.edges)-1):
			self.edges[eIdx].next = self.edges[eIdx+1]
			self.edges[eIdx].prev = self.edges[eIdx-1]
		# Now last
		self.edges[-1].next = self.edges[0]
		self.edges[-1].prev = self.edges[-2]
		
		
		
		# GENERATE AN AVERAGE NORMAL FOR THE WHOLE LOOP.
		self.normal = Vector()
		for e in self.edges:
			n = (self.centre-e.co1).cross(self.centre-e.co2)
			# Do we realy need tot normalize?
			n.normalize()
			self.normal += n
			
			# Generate the angle
			va= e.cent - e.prev.cent
			vb= e.next.cent - e.cent
			
			e.angle= AngleBetweenVecs(va, vb)
		
		# Blur the angles
		#for e in self.edges:
		#	e.angle= (e.angle+e.next.angle)/2
		
		# Blur the angles
		#for e in self.edges:
		#	e.angle= (e.angle+e.prev.angle)/2
			
		self.normal.normalize()
		
		# Generate a normal for each edge.
		for e in self.edges:
			
			n1 = e.co1
			n2 = e.co2
			n3 = e.prev.co1
			
			a = n1-n2
			b = n1-n3
			normal1 = a.cross(b)
			normal1.normalize()
			
			n1 = e.co2
			n3 = e.next.co2
			n2 = e.co1
			
			a = n1-n2
			b = n1-n3
			
			normal2 = a.cross(b)
			normal2.normalize()
			
			# Reuse normal1 var
			normal1 += normal1 + normal2
			normal1.normalize()
			
			e.normal = normal1
			#print e.normal


		
	def backup(self):
		# Keep a backup of the edges
		self.backup_edges = self.edges[:]
			
	def restore(self):
		self.edges = self.backup_edges[:]
		for e in self.edges:
			e.removed = 0
		
	def reverse(self):
		self.edges.reverse()
		self.normal.negate()
		
		for e in self.edges:
			e.normal.negate()
			e.v1, e.v2 = e.v2, e.v1
			e.co1, e.co2 = e.co2, e.co1
			e.next, e.prev = e.prev, e.next
		
	
	def removeSmallest(self, cullNum, otherLoopLen):
		'''
		Removes N Smallest edges and backs up the loop,
		this is so we can loop between 2 loops as if they are the same length,
		backing up and restoring incase the loop needs to be skinned with another loop of a different length.
		'''
		global CULL_METHOD
		if CULL_METHOD == 1: # Shortest edge
			eloopCopy = self.edges[:]
			
			# Length sort, smallest first
			try:	eloopCopy.sort(key = lambda e1: e1.length)
			except:	eloopCopy.sort(lambda e1, e2: cmp(e1.length, e2.length ))
			
			# Dont use atm
			#eloopCopy.sort(lambda e1, e2: cmp(e1.angle*e1.length, e2.angle*e2.length)) # Length sort, smallest first
			#eloopCopy.sort(lambda e1, e2: cmp(e1.angle, e2.angle)) # Length sort, smallest first
			
			remNum = 0
			for i, e in enumerate(eloopCopy):
				if not e.fake:
					e.removed = 1
					self.edges.remove( e ) # Remove from own list, still in linked list.
					remNum += 1
				
					if not remNum < cullNum:
						break
			
		else: # CULL METHOD is even
				
			culled = 0
			
			step = int(otherLoopLen / float(cullNum)) * 2
			
			currentEdge = self.edges[0]
			while culled < cullNum:
				
				# Get the shortest face in the next STEP
				step_count= 0
				bestAng= 360.0
				smallestEdge= None
				while step_count<=step or smallestEdge==None:
					step_count+=1
					if not currentEdge.removed: # 0 or -1 will not be accepted
						if currentEdge.angle<bestAng and not currentEdge.fake:
							smallestEdge= currentEdge
							bestAng= currentEdge.angle
					
					currentEdge = currentEdge.next
				
				# In that stepping length we have the smallest edge.remove it
				smallestEdge.removed = 1
				self.edges.remove(smallestEdge)
				
				# Start scanning from the edge we found? - result is over fanning- no good.
				#currentEdge= smallestEdge.next
				
				culled+=1
	

# Returns face edges.
# face must have edge data.

def getSelectedEdges(me, ob):	
	MESH_MODE= Blender.Mesh.Mode()
	
	if MESH_MODE & Blender.Mesh.SelectModes.EDGE or MESH_MODE & Blender.Mesh.SelectModes.VERTEX:
		Blender.Mesh.Mode(Blender.Mesh.SelectModes.EDGE)
		edges= [ ed for ed in me.edges if ed.sel ]
		# print len(edges), len(me.edges)
		Blender.Mesh.Mode(MESH_MODE)
		return edges
	
	elif MESH_MODE & Blender.Mesh.SelectModes.FACE:
		Blender.Mesh.Mode(Blender.Mesh.SelectModes.EDGE)
		
		# value is [edge, face_sel_user_in]
		'''
		try: # Python 2.4 only
			edge_dict=  dict((ed.key, [ed, 0]) for ed in me.edges)
		except:
		'''
		# Cant try 2.4 syntax because python 2.3 will complain still
		edge_dict=  dict([(ed.key, [ed, 0]) for ed in me.edges])
		
		for f in me.faces:
			if f.sel:
				for edkey in f.edge_keys:
					edge_dict[edkey][1] += 1
		
		Blender.Mesh.Mode(MESH_MODE)
		return [ ed_data[0] for ed_data in edge_dict.itervalues() if ed_data[1] == 1 ]
	
	

def getVertLoops(selEdges, me):
	'''
	return a list of vert loops, closed and open [(loop, closed)...]
	'''
	
	mainVertLoops = []
	# second method
	tot = len(me.verts)
	vert_siblings = [[] for i in xrange(tot)]
	vert_used = [False] * tot
	
	for ed in selEdges:
		i1, i2 = ed.key
		vert_siblings[i1].append(i2)
		vert_siblings[i2].append(i1)
	
	# find the first used vert and keep looping.
	for i in xrange(tot):
		if vert_siblings[i] and not vert_used[i]:
			sbl = vert_siblings[i] # siblings
			
			if len(sbl) > 2:
				return None
			
			vert_used[i] = True
			
			# do an edgeloop seek
			if len(sbl) == 2:
				contextVertLoop= [sbl[0], i, sbl[1]] # start the vert loop
				vert_used[contextVertLoop[ 0]] = True
				vert_used[contextVertLoop[-1]] = True
			else:
				contextVertLoop= [i, sbl[0]]
				vert_used[contextVertLoop[ 1]] = True
			
			# Always seek up
			ok = True
			while ok:
				ok = False
				closed = False
				sbl = vert_siblings[contextVertLoop[-1]]
				if len(sbl) == 2:
					next = sbl[not sbl.index( contextVertLoop[-2] )]
					if vert_used[next]:
						closed = True
						# break
					else:
						contextVertLoop.append( next ) # get the vert that isnt the second last
						vert_used[next] = True
						ok = True
			
			# Seek down as long as the starting vert was not at the edge.
			if not closed and len(vert_siblings[i]) == 2:
				
				ok = True
				while ok:
					ok = False
					sbl = vert_siblings[contextVertLoop[0]]
					if len(sbl) == 2:
						next = sbl[not sbl.index( contextVertLoop[1] )]
						if vert_used[next]:
							closed = True
						else:
							contextVertLoop.insert(0, next) # get the vert that isnt the second last
							vert_used[next] = True
							ok = True
			
			mainVertLoops.append((contextVertLoop, closed))
	
	
	verts = me.verts
	# convert from indicies to verts
	# mainVertLoops = [([verts[i] for i in contextVertLoop], closed) for contextVertLoop, closed in  mainVertLoops]
	# print len(mainVertLoops)
	return mainVertLoops
	


def skin2EdgeLoops(eloop1, eloop2, me, ob, MODE):
	
	new_faces= [] # 
	
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
	
	# IS THE LOOP FLIPPED, IF SO FLIP BACK. we keep it flipped, its ok,
	if eloop1.closed or eloop2.closed:
		angleBetweenLoopNormals = AngleBetweenVecs(eloop1.normal, eloop2.normal)
		if angleBetweenLoopNormals > 90:
			eloop2.reverse()
			

		DIR= eloop1.centre - eloop2.centre
		
		# if eloop2.closed:
		bestEloopDist = BIG_NUM
		bestOffset = 0
		# Loop rotation offset to test.1
		eLoopIdxs = range(len(eloop1.edges))
		for offset in xrange(len(eloop1.edges)):
			totEloopDist = 0 # Measure this total distance for thsi loop.
			
			offsetIndexLs = eLoopIdxs[offset:] + eLoopIdxs[:offset] # Make offset index list
			
			
			# e1Idx is always from 0uu to N, e2Idx is offset.
			for e1Idx, e2Idx in enumerate(offsetIndexLs):
				e1= eloop1.edges[e1Idx]
				e2= eloop2.edges[e2Idx]
				
				
				# Include fan connections in the measurement.
				OK= True
				while OK or e1.removed:
					OK= False
					
					# Measure the vloop distance ===============
					diff= ((e1.cent - e2.cent).length) #/ nangle1
					
					ed_dir= e1.cent-e2.cent
					a_diff= AngleBetweenVecs(DIR, ed_dir)/18 # 0 t0 18
					
					totEloopDist += (diff * (1+a_diff)) / (1+loopDist)
					
					# Premeture break if where no better off
					if totEloopDist > bestEloopDist:
						break
					
					e1=e1.next
					
			if totEloopDist < bestEloopDist:
				bestOffset = offset
				bestEloopDist = totEloopDist
		
		# Modify V2 LS for Best offset
		eloop2.edges = eloop2.edges[bestOffset:] + eloop2.edges[:bestOffset]
			
	else:
		# Both are open loops, easier to calculate.
		
		
		# Make sure the fake edges are at the start.
		for i, edloop in enumerate((eloop1, eloop2)):
			# print "LOOPO"
			if edloop.edges[0].fake:
				# alredy at the start
				#print "A"
				pass
			elif edloop.edges[-1].fake:
				# put the end at the start
				edloop.edges.insert(0, edloop.edges.pop())
				#print "B"
				
			else:
				for j, ed in enumerate(edloop.edges):
					if ed.fake:
						#print "C"
						edloop.edges = edloop.edges = edloop.edges[j:] + edloop.edges[:j]
						break
		# print "DONE"
		ed1, ed2 = eloop1.edges[0], eloop2.edges[0]
		
		if not ed1.fake or not ed2.fake:
			raise "Error"
		
		# Find the join that isnt flipped (juts like detecting a bow-tie face)
		a1 = (ed1.co1 - ed2.co1).length + (ed1.co2 - ed2.co2).length
		a2 = (ed1.co1 - ed2.co2).length + (ed1.co2 - ed2.co1).length
		
		if a1 > a2:
			eloop2.reverse()
			# make the first edge the start edge still
			eloop2.edges.insert(0, eloop2.edges.pop())
	
	
	
	
	for loopIdx in xrange(len(eloop2.edges)):
		e1 = eloop1.edges[loopIdx]
		e2 = eloop2.edges[loopIdx]
		
		# Remember the pairs for fan filling culled edges.
		e1.match = e2; e2.match = e1
		
		if not (e1.fake or e2.fake):
			new_faces.append([e1.v1, e1.v2, e2.v2, e2.v1])
	
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
				#if not contextEdge.next.fake:
				new_faces.append([contextEdge.next.v1, contextEdge.next.v2, vertFanPivot])
				
				# Should we use another var?, this will work for now.
				contextEdge.next.removed = 1
				
				contextEdge = contextEdge.next
				FAN_FILLED_FACES += 1
		
		# may need to fan fill backwards 1 for non closed loops.
		
		eloop1.restore() # Add culled back into the list.
	
	return new_faces

def main():
	global CULL_METHOD
	
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	ob = bpy.data.scenes.active.objects.active
	if ob == None or ob.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return
	
	me = ob.getData(mesh=1)
	
	if me.multires:
		BPyMessages.Error_NoMeshMultiresEdit()
		return
	
	time1 = Blender.sys.time()
	selEdges = getSelectedEdges(me, ob)
	vertLoops = getVertLoops(selEdges, me) # list of lists of edges.
	if vertLoops == None:
		PupMenu('Error%t|Selection includes verts that are a part of more then 1 loop')
		if is_editmode: Window.EditMode(1)
		return
	# print len(vertLoops)
	
	
	if len(vertLoops) > 2:
		choice = PupMenu('Loft '+str(len(vertLoops))+' edge loops%t|loop|segment')
		if choice == -1:
			if is_editmode: Window.EditMode(1)
			return
	elif len(vertLoops) < 2:
		PupMenu('Error%t|No Vertloops found!')
		if is_editmode: Window.EditMode(1)	
		return
	else:
		choice = 2
	
	
	# The line below checks if any of the vert loops are differenyt in length.
	if False in [len(v[0]) == len(vertLoops[0][0]) for v in vertLoops]:
		CULL_METHOD = PupMenu('Small to large edge loop distrobution method%t|remove edges evenly|remove smallest edges')
		if CULL_METHOD == -1:
			if is_editmode: Window.EditMode(1)
			return
		
		if CULL_METHOD ==1: # RESET CULL_METHOD
			CULL_METHOD = 0 # shortest
		else:
			CULL_METHOD = 1 # even
	
	
	time1 = Blender.sys.time()
	# Convert to special edge data.
	edgeLoops = []
	for vloop, closed in vertLoops:
		edgeLoops.append(edgeLoop(vloop, me, closed))
		
	
	# VERT LOOP ORDERING CODE
	# "Build a worm" list - grow from Both ends
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
	
	faces = []
	
	for i in xrange(len(edgeOrderedList)-1):
		faces.extend( skin2EdgeLoops(edgeOrderedList[i], edgeOrderedList[i+1], me, ob, 0) )
	if choice == 1 and len(edgeOrderedList) > 2: # Loop
		faces.extend( skin2EdgeLoops(edgeOrderedList[0], edgeOrderedList[-1], me, ob, 0) )
	
	# REMOVE SELECTED FACES.
	MESH_MODE= Blender.Mesh.Mode()
	if MESH_MODE & Blender.Mesh.SelectModes.EDGE or MESH_MODE & Blender.Mesh.SelectModes.VERTEX: pass
	elif MESH_MODE & Blender.Mesh.SelectModes.FACE:
		try: me.faces.delete(1, [ f for f in me.faces if f.sel ])
		except: pass
	
	me.faces.extend(faces, smooth = True)
	
	print '\nSkin done in %.4f sec.' % (Blender.sys.time()-time1)
	
	
	if is_editmode: Window.EditMode(1)

if __name__ == '__main__':
	main()
