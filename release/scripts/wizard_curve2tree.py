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
from Blender.Mathutils import Vector, Matrix, CrossVecs, AngleBetweenVecs, LineIntersect, TranslationMatrix, ScaleMatrix, RotationMatrix, Rand
from Blender.Geometry import ClosestPointOnLine

GLOBALS = {}
GLOBALS['non_bez_error'] = 0

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

eul = 0.00001

class tree:
	def __init__(self):
		self.branches_all =		[]
		self.branches_root =	[]
		self.branches_twigs =	[]
		self.mesh = None
		self.armature = None
		self.objectCurve = None
		self.objectCurveMat = None
		self.objectTwigBounds = None # use for twigs only at the moment.
		self.objectTwigBoundsIMat = None
		self.objectTwigBoundsMesh = None
		self.limbScale = 1.0
		
		self.debug_objects = []
	
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
		curve = objectCurve.data
		steps = curve.resolu # curve resolution
		
		# Set the curve object scale
		if curve.bevob:
			# A bit of a hack to guess the size of the curve object if you have one.
			bb = curve.bevob.boundingBox
			# self.limbScale = (bb[0] - bb[7]).length / 2.825 # THIS IS GOOD WHEN NON SUBSURRFED
			self.limbScale = (bb[0] - bb[7]).length / 1.8
		
		# forward_diff_bezier will fill in the blanks
		# nice we can reuse these for every curve segment :)
		pointlist = [[None, None, None] for i in xrange(steps+1)]
		radlist = [ None for i in xrange(steps+1) ]


		for spline in curve:
			
			if len(spline) < 2: # Ignore single point splines
				continue
			
			if spline.type != 1: # 0 poly, 1 bez, 4 nurbs
				GLOBALS['non_bez_error'] = 1
				continue
			
				
			brch = branch()
			self.branches_all.append(brch)
			
			bez_list = list(spline)
			for i in xrange(1, len(bez_list)):
				bez1 = bez_list[i-1]
				bez2 = bez_list[i]
				bez1_vec = bez1.vec
				bez2_vec = bez2.vec
				
				radius1 = bez1.radius
				radius2 = bez2.radius
				
				# x,y,z,axis
				for ii in (0,1,2):
					forward_diff_bezier(bez1_vec[1][ii], bez1_vec[2][ii],  bez2_vec[0][ii], bez2_vec[1][ii], pointlist, steps, ii)
				
				# radius - no axis, Copied from blenders BBone roll interpolation.
				forward_diff_bezier(radius1, radius1 + 0.390464*(radius2-radius1), radius2 - 0.390464*(radius2-radius1),	radius2,	radlist, steps, None)
				
				bpoints = [ bpoint(brch, Vector(pointlist[ii]), Vector(), radlist[ii] * self.limbScale) for ii in xrange(len(pointlist)) ]
				
				# remove endpoint for all but the last
				if i != len(bez_list)-1:
					bpoints.pop()
				
				brch.bpoints.extend(bpoints)
			
			# Finalize once point data is there
			brch.calcData()
			
		# Sort from big to small, so big branches get priority
		self.branches_all.sort( key = lambda brch: -brch.bpoints[0].radius )
		
	def setTwigBounds(self, objectMesh):
		self.objectTwigBounds = objectMesh
		self.objectTwigBoundsMesh = objectMesh.getData(mesh=1)
		self.objectTwigBoundsIMat = objectMesh.matrixWorld.copy().invert()
		#self.objectTwigBoundsIMat = objectMesh.matrixWorld.copy()
		
		for brch in self.branches_all:
			brch.calcTwigBounds(self)
	
	def isPointInTwigBounds(self, co):
		return self.objectTwigBoundsMesh.pointInside(co * self.objectCurveMat * self.objectTwigBoundsIMat)
	
	def resetTags(self, value):
		for brch in self.branches_all:
			brch.tag = value
	
	def buildConnections(	self,\
							sloppy = 1.0,\
							base_trim = 1.0,\
							do_twigs = False,\
							twig_ratio = 2.0,\
							twig_scale = 0.8,\
							twig_lengthen = 1.0,\
							twig_random_orientation = 180,\
							twig_random_angle = 33,\
							twig_recursive=True,\
							twig_recursive_limit=3,\
							twig_ob_bounds=None,\
							twig_ob_bounds_prune=True,\
							twig_ob_bounds_prune_taper=True,\
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
						if dist < pt_best_j.radius * sloppy:
							brch_i.parent_pt = pt_best_j
							pt_best_j.childCount += 1 # dont remove me
							
							brch_i.baseTrim(base_trim)
							
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
				# calculate the median point the 2 segments would span
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
		
		# Important we so this with existing parent/child but before connecting and calculating verts.
		
		if do_twigs:
			irational_num = 22.0/7.0 # use to make the random number more odd
			
			if twig_ob_bounds: # Only spawn twigs inside this mesh
				self.setTwigBounds(twig_ob_bounds)
			
			if not twig_recursive:
				twig_recursive_limit = 0
			
			self.buildTwigs(twig_ratio)
			
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
					if twig_pt_index != -1 and (twig_recursive_limit==0 or brch_parent.generation <= twig_recursive_limit):
						
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
						rnd1 = (((irational_num * scale * 10000000) % 360) - 180) * twig_random_orientation
						rnd2 = (((irational_num * scale * 66666666) % 360) - 180) * twig_random_angle
						
						
						# Align this with the existing branch
						angle = AngleBetweenVecs(zup, parent_pt.no)
						cross = CrossVecs(zup, parent_pt.no)
						mat_align = RotationMatrix(angle, 3, 'r', cross)
						
						# Use the bend on the point to work out which way to make the branch point!
						if parent_pt.prev:	cross = CrossVecs(parent_pt.no, parent_pt.prev.no - parent_pt.no)
						else:				cross = CrossVecs(parent_pt.no, parent_pt.next.no - parent_pt.no)
						
						if parent_pt.branch.parent_pt:
							angle = AngleBetweenVecs(parent_pt.branch.parent_pt.no, parent_pt.no)
						else:
							# Should add a UI for this... only happens when twigs come off a root branch
							angle = 66
						
						mat_branch_angle = RotationMatrix(angle+rnd1, 3, 'r', cross)
						mat_scale = Matrix([scale,0,0],[0,scale,0],[0,0,scale])
						
						mat_orientation = RotationMatrix(rnd2, 3, 'r', parent_pt.no)
						
						if twig_lengthen != 1.0:
							# adjust length - no radius adjusting
							for pt in brch_twig.bpoints:
								pt.co *= twig_lengthen
						
						brch_twig.transform(mat_scale * mat_branch_angle * mat_align * mat_orientation, parent_pt.co)
						
						# When using a bounding mesh, clip and calculate points in bounds.
						#print "Attempting to trim base"
						brch_twig.baseTrim(base_trim)
						
						if twig_ob_bounds and (twig_ob_bounds_prune or twig_recursive):
							brch_twig.calcTwigBounds(self)
						
							# we would not have been but here if the bounds were outside
							if twig_ob_bounds_prune:
								brch_twig.boundsTrim()
								if twig_ob_bounds_prune_taper:
									# taper to a point. we could use some nice taper algo here - just linear atm.
									
									brch_twig.taper()
						
						# Make sure this dosnt mess up anything else
						
						brch_twig_index += 1
						
						# Add to the branches
						#self.branches_all.append(brch_twig)
						if len(brch_twig.bpoints) > 2:
							branches_twig_attached.append(brch_twig)
							brch_twig.generation = brch_parent.generation + 1
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
		
		for brch in self.branches_all:
			brch.branchReJoin()
	
	def buildTwigs(self, twig_ratio=1.0):
		
		ratio_int = int(len(self.branches_all) * twig_ratio)
		if ratio_int == 0:
			return
		
		# So we only mix branches of similar lengths
		branches_sorted = self.branches_all[:]
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
		
		
	
	def toMesh(self, mesh=None, do_uv=True, do_uv_keep_vproportion=True, do_uv_vnormalize=False, do_uv_uscale=False, uv_image = None, uv_x_scale=1.0, uv_y_scale=4.0, do_uv_blend_layer= False, do_cap_ends=False):
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

		faces.extend(faces_extend)
		
		if do_uv:
			# Assign the faces back
			face_index = 0
			for brch in self.branches_all:
				if brch.parent_pt:
					for i in (0,1,2,3):
						face = brch.faces[i] = faces[face_index+i]
						face.smooth = 1
					face_index +=4
				
				for pt in brch.bpoints:
					for i in (0,1,2,3):
						if pt.faces[i]:
							pt.faces[i] = faces[face_index]
							pt.faces[i].smooth = True
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
			for f in self.mesh.faces:
				f.smooth = True
		
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
	
	def toLeafMesh(self, mesh_leaf, leaf_branch_limit = 0.5, leaf_size = 0.5):
		'''
		return a mesh with leaves seperate from the tree
		
		Add to the existing mesh.
		'''
		
		# first collect stats, we want to know the average radius and total segments
		#radius = [(pt.radius for pt in self.branches_all for pt in brch.bpoints for pt in brch.bpoints]
		mesh_leaf = freshMesh(mesh_leaf)
		self.mesh_leaf = mesh_leaf
		
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
		
		co1,co2,co3,co4 = Vector(),Vector(),Vector(),Vector()
		
		for brch in self.branches_all:
			
			# quick test, do we need leaves on this branch?
			if brch.bpoints[-1].radius > radius_max:
				continue
			
			count = 0
			for pt in brch.bpoints:
				if pt.childCount == 0 and pt.radius < radius_max:
					# Ok we can add a leaf here. set the co's correctly
					co1[:] = pt.co
					co2[:] = pt.co
					co3[:] = pt.co
					co4[:] = pt.co
					
					cross_leafdir = CrossVecs( zup, pt.no )
					cross_leafdir.length = leaf_size

					
					#cross_leafwidth = CrossVecs(pt.no, cross_leafdir)
					
					# Facing up
					cross_leafwidth_up = CrossVecs(zup, cross_leafdir).normalize() * leaf_size
					cross_leafwidth_aligned = pt.no
					
					#cross_leafwidth = (cross_leafwidth_up + cross_leafwidth_aligned)/2
					cross_leafwidth = cross_leafwidth_aligned
					
					cross_leafwidth.length = leaf_size/2
					
					if count % 2:
						cross_leafwidth.negate()
						cross_leafdir.negate()
					
					co1 += cross_leafdir
					co2 += cross_leafdir
					
					co2 += cross_leafwidth
					co3 += cross_leafwidth
					
					co1 -= cross_leafwidth
					co4 -= cross_leafwidth
					
					
					i = len(verts_extend)
					faces_extend.append( (i,i+1,i+2,i+3) )
					verts_extend.extend([tuple(co1), tuple(co2), tuple(co3), tuple(co4)])
					count += 1
		
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
				cross = CrossVecs(self.no, self.branch.getParentFaceCent() - self.branch.parent_pt.getAbsVec( self.branch.getParentQuadIndex() ))
			else:
				# parentless branch - for best results get a cross thats not the same as the normal, in rare cases this happens.
				
				# Was just doing 
				#  cross = zup
				# which works most of the time, but no verticle lines
				
				if AngleBetweenVecs(self.no, zup) > 1.0:	cross = zup
				elif AngleBetweenVecs(self.no, yup) > 1.0:	cross = yup
				else:										cross = xup
				
		else:
			cross = CrossVecs(self.prev.vecs[0], self.no)
		
		self.vecs[0] = Blender.Mathutils.CrossVecs(self.no, cross)
		self.vecs[0].length = self.radius
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
		
		# self.myindex = -1
		### self.segment_spacing_scale = 1.0 # use this to scale up the spacing - so small twigs dont get WAY too many polys
	
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
			return AngleBetweenVecs(self.parent_pt.no, self.bpoints[0].no )
		else:
			return 45.0
	
	def getParentRadiusRatio(self):
		if self.parent_pt:
			return self.bpoints[0].radius / self.parent_pt.radius
		else:
			return 0.8
	
		
	
	def getLength(self):
		return (self.bpoints[0].co - self.bpoints[-1].co).length
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
				
	
	def baseTrim(self, base_trim):
		# if 1)	dont remove the whole branch, maybe an option but later
		# if 2)	we are alredy a parent, cant remove me now.... darn :/ not nice...
		#		could do this properly but it would be slower and its a corner case.
		#
		# if 3)	this point is within the branch, remove it.
		#		Scale this value by the difference in radius, a low trim looks better when the parent is a lot bigger..
		# 
		
		while	len(self.bpoints)>2 and\
				self.bpoints[0].childCount == 0 and\
				(self.bpoints[0].co - self.parent_pt.nextMidCo).length / (1+ ((self.bpoints[0].radius/self.parent_pt.radius) / base_trim)) < self.parent_pt.radius * base_trim:
			
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
	
	def taper(self):
		l = float(len( self.bpoints ))
		
		for i, pt in enumerate(self.bpoints):
			pt.radius *= (l-i)/l
	
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
		
		for pt in self.bpoints:
			pt.targetCos = []
			if pt.childrenMidCo:
				# Move this and the next segment to be around the child point.
				# TODO - factor in the branch angle, be careful with this - close angles can have extreme values.
				co = pt.slideCo( (pt.childrenMidCo - pt.co).length - (pt.childrenMidRadius * joint_compression) )
				if co:
					pt.targetCos.append( co )
				
				co = pt.next.slideCo((pt.childrenMidRadius * joint_compression) - (pt.childrenMidCo - pt.next.co).length )
				if co:
					pt.next.targetCos.append( co )
		
		for pt in self.bpoints:
			pt.applyTargetLocation()
	
	def collapsePoints(self, seg_density=0.5, seg_density_angle=20.0, seg_density_radius=0.3, smooth_joint=1.0):
		
		collapse = True
		while collapse:
			collapse = False
			pt = self.bpoints[0]
			while pt:
				if pt.prev and pt.next and pt.prev.childCount == 0:
					if abs(pt.radius - pt.prev.radius) / (pt.radius + pt.prev.radius) < seg_density_radius:
						if seg_density_angle == 180 or AngleBetweenVecs(pt.no, pt.prev.no) < seg_density_angle:
							## if (pt.prev.nextMidCo-pt.co).length < ((pt.radius + pt.prev.radius)/2) * seg_density:
							if (pt.prev.nextMidCo-pt.co).length < seg_density:
								pt_save = pt.prev
								if pt.next.collapseUp(): # collapse this point
									collapse = True
									pt = pt_save # so we never reference a removed point
				
				if pt.childCount == 0 and pt.next: #if pt.childrenMidCo == None:
					if abs(pt.radius - pt.next.radius) / (pt.radius + pt.next.radius) < seg_density_radius:
						if seg_density_angle == 180 or AngleBetweenVecs(pt.no, pt.next.no) < seg_density_angle:
							# do here because we only want to run this on points with no children,
							# Are we closer theto eachother then the radius?
							## if (pt.nextMidCo-pt.co).length < ((pt.radius + pt.next.radius)/2) * seg_density:
							if (pt.nextMidCo-pt.co).length < seg_density:
								if pt.collapseDown():
									collapse = True
				
				pt = pt.next
		## self.checkPointList()
		self.evenPointDistrobution(1.0, smooth_joint)
		
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
			mat = RotationMatrix(AngleBetweenVecs(cos_ls[-1], zup), 3, 'r', cross)
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
			mat = RotationMatrix(AngleBetweenVecs(xy_nor, xup), 3, 'r', cross) * Matrix([scale,0,0],[0,scale,0],[0,0,scale])
			cos_ls[:] = [co*mat for co in cos_ls]
			
			scales.append(scale)
		
		# Make the new branch
		new_brch = branch()
		for i in xrange(len(cos1)):
			new_brch.bpoints.append( bpoint(new_brch, (cos1[i]+cos2[i])*0.5, Vector(), (brch1.bpoints[i].radius*scales[0] + brch2.bpoints[i].radius*scales[1])/2) )
		
		new_brch.calcData()
		return new_brch
		
	
	def toMesh(self):
		pass





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
PREFS['connect_sloppy'] = Draw.Create(1.0)
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
PREFS['do_material'] = Draw.Create(1)
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

PREFS['do_twigs'] = Draw.Create(0)
PREFS['twig_ratio'] = Draw.Create(2.0)
PREFS['twig_scale'] = Draw.Create(0.8)
PREFS['twig_lengthen'] = Draw.Create(1.0)
PREFS['twig_random_orientation'] = Draw.Create(180)
PREFS['twig_random_angle'] = Draw.Create(33)
PREFS['twig_recursive'] = Draw.Create(1)
PREFS['twig_recursive_limit'] = Draw.Create(3)
PREFS['twig_ob_bounds'] = Draw.Create('')
PREFS['twig_ob_bounds_prune'] = Draw.Create(1)
PREFS['twig_ob_bounds_prune_taper'] = Draw.Create(1)

PREFS['do_leaf'] = Draw.Create(1)
PREFS['leaf_branch_limit'] = Draw.Create(0.25)
PREFS['leaf_size'] = Draw.Create(0.5)

GLOBAL_PREFS = {}
GLOBAL_PREFS['realtime_update'] = Draw.Create(0)


def getContextCurveObjects():
	sce = bpy.data.scenes.active
	objects = []
	for ob in sce.objects.context:
		if ob.type != 'Curve':
			ob = ob.parent
		if not ob or ob.type != 'Curve':
			continue
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
		try:	new_prefs[key] = Blender.Draw.Create( val )
		except:	new_prefs[key] = val
	return new_prefs

def Prefs2IDProp(idprop, prefs):
	new_prefs = {}
	Prefs2Dict(prefs, new_prefs)
	try:	del idprop[ID_SLOT_NAME]
	except:	pass
	
	idprop[ID_SLOT_NAME] = new_prefs
	
def IDProp2Prefs(idprop, prefs):
	try:	prefs = idprop[ID_SLOT_NAME]
	except:	return False
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
		ob_new.Layers = parent.Layers
		
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
	
	twig_ob_bounds = PREFS['twig_ob_bounds'].val
	if twig_ob_bounds:
		try:	twig_ob_bounds = bpy.data.objects[twig_ob_bounds]
		except:	twig_ob_bounds = None
	else:
		twig_ob_bounds = None
	#print t
	t.buildConnections(\
		sloppy = PREFS['connect_sloppy'].val,\
		base_trim = PREFS['connect_base_trim'].val,\
		do_twigs = PREFS['do_twigs'].val,\
		twig_ratio = PREFS['twig_ratio'].val,\
		twig_scale = PREFS['twig_scale'].val,\
		twig_lengthen = PREFS['twig_lengthen'].val,\
		twig_random_orientation = PREFS['twig_random_orientation'].val,\
		twig_random_angle = PREFS['twig_random_angle'].val,\
		twig_recursive = PREFS['twig_recursive'].val,\
		twig_recursive_limit = PREFS['twig_recursive_limit'].val,\
		twig_ob_bounds = twig_ob_bounds,\
		twig_ob_bounds_prune = PREFS['twig_ob_bounds_prune'].val,\
		twig_ob_bounds_prune_taper = PREFS['twig_ob_bounds_prune_taper'].val,\
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
	if PREFS['material_stencil'].val and PREFS['material_texture'].val:
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
	"""
	if PREFS['do_leaf'].val:
		ob_leaf = getObChild(ob_mesh, 'Mesh')
		if not ob_leaf: # New object
			mesh_leaf = bpy.data.meshes.new('tree_' + ob_curve.name)
			ob_leaf = newObChild(ob_mesh, mesh_leaf)
		else:
			mesh_leaf = ob_leaf.getData(mesh=1)
			ob_leaf.setMatrix(Matrix())
		
		mesh_leaf = t.toLeafMesh(mesh_leaf,\
			leaf_branch_limit = PREFS['leaf_branch_limit'].val,\
			leaf_size = PREFS['leaf_size'].val,\
		)
	"""
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

def do_pref_read(e,v):
	sce = bpy.data.scenes.active
	ob = sce.objects.active
	
	if not ob:
		Blender.Draw.PupMenu('No active curve object')
	
	if ob.type != 'Curve':
		ob = ob.parent
	
	if ob.type != 'Curve':
		Blender.Draw.PupMenu('No active curve object')
		return
	
	if not IDProp2Prefs(ob.properties, PREFS):
		Blender.Draw.PupMenu('Curve object has no settings stored on it')
	
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
		try:	del idprop[ID_SLOT_NAME]
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
		PREFS['twig_ob_bounds'].val = ''
		Draw.PupMenu('Object dosnt exist!')
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
	Blender.Window.WaitCursor(0)
	if is_editmode:
		Blender.Window.EditMode(1, '', 0)
	
	Blender.Window.RedrawAll()


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
	MARGIN = 10
	rect = BPyWindow.spaceRect()
	but_width = int((rect[2]-MARGIN*2)/4.0) # 72
	# Clamp
	if but_width>100: but_width = 100
		
	but_height = 20
	
	
	x=MARGIN
	y=rect[3]-but_height-MARGIN
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
	
	PREFS['connect_sloppy'] =	Draw.Number('Connect Limit',EVENT_UPDATE, xtmp, y, but_width*4, but_height, PREFS['connect_sloppy'].val,	0.1, 2.0,	'Strictness when connecting branches'); xtmp += but_width*4;
	
	y-=but_height
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	PREFS['connect_base_trim'] =	Draw.Number('Trim Base',	EVENT_UPDATE, xtmp, y, but_width*4, but_height, PREFS['connect_base_trim'].val,	0.1, 2.0,	'Trim branch base to better connect with parent branch'); xtmp += but_width*4;
	Blender.Draw.EndAlign()
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	Blender.Draw.BeginAlign()
	PREFS['do_twigs'] =	Draw.Toggle('Generate Twigs',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['do_twigs'].val,	'Generate child branches based existing branches'); xtmp += but_width*2;
	if PREFS['do_twigs'].val:
		
		PREFS['twig_ratio'] =	Draw.Number('Twig Multiply',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_ratio'].val, 0.01, 500.0,	'How many twigs to generate per branch'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		PREFS['twig_recursive'] =	Draw.Toggle('Recursive Twigs',EVENT_UPDATE_AND_UI, xtmp, y, but_width*2, but_height, PREFS['twig_recursive'].val,	'Recursively add twigs into eachother'); xtmp += but_width*2;
		PREFS['twig_recursive_limit'] =	Draw.Number('Generations',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_recursive_limit'].val, 0.0, 16,	'Number of generations allowed, 0 is inf'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_scale'] =	Draw.Number('Twig Scale',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_scale'].val, 0.01, 1.0,	'Scale down twigs in relation to their parents each generation'); xtmp += but_width*2;
		PREFS['twig_lengthen'] =	Draw.Number('Twig Lengthen',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_lengthen'].val, 0.01, 20.0,	'Scale the twig length only (not thickness)'); xtmp += but_width*2;
		y-=but_height
		xtmp = x
		
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_random_orientation'] =	Draw.Number('Rand Orientation',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_random_orientation'].val, 0.0, 360.0,	'Random rotation around the parent'); xtmp += but_width*2;
		PREFS['twig_random_angle'] =	Draw.Number('Rand Angle',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_random_angle'].val, 0.0, 360.0,	'Random rotation to the parent joint'); xtmp += but_width*2;
		
		#PREFS['uv_y_scale'] =	Draw.Number('Scale V',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['uv_y_scale'].val,	0.01, 10.0,	'Edge loop spacing around branch join, lower value for less webed joins'); xtmp += but_width*2;
		
		y-=but_height
		xtmp = x
		# ---------- ---------- ---------- ----------
		
		PREFS['twig_ob_bounds'] =	Draw.String('OB Bound: ',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['twig_ob_bounds'].val, 64,	'Only grow twigs inside this mesh object', do_ob_check); xtmp += but_width*2;
		if PREFS['twig_ob_bounds_prune'].val:
			but_width_tmp = but_width
		else:
			but_width_tmp = but_width*2
		
		PREFS['twig_ob_bounds_prune'] =	Draw.Toggle('Prune',EVENT_UPDATE_AND_UI, xtmp, y, but_width_tmp, but_height, PREFS['twig_ob_bounds_prune'].val,	'Prune twigs to the mesh object bounds'); xtmp += but_width_tmp;
		if PREFS['twig_ob_bounds_prune'].val:
			PREFS['twig_ob_bounds_prune_taper'] =	Draw.Toggle('Taper',EVENT_UPDATE_AND_UI, xtmp, y, but_width, but_height, PREFS['twig_ob_bounds_prune_taper'].val,	'Taper pruned branches to a point'); xtmp += but_width;
		
		
		#PREFS['image_main'] =	Draw.String('IM: ',	EVENT_UPDATE, xtmp, y, but_width*3, but_height, PREFS['image_main'].val, 64,	'Image to apply to faces'); xtmp += but_width*3;
		#Draw.PushButton('Use Active',	EVENT_UPDATE, xtmp, y, but_width, but_height,	'Get the image from the active image window', do_active_image); xtmp += but_width;
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	
	
	"""
	Blender.Draw.BeginAlign()
	if PREFS['do_leaf'].val == 0:
		but_width_tmp = but_width*2
	else:
		but_width_tmp = but_width*4
	PREFS['do_leaf'] =	Draw.Toggle('Generate Leaves',EVENT_UPDATE_AND_UI, xtmp, y, but_width_tmp, but_height, PREFS['do_leaf'].val,		'Generate a separate leaf mesh'); xtmp += but_width_tmp;
	
	if PREFS['do_leaf'].val:
		# ---------- ---------- ---------- ----------
		y-=but_height
		xtmp = x
		
		PREFS['leaf_branch_limit'] =	Draw.Number('Branch Limit',	EVENT_UPDATE, xtmp, y, but_width*4, but_height, PREFS['leaf_branch_limit'].val,	0.1, 2.0,	'Maximum thichness where a branch can bare leaves'); xtmp += but_width*4;
		
		'''
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
		'''
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	"""
	
	
	
	
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
		PREFS['material_stencil'] =	Draw.Toggle('Blend Joints',	EVENT_UPDATE, xtmp, y, but_width*2, but_height, PREFS['material_stencil'].val,		'Use a second texture and UV layer to blend joints'); xtmp += but_width*2;
	Blender.Draw.EndAlign()
	
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	Blender.Draw.BeginAlign()
	PREFS['do_armature'] =	Draw.Toggle('Generate Armature & Skin Mesh',	EVENT_UPDATE_AND_UI, xtmp, y, but_width*4, but_height, PREFS['do_armature'].val,	'Generate Armatuer'); xtmp += but_width*4;
	
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
	Draw.PushButton('Exit',	EVENT_EXIT, xtmp, y, but_width, but_height,	'', do_active_image); xtmp += but_width;
	Draw.PushButton('Generate from selection',	EVENT_REDRAW, xtmp, y, but_width*3, but_height,	'Generate mesh', do_tree_generate); xtmp += but_width*3;
	Blender.Draw.EndAlign()
	y-=but_height+MARGIN
	xtmp = x
	# ---------- ---------- ---------- ----------
	
	GLOBAL_PREFS['realtime_update'] =	Draw.Toggle('Automatic Update',	EVENT_UPDATE, xtmp, y, but_width*4, but_height, GLOBAL_PREFS['realtime_update'].val,	'Update automatically when settings change'); xtmp += but_width*4;
	
	

if __name__ == '__main__':
	Draw.Register(gui, evt, bevt)
