import bpy
import Blender
from Blender.Mathutils import Vector, CrossVecs, AngleBetweenVecs, LineIntersect, TranslationMatrix, ScaleMatrix
from Blender.Geometry import ClosestPointOnLine

def debug_pt(co):
	Blender.Window.SetCursorPos(tuple(co))
	Blender.Window.RedrawAll()
	print 'debugging', co


def closestVecIndex(vec, vecls):
	best= -1
	best_dist = 100000000
	for i, vec_test in enumerate(vecls):
		dist = (vec-vec_test).length
		if dist < best_dist:
			best = i
			best_dist = dist
	
	return best

eul = 0.00001

class tree:
	def __init__(self):
		self.branches_all =		[]
		self.branches_root =	[]
		self.mesh = None
		self.object = None
		self.limbScale = 1.0
		
		self.debug_objects = []
		
	def fromCurve(self, object):
		
		curve = object.data
		
		# Set the curve object scale
		if curve.bevob:
			# A bit of a hack to guess the size of the curve object if you have one.
			bb = curve.bevob.boundingBox
			# self.limbScale = (bb[0] - bb[7]).length / 2.825 # THIS IS GOOD WHEN NON SUBSURRFED
			self.limbScale = (bb[0] - bb[7]).length / 1.8
		# end
		
		
		# Get the curve points as bpoints
		for spline in curve:
			brch = branch()
			self.branches_all.append(brch)
			
			for bez in spline:
				# calc normal vector later
				pt = bpoint(brch, Vector(bez.vec[1]), Vector(), bez.radius * self.limbScale)
				brch.bpoints.append( pt )
		
		
		# Get the curve as a mesh. - for inbetween points
		tmpme = bpy.data.meshes.new()	
		
		# remove/backup bevel ob
		bev_back = curve.bevob
		if bev_back: curve.bevob = None
		
		# get the curve mesh data
		tmpob = bpy.data.scenes.active.objects.new( curve )
		tmpme.getFromObject(object)
		bpy.data.scenes.active.objects.unlink(tmpob)
		
		# restore bevel ob
		if bev_back:
			curve.bevob = bev_back
			
			# Guess the size of the curve object if you have one. This is not perfect but good enough
			bb = bev_back.boundingBox
			self.limbScale = (bb[0] - bb[7]).length / 2.825
			
			
		
		# TEMP FOR TESTING
		# bpy.data.scenes.active.objects.new(tmpme)
		
		vecs = [ v.co for v in tmpme.verts ]
		del tmpme
		
		# for branch
		#used_points = set()
		for brch in self.branches_all:
			offset = 0
			for i in xrange(1, len(brch.bpoints)):
				# find the start/end points
				start_pt =	brch.bpoints[offset+i-1]
				end_pt =	brch.bpoints[offset+i]
				
				start = end = None
				for j, co in enumerate(vecs):
					if start == None:
						if (co-start_pt.co).length < eul:
							start = j
					if end == None:
						if (co-end_pt.co).length < eul:
							end = j
					if start != None and end != None:
						break
				
				# for now we assuem the start is always a lower index.
				#if start > end:
				#	raise "error index is not one we like"
				
				#used_points.add(start)
				#used_points.add(end)
				radius = start_pt.radius
				
				#print 'coords', start_co, end_co
				#### print "starting", start, end
				if start > end:
					j = start-1
					raise "some bug!"
				else:
					j = start+1
				
				step = 1
				step_tot = abs(start-end)
				while j!=end:
					#radius = (start_pt.radius*(step_tot-step) - end_pt.radius*step ) / step_tot
					w1 = step_tot-step
					w2 = step
					
					radius = ((start_pt.radius*w1) + (end_pt.radius*w2)) / step_tot
					
					#### print i,j, radius
					pt = bpoint(brch, Vector(vecs[j]), Vector(), radius)
					brch.bpoints.insert(offset+i, pt)
					offset+=1
					
					if start > end:
						j-=1
					else:
						j+=1
					
					step +=1
		
		# Now calculate the normals
		for brch in self.branches_all:
			for i in xrange(1, len(brch.bpoints)-1):
				brch.bpoints[i].next = brch.bpoints[i+1]
				brch.bpoints[i].prev = brch.bpoints[i-1]
			
			brch.bpoints[0].next = brch.bpoints[1]	
			brch.bpoints[-1].prev = brch.bpoints[-2]
			
			
			for pt in brch.bpoints:
				pt.calcNormal()
				pt.calcNextMidCo()
		
		# remove segments
		# We may want to remove segments for 2 reasons
		# 1) - too high resolution
		# 2) - too close together (makes yucky geometry)
		
	def resetTags(self, value):
		for brch in self.branches_all:
			brch.tag = value
	
	def buildConnections(self, sloppy=1.0, stem_trim = 1.0):
		'''
		build tree data - fromCurve must run first
		
		'''
		
		# Connect branches
		for i in xrange(len(self.branches_all)):
			
			brch_i = self.branches_all[i]
			brch_i_head_pt = brch_i.bpoints[0]
			
			for j in xrange(len(self.branches_all)):
				if i != j:
					# See if any of the points match this branch
					# see if Branch 'i' is the child of branch 'j'
					
					brch_j = self.branches_all[j]
					
					best_j, dist = brch_j.findClosest(brch_i_head_pt.co)
					
					# Check its in range, allow for a bit out - hense the 1.5
					if dist < best_j.radius * sloppy:
						
						# Remove points that are within the radius, so as to not get ugly joins
						# TODO - dont remove the whole branch
						while len(brch_i.bpoints)>2 and (brch_i.bpoints[0].co - best_j.nextMidCo).length < best_j.radius * stem_trim:
							del brch_i.bpoints[0]
							brch_i.bpoints[0].prev = None
						
						brch_i.parent_pt = best_j
						
						# addas a member of best_j.children later when we have the geometry info available.
						
						#### print "Found Connection!!!", i, j
						break # go onto the next branch
		
		"""
			children = [brch_child for brch_child in pt.children]
			if children:
				# This pt is one side of the segment, pt.next joins this segment.
				# calculate the median point the 2 segments would span
				# Once this is done we need to adjust 2 things
				# 1) move both segments up/down so they match the branches best.
				# 2) set the spacing of the segments around the point.
				
		
		# First try to get the ideal some space around each joint
		# the spacing shoule be an average of 
		for brch.bpoints:
		"""	
		
		
		# Calc points with dependancies
		# detect circular loops!!! - TODO
		done_nothing = False
		self.resetTags(False)
		while done_nothing == False:
			done_nothing = True
			for brch in self.branches_all:
					
				if brch.tag == False and (brch.parent_pt == None or brch.parent_pt.branch.tag == True):
					
					# Assign this to a spesific side of the parents point
					# we know this is a child but not which side it should be attached to.
					if brch.parent_pt:
						child_locs = [\
						brch.parent_pt.childPoint(0),\
						brch.parent_pt.childPoint(1),\
						brch.parent_pt.childPoint(2),\
						brch.parent_pt.childPoint(3)]
						
						best_idx = closestVecIndex(brch.bpoints[0].co, child_locs)
						brch.parent_pt.children[best_idx] = brch
					# DONE
					
					done_nothing = False
					
					for pt in brch.bpoints:
						# for temp debugging
						## self.mesh.faces.extend([pt.verts])
						pt.calcVerts()
						# pt.toMesh(self.mesh) # Cant do here because our children arnt calculated yet!
					
					brch.tag = True
	
	def optimizeSpacing(self, density=1.0, joint_compression=1.0):
		'''
		Optimize spacing, taking branch hierarchy children into account,
		can add or subdivide segments so branch joins dont look horrible.
		'''
		for brch in self.branches_all:
			brch.evenJointDistrobution(joint_compression)
		
		# Correct points that were messed up from sliding
		# This happens when one point is pushed past another and the branch gets an overlaping line
		for brch in self.branches_all:
			brch.fixOverlapError()
		
		# Collapsing
		for brch in self.branches_all:
			brch.collapsePoints(density)
		
		for brch in self.branches_all:
			brch.branchReJoin()
	
	
	def toDebugDisplay(self):
		'''
		Should be able to call this at any time to see whats going on
		'''
		sce = bpy.data.scenes.active
		
		for ob in self.debug_objects:
			for ob in sce.objects:
				sce.objects.unlink(ob)
		
		for branch_index, brch in enumerate(self.branches_all):
			pt_index = 0
			for pt_index, pt in enumerate(brch.bpoints):
				name = '%.3d_%.3d' % (branch_index, pt_index) 
				if pt.next==None:
					name += '_end'
				if pt.prev==None:
					name += '_start'
					
				ob = sce.objects.new('Empty', name)
				self.debug_objects.append(ob)
				mat = ScaleMatrix(pt.radius, 4) * TranslationMatrix(pt.co)
				ob.setMatrix(mat)
				ob.setDrawMode(8) # drawname
		Blender.Window.RedrawAll()
		
		
	
	def toMesh(self, mesh=None, do_uvmap=True, do_uv_scalewidth=True, uv_image = None):
		# Simple points
		'''
		self.mesh = bpy.data.meshes.new()
		self.object = bpy.data.scenes.active.objects.new(self.mesh)
		self.mesh.verts.extend([ pt.co for brch in self.branches_all for pt in brch.bpoints ])
		'''
		if mesh:
			self.mesh = mesh
			mesh.verts = None
		else:
			self.mesh = bpy.data.meshes.new()
		
		totverts = 0
		
		for brch in self.branches_all:
			totverts += len(brch.bpoints)
		
		self.mesh.verts.extend( [ (0.0,0.0,0.0) ] * ((totverts * 4)+1) ) # +1 is a dummy vert
		verts = self.mesh.verts
		
		# Assign verts to points, 4 verts for each point.
		i = 1 # dummy vert, should be 0
		for brch in self.branches_all:			
			for pt in brch.bpoints:
				pt.verts[0] = verts[i]
				pt.verts[1] = verts[i+1]
				pt.verts[2] = verts[i+2]
				pt.verts[3] = verts[i+3]
				i+=4
				
			# Do this again because of collapsing
			# pt.calcVerts(brch)
		
		# roll the tube so quads best meet up to their branches.
		for brch in self.branches_all:
			#for pt in brch.bpoints:
			if brch.parent_pt:
				
				# Use temp lists for gathering an average
				if brch.parent_pt.roll_angle == None:
					brch.parent_pt.roll_angle = [brch.getParentQuadAngle()]
				# More then 2 branches use this point, add to the list
				else:
					brch.parent_pt.roll_angle.append( brch.getParentQuadAngle() )
		
		# average the temp lists into floats
		for brch in self.branches_all:
			#for pt in brch.bpoints:
			if brch.parent_pt and type(brch.parent_pt.roll_angle) == list:
				# print brch.parent_pt.roll_angle
				f = 0.0
				for val in brch.parent_pt.roll_angle:
					f += val
				brch.parent_pt.roll_angle = f/len(brch.parent_pt.roll_angle)
		
		# set the roll of all the first segments that have parents,
		# this is because their roll is set from their parent quad and we dont want them to roll away from that.
		for brch in self.branches_all:
			if brch.parent_pt:
				# if the first joint has a child then apply half the roll
				# theres no correct solition here, but this seems ok
				if brch.bpoints[0].roll_angle != None:
					#brch.bpoints[0].roll_angle *= 0.5
					#brch.bpoints[0].roll_angle = 0.0
					#brch.bpoints[1].roll_angle = 0.0
					brch.bpoints[0].roll_angle = 0
					pass
				else:
					# our roll was set from the branches parent and needs no changing
					# set it to zero so the following functions know to interpolate.
					brch.bpoints[0].roll_angle = 25.0
					#brch.bpoints[1].roll_angle = 0.0
		
		'''
		Now interpolate the roll!
		The method used here is a little odd.
		
		* first loop up the branch and set each points value to the "last defined" value and record the steps
		since the last defined value
		* Do the same again but backwards
		
		now for each undefined value we have 1 or 2 values, if its 1 its simple we just use that value 
		( no interpolation ), if there are 2 then we use the offsets from each end to work out the interpolation.
		
		one up, one back, and another to set the values, so 3 loops all up.
		'''
		#### print "scan up the branch..."
		for brch in self.branches_all:
			last_value = None
			last_index = -1
			for i in xrange(len(brch.bpoints)):
				pt = brch.bpoints[i]
				if type(pt.roll_angle) in (float, int):
					last_value = pt.roll_angle
					last_index = i
				else:
					if type(last_value) in (float, int):
						# Assign a list, because there may be a connecting roll value from another joint
						pt.roll_angle = [(last_value, i-last_index)]
				
			#### print "scan down the branch..."
			last_value = None
			last_index = -1
			for i in xrange(len(brch.bpoints)-1, -1, -1): # same as above but reverse
				pt = brch.bpoints[i]
				if type(pt.roll_angle) in (float, int):
					last_value = pt.roll_angle
					last_index = i
				else:
					if last_value != None:
						if type(pt.roll_angle) == list:
							pt.roll_angle.append((last_value, last_index-i))
						else:
							#pt.roll_angle = [(last_value, last_index-i)]
							
							# Dont bother assigning a list because we wont need to add to it later
							pt.roll_angle = last_value 
			
			# print "looping ,...."
			### print "assigning/interpolating roll values"
			for pt in brch.bpoints:
				
				# print "this roll IS", pt.roll_angle
				
				if pt.roll_angle == None:
					continue
				elif type(pt.roll_angle) in (float, int):
					pass
				elif len(pt.roll_angle) == 1:
					pt.roll_angle = pt.roll_angle[0][0]
				else:
					# interpolate
					tot = pt.roll_angle[0][1] + pt.roll_angle[1][1]
					pt.roll_angle = \
					 (pt.roll_angle[0][0] * (tot - pt.roll_angle[0][1]) +\
					  pt.roll_angle[1][0] * (tot - pt.roll_angle[1][1])) / tot
					
					#### print pt.roll_angle, 'interpolated roll'
					
				pt.roll(pt.roll_angle)
				
		# Done with temp average list. now we know the best roll for each branch.
		
		# mesh the data
		for brch in self.branches_all:
			for pt in brch.bpoints:
				pt.toMesh(self.mesh)
		
		faces = self.mesh.faces
		faces.extend([ face for brch in self.branches_all for pt in brch.bpoints for face in pt.faces if face ])
		
		if do_uvmap:
			# Assign the faces back
			face_index = 0
			for brch in self.branches_all:
				for pt in brch.bpoints:
					for i in (0,1,2,3):
						if pt.faces[i]:
							pt.faces[i] = faces[face_index]
							face_index +=1
			
			self.mesh.faceUV = True
			for brch in self.branches_all:
				
				y_val = 0.0
				for pt in brch.bpoints:
					if pt.next:
						y_size = (pt.co-pt.next.co).length
						
						# scale the uvs by the radiusm, avoids stritching.
						if do_uv_scalewidth:
							y_size = y_size / pt.radius
						
						for i in (0,1,2,3):
							if pt.faces[i]:
								if uv_image:
									pt.faces[i].image = uv_image
								uvs = pt.faces[i].uv
								'''
								uvs[0].x = i
								uvs[1].x = i
								
								uvs[2].x = i+1
								uvs[3].x = i+1
								
								uvs[1].y = y_val
								uvs[2].y = y_val
								
								uvs[0].y = y_val+y_size
								uvs[3].y = y_val+y_size
								'''
								uvs[3].x = i
								uvs[3].y = y_val+y_size
								
								uvs[0].x = i
								uvs[0].y = y_val
								
								uvs[1].x = i+1
								uvs[1].y = y_val
								
								uvs[2].x = i+1
								uvs[2].y = y_val+y_size
								
						
						do_uv_scalewidth
						if pt.next:	
							y_val += y_size
		
		
		return self.mesh

