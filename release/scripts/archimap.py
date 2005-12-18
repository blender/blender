#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'ArchiMap UV Unwrapper'
Blender: 237
Group: 'UV'
Tooltip: 'ArchiMap UV Unwrap Mesh faces.'
"""


__author__ = "Campbell Barton"
__url__ = ("blender", "elysiun")
__version__ = "1.0 6/13/05"

__bpydoc__ = """\
This script projection unwraps the selected faces of a mesh.

It operates on all selected mesh objects, and can be set to unwrap
selected faces, or all faces.

"""

# -------------------------------------------------------------------------- 
# Archimap UV Projection Unwrapper v1.0 by Campbell Barton (AKA Ideasman) 
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



from Blender import *
from Blender import Mathutils
from Blender.Mathutils import *

from math import cos, acos,pi,sqrt


try:
	import sys as py_sys
except:
	py_sys = None


DEG_TO_RAD = pi/180.0
SMALL_NUM = 0.000000001
BIG_NUM = 1e15



try:
	dummy = Mathutils.Matrix( Mathutils.Matrix() )
	del dummy
	NEW_2_38_MATHUTILS = True
except TypeError:
	NEW_2_38_MATHUTILS = False


global USER_FILL_HOLES
global USER_FILL_HOLES_QUALITY
USER_FILL_HOLES = None
USER_FILL_HOLES_QUALITY = None

# ================================================== USER_BOX_PACK_MOD.py
from Blender import NMesh, Window, sys

# a box packing vert
class vt:
	def __init__(self, x,y):
		self.x, self.y = x, y
		
		self.free = 15
		
		# Set flags so cant test bottom left of 0/0
		#~ BLF = 1; TRF = 2; TLF = 4; BRF = 8
		
		#self.users = [] # A list of boxes.
		# Rather then users, store Quadrents
		self.blb = self.tlb = self.brb = self.trb = None
		
		
		# A hack to remember the box() that last intersectec this vert
		self.intersectCache = ([], [], [], [])
		
class vertList:
	def __init__(self, verts=[]):
		self.verts = verts
	
	# Sorts closest first. - uses the box w/h as a bias,
	# this makes it so its less likely to have lots of poking out bits
	# that use too much 
	# Lambada based sort
	def sortCorner(self,w,h):
		self.verts.sort(lambda A, B: cmp(max(A.x+w, A.y+h) , max(B.x+w, B.y+h))) # Reverse area sort
		

class box:
	global packedVerts
	def __init__(self, width, height, id=None):
		global packedVerts
		
		self.id= id
		
		self.area = width * height # real area
		self.farea = width + height # fake area
		#self.farea = float(min(width, height)) / float(max(width, height))  # fake area
		
		self.width = width
		self.height = height
		
		# Append 4 new verts
		# (BL,TR,TL,BR) / 0,1,2,3
		self.v = [vt(0,0), vt(width,height), vt(0,height), vt(width,0)]
		
		# Set the interior quadrents as used.
		self.v[0].free &= ~TRF
		self.v[1].free &= ~BLF
		self.v[2].free &= ~BRF
		self.v[3].free &= ~TLF
		
		#for v in self.v:
		#	v.users.append(self)
		self.v[0].trb = self
		self.v[1].blb = self
		self.v[2].brb = self
		self.v[3].tlb = self
		
		
	
	# Updates verts 3 & 4 from 1 and 2
	# since 3 and 4 are only there foill need is resizing/ rotating of patterns on the fly while I painr new box placement
	# but may be merged later with other verts
	def updateV34(self):
		
		self.v[TL].x = self.v[BL].x
		self.v[TL].y = self.v[TR].y
		
		self.v[BR].x = self.v[TR].x
		self.v[BR].y = self.v[BL].y 
		

	def setLeft(self, lft=None):     
		self.v[TR].x = lft + self.v[TR].x - self.v[BL].x
		self.v[BL].x = lft
		# update othere verts
		self.updateV34()

	def setRight(self, rgt=None):    
		self.v[BL].x = rgt - (self.v[TR].x - self.v[BL].x)
		self.v[TR].x = rgt
		self.updateV34()

	def setBottom(self, btm=None):     
		self.v[TR].y = btm + self.v[TR].y - self.v[BL].y
		self.v[BL].y = btm
		self.updateV34()

	def setTop(self, tp=None):    
		self.v[BL].y = tp - (self.v[TR].y - self.v[BL].y)
		self.v[TR].y = tp
		self.updateV34()
			
	def getLeft(self):
		return self.v[BL].x

	def getRight(self):
		return self.v[TR].x

	def getBottom(self):
		return self.v[BL].y

	def getTop(self):
		return self.v[TR].y
	
	# Returns none, meaning it didnt overlap any new boxes
	def overlapAll(self, boxLs, intersectCache): # Flag index lets us know which quadere
		if self.v[BL].x < 0:
			return None
		elif self.v[BL].y < 0:
			return None			
		else:
			bIdx = len(intersectCache)
			while bIdx:
				bIdx-=1
				b = intersectCache[bIdx]
				if not (self.v[TR].y <= b.v[BL].y or\
								self.v[BL].y >= b.v[TR].y or\
								self.v[BL].x >= b.v[TR].x or\
								self.v[TR].x <= b.v[BL].x ):
					
					return None # Intersection with existing box
			#return 0 # Must keep looking
			
			
			for b in boxLs.boxes:
				if not (self.v[TR].y <= b.v[BL].y or\
								self.v[BL].y >= b.v[TR].y or\
								self.v[BL].x >= b.v[TR].x or\
								self.v[TR].x <= b.v[BL].x ):
					
					return b # Intersection with new box.
			return 0
	
	
	
	def place(self, vert, quad):
		if quad == BLF:
			self.setLeft(vert.x)
			self.setBottom(vert.y)
			
		elif quad == TRF:
			self.setRight(vert.x)
			self.setBottom(vert.y)
		
		elif quad == TLF:
			self.setLeft(vert.x)
			self.setTop(vert.y)

		elif quad == BRF:
			self.setRight(vert.x)
			self.setTop(vert.y)
	
	# Trys to lock a box onto another box's verts
	# cleans up double verts after 
	def tryVert(self, boxes, baseVert):
		flagIndex = -1
		for freeQuad in quadFlagLs:
			flagIndex +=1
			#print 'Testing ', self.width
			if not baseVert.free & freeQuad:
				continue
			
			self.place(baseVert, freeQuad)
			overlapBox = self.overlapAll(boxes, baseVert.intersectCache[flagIndex])
			if overlapBox == 0: # There is no overlap
				baseVert.free &= ~freeQuad # Removes quad
				# Appends all verts but the one that matches. this removes the need for remove doubles
				for vIdx in range(4): # (BL,TR,TL,BR): # (BL,TR,TL,BR) / 0,1,2,3
					self_v = self.v[vIdx] # shortcut
					if not (self_v.x == baseVert.x and self_v.y == baseVert.y):
						packedVerts.verts.append(self_v)
					else:
						baseVert.free &= self.v[vIdx].free # make sure the 
						
						# Inherit used boxes from old verts
						if self_v.blb: baseVert.blb = self_v.blb 
						if self_v.brb: baseVert.brb = self_v.brb #print 'inherit2'
						if self_v.tlb: baseVert.tlb = self_v.tlb #print 'inherit3'
						if self_v.trb: baseVert.trb = self_v.trb #print 'inherit4'
						self.v[vIdx] = baseVert
				
				
				# ========================== WHY DOSENT THIS WORK???
				#~ if baseVert.tlb and baseVert.trb:
					#~ if self == baseVert.tlb or self == baseVert.trb:
						
						#~ if baseVert.tlb.height > baseVert.trb.height:
							#~ #baseVert.trb.v[TL].free &= ~TLF & ~BLF
							#~ baseVert.trb.v[TL].free &= ~TLF
							#~ baseVert.trb.v[TL].free &= ~BLF
						
						#~ elif baseVert.tlb.height < baseVert.trb.height:
							#~ #baseVert.trb.v[TL].free &= ~TLF & ~BLF
							#~ baseVert.tlb.v[TR].free &= ~TRF
							#~ baseVert.tlb.v[TR].free &= ~BRF
						#~ else: # same
							#~ baseVert.tlb.v[TR].free &= ~BLF
							#~ baseVert.trb.v[TL].free &= ~BRF						
							
				
				#~ if baseVert.blb and baseVert.brb:
					#~ if self == baseVert.blb or self == baseVert.brb:
						
						#~ if baseVert.blb.height > baseVert.brb.height:
							#~ #baseVert.trb.v[TL].free &= ~TLF & ~BLF
							#~ baseVert.brb.v[BL].free &= ~TLF
							#~ baseVert.brb.v[BL].free &= ~BLF
						
						#~ elif baseVert.blb.height < baseVert.brb.height:
							#~ #baseVert.trb.v[TL].free &= ~TLF & ~BLF
							#~ baseVert.blb.v[BR].free &= ~TRF
							#~ baseVert.blb.v[BR].free &= ~BRF
						#~ else: # same
							#~ baseVert.blb.v[BR].free &= ~TLF
							#~ baseVert.brb.v[BL].free &= ~TRF		

							#~ # print 'Hay', baseVert.tlb.height, baseVert.trb.height
						
				
				
				return 1 # Working
				
			# We have a box that intersects that quadrent.
			elif overlapBox != None:  # None is used for a box thats alredt in the freq list.
				
				# There was an overlap, add this box to the verts list
				#quadFlagLs = (BLF,BRF,TLF,TRF)
				baseVert.intersectCache[flagIndex].append(overlapBox)
				
				
				
		return 0
		

class boxList:
	global packedVerts
	def __init__(self, boxes):
		self.boxes = boxes
		
		# keep a running update of the width and height so we know the area
		# initialize with first box, fixes but where we whwere only packing 1 box
		self.width = 0
		self.height = 0
		if len(boxes) > 0:
			for b in boxes:
				self.width = max(self.width, b.width)
				self.height = max(self.height, b.height)

		
		
		
		# boxArea is the total area of all boxes in the list,
		# can be used with packArea() to determine waistage.
		self.boxArea = 0 # incremented with addBox()
		
	
	# Just like MyBoxLs.boxes.append(), but sets bounds 
	def addBoxPack(self, box):
		
		
		# Resize this boxlist bounds for the current box.
		self.width = max(self.width, box.getRight())
		self.height = max(self.height, box.getTop())
		
		self.boxArea += box.area
		
		
		
		# iterate through these
		#~ quadFlagLs = (1,8,4,2) 
		#~ # Flags for vert idx used quads
		#~ BLF = 1; TRF = 2; TLF = 4; BRF = 8
		#~ quadFlagLs = (BLF,BRF,TLF,TRF)
		
		# Look through all the free vert quads and see if there are some we can remove 
		'''
		for v in box.v:
			
			# Is my bottom being used.
			
			if v.free & BLF and v.free & BRF: # BLF and BRF
				for b in self.boxes:
					if b.v[TR].y == v.y:
						if b.v[TR].x > v.x:
							if b.v[BL].x < v.x:
								v.free &= ~BLF # Removes quad
								v.free &= ~BRF # Removes quad
				
				# Is my left being used.
			if v.free & BLF and v.free & TLF:
				for b in self.boxes:
					if b.v[TR].x == v.x:
						if b.v[TR].y > v.y:
							if b.v[BL].y < v.y:
								v.free &= ~BLF # Removes quad
								v.free &= ~TLF # Removes quad
				
			if v.free & TRF and v.free & TLF:
				# Is my top being used.
				for b in self.boxes:
					if b.v[BL].y == v.y:
						if b.v[TR].x > v.x:
							if b.v[BL].x < v.x:
								v.free &= ~TLF # Removes quad
								v.free &= ~TRF # Removes quad
								
				
				# Is my right being used.
			if v.free & TRF and v.free & BRF:
				for b in self.boxes:
					if b.v[BL].x == v.x:
						if b.v[TR].y > v.y:
							if b.v[BL].y < v.y:
								v.free &= ~BRF # Removes quad
								v.free &= ~TRF # Removes quad
								
		'''
		self.boxes.append(box)
		
		
		
	# Just like MyBoxLs.boxes.append(), but sets bounds 
	def addBox(self, box):
		self.boxes.append(box)		
		self.boxArea += box.area

	# The area of the backing bounds.
	def packedArea(self):
		return self.width * self.height
		
	# Sort boxes by area
	# TODO REPLACE WITH SORT(LAMBDA(CMP...))
	def sortArea(self):
		self.boxes.sort(lambda A, B: cmp(B.area, A.area) ) # Reverse area sort
	
	# BLENDER only
	def draw(self):
		m = NMesh.GetRaw()
		
		
		for b in self.boxes:
			z = min(b.width, b.height ) / max(b.width, b.height )
			#z =  b.farea
			#z=0
			f = NMesh.Face()
			m.verts.append(NMesh.Vert(b.getLeft(), b.getBottom(), z))
			f.v.append(m.verts[-1])
			m.verts.append(NMesh.Vert(b.getRight(), b.getBottom(), z))
			f.v.append(m.verts[-1])		
			m.verts.append(NMesh.Vert(b.getRight(), b.getTop(), z))		
			f.v.append(m.verts[-1])
			m.verts.append(NMesh.Vert(b.getLeft(), b.getTop(), z))
			f.v.append(m.verts[-1])
			m.faces.append(f)
		NMesh.PutRaw(m, 's')	
		Window.Redraw(1)
	
	def pack(self):
		global packedVerts
		packedVerts = vertList()
		
		self.sortArea()
		
		if len(self.boxes) == 0:
			return
			
		packedboxes = boxList([self.boxes[0]])
		
		# Remove verts we KNOW cant be added to
		
		unpackedboxes = boxList(self.boxes[1:])
		
		# STart with this box
		packedVerts.verts.extend(packedboxes.boxes[0].v)
		
		while unpackedboxes.boxes != []:
			
			freeBoxIdx = 0
			while freeBoxIdx < len(unpackedboxes.boxes):
				
				# Sort the verts with this boxes dimensions as a bias, so less poky out bits are made.
				packedVerts.sortCorner(unpackedboxes.boxes[freeBoxIdx].width, unpackedboxes.boxes[freeBoxIdx].height)
				
				vertIdx = 0
				
				while vertIdx < len(packedVerts.verts):
					baseVert = packedVerts.verts[vertIdx]
					
					if baseVert.free != 0:
						# This will lock the box if its possibel
						if unpackedboxes.boxes[freeBoxIdx].tryVert(packedboxes, baseVert):
							packedboxes.addBoxPack(unpackedboxes.boxes[freeBoxIdx])
							unpackedboxes.boxes.pop(freeBoxIdx) 
							freeBoxIdx = -1
							break
						
					vertIdx +=1
				freeBoxIdx +=1
		self.width = packedboxes.width
		self.height = packedboxes.height
	# All boxes as a list - X/Y/WIDTH/HEIGHT
	def list(self):
		ls = []
		for b in self.boxes:
			ls.append( (b.id, b.getLeft(), b.getBottom(), b.width, b.height ) )
		return ls


''' Define all globals here '''
# vert IDX's, make references easier to understand.
BL = 0; TR = 1; TL = 2; BR = 3

# iterate through these
quadFlagLs = (1,8,4,2) 
# Flags for vert idx used quads
BLF = 1; TRF = 2; TLF = 4; BRF = 8
quadFlagLs = (BLF,BRF,TLF,TRF)


# Global vert pool, stores used lists
packedVerts = vertList()


# Packs a list w/h's into box types and places then #Iter times
def boxPackIter(boxLs, iter=1, draw=0):
	iterIdx = 0
	bestArea = None
	# Iterate over packing the boxes to get the best FIT!
	while iterIdx < iter:
		myBoxLs = boxList([])
		for b in boxLs:
			myBoxLs.addBox( box(b[1], b[2], b[0]) ) # w/h/id
		
		myBoxLs.pack()
		# myBoxLs.draw() # Draw as we go?
		
		newArea = myBoxLs.packedArea()
		
		#print 'pack test %s of %s, area:%.2f' % (iterIdx, iter, newArea)
		
		# First time?
		if bestArea == None:
			bestArea = newArea
			bestBoxLs = myBoxLs
		elif newArea < bestArea:
			bestArea = newArea
			bestBoxLs = myBoxLs
		iterIdx+=1
	
	
	if draw:
		bestBoxLs.draw()
	
	#print 'best area: %.4f, %.2f%% efficient' % (bestArea, (float(bestBoxLs.boxArea) / (bestArea+0.000001))*100)
	return bestBoxLs.width, bestBoxLs.height, bestBoxLs.list()
# END USER_BOX_PACK_MOD.py

# ==============================================================


# Box Packer is included for distrobution.
#import box_pack_mod
#reload(box_pack_mod)




# Do 2 lines intersect, if so where, DOSENT HANDLE HOZ/VERT LINES!!!

def lineIntersection2D(x1,y1, x2,y2, _x1,_y1, _x2,_y2):
	
	# Bounding box intersection first.
	if min(x1, x2) > max(_x1, _x2) or \
	max(x1, x2) < min(_x1, _x2) or \
	min(y1, y2) > max(_y1, _y2) or \
	max(y1, y2) < min(_y1, _y2):
		return None, None # BAsic Bounds intersection TEST returns false.
	
	# are either of the segments points? Check Seg1
	if abs(x1 - x2) + abs(y1 - y2) <= SMALL_NUM:
		return None, None
	
	# are either of the segments points? Check Seg2
	if abs(_x1 - _x2) + abs(_y1 - _y2) <= SMALL_NUM:
		return None, None
	
	
	# Make sure the HOZ/Vert Line Comes first.
	if abs(_x1 - _x2) < SMALL_NUM or abs(_y1 - _y2) < SMALL_NUM:
		x1, x2, y1, y2, _x1, _x2, _y1, _y2 = _x1, _x2, _y1, _y2, x1, x2, y1, y2


	if abs(x2-x1) < SMALL_NUM: # VERTICLE LINE
		if abs(_x2-_x1) < SMALL_NUM: # VERTICLE LINE SEG2
			return None, None # 2 verticle lines dont intersect.
		
		elif abs(_y2-_y1) < SMALL_NUM:
			return x1, _y1 # X of vert, Y of hoz. no calculation.		
		
		yi = ((_y1 / abs(_x1 - _x2)) * abs(_x2 - x1)) + ((_y2 / abs(_x1 - _x2)) * abs(_x1 - x1))
		
		if yi > max(y1, y2): # New point above seg1's vert line
			return None, None
		elif yi < min(y1, y2): # New point below seg1's vert line
			return None, None
			
		return x1, yi # Intersecting.
	
	if abs(y2-y1) < SMALL_NUM: # HOZ LINE
		if abs(_y2-_y1) < SMALL_NUM: # HOZ LINE SEG2
			return None, None # 2 hoz lines dont intersect.
		
		# Can skip vert line check for seg 2 since its covered above.	
		xi = ((_x1 / abs(_y1 - _y2)) * abs(_y2 - y1)) + ((_x2 / abs(_y1 - _y2)) * abs(_y1 - y1))
		if xi > max(x1, x2): # New point right of seg1's hoz line
			return None, None
		elif xi < min(x1, x2): # New point left of seg1's hoz line
			return None, None
			
		return xi, y1 # Intersecting.
		
	
	# ACCOUNTED FOR HOZ/VERT LINES. GO ON WITH BOTH ANGLULAR.
	
	b1 = (y2-y1)/(x2-x1)
	b2 = (_y2-_y1)/(_x2-_x1)
	a1 = y1-b1*x1
	a2 = _y1-b2*_x1
	
	if b1 - b2 == 0.0:
		return None, None	
	
	xi = - (a1-a2)/(b1-b2)
	yi = a1+b1*xi
	if (x1-xi)*(xi-x2) >= 0 and (_x1-xi)*(xi-_x2) >= 0 and (y1-yi)*(yi-y2) >= 0 and (_y1-yi)*(yi-_y2)>=0:
		return xi, yi
	else:
		return None, None

def triArea(p1, p2, p3):
	return CrossVecs(p1-p2, p3-p2).length/2
	
# IS a point inside our triangle?
'''
def pointInTri2D(PT, triP1, triP2, triP3):
	def triArea(p1, p2, p3):
		return CrossVecs(p1-p2, p3-p2).length /2
	area1 = triArea(PT, triP2, triP3)
	area2 = triArea(PT, triP1, triP3)
	area3 = triArea(PT, triP1, triP2)
	triArea = triArea(triP1, triP2, triP3)
	if area1 + area2 + area3 > triArea+0.01:
		return False
	else:
		return True
'''

dict_matrix = {}

def pointInTri2D(v, v1, v2, v3):
	global dict_matrix
	
	key = (v1.x, v1.y, v2.x, v2.y, v3.x, v3.y)
	
	try:
		mtx = dict_matrix[key]
		if not mtx:
			return False
	except:
		side1 = v2 - v1
		side2 = v3 - v1
		
		nor = Mathutils.CrossVecs(side1, side2)
		
		l1 = [side1[0], side1[1], side1[2]]
		l2 = [side2[0], side2[1], side2[2]]
		l3 = [nor[0], nor[1], nor[2]]
		
		mtx = Mathutils.Matrix(l1, l2, l3)
		
		# Zero area 2d tri, even tho we throw away zerop area faces
		# the projection UV can result in a zero area UV.
		if not mtx.determinant():
			dict_matrix[key] = None
			return False
		
		mtx.invert()
		
		dict_matrix[key] = mtx
	
	if NEW_2_38_MATHUTILS:
		uvw = (v - v1) * mtx
	else:
		uvw = Mathutils.VecMultMat(v - v1, mtx)
	
	return 0 <= uvw[0] and 0 <= uvw[1] and uvw[0] + uvw[1] <= 1



def faceArea(f):
	if len(f) == 3:
		return triArea(f.v[0].co, f.v[1].co, f.v[2].co)
	elif len(f) == 4:
		return\
		 triArea(f.v[0].co, f.v[1].co, f.v[2].co) +\
		 triArea(f.v[0].co, f.v[2].co, f.v[3].co)



	
def boundsIsland(faces):
	minx = maxx = faces[0].uv[0][0] # Set initial bounds.
	miny = maxy = faces[0].uv[0][1]
	# print len(faces), minx, maxx, miny , maxy
	for f in faces:
		for uv in f.uv:
			minx = min(minx, uv[0])
			maxx = max(maxx, uv[0])
			
			miny = min(miny, uv[1])
			maxy = max(maxy, uv[1])
	
	return minx, miny, maxx, maxy

def boundsEdgeLoop(edges):
	minx = maxx = edges[0][0] # Set initial bounds.
	miny = maxy = edges[0][1]
	# print len(faces), minx, maxx, miny , maxy
	for ed in edges:
		for pt in ed:
			minx = min(minx, pt[0])
			maxx = max(maxx, pt[0])
			
			miny = min(miny, pt[1])
			maxy = max(maxy, pt[1])
	
	return minx, miny, maxx, maxy


# Turns the islands into a list of unpordered edges (Non internal)
# Onlt for UV's

def island2Edge(island):
	# Vert index edges
	edges = {}
	
	for f in island:
		for vIdx in range(len(f)):
			if f.v[vIdx].index > f.v[vIdx-1].index:
				edges[((f.uv[vIdx-1][0], f.uv[vIdx-1][1]), (f.uv[vIdx][0], f.uv[vIdx][1]))] =\
				(Vector([f.uv[vIdx-1][0], f.uv[vIdx-1][1]]) - Vector([f.uv[vIdx][0], f.uv[vIdx][1]])).length
			else:
				edges[((f.uv[vIdx][0], f.uv[vIdx][1]), (f.uv[vIdx-1][0], f.uv[vIdx-1][1]) )] =\
				(Vector([f.uv[vIdx-1][0], f.uv[vIdx-1][1]]) - Vector([f.uv[vIdx][0], f.uv[vIdx][1]])).length
	
	# If 2 are the same then they will be together, but full [a,b] order is not correct.
	
	# Sort by length
	length_sorted_edges = []
	for key in edges.keys():
		length_sorted_edges.append([key[0], key[1], edges[key]])
	
	length_sorted_edges.sort(lambda A, B: cmp(B[2], A[2]))
	#for e in length_sorted_edges:
	#	e.pop(2)
	
	return length_sorted_edges
	
# ========================= NOT WORKING????
# Find if a points inside an edge loop, un-orderd.
# pt is and x/y
# edges are a non ordered loop of edges.
# #offsets are the edge x and y offset.
def pointInEdges(pt, edges):
	#
	x1 = pt[0] 
	y1 = pt[1]
	
	# Point to the left of this line.
	x2 = -100000
	y2 = -10000
	intersectCount = 0
	for ed in edges:
		xi, yi = lineIntersection2D(x1,y1, x2,y2, ed[0][0], ed[0][1], ed[1][0], ed[1][1])
		if xi != None: # Is there an intersection.
			intersectCount+=1
	
	return intersectCount % 2
	

def uniqueEdgePairPoints(edges):
	points = {}
	pointsVec = []
	for e in edges:
		points[e[0]] = points[e[1]] = None
		
	for p in points.keys():
		pointsVec.append( Vector([p[0], p[1], 0])  )
	return pointsVec
	

def pointInIsland(pt, island):
	vec1 = Vector(); vec2 = Vector(); vec3 = Vector()	
	for f in island:
		vec1.x, vec1.y = f.uv[0]
		vec2.x, vec2.y = f.uv[1]
		vec3.x, vec3.y = f.uv[2]

		if pointInTri2D(pt, vec1, vec2, vec3):
			return True
		
		if len(f) == 4:
			vec1.x, vec1.y = f.uv[0]
			vec2.x, vec2.y = f.uv[2]
			vec3.x, vec3.y = f.uv[3]			
			if pointInTri2D(pt, vec1, vec2, vec3):
				return True
	return False


# box is (left,bottom, right, top)
def islandIntersectUvIsland(source, target, xSourceOffset, ySourceOffset):
	# Is 1 point in the box, inside the vertLoops
	edgeLoopsSource = source[6] # Pretend this is offset
	edgeLoopsTarget = target[6]
	

	
	# Edge intersect test	
	for ed in edgeLoopsSource:
		for seg in edgeLoopsTarget:
			xi, yi = lineIntersection2D(\
			seg[0][0], seg[0][1], seg[1][0], seg[1][1],\
			xSourceOffset+ed[0][0], ySourceOffset+ed[0][1], xSourceOffset+ed[1][0], ySourceOffset+ed[1][1])
			if xi != None:
				return 1 # LINE INTERSECTION
	
	# 1 test for source being totally inside target
	for pv in source[7]:
		if NEW_2_38_MATHUTILS:
			p = Vector(pv)
		else:
			p = CopyVec(pv)
		
		p.x += xSourceOffset
		p.y += ySourceOffset		
		if pointInIsland(p, target[0]):
			return 2 # SOURCE INSIDE TARGET
	
	# 2 test for a part of the target being totaly inside the source.
	for pv in target[7]:
		
		if NEW_2_38_MATHUTILS:
			p = Vector(pv)
		else:
			p = CopyVec(pv)
		
		p.x -= xSourceOffset
		p.y -= ySourceOffset
		if pointInIsland(p, source[0]):
			return 3 # PART OF TARGET INSIDE SOURCE.

	return 0 # NO INTERSECTION




# Returns the X/y Bounds of a list of vectors.
def testNewVecLs2DRotIsBetter(vecs, mat=-1, bestAreaSoFar = -1):
	
	# UV's will never extend this far.
	minx = miny = BIG_NUM
	maxx = maxy = -BIG_NUM
	
	for i, v in enumerate(vecs):
		
		# Do this allong the way
		if mat != -1:
			# 2.37 depricated
			if NEW_2_38_MATHUTILS:
				v = vecs[i] = v*mat
			else:
				v = vecs[i] = VecMultMat(v, mat)
		
		minx = min(minx, v.x)
		maxx = max(maxx, v.x)
		
		miny = min(miny, v.y)
		maxy = max(maxy, v.y)
		
		# Spesific to this algo, bail out if we get bigger then the current area
		if bestAreaSoFar != -1 and (maxx-minx) * (maxy-miny) > bestAreaSoFar:
			return (BIG_NUM, None), None
	w = maxx-minx
	h = maxy-miny
	return (w*h, w,h), vecs # Area, vecs
	
# Takes a list of faces that make up a UV island and rotate
# until they optimally fit inside a square.
ROTMAT_2D_POS_90D = RotationMatrix( 90, 2)
ROTMAT_2D_POS_45D = RotationMatrix( 45, 2)

RotMatStepRotation = []
rot_angle = 22.5 #45.0/2
while rot_angle > 0.1:
	RotMatStepRotation.append([\
	 RotationMatrix( rot_angle, 2),\
	 RotationMatrix( -rot_angle, 2)])
	
	rot_angle = rot_angle/2.0
	

def optiRotateUvIsland(faces):
	global currentArea
	
	# Bestfit Rotation
	def best2dRotation(uvVecs, MAT1, MAT2):
		global currentArea
		
		newAreaPos, newfaceProjectionGroupListPos =\
		testNewVecLs2DRotIsBetter(uvVecs[:], MAT1, currentArea[0])
		
		
		# Why do I use newpos here? May as well give the best area to date for an early bailout
		# some slight speed increase in this.
		# If the new rotation is smaller then the existing, we can 
		# avoid copying a list and overwrite the old, crappy one.
		
		if newAreaPos[0] < currentArea[0]:
			newAreaNeg, newfaceProjectionGroupListNeg =\
			testNewVecLs2DRotIsBetter(uvVecs, MAT2, newAreaPos[0])  # Reuse the old bigger list.
		else:
			newAreaNeg, newfaceProjectionGroupListNeg =\
			testNewVecLs2DRotIsBetter(uvVecs[:], MAT2, currentArea[0])  # Cant reuse, make a copy.
		
		
		# Now from the 3 options we need to discover which to use
		# we have cerrentArea/newAreaPos/newAreaNeg
		bestArea = min(currentArea[0], newAreaPos[0], newAreaNeg[0])
		
		if currentArea[0] == bestArea:
			return uvVecs
		elif newAreaPos[0] == bestArea:
			uvVecs = newfaceProjectionGroupListPos
			currentArea = newAreaPos		
		elif newAreaNeg[0] == bestArea:
			uvVecs = newfaceProjectionGroupListNeg
			currentArea = newAreaNeg
		
		return uvVecs
		
	
	# Serialized UV coords to Vectors
	uvVecs = [Vector(uv[:2]) for f in faces  for uv in f.uv]
	
	# Theres a small enough number of these to hard code it
	# rather then a loop.
	
	# Will not modify anything
	currentArea, dummy =\
	testNewVecLs2DRotIsBetter(uvVecs)
	
	
	# Try a 45d rotation
	newAreaPos, newfaceProjectionGroupListPos = testNewVecLs2DRotIsBetter(uvVecs[:], ROTMAT_2D_POS_45D, currentArea[0])
	
	if newAreaPos[0] < currentArea[0]:
		uvVecs = newfaceProjectionGroupListPos
		currentArea = newAreaPos
	# 45d done
	
	# Testcase different rotations and find the onfe that best fits in a square
	for ROTMAT in RotMatStepRotation:
		uvVecs = best2dRotation(uvVecs, ROTMAT[0], ROTMAT[1])
	
	
	# Only if you want it, make faces verticle!
	if currentArea[1] > currentArea[2]:
		# Rotate 90d
		# Work directly on the list, no need to return a value.
		testNewVecLs2DRotIsBetter(uvVecs, ROTMAT_2D_POS_90D)
		
	
	
	
	# Now write the vectors back to the face UV's
	i = 0 # count the serialized uv/vectors
	for f in faces:
		f.uv = [uv for uv in uvVecs[i:len(f)+i] ]
		i += len(f)


# Takes an island list and tries to find concave, hollow areas to pack smaller islands into.
def mergeUvIslands(islandList, islandListArea):
	global USER_FILL_HOLES
	global USER_FILL_HOLES_QUALITY
	
	# Pack islands to bottom LHS
	# Sync with island
	
	#islandTotFaceArea = [] # A list of floats, each island area
	#islandArea = [] # a list of tuples ( area, w,h)
	
	
	decoratedIslandList = []
	
	islandIdx = len(islandList)
	while islandIdx:
		islandIdx-=1
		minx, miny, maxx, maxy = boundsIsland(islandList[islandIdx])
		w, h = maxx-minx, maxy-miny
		
		totFaceArea = 0
		fIdx = len(islandList[islandIdx])
		while fIdx:
			fIdx-=1
			f = islandList[islandIdx][fIdx]
			f.uv = [(uv[0]-minx, uv[1]-miny) for uv in f.uv]
			totFaceArea += islandListArea[islandIdx][fIdx] # Use Cached area. dont recalculate.
		islandBoundsArea = w*h
		efficiency = abs(islandBoundsArea - totFaceArea)
		
		# UV Edge list used for intersections
		edges = island2Edge(islandList[islandIdx])
		
		
		uniqueEdgePoints = uniqueEdgePairPoints(edges)
		
		decoratedIslandList.append([islandList[islandIdx], totFaceArea, efficiency, islandBoundsArea, w,h, edges, uniqueEdgePoints]) 
		
	
	# Sort by island bounding box area, smallest face area first.
	# no.. chance that to most simple edge loop first.
	decoratedIslandListAreaSort =decoratedIslandList[:]
	decoratedIslandListAreaSort.sort(lambda A, B: cmp(A[1], B[1]))
	
	# sort by efficiency, Most Efficient first.
	decoratedIslandListEfficSort = decoratedIslandList[:]
	decoratedIslandListEfficSort.sort(lambda A, B: cmp(B[2], A[2]))
	
	# ================================================== THESE CAN BE TWEAKED.
	# This is a quality value for the number of tests.
	# from 1 to 4, generic quality value is from 1 to 100
	USER_STEP_QUALITY =   ((USER_FILL_HOLES_QUALITY - 1) / 25.0) + 1
	
	# If 100 will test as long as there is enough free space.
	# this is rarely enough, and testing takes a while, so lower quality speeds this up.
	
	# 1 means they have the same quaklity 
	USER_FREE_SPACE_TO_TEST_QUALITY = 1 + (((100 - USER_FILL_HOLES_QUALITY)/100.0) *5)
	
	#print 'USER_STEP_QUALITY', USER_STEP_QUALITY
	#print 'USER_FREE_SPACE_TO_TEST_QUALITY', USER_FREE_SPACE_TO_TEST_QUALITY
	
	removedCount = 0
	
	areaIslandIdx = 0
	ctrl = Window.Qual.CTRL
	while areaIslandIdx < len(decoratedIslandListAreaSort) and (not Window.GetKeyQualifiers() & ctrl):
		sourceIsland = decoratedIslandListAreaSort[areaIslandIdx]
		
		# Alredy packed?
		if not sourceIsland[0]:
			areaIslandIdx+=1
		else:
			efficIslandIdx = 0
			while efficIslandIdx < len(decoratedIslandListEfficSort):
				
				# Now we have 2 islands, is the efficience of the islands lowers theres an
				# increasing likely hood that we can fit merge into the bigger UV island.
				# this ensures a tight fit.
				
				# Just use figures we have about user/unused area to see if they might fit.
				
				targetIsland = decoratedIslandListEfficSort[efficIslandIdx]
				
				
				if sourceIsland[0] == targetIsland[0] or\
				targetIsland[0] == [] or\
				sourceIsland[0] == []:
					efficIslandIdx+=1
					continue
				
				
				# ([island, totFaceArea, efficiency, islandArea, w,h])
				# Waisted space on target is greater then UV bounding island area.
				
				
				# if targetIsland[3] > (sourceIsland[2]) and\ #
				
				if targetIsland[3] > (sourceIsland[1] * USER_FREE_SPACE_TO_TEST_QUALITY) and\
				targetIsland[4] > sourceIsland[4] and\
				targetIsland[5] > sourceIsland[5]:
					
					
					# DEBUG # print '%.10f  %.10f' % (targetIsland[3], sourceIsland[1])
					
					# These enough spare space lets move the box until it fits
					
					# How many times does the source fit into the target x/y
					blockTestXUnit = targetIsland[4]/sourceIsland[4]
					blockTestYUnit = targetIsland[5]/sourceIsland[5]
					
					boxLeft = 0
					
					# Distance we can move between whilst staying inside the targets bounds.
					testWidth = targetIsland[4] - sourceIsland[4]
					testHeight = targetIsland[5] - sourceIsland[5]
					
					# Increment we move each test. x/y
					xIncrement = (testWidth / (blockTestXUnit * USER_STEP_QUALITY))
					yIncrement = (testHeight / (blockTestYUnit * USER_STEP_QUALITY))
					
					boxLeft = 0 # Start 1 back so we can jump into the loop.
					boxBottom= 0 #-yIncrement
					
					while boxLeft <= testWidth or boxBottom <= testHeight:
						
						
						Intersect = islandIntersectUvIsland(sourceIsland, targetIsland, boxLeft, boxBottom)
						
						if Intersect == 1:  # Line intersect, dont bother with this any more
							pass
						
						if Intersect == 2:  # Source inside target
							'''
							We have an intersection, if we are inside the target 
							then move us 1 whole width accross,
							Its possible this is a bad idea since 2 skinny Angular faces
							could join without 1 whole move, but its a lot more optimal to speed this up
							since we have alredy tested for it.
							
							It gives about 10% speedup with minimal errors.
							'''
							# Move the test allong its width + SMALL_NUM
							boxLeft += sourceIsland[4] + SMALL_NUM
							#py_sys.stdout.write('>')
							#pass
						elif Intersect == 0: # No intersection?? Place it.
							# Progress
							removedCount +=1
							Window.DrawProgressBar(0.0, 'Merged: %i islands, Ctrl to finish early.' % removedCount)
							
							'''
							if py_sys:
								py_sys.stdout.write('#')
								py_sys.stdout.flush()
							else:
								print '#'
							'''
							
							# Move faces into new island and offset
							targetIsland[0].extend(sourceIsland[0])
							while sourceIsland[0]:
								f = sourceIsland[0].pop()
								f.uv = [(uv[0]+boxLeft, uv[1]+boxBottom) for uv in f.uv]

							# Move edge loop into new and offset.
							# targetIsland[6].extend(sourceIsland[6])
							while sourceIsland[6]:
								e = sourceIsland[6].pop()
								targetIsland[6].append(\
								 ((e[0][0]+boxLeft, e[0][1]+boxBottom),\
								 (e[1][0]+boxLeft, e[1][1]+boxBottom), e[2])\
								)
							
							# Sort by edge length, reverse so biggest are first.
							targetIsland[6].sort(lambda B,A: cmp(A[2], B[2] ))
							
							targetIsland[7].extend(sourceIsland[7])
							while sourceIsland[7]:
								p = sourceIsland[7].pop()
								p.x += boxLeft; p.y += boxBottom
							
							
							# Decrement the efficiency
							targetIsland[1]+=sourceIsland[1] # Increment totFaceArea
							targetIsland[2]-=sourceIsland[1] # Decrement efficiency
							# IF we ever used these again, should set to 0, eg
							sourceIsland[2] = 0 # No area is anyone wants to know
							
							break
						
						
						# INCREMENR NEXT LOCATION
						if boxLeft > testWidth:
							boxBottom += yIncrement
							boxLeft = 0.0
						else:
							boxLeft += xIncrement
						
							
				efficIslandIdx+=1
		areaIslandIdx+=1
	
	# Remove empty islands
	# removedCount = 0
	i = len(islandList)
	while i:
		i-=1
		if not islandList[i]:
			islandList.pop(i)
			# removedCount+=1
	
	# Dont need to return anything
	# if py_sys: py_sys.stdout.flush()
	
	# print ''
	# print removedCount, 'merged'
	
# Takes groups of faces. assumes face groups are UV groups.
def packLinkedUvs(faceGroups, faceGroupsArea, me):
	islandList = []
	islandListArea = []
	
	Window.DrawProgressBar(0.0, 'Splitting %d projection groups into UV islands:' % len(faceGroups))
	#print '\tSplitting %d projection groups into UV islands:' % len(faceGroups),
	# Find grouped faces
	
	faceGroupIdx = len(faceGroups)
	
	while faceGroupIdx:
		faceGroupIdx-=1
		faces = faceGroups[faceGroupIdx]
		facesArea = faceGroupsArea[faceGroupIdx]
		# print '.',
		
		faceUsers = [[] for i in xrange(len(me.verts)) ]
		faceUsersArea = [[] for i in xrange(len(me.verts)) ]
		# Do the first face
		fIdx = len(faces)
		while fIdx:
			fIdx-=1
			for v in faces[fIdx].v:
				faceUsers[v.index].append(faces[fIdx])
				faceUsersArea[v.index].append(facesArea[fIdx])
				
		
		while 1:			
			
			# This is an index that is used to remember
			# what was the last face that was removed, so we know which faces are new and need to have 
			# faces next to them added into the list
			searchFaceIndex = 0
			
			# Find a face that hasnt been used alredy to start the search with
			newIsland = []
			newIslandArea = []
			while not newIsland:
				hasBeenUsed = 1 # Assume its been used.
				if searchFaceIndex >= len(faces):
					break
				for v in faces[searchFaceIndex].v:
					if faces[searchFaceIndex] in faceUsers[v.index]:
						# This has not yet been used, it still being used by a vert
						hasBeenUsed = 0
						break
				if hasBeenUsed == 0:
					newIsland.append(faces.pop(searchFaceIndex))
					newIslandArea.append(facesArea.pop(searchFaceIndex))
				
				searchFaceIndex+=1

			if newIsland == []:
				break
			
			
			# Before we start remove the first, search face from being used.
			for v in newIsland[0].v:
				popoffset = 0
				for fIdx in xrange(len(faceUsers[v.index])):
					if faceUsers[v.index][fIdx - popoffset] == newIsland[0]:						
						faceUsers[v.index].pop(fIdx - popoffset)
						faceUsersArea[v.index].pop(fIdx - popoffset)
						
						popoffset += 1
			
			searchFaceIndex = 0
			while searchFaceIndex != len(newIsland):
				for v in newIsland[searchFaceIndex].v:
					
					# Loop through all faces that use this vert
					while faceUsers[v.index]:
						sharedFace = faceUsers[v.index][-1]
						sharedFaceArea = faceUsersArea[v.index][-1]
						
						newIsland.append(sharedFace)
						newIslandArea.append(sharedFaceArea)
						# Before we start remove the first, search face from being used.
						for vv in sharedFace.v:
							#faceUsers = [f for f in faceUsers[vv.index] if f != sharedFace]
							fIdx = 0
							for fIdx in xrange(len(faceUsers[vv.index])):
								if faceUsers[vv.index][fIdx] == sharedFace:
									faceUsers[vv.index].pop(fIdx)
									faceUsersArea[vv.index].pop(fIdx)
									break # Can only be used once.
				
				searchFaceIndex += 1
				
				# If all the faces are done and no face has been added then we can quit
			if newIsland:
				islandList.append(newIsland)
				
				islandListArea.append(newIslandArea)
			
			else:
				print '\t(empty island found, ignoring)'
			
	

	
	
	#Window.DrawProgressBar(0.1, 'Found %i UV Islands' % len(islandList))
	#print '\n\tFound %i UV Islands' % len(islandList)
	
	#print '\tOptimizing Island Rotation...'
	
	Window.DrawProgressBar(0.1, 'Optimizing Rotation for %i UV Islands' % len(islandList))
	
	for island in islandList:
		optiRotateUvIsland(island)
	
	
	
	if USER_FILL_HOLES:
		Window.DrawProgressBar(0.1, 'Merging Islands...')
		#print '\tMerging islands to save space ("#" == one merge):\n',
		if py_sys: py_sys.stdout.flush()
		mergeUvIslands(islandList, islandListArea) # Modify in place
		
	
	# Now we have UV islands, we need to pack them.
	
	# Make a synchronised list with the islands
	# so we can box pak the islands.
	boxes2Pack = []
	
	# Keep a list of X/Y offset so we can save time by writing the 
	# uv's and packed data in one pass.
	islandOffsetList = [] 
	
	islandIdx = 0
	
	while islandIdx < len(islandList):
		minx, miny, maxx, maxy = boundsIsland(islandList[islandIdx])
		w, h = maxx-minx, maxy-miny
		
		if w < 0.00001 or h < 0.00001:
			del islandList[islandIdx]
			islandIdx -=1
			continue
		
		'''Save the offset to be applied later,
		we could apply to the UVs now and allign them to the bottom left hand area
		of the UV coords like the box packer imagines they are
		but, its quicker just to remember their offset and
		apply the packing and offset in 1 pass '''
		islandOffsetList.append((minx, miny))
		
		# Add to boxList. use the island idx for the BOX id.
		boxes2Pack.append([islandIdx, w,h])
		islandIdx+=1
		
	# Now we have a list of boxes to pack that syncs
	# with the islands.
	
	#print '\tPacking UV Islands...'
	Window.DrawProgressBar(0.7, 'Packing %i UV Islands...' % len(boxes2Pack) )
	
	time1 = sys.time()
	packWidth, packHeight, packedLs = boxPackIter(boxes2Pack)
	# print 'Box Packing Time:', sys.time() - time1
	
	#if len(pa	ckedLs) != len(islandList):
	#	raise "Error packed boxes differes from original length"
	
	#print '\tWriting Packed Data to faces'
	Window.DrawProgressBar(0.8, 'Writing Packed Data to faces')
	packedLs.sort(lambda A, B: cmp(A[0] , B[0])) # Sort by ID, so there in sync again
	
	islandIdx = len(islandList)
	# Having these here avoids devide by 0
	if islandIdx:
		xfactor = 1.0 / packWidth
		yfactor = 1.0 / packHeight	
	
	while islandIdx:
		islandIdx -=1
		# Write the packed values to the UV's
		
		
		xoffset = packedLs[islandIdx][1] - islandOffsetList[islandIdx][0]
		yoffset = packedLs[islandIdx][2] - islandOffsetList[islandIdx][1]
		for f in islandList[islandIdx]: # Offsetting the UV's so they fit in there packed box
			f.uv = [(((uv[0]+xoffset)*xfactor), ((uv[1]+yoffset)*yfactor)) for uv in f.uv]

def VectoMat(vec):
	
	if NEW_2_38_MATHUTILS:
		a3 = Vector(vec)
	else:
		a3 = CopyVec(vec)
	
	a3.normalize()
	
	up = Vector([0,0,1])
	if abs(DotVecs(a3, up)) == 1.0:
		up = Vector([0,1,0])
	
	a1 = CrossVecs(a3, up)
	a1.normalize()
	a2 = CrossVecs(a3, a1)
	return Matrix([a1[0], a1[1], a1[2]], [a2[0], a2[1], a2[2]], [a3[0], a3[1], a3[2]])
	
	
global ob
ob = None
def main():
	global USER_FILL_HOLES
	global USER_FILL_HOLES_QUALITY
	try:
		obList =  Object.GetSelected()
		
	except:
		Draw.PupMenu('error, no selected objects or mesh')
		return
	
	USER_FILL_HOLES = Draw.PupMenu('ArchiMap UV Unwrapper%t|Fill in holes (space efficient, slow)%x1|No Filling (waste space, fast)%x0')
	if USER_FILL_HOLES == -1:
		return

	USER_ONLY_SELECTED_FACES = Draw.PupMenu('Faces to Unwrap%t|Only Selected%x1|Unwrap All%x0')
	if USER_ONLY_SELECTED_FACES == -1:
		return
	
	if USER_FILL_HOLES:
		USER_FILL_HOLES_QUALITY = Draw.PupIntInput('compression: ', 50, 1, 100)
		if USER_FILL_HOLES_QUALITY == None:
			return
			
	USER_PROJECTION_LIMIT = Draw.PupIntInput('angle limit:', 66, 1, 89)
	if USER_PROJECTION_LIMIT == None:
		return
	
	
	# Toggle Edit mode
	if Window.EditMode():
		Window.EditMode(0)
	
	Window.WaitCursor(1)
	
	time1 = sys.time()
	for ob in obList:
		
		# Only meshes
		if ob.getType() != 'Mesh':
			continue
		
		me = ob.getData()
		if USER_ONLY_SELECTED_FACES:
			meshFaces = [f for f in me.getSelectedFaces() if len(f) > 2]
		else:
			meshFaces = [f for f in me.faces if len(f) > 2]
		
		if not meshFaces:
			continue
		
		#print '\n\n\nArchimap UV Unwrapper, mapping "%s", %i faces.' % (me.name, len(meshFaces))
		Window.DrawProgressBar(0.1, 'Archimap UV Unwrapper, mapping "%s", %i faces.' % (me.name, len(meshFaces)))
		
		# Generate Projection
		projectVecs = [] # We add to this allong the way
		
		# =======
		# Generate a projection list from face normals, this is ment to be smart :)
		
		# make a list of face props that are in sync with meshFaces		
		# Make a Face List that is sorted by area.
		faceListProps = []		
		
		for f in meshFaces:
			area = faceArea(f)
			if area <= SMALL_NUM:
				f.uv = [(0.0, 0.0)] * len(f)
				print 'found zero area face, removing.'
				
			else:
				# Store all here
				n = f.no
				faceListProps.append( [f, area, Vector(n)] )
		
		del meshFaces
		
		faceListProps.sort( lambda A, B: cmp(B[1] , A[1]) ) # Biggest first.
		# Smallest first is slightly more efficient, but if the user cancels early then its better we work on the larger data.
		
		# Generate Projection Vecs
		# 0d is   1.0
		# 180 IS -0.59846
		
		USER_PROJECTION_LIMIT_CONVERTED = cos(USER_PROJECTION_LIMIT * DEG_TO_RAD)
		#print  USER_PROJECTION_LIMIT_CONVERTED
		USER_PROJECTION_LIMIT_HALF_CONVERTED = cos((USER_PROJECTION_LIMIT/2) * DEG_TO_RAD)
		
		# Initialize projectVecs
		newProjectVec = faceListProps[0][2] 
		newProjectFacePropList = [faceListProps[0]]	# Popping stuffs it up.
		
		# Predent that the most unique angke is ages away to start the loop off
		mostUniqueAngle = -1.0
		
		# This is popped
		tempFaceListProps = faceListProps[:]
		
		while 1:
			# If theres none there then start with the largest face
			
			# Pick the face thats most different to all existing angles :)
			mostUniqueAngle = 1.0 # 1.0 is 0d. no difference.
			mostUniqueIndex = 0 # fake
			
			fIdx = len(tempFaceListProps)
			while fIdx:
				fIdx-=1
				angleDifference = -1.0 # 180d difference.
				
				# Get the closest vec angle we are to.
				for p in projectVecs:
					angleDifference = max(angleDifference, DotVecs(p, tempFaceListProps[fIdx][2]))
				
				if angleDifference < mostUniqueAngle:
					# We have a new most different angle
					mostUniqueIndex = fIdx
					mostUniqueAngle = angleDifference
			
			
			if mostUniqueAngle < USER_PROJECTION_LIMIT_CONVERTED:
				#print 'adding', mostUniqueAngle, USER_PROJECTION_LIMIT, len(newProjectFacePropList)
				newProjectVec = tempFaceListProps[mostUniqueIndex][2]
				newProjectFacePropList = [tempFaceListProps.pop(mostUniqueIndex)]				
			else:
				if len(projectVecs) >= 1: # Must have at least 2 projections
					break
			
			
			# Now we have found the most different vector, add all the faces that are close.
			fIdx = len(tempFaceListProps)
			while fIdx:
				fIdx -= 1
				
				# Use half the angle limit so we dont overweight faces towards this
				# normal and hog all the faces.
				if DotVecs(newProjectVec, tempFaceListProps[fIdx][2]) > USER_PROJECTION_LIMIT_HALF_CONVERTED:
					newProjectFacePropList.append(tempFaceListProps.pop(fIdx))
			
			
			# Now weight the vector to all its faces, will give a more direct projection
			# if the face its self was not representive of the normal from surrounding faces.
			averageVec = Vector([0,0,0])
			for fprop in newProjectFacePropList:
				averageVec += (fprop[2] * fprop[1]) # / len(newProjectFacePropList)
			
			if averageVec.x != 0 or averageVec.y != 0 or averageVec.z != 0: # Avoid NAN
				averageVec.normalize()				
				projectVecs.append(averageVec)
			
			# Now we have used it, ignore it.
			newProjectFacePropList = []
			
		# If there are only zero area faces then its possible
		# there are no projectionVecs
		if not len(projectVecs):
			Draw.PupMenu('error, no projection vecs where generated, 0 area faces can cause this.')
			return
		
		faceProjectionGroupList =[[] for i in xrange(len(projectVecs)) ]
		faceProjectionGroupListArea =[[] for i in xrange(len(projectVecs)) ]
		
		# We need the area later, and we alredy have calculated it. so store it here.
		#faceProjectionGroupListArea =[[] for i in xrange(len(projectVecs)) ]
		
		# MAP and Arrange # We know there are 3 or 4 faces here 
		fIdx = len(faceListProps)
		while fIdx:
			fIdx-=1
			fvec = Vector(faceListProps[fIdx][2])
			i = len(projectVecs)
			
			# Initialize first
			
			bestAng = DotVecs(fvec, projectVecs[0])
			# print bestAng
			bestAngIdx = 0
			
			# Cycle through the remaining, first alredy done
			while i-1:
				i-=1
				
				newAng = DotVecs(fvec, projectVecs[i])
				if newAng > bestAng: # Reverse logic for dotvecs
					bestAng = newAng
					bestAngIdx = i
			
			# Store the area for later use.
			faceProjectionGroupList[bestAngIdx].append(faceListProps[fIdx][0])
			faceProjectionGroupListArea[bestAngIdx].append(faceListProps[fIdx][1])
			
		
		# Cull faceProjectionGroupList,
		
		
		# Now faceProjectionGroupList is full of faces that face match the project Vecs list
		i= len(projectVecs)
		while i:
			i-=1
			
			# Account for projectVecs having no faces.
			if not faceProjectionGroupList[i]:
				continue
					
			# Make a projection matrix from a unit length vector.
			MatProj = VectoMat(projectVecs[i])
			
			# Get the faces UV's from the projected vertex.
			for f in faceProjectionGroupList[i]:
				
				if NEW_2_38_MATHUTILS:
					f.uv = [MatProj * v.co for v in f.v]
				else:
					f.uv = [MatMultVec(MatProj, v.co) for v in f.v]
		
		packLinkedUvs(faceProjectionGroupList, faceProjectionGroupListArea, me)
		
		#print "ArchiMap time: %.2f" % (sys.time() - time1)
		Window.DrawProgressBar(0.9, "ArchiMap Done, time: %.2f sec." % (sys.time() - time1))
		
		# Update and dont mess with edge data.
		me.update(0, (me.edges != []), 0)
	Window.RedrawAll()


try:
	main()
except KeyboardInterrupt:
	print '\nUser Canceled.'
	Draw.PupMenu('user canceled execution, unwrap aborted.')
	
Window.WaitCursor(0)
