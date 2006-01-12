'''
# 2D Box packing function used by archimap
# packs any list of 2d boxes into a square and returns a list of packed boxes.
# Example of usage.
import boxpack2d

# Build boxe list.
# the unique ID is not used.
# just the width and height.
boxes2Pack = []
anyUniqueID = 0; w = 2.2; h = 3.8
boxes2Pack.append([anyUniqueID, w,h])
anyUniqueID = 1; w = 4.1; h = 1.2
boxes2Pack.append([anyUniqueID, w,h])
anyUniqueID = 2; w = 5.2; h = 9.2
boxes2Pack.append([anyUniqueID, w,h])
anyUniqueID = 3; w = 8.3; h = 7.3
boxes2Pack.append([anyUniqueID, w,h])
anyUniqueID = 4; w = 1.1; h = 5.1
boxes2Pack.append([anyUniqueID, w,h])
anyUniqueID = 5; w = 2.9; h = 8.1
boxes2Pack.append([anyUniqueID, w,h])
anyUniqueID = 6; w = 4.2; h = 6.2
boxes2Pack.append([anyUniqueID, w,h])
# packedLs is a list of [(anyUniqueID, left, bottom, width, height)...]
packWidth, packHeight, packedLs = boxpack2d.boxPackIter(boxes2Pack)
'''

from Blender import NMesh, Window

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
	def __init__(self, width, height, id=None):
		
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
						boxList.packedVerts.verts.append(self_v)
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
	# Global vert pool, stores used lists
	packedVerts = vertList() # will be vertList()
	
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
		# buggy but dont know why???, dont use.
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
		self.sortArea()
		
		if len(self.boxes) == 0:
			return
			
		packedboxes = boxList([self.boxes[0]])
		
		# Remove verts we KNOW cant be added to
		
		unpackedboxes = boxList(self.boxes[1:])
		
		# STart with this box
		boxList.packedVerts.verts.extend(packedboxes.boxes[0].v)
		
		while unpackedboxes.boxes != []:
			
			freeBoxIdx = 0
			while freeBoxIdx < len(unpackedboxes.boxes):
				
				# Sort the verts with this boxes dimensions as a bias, so less poky out bits are made.
				boxList.packedVerts.sortCorner(unpackedboxes.boxes[freeBoxIdx].width, unpackedboxes.boxes[freeBoxIdx].height)
				
				vertIdx = 0
				
				while vertIdx < len(boxList.packedVerts.verts):
					baseVert = boxList.packedVerts.verts[vertIdx]
					
					if baseVert.free != 0:
						# This will lock the box if its possibel
						if unpackedboxes.boxes[freeBoxIdx].tryVert(packedboxes, baseVert):
							packedboxes.addBoxPack(unpackedboxes.boxes[freeBoxIdx])
							unpackedboxes.boxes.pop(freeBoxIdx) 
							freeBoxIdx = -1
							break
						
					vertIdx +=1
				freeBoxIdx +=1
				
		boxList.packedVerts.verts = [] # Free the list, so it dosent use ram between runs.
		
		self.width = packedboxes.width
		self.height = packedboxes.height
	# All boxes as a list - X/Y/WIDTH/HEIGHT
	def list(self):
		return [(b.id, b.getLeft(), b.getBottom(), b.width, b.height ) for b in self.boxes]


''' Define all globals here '''
# vert IDX's, make references easier to understand.
BL = 0; TR = 1; TL = 2; BR = 3

# iterate through these
# Flags for vert idx used quads
BLF = 1; TRF = 2; TLF = 4; BRF = 8
quadFlagLs = (BLF,BRF,TLF,TRF)


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