zup = Vector(0,0,1)

class bpoint:
	''' The point in the middle of the branch, not the mesh points
	'''
	def __init__(self, brch, co, no, radius):
		self.branch = brch
		self.co = co
		self.no = no
		self.radius = radius
		self.vecs =		[None, None, None, None] # 4 for now
		self.verts =	[None, None, None, None]
		self.children = [None, None, None, None] # child branches, dont fill in faces here
		self.faces = [None, None, None, None]
		self.next = None
		self.prev = None
		
		
		# when set, This is the angle we need to roll to best face our branches
		# the roll that is set may be interpolated if we are between 2 branches that need to roll.
		# Set to None means that the roll will be left default (from parent)
		self.roll_angle = None
		
		
		# The location between this and the next point,
		# if we want to be tricky we can try make this not just a simple
		# inbetween and use the normals to have some curvature
		self.nextMidCo = None
		
		# Similar to above, median point of all children
		self.childrenMidCo = None
		
		# Similar as above, but for radius
		self.childrenMidRadius = None
		
		# Target locations are used when you want to move the point to a new location but there are
		# more then 1 influence, build up a list and then apply
		self.targetCos = []
	
	def setCo(self, co):
		self.co[:] = co
		self.calcNextMidCo()
		self.calcNormal()
		
		if self.prev:
			self.prev.calcNextMidCo()
			self.prev.calcNormal()
			self.prev.calcChildrenMidData()
		
		if self.next:
			self.prev.calcNormal()
		
		self.calcChildrenMidData()
		
		
	def nextLength(self):
		return (self.co-self.next.co).length
	def prevLength(self):
		return (self.co-self.prev.co).length
		
	def hasOverlapError(self):
		if self.prev == None:
			return False
		if self.next == None:
			return False
		'''
		# see if this point sits on the line between its siblings.
		co, fac = ClosestPointOnLine(self.co, self.prev.co, self.next.co)
		
		if fac >= 0.0 and fac <= 1.0:
			return False # no overlap, we are good
		else:
			return True # error, some overlap
		'''
		
		
		# Alternate method, maybe better
		ln = self.nextLength()
		lp = self.prevLength()
		ls = (self.prev.co-self.next.co).length
		
		# Are we overlapping? the length from our next or prev is longer then the next-TO-previous?
		if ln>ls or lp>ls:
			return True
		else:
			return False
		
	
	def applyTargetLocation(self):
		if not self.targetCos:
			return False
		elif len(self.targetCos) == 1:
			self.setCo(self.targetCos[0])
		else:
			co_all = Vector()
			for co in self.targetCos:
				co_all += co
			
			self.setCo(co_all / len(self.targetCos))
			self.targetCos[:] = []
		return True
	
	def calcNextMidCo(self):
		if not self.next:
			return None
		
		# be tricky later.
		self.nextMidCo = (self.co + self.next.co) * 0.5
	
	def calcNormal(self):
		if self.prev == None:
			self.no = (self.next.co - self.co).normalize()
		elif self.next == None:
			self.no = (self.co - self.prev.co).normalize()
		else:
			self.no = (self.next.co - self.prev.co).normalize()
	
	def calcChildrenMidData(self):
		'''
		Calculate childrenMidCo & childrenMidRadius
		This is a bit tricky, we need to find a point between this and the next,
		the medium of all children, this point will be on the line between this and the next.
		'''
		if not self.next:
			return None
		
		# factor between this and the next point
		radius = factor = factor_i = 0.0
		
		count = 0
		for brch in self.children:
			if brch: # we dont need the co at teh moment.
				co, fac = ClosestPointOnLine(brch.bpoints[0].co, self.co, self.next.co)
				factor_i += fac
				count += 1
				
				radius += brch.bpoints[0].radius
		
		if not count:
			return
		
		# interpolate points 
		factor_i	= factor_i/count
		factor		= 1-factor_i
		
		self.childrenMidCo = (self.co * factor) + (self.next.co * factor_i)
		self.childrenMidRadius = radius
		
		#debug_pt(self.childrenMidCo)
		
	def getAbsVec(self, index):
		# print self.vecs, index
		return self.co + self.vecs[index]
	
	def slide(self, factor):
		'''
		Slides the segment up and down using the prev and next points
		'''
		self.setCo(self.slideCo(factor))
	
	def slideCo(self, factor):
		if self.prev == None or self.next == None or factor==0.0:
			return
		
		if factor < 0.0:
			prev_co = self.prev.co
			co = self.co
			
			ofs = co-prev_co
			ofs.length = abs(factor)
			self.co - ofs
			
			return self.co - ofs
		else:
			next_co = self.next.co
			co = self.co
			
			ofs = co-next_co
			ofs.length = abs(factor)
			
			return self.co - ofs
		
	
	def collapseDown(self, branch):
		'''
		Collapse the next point into this one
		'''
		
		# self.next.next == None check is so we dont shorten the final length of branches.
		if self.next == None or self.next.next == None or self.hasChildren() or self.next.hasChildren():
			return False
		
		branch.bpoints.remove(self.next)
		self.next = self.next.next # skip 
		self.next.prev = self
		
		# Watch this place - must update all data thats needed. roll is not calculaetd yet.
		self.calcNextMidCo()
		return True
		
	def collapseUp(self, branch):
		'''
		Collapse the previous point into this one
		'''
		
		# self.next.next == None check is so we dont shorten the final length of branches.
		if self.prev == None or self.prev.prev == None or self.prev.hasChildren() or self.prev.prev.hasChildren():
			return False
		
		branch.bpoints.remove(self.prev)
		self.prev = self.prev.prev # skip 
		self.prev.next = self
		
		# Watch this place - must update all data thats needed. roll is not calculaetd yet.
		self.prev.calcNextMidCo()
		return True
		
	
	def smooth(self, factor):
		'''
		Blend this point into the other 2 points
		'''
		
		#if self.next == None or self.prev == None or self.hasChildren() or self.prev.hasChildren():
		if self.next == None or self.prev == None:
			return False
		
		radius = (self.next.radius + self.prev.radius)/2.0
		no = (self.next.no + self.prev.no).normalize()
		
		# do a line intersect to work out the best location
		'''
		cos = LineIntersect(	self.next.co, self.next.co+self.next.no,\
								self.prev.co, self.prev.co+self.prev.no)
		if cos == None:
			co = (self.prev.co + self.next.co)/2.0
		else:
			co = (cos[0]+cos[1])/2.0
		'''
		# Above can give odd results every now and then
		co = (self.prev.co + self.next.co)/2.0
		
		# Now apply
		factor_i = 1.0-factor
		self.setCo(self.co*factor_i  +  co*factor)
		self.radius = self.radius*factor_i  +  radius*factor
		
		return True
		
	def childPoint(self, index):
		'''
		Returns the middle point for any children between this and the next edge
		'''
		if self.next == None:
			raise 'Error'
		
		if index == 0:	return (self.getAbsVec(0) + self.next.getAbsVec(1)) / 2
		if index == 1:	return (self.getAbsVec(1) + self.next.getAbsVec(2)) / 2
		if index == 2:	return (self.getAbsVec(2) + self.next.getAbsVec(3)) / 2
		if index == 3:	return (self.getAbsVec(3) + self.next.getAbsVec(0)) / 2
	
	def roll(self, angle):
		'''
		Roll the quad about its normal 
		use for aurienting the sides of a quad to meet a branch that stems from here...
		'''
		
		mat = Blender.Mathutils.RotationMatrix(angle, 3, 'r', self.no)
		for i in xrange(4):
			self.vecs[i] = self.vecs[i] * mat
	
	
	def toMesh(self, mesh):
		self.verts[0].co = self.getAbsVec(0)
		self.verts[1].co = self.getAbsVec(1)
		self.verts[2].co = self.getAbsVec(2)
		self.verts[3].co = self.getAbsVec(3)
		
		if not self.next:
			return
		verts = self.verts
		next_verts = self.next.verts
		
		ls = []
		if self.prev == None and self.branch.parent_pt:
			# join from parent branch
			
			# which side are we of the parents quad
			index = self.branch.parent_pt.children.index(self.branch)
			
			if index==0:	verts = [self.branch.parent_pt.verts[0], self.branch.parent_pt.verts[1], self.branch.parent_pt.next.verts[1], self.branch.parent_pt.next.verts[0]]
			if index==1:	verts = [self.branch.parent_pt.verts[1], self.branch.parent_pt.verts[2], self.branch.parent_pt.next.verts[2], self.branch.parent_pt.next.verts[1]]
			if index==2:	verts = [self.branch.parent_pt.verts[2], self.branch.parent_pt.verts[3], self.branch.parent_pt.next.verts[3], self.branch.parent_pt.next.verts[2]]
			if index==3:	verts = [self.branch.parent_pt.verts[3], self.branch.parent_pt.verts[0], self.branch.parent_pt.next.verts[0], self.branch.parent_pt.next.verts[3]]
			
			if not self.children[0]:	self.faces[0] = [verts[0], verts[1], next_verts[1], next_verts[0]]
			if not self.children[1]:	self.faces[1] = [verts[1], verts[2], next_verts[2], next_verts[1]]
			if not self.children[2]:	self.faces[2] = [verts[2], verts[3], next_verts[3], next_verts[2]]
			if not self.children[3]:	self.faces[3] = [verts[3], verts[0], next_verts[0], next_verts[3]]
			
		else:
			# normal join
			if not self.children[0]:	self.faces[0] = [verts[0], verts[1], next_verts[1], next_verts[0]]
			if not self.children[1]:	self.faces[1] = [verts[1], verts[2], next_verts[2], next_verts[1]]
			if not self.children[2]:	self.faces[2] = [verts[2], verts[3], next_verts[3], next_verts[2]]
			if not self.children[3]:	self.faces[3] = [verts[3], verts[0], next_verts[0], next_verts[3]]
		
		mesh.faces.extend(ls)
	
	def calcVerts(self):
		# place the 4 verts we have assigned.
		#for i, v in self.verts:
		#	v.co = self.co	
		
		if self.prev == None:
			if self.branch.parent_pt:
				#cross = CrossVecs(brch.parent_pt.vecs[3], self.no)
				
				# TOD - if branch insets the trunk
				### cross = CrossVecs(self.no, brch.getParentFaceCent() - brch.parent_pt.getAbsVec( brch.getParentQuadIndex() ))
				# cross = CrossVecs(self.no, self.)
				
				
				#debug_pt( brch.parent_pt.getAbsVec( brch.getParentQuadIndex() ))
				#debug_pt( brch.getParentFaceCent() )
				#debug_pt( brch.parent_pt.getAbsVec( brch.getParentQuadIndex() ))
				#debug_pt( brch.getParentFaceCent() )
				
				cross = CrossVecs(self.no, self.branch.getParentFaceCent() - self.branch.parent_pt.getAbsVec( self.branch.getParentQuadIndex() ))
				
				
			else:
				# parentless branch
				cross = zup
		else:
			cross = CrossVecs(self.prev.vecs[0], self.no)
		
		self.vecs[0] = Blender.Mathutils.CrossVecs(self.no, cross)
		self.vecs[0].length = self.radius
		mat = Blender.Mathutils.RotationMatrix(90, 3, 'r', self.no)
		self.vecs[1] = self.vecs[0] * mat
		self.vecs[2] = self.vecs[1] * mat
		self.vecs[3] = self.vecs[2] * mat
		
		
		'''
		Blender.Window.SetCursorPos(tuple(v.co))
		Blender.Window.RedrawAll()
		
		while True:
			val = Blender.Window.QRead()[1]
			if val:
				break
		
		v.co += (self.no * (self.radius * 0.01))
		'''
	
	def hasChildren(self):
		if	self.children[0] != None or\
			self.children[1] != None or\
			self.children[2] != None or\
			self.children[3] != None:
			return True
		else:
			return False

