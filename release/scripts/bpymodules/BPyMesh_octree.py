from Blender import *

try:
	import psyco
	psyco.full()
except:
	print 'no psyco for you!'

DotVecs= Mathutils.DotVecs
#======================================================== 
# SPACIAL TREE - Seperate Class - use if you want to
# USed for getting vert is a proximity
LEAF_SIZE = 128
class octreeNode:
	def __init__(self, verts, parent):
		
		# Assunme we are a leaf node, until split is run.
		self.verts = verts 
		self.children = []
		
		if parent == None: # ROOT NODE, else set bounds when making children,
			# BOUNDS
			v= verts[0]
			maxx,maxy,maxz= v.co
			minx,miny,minz= maxx,maxy,maxz
			
			for v in verts:
				x,y,z= v.co
				if x>maxx: maxx= x
				if y>maxy: maxy= y
				if z>maxz: maxz= z
				
				if x<minx: minx= x
				if y<miny: miny= y
				if z<minz: minz= z
			
			self.minx= minx
			self.miny= miny
			self.minz= minz
			
			self.maxx= maxx
			self.maxy= maxy
			self.maxz= maxz
			
			# We have no parent to split us so split ourselves.
			#self.setCornerPoints()
			self.splitNode()
			
	def splitNode(self):
		if len(self.verts) > LEAF_SIZE:
			self.makeChildren() # 8 new children,
			self.verts = None
		# Alredy assumed a leaf not so dont do anything here.
		
	def makeChildren(self):
		verts= self.verts
		# Devide into 8 children.
		axisDividedVerts = [[],[],[],[],[],[],[],[]] # Verts Only
		
		
		divx = (self.maxx + self.minx) / 2
		divy = (self.maxy + self.miny) / 2
		divz = (self.maxz + self.minz) / 2
		
		# Sort into 8
		for v in verts:
			x,y,z = v.co
			
			if x > divx:
				if y > divy:
					if z > divz:
						axisDividedVerts[0].append(v)
					else:
						axisDividedVerts[1].append(v)
				else:
					if z > divz:
						axisDividedVerts[2].append(v)
					else:
						axisDividedVerts[3].append(v)
			else:
				if y > divy:
					if z > divz:
						axisDividedVerts[4].append(v)
					else:
						axisDividedVerts[5].append(v)
				else:
					if z > divz:
						axisDividedVerts[6].append(v)
					else:
						axisDividedVerts[7].append(v)
					
		# populate self.children
		for i in xrange(8):
			octNode = octreeNode(axisDividedVerts[i], self)
			# Set bounds manually
			if i == 0:
				octNode.minx = divx
				octNode.maxx = self.maxx
				octNode.miny = divy
				octNode.maxy = self.maxy
				octNode.minz = divz
				octNode.maxz = self.maxz
			elif i == 1:
				octNode.minx = divx
				octNode.maxx = self.maxx
				octNode.miny = divy
				octNode.maxy = self.maxy
				octNode.minz = self.minz #
				octNode.maxz = divz #
			elif i == 2:
				octNode.minx = divx
				octNode.maxx = self.maxx
				octNode.miny = self.miny  # 
				octNode.maxy = divy #
				octNode.minz = divz
				octNode.maxz = self.maxz
			elif i == 3:
				octNode.minx = divx
				octNode.maxx = self.maxx
				octNode.miny = self.miny #
				octNode.maxy = divy #
				octNode.minz = self.minz #
				octNode.maxz = divz #
			elif i == 4:
				octNode.minx = self.minx #
				octNode.maxx = divx #
				octNode.miny = divy
				octNode.maxy = self.maxy
				octNode.minz = divz
				octNode.maxz = self.maxz
			elif i == 5:
				octNode.minx = self.minx #
				octNode.maxx = divx #
				octNode.miny = divy
				octNode.maxy = self.maxy
				octNode.minz = self.minz #
				octNode.maxz = divz #
			elif i == 6:
				octNode.minx = self.minx #
				octNode.maxx = divx #
				octNode.miny = self.miny  # 
				octNode.maxy = divy #
				octNode.minz = divz
				octNode.maxz = self.maxz
			elif i == 7:
				octNode.minx = self.minx #
				octNode.maxx = divx #
				octNode.miny = self.miny  # 
				octNode.maxy = divy #
				octNode.minz = self.minz #
				octNode.maxz = divz #
			#octNode.setCornerPoints()
			octNode.splitNode() # Splits the node if it can.
			self.children.append(octNode)
	
	# GETS VERTS IN A Distance RANGE-
	def getVertsInRange(self, loc, normal, range_val, vertList):
		#loc= Mathutils.Vector(loc)			# MUST BE VECTORS
		#normal= Mathutils.Vector(normal)	

		'''
		loc: Vector of the location to search from
		normal: None or Vector - if a vector- will only get verts on this side of the vector
		range_val: maximum distance. A negative value will fill the list with teh closest vert only.
		vertList: starts as an empty list
		list that this function fills with verts that match
		'''
		xloc,yloc,zloc= loc
		
		if range_val<0:
			range_val= -range_val
			FIND_CLOSEST= True
			vertList.append(None) # just update the 1 vertex
		else:
			FIND_CLOSEST= False
		
		if self.children:
			# Check if the bounds are in range_val,
			for childNode in self.children:
				# First test if we are surrounding the point.
				if\
				childNode.minx - range_val < xloc and\
				childNode.maxx + range_val > xloc and\
				childNode.miny - range_val < yloc and\
				childNode.maxy + range_val > yloc and\
				childNode.minz - range_val < zloc and\
				childNode.maxz + range_val > zloc:
					# Recurse down or get virts.
					childNode.getVertsInRange(loc, normal, range_val, vertList)
					#continue # Next please
		
		else: # we are a leaf node. Test vert locations.
			if not normal:
				# Length only check
				for v in self.verts:
					length = (loc - v.co).length
					if length < range_val:
						if FIND_CLOSEST:
							# Just update the 1 vert
							vertList[0]= (v, length)
							range_val= length # Shink the length so we only get verts from their.
						else:
							vertList.append((v, length))
			else:
				# Lengh and am I infront of the vert.
				for v in self.verts:
					length = (loc - v.co).length
					if length < range_val:
						# Check if the points in front
						dot= DotVecs(normal, loc) - DotVecs(normal, v.co)
						if dot<0:
							vertList.append((v, length))
				
