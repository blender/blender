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

from Blender import NMesh, Window,   	Object, Scene
'''
def debug_(x,y,z):
	ob = Object.New("Empty")
	ob.loc= x,y,z
	Scene.GetCurrent().link(ob)
'''

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
	
	def sortCorner(self,w,h):
		'''
		Sorts closest first. - uses the box w/h as a bias,
		this makes it so its less likely to have lots of poking out bits
		that use too much 
		Lambada based sort
		'''
		# self.verts.sort(lambda A, B: cmp(max(A.x+w, A.y+h) , max(B.x+w, B.y+h))) # Reverse area sort
		self.verts.sort(key = lambda b: max(b.x+w, b.y+h) ) # Reverse area sort
		

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
		self.v=v= [vt(0,0), vt(width,height), vt(0,height), vt(width,0)]
		
		# Set the interior quadrents as used.
		v[0].free &= ~TRF
		v[1].free &= ~BLF
		v[2].free &= ~BRF
		v[3].free &= ~TLF
		
		#for v in self.v:
		#	v.users.append(self)
		v[0].trb = self
		v[1].blb = self
		v[2].brb = self
		v[3].tlb = self
		
		
	def updateV34(self):
		'''
		Updates verts 3 & 4 from 1 and 2
		since 3 and 4 are only there foill need is resizing/ rotating of patterns on the fly while I painr new box placement
		but may be merged later with other verts
		'''	
		self.v[TL].x = self.v[BL].x
		self.v[TL].y = self.v[TR].y
		
		self.v[BR].x = self.v[TR].x
		self.v[BR].y = self.v[BL].y 
		

	def setLeft(self, lft):     
		self.v[TR].x = lft + self.v[TR].x - self.v[BL].x
		self.v[BL].x = lft
		# update othere verts
		self.updateV34()

	def setRight(self, rgt):    
		self.v[BL].x = rgt - (self.v[TR].x - self.v[BL].x)
		self.v[TR].x = rgt
		self.updateV34()

	def setBottom(self, btm):     
		self.v[TR].y = btm + self.v[TR].y - self.v[BL].y
		self.v[BL].y = btm
		self.updateV34()

	def setTop(self, tp):    
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
	
	def overlapAll(self, boxLs, intersectCache): # Flag index lets us know which quadere
		''' Returns none, meaning it didnt overlap any new boxes '''
		v= self.v
		if v[BL].x < 0:
			return True
		elif v[BL].y < 0:
			return True
		else:
			bIdx = len(intersectCache)
			while bIdx:
				bIdx-=1
				b = intersectCache[bIdx]
				if not (	v[TR].y <= b.v[BL].y or\
							v[BL].y >= b.v[TR].y or\
							v[BL].x >= b.v[TR].x or\
							v[TR].x <= b.v[BL].x ):
					
					return True # Intersection with existing box
			#return 0 # Must keep looking
			
			for b in boxLs.boxes:
				if not (v[TR].y <= b.v[BL].y or\
						v[BL].y >= b.v[TR].y or\
						v[BL].x >= b.v[TR].x or\
						v[TR].x <= b.v[BL].x ):
						
					return b # Intersection with new box.
			return False
	
	
	
	def place(self, vert, quad):
		'''
		Place the box on the free quadrent of the vert
		'''
		if quad == BLF:
			self.setRight(vert.x)
			self.setTop(vert.y)
			
		elif quad == TRF:
			self.setLeft(vert.x)
			self.setBottom(vert.y)
		
		elif quad == TLF:
			self.setRight(vert.x)
			self.setBottom(vert.y)

		elif quad == BRF:
			self.setLeft(vert.x)
			self.setTop(vert.y)
	
	# Trys to lock a box onto another box's verts
	# cleans up double verts after
	def tryVert(self, boxes, baseVert):
		for flagIndex, freeQuad in enumerate(quadFlagLs):
			#print 'Testing ', self.width
			if baseVert.free & freeQuad:
				
				self.place(baseVert, freeQuad)
				overlapBox = self.overlapAll(boxes, baseVert.intersectCache[flagIndex])
				if overlapBox is False: # There is no overlap
					baseVert.free &= ~freeQuad # Removes quad
					# Appends all verts but the one that matches. this removes the need for remove doubles
					for vIdx in (0,1,2,3): # (BL,TR,TL,BR) / 0,1,2,3
						self_v= self.v[vIdx] # shortcut
						if not (self_v.x == baseVert.x and self_v.y == baseVert.y):
							boxList.packedVerts.verts.append(self_v)
						else:
							baseVert.free &= self_v.free # make sure the that any unfree areas are wiped.
							
							# Inherit used boxes from old verts
							if self_v.blb: baseVert.blb = self_v.blb 
							if self_v.brb: baseVert.brb = self_v.brb #print 'inherit2'
							if self_v.tlb: baseVert.tlb = self_v.tlb #print 'inherit3'
							if self_v.trb: baseVert.trb = self_v.trb #print 'inherit4'
							self.v[vIdx] = baseVert

					
					
					# Logical checking for used verts by compares box sized and works out verts that may be free.
					# Verticle
					
					if baseVert.tlb and baseVert.trb and\
					(self == baseVert.tlb or self == baseVert.trb):
						if baseVert.tlb.height > baseVert.trb.height:
							baseVert.trb.v[TL].free &= ~(TLF|BLF)
						elif baseVert.tlb.height < baseVert.trb.height:
							baseVert.tlb.v[TR].free &= ~(TRF|BRF)
						else: # same
							baseVert.tlb.v[TR].free &= ~BLF
							baseVert.trb.v[TL].free &= ~BRF						
								
					
					elif baseVert.blb and baseVert.brb and\
					(self == baseVert.blb or self == baseVert.brb):
						if baseVert.blb.height > baseVert.brb.height:
							baseVert.brb.v[BL].free &= ~(TLF|BLF)
						elif baseVert.blb.height < baseVert.brb.height:
							baseVert.blb.v[BR].free &= ~(TRF|BRF)
						else: # same
							baseVert.blb.v[BR].free &= ~TRF
							baseVert.brb.v[BL].free &= ~TLF
					
					# Horizontal
					if baseVert.tlb and baseVert.blb and\
					(self == baseVert.tlb or self == baseVert.blb):
						if baseVert.tlb.width > baseVert.blb.width:
							baseVert.blb.v[TL].free &= ~(TLF|TRF)
						elif baseVert.tlb.width < baseVert.blb.width:
							baseVert.tlb.v[BL].free &= ~(BLF|BRF)
						else: # same
							baseVert.blb.v[TL].free &= ~TRF
							baseVert.tlb.v[BL].free &= ~BRF						
								
					
					elif baseVert.trb and baseVert.brb and\
					(self == baseVert.trb or self == baseVert.brb):
						if baseVert.trb.width > baseVert.brb.width:
							baseVert.brb.v[TR].free &= ~(TRF|TRF)
						elif baseVert.trb.width < baseVert.brb.width:
							baseVert.trb.v[BR].free &= ~(BLF|BRF)
						else: # same
							baseVert.brb.v[TR].free &= ~TLF
							baseVert.trb.v[BR].free &= ~BLF	
					# END LOGICAL VREE SIZE REMOVAL
					
					
					
					
					return 1 # Working
				
				# We have a box that intersects that quadrent.
				elif overlapBox is not False and overlapBox  is not True:  # True is used for a box thats alredt in the freq list or out of bounds error.
					# There was an overlap, add this box to the verts list
					#quadFlagLs = (BLF,BRF,TLF,TRF)
					baseVert.intersectCache[flagIndex].append(overlapBox)
					
					# Limit the cache size
					if len(baseVert.intersectCache[flagIndex]) > 8:
						del baseVert.intersectCache[flagIndex][0]
				
		return 0