class branch:
	def __init__(self):
		self.bpoints = []
		self.parent_pt = None
		self.tag = False # have we calculated our points
		
		# Bones per branch
		self.bones = []
	
	def getParentQuadAngle(self):
		'''
		The angle off we are from our parent quad,
		'''
		# used to roll the parent so its faces us better
		parent_normal = self.getParentFaceCent() - self.parent_pt.nextMidCo
		self_normal = self.bpoints[1].co - self.parent_pt.co
		# We only want the angle in relation to the parent points normal
		# modify self_normal to make this so
		cross = CrossVecs(self_normal, self.parent_pt.no)
		self_normal = CrossVecs(self.parent_pt.no, cross) # CHECK
		angle = AngleBetweenVecs(parent_normal, self_normal)
		
		# see if we need to rotate positive or negative
		# USE DOT PRODUCT!
		cross = CrossVecs(parent_normal, self_normal)
		if AngleBetweenVecs(cross, self.parent_pt.no) > 90:
			angle = -angle
		
		return angle
	
	def getParentQuadIndex(self):
		return self.parent_pt.children.index(self)
	def getParentFaceCent(self):
		return self.parent_pt.childPoint(  self.getParentQuadIndex()  )
	
	def findClosest(self, co):
		''' # this dosnt work, but could.
		best = None
		best_dist = 100000000
		for pt in self.bpoints:
			if pt.next:
				co_on_line, fac = ClosestPointOnLine(co, pt.co, pt.next.co)
				print fac
				if fac >= 0.0 and fac <= 1.0:
					return pt, (co-co_on_line).length
		
		return best, best_dist
		'''
		best = None
		best_dist = 100000000
		for pt in self.bpoints:
			if pt.nextMidCo:
				dist = (pt.nextMidCo-co).length
				if dist < best_dist:
					best = pt
					best_dist = dist
		
		return best, best_dist
		
	def evenPointDistrobution(self, factor):
		'''
		Redistribute points that are not evenly distributed
		factor is between 0.0 and 1.0
		'''
		
		for pt in self.bpoints:
			if pt.next and pt.prev and pt.hasChildren() == False and pt.prev.hasChildren() == False:
				w1 = pt.nextLength()
				w2 = pt.prevLength()
				wtot = w1+w2
				w1=w1/wtot
				#w2=w2/wtot
				w1 = abs(w1-0.5)*2 # make this from 0.0 to 1.0, where 0 is the middle and 1.0 is as far out of the middle as possible.
				# print "%.6f" % w1
				pt.smooth(w1*factor)
	
	def fixOverlapError(self):
		# Keep fixing until no hasOverlapError left to fix.
		error = True
		while error:
			error = False
			for pt in self.bpoints:
				#if pt.prev and pt.hasChildren() == False and pt.prev.hasChildren() == False:
				if pt.prev and pt.next:
					if pt.hasOverlapError():
						error = True
						pt.smooth(1.0)
	
	def evenJointDistrobution(self, joint_compression = 1.0):
		# See if we need to evaluate this branch at all 
		if len(self.bpoints) <= 2: # Rare but in this case we cant do anything
			return
		has_children = False
		for pt in self.bpoints:
			if pt.hasChildren():
				has_children = True
				break
		
		if not has_children:
			return
		
		# OK, we have children, so we have some work to do...
		# center each segment
		
		# work out the median location of all points children.
		for pt in self.bpoints:
			pt.calcChildrenMidData()
		
		for pt in self.bpoints:
			pt.targetCos = []
			if pt.childrenMidCo:
				# Move this and the next segment to be around the child point.
				# TODO - collect slides and then average- only happens sometimes so not that bad.
				# TODO - factor in the branch angle, be careful with this - close angles can have extreme values.
				co = pt.slideCo( (pt.childrenMidCo - pt.co).length - (pt.childrenMidRadius * joint_compression) )
				if co:
					pt.targetCos.append( co )
				
				co = pt.next.slideCo((pt.childrenMidRadius * joint_compression) - (pt.childrenMidCo - pt.next.co).length )
				if co:
					pt.next.targetCos.append( co )
		
		for pt in self.bpoints:
			pt.applyTargetLocation()
	
	def collapsePoints(self, density):
		collapse = True
		while collapse:
			collapse = False
			
			pt = self.bpoints[0]
			while pt:
				
				if pt.prev and pt.next and not pt.prev.hasChildren():
					if (pt.prev.nextMidCo-pt.co).length < ((pt.radius + pt.prev.radius)/2) * density:
						pt_save = pt.prev
						if pt.next.collapseUp(self): # collapse this point
							collapse = True
							pt = pt_save # so we never reference a removed point
				
				if not pt.hasChildren(): #if pt.childrenMidCo == None:
					# Collapse, if tehre is any problems here we can move into a seperate losop.
					# do here because we only want to run this on points with no childzren,
					
					# Are we closer theto eachother then the radius?
					if pt.next and (pt.nextMidCo-pt.co).length < ((pt.radius + pt.next.radius)/2) * density:
						if pt.collapseDown(self):
							collapse = True
				
				pt = pt.next
		## self.checkPointList()
		self.evenPointDistrobution(1.0)
		
		for pt in self.bpoints:
			pt.calcNormal()
			pt.calcNextMidCo()
	
	def branchReJoin(self):
		'''
		Not needed but nice to run after collapsing incase segments moved a lot
		'''
		if not self.parent_pt:
			return # nothing to do
		
		# see if the next segment is closer now (after collapsing)
		par_pt = self.parent_pt
		root_pt = self.bpoints[0]
		
		index = par_pt.children.index(self)
		
		current_dist = (par_pt.nextMidCo - root_pt.co).length
		
		# TODO - Check size of new area is ok to move into
		
		if par_pt.next and par_pt.next.next and par_pt.next.children[index] == None:
			# We can go here if we want, see if its better
			if current_dist > (par_pt.next.nextMidCo - root_pt.co).length:
				self.parent_pt.children[index] = None
				self.parent_pt = par_pt.next
				self.parent_pt.children[index] = self
				return
		
		if par_pt.prev and par_pt.prev.children[index] == None:
			# We can go here if we want, see if its better
			if current_dist > (par_pt.prev.nextMidCo - root_pt.co).length:
				self.parent_pt.children[index] = None
				self.parent_pt = par_pt.prev
				self.parent_pt.children[index] = self
				return
	
	def checkPointList(self):
		'''
		Error checking. use to check if collapsing worked.
		'''
		p_link = self.bpoints[0]
		i = 0
		while p_link:
			if self.bpoints[i] != p_link:
				raise "Error"
			
			if p_link.prev and p_link.prev != self.bpoints[i-1]:
				raise "Error Prev"
			
			if p_link.next and p_link.next != self.bpoints[i+1]:
				raise "Error Next"
			
			p_link = p_link.next
			i+=1

	def toMesh(self):
		pass


def buildTree(ob):
	'''
	Must be a curve object
	'''
	t = tree()
	t.fromCurve(ob)
	#t.toDebugDisplay()
	t.buildConnections(sloppy = 1.5, stem_trim = 0.5) # how sloppy to be
	#t.toDebugDisplay()
	t.optimizeSpacing(density=0.4, joint_compression=2.8)
	#t.toDebugDisplay()
	mesh = Blender.Mesh.Get('Mesh')
	mesh = t.toMesh(mesh, uv_image= Blender.Image.Get('bark'), do_uv_scalewidth = False)
	#t.toDebugDisplay()
	
	#armature = t.toArmature()
	if 0:
		ob_mesh = bpy.data.scenes.active.objects.new(mesh)
		ob_mesh.setMatrix(ob.matrixWorld)
		ob_mesh.sel = 0



# this should run as a module
if __name__ == '__main__':
	sce = bpy.data.scenes.active
	ob = sce.objects.active
	#ob = bpy.data.objects['Curve']
	buildTree(ob)
