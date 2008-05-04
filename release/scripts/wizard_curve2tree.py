#!BPY
"""
Name: 'Tree from Curves'
Blender: 245
Group: 'Wizards'
Tip: 'Generate trees from curve shapes'
"""

__author__ = "Campbell Barton"
__url__ = ['www.blender.org', 'blenderartists.org']
__version__ = "0.1"

__bpydoc__ = """\

"""

# --------------------------------------------------------------------------
# Tree from Curves v0.1 by Campbell Barton (AKA Ideasman42)
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

import bpy
import Blender
import BPyMesh
from Blender.Mathutils import Vector, Matrix, CrossVecs, AngleBetweenVecs, LineIntersect, TranslationMatrix, ScaleMatrix, RotationMatrix, Rand
from Blender.Geometry import ClosestPointOnLine
from Blender.Noise import randuvec

GLOBALS = {}
GLOBALS['non_bez_error'] = 0

'''
def debugVec(v1,v2):
	sce = bpy.data.scenes.active
	me = bpy.data.meshes.new()
	me.verts.extend( [v1,v2] )
	me.edges.extend( [(0,1)] )
	sce.objects.new(me)
'''

def AngleBetweenVecsSafe(a1, a2):
	try:
		return AngleBetweenVecs(a1,a2)
	except:
		return 180.0

# Copied from blender, we could wrap this! - BKE_curve.c
# But probably not toooo bad in python
def forward_diff_bezier(q0, q1, q2, q3, pointlist, steps, axis):
	f= float(steps)
	rt0= q0
	rt1= 3.0*(q1-q0)/f
	f*= f
	rt2= 3.0*(q0-2.0*q1+q2)/f
	f*= steps
	rt3= (q3-q0+3.0*(q1-q2))/f
	
	q0= rt0
	q1= rt1+rt2+rt3
	q2= 2*rt2+6*rt3
	q3= 6*rt3
	if axis == None:
		for a in xrange(steps+1):
			pointlist[a] = q0
			q0+= q1
			q1+= q2
			q2+= q3;
		
	else:
		for a in xrange(steps+1):
			pointlist[a][axis] = q0
			q0+= q1
			q1+= q2
			q2+= q3;

def points_from_bezier_seg(steps, pointlist, radlist, bez1_vec, bez2_vec, radius1, radius2):
	
	# x,y,z,axis
	for ii in (0,1,2):
		forward_diff_bezier(bez1_vec[1][ii], bez1_vec[2][ii],  bez2_vec[0][ii], bez2_vec[1][ii], pointlist, steps, ii)
	
	# radius - no axis, Copied from blenders BBone roll interpolation.
	forward_diff_bezier(radius1, radius1 + 0.390464*(radius2-radius1), radius2 - 0.390464*(radius2-radius1),	radius2,	radlist, steps, None)


def debug_pt(co):
	Blender.Window.SetCursorPos(tuple(co))
	Blender.Window.RedrawAll()
	print 'debugging', co

def freshMesh(mesh):
	'''
	Utility function to get a new mesh or clear the existing one, but dont clear everything.
	'''
	if mesh:
		materials = mesh.materials
		mesh.verts = None
		for group in mesh.getVertGroupNames():
			mesh.removeVertGroup(group) 
			
		# Add materials back
		mesh.materials = materials
	else:
		mesh = bpy.data.meshes.new()
		
	return mesh

def getObFromName(name):
	if name:
		try:	return bpy.data.objects[name]
		except:	return None
	else:
		return None

def getGroupFromName(name):
	if name:
		try:	return bpy.data.groups[name]
		except:	return None
	else:
		return None	

def closestVecIndex(vec, vecls):
	best= -1
	best_dist = 100000000
	for i, vec_test in enumerate(vecls):
		# Dont use yet, we may want to tho
		if vec_test: # Seems odd, but use this so we can disable some verts in the list.
			dist = (vec-vec_test).length
			if dist < best_dist:
				best = i
				best_dist = dist
	
	return best

IRATIONAL_NUM = 22.0/7.0
def next_random_num(rnd):
	'''
	return a random number between 0.0 and 1.0
	'''
	rnd[0] += (rnd[0] * IRATIONAL_NUM) % 1
	# prevent 
	if rnd[0] > 1000000:
		rnd[0]-=1000000
	return rnd[0] % 1

eul = 0.00001

BRANCH_TYPE_CURVE = 0
BRANCH_TYPE_GROWN = 1
BRANCH_TYPE_FILL = 2