class boxList:
	#Global vert pool, stores used lists
	packedVerts = vertList() # will be vertList()
	
	def __init__(self, boxes):
		self.boxes = boxes
		
		# keep a running update of the width and height so we know the area
		# initialize with first box, fixes but where we whwere only packing 1 box
		# At the moment we only start with 1 box so the code below will loop over 1. but thats ok.
		width = height = 0.0
		if boxes:
			for b in boxes:
				if width  < b.width: width= b.width
				if height < b.height: height= b.height
		self.width= width
		self.height= height
		
		# boxArea is the total area of all boxes in the list,
		# can be used with packArea() to determine waistage.
		self.boxArea = 0 # incremented with addBox()
		
	
	# Just like MyBoxLs.boxes.append(), but sets bounds 
	def addBoxPack(self, box):
		'''Adds the box to the boxlist and resized the main bounds and adds area. '''
		self.width = max(self.width, box.getRight())
		self.height = max(self.height, box.getTop())
		
		self.boxArea += box.area
		
		# iterate through these
		#~ quadFlagLs = (1,8,4,2) 
		#~ # Flags for vert idx used quads
		#~ BLF = 1; TRF = 2; TLF = 4; BRF = 8
		#~ quadFlagLs = (BLF,BRF,TLF,TRF)
		
		# Look through all the free vert quads and see if there are some we can remove
		# 
		
		for v in box.v:
			
			# Is my bottom being used.
			
			if v.free & BLF and v.free & BRF: # BLF and BRF
				for b in self.boxes:
					if b.v[TR].y == v.y:
						if b.v[TR].x > v.x:
							if b.v[BL].x < v.x:
								v.free &= ~(BLF|BRF) # Removes quad
				
				# Is my left being used.
			if v.free & BLF and v.free & TLF:
				for b in self.boxes:
					if b.v[TR].x == v.x:
						if b.v[TR].y > v.y:
							if b.v[BL].y < v.y:
								v.free &= ~(BLF|TLF) # Removes quad
				
			if v.free & TRF and v.free & TLF:
				# Is my top being used.
				for b in self.boxes:
					if b.v[BL].y == v.y:
						if b.v[TR].x > v.x:
							if b.v[BL].x < v.x:
								v.free &= ~(TLF|TRF) # Removes quad
								
				
				# Is my right being used.
			if v.free & TRF and v.free & BRF:
				for b in self.boxes:
					if b.v[BL].x == v.x:
						if b.v[TR].y > v.y:
							if b.v[BL].y < v.y:
								v.free &= ~(BRF|TRF) # Removes quad
								
		
		self.boxes.append(box)
		
		
		
	# Just like MyBoxLs.boxes.append(), but sets bounds 
	def addBox(self, box):
		self.boxes.append(box)		
		self.boxArea += box.area

	# The area of the backing bounds.
	def packedArea(self):
		return self.width * self.height
		
	# Sort boxes by area
	def sortArea(self):
		# uvlist.sort(key=lambda v: v.uv.x) #X coord sort
		# self.boxes.sort(lambda A, B: cmp(A.area, B.area) ) # Reverse area sort
		self.boxes.sort(key=lambda b: b.area ) # Reverse area sort
	
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
		
		if not self.boxes:
			return
			
		packedboxes = boxList([self.boxes[-1]])
		
		# Remove verts we KNOW cant be added to
		
		unpackedboxes = self.boxes[:-1]
		
		# Start with this box, the biggest box
		boxList.packedVerts.verts.extend(packedboxes.boxes[0].v)
		
		while unpackedboxes: # != [] - while the list of unpacked boxes is not empty.
			
			freeBoxIdx = len(unpackedboxes)
			while freeBoxIdx:
				freeBoxIdx-=1
				freeBoxContext= unpackedboxes[freeBoxIdx]
				# Sort the verts with this boxes dimensions as a bias, so less poky out bits are made.
				boxList.packedVerts.sortCorner(freeBoxContext.width, freeBoxContext.height)
				
				vertIdx = 0
				
				for baseVert in boxList.packedVerts.verts:
					if baseVert.free: # != 0
						# This will lock the box if its possibel
						if freeBoxContext.tryVert(packedboxes, baseVert):
							packedboxes.addBoxPack( unpackedboxes.pop(freeBoxIdx) ) # same as freeBoxContext. but may as well pop at the same time.
							freeBoxIdx = -1
							break
				
				freeBoxIdx +=1
				
		boxList.packedVerts.verts = [] # Free the list, so it dosent use ram between runs.
		
		self.width = packedboxes.width
		self.height = packedboxes.height
	# 
	def list(self):
		''' Once packed, return a list of all boxes as a list of tuples - (X/Y/WIDTH/HEIGHT) '''
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