# END TREE




# EXAMPLE RADIO IN PYTHON USING THE ABOVE FUNCTION
"""
import BPyMesh
# Radio bake
def bake():
	
	_AngleBetweenVecs_= Mathutils.AngleBetweenVecs
	def AngleBetweenVecs(a1,a2):
		try:
			return _AngleBetweenVecs_(a1,a2)
		except:
			return 180
	
	
	
	scn = Scene.GetCurrent()
	ob = scn.getActiveObject()
	me = ob.getData(mesh=1)
	
	dist= Draw.PupFloatInput('MaxDist:', 2.0, 0.1, 20.0, 0.1, 3)
	if dist==None:
		return
	
	# Make nice normals
	BPyMesh.meshCalcNormals(me)
	
	
	len_verts= len(me.verts)
	#me.sel= False
	meshOctTree = octreeNode(me.verts, None)

	
	
	# Store face areas
	vertex_areas= [0.0] * len_verts
	
	# Get vertex areas - all areas of face users
	for f in me.faces:
		a= f.area
		for v in f.v:
			vertex_areas[v.index] += a
			
	
	
	bias= 0.001
	
	t= sys.time()
	
	# Tone for the verts
	vert_tones= [0.0] * len_verts
	maxtone= 0.0
	mintone= 100000000
	for i, v in enumerate(me.verts):
		if not i%10:
			print 'verts to go', len_verts-i
		v_co= v.co
		v_no= v.no
		verts_in_range= []
		meshOctTree.getVertsInRange(v_co, v_no, dist, verts_in_range)
		
		tone= 0.0
		# These are verts in our range
		for test_v, length in verts_in_range:
			if bias<length:
				try:
					# Make sure this isnt a back facing vert
					normal_diff= AngleBetweenVecs(test_v.no, v_no)
				except:
					continue
				
				if normal_diff > 90: # were facing this vert
					#if 1:	
					# Current value us between zz90 and 180
					# make between 0 and 90
					# so 0 is right angles and 90 is direct opposite vertex normal
					normal_diff= (normal_diff-90)
					
					# Vertex area needs to be taken into account so we dont have small faces over influencing.
					vertex_area= vertex_areas[test_v.index]
					
					# Get the angle the vertex is in location from the location and normal of the vert.
					above_diff= AngleBetweenVecs(test_v.co-v.co, v_no)
					## Result will be between 0 :above and 90: horizon.. invert this so horizon has littel effect
					above_diff= 90-above_diff
					# dist-length or 1.0/length both work well
					tone= (dist-length) * vertex_area * above_diff * normal_diff
					vert_tones[i] += tone
		
		if maxtone<vert_tones[i]:
			maxtone= vert_tones[i]
		if mintone>vert_tones[i]:
			mintone= vert_tones[i]
	
	
	if not maxtone:
		Draw.PupMenu('No verts in range, use a larger range')
		return
	
	# Apply tones
	for f in me.faces:
		f_col= f.col
		for i, v in enumerate(f.v):
			c= f_col[i]
			v_index= v.index
			tone= int(((maxtone - vert_tones[v.index]) / maxtone) * 255 )
			#print tone
			c.r= c.g= c.b= tone
	
	print 'time', sys.time()-t
	
	
if __name__=="__main__":
	bake()
"""