class tree:
	def __init__(self):
		self.branches_all =		[]
		self.branches_root =	[]
		self.branches_twigs =	[]
		self.mesh = None
		self.armature = None
		self.objectCurve = None
		self.objectCurveMat = None
		self.objectCurveIMat = None
		
		self.objectTwigBounds = None # use for twigs only at the moment.
		self.objectTwigBoundsIMat = None
		self.objectTwigBoundsMat = None
		self.objectTwigBoundsMesh = None
		
		self.objectLeafBounds = None
		self.objectLeafBoundsIMat = None
		self.objectLeafBoundsMesh = None
		
		self.limbScale = 1.0
		
		self.debug_objects = []
		self.steps = 6 # defalt, curve overwrites
	
	def __repr__(self):
		s = ''
		s += '[Tree]'
		s += '  limbScale: %.6f' % self.limbScale
		s += '  object: %s' % self.objectCurve
		
		for brch in self.branches_root:
			s += str(brch)
		return s
	
	def fromCurve(self, objectCurve):
		# Now calculate the normals
		self.objectCurve = objectCurve
		self.objectCurveMat = objectCurve.matrixWorld
		self.objectCurveIMat = self.objectCurveMat.copy().invert()
		curve = objectCurve.data
		self.steps = curve.resolu # curve resolution
		
		# Set the curve object scale
		if curve.bevob:
			# A bit of a hack to guess the size of the curve object if you have one.
			bb = curve.bevob.boundingBox
			# self.limbScale = (bb[0] - bb[7]).length / 2.825 # THIS IS GOOD WHEN NON SUBSURRFED
			self.limbScale = (bb[0] - bb[7]).length / 1.8
		elif curve.ext2 != 0.0:
			self.limbScale = curve.ext2 * 1.5
			
		# forward_diff_bezier will fill in the blanks
		# nice we can reuse these for every curve segment :)
		pointlist = [[None, None, None] for i in xrange(self.steps+1)]
		radlist = [ None for i in xrange(self.steps+1) ]
		
		for spline in curve:
			
			if len(spline) < 2: # Ignore single point splines
				continue
			
			if spline.type != 1: # 0 poly, 1 bez, 4 nurbs
				GLOBALS['non_bez_error'] = 1
				continue
			
				
			brch = branch()
			brch.type = BRANCH_TYPE_CURVE
			
			
			
			bez_list = list(spline)
			for i in xrange(1, len(bez_list)):
				bez1 = bez_list[i-1]
				bez2 = bez_list[i]
				vec1 = bez1.vec
				vec2 = bez2.vec
				if abs(vec1[1][0]-vec2[1][0]) > 0.000001 or\
				   abs(vec1[1][1]-vec2[1][1]) > 0.000001 or\
				   abs(vec1[1][2]-vec2[1][2]) > 0.000001:

					points_from_bezier_seg(self.steps, pointlist, radlist, vec1, vec2, bez1.radius, bez2.radius)
				
					# remove endpoint for all but the last
					len_pointlist = len(pointlist)
					if i != len(bez_list)-1:
						len_pointlist -= 1
					
					brch.bpoints.extend([ bpoint(brch, Vector(pointlist[ii]), Vector(), radlist[ii] * self.limbScale) for ii in xrange(len_pointlist) ])
			
			# Finalize once point data is there
			if brch.bpoints:
				# if all points are in the same location, this is possible
				self.branches_all.append(brch)
				if brch.bpoints[0].radius < brch.bpoints[-1].radius: # This means we dont have to worry about curve direction.
					brch.bpoints.reverse()
				brch.calcData()
			
		# Sort from big to small, so big branches get priority
		self.branches_all.sort( key = lambda brch: -brch.bpoints[0].radius )
	
	
	def closestBranchPt(self, co):
		best_brch = None
		best_pt = None
		best_dist = 10000000000
		for brch in self.branches_all:
			for pt in brch.bpoints:
				# if pt.inTwigBounds: # only find twigs, give different results for leaves
				l = (pt.co-co).length
				if l < best_dist:
					best_dist = l
					best_brch = brch
					best_pt = pt
		return best_brch, best_pt
	
	def setTwigBounds(self, objectMesh):
		self.objectTwigBounds = objectMesh
		self.objectTwigBoundsMesh = objectMesh.getData(mesh=1)
		self.objectTwigBoundsMat = objectMesh.matrixWorld.copy()
		self.objectTwigBoundsIMat = self.objectTwigBoundsMat.copy().invert()
		
		for brch in self.branches_all:
			brch.calcTwigBounds(self)
			
	def setLeafBounds(self, objectMesh):
		self.objectLeafBounds = objectMesh
		self.objectLeafBoundsMesh = objectMesh.getData(mesh=1)
		self.objectLeafBoundsIMat = objectMesh.matrixWorld.copy().invert()
	
	def isPointInTwigBounds(self, co, selected_only=False):
		return self.objectTwigBoundsMesh.pointInside(co * self.objectCurveMat * self.objectTwigBoundsIMat, selected_only)

	def isPointInLeafBounds(self, co, selected_only=False):
		return self.objectLeafBoundsMesh.pointInside(co * self.objectCurveMat * self.objectLeafBoundsIMat, selected_only)

	def resetTags(self, value):
		for brch in self.branches_all:
			brch.tag = value
	
	def buildConnections(	self,\
							sloppy = 1.0,\
							connect_base_trim = 1.0,\
							do_twigs = False,\
							twig_ratio = 2.0,\
							twig_select_mode = 0,\
							twig_select_factor = 0.5,\
							twig_scale = 0.8,\
							twig_scale_width = 1.0,\
							twig_random_orientation = 180,\
							twig_random_angle = 33,\
							twig_recursive=True,\
							twig_recursive_limit=3,\
							twig_ob_bounds=None,\
							twig_ob_bounds_prune=True,\
							twig_ob_bounds_prune_taper=1.0,\
							twig_placement_maxradius=10.0,\
							twig_placement_maxtwig=0,\
							twig_follow_parent=0.0,\
							twig_follow_x=0.0,\
							twig_follow_y=0.0,\
							twig_follow_z=0.0,\
							do_variation = 0,\
							variation_seed = 1,\
							variation_orientation = 0.0,\
							variation_scale = 0.0,\
							do_twigs_fill = 0,\
							twig_fill_levels=4,\
							twig_fill_rand_scale=0.0,\
							twig_fill_fork_angle_max=180.0,\
							twig_fill_radius_min=0.1,\
							twig_fill_radius_factor=0.75,\
							twig_fill_shape_type=0,\
							twig_fill_shape_rand=0.0,\
							twig_fill_shape_power=0.3,\
						):
		'''
		build tree data - fromCurve must run first
		'''
		
		
		# Sort the branchs by the first radius, so big branchs get joins first
		### self.branches_all.sort( key = lambda brch: brch.bpoints[0].radius )
		
		#self.branches_all.reverse()
		
		# Connect branches
		for i in xrange(len(self.branches_all)):
			brch_i = self.branches_all[i]
			
			for j in xrange(len(self.branches_all)):
				if i != j:
					# See if any of the points match this branch
					# see if Branch 'i' is the child of branch 'j'
					
					brch_j = self.branches_all[j]
					
					if not brch_j.inParentChain(brch_i): # So we dont make cyclic tree!
						
						pt_best_j, dist = brch_j.findClosest(brch_i.bpoints[0].co)
						
						# Check its in range, allow for a bit out - hense the sloppy
						# The second check in the following IF was added incase the point is close enough to the line but the midpoint is further away
						# ...in this case the the resulting mesh will be adjusted to fit the join so its best to make it.
						if 	(dist <											pt_best_j.radius * sloppy)  or \
							((brch_i.bpoints[0].co - pt_best_j.co).length <	pt_best_j.radius * sloppy):
							
							
							brch_i.parent_pt = pt_best_j
							pt_best_j.childCount += 1 # dont remove me
							
							brch_i.baseTrim(connect_base_trim)
							
							'''
							if pt_best_j.childCount>4:
								raise "ERROR"
							'''
							
							# addas a member of best_j.children later when we have the geometry info available.
							
							#### print "Found Connection!!!", i, j
							break # go onto the next branch
		
		"""
			children = [brch_child for brch_child in pt.children]
			if children:
				# This pt is one side of the segment, pt.next joins this segment.
				# calculate the median point the 2 segments would spanal
				# Once this is done we need to adjust 2 things
				# 1) move both segments up/down so they match the branches best.
				# 2) set the spacing of the segments around the point.
				
		
		# First try to get the ideal some space around each joint
		# the spacing shoule be an average of 
		for brch.bpoints:
		"""
		
		'''
		for brch in self.branches_all:
			brch.checkPointList()
		'''
		
		# Variations - use for making multiple versions of the same tree.
		if do_variation:
			irational_num = 22.0/7.0 # use to make the random number more odd
			rnd = [variation_seed]
			
			# Add children temporarily
			for brch in self.branches_all:
				if brch.parent_pt:
					rnd_rot = ((next_random_num(rnd) * variation_orientation) - 0.5) * 720
					mat_orientation = RotationMatrix(rnd_rot, 3, 'r', brch.parent_pt.no)
					rnd_sca = 1 + ((next_random_num(rnd)-0.5)* variation_scale )
					mat_scale = Matrix([rnd_sca,0,0],[0,rnd_sca,0],[0,0,rnd_sca])
					# mat_orientation = RotationMatrix(0, 3, 'r', brch.parent_pt.no)
					brch.transformRecursive(self, mat_scale * mat_orientation, brch.parent_pt.co)
		
		if (do_twigs or do_twigs_fill) and twig_ob_bounds: # Only spawn twigs inside this mesh
			self.setTwigBounds(twig_ob_bounds)
		
		# Important we so this with existing parent/child but before connecting and calculating verts.
		if do_twigs:
			
			# scale values down
			twig_random_orientation= twig_random_orientation/360.0
			twig_random_angle= twig_random_angle/360.0
			
			irational_num = 22.0/7.0 # use to make the random number more odd
			
			if not twig_recursive:
				twig_recursive_limit = 0
			
			self.buildTwigs(twig_ratio, twig_select_mode, twig_select_factor)
			
			branches_twig_attached = []
			
			# This wont add all! :/
			brch_twig_index = 0
			brch_twig_index_LAST = -1 # use this to prevent in inf loop, since its possible we cant place every branch
			while brch_twig_index < len(self.branches_twigs) and brch_twig_index_LAST != brch_twig_index:
				###print "While"
				### print brch_twig_index, len(self.branches_twigs) # if this dosnt change, quit the while
				
				brch_twig_index_LAST = brch_twig_index
				
				# new twigs have been added, recalculate
				branches_twig_sort = [brch.bestTwigSegment() for brch in self.branches_all]
				branches_twig_sort.sort() # this will sort the branches with best braches for adding twigs to at the start of the list
				
				for tmp_sortval, twig_pt_index, brch_parent in branches_twig_sort: # tmp_sortval is not used.
					if		twig_pt_index != -1 and \
							(twig_recursive_limit == 0 or brch_parent.generation < twig_recursive_limit) and \
							(twig_placement_maxtwig == 0 or brch_parent.twig_count < twig_placement_maxtwig) and \
							brch_parent.bpoints[twig_pt_index].radius < twig_placement_maxradius:
						
						if brch_twig_index >= len(self.branches_twigs):
							break
						
						brch_twig = self.branches_twigs[brch_twig_index]
						parent_pt = brch_parent.bpoints[twig_pt_index]
						
						brch_twig.parent_pt = parent_pt
						parent_pt.childCount += 1
						
						# Scale this twig using this way...
						# The size of the parent, scaled by the parent point's radius,
						# ...compared to the parent branch;s root point radius.
						# Also take into account the length of the parent branch
						# Use this for pretend random numbers too.
						scale = twig_scale * (parent_pt.branch.bpoints[0].radius / brch_twig.bpoints[0].radius) * (parent_pt.radius / parent_pt.branch.bpoints[0].radius)
						
						# Random orientation
						# THIS IS NOT RANDOM - Dont be real random so we can always get re-produceale results.
						if twig_random_orientation:	rnd1 = (((irational_num * scale * 10000000) % 360) - 180) * twig_random_orientation
						else:						rnd1 = 0.0
						if twig_random_angle:		rnd2 = (((irational_num * scale * 66666666) % 360) - 180) * twig_random_angle
						else:						rnd2 = 0.0
						
						# Align this with the existing branch
						angle = AngleBetweenVecsSafe(zup, parent_pt.no)
						cross = CrossVecs(zup, parent_pt.no)
						mat_align = RotationMatrix(angle, 3, 'r', cross)
						
						# Use the bend on the point to work out which way to make the branch point!
						if parent_pt.prev:	cross = CrossVecs(parent_pt.no, parent_pt.prev.no - parent_pt.no)
						else:				cross = CrossVecs(parent_pt.no, parent_pt.next.no - parent_pt.no)
						
						if parent_pt.branch.parent_pt:
							angle = AngleBetweenVecsSafe(parent_pt.branch.parent_pt.no, parent_pt.no)
						else:
							# Should add a UI for this... only happens when twigs come off a root branch
							angle = 66
						
						mat_branch_angle = RotationMatrix(angle+rnd1, 3, 'r', cross)
						mat_scale = Matrix([scale,0,0],[0,scale,0],[0,0,scale])
						
						mat_orientation = RotationMatrix(rnd2, 3, 'r', parent_pt.no)
						
						if twig_scale_width != 1.0:
							# adjust length - no radius adjusting
							for pt in brch_twig.bpoints:
								pt.radius *= twig_scale_width
						
						brch_twig.transform(mat_scale * mat_branch_angle * mat_align * mat_orientation, parent_pt.co)
						
						# Follow the parent normal
						if twig_follow_parent or twig_follow_x or twig_follow_y or twig_follow_z:
							
							vecs = []
							brch_twig_len = float(len(brch_twig.bpoints))
							
							if twig_follow_parent:
								no = parent_pt.no.copy() * twig_follow_parent
							else:
								no = Vector()
							
							no.x += twig_follow_x
							no.y += twig_follow_y
							no.z += twig_follow_z
							
							for i, pt in enumerate(brch_twig.bpoints):
								if pt.prev:
									fac = i / brch_twig_len
									
									# Scale this value
									fac_inv = 1-fac
									
									no_orig = pt.co - pt.prev.co
									len_orig = no_orig.length
									
									no_new = (fac_inv * no_orig) + (fac * no)
									no_new.length = len_orig
									
									# Mix the 2 normals
									vecs.append(no_new)
									
							# Apply the coords
							for i, pt in enumerate(brch_twig.bpoints):
								if pt.prev:
									pt.co = pt.prev.co + vecs[i-1]
							
							brch_twig.calcPointExtras()
						
						
						# When using a bounding mesh, clip and calculate points in bounds.
						#print "Attempting to trim base"
						brch_twig.baseTrim(connect_base_trim)
						
						if twig_ob_bounds and (twig_ob_bounds_prune or twig_recursive):
							brch_twig.calcTwigBounds(self)
						
							# we would not have been but here if the bounds were outside
							if twig_ob_bounds_prune:
								brch_twig.boundsTrim()
								if twig_ob_bounds_prune_taper != 1.0:
									# taper to a point. we could use some nice taper algo here - just linear atm.
									
									brch_twig.taper(twig_ob_bounds_prune_taper)
						
						# Make sure this dosnt mess up anything else
						
						brch_twig_index += 1
						
						# Add to the branches
						#self.branches_all.append(brch_twig)
						if len(brch_twig.bpoints) > 2:
							branches_twig_attached.append(brch_twig)
							brch_twig.generation = brch_parent.generation + 1
							brch_parent.twig_count += 1
						else:
							# Dont add the branch
							parent_pt.childCount -= 1
				
				# Watch This! - move 1 tab down for no recursive twigs
				if twig_recursive:
					self.branches_all.extend(branches_twig_attached)
					branches_twig_attached = []
			
			if not twig_recursive:
				self.branches_all.extend(branches_twig_attached)
				branches_twig_attached = []
		
		
		if do_twigs_fill and twig_ob_bounds:
			self.twigFill(\
				twig_fill_levels,\
				twig_fill_rand_scale,\
				twig_fill_fork_angle_max,\
				twig_fill_radius_min,\
				twig_fill_radius_factor,\
				twig_fill_shape_type,\
				twig_fill_shape_rand,\
				twig_fill_shape_power,\
			)
		
		### self.branches_all.sort( key = lambda brch: brch.parent_pt != None )
		
		# Calc points with dependancies
		# detect circular loops!!! - TODO
		#### self.resetTags(False) # NOT NEEDED NOW
		done_nothing = False
		while done_nothing == False:
			done_nothing = True
			
			for brch in self.branches_all:
				
				if brch.tag == False and (brch.parent_pt == None or brch.parent_pt.branch.tag == True):
					# Assign this to a spesific side of the parents point
					# we know this is a child but not which side it should be attached to.
					if brch.parent_pt:
						
						child_locs = [\
						brch.parent_pt.childPointUnused(0),\
						brch.parent_pt.childPointUnused(1),\
						brch.parent_pt.childPointUnused(2),\
						brch.parent_pt.childPointUnused(3)]
						
						best_idx = closestVecIndex(brch.bpoints[0].co, child_locs)
						
						# best_idx could be -1 if all childPoint's are used however we check for this and dont allow it to happen.
						#if best_idx==-1:
						#	raise "Error"z
						brch.parent_pt.children[best_idx] = brch
					
					for pt in brch.bpoints:
						pt.calcVerts()
					
					done_nothing = False
					brch.tag = True
		
		'''
		for i in xrange(len(self.branches_all)):
			brch_i = self.branches_all[i]
			print brch_i.myindex,
			print 'tag', brch_i.tag,
			print 'parent is',
			if brch_i.parent_pt:
				print brch_i.parent_pt.branch.myindex
			else:
				print None
		'''
	
	def optimizeSpacing(self, seg_density=0.5, seg_density_angle=20.0, seg_density_radius=0.3, joint_compression=1.0, joint_smooth=1.0):
		'''
		Optimize spacing, taking branch hierarchy children into account,
		can add or subdivide segments so branch joins dont look horrible.
		'''
		for brch in self.branches_all:
			brch.evenJointDistrobution(joint_compression)
		
		# Correct points that were messed up from sliding
		# This happens when one point is pushed past another and the branch gets an overlaping line
		
		for brch in self.branches_all:
			brch.fixOverlapError(joint_smooth)
		
		
		# Collapsing
		for brch in self.branches_all:
			brch.collapsePoints(seg_density, seg_density_angle, seg_density_radius, joint_smooth)
			
		'''
		for brch in self.branches_all:
			brch.branchReJoin()
		'''
	
	def twigFill(self_tree,\
			twig_fill_levels,\
			twig_fill_rand_scale,\
			twig_fill_fork_angle_max,\
			twig_fill_radius_min,\
			twig_fill_radius_factor,\
			twig_fill_shape_type,\
			twig_fill_shape_rand,\
			twig_fill_shape_power,\
		):
		'''
		Fill with twigs, this function uses its own class 'segment'
		
		twig_fill_shape_type;
			0 - no child smoothing
			1 - smooth one child
			2 - smooth both children
		
		'''
		
		rnd = [1]
		
		segments_all = []
		segments_level = []
		
		# Only for testing
		def preview_curve():
			TWIG_WIDTH_MAX = 1.0
			TWIG_WIDTH_MIN = 0.1
			cu = bpy.data.curves["cu"]
			# remove all curves
			while len(cu):
				del cu[0]
			# return
			
			cu.setFlag(1)
			cu.ext2 = 0.01
			
			WIDTH_STEP = (TWIG_WIDTH_MAX-TWIG_WIDTH_MIN) / twig_fill_levels
			
			for i, seg in enumerate(segments_all):
				
				# 1 is the base and 2 is the tail
				
				p1_h2 = seg.getHeadHandle() # isnt used
				p1_co = seg.headCo
				p1_h1 = seg.getHeadHandle()
				
				p2_h1 = seg.getTailHandle()
				
				p2_co = seg.tailCo
				p2_h2 = seg.tailCo # isnt used
				
				bez1 = Blender.BezTriple.New([ p1_h1[0], p1_h1[1], p1_h1[2], p1_co[0], p1_co[1], p1_co[2], p1_h2[0], p1_h2[1], p1_h2[2] ])
				bez2 = Blender.BezTriple.New([ p2_h1[0], p2_h1[1], p2_h1[2], p2_co[0], p2_co[1], p2_co[2], p2_h2[0], p2_h2[1], p2_h2[2] ])
				bez1.handleTypes = bez2.handleTypes = [Blender.BezTriple.HandleTypes.FREE, Blender.BezTriple.HandleTypes.FREE]
				
				bez1.radius = TWIG_WIDTH_MIN + (WIDTH_STEP * (seg.levelFromLeaf+1))
				bez2.radius = TWIG_WIDTH_MIN + (WIDTH_STEP * seg.levelFromLeaf)
				
				cunurb = cu.appendNurb(bez1)
				cunurb.append(bez2)
				
				# This sucks
				for bez in cunurb:
					bez.handleTypes = [Blender.BezTriple.HandleTypes.FREE, Blender.BezTriple.HandleTypes.FREE]
			
			### cc = sce.objects.new( cu )
			cu.update()
		
		
		def mergeCo(parentCo, ch1Co, ch2Co, twig_fill_shape_rand):
			if twig_fill_shape_rand==0.0:
				return (parentCo + ch1Co + ch2Co) / 3.0
			else:
				
				w1 = (next_random_num(rnd)*twig_fill_shape_rand) + (1-twig_fill_shape_rand)
				w2 = (next_random_num(rnd)*twig_fill_shape_rand) + (1-twig_fill_shape_rand)
				w3 = (next_random_num(rnd)*twig_fill_shape_rand) + (1-twig_fill_shape_rand)
				wtot = w1+w2+w3
				w1=w1/wtot
				w2=w2/wtot
				w3=w3/wtot
				
				# return (parentCo*w1 + ch1Co*w2 + ch2Co*w2)
				co1 = (parentCo * w1)	+ (ch1Co * (1.0-w1))
				co2 = (ch1Co * w2)		+ (ch2Co * (1.0-w2))
				co3 = (ch2Co * w3)		+ (parentCo * (1.0-w3))
				
				return (co1 + co2 + co3) / 3.0
				
				
		
		class segment:
			def __init__(self, level):
				self.headCo = Vector()
				self.tailCo = Vector()
				self.parent = None
				self.mergeCount = 0
				self.levelFromLeaf = level # how far we are from the leaf in levels
				self.levelFromRoot = -1 # set later, assume root bone
				self.children = []
				segments_all.append(self)
				
				if level >= len(segments_level):	segments_level.append([self])
				else:								segments_level[level].append(self)
				
				self.brothers = []
				self.no = Vector() # only endpoints have these
				# self.id = len(segments_all)
				
				# First value is the bpoint,
				# Second value is what to do -
				#  0 - dont join
				#  1 - Join to parent (tree point)
				#  2 - join to parent, point from another fill-twig branch we just created.
				
				self.bpt = (None, False) # branch point for root segs only
				self.new_bpt = None
				
				self.used = False # use this to tell if we are apart of a branch
			
			def sibling(self):
				i = self.parent.children.index(self)
				
				if i == 0:
					return self.parent.children[ 1 ]
				elif i == 1:
					return self.parent.children[ 0 ]
				else:
					raise "error"
					
			
			def getHeadHandle(self):
				"""
				For Bezier only
				"""
				
				if not self.parent:
					return self.headCo
				
				if twig_fill_shape_type == 0: # no smoothing
					return self.headCo
				elif twig_fill_shape_type == 1:
					if self.parent.children[1] == self:
						return self.headCo
				# 2 - always do both
				
				
				# Y shape with curve? optional
				
				# we have a parent but it has no handle direction, easier
				if not self.parent.parent:	no = self.parent.headCo - self.parent.tailCo
				else:						no = self.parent.parent.headCo-self.parent.tailCo
				
				no.length =  self.getLength() * twig_fill_shape_power
				# Ok we have to account for the parents handle
				return self.headCo - no
				# return self.headCo - Vector(1, 0,0)
			
			def getTailHandle(self):
				"""
				For Bezier only
				"""
				if self.parent:
					no = self.parent.headCo-self.tailCo
					no.length = self.getLength() * twig_fill_shape_power
					return self.tailCo + no
				else:
					return self.tailCo # isnt used
			
			def getRootSeg(self):
				seg = self
				while seg.parent:
					seg = seg.parent
				
				return seg
			
			def calcBrothers(self):
				# Run on children first
				self.brothers.extend( \
					[seg_child_sibling.parent \
						for seg_child in self.children \
						for seg_child_sibling in seg_child.brothers \
						if seg_child_sibling.parent not in (self, None)]\
					)
					#print self.brothers
			
			def calcLevelFromRoot(self):
				if self.parent:
					self.levelFromRoot = self.parent.levelFromRoot + 1
				
				for seg_child in self.children:
					seg_child.calcLevelFromRoot()
					
			# Dont use for now, but scale worked, transform was never tested.
			"""
			def transform(self, matrix):
				self.headCo = self.headCo * matrix
				self.tailCo = self.tailCo * matrix
				
				if self.children:
					ch1 = self.children[0]
					ch2 = self.children[1]
					
					ch1.transform(matrix)
					ch2.transform(matrix)
			
			def scale(self, scale, cent=None):
				# scale = 0.9
				#matrix = Matrix([scale,0,0],[0,scale,0],[0,0,scale]).resize4x4()
				#self.transform(matrix)
				if cent == None: # first iter
					cent = self.headCo
					self.tailCo = ((self.tailCo-cent) * scale) + cent
				else:
					self.headCo = ((self.headCo-cent) * scale) + cent
					self.tailCo = ((self.tailCo-cent) * scale) + cent
				
				if self.children:
					self.children[0].scale(scale, cent)
					self.children[1].scale(scale, cent)
			"""
			def recalcChildLoc(self):
				if not self.children:
					return
				ch1 = self.children[0]
				ch2 = self.children[1]
				new_mid = mergeCo(self.headCo, ch1.tailCo, ch2.tailCo, twig_fill_shape_rand)
				
				self.tailCo[:] = ch1.headCo[:] = ch2.headCo[:] = new_mid
				
				ch1.recalcChildLoc()
				ch2.recalcChildLoc()
			
			def merge(self, other):
				"""
				Merge other into self and make a new segment
				"""
				"""
				seg_child = segment(self.levelFromLeaf)
				self.levelFromLeaf += 1
				
				seg_child.parent = other.parent = self
				
				# No need, recalcChildLoc sets the other coords
				#self.parent.tailCo = (self.headCo + self.tailCo + other.tailCo) / 3.0
				#self.parent.headCo[:] = self.headCo
				
				seg_child.headCo[:] = self.headCo
				
				# isect = LineIntersect(self.headCo, self.tailCo, other.headCo, other.tailCo)
				# new_head = (isect[0]+isect[1]) * 0.5
				
				seg_child.mergeCount += 1
				other.mergeCount += 1
				
				self.children.extend([ seg_child, other ])
				
				self.recalcChildLoc()
				
				# print 'merging', self.id, other.id
				"""
				
				#new_head = (self.headCo + self.tailCo + other.headCo + other.tailCo) * 0.25
				
				self.parent = other.parent = segment(self.levelFromLeaf + 1)
				
				# No need, recalcChildLoc sets the self.parent.tailCo
				# self.parent.tailCo = (self.headCo + self.tailCo + other.tailCo) / 3.0
				
				self.parent.headCo[:] = self.headCo
				self.parent.bpt = self.bpt
				self.bpt = (None, False)
				
				# isect = LineIntersect(self.headCo, self.tailCo, other.headCo, other.tailCo)
				# new_head = (isect[0]+isect[1]) * 0.5
				
				self.mergeCount += 1
				other.mergeCount += 1
				
				self.parent.children.extend([ self, other ])
				
				self.parent.recalcChildLoc()
				# print 'merging', self.id, other.id
				
				
			def findBestMerge(self, twig_fill_fork_angle_max):
				# print "findBestMerge"
				if self.parent != None:
					return
				
				best_dist = 1000000
				best_seg = None
				for seg_list in (self.brothers, segments_level[self.levelFromLeaf]):
					#for seg_list in (segments_level[self.levelFromLeaf],):
					
					# only use all other segments if we cant find any from our brothers
					if seg_list == segments_level[self.levelFromLeaf] and best_seg != None:
						break
					
					for seg in seg_list:
						# 2 ppoint join 
						if seg != self and seg.mergeCount == 0 and seg.parent == None:
							
							# find the point they would join	
							test_dist = (self.tailCo - seg.tailCo).length
							if test_dist < best_dist:
								if twig_fill_fork_angle_max > 179:
									best_dist = test_dist
									best_seg = seg
								else:
									# Work out if the desired angle range is ok.
									mco = mergeCo( self.headCo, self.tailCo, seg.tailCo, 0.0 ) # we dont want the random value for this test
									ang = AngleBetweenVecsSafe(self.tailCo-mco, seg.tailCo-mco)
									if ang < twig_fill_fork_angle_max:
										best_dist = test_dist
										best_seg = seg
				return best_seg
			
			def getNormal(self):
				return (self.headCo - self.tailCo).normalize()
			
			def getLength(self):
				return (self.headCo - self.tailCo).length
			"""
			def toMatrix(self, LEAF_SCALE, LEAF_RANDSCALE, LEAF_RANDVEC):
				if LEAF_RANDSCALE:	scale = LEAF_SCALE * Rand(1.0-LEAF_RANDSCALE, 1.0+LEAF_RANDSCALE)
				else:				scale = LEAF_SCALE * 1.0
				
				if LEAF_RANDVEC:	rand_vec = Vector( Rand(-1, 1), Rand(-1, 1), Rand(-1, 1) ).normalize() * LEAF_RANDVEC
				else:				rand_vec = Vector( )
				
				return Matrix([scale,0,0],[0,scale,0],[0,0,scale]).resize4x4() * (self.no + rand_vec).toTrackQuat('x', 'z').toMatrix().resize4x4() * TranslationMatrix(self.tailCo)
			"""
		def distripute_seg_on_mesh(me__, face_group):
			"""
			add segment endpoints
			"""
			
			vert_segment_mapping = {}
			for f in face_group:
				for v in f:
					i = v.index
					if i not in vert_segment_mapping:
						vert_segment_mapping[i] = len(segments_all)
						v.sel = True
						seg = segment(0)
						# seg.tailCo = v.co.copy() # headCo undefined atm.
						seg.tailCo = v.co.copy() * self_tree.objectTwigBoundsMat * self_tree.objectCurveIMat
						
						# self_tree.objectCurveMat
						
						seg.no = v.no
			
			# Build connectivity
			for ed in me__.edges:
				if ed.v1.sel and ed.v2.sel:
					i1,i2 = ed.key
					i1 = vert_segment_mapping[i1]
					i2 = vert_segment_mapping[i2]
					
					segments_all[i1].brothers.append( segments_all[i2] )
					segments_all[i2].brothers.append( segments_all[i1] )
			
			# Dont need to return anything, added when created.
		
		def set_seg_attach_point(seg, interior_points, twig_fill_rand_scale):
			"""
			Can only run on end nodes that have normals set
			"""
			best_dist = 1000000000.0
			best_point = None
			
			co = seg.tailCo
			
			for pt in interior_points:
				# line from the point to the seg endpoint
				
				line_normal = seg.tailCo - pt.nextMidCo
				l = line_normal.length
				
				
				cross1 = CrossVecs( seg.no, line_normal )
				cross2 = CrossVecs( pt.no, line_normal )
				
				angle_line = min(AngleBetweenVecsSafe(cross1, cross2), AngleBetweenVecsSafe(cross1, -cross2))
				angle_leaf_no_diff = min(AngleBetweenVecsSafe(line_normal, seg.no), AngleBetweenVecsSafe(line_normal, -seg.no))
				
				# BEST_ANG=66.0
				# angle = 66.0 # min(AngleBetweenVecs(v2_co-v1_co, leaf.co-cc), AngleBetweenVecs(v1_co-v2_co, leaf.co-cc))
				# print angle, angle2
				# l = (l * ((1+abs(angle-BEST_ANG))**2 )) / (1+angle_line)
				l = (1+(angle_leaf_no_diff/180)) * (1+(angle_line/180)) * l
				
				if l < best_dist:
					best_pt = pt
					best_co = pt.nextMidCo
					
					best_dist = l
	
			# twig_fill_rand_scale
			seg.headCo = best_co.copy()
			
			if twig_fill_rand_scale:
				seg_dir = seg.tailCo - seg.headCo
				
				seg_dir.length = seg_dir.length * ( 1.0 - (next_random_num(rnd)*twig_fill_rand_scale) )
				seg.tailCo = seg.headCo + seg_dir
			
			
			if best_pt.childCount < 4:
				# Watch this!!! adding a user before its attached and the branch is created!
				# make sure if its not added later on, this isnt left added
				best_pt.childCount += 1
				
				# True/False denotes weather we try to connect to our parent branch
				seg.bpt = (best_pt, True)
			else:
				seg.bpt = (best_pt, False)
				
			return True


		# END Twig code, next add them
		
		
		"""
		Uses a reversed approch, fill in twigs from a bounding mesh
		"""
		# print "twig_fill_fork_angle_max"
		# twig_fill_fork_angle_max = 60.0 # 
		# forward_diff_bezier will fill in the blanks
		# nice we can reuse these for every curve segment :)
		pointlist = [[None, None, None] for i in xrange(self_tree.steps+1)]
		radlist = [ None for i in xrange(self_tree.steps+1) ]
		
		orig_branch_count = len(self_tree.branches_all)
		
		for face_group in BPyMesh.mesh2linkedFaces(self_tree.objectTwigBoundsMesh):
			# Set the selection to do point inside.
			self_tree.objectTwigBoundsMesh.sel = False
			for f in face_group: f.sel = True
			
			interior_points = []
			interior_normal = Vector()
			for i, brch in enumerate(self_tree.branches_all):
				
				if i == orig_branch_count:
					break # no need to check new branches are inside us
					
				for pt in brch.bpoints:
					if pt.next and pt.childCount < 4: # cannot attach to the last points
						if self_tree.isPointInTwigBounds(pt.co, True): # selected_only
							interior_points.append(pt)
							interior_normal += pt.no * pt.radius
			
			segments_all[:] = []
			segments_level[:] = []
			
			if interior_points:
				# Ok, we can add twigs now
				distripute_seg_on_mesh( self_tree.objectTwigBoundsMesh, face_group )
				
				for seg in segments_level[0]: # only be zero segments
					# Warning, increments the child count for bpoints we attach to!!
					set_seg_attach_point(seg, interior_points, twig_fill_rand_scale)
				
				# Try sorting by other properties! this is ok for now
				for segments_level_current in segments_level:
					segments_level_current.sort( key = lambda seg:	-(seg.headCo-seg.tailCo).length )
				
				for level in xrange(twig_fill_levels):
					if len(segments_level) > level:
						for seg in segments_level[level]:
							# print level, seg.brothers
							if seg.mergeCount == 0:
								seg_merge = seg.findBestMerge(twig_fill_fork_angle_max)
								if seg_merge:
									seg.merge( seg_merge )
					
					if len(segments_level) > level+1:
						for seg in segments_level[level+1]:
							seg.calcBrothers()
				
				for seg in segments_all:
					if seg.parent == None:
						seg.levelFromRoot = 0
						seg.calcLevelFromRoot()
				
				'''
				for i, seg in enumerate(segments_all):	
					# Make a branch from this data!
					
					brch = branch()
					brch.type = BRANCH_TYPE_FILL
					self_tree.branches_all.append(brch)
					
					# ============================= do this per bez pair
					# 1 is the base and 2 is the tail
					
					#p1_h1 = seg.getHeadHandle()
					p1_co = seg.headCo.copy()
					p1_h2 = seg.getHeadHandle() # isnt used
					
					p2_h1 = seg.getTailHandle()
					p2_co = seg.tailCo.copy()
					#p2_h2 = seg.getTailHandle() # isnt used
					
					
					bez1_vec = (None, p1_co, p1_h2)
					bez2_vec = (p2_h1, p2_co, None)
					
					seg_root = seg.getRootSeg()
					
					radius_root = seg_root.bpt.radius * twig_fill_radius_factor
					# Clamp so the head is never smaller then the tail
					if radius_root < twig_fill_radius_min: radius_root = twig_fill_radius_min
						
					if seg_root.levelFromLeaf:
						# print seg_root.levelFromLeaf, seg.levelFromRoot
						WIDTH_STEP = (radius_root - twig_fill_radius_min) / (seg_root.levelFromLeaf+1)
						
						radius1 = twig_fill_radius_min + (WIDTH_STEP * (seg.levelFromLeaf+1))
						if seg.levelFromLeaf:	radius2 = twig_fill_radius_min + (WIDTH_STEP * seg.levelFromLeaf)
						else:					radius2 = twig_fill_radius_min
					else:
						radius1 = radius_root
						radius2 = twig_fill_radius_min 
					
					
					points_from_bezier_seg(self_tree.steps, pointlist, radlist, bez1_vec, bez2_vec, radius1, radius2)
					
					# dont apply self_tree.limbScale here! - its alredy done
					bpoints = [ bpoint(brch, Vector(pointlist[ii]), Vector(), radlist[ii]) for ii in xrange(len(pointlist)) ]
					
					# remove endpoint for all but the last
					#if i != len(bez_list)-1:
					#	bpoints.pop()
					
					brch.bpoints.extend(bpoints)
					# =============================
					
					# Finalize once point data is there
					brch.calcData()
				#
				#preview_curve()
				'''
			
				for segments_level_current in reversed(segments_level):
					for seg in segments_level_current:
						if seg.used == False and (seg.parent == None or seg.parent.used == True):
							
							# The root segment for this set of links.
							# seg_root_linked = seg
							
							brch = branch()
							brch.type = BRANCH_TYPE_FILL
							self_tree.branches_all.append(brch)
							
							# Can we attach to a real branch?
							if seg.parent == None:
								if seg.bpt[1]: # we can do a real join into the attach point
									brch.parent_pt = seg.bpt[0]
									# brch.parent_pt.childCount # this has alredy changed from 
							
							'''
							if seg.parent:
								if seg.bpt[1] == 2:
									#if seg.bpt[1]:
									# print "Making Connection"
									if seg.bpt[0] == None:
										raise "Error"
									if seg.bpt[1] != 2:
										print seg.bpt[1]
										raise "Error"
									
									brch.parent_pt = seg.bpt[1]
									brch.parent_pt.childCount += 1
									if brch.parent_pt.childCount > 4:
										raise "Aeeae"
									print "\n\nM<aking Joint!!"
							'''
							
							if seg.parent:
								sibling = seg.sibling()
								if sibling.new_bpt:
									if sibling.new_bpt.childCount < 4:
										brch.parent_pt = sibling.new_bpt
										brch.parent_pt.childCount +=1
							
							# Go down the hierarhy
							is_first = True
							while seg != None:
								seg.used = True
								
								# ==============================================
								
								#p1_h1 = seg.getHeadHandle()
								p1_co = seg.headCo.copy()
								p1_h2 = seg.getHeadHandle() # isnt used
								
								p2_h1 = seg.getTailHandle()
								p2_co = seg.tailCo.copy()
								#p2_h2 = seg.getTailHandle() # isnt used
								
								
								bez1_vec = (None, p1_co, p1_h2)
								bez2_vec = (p2_h1, p2_co, None)
								
								seg_root = seg.getRootSeg()
								
								radius_root = seg_root.bpt[0].radius * twig_fill_radius_factor
								# Clamp so the head is never smaller then the tail
								if radius_root < twig_fill_radius_min: radius_root = twig_fill_radius_min
									
								if seg_root.levelFromLeaf:	
									# print seg_root.levelFromLeaf, seg.levelFromRoot
									widthStep = (radius_root - twig_fill_radius_min) / (seg_root.levelFromLeaf+1)
									
									radius1 = twig_fill_radius_min + (widthStep * (seg.levelFromLeaf+1))
									if seg.levelFromLeaf:	radius2 = twig_fill_radius_min + (widthStep * seg.levelFromLeaf)
									else:					radius2 = twig_fill_radius_min
								else:
									radius1 = radius_root
									radius2 = twig_fill_radius_min 
								
								points_from_bezier_seg(self_tree.steps, pointlist, radlist, bez1_vec, bez2_vec, radius1, radius2)
								
								
								start_pointlist = 0
								
								# This is like baseTrim, (remove the base points to make nice joins, accounting for radius of parent point)
								# except we do it before the branch is made
								
								if brch.parent_pt:
									while len(pointlist) - start_pointlist > 2 and (Vector(pointlist[start_pointlist]) - brch.parent_pt.co).length < (brch.parent_pt.radius*2):
										start_pointlist +=1

								if is_first and brch.parent_pt:
									# We need to move the base point to a place where it looks good on the parent branch
									# to do this. move the first point, then remove the following points that look horrible (double back on themself)
									
									no = Vector(pointlist[0]) - Vector(pointlist[-1])
									no.length = brch.parent_pt.radius*2
									pointlist[0] = list(Vector(pointlist[0]) - no)
									
									"""
									pointlist[1][0] = (pointlist[0][0] + pointlist[2][0])/2.0
									pointlist[1][1] = (pointlist[0][1] + pointlist[2][1])/2.0
									pointlist[1][2] = (pointlist[0][2] + pointlist[2][2])/2.0
									
									pointlist[2][0] = (pointlist[1][0] + pointlist[3][0])/2.0
									pointlist[2][1] = (pointlist[1][1] + pointlist[3][1])/2.0
									pointlist[2][2] = (pointlist[1][2] + pointlist[3][2])/2.0
									"""
									
									
								# Done setting the start point
									
									
								len_pointlist = len(pointlist)
								if seg.children:
									len_pointlist -= 1
								
								# dont apply self_tree.limbScale here! - its alredy done
								bpoints = [ bpoint(brch, Vector(pointlist[ii]), Vector(), radlist[ii]) for ii in xrange(start_pointlist, len_pointlist) ]
								brch.bpoints.extend( bpoints )
								# ==============================================
								
								seg.new_bpt = bpoints[0]
								
								if seg.children:
									seg = seg.children[0]
								else:
									seg = None
								
								is_first = False
								
							# done adding points
							brch.calcData()
							
							
							
		
	def buildTwigs(self, twig_ratio, twig_select_mode, twig_select_factor):
		
		ratio_int = int(len(self.branches_all) * twig_ratio)
		if ratio_int == 0:
			return
		
		# So we only mix branches of similar lengths
		branches_sorted = self.branches_all[:]
		
		# Get the branches based on our selection method!
		if twig_select_mode==0:
			branches_sorted.sort( key = lambda brch: brch.getLength())
		elif twig_select_mode==1:
			branches_sorted.sort( key = lambda brch:-brch.getLength())
		elif twig_select_mode==2:
			branches_sorted.sort( key = lambda brch:brch.getStraightness())
		elif twig_select_mode==3:
			branches_sorted.sort( key = lambda brch:-brch.getStraightness())
		
		factor_int = int(len(self.branches_all) * twig_select_factor)
		branches_sorted[factor_int:] = []  # remove the last part of the list
		
		branches_sorted.sort( key = lambda brch: len(brch.bpoints))
		
		branches_new = []
		#for i in xrange(ratio_int):
		tot_twigs = 0
		
		step = 1
		while tot_twigs < ratio_int and step < len(branches_sorted):
			# Make branches from the existing
			for j in xrange(step, len(branches_sorted)):
				brch = branches_sorted[j-step].mixToNew(branches_sorted[j], None)
				branches_new.append( brch )
				tot_twigs +=1
				
				if tot_twigs > ratio_int:
					break
		
		### print "TwigCount", len(branches_new), ratio_int
		
		self.branches_twigs = branches_new
	
	def toDebugDisplay(self):
		'''
		Should be able to call this at any time to see whats going on, dosnt work so nice ATM.
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
		
		
	
	def toMesh(self, mesh=None,\
			do_uv=True,\
			do_uv_keep_vproportion=True,\
			do_uv_vnormalize=False,\
			do_uv_uscale=False,\
			uv_image = None,\
			uv_x_scale=1.0,\
			uv_y_scale=4.0,\
			do_uv_blend_layer= False,\
			do_cap_ends=False,\
		):
		
		self.mesh = freshMesh(mesh)
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
					brch.bpoints[0].roll_angle = 0.0
					pass
				else:
					# our roll was set from the branches parent and needs no changing
					# set it to zero so the following functions know to interpolate.
					brch.bpoints[0].roll_angle = 0.0
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
		
		#faces_extend = [ face for brch in self.branches_all for pt in brch.bpoints for face in pt.faces if face ]
		
		
		
		faces_extend = []
		for brch in self.branches_all:
			if brch.parent_pt:
				faces_extend.extend(brch.faces)
			for pt in brch.bpoints:
				for face in pt.faces:
					if face:
						faces_extend.append(face)
		
		if do_cap_ends:
			# TODO - UV map and image?
			faces_extend.extend([ brch.bpoints[-1].verts for brch in self.branches_all ])
		
		faces = self.mesh.faces

		faces.extend(faces_extend, smooth=True)
		
		if do_uv:
			# Assign the faces back
			face_index = 0
			for brch in self.branches_all:
				if brch.parent_pt:
					for i in (0,1,2,3):
						face = brch.faces[i] = faces[face_index+i]
					face_index +=4
				
				for pt in brch.bpoints:
					for i in (0,1,2,3):
						if pt.faces[i]:
							pt.faces[i] = faces[face_index]
							face_index +=1
			
			#if self.mesh.faces:
			#	self.mesh.faceUV = True
			mesh.addUVLayer( 'base' )
			
			# rename the uv layer
			#mesh.renameUVLayer(mesh.getUVLayerNames()[0], 'base')
			
			for brch in self.branches_all:
				
				uv_x_scale_branch = 1.0
				if do_uv_uscale:
					uv_x_scale_branch = 0.0
					for pt in brch.bpoints:
						uv_x_scale_branch += pt.radius
					
					uv_x_scale_branch = uv_x_scale_branch / len(brch.bpoints)
					# uv_x_scale_branch = brch.bpoints[0].radius
				
				if do_uv_vnormalize:
					uv_normalize = []
				
				def uvmap_faces(my_faces, y_val, y_size):
					'''
					Accept a branch or pt faces
					'''
					uv_ls = [None, None, None, None]
					for i in (0,1,2,3):
						if my_faces[i]:
							if uv_image:
								my_faces[i].image = uv_image
							uvs = my_faces[i].uv
						else:
							# Use these for calculating blending values
							uvs = [Vector(0,0), Vector(0,0), Vector(0,0), Vector(0,0)]
						
						uv_ls[i] = uvs
						
						x1 = i*0.25 * uv_x_scale * uv_x_scale_branch	
						x2 = (i+1)*0.25 * uv_x_scale * uv_x_scale_branch
						
						uvs[3].x = x1;
						uvs[3].y = y_val+y_size
						
						uvs[0].x = x1
						uvs[0].y = y_val
						
						uvs[1].x = x2
						uvs[1].y = y_val
						
						uvs[2].x = x2
						uvs[2].y = y_val+y_size
						
						if do_uv_vnormalize:
							uv_normalize.extend(uvs)
					
					return uv_ls
					
				# Done uvmap_faces
				
				y_val = 0.0
				
				if brch.parent_pt:
					y_size = (brch.getParentFaceCent() - brch.bpoints[0].co).length
					
					if do_uv_keep_vproportion:
						y_size = y_size / ((brch.bpoints[0].radius + brch.parent_pt.radius)/2) * uv_y_scale
					
					brch.uv = uvmap_faces(brch.faces, 0.0, y_size)
					
					y_val += y_size
				
				for pt in brch.bpoints:
					if pt.next:
						y_size = (pt.co-pt.next.co).length
						# scale the uvs by the radius, avoids stritching.
						if do_uv_keep_vproportion:
							y_size = y_size / pt.radius * uv_y_scale
						pt.uv = uvmap_faces(pt.faces, y_val, y_size)
						y_val += y_size
				
				
				if do_uv_vnormalize and uv_normalize:
					# Use yscale here so you can choose to have half the normalized value say.
					vscale = (1/uv_normalize[-1].y) * uv_y_scale
					for uv in uv_normalize:
						uv.y *= vscale
			
			
			# Done with UV mapping the first layer! now map the blend layers
			if do_uv_blend_layer:
				# Set up the blend UV layer - this is simply the blending for branch joints
				mesh.addUVLayer( 'blend' )
				mesh.activeUVLayer = 'blend'
				
				# Set all faces to be on full blend
				for f in mesh.faces:
					for uv in f.uv:
						uv.y = uv.x = 0.0
				
				for brch in self.branches_all:
					if brch.parent_pt:
						for f in brch.faces:
							if f:
								uvs = f.uv
								uvs[0].x = uvs[1].x = uvs[2].x = uvs[3].x = 0.0 
								uvs[0].y = uvs[1].y = 1.0 # swap these? - same as inverting the blend
								uvs[2].y = uvs[3].y = 0.0
				
				# Set up the join UV layer, this overlays nice blended
				mesh.addUVLayer( 'join' )
				mesh.activeUVLayer = 'join'
				
				# Set all faces to be on full blend
				for f in mesh.faces:
					for uv in f.uv:
						uv.y = uv.x = 0.0
				
				for brch in self.branches_all:
					if brch.parent_pt:
						# The UV's that this branch would cover if it was a face, 
						uvs_base = brch.parent_pt.uv[brch.getParentQuadIndex()]
						
						uvs_base_mid = Vector(0,0)
						for uv in uvs_base:
							uvs_base_mid += uv
							
						uvs_base_mid *= 0.25
						
						# TODO - Factor scale and distance in here 
						## uvs_base_small = [(uv+uvs_base_mid)*0.5 for uv in uvs_base]
						uvs_base_small = [uvs_base_mid, uvs_base_mid, uvs_base_mid, uvs_base_mid]
						
						if brch.faces[0]:
							f = brch.faces[0]
							uvs = f.uv
							uvs[0][:] = uvs_base[0]
							uvs[1][:] = uvs_base[1]
							
							uvs[2][:] = uvs_base_small[1]
							uvs[3][:] = uvs_base_small[0]
						
						if brch.faces[1]:
							f = brch.faces[1]
							uvs = f.uv
							uvs[0][:] = uvs_base[1]
							uvs[1][:] = uvs_base[2]
							
							uvs[2][:] = uvs_base_small[2]
							uvs[3][:] = uvs_base_small[1]
							
						if brch.faces[2]:
							f = brch.faces[2]
							uvs = f.uv
							uvs[0][:] = uvs_base[2]
							uvs[1][:] = uvs_base[3]
							
							uvs[2][:] = uvs_base_small[3]
							uvs[3][:] = uvs_base_small[2]
							
						if brch.faces[3]:
							f = brch.faces[3]
							uvs = f.uv
							uvs[0][:] = uvs_base[3]
							uvs[1][:] = uvs_base[0]
							
							uvs[2][:] = uvs_base_small[0]
							uvs[3][:] = uvs_base_small[3]
			
			mesh.activeUVLayer = 'base'  # just so people dont get worried the texture is not there - dosnt effect rendering.
		else:
			# no UV's
			pass
		
		if do_cap_ends:
			# de-select end points for 
			i = len(faces)-1
			
			cap_end_face_start = len(faces) - len(self.branches_all)
			
			j = 0
			for i in xrange(cap_end_face_start, len(faces)):
				self.branches_all[j].face_cap = faces[i]
				faces[i].sel = 0
				
				# default UV's are ok for now :/
				if do_uv and uv_image:
					faces[i].image = uv_image
				
				j +=1
			
			# set edge crease for capped ends.
			for ed in self.mesh.edges:
				if ed.v1.sel==False and ed.v2.sel==False:
					ed.crease = 255
					ed.sel = True # so its all selected still
			
		del faces_extend
		
		return self.mesh
	
	def toLeafMesh(self, mesh_leaf,\
			leaf_branch_limit = 0.5,\
			leaf_branch_limit_rand = 0.8,\
			leaf_branch_limit_type_curve = False,\
			leaf_branch_limit_type_grow = False,\
			leaf_branch_limit_type_fill = False,\
			leaf_size = 0.5,\
			leaf_size_rand = 0.5,\
			leaf_branch_density = 0.2,\
			leaf_branch_pitch_angle = 0.0,\
			leaf_branch_pitch_rand = 0.2,\
			leaf_branch_roll_rand = 0.2,\
			leaf_branch_angle = 75.0,\
			leaf_rand_seed = 1.0,\
			leaf_object=None,\
		):
		
		'''
		return a mesh with leaves seperate from the tree
		
		Add to the existing mesh.
		'''
		
		#radius = [(pt.radius for pt in self.branches_all for pt in brch.bpoints for pt in brch.bpoints]
		mesh_leaf = freshMesh(mesh_leaf)
		self.mesh_leaf = mesh_leaf
		
		# if not leaf_object: return # make the dupli anyway :/ - they can do it later or the script could complain
		
		if leaf_branch_limit == 1.0:
			max_radius = 1000000.0
		else:
			# We wont place leaves on all branches so...
			# first collect stats, we want to know the average radius and total segments
			totpoints = 0
			radius = 0.0
			max_radius = 0.0
			for brch in self.branches_all:
				for pt in brch.bpoints:
					radius += pt.radius
					if pt.radius > max_radius:
						max_radius = pt.radius
				
				#totpoints += len(brch.bpoints)
				
			radius_max = max_radius * leaf_branch_limit
		
		verts_extend = []
		faces_extend = []
		
		co1 = Vector(0.0, -0.5, -0.5)
		co2 = Vector(0.0, -0.5, 0.5)
		co3 = Vector(0.0, 0.5, 0.5)
		co4 = Vector(0.0, 0.5, -0.5)
		
		rnd_seed = [leaf_rand_seed] # could have seed as an input setting
		
		for brch in self.branches_all:
			
			# quick test, do we need leaves on this branch?
			if leaf_branch_limit != 1.0 and brch.bpoints[-1].radius > radius_max:
				continue
			
			
			for pt in brch.bpoints:
				
				# For each point we can add 2 leaves
				for odd_even in (0,1):
					
					
					if	(pt == brch.bpoints[-1] and odd_even==1) or \
						(leaf_branch_density != 1.0 and leaf_branch_density < next_random_num(rnd_seed)):
						pass
					else:
						if leaf_branch_limit_rand:
							# (-1 : +1) * leaf_branch_limit_rand
							rnd = 1 + (((next_random_num(rnd_seed) - 0.5) * 2 ) * leaf_branch_limit_rand)
						else:
							rnd = 1.0
						
						if pt.childCount == 0 and (leaf_branch_limit == 1.0 or (pt.radius * rnd) < radius_max):
							leaf_size_tmp = leaf_size * (1.0-(next_random_num(rnd_seed)*leaf_size_rand))
							
							# endpoints dont rotate
							if pt.next != None:
								cross1 = CrossVecs(zup, pt.no) # use this to offset the leaf later
								cross2 = CrossVecs(cross1, pt.no)
								if odd_even ==0:
									mat_yaw = RotationMatrix(leaf_branch_angle, 3, 'r',  cross2)
								else:
									mat_yaw = RotationMatrix(-leaf_branch_angle, 3, 'r', cross2)
								
								leaf_no = (pt.no * mat_yaw)
								
								# Correct upwards pointing from changing the yaw 
								#my_up = zup * mat
								
								# correct leaf location for branch width
								cross1.length = pt.radius/2
								leaf_co = pt.co + cross1
							else:
								# no correction needed, we are at the end of the branch
								leaf_no = pt.no
								leaf_co = pt.co
							
							mat = Matrix([leaf_size_tmp,0,0],[0,leaf_size_tmp,0],[0,0,leaf_size_tmp]) * leaf_no.toTrackQuat('x', 'z').toMatrix()
							
							# Randomize pitch and roll for the leaf
							
							# work out the axis to pitch and roll
							cross1 = CrossVecs(zup, leaf_no) # use this to offset the leaf later
							if leaf_branch_pitch_rand or leaf_branch_pitch_angle:
								
								angle = -leaf_branch_pitch_angle
								if leaf_branch_pitch_rand:
									angle += leaf_branch_pitch_rand * ((next_random_num(rnd_seed)-0.5)*360)
								
								mat_pitch = RotationMatrix( angle, 3, 'r', cross1)
								mat = mat * mat_pitch
							if leaf_branch_roll_rand:
								mat_roll =  RotationMatrix( leaf_branch_roll_rand * ((next_random_num(rnd_seed)-0.5)*360), 3, 'r', leaf_no)
								mat = mat * mat_roll
							
							mat = mat.resize4x4() * TranslationMatrix(leaf_co)
							
							i = len(verts_extend)
							faces_extend.append( (i,i+1,i+2,i+3) )
							verts_extend.extend([tuple(co4*mat), tuple(co3*mat), tuple(co2*mat), tuple(co1*mat)])
					#count += 1
				
		
		# setup dupli's
		
		self.mesh_leaf.verts.extend(verts_extend)
		self.mesh_leaf.faces.extend(faces_extend)
		
		return self.mesh_leaf
	
	
	def toArmature(self, ob_arm, armature):
		
		armature.drawType = Blender.Armature.STICK
		armature.makeEditable() # enter editmode
		
		# Assume toMesh has run
		self.armature = armature
		for bonename in armature.bones.keys():
			del armature.bones[bonename]
		
		
		group_names = []
		
		for i, brch in enumerate(self.branches_all):
			
			# get a list of parent points to make into bones. use parents and endpoints
			bpoints_parent = [pt for pt in brch.bpoints if pt.childCount or pt.prev == None or pt.next == None]
			bpbone_last = None
			for j in xrange(len(bpoints_parent)-1):
				
				# bone container class
				bpoints_parent[j].bpbone = bpbone = bpoint_bone()
				bpbone.name = '%i_%i' % (i,j) # must be unique
				group_names.append(bpbone.name)
				
				bpbone.editbone = Blender.Armature.Editbone() # automatically added to the armature
				self.armature.bones[bpbone.name] = bpbone.editbone
				
				bpbone.editbone.head = bpoints_parent[j].co
				bpbone.editbone.head = bpoints_parent[j].co
				bpbone.editbone.tail = bpoints_parent[j+1].co
				
				# parent the chain.
				if bpbone_last:
					bpbone.editbone.parent = bpbone_last.editbone
					bpbone.editbone.options = [Blender.Armature.CONNECTED]
				
				bpbone_last = bpbone
		
		for brch in self.branches_all:
			if brch.parent_pt: # We must have a parent
				
				# find the bone in the parent chain to use for the parent of this
				parent_pt = brch.parent_pt
				bpbone_parent = None
				while parent_pt:
					bpbone_parent = parent_pt.bpbone
					if bpbone_parent:
						break
					
					parent_pt = parent_pt.prev
				
				
				if bpbone_parent:
					brch.bpoints[0].bpbone.editbone.parent = bpbone_parent.editbone
				else: # in rare cases this may not work. should be verry rare but check anyway.
					print 'this is really odd... look into the bug.'
		
		self.armature.update() # exit editmode
		
		# Skin the mesh
		if self.mesh:
			for group in group_names:
				self.mesh.addVertGroup(group)
		
		for brch in self.branches_all:
			vertList = []
			group = '' # dummy
			
			for pt in brch.bpoints:
				if pt.bpbone:
					if vertList:
						self.mesh.assignVertsToGroup(group, vertList, 1.0, Blender.Mesh.AssignModes.ADD)
					
					vertList = []
					group = pt.bpbone.name
				
				vertList.extend( [v.index for v in pt.verts] )
			
			if vertList:
				self.mesh.assignVertsToGroup(group, vertList, 1.0, Blender.Mesh.AssignModes.ADD)
		
		return self.armature
	
	def toAction(self, ob_arm, texture, anim_speed=1.0, anim_magnitude=1.0, anim_speed_size_scale=True, anim_offset_scale=1.0):
		# Assume armature
		action = ob_arm.action
		if not action:
			action = bpy.data.actions.new()
			action.fakeUser = False # so we dont get masses of bad data
			ob_arm.action = action
		
		# Blender.Armature.NLA.ob_arm.
		pose = ob_arm.getPose()
		
		for pose_bone in pose.bones.values():
			pose_bone.insertKey(ob_arm, 0, [Blender.Object.Pose.ROT], True)
		
		# Now get all the IPO's
		
		ipo_dict = action.getAllChannelIpos()
		# print ipo_dict
		
		# Sicne its per frame, it increases very fast. scale it down a bit
		anim_speed = anim_speed/100
		
		# When we have the same trees next to eachother, they will animate the same way unless we give each its own texture or offset settings.
		# We can use the object's location as a factor - this also will have the advantage? of seeing the animation move across the tree's
		# allow a scale so the difference between tree textures can be adjusted.
		anim_offset = self.objectCurve.matrixWorld.translationPart() * anim_offset_scale
		
		anim_speed_final = anim_speed
		# Assign drivers to them all
		for name, ipo in ipo_dict.iteritems():
			tex_str = 'b.Texture.Get("%s")' % texture.name
			
			if anim_speed_size_scale:
				# Adjust the speed by the bone size.
				# get the point from the name. a bit ugly but works fine ;) - Just dont mess the index up!
				lookup = [int(val) for val in name.split('_')]
				pt = self.branches_all[ lookup[0] ].bpoints[ lookup[1] ]
				anim_speed_final = anim_speed / (1+pt.radius)
			
			cu = ipo[Blender.Ipo.PO_QUATX]
			try:	cu.delBezier(0) 
			except:	pass
			cu.driver = 2 # Python expression
			cu.driverExpression = '%.3f*(%s.evaluate(((b.Get("curframe")*%.3f)+%.3f,%.3f,%.3f)).w-0.5)' % (anim_magnitude, tex_str, anim_speed_final, anim_offset.x, anim_offset.y, anim_offset.z)
			
			cu = ipo[Blender.Ipo.PO_QUATY]
			try:	cu.delBezier(0) 
			except:	pass
			cu.driver = 2 # Python expression
			cu.driverExpression = '%.3f*(%s.evaluate((%.3f,(b.Get("curframe")*%.3f)+%.3f,%.3f)).w-0.5)' % (anim_magnitude, tex_str,  anim_offset.x, anim_speed_final, anim_offset.y, anim_offset.z)
			
			cu = ipo[Blender.Ipo.PO_QUATZ]
			try:	cu.delBezier(0) 
			except:	pass
			cu.driver = 2 # Python expression
			cu.driverExpression = '%.3f*(%s.evaluate((%.3f,%.3f,(b.Get("curframe")*%.3f)+%.3f)).w-0.5)' % (anim_magnitude, tex_str,  anim_offset.x, anim_offset.y, anim_speed_final, anim_offset.z)
		
xyzup = Vector(1,1,1).normalize()
xup = Vector(1,0,0)
yup = Vector(0,1,0)
zup = Vector(0,0,1)

class bpoint_bone:
	def __init__(self):
		self.name = None
		self.editbone = None
		self.blenbone = None
		self.posebone = None

class bpoint(object):
	''' The point in the middle of the branch, not the mesh points
	'''
	__slots__ = 'branch', 'co', 'no', 'radius', 'vecs', 'verts', 'children', 'faces', 'uv', 'next', 'prev', 'childCount', 'bpbone', 'roll_angle', 'nextMidCo', 'childrenMidCo', 'childrenMidRadius', 'targetCos', 'inTwigBounds'
	def __init__(self, brch, co, no, radius):
		self.branch = brch
		self.co = co
		self.no = no
		self.radius = radius
		self.vecs =		[None, None, None, None] # 4 for now
		self.verts =	[None, None, None, None]
		self.children = [None, None, None, None] # child branches, dont fill in faces here
		self.faces = [None, None, None, None]
		self.uv = None # matching faces, except - UV's are calculated even if there is no face, this is so we can calculate the blending UV's
		self.next = None
		self.prev = None
		self.childCount = 0
		self.bpbone = None # bpoint_bone instance
		
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
		
		# When we use twig bounding mesh, store if this point is in the bounding mesh. Assume true unless we set to false and do the test
		self.inTwigBounds = True 
	
	def __repr__(self):
		s = ''
		s += '\t\tco:', self.co
		s += '\t\tno:', self.no
		s += '\t\tradius:', self.radius
		s += '\t\tchildren:', [(child != False) for child in self.children]
		return s
		
	def makeLast(self):
		self.next = None
		self.nextMidCo = None
		self.childrenMidCo = None
	
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
			co_all = self.targetCos[0]
		else:
			co_all = Vector()
			for co in self.targetCos:
				co_all += co
			co_all = co_all / len(self.targetCos)
		
		self.targetCos[:] = []
		
		length = (self.co-co_all).length
		# work out if we are moving up or down
		if AngleBetweenVecsSafe(self.no, self.co - co_all) < 90:
			
			# Up
			while length > (self.co-self.prev.co).length:
				if not self.collapseUp():
					break
			
		else:
			# Down
			while length*2 > (self.co-self.next.co).length:
				if not self.collapseDown():
					break
				
		self.setCo(co_all)
		
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
		
	
	def collapseDown(self):
		'''
		Collapse the next point into this one
		'''
		
		# self.next.next == None check is so we dont shorten the final length of branches.
		if self.next == None or self.next.next == None or self.childCount or self.next.childCount:
			return False
		
		self.branch.bpoints.remove(self.next)
		self.next = self.next.next # skip 
		self.next.prev = self
		
		# Watch this place - must update all data thats needed. roll is not calculaetd yet.
		self.calcNextMidCo()
		return True
		
	def collapseUp(self):
		'''
		Collapse the previous point into this one
		'''
		
		# self.next.next == None check is so we dont shorten the final length of branches.
		if self.prev == None or self.prev.prev == None or self.prev.childCount or self.prev.prev.childCount:
			return False
		
		self.branch.bpoints.remove(self.prev)
		self.prev = self.prev.prev # skip 
		self.prev.next = self
		
		# Watch this place - must update all data thats needed. roll is not calculaetd yet.
		self.prev.calcNextMidCo()
		return True
		
	
	def smooth(self, factor, factor_joint):
		'''
		Blend this point into the other 2 points
		'''
		if self.next == None or self.prev == None:
			return False
		
		if self.childCount or self.prev.childCount:
			factor = factor_joint;
		
		if factor==0.0:
			return False;
		
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
	
	def childPointUnused(self, index):
		'''
		Same as above but return None when the point is alredy used.
		'''
		if self.children[index]:
			return None
		return self.childPoint(index)
		
	
	def roll(self, angle):
		'''
		Roll the quad about its normal 
		use for aurienting the sides of a quad to meet a branch that stems from here...
		'''
		# debugVec(self.co, self.co + self.no)
		
		mat = RotationMatrix(angle, 3, 'r', self.no)
		for i in xrange(4):
			self.vecs[i] = self.vecs[i] * mat
	
	
	def toMesh(self, mesh):
		self.verts[0].co = self.getAbsVec(0)
		self.verts[1].co = self.getAbsVec(1)
		self.verts[2].co = self.getAbsVec(2)
		self.verts[3].co = self.getAbsVec(3)
		
		if not self.next:
			return
		
		if self.prev == None and self.branch.parent_pt:
			# join from parent branch
			
			# which side are we of the parents quad
			index = self.branch.parent_pt.children.index(self.branch)
			
			# collect the points we are to merge into between the parent its next point
			if index==0:	verts = [self.branch.parent_pt.verts[0], self.branch.parent_pt.verts[1], self.branch.parent_pt.next.verts[1], self.branch.parent_pt.next.verts[0]]
			if index==1:	verts = [self.branch.parent_pt.verts[1], self.branch.parent_pt.verts[2], self.branch.parent_pt.next.verts[2], self.branch.parent_pt.next.verts[1]]
			if index==2:	verts = [self.branch.parent_pt.verts[2], self.branch.parent_pt.verts[3], self.branch.parent_pt.next.verts[3], self.branch.parent_pt.next.verts[2]]
			if index==3:	verts = [self.branch.parent_pt.verts[3], self.branch.parent_pt.verts[0], self.branch.parent_pt.next.verts[0], self.branch.parent_pt.next.verts[3]]
				
				
			# Watchout for overlapping faces!
			self.branch.faces[:] =\
				[verts[0], verts[1], self.verts[1], self.verts[0]],\
				[verts[1], verts[2], self.verts[2], self.verts[1]],\
				[verts[2], verts[3], self.verts[3], self.verts[2]],\
				[verts[3], verts[0], self.verts[0], self.verts[3]]
		
		# normal join, parents or no parents
		if not self.children[0]:	self.faces[0] = [self.verts[0], self.verts[1], self.next.verts[1], self.next.verts[0]]
		if not self.children[1]:	self.faces[1] = [self.verts[1], self.verts[2], self.next.verts[2], self.next.verts[1]]
		if not self.children[2]:	self.faces[2] = [self.verts[2], self.verts[3], self.next.verts[3], self.next.verts[2]]
		if not self.children[3]:	self.faces[3] = [self.verts[3], self.verts[0], self.next.verts[0], self.next.verts[3]]
	
	def calcVerts(self):
		if self.prev == None:
			if self.branch.parent_pt:
				cross = CrossVecs(self.no, self.branch.parent_pt.no) * RotationMatrix(-45, 3, 'r', self.no)
			else:
				# parentless branch - for best results get a cross thats not the same as the normal, in rare cases this happens.
				
				# Was just doing 
				#  cross = zup
				# which works most of the time, but no verticle lines
				
				if AngleBetweenVecsSafe(self.no, zup) > 1.0:	cross = zup
				elif AngleBetweenVecsSafe(self.no, yup) > 1.0:	cross = yup
				else:											cross = xup
				
		else:
			cross = CrossVecs(self.prev.vecs[0], self.no)
		
		self.vecs[0] = Blender.Mathutils.CrossVecs(self.no, cross)
		self.vecs[0].length = abs(self.radius)
		mat = RotationMatrix(90, 3, 'r', self.no)
		self.vecs[1] = self.vecs[0] * mat
		self.vecs[2] = self.vecs[1] * mat
		self.vecs[3] = self.vecs[2] * mat
	
	def hasChildren(self):
		'''
		Use .childCount where possible, this does the real check
		'''
		if self.children.count(None) == 4:
			return False
		else:
			return True
	
class branch:
	def __init__(self):
		self.bpoints = []
		self.parent_pt = None
		self.tag = False # have we calculated our points
		self.face_cap = None
		self.length = -1
		# self.totchildren = 0
		# Bones per branch
		self.faces = [None, None, None, None]
		self.uv = None # face uvs can be fake, always 4
		self.bones = []
		self.generation = 0 # use to limit twig reproduction
		self.twig_count = 0 # count the number of twigs - so as to limit how many twigs a branch gets
		# self.myindex = -1
		### self.segment_spacing_scale = 1.0 # use this to scale up the spacing - so small twigs dont get WAY too many polys
		self.type = None
	
	def __repr__(self):
		s = ''
		s += '\tbranch'
		s += '\tbpoints:', len(self.bpoints)
		for pt in brch.bpoints:
			s += str(self.pt)
	
	def getNormal(self):
		return (self.bpoints[-1].co - self.bpoints[0].co).normalize()
	
	def getParentAngle(self):
		if self.parent_pt:
			return AngleBetweenVecsSafe(self.parent_pt.no, self.bpoints[0].no )
		else:
			return 45.0
	
	def getParentRadiusRatio(self):
		if self.parent_pt:
			return self.bpoints[0].radius / self.parent_pt.radius
		else:
			return 0.8
	
	def getLength(self):
		return (self.bpoints[0].co - self.bpoints[-1].co).length
	
	def getStraightness(self):
		straight = 0.0
		pt = self.bpoints[0]
		while pt.next:
			straight += AngleBetweenVecsSafe(pt.no, pt.next.no)
			pt = pt.next
		return straight
		
	
	'''
	def calcTotChildren(self):
		for pt in self.bpoints:
			self.totchildren += pt.childCount
	'''
	def calcData(self):
		'''
		Finalize once point data is there
		'''
		self.calcPointLinkedList()
		self.calcPointExtras()
	
	def calcPointLinkedList(self):
		for i in xrange(1, len(self.bpoints)-1):
			self.bpoints[i].next = self.bpoints[i+1]
			self.bpoints[i].prev = self.bpoints[i-1]
		
		self.bpoints[0].next = self.bpoints[1]	
		self.bpoints[-1].prev = self.bpoints[-2]
		
	def calcPointExtras(self):
		'''
		Run on a new branch or after transforming an existing one.
		'''
		for pt in self.bpoints:
			pt.calcNormal()
			pt.calcNextMidCo()
	
	def calcTwigBounds(self, tree):
		'''
		Check if out points are 
		'''
		for pt in self.bpoints:
			pt.inTwigBounds = tree.isPointInTwigBounds(pt.co)
			#if pt.inTwigBounds:
			#	debug_pt(pt.co)
	
	def baseTrim(self, connect_base_trim):
		# if 1)	dont remove the whole branch, maybe an option but later
		# if 2)	we are alredy a parent, cant remove me now.... darn :/ not nice...
		#		could do this properly but it would be slower and its a corner case.
		#
		# if 3)	this point is within the branch, remove it.
		#		Scale this value by the difference in radius, a low trim looks better when the parent is a lot bigger..
		# 
		
		while	len(self.bpoints)>2 and\
				self.bpoints[0].childCount == 0 and\
				(self.parent_pt.nextMidCo - self.bpoints[0].co).length < ((self.parent_pt.radius + self.parent_pt.next.radius)/4) + (self.bpoints[0].radius * connect_base_trim):
			# Note /4 - is a bit odd, since /2 is correct, but /4 lets us have more tight joints by default
			
			
			del self.bpoints[0]
			self.bpoints[0].prev = None
	
	def boundsTrim(self):
		'''
		depends on calcTwigBounds running first. - also assumes no children assigned yet! make sure this is always the case.
		'''
		trim = False
		for i, pt in enumerate(self.bpoints):
			if not pt.inTwigBounds:
				trim = True
				break
		
		# We must have at least 2 points to be a valid branch. this will be a stump :/
		if not trim or i < 3:
			self.bpoints = [] # 
			return
		
		# Shorten the point list
		self.bpoints = self.bpoints[:i]
		self.bpoints[-1].makeLast()
	
	def taper(self, twig_ob_bounds_prune_taper = 0.0):
		l = float(len( self.bpoints ))
		for i, pt in enumerate(self.bpoints):
			pt.radius *= (((l-i)/l) + (twig_ob_bounds_prune_taper*(i/l)) )
	
	def getParentBranch(self):
		if not self.parent_pt:
			return None
		return self.parent_pt.branch
		
	def getParentQuadAngle(self):
		'''
		The angle off we are from our parent quad,
		'''
		# used to roll the parent so its faces us better
		
		# Warning this can be zero sometimes, see the try below for the error
		parent_normal = self.getParentFaceCent() - self.parent_pt.nextMidCo
		
		
		self_normal = self.bpoints[1].co - self.parent_pt.co
		# We only want the angle in relation to the parent points normal
		# modify self_normal to make this so
		cross = CrossVecs(self_normal, self.parent_pt.no)
		self_normal = CrossVecs(self.parent_pt.no, cross) # CHECK
		
		#try:	angle = AngleBetweenVecs(parent_normal, self_normal)
		#except:	return 0.0
		angle = AngleBetweenVecsSafe(parent_normal, self_normal)
		
		
		# see if we need to rotate positive or negative
		# USE DOT PRODUCT!
		cross = CrossVecs(parent_normal, self_normal)
		if AngleBetweenVecsSafe(cross, self.parent_pt.no) > 90:
			angle = -angle
		
		return angle
	
	def getParentQuadIndex(self):
		return self.parent_pt.children.index(self)
	def getParentFaceCent(self):
		return self.parent_pt.childPoint(  self.getParentQuadIndex()  )
	
	def findClosest(self, co):
		'''
		Find the closest point that can bare a child
		'''
		
		
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
			if pt.nextMidCo and pt.childCount < 4:
				dist = (pt.nextMidCo-co).length
				if dist < best_dist:
					best = pt
					best_dist = dist
		
		return best, best_dist
	
	def inParentChain(self, brch):
		'''
		See if this branch is a parent of self or in the chain
		'''
		
		self_parent_lookup = self.getParentBranch()
		while self_parent_lookup:
			if self_parent_lookup == brch:
				return True
			self_parent_lookup = self_parent_lookup.getParentBranch()
		
		return False
	
	def transform(self, mat, loc=None, scale=None):
		if scale==None:
			scale = (xyzup * mat).length
		
		for pt in self.bpoints:
			if loc:
				pt.co = (pt.co * mat) + loc
			else:
				pt.co = pt.co * mat
			pt.radius *= scale
		
		for pt in self.bpoints:
			self.calcPointExtras()
	
	def translate(self, co):
		'''
		Simply move the twig on the branch
		'''
		ofs = self.bpoints[0].co-co
		for pt in self.bpoints:
			pt.co -= ofs
	
	def transformRecursive(self, tree, mat3x3, cent, scale=None):
		
		if scale==None:
			# incase this is a translation matrix
			scale = ((xyzup * mat3x3) - (Vector(0,0,0) * mat3x3)).length
		
		for pt in self.bpoints:		pt.co = ((pt.co-cent) * mat3x3) + cent
		#for pt in self.bpoints:		pt.co = (pt.co * mat3x3)
		for pt in self.bpoints:		self.calcPointExtras()
			
			
		for brch in tree.branches_all:
			if brch.parent_pt:
				if brch.parent_pt.branch == self:
					
					brch.transformRecursive(tree, mat3x3, cent, scale)
			
		'''
		for pt in self.bpoints:
			for brch in pt.children:
				if brch:
					brch.transformRecursive(mat3x3, cent, scale)
		'''
	def bestTwigSegment(self):
		'''
		Return the most free part on the branch to place a new twig
		return (sort_value, best_index, self)
		'''
		
		# loop up and down the branch - counding how far from the last parent segment we are
		spacing1 = [0] * (len(self.bpoints)-1)
		spacing2 = spacing1[:]
		
		step_from_parent = 0
		for i in xrange(len(spacing1)): # -1 because the last pt cant have kits
			
			if self.bpoints[i].childCount or self.bpoints[i].inTwigBounds==False:
				step_from_parent = 0
			else:
				step_from_parent += 1
			
			spacing1[i] += step_from_parent # -1 because the last pt cant have kits
		
		best_index = -1
		best_val = -1
		step_from_parent = 0
		for i in xrange(len(spacing1)-1, -1, -1):
			
			if self.bpoints[i].childCount or self.bpoints[i].inTwigBounds==False:
				step_from_parent = 0
			else:
				step_from_parent += 1
			
			spacing2[i] += step_from_parent
			
			# inTwigBounds is true by default, when twigBounds are used it can be false
			if self.bpoints[i].childCount < 4 and self.bpoints[i].inTwigBounds:
				# Dont allow to assign more verts then 4
				val = spacing1[i] * spacing2[i]
				if val > best_val:
					best_val = val
					best_index = i
		
		#if best_index == -1:
		#	raise "Error"
			
		# This value is only used for sorting, so the lower the value - the sooner it gets a twig.
		#sort_val = -best_val + (1/self.getLength())
		sort_val=self.getLength()
		
		return sort_val, best_index, self
	
	def evenPointDistrobution(self, factor=1.0, factor_joint=1.0):
		'''
		Redistribute points that are not evenly distributed
		factor is between 0.0 and 1.0
		'''
		
		for pt in self.bpoints:
			if pt.next and pt.prev and pt.childCount == 0 and pt.prev.childCount == 0:
				w1 = pt.nextLength()
				w2 = pt.prevLength()
				wtot = w1+w2
				if wtot > 0.0:
					w1=w1/wtot
					#w2=w2/wtot
					w1 = abs(w1-0.5)*2 # make this from 0.0 to 1.0, where 0 is the middle and 1.0 is as far out of the middle as possible.
					# print "%.6f" % w1
					pt.smooth(w1*factor, w1*factor_joint)
	
	def fixOverlapError(self, joint_smooth=1.0):
		# Keep fixing until no hasOverlapError left to fix.
		
		error = True
		while error:
			error = False
			for pt in self.bpoints:
				if pt.prev and pt.next:
					if pt.hasOverlapError():
						if pt.smooth(1.0, joint_smooth): # if we cant fix then dont bother trying again.
							error = True
	
	def evenJointDistrobution(self, joint_compression = 1.0):
		# See if we need to evaluate this branch at all 
		if len(self.bpoints) <= 2: # Rare but in this case we cant do anything
			return
		has_children = False
		for pt in self.bpoints:
			if pt.childCount:
				has_children = True
				break
		
		if not has_children:
			return
		
		# OK, we have children, so we have some work to do...
		# center each segment
		
		# work out the median location of all points children.
		for pt in self.bpoints:
			pt.calcChildrenMidData()
			pt.targetCos[:] = []
		
		for pt in self.bpoints:
			
			if pt.childrenMidCo:
				# Move this and the next segment to be around the child point.
				# TODO - factor in the branch angle, be careful with this - close angles can have extreme values.
				slide_dist = (pt.childrenMidCo - pt.co).length - (pt.childrenMidRadius * joint_compression)
				co = pt.slideCo( slide_dist )
				if co:
					pt.targetCos.append( co )
				
				slide_dist = (pt.childrenMidRadius * joint_compression) - (pt.childrenMidCo - pt.next.co).length
				co = pt.next.slideCo( slide_dist )
				if co:
					pt.next.targetCos.append( co )
		
		for pt in reversed(self.bpoints):
			pt.applyTargetLocation()
	
	def collapsePoints(self, seg_density=0.5, seg_density_angle=20.0, seg_density_radius=0.3, smooth_joint=1.0):
		
		collapse = True
		while collapse:
			collapse = False
			pt = self.bpoints[0]
			while pt:
				# Collapse angles greater then 90. they are useually artifacts
				
				if pt.prev and pt.next and pt.prev.childCount == 0:
					if (pt.radius + pt.prev.radius) != 0.0 and abs(pt.radius - pt.prev.radius) / (pt.radius + pt.prev.radius) < seg_density_radius:
						ang = AngleBetweenVecsSafe(pt.no, pt.prev.no)
						if seg_density_angle == 180 or ang > 90 or ang < seg_density_angle:
							## if (pt.prev.nextMidCo-pt.co).length < ((pt.radius + pt.prev.radius)/2) * seg_density:
							if (pt.prev.nextMidCo-pt.co).length < seg_density or ang > 90:
								pt_save = pt.prev
								if pt.next.collapseUp(): # collapse this point
									collapse = True
									pt = pt_save # so we never reference a removed point
				
				if pt.childCount == 0 and pt.next: #if pt.childrenMidCo == None:
					if (pt.radius + pt.next.radius) != 0.0 and abs(pt.radius - pt.next.radius) / (pt.radius + pt.next.radius) < seg_density_radius:
						ang = AngleBetweenVecsSafe(pt.no, pt.next.no)
						if seg_density_angle == 180 or ang > 90 or ang < seg_density_angle:
							# do here because we only want to run this on points with no children,
							# Are we closer theto eachother then the radius?
							## if (pt.nextMidCo-pt.co).length < ((pt.radius + pt.next.radius)/2) * seg_density:
							if (pt.nextMidCo-pt.co).length < seg_density or ang > 90:
								if pt.collapseDown():
									collapse = True
				
				pt = pt.next
		## self.checkPointList()
		self.evenPointDistrobution(1.0, smooth_joint)
		
		for pt in self.bpoints:
			pt.calcNormal()
			pt.calcNextMidCo()
	
	# This is a bit dodgy - moving the branches around after placing can cause problems
	"""
	def branchReJoin(self):
		'''
		Not needed but nice to run after collapsing incase segments moved a lot
		'''
		if not self.parent_pt:
			return # nothing to do
		
		# see if the next segment is closer now (after collapsing)
		parent_pt = self.parent_pt
		root_pt = self.bpoints[0]
		
		#try:
		index = parent_pt.children.index(self)
		#except:
		#print "This is bad!, but not being able to re-join isnt that big a deal"
		
		current_dist = (parent_pt.nextMidCo - root_pt.co).length
		
		# TODO - Check size of new area is ok to move into
		
		if parent_pt.next and parent_pt.next.next and parent_pt.next.children[index] == None:
			# We can go here if we want, see if its better
			if current_dist > (parent_pt.next.nextMidCo - root_pt.co).length:
				self.parent_pt.children[index] = None
				self.parent_pt.childCount -= 1
				
				self.parent_pt = parent_pt.next
				self.parent_pt.children[index] = self
				self.parent_pt.childCount += 1
				return
		
		if parent_pt.prev and parent_pt.prev.children[index] == None:
			# We can go here if we want, see if its better
			if current_dist > (parent_pt.prev.nextMidCo - root_pt.co).length:
				self.parent_pt.children[index] = None
				self.parent_pt.childCount -= 1
				
				self.parent_pt = parent_pt.prev
				self.parent_pt.children[index] = self
				self.parent_pt.childCount += 1
				return
	"""
	
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
	
	def mixToNew(self, other, BLEND_MODE):
		'''
		Generate a new branch based on 2 existing ones
		These branches will point 'zup' - aurient 'xup' and have a tip length of 1.0
		'''
		
		# Lets be lazy! - if the branches are different sizes- use the shortest.
		# brch1 is always smaller
		
		brch1 = self
		brch2 = other
		if len(brch1.bpoints) > len(brch2.bpoints):
			brch1, brch2 = brch2, brch1
		
		if len(brch1.bpoints) == 1:
			return None
		
		co_start = brch1.bpoints[0].co
		cos1 = [ pt.co - co_start for pt in brch1.bpoints ]
		
		co_start = brch2.bpoints[0].co
		if len(brch1.bpoints) == len(brch2.bpoints):
			cos2 = [ pt.co - co_start for pt in brch2.bpoints ]
		else: # truncate the points
			cos2 = [ brch2.bpoints[i].co - co_start for i in xrange(len(brch1.bpoints)) ]
			
		scales = []
		for cos_ls in (cos1, cos2):
			cross = CrossVecs(cos_ls[-1], zup)
			mat = RotationMatrix(AngleBetweenVecsSafe(cos_ls[-1], zup), 3, 'r', cross)
			cos_ls[:] = [co*mat for co in cos_ls]
		
			# point z-up
			
			# Now they are both pointing the same way aurient the curves to be rotated the same way
			xy_nor = Vector(0,0,0)
			for co in cos_ls:
				xy_nor.x += co.x
				xy_nor.y += co.y
			cross = CrossVecs(xy_nor, xup)
			
			# Also scale them here so they are 1.0 tall always
			scale = 1.0/(cos_ls[0]-cos_ls[-1]).length
			mat = RotationMatrix(AngleBetweenVecsSafe(xy_nor, xup), 3, 'r', cross) * Matrix([scale,0,0],[0,scale,0],[0,0,scale])
			cos_ls[:] = [co*mat for co in cos_ls]
			
			scales.append(scale)
		
		# Make the new branch
		new_brch = branch()
		new_brch.type = BRANCH_TYPE_GROWN
		for i in xrange(len(cos1)):
			new_brch.bpoints.append( bpoint(new_brch, (cos1[i]+cos2[i])*0.5, Vector(), (brch1.bpoints[i].radius*scales[0] + brch2.bpoints[i].radius*scales[1])/2) )
		
		new_brch.calcData()
		return new_brch
	
	'''
	def toMesh(self):
		pass
	'''




# No GUI code above this! ------------------------------------------------------

# PREFS - These can be saved on the object's id property. use 'tree2curve' slot
from Blender import Draw
import BPyWindow
ID_SLOT_NAME = 'Curve2Tree'

EVENT_NONE = 0
EVENT_EXIT = 1
EVENT_UPDATE = 2
EVENT_UPDATE_AND_UI = 2
EVENT_REDRAW = 3


# Prefs for each tree
PREFS = {}
PREFS['connect_sloppy'] = Draw.Create(1.5)
PREFS['connect_base_trim'] = Draw.Create(1.0)
PREFS['seg_density'] = Draw.Create(0.5)
PREFS['seg_density_angle'] = Draw.Create(20.0)
PREFS['seg_density_radius'] = Draw.Create(0.3)
PREFS['seg_joint_compression'] = Draw.Create(1.0)
PREFS['seg_joint_smooth'] = Draw.Create(2.0)
PREFS['image_main'] = Draw.Create('')
PREFS['do_uv'] = Draw.Create(0)
PREFS['uv_x_scale'] = Draw.Create(4.0)
PREFS['uv_y_scale'] = Draw.Create(1.0)
PREFS['do_material'] = Draw.Create(0)
PREFS['material_use_existing'] = Draw.Create(1)
PREFS['material_texture'] = Draw.Create(1)
PREFS['material_stencil'] = Draw.Create(1)
PREFS['do_subsurf'] = Draw.Create(1)
PREFS['do_cap_ends'] = Draw.Create(0)
PREFS['do_uv_keep_vproportion'] = Draw.Create(1)
PREFS['do_uv_vnormalize'] = Draw.Create(0)
PREFS['do_uv_uscale'] = Draw.Create(0)
PREFS['do_armature'] = Draw.Create(0)
PREFS['do_anim'] = Draw.Create(1)
try:		PREFS['anim_tex'] = Draw.Create([tex for tex in bpy.data.textures][0].name)
except:		PREFS['anim_tex'] = Draw.Create('')

PREFS['anim_speed'] = Draw.Create(0.2)
PREFS['anim_magnitude'] = Draw.Create(0.2)
PREFS['anim_speed_size_scale'] = Draw.Create(1)
PREFS['anim_offset_scale'] = Draw.Create(1.0)

PREFS['do_twigs_fill'] = Draw.Create(0)
PREFS['twig_fill_levels'] = Draw.Create(4)

PREFS['twig_fill_rand_scale'] = Draw.Create(0.1)
PREFS['twig_fill_fork_angle_max'] = Draw.Create(180.0)
PREFS['twig_fill_radius_min'] = Draw.Create(0.001)
PREFS['twig_fill_radius_factor'] = Draw.Create(0.75)
PREFS['twig_fill_shape_type'] = Draw.Create(1)
PREFS['twig_fill_shape_rand'] = Draw.Create(0.5)
PREFS['twig_fill_shape_power'] = Draw.Create(0.5)

PREFS['do_twigs'] = Draw.Create(0)
PREFS['twig_ratio'] = Draw.Create(2.0)
PREFS['twig_select_mode'] = Draw.Create(0)
PREFS['twig_select_factor'] = Draw.Create(0.5)
PREFS['twig_scale'] = Draw.Create(0.8)
PREFS['twig_scale_width'] = Draw.Create(1.0)
PREFS['twig_random_orientation'] = Draw.Create(180)
PREFS['twig_random_angle'] = Draw.Create(33)
PREFS['twig_recursive'] = Draw.Create(1)
PREFS['twig_recursive_limit'] = Draw.Create(3)
PREFS['twig_ob_bounds'] = Draw.Create('') # WATCH out, used for do_twigs_fill AND do_twigs
PREFS['twig_ob_bounds_prune'] = Draw.Create(1)
PREFS['twig_ob_bounds_prune_taper'] = Draw.Create(1.0)
PREFS['twig_placement_maxradius'] = Draw.Create(10.0)
PREFS['twig_placement_maxtwig'] = Draw.Create(4)
PREFS['twig_follow_parent'] = Draw.Create(0.0)
PREFS['twig_follow_x'] = Draw.Create(0.0)
PREFS['twig_follow_y'] = Draw.Create(0.0)
PREFS['twig_follow_z'] = Draw.Create(0.0)

PREFS['do_leaf'] = Draw.Create(0)

PREFS['leaf_branch_limit'] = Draw.Create(0.25)
PREFS['leaf_branch_limit_rand'] = Draw.Create(0.1)
PREFS['leaf_branch_density'] = Draw.Create(0.1)
PREFS['leaf_branch_pitch_angle'] = Draw.Create(0.0)
PREFS['leaf_branch_pitch_rand'] = Draw.Create(0.2)
PREFS['leaf_branch_roll_rand'] = Draw.Create(0.2)
PREFS['leaf_branch_angle'] = Draw.Create(75.0)
PREFS['leaf_rand_seed'] = Draw.Create(1.0)
PREFS['leaf_size'] = Draw.Create(0.5)
PREFS['leaf_size_rand'] = Draw.Create(0.5)

PREFS['leaf_object'] = Draw.Create('')

PREFS['do_variation'] = Draw.Create(0)
PREFS['variation_seed'] = Draw.Create(1)
PREFS['variation_orientation'] = Draw.Create(0.0)
PREFS['variation_scale'] = Draw.Create(0.0)

GLOBAL_PREFS = {}
GLOBAL_PREFS['realtime_update'] = Draw.Create(0)


def getContextCurveObjects():
	sce = bpy.data.scenes.active
	objects = []
	ob_act = sce.objects.active
	for ob in sce.objects.context:
		if ob == ob_act: ob_act = None
		
		if ob.type != 'Curve':
			ob = ob.parent
		if not ob or ob.type != 'Curve':
			continue
		objects.append(ob)
		
		# Alredy delt with 
		
	
	# Add the active, important when using localview or local layers
	if ob_act:
		ob = ob_act
		if ob.type != 'Curve':
			ob = ob.parent
		if not ob or ob.type != 'Curve':
			pass
		else:
			objects.append(ob)
	
	return objects


def Prefs2Dict(prefs, new_prefs):
	'''
	Make a copy with no button settings
	'''
	new_prefs.clear()
	for key, val in prefs.items():
		try:	new_prefs[key] = val.val
		except:	new_prefs[key] = val
	return new_prefs

def Dict2Prefs(prefs, new_prefs):
	'''
	Make a copy with button settings
	'''
	for key in prefs: # items would be nice for id groups
		val = prefs[key]
		ok = True
		
		try:
			# If we have this setting allredy but its a different type, use the old setting (converting int's to floats for instance)
			new_val = new_prefs[key] # this may fail, thats ok
			if (type(new_val)==Blender.Types.ButtonType) and (type(new_val.val) != type(val)):
				ok = False
		except:
			pass
		
		if ok:
			try:
				new_prefs[key] = Blender.Draw.Create( val )
			except:
				new_prefs[key] = val
		
	return new_prefs

def Prefs2IDProp(idprop, prefs):
	new_prefs = {}
	Prefs2Dict(prefs, new_prefs)
	try:	del idprop[ID_SLOT_NAME]
	except:	pass
	
	idprop[ID_SLOT_NAME] = new_prefs
	
def IDProp2Prefs(idprop, prefs):
	try:
		prefs = idprop[ID_SLOT_NAME]
	except:
		return False
	Dict2Prefs(prefs, PREFS)
	return True

def buildTree(ob_curve, single=False):
	'''
	Must be a curve object, write to a child mesh
	Must check this is a curve object!
	'''
	print 'Curve2Tree, starting...'
	# if were only doing 1 object, just use the current prefs
	prefs = {}
	
	if single or not (IDProp2Prefs(ob_curve.properties, prefs)):
		prefs = PREFS
		
	
	# Check prefs are ok.
	
	
	sce = bpy.data.scenes.active
	
	def getObChild(parent, obtype):
		try:
			return [ _ob for _ob in sce.objects if _ob.type == obtype if _ob.parent == parent ][0]
		except:
			return None
	
	def newObChild(parent, obdata):
		
		ob_new = bpy.data.scenes.active.objects.new(obdata)
		# ob_new.Layers = parent.Layers
		
		# new object settings
		parent.makeParent([ob_new])
		ob_new.setMatrix(Matrix())
		ob_new.sel = 0
		return ob_new
	
	def hasModifier(modtype):
		return len([mod for mod in ob_mesh.modifiers if mod.type == modtype]) > 0
			
	
	sce = bpy.data.scenes.active
	
	if PREFS['image_main'].val:
		try:		image = bpy.data.images[PREFS['image_main'].val]
		except:		image = None
	else:			image = None
	
	# Get the mesh child
	
	print '\treading blenders curves...',
	time1 = Blender.sys.time()
	
	t = tree()
	t.fromCurve(ob_curve)
	if not t.branches_all:
		return # Empty curve? - may as well not throw an error
	
	time2 = Blender.sys.time() # time print
	"""
	print '%.4f sec' % (time2-time1)
	if PREFS['do_twigs'].val:
		print '\tbuilding twigs...',
		t.buildTwigs(ratio=PREFS['twig_ratio'].val)
		time3 = Blender.sys.time() # time print
		print '%.4f sec' % (time3 - time2)
	"""
	if 0: pass
	else:
		time3 = Blender.sys.time() # time print
	
	print '\tconnecting branches...',
	
	twig_ob_bounds = getObFromName(PREFS['twig_ob_bounds'].val)
	
	t.buildConnections(\
		sloppy = PREFS['connect_sloppy'].val,\
		connect_base_trim = PREFS['connect_base_trim'].val,\
		do_twigs = PREFS['do_twigs'].val,\
		twig_ratio = PREFS['twig_ratio'].val,\
		twig_select_mode = PREFS['twig_select_mode'].val,\
		twig_select_factor = PREFS['twig_select_factor'].val,\
		twig_scale = PREFS['twig_scale'].val,\
		twig_scale_width = PREFS['twig_scale_width'].val,\
		twig_random_orientation = PREFS['twig_random_orientation'].val,\
		twig_random_angle = PREFS['twig_random_angle'].val,\
		twig_recursive = PREFS['twig_recursive'].val,\
		twig_recursive_limit = PREFS['twig_recursive_limit'].val,\
		twig_ob_bounds = twig_ob_bounds,\
		twig_ob_bounds_prune = PREFS['twig_ob_bounds_prune'].val,\
		twig_ob_bounds_prune_taper = PREFS['twig_ob_bounds_prune_taper'].val,\
		twig_placement_maxradius = PREFS['twig_placement_maxradius'].val,\
		twig_placement_maxtwig = PREFS['twig_placement_maxtwig'].val,\
		twig_follow_parent = PREFS['twig_follow_parent'].val,\
		twig_follow_x = PREFS['twig_follow_x'].val,\
		twig_follow_y = PREFS['twig_follow_y'].val,\
		twig_follow_z = PREFS['twig_follow_z'].val,\
		do_variation = PREFS['do_variation'].val,\
		variation_seed = PREFS['variation_seed'].val,\
		variation_orientation = PREFS['variation_orientation'].val,\
		variation_scale = PREFS['variation_scale'].val,\
		do_twigs_fill = PREFS['do_twigs_fill'].val,\
		twig_fill_levels = PREFS['twig_fill_levels'].val,\
		twig_fill_rand_scale = PREFS['twig_fill_rand_scale'].val,\
		twig_fill_fork_angle_max = PREFS['twig_fill_fork_angle_max'].val,\
		twig_fill_radius_min = PREFS['twig_fill_radius_min'].val,\
		twig_fill_radius_factor = PREFS['twig_fill_radius_factor'].val,\
		twig_fill_shape_type = PREFS['twig_fill_shape_type'].val,\
		twig_fill_shape_rand = PREFS['twig_fill_shape_rand'].val,\
		twig_fill_shape_power = PREFS['twig_fill_shape_power'].val,\
	)
	
	time4 = Blender.sys.time() # time print
	print '%.4f sec' % (time4-time3)
	print '\toptimizing point spacing...',
	
	t.optimizeSpacing(\
		seg_density=PREFS['seg_density'].val,\
		seg_density_angle=PREFS['seg_density_angle'].val,\
		seg_density_radius=PREFS['seg_density_radius'].val,\
		joint_compression = PREFS['seg_joint_compression'].val,\
		joint_smooth = PREFS['seg_joint_smooth'].val\
	)
	
	time5 = Blender.sys.time() # time print
	print '%.4f sec' % (time5-time4)
	print '\tbuilding mesh...',
	
	ob_mesh = getObChild(ob_curve, 'Mesh')
	if not ob_mesh:
		# New object
		mesh = bpy.data.meshes.new('tree_' + ob_curve.name)
		ob_mesh = newObChild(ob_curve, mesh)
		# do subsurf later
	
	else:
		# Existing object
		mesh = ob_mesh.getData(mesh=1)
		ob_mesh.setMatrix(Matrix())
	
	# Do we need a do_uv_blend_layer?
	if PREFS['do_material'].val and PREFS['material_stencil'].val and PREFS['material_texture'].val:
		do_uv_blend_layer = True
	else:
		do_uv_blend_layer = False
	
	mesh = t.toMesh(mesh,\
		do_uv = PREFS['do_uv'].val,\
		uv_image = image,\
		do_uv_keep_vproportion = PREFS['do_uv_keep_vproportion'].val,\
		do_uv_vnormalize = PREFS['do_uv_vnormalize'].val,\
		do_uv_uscale = PREFS['do_uv_uscale'].val,\
		uv_x_scale = PREFS['uv_x_scale'].val,\
		uv_y_scale = PREFS['uv_y_scale'].val,\
		do_uv_blend_layer = do_uv_blend_layer,\
		do_cap_ends = PREFS['do_cap_ends'].val
	)
	
	if PREFS['do_leaf'].val:
		ob_leaf_dupliface = getObChild(ob_mesh, 'Mesh')
		if not ob_leaf_dupliface: # New object
			mesh_leaf = bpy.data.meshes.new('leaf_' + ob_curve.name)
			ob_leaf_dupliface = newObChild(ob_mesh, mesh_leaf)
		else:
			mesh_leaf = ob_leaf_dupliface.getData(mesh=1)
			ob_leaf_dupliface.setMatrix(Matrix())
		
		leaf_object = getObFromName(PREFS['leaf_object'].val)

		mesh_leaf = t.toLeafMesh(mesh_leaf,\
			leaf_branch_limit = PREFS['leaf_branch_limit'].val,\
			leaf_branch_limit_rand = PREFS['leaf_branch_limit_rand'].val,\
			leaf_size = PREFS['leaf_size'].val,\
			leaf_size_rand = PREFS['leaf_size_rand'].val,\
			leaf_branch_density = PREFS['leaf_branch_density'].val,\
			leaf_branch_pitch_angle = PREFS['leaf_branch_pitch_angle'].val,\
			leaf_branch_pitch_rand = PREFS['leaf_branch_pitch_rand'].val,\
			leaf_branch_roll_rand = PREFS['leaf_branch_roll_rand'].val,\
			leaf_branch_angle = PREFS['leaf_branch_angle'].val,\
			leaf_rand_seed = PREFS['leaf_rand_seed'].val,\
			leaf_object = leaf_object,\
		)
		
		if leaf_object:
			ob_leaf_dupliface.enableDupFaces = True
			ob_leaf_dupliface.enableDupFacesScale = True
			ob_leaf_dupliface.makeParent([leaf_object], 1)
		else:
			ob_leaf_dupliface.enableDupFaces = False
	
	mesh.calcNormals()
	
	if PREFS['do_material'].val:
		
		materials = mesh.materials
		if PREFS['material_use_existing'].val and materials:
			t.material = materials[0]
		else:
			t.material = bpy.data.materials.new(ob_curve.name)
			mesh.materials = [t.material]
		
		if PREFS['material_texture'].val:
			
			# Set up the base image texture
			t.texBase = bpy.data.textures.new('base_' + ob_curve.name)
			t.material.setTexture(0, t.texBase, Blender.Texture.TexCo.UV, Blender.Texture.MapTo.COL)
			t.texBase.type = Blender.Texture.Types.IMAGE
			if image:
				t.texBase.image = image
			t.texBaseMTex = t.material.getTextures()[0]
			t.texBaseMTex.uvlayer = 'base'
			
			if PREFS['material_stencil'].val:
				# Set up the blend texture
				t.texBlend = bpy.data.textures.new('blend_' + ob_curve.name)
				t.material.setTexture(1, t.texBlend, Blender.Texture.TexCo.UV, 0) # map to None
				t.texBlend.type = Blender.Texture.Types.BLEND
				t.texBlend.flags |= Blender.Texture.Flags.FLIPBLEND
				t.texBlendMTex = t.material.getTextures()[1]
				t.texBlendMTex.stencil = True
				t.texBlendMTex.uvlayer = 'blend'
				
				
				# Now make the texture for the stencil to blend, can reuse texBase here, jus tdifferent settings for the mtex
				t.material.setTexture(2, t.texBase, Blender.Texture.TexCo.UV, Blender.Texture.MapTo.COL)
				t.texJoinMTex = t.material.getTextures()[2]
				t.texJoinMTex.uvlayer = 'join'
				
				# Add a UV layer for blending
				
				
	
	
	time6 = Blender.sys.time() # time print
	print '%.4f sec' % (time6-time5)
	
	# Do armature stuff....
	if PREFS['do_armature'].val:
		
		print '\tbuilding armature & animation...',
		
		ob_arm = getObChild(ob_curve, 'Armature')
		if ob_arm:
			armature = ob_arm.data
			ob_arm.setMatrix(Matrix())
		else:
			armature = bpy.data.armatures.new()
			ob_arm = newObChild(ob_curve, armature)
		
		t.toArmature(ob_arm, armature)
		
		# Add the modifier.
		if not hasModifier(Blender.Modifier.Types.ARMATURE):
			mod = ob_mesh.modifiers.append(Blender.Modifier.Types.ARMATURE)
			
			# TODO - assigne object anyway, even if an existing modifier exists.
			mod[Blender.Modifier.Settings.OBJECT] = ob_arm
		
		if PREFS['do_anim'].val:
			try:
				tex = bpy.data.textures[PREFS['anim_tex'].val]
			except:
				tex = None
				Blender.Draw.PupMenu('error no texture, cannot animate bones')
			
			if tex:
				t.toAction(ob_arm, tex,\
						anim_speed = PREFS['anim_speed'].val,\
						anim_magnitude = PREFS['anim_magnitude'].val,\
						anim_speed_size_scale= PREFS['anim_speed_size_scale'].val,\
						anim_offset_scale=PREFS['anim_offset_scale'].val
						)
		
		time7 = Blender.sys.time() # time print
		print '%.4f sec\n' % (time7-time6)
	else:
		time7 = Blender.sys.time() # time print
	
	print 'done in %.4f sec' % (time7 - time1)
	
	# Add subsurf last it needed. so armature skinning is done first.
	# Do subsurf?
	if PREFS['do_subsurf'].val:
		if not hasModifier(Blender.Modifier.Types.SUBSURF):
			mod = ob_mesh.modifiers.append(Blender.Modifier.Types.SUBSURF)
			
	#ob_mesh.makeDisplayList()
	#mesh.update()
	bpy.data.scenes.active.update()

def do_pref_read(e=0,v=0, quiet=False):
	'''
	We dont care about e and v values, only there because its a callback
	'''
	sce = bpy.data.scenes.active
	ob = sce.objects.active
	
	if not ob:
		if not quiet:
			Blender.Draw.PupMenu('No active curve object')
		return
	
	if ob.type != 'Curve':
		ob = ob.parent
	
	if ob == None or ob.type != 'Curve':
		if not quiet:
			Blender.Draw.PupMenu('No active curve object')
		return
	
	if not IDProp2Prefs(ob.properties, PREFS):
		if not quiet:
			Blender.Draw.PupMenu('Curve object has no settings stored on it')
		return
	
	Blender.Draw.Redraw()

def do_pref_write(e,v):
	
	objects = getContextCurveObjects()
	if not objects:
		Blender.Draw.PupMenu('No curve objects selected')
		return
	
	for ob in objects:
		Prefs2IDProp(ob.properties, PREFS)
	
def do_pref_clear(e,v):
	objects = getContextCurveObjects()
	if not objects:
		Blender.Draw.PupMenu('No curve objects selected')
		return
	
	for ob in objects:
		try:	del ob.properties[ID_SLOT_NAME]
		except:	pass

def do_tex_check(e,v):
	if not v: return
	try:
		bpy.data.textures[v]
	except:
		PREFS['anim_tex'].val = ''
		Draw.PupMenu('Texture dosnt exist!')
		Draw.Redraw()

def do_ob_check(e,v):
	if not v: return
	try:
		bpy.data.objects[v]
	except:
		# PREFS['twig_ob_bounds'].val = ''
		Draw.PupMenu('Object dosnt exist!')
		Draw.Redraw()

def do_group_check(e,v):
	if not v: return
	try:
		bpy.data.groups[v]
	except:
		# PREFS['leaf_object'].val = ''
		Draw.PupMenu('dosnt exist!')
		Draw.Redraw()

# Button callbacks
def do_active_image(e,v):
	img = bpy.data.images.active
	if img:
		PREFS['image_main'].val = img.name
	else:
		PREFS['image_main'].val = ''

# Button callbacks
def do_tree_generate__real():
	sce = bpy.data.scenes.active
	objects = getContextCurveObjects()
	
	if not objects:
		Draw.PupMenu('Select one or more curve objects or a mesh/armature types with curve parents')
	
	is_editmode = Blender.Window.EditMode()
	if is_editmode:
		Blender.Window.EditMode(0, '', 0)
	Blender.Window.WaitCursor(1)
	
	for ob in objects:
		buildTree(ob, len(objects)==1)
	
	if is_editmode:
		Blender.Window.EditMode(1, '', 0)
	
	Blender.Window.RedrawAll()
	
	Blender.Window.WaitCursor(0)


# Profile
# Had to do this to get it to work in ubuntu "sudo aptitude install python-profiler"
'''
import hotshot
import profile
from hotshot import stats
'''
def do_tree_generate(e,v):
	
	do_tree_generate__real()
	'''
	prof = hotshot.Profile("hotshot_edi_stats")
	prof.runcall(do_tree_generate__real)
	prof.close()
	s = stats.load("hotshot_edi_stats")
	s.sort_stats("time").print_stats()
	'''
	if GLOBALS['non_bez_error']:
		Blender.Draw.PupMenu('Error%t|Nurbs and Poly curve types cant be used!')
		GLOBALS['non_bez_error'] = 0
		
def do_tree_help(e,v):
	url = 'http://wiki.blender.org/index.php/Scripts/Manual/Wizards/TreeFromCurves'
	print 'Trying to open web browser with documentation at this address...'
	print '\t' + url
	
	try:
		import webbrowser
		webbrowser.open(url)
	except:
		print '...could not open a browser window.'


def evt(e,val):
	pass

def bevt(e):
	
	if e==EVENT_NONE:
		return
	
	if e == EVENT_UPDATE or e == EVENT_UPDATE_AND_UI:
		if GLOBAL_PREFS['realtime_update'].val:
			do_tree_generate(0,0) # values dont matter
	
	if e == EVENT_REDRAW or e == EVENT_UPDATE_AND_UI:
		Draw.Redraw()
	if e == EVENT_EXIT:
		Draw.Exit()
	pass
	
def gui():
	MARGIN = 4
	rect = BPyWindow.spaceRect()
	but_width = int((rect[2]-MARGIN*2)/4.0) # 72
	# Clamp
	if but_width>100: but_width = 100
	but_height = 17
	
	x=MARGIN
	y=rect[3]-but_height-MARGIN
	xtmp = x

	Blender.Draw.BeginAlign()
	PREFS['do_twigs_fill'] =	Draw.Toggle('Fill Twigs',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_twigs_fill'].val,	'Generate child branches based existing branches'); xtmp += but_width*2;
	if PREFS['do_twigs_fill'].val:
		
		PREFS['twig_fill_levels'] =	Draw.Number('Generations',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_levels'].val, 1, 32,	'How many generations to make for filled twigs'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		# WARNING USED IN 2 PLACES!! - see below
		PREFS['twig_ob_bounds'] =	Draw.String('OB Bound: ',	EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['twig_ob_bounds'].val, 64,	'Only grow twigs inside this mesh object', do_ob_check); xtmp += but_width*2;
		PREFS['twig_fill_rand_scale'] =	Draw.Number('Randomize Scale',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_rand_scale'].val, 0.0, 1.0,	'Randomize twig scale from the bounding mesh'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x
		
		PREFS['twig_fill_radius_min'] =	Draw.Number('Min Radius',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_radius_min'].val, 0.0, 1.0,	'Radius at endpoints of all twigs'); xtmp += but_width*2;
		PREFS['twig_fill_radius_factor'] =	Draw.Number('Inherit Scale',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_radius_factor'].val, 0.0, 1.0,	'What attaching to branches, scale the radius by this value for filled twigs, 0.0 for fixed width twigs.'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x
		
		#PREFS['twig_fill_shape_type'] =	Draw.Number('Shape Type',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_shape_type'].val, 0.0, 1.0,	'Shape used for the fork'); xtmp += but_width*2;
		PREFS['twig_fill_shape_type'] =	Draw.Menu('Join Type%t|Even%x0|Smooth One Child%x1|Smooth Both Children%x2',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['twig_fill_shape_type'].val,	'Select the wat twigs '); xtmp += but_width*2;
		PREFS['twig_fill_fork_angle_max'] =	Draw.Number('Shape Max Ang',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_fork_angle_max'].val, 0.0, 180.0,	'Maximum fork angle'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x		
		
		PREFS['twig_fill_shape_rand'] =	Draw.Number('Shape Rand',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_shape_rand'].val, 0.0, 1.0,	'Randomize the shape of forks'); xtmp += but_width*2;
		PREFS['twig_fill_shape_power'] = Draw.Number('Shape Strength',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_fill_shape_power'].val, 0.0, 1.0,	'Strength of curves'); xtmp += but_width*2;
	
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	
	
	# ---------- ---------- ---------- ----------
	Blender.Draw.BeginAlign()
	PREFS['do_twigs'] =	Draw.Toggle('Grow Twigs',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_twigs'].val,	'Generate child branches based existing branches'); xtmp += but_width*2;
	if PREFS['do_twigs'].val:
		
		PREFS['twig_ratio'] =	Draw.Number('Twig Multiply',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_ratio'].val, 0.01, 500.0,	'How many twigs to generate per branch'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		PREFS['twig_select_mode'] =	Draw.Menu('Branch Selection Method%t|From Short%x0|From Long%x1|From Straight%x2|From Bent%x3|',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['twig_select_mode'].val,	'Select branches to use as twigs based on this attribute'); xtmp += but_width*2;
		PREFS['twig_select_factor'] =	Draw.Number('From Factor',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_select_factor'].val, 0.0, 16,	'Select branches, lower value is more strict and will give you less variation'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		PREFS['twig_recursive'] =	Draw.Toggle('Recursive Twigs',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['twig_recursive'].val,	'Recursively add twigs into eachother'); xtmp += but_width*2;
		PREFS['twig_recursive_limit'] =	Draw.Number('Generations',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_recursive_limit'].val, 0.0, 16,	'Number of generations allowed, 0 is inf'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_scale'] =	Draw.Number('Twig Scale',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_scale'].val, 0.01, 10.0,	'Scale down twigs in relation to their parents each generation'); xtmp += but_width*2;
		PREFS['twig_scale_width'] =	Draw.Number('Twig Scale Width',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_scale_width'].val, 0.01, 20.0,	'Scale the twig length only (not thickness)'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_random_orientation'] =	Draw.Number('Rand Orientation',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_random_orientation'].val, 0.0, 360.0,	'Random rotation around the parent'); xtmp += but_width*2;
		PREFS['twig_random_angle'] =	Draw.Number('Rand Angle',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_random_angle'].val, 0.0, 360.0,	'Random rotation to the parent joint'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_placement_maxradius'] =	Draw.Number('Place Max Radius',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_placement_maxradius'].val, 0.0, 50.0,	'Only place twigs on branches below this radius'); xtmp += but_width*2;
		PREFS['twig_placement_maxtwig'] =	Draw.Number('Place Max Count',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_placement_maxtwig'].val, 0.0, 50.0,	'Limit twig placement to this many per branch'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_follow_parent'] =	Draw.Number('ParFollow',	EVENT_UPDATE, xtmp, y, but_width, but_height, PREFS['twig_follow_parent'].val, 0.0, 10.0,	'Follow the parent branch'); xtmp += but_width;
		PREFS['twig_follow_x'] =	Draw.Number('Grav X',	EVENT_UPDATE, xtmp, y, but_width, but_height, PREFS['twig_follow_x'].val, -10.0, 10.0,	'Twigs gravitate on the X axis'); xtmp += but_width;
		PREFS['twig_follow_y'] =	Draw.Number('Grav Y',	EVENT_UPDATE, xtmp, y, but_width, but_height, PREFS['twig_follow_y'].val, -10.0, 10.0,	'Twigs gravitate on the Y axis'); xtmp += but_width;
		PREFS['twig_follow_z'] =	Draw.Number('Grav Z',	EVENT_UPDATE, xtmp, y, but_width, but_height, PREFS['twig_follow_z'].val, -10.0, 10.0,	'Twigs gravitate on the Z axis'); xtmp += but_width;
		
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		# WARNING USED IN 2 PLACES!!
		PREFS['twig_ob_bounds'] =	Draw.String('OB Bound: ',	EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['twig_ob_bounds'].val, 64,	'Only grow twigs inside this mesh object', do_ob_check); xtmp += but_width*2;
		
		if PREFS['twig_ob_bounds_prune'].val:
			but_width_tmp = but_width
		else:
			but_width_tmp = but_width*2
		
		PREFS['twig_ob_bounds_prune'] =	Draw.Toggle('Prune',EVENT_UPDATE_AND_UI, xtmp, y, but_width_tmp, but_height, PREFS['twig_ob_bounds_prune'].val,	'Prune twigs to the mesh object bounds'); xtmp += but_width_tmp;
		if PREFS['twig_ob_bounds_prune'].val:
			PREFS['twig_ob_bounds_prune_taper'] =	Draw.Number('Taper',	EVENT_UPDATE_AND_UI, xtmp, y, but_width, but_height, PREFS['twig_ob_bounds_prune_taper'].val, 0.0, 1.0,	'Taper pruned branches to a point'); xtmp += but_width;
		
		#PREFS['image_main'] =	Draw.String('IM: ',	EVENT_UPDATE, xtmp, y, but_width*3, but_height, PREFS['image_main'].val, 64,	'Image to apply to faces'); xtmp += but_width*3;
		#Draw.PushButton('Use Active',	EVENT_UPDATE, xtmp, y, but_width, but_height,	'Get the image from the active image window', do_active_image); xtmp += but_width;
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	
	
	Blender.Draw.BeginAlign()
	PREFS['do_leaf'] =	Draw.Toggle('Generate Leaves',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_leaf'].val,		'Generate leaves using duplifaces'); xtmp += but_width*2;
	
	if PREFS['do_leaf'].val:
		
		PREFS['leaf_object'] =	Draw.String('OB: ',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_object'].val, 64,	'Use this object as a leaf', do_ob_check); xtmp += but_width*2;
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['leaf_size'] =	Draw.Number('Size',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_size'].val, 0.001, 10.0,	'size of the leaf'); xtmp += but_width*2;
		PREFS['leaf_size_rand'] =	Draw.Number('Randsize',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_size_rand'].val, 0.0, 1.0,	'randomize the leaf size'); xtmp += but_width*2;
		
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		# Dont use yet
		PREFS['leaf_branch_limit'] =		Draw.Number('Branch Limit',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_limit'].val,	0.0, 1.0,	'Maximum thichness where a branch can bare leaves, higher value to place leaves on bigger branches'); xtmp += but_width*2;
		PREFS['leaf_branch_limit_rand'] =	Draw.Number('Limit Random',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_limit_rand'].val,	0.0, 1.0,	'Randomize the allowed minimum branch width to place leaves'); xtmp += but_width*2;
		
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['leaf_branch_density'] =	Draw.Number('Density',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_density'].val,	0.0, 1.0,	'Chance each segment has of baring a leaf, use a high value for more leaves'); xtmp += but_width*2;
		PREFS['leaf_branch_angle'] =	Draw.Number('Angle From Branch',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_angle'].val,	0.0, 90.0,	'angle the leaf is from the branch direction'); xtmp += but_width*2;
		
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['leaf_rand_seed'] =	Draw.Number('Random Seed',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_rand_seed'].val,	0.0, 10000.0,	'Set the seed for leaf random values'); xtmp += but_width*2;
		PREFS['leaf_branch_pitch_angle'] =	Draw.Number('Pitch Angle',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_pitch_angle'].val,	-180, 180.0,	'Change the pitch rotation of leaves, negative angle to point down'); xtmp += but_width*2;
		
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['leaf_branch_pitch_rand'] =	Draw.Number('Random Pitch',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_pitch_rand'].val,	0.0, 1.0,	'Randomize the leaf rotation (up-down/pitch)'); xtmp += but_width*2;
		PREFS['leaf_branch_roll_rand'] =	Draw.Number('Random Roll',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['leaf_branch_roll_rand'].val,	0.0, 1.0,	'Randomize the leaf rotation (roll/tilt/yaw)'); xtmp += but_width*2;
		
	
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	
	Blender.Draw.BeginAlign()
	if PREFS['do_uv'].val == 0:	but_width_tmp = but_width*2
	else:						but_width_tmp = but_width*4
	PREFS['do_uv'] =	Draw.Toggle('Generate UVs',EVENT_UPDATE_AND_UI, xtmp, y, but_width_tmp, but_height, PREFS['do_uv'].val,		'Calculate UVs coords'); xtmp += but_width_tmp;
	
	if PREFS['do_uv'].val:
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['do_uv_uscale'] =	Draw.Toggle('U-Scale',	EVENT_UPDATE, xtmp, y, but_width, but_height, PREFS['do_uv_uscale'].val,		'Scale the width according to the face size (will NOT tile)'); xtmp += but_width;
		PREFS['do_uv_keep_vproportion'] =	Draw.Toggle('V-Aspect',	EVENT_UPDATE, xtmp, y, but_width, but_height, PREFS['do_uv_keep_vproportion'].val,		'Correct the UV aspect with the branch width'); xtmp += but_width;
		PREFS['do_uv_vnormalize'] =	Draw.Toggle('V-Normaize',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['do_uv_vnormalize'].val,		'Scale the UVs to fit onto the image verticaly'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x
		# ---------- ---------- ---------- ----------
		
		PREFS['uv_x_scale'] =	Draw.Number('Scale U',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['uv_x_scale'].val,	0.01, 10.0,	'Edge loop spacing around branch join, lower value for less webed joins'); xtmp += but_width*2;
		PREFS['uv_y_scale'] =	Draw.Number('Scale V',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['uv_y_scale'].val,	0.01, 10.0,	'Edge loop spacing around branch join, lower value for less webed joins'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x
		# ---------- ---------- ---------- ----------
		
		PREFS['image_main'] =	Draw.String('IM: ',	EVENT_UPDATE, xtmp, y, but_width*3, but_height, PREFS['image_main'].val, 64,	'Image to apply to faces'); xtmp += but_width*3;
		Draw.PushButton('Use Active',	EVENT_UPDATE, xtmp, y, but_width, but_height,	'Get the image from the active image window', do_active_image); xtmp += but_width;
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	Blender.Draw.BeginAlign()
	PREFS['do_material'] =	Draw.Toggle('Generate Material',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_material'].val,		'Create material and textures (for seamless joints)'); xtmp += but_width*2;
	
	if PREFS['do_material'].val:
		PREFS['material_use_existing'] =	Draw.Toggle('ReUse Existing',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['material_use_existing'].val,		'Modify the textures of the existing material'); xtmp += but_width*2;
		
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['material_texture'] =	Draw.Toggle('Texture', EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['material_texture'].val,		'Create an image texture for this material to use'); xtmp += but_width*2;
		PREFS['material_stencil'] =	Draw.Toggle('Blend Joints',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['material_stencil'].val,		'Use a 2 more texture and UV layers to blend the seams between joints'); xtmp += but_width*2;
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	Blender.Draw.BeginAlign()
	if PREFS['do_armature'].val == 0:
		but_width_tmp = but_width*2
	else:
		but_width_tmp = but_width*4
	
	Blender.Draw.BeginAlign()
	PREFS['do_armature'] =	Draw.Toggle('Generate Motion',	EVENT_UPDATE_AND_UI, xtmp, y, but_width_tmp, but_height, PREFS['do_armature'].val,	'Generate Armatuer animation and apply to branches'); xtmp += but_width_tmp;
	
	# ---------- ---------- ---------- ----------
	if PREFS['do_armature'].val:
		y-=but_height
		xtmp = x
		
		PREFS['do_anim'] =	Draw.Toggle('Texture Anim',	EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_anim'].val,	'Use a texture to animate the bones'); xtmp += but_width*2;
		
		if PREFS['do_anim'].val:
			
			PREFS['anim_tex'] =	Draw.String('TEX: ',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['anim_tex'].val, 64,	'Texture to use for the IPO Driver animation', do_tex_check); xtmp += but_width*2;
			y-=but_height
			xtmp = x		
			# ---------- ---------- ---------- ----------
			
			PREFS['anim_speed'] =		Draw.Number('Speed',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['anim_speed'].val,	0.001, 10.0,	'Animate the movement faster with a higher value'); xtmp += but_width*2;
			PREFS['anim_magnitude'] =	Draw.Number('Magnitude',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['anim_magnitude'].val,	0.001, 10.0,	'Animate with more motion with a higher value'); xtmp += but_width*2;
			y-=but_height
			xtmp = x
			# ---------- ---------- ---------- ----------
			
			PREFS['anim_offset_scale'] =	Draw.Number('Unique Offset Scale',	EVENT_UPDATE, xtmp, y, but_width*4, but_height, PREFS['anim_offset_scale'].val,	0.001, 10.0,	'Use the curve object location as input into the texture so trees have more unique motion, a low value is less unique'); xtmp += but_width*4;
			y-=but_height
			xtmp = x
			
			# ---------- ---------- ---------- ----------
			
			PREFS['anim_speed_size_scale'] =	Draw.Toggle('Branch Size Scales Speed',	EVENT_UPDATE, xtmp, y, but_width*4, but_height, PREFS['anim_speed_size_scale'].val,	'Use the branch size as a factor when calculating speed'); xtmp += but_width*4;
	
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	
	
	
	
	# ---------- ---------- ---------- ----------
	
	Blender.Draw.BeginAlign()
	PREFS['do_variation'] =	Draw.Toggle('Generate Variation',	EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_variation'].val,	'Create a variant by moving the branches'); xtmp += but_width*2;
	
	# ---------- ---------- ---------- ----------
	if PREFS['do_variation'].val:
		PREFS['variation_seed'] =		Draw.Number('Rand Seed',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['variation_seed'].val,	1, 100000,	'Change this to get a different variation'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		
		PREFS['variation_orientation'] =		Draw.Number('Orientation',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['variation_orientation'].val,	0, 1.0,	'Randomize rotation of the branch around its parent'); xtmp += but_width*2;
		PREFS['variation_scale'] =	Draw.Number('Scale',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['variation_scale'].val,	0.0, 1.0,	'Randomize the scale of branches'); xtmp += but_width*2;
	
	Blender.Draw.EndAlign()
	
	y-=but_height+(MARGIN*2)
	xtmp = x
	
	
	
	# ---------- ---------- ---------- ----------
	Blender.Draw.BeginAlign()
	PREFS['seg_density'] =	Draw.Number('Segment Spacing',EVENT_UPDATE, xtmp, y, but_width*4, but_height, PREFS['seg_density'].val,	0.05, 10.0,	'Scale the limit points collapse, that are closer then the branch width'); xtmp += but_width*4;
	
	y-=but_height
	xtmp = x

	# ---------- ---------- ---------- ----------
	PREFS['seg_density_angle'] =	Draw.Number('Angle Spacing',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['seg_density_angle'].val,	0.0, 180.0,	'Segments above this angle will not collapse (lower value for more detail)'); xtmp += but_width*2;
	PREFS['seg_density_radius'] =	Draw.Number('Radius Spacing',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['seg_density_radius'].val,	0.0, 1.0,	'Segments above this difference in radius will not collapse (lower value for more detail)'); xtmp += but_width*2;
	
	y-=but_height
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	PREFS['seg_joint_compression'] =	Draw.Number('Joint Width',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['seg_joint_compression'].val,	0.1, 2.0,	'Edge loop spacing around branch join, lower value for less webed joins'); xtmp += but_width*2;
	PREFS['seg_joint_smooth'] =	Draw.Number('Joint Smooth',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['seg_joint_smooth'].val,	0.0, 1.0,	'Edge loop spacing around branch join, lower value for less webed joins'); xtmp += but_width*2;
	
	y-=but_height
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	PREFS['connect_sloppy'] =	Draw.Number('Connect Limit',EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['connect_sloppy'].val,	0.1, 3.0,	'Strictness when connecting branches'); xtmp += but_width*2;
	PREFS['connect_base_trim'] =	Draw.Number('Joint Bevel',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['connect_base_trim'].val,	0.0, 2.0,	'low value for a tight join, hi for a smoother bevel'); xtmp += but_width*2;
	Blender.Draw.EndAlign()
	y-=but_height+MARGIN
	xtmp = x
	
	# ---------- ---------- ---------- ----------
	Blender.Draw.BeginAlign()
	PREFS['do_cap_ends'] =	Draw.Toggle('Cap Ends',EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['do_cap_ends'].val,		'Add faces onto branch endpoints'); xtmp += but_width*2;
	PREFS['do_subsurf'] =	Draw.Toggle('SubSurf',EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['do_subsurf'].val,		'Enable subsurf for newly generated objects'); xtmp += but_width*2;
	Blender.Draw.EndAlign()
	y-=but_height+MARGIN
	xtmp = x
	
	
	# ---------- ---------- ---------- ----------
	Blender.Draw.BeginAlign()
	Draw.PushButton('Read Active Prefs',	EVENT_REDRAW, xtmp, y, but_width*2, but_height,	'Read the ID Property settings from the active curve object', do_pref_read); xtmp += but_width*2;
	Draw.PushButton('Write Prefs to Sel',	EVENT_NONE, xtmp, y, but_width*2, but_height,	'Save these settings in the ID Properties of all selected curve objects', do_pref_write); xtmp += but_width*2;

	y-=but_height
	xtmp = x
	
	# ---------- ---------- ---------- ----------
	Draw.PushButton('Clear Prefs from Sel',	EVENT_NONE, xtmp, y, but_width*4, but_height,	'Remove settings from the selected curve aaobjects', do_pref_clear); xtmp += but_width*4;
	Blender.Draw.EndAlign()

	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	Blender.Draw.BeginAlign()
	Draw.PushButton('Exit',	EVENT_EXIT, xtmp, y, but_width, but_height,	''); xtmp += but_width;
	Draw.PushButton('Help',	EVENT_NONE, xtmp, y, but_width, but_height,	'', do_tree_help); xtmp += but_width;
	Draw.PushButton('Generate from selection',	EVENT_REDRAW, xtmp, y, but_width*2, but_height,	'Generate mesh', do_tree_generate); xtmp += but_width*3;
	Blender.Draw.EndAlign()
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	GLOBAL_PREFS['realtime_update'] =	Draw.Toggle('Automatic Update',	EVENT_UPDATE, xtmp, y, but_width*4, but_height, GLOBAL_PREFS['realtime_update'].val,	'Update automatically when settings change'); xtmp += but_width*4;
	
	

if __name__ == '__main__':
	# Read the active objects prefs on load. if they exist
	do_pref_read(quiet=True)
	
	Draw.Register(gui, evt, bevt)
