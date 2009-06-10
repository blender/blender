#!BPY
"""
Name: 'Cookie Cut from View'
Blender: 234
Group: 'Object'
Tooltip: 'Cut from the view axis, (Sel 3d Curves and Meshes (only edges) into other meshes with faces)'
""" 
__author__= "Campbell Barton"
__url__= ["blender", "blenderartist"]
__version__= "1.0"

__bpydoc__= """\
This script takes the selected mesh objects, divides them into 2 groups
Cutters and The objects to be cut.

Cutters are meshes with no faces, just edge loops. and any meshes with faces will be cut.

Usage:

Select 2 or more meshes, one with no faces (a closed polyline) and one with faces to cut.

Align the view on the axis you want to cut.
For shapes that have overlapping faces (from the view), hide any backfacing faces so they will be ignored during the cut.
Run the script.

You can choose to make the cut verts lie on the face that they were cut from or on the edge that cut them.
This script supports UV coordinates and images.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell Barton
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

import Blender
from math import sqrt
import BPyMesh
Vector= Blender.Mathutils.Vector
LineIntersect2D= Blender.Geometry.LineIntersect2D
PointInTriangle2D= Blender.Geometry.PointInTriangle2D

# Auto class
def auto_class(slots):
	exec('class container_class(object): __slots__=%s' % slots)
	return container_class


bignum= 1<<30
def bounds_xy(iter_item):
	'''
	Works with types
	MMesh.verts
	MFace
	MEdge
	'''
	xmin= ymin=  bignum
	xmax= ymax= -bignum
	for v in iter_item:
		x= v.co.x
		y= v.co.y
		if x<xmin: xmin= x
		if y<ymin: ymin= y
		if x>xmax: xmax= x
		if y>ymax: ymax= y
	
	return xmin, ymin, xmax, ymax
	
def bounds_intersect(a,b):
	'''
	each tuple is
	xmin, ymin, xmax, ymax
	'''
	if\
	a[0]>b[2] or\
	a[1]>b[3] or\
	a[2]<b[0] or\
	a[3]<b[1]:
		return False
	else:
		return True

def point_in_bounds(pt, bounds):
	'''
	each tuple is
	xmin, ymin, xmax, ymax
	'''	
	if bounds[0] < pt.x < bounds[2] and bounds[1] < pt.y < bounds[3]:
		return True
	else:
		return False

def point_in_poly2d(pt, fvco):
	if PointInTriangle2D(pt, fvco[0], fvco[1], fvco[2]):
		return True
	if len(fvco) == 4:
		if PointInTriangle2D(pt, fvco[0], fvco[2], fvco[3]):
			return True
	return False

# reuse me more.
def sorted_edge_indicies(ed):
	i1= ed.v1.index
	i2= ed.v2.index
	if i1>i2:
		i1,i2= i2,i1
	return i1, i2

def sorted_indicies(i1, i2):
	if i1>i2:
		i1,i2= i2,i1
	return i1, i2

def fake_length2d(pt1, pt2):
	'''
	Only used for comparison so don't sqrt
	'''
	#return math.sqrt(abs(pow(x1-x2, 2)+ pow(y1-y2, 2)))
	return pow(pt1[0]-pt2[0], 2) + pow(pt1[1]- pt2[1], 2)

def length2d(pt1, pt2):
	'''
	Only used for comparison so don't sqrt
	'''
	#return math.sqrt(abs(pow(x1-x2, 2)+ pow(y1-y2, 2)))
	return sqrt(pow(pt1[0]-pt2[0], 2) + pow(pt1[1]- pt2[1], 2))



def tri_area_2d(v1, v2, v3):
	e1 = length2d(v1, v2)
	e2 = length2d(v2, v3)
	e3 = length2d(v3, v1)
	p = e1+e2+e3
	return 0.25 * sqrt(abs(p*(p-2*e1)*(p-2*e2)*(p-2*e3)))

def tri_pt_find_z_2d(pt, tri):
	""" Takes a face and 3d vector and assigns the vectors Z to its on the face"""
	
	l1= tri_area_2d(tri[1], tri[2], pt)
	l2= tri_area_2d(tri[0], tri[2], pt)
	l3= tri_area_2d(tri[0], tri[1], pt)
	
	tot= l1+l2+l3
	# Normalize
	l1=l1/tot
	l2=l2/tot
	l3=l3/tot
	
	z1= tri[0].z*l1
	z2= tri[1].z*l2
	z3= tri[2].z*l3
	
	return z1+z2+z3


def tri_pt_find_uv_2d(pt, tri, uvs):
	""" Takes a face and 3d vector and assigns the vectors Z to its on the face"""
	
	l1= tri_area_2d(tri[1], tri[2], pt)
	l2= tri_area_2d(tri[0], tri[2], pt)
	l3= tri_area_2d(tri[0], tri[1], pt)
	
	tot= l1+l2+l3
	if not tot: # No area, just return the first uv
		return Vector(uvs[0])
	
	# Normalize
	l1=l1/tot
	l2=l2/tot
	l3=l3/tot
	
	uv1= uvs[0]*l1
	uv2= uvs[1]*l2
	uv3= uvs[2]*l3
	
	return uv1+uv2+uv3




def mesh_edge_dict(me):
	ed_dict= {}
	for f in me.faces:
		if not f.hide:
			for edkey in f.edge_keys:
				ed_dict.setdefault(edkey, []).append(f)
	
	return ed_dict



def terrain_cut_2d(t, c, PREF_Z_LOC):
	'''
	t is the terrain
	c is the cutter
	
	PREF_Z_LOC:		0 - from terrain face
					1 - from cutter edge
					
	returns nothing
	'''
	
	# do we have a 2d intersection
	if not bounds_intersect(t.bounds, c.bounds):
		return
	
	# Local vars
	me_t= t.mesh
	me_c= c.mesh
	
	has_uv= me_t.faceUV

	Blender.Mesh.Mode(Blender.Mesh.SelectModes['VERTEX'])
	'''
	first assign a face terrain face for each cutter verticie
	'''
	cut_verts_temp= list(me_c.verts)
	cut_vert_terrain_faces= [None] * len(me_c.verts)
	vert_z_level= [-10.0] * len(me_c.verts)
	
	for v in me_c.verts:
		v_index= v.index
		v_co= v.co
		for fidx, f in enumerate(me_t.faces):
			if not f.hide:
				if point_in_bounds(v_co, t.face_bounds[fidx]):
					f_v= [vv.co for vv in f]
					if point_in_poly2d(v_co, f_v):
						
						
						if PREF_Z_LOC==0:
							'''
							Get the z location from the face.
							'''
							
							if len(f_v)==3:
								vert_z_level[v_index]= tri_pt_find_z_2d(v_co, (f_v[0], f_v[1], f_v[2]) )
							else:
								# Quad, which side are we on?
								a1= tri_area_2d(f_v[0], f_v[1], v_co)
								a2= tri_area_2d(f_v[1], f_v[2], v_co)
								
								a3= tri_area_2d(f_v[0], f_v[1], f_v[2])
								
								if a1+a2<a3:
									vert_z_level[v_index]= tri_pt_find_z_2d(v_co, (f_v[0], f_v[1], f_v[2]) )
								else:
									vert_z_level[v_index]= tri_pt_find_z_2d(v_co, (f_v[0], f_v[2], f_v[3]) )
							
						else: # PREF_Z_LOC==1
							'''
							Get the z location from the vert
							'''
							vert_z_level[v_index]= v_co.z
						
						# Non overlapping faces in terrain mean we can break
						cut_vert_terrain_faces[v_index]= f
						break
	
	del cut_verts_temp
	
	
	
	edge_intersections= []
	edge_isect_type= auto_class(['point', 'ed_terrain', 'ed_cut'])
	
	# intersect cutter faces with terrain edges.
	for ei_t, ed_t in enumerate(me_t.edges):
		
		eb_t= t.edge_bounds[ei_t]
		if bounds_intersect(eb_t, c.bounds): # face/cutter bounds intersect?
			# Loop through the cutter edges.
			for ei_c, ed_c in enumerate(me_c.edges):
				# If the cutter edge has 2 verts inside the same face then we can ignore it
				# Both are different faces or None
				if cut_vert_terrain_faces[ed_c.v1.index] != cut_vert_terrain_faces[ed_c.v2.index] or\
				cut_vert_terrain_faces[ed_c.v1.index] == cut_vert_terrain_faces[ed_c.v2.index] == None:
					eb_c= c.edge_bounds[ei_c]
					if bounds_intersect(eb_t, eb_c): # face/edge bounds intersect?
						# Now we know the 2 edges might intersect, we'll do a proper test
						
						x= LineIntersect2D(ed_t.v1.co, ed_t.v2.co,   ed_c.v1.co, ed_c.v2.co)					
						if x:
							ed_isect= edge_isect_type()
							ed_isect.point= x.resize3D() # fake 3d
							
							# Find the interpolation Z point
							
							if PREF_Z_LOC==0:	
								'''
								Terrains edge
								'''
								l1= length2d(ed_isect.point, ed_t.v1.co)
								l2= length2d(ed_isect.point, ed_t.v2.co)
								ed_isect.point.z= ((l2*ed_t.v1.co.z) + (l1*ed_t.v2.co.z)) / (l1+l2)
							else:
								'''
								Cutters edge
								'''
								l1= length2d(ed_isect.point, ed_c.v1.co)
								l2= length2d(ed_isect.point, ed_c.v2.co)
								ed_isect.point.z= ((l2*ed_c.v1.co.z) + (l1*ed_c.v2.co.z)) / (l1+l2)
							
							ed_isect.ed_terrain= ed_t
							ed_isect.ed_cut= ed_c
							
							edge_intersections.append(ed_isect)
	
	if not edge_intersections:
		return

	# Now we have collected intersections we need to apply them
	
	# Find faces that have intersections, these faces will need to be cut.
	faces_intersecting= {} # face index as key, list of edges as values
	for ed_isect in edge_intersections:
		
		try:
			faces= t.edge_dict[ sorted_edge_indicies(ed_isect.ed_terrain) ]
		except:
			# If the faces are hidden then the faces wont exist.
			faces= []
		
		for f in faces:
			faces_intersecting.setdefault(f.index, []).append(ed_isect)
	
	# this list is used to store edges that are totally inside a face ( no intersections with terrain)
	# we can remove these as we
	face_containing_edges= [[] for i in xrange(len(me_t.faces))]
	for ed_c in me_c.edges:
		if cut_vert_terrain_faces[ed_c.v1.index]==cut_vert_terrain_faces[ed_c.v2.index] != None:
			# were inside a face.
			face_containing_edges[cut_vert_terrain_faces[ed_c.v1.index].index].append(ed_c)
			
	# New Mesh for filling faces only
	new_me= Blender.Mesh.New()
	scn= Blender.Scene.GetCurrent()
	ob= Blender.Object.New('Mesh')
	ob.link(new_me)
	scn.link(ob)
	ob.sel= True
	
	new_faces= []
	new_faces_props= []
	new_uvs= []
	new_verts= []
	
	# Loop through inter
	for fidx_t, isect_edges in faces_intersecting.iteritems():
		f= me_t.faces[fidx_t]
		f_v= f.v
		fidxs_s= [v.index for v in f_v]
		
		# Make new fake edges for each edge, each starts as a list of 2 verts, but more verts can be added
		# This list will then be sorted so the edges are in order from v1 to v2 of the edge.
		face_new_verts= [ (f_v[i], [], f_v[i-1]) for i in xrange(len(f_v)) ]
		# if len(face_new_verts)  < 3: raise 'weirdo'
		
		face_edge_dict = dict( [(sorted_indicies(fidxs_s[i], fidxs_s[i-1]), i) for i in xrange(len(f_v))] )
		
		for ed_isect in isect_edges:
			edge_index_in_face = face_edge_dict[ sorted_edge_indicies(ed_isect.ed_terrain) ]
			# Add this intersection to the face
			face_new_verts[edge_index_in_face][1].append(ed_isect)
		
		# Now sort the intersections
		for new_edge in face_new_verts:
			if len(new_edge[1]) > 1:
				# We have 2+ verts to sort
				edv1= tuple(new_edge[0].co) # 3d but well only use the 2d part
				new_edge[1].sort(lambda a,b: cmp(fake_length2d(a.point, edv1), fake_length2d(b.point, edv1) ))
		
		# now build up a new face by getting edges
		random_face_edges= []
		unique_verts= [] # store vert
		rem_double_edges= {}
		
		def add_edge(p1, p2):
			k1= tuple(p1)
			k2= tuple(p2)
			
			# Adds new verts where needed
			try:
				i1= rem_double_edges[k1]
			except:
				i1= rem_double_edges[k1]= len(rem_double_edges)
				unique_verts.append(k1)
			
			try:
				i2= rem_double_edges[k2]
			except:
				i2= rem_double_edges[k2]= len(rem_double_edges)
				unique_verts.append(k2)
				
			random_face_edges.append( (i1, i2) )
		
		
		
		# edges that don't have a vert in the face have to span between to intersection points
		# since we don't know the other point at any 1 time we need to remember edges that 
		# span a face and add them once we'v collected both
		# first add outline edges
		edge_span_face= {}
		for new_edge in face_new_verts:
			new_edge_subdiv= len(new_edge[1])
			if new_edge_subdiv==0:
				# no subdiv edges, just add
				add_edge(new_edge[0].co, new_edge[2].co)
			elif new_edge_subdiv==1:
				add_edge(new_edge[0].co, new_edge[1][0].point)
				add_edge(new_edge[1][0].point, new_edge[2].co)
			else:
				# 2 or more edges
				add_edge(new_edge[0].co, new_edge[1][0].point)
				add_edge(new_edge[1][-1].point, new_edge[2].co)
				
				# now add multiple
				for i in xrange(new_edge_subdiv-1):
					add_edge(new_edge[1][i].point, new_edge[1][i+1].point)
				
			# done adding outline
			# while looping through the edge subdivs, add the edges that intersect
			
			
			for ed_isect in new_edge[1]:
				ed_cut= ed_isect.ed_cut
				if cut_vert_terrain_faces[ed_cut.v1.index]==f:
					# our first vert is inside the face
					point= Vector(ed_cut.v1.co)
					point.z= vert_z_level[ed_cut.v1.index]
					
					add_edge(point, ed_isect.point)
				elif cut_vert_terrain_faces[ed_cut.v2.index]==f:
					# assume second vert is inside the face
					point= Vector(ed_cut.v2.co)
					point.z= vert_z_level[ed_cut.v2.index]
					add_edge(point, ed_isect.point)
				else:
					# this edge has no verts in the face so it will need to be clipped in 2 places
					try:
						point= edge_span_face[ed_cut]
						
						# if were here it worked ;)
						add_edge(point, ed_isect.point)
						
					except:
						# add the first intersecting point
						edge_span_face[ed_cut]= ed_isect.point
					
			# now add all edges that are inside the the face
			for ed_c in face_containing_edges[fidx_t]:
				point1= Vector(ed_c.v1.co)
				point2= Vector(ed_c.v2.co)
				point1.z= vert_z_level[ed_c.v1.index]
				point2.z= vert_z_level[ed_c.v2.index]
				add_edge(point1, point2)
		
		new_me.verts.extend(unique_verts)
		new_me.edges.extend(random_face_edges)
		new_me.sel= 1
		
		# backup the z values, fill and restore
		
		backup_z= [v.co.z for v in new_me.verts]
		for v in new_me.verts: v.co.z= 0
		#raise 'as'
		new_me.fill()
		for i, v in enumerate(new_me.verts): v.co.z= backup_z[i]
		
		
		# ASSIGN UV's
		if has_uv:
			f_uv= f_uv_mod= f.uv
			f_vco= f_vco_mod= [v.co for v in f]
			
			# f is the face, get the uv's from that.
			
			uvs= [None] * len(new_me.verts)
			for i, v in enumerate(new_me.verts):
				v_co= v.co
				f_uv_mod= f_uv
				f_vco_mod= f_vco
				
				if len(f_v)==4:
					# Quad, which side are we on?
					a1= tri_area_2d(f_vco[0], f_vco[1], v_co)
					a2= tri_area_2d(f_vco[1], f_vco[2], v_co)
					
					a3= tri_area_2d(f_vco[0], f_vco[1], f_vco[2])
					if a1+a2 > a3:
						# 0,2,3
						f_uv_mod= f_uv[0], f_uv[2], f_uv[3]
						f_vco_mod= f_vco[0], f_vco[2], f_vco[3]
					# else - side of 0,1,2 - don't modify the quad
				
				uvs[i]= tri_pt_find_uv_2d(v_co, f_vco_mod, f_uv_mod)	
			
			new_uvs.extend(uvs)
			new_faces_props.extend( [f.image] * len(new_me.faces) )
			
		# collect the fill results
		new_verts_len= len(new_verts) + len(me_t.verts)
		new_faces.extend( [[v.index+new_verts_len for v in ff] for ff in new_me.faces] )
		
		
		
		new_verts.extend(unique_verts)
		
		new_me.verts= None
		#raise 'error'
	
	# Finished filling
	scn.unlink(ob)
	
	
	# Remove faces
	face_len = len(me_t.faces)
	verts_len = len(me_t.verts)
	me_t.verts.extend(new_verts)
	me_t.faces.extend(new_faces)
	
	for i in xrange(len(new_faces)):
		f= me_t.faces[face_len+i]
		
		if has_uv:
			img= new_faces_props[i]
			if img: f.image= img
			
			f_uv= f.uv
			for ii, v in enumerate(f):
				v_index= v.index-verts_len
				new_uv= new_uvs[v_index]
				uv= f_uv[ii]
				uv.x= new_uv.x
				uv.y= new_uv.y
	
	me_t.faces.delete(1, faces_intersecting.keys())
	me_t.sel= 1
	me_t.remDoubles(0.0000001)
	
	
def main():
	PREF_Z_LOC= Blender.Draw.PupMenu('Cut Z Location%t|Original Faces|Cutting Polyline')
	
	if PREF_Z_LOC==-1:
		return
	PREF_Z_LOC-=1
	
	Blender.Window.WaitCursor(1)
	
	print '\nRunning Cookie Cutter'
	time= Blender.sys.time()
	scn = Blender.Scene.GetCurrent()
	obs= [ob for ob in scn.objects.context if ob.type in ('Mesh', 'Curve')]
	MULTIRES_ERROR = False
	
	# Divide into 2 lists- 1 with faces, one with only edges
	terrains=	[] #[me for me in mes if me.faces]
	cutters=	[] #[me for me in mes if not me.faces]
	
	terrain_type= auto_class(['mesh', 'bounds', 'face_bounds', 'edge_bounds', 'edge_dict', 'cutters', 'matrix'])
	
	for ob in obs:
		if ob.type == 'Mesh':
			me= ob.getData(mesh=1)
		elif ob.data.flag & 1: # Is the curve 3D? else don't use.
			me= BPyMesh.getMeshFromObject(ob) # get the curve
		else:
			continue
			
		# a new terrain instance
		if me.multires:
			MULTIRES_ERROR = True		
		else:
			t= terrain_type()
			
			t.matrix= ob.matrixWorld * Blender.Window.GetViewMatrix()
			
			# Transform the object by its matrix
			me.transform(t.matrix)
			
			# Set the terrain bounds
			t.bounds=		bounds_xy(me.verts)
			t.edge_bounds= 	[bounds_xy(ed) for ed in me.edges]
			t.mesh=			me
			
			if me.faces: # Terrain.
				t.edge_dict=					mesh_edge_dict(me)
				t.face_bounds=					[bounds_xy(f) for f in me.faces]
				t.cutters= 						[] # Store cutting objects that cut us here
				terrains.append(t)
			elif len(me.edges)>2: # Cutter
				cutters.append(t)
	
	totcuts= len(terrains)*len(cutters)
	if not totcuts:
		Blender.Window.WaitCursor(0)
		Blender.Draw.PupMenu('ERROR%t|Select at least 1 closed loop mesh (edges only)|as the cutter...|and 1 or more meshes to cut into')
	
	crazy_point= Vector(100000, 100000)
	
	for t in terrains:
		for c in cutters:
			# Main curring function
			terrain_cut_2d(t, c, PREF_Z_LOC)
			
			# Was the terrain touched?
			if len(t.face_bounds) != len(t.mesh.faces):
				t.edge_dict=					mesh_edge_dict(t.mesh)
				# remake the bounds
				t.edge_bounds=					[bounds_xy(ed) for ed in t.mesh.edges]
				t.face_bounds=					[bounds_xy(f) for f in t.mesh.faces]
				t.cutters.append(c)
			
			print '\t%i remaining' % totcuts
			totcuts-=1
		
		# SELECT INTERNAL FACES ONCE THIS TERRAIN IS CUT
		Blender.Mesh.Mode(Blender.Mesh.SelectModes['FACE'])
		t.mesh.sel= 0
		for c in t.cutters:
			edge_verts_c= [(ed_c.v1.co, ed_c.v2.co) for ed_c in c.mesh.edges]
			for f in t.mesh.faces:
				# How many edges do we intersect on our way to the faces center
				if not f.hide and not f.sel: # Not alredy selected
					c= f.cent
					if point_in_bounds(c, t.bounds):
						isect_count= 0
						for edv1, edv2 in edge_verts_c:
							isect_count += (LineIntersect2D(c, crazy_point,  edv1, edv2) != None)
						
						if isect_count%2:
							f.sel= 1
	Blender.Mesh.Mode(Blender.Mesh.SelectModes['FACE'])
	
	# Restore the transformation
	for data in (terrains, cutters):
		for t in data:
			if t.mesh.users: # it may have been a temp mesh from a curve.
				t.mesh.transform(t.matrix.copy().invert())
	
	Blender.Window.WaitCursor(0)
	
	if MULTIRES_ERROR:
		Blender.Draw.PupMenu('Error%t|One or more meshes meshes not cut because they are multires.')
	
	print 'terrains:%i cutters %i  %.2f secs taken' % (len(terrains), len(cutters), Blender.sys.time()-time)


if __name__=='__main__':
	main()
