#!BPY
"""
Name: 'Solidify Selection'
Blender: 240
Group: 'Mesh'
Tooltip: 'Makes the mesh solid by creating a second skin.'
"""

__author__ = "Campbell Barton"
__url__ = ("www.blender.org", "blenderartists.org")
__version__ = "1.0"

__bpydoc__ = """\
This script makes a skin from the selected faces.
Optionaly you can skin between the original and new faces to make a watertight solid object
"""


from Blender import *
import BPyMesh
Ang= Mathutils.AngleBetweenVecs
SMALL_NUM=0.00001


# returns a length from an angle
# Imaging a 2d space.
# there is a hoz line at Y1 going to inf on both X ends, never moves (LINEA)
# down at Y0 is a unit length line point up at (angle) from X0,Y0 (LINEB)
# This function returns the length of LINEB at the point it would intersect LINEA
# - Use this for working out how long to make the vector - differencing it from surrounding faces,
import math

def lengthFromAngle(angle):
	if angle < SMALL_NUM:
		return 1.0
	angle = 2*math.pi*angle/360
	x,y = math.cos(angle), math.sin(angle)
	# print "YX", x,y
	# 0 d is hoz to the right.
	# 90d is vert upward.
	fac=1/x
	x=x*fac
	y=y*fac
	return math.sqrt((x*x)+(y*y))


def main():
	scn = Scene.GetCurrent()
	ob = scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('ERROR: Active object is not a mesh, aborting.')
		return
	
	# Create the variables.
	PREF_THICK = Draw.Create(-0.1)
	PREF_SKIN_SIDES= Draw.Create(1)
	PREF_REM_ORIG= Draw.Create(0)
	
	pup_block = [\
	('Thick:', PREF_THICK, -10, 10, 'Skin thickness in mesh space.'),\
	('Skin Sides', PREF_SKIN_SIDES, 'Skin between the original and new faces.'),\
	('Remove Original', PREF_REM_ORIG, 'Remove the selected faces after skinning.'),\
	]
	
	if not Draw.PupBlock('Solid Skin Selection', pup_block):
		return
	
	PREF_THICK= PREF_THICK.val
	PREF_SKIN_SIDES= PREF_SKIN_SIDES.val
	PREF_REM_ORIG= PREF_REM_ORIG.val
	
	Window.WaitCursor(1)
	
	is_editmode = Window.EditMode() 
	if is_editmode: Window.EditMode(0)
	
	# Main code function	
	me = ob.getData(mesh=True)
	me_faces = me.faces
	faces_sel= [f for f in me_faces if f.sel]
	
	
	BPyMesh.meshCalcNormals(me)
	normals= [v.no for v in me.verts]
	vertFaces= [[] for i in xrange(len(me.verts))]
	for f in me_faces:
		no=f.no
		for v in f:
			vertFaces[v.index].append(no)
	
	# Scale the normals by the face angles from the vertex Normals.
	for i in xrange(len(me.verts)):
		length=0.0
		if vertFaces[i]:
			for fno in vertFaces[i]:
				try:
					a= Ang(fno, normals[i])
				except:
					a= 0	
				if a>=90:
					length+=1
				elif a < SMALL_NUM:
					length+= 1
				else:
					length+= lengthFromAngle(a)
			
			length= length/len(vertFaces[i])
			#print 'LENGTH %.6f' % length
			normals[i]= (normals[i] * length) * PREF_THICK
	
	len_verts = len( me.verts )
	len_faces = len( me_faces )
	
	vert_mapping= [-1] * len(me.verts)
	verts= []
	for f in faces_sel:
		for v in f:
			i= v.index
			if vert_mapping[i]==-1:
				vert_mapping[i]= len_verts + len(verts)
				verts.append(v.co + normals[i])
	
	#verts= [v.co + normals[v.index] for v in me.verts]
	
	me.verts.extend( verts )
	#faces= [tuple([ me.verts[v.index+len_verts] for v in reversed(f.v)]) for f in me_faces ]
	faces= [ tuple([vert_mapping[v.index] for v in reversed(f.v)]) for f in faces_sel ]
	me_faces.extend( faces )
	
	has_uv = me.faceUV
	has_vcol = me.vertexColors
	
	for i, orig_f in enumerate(faces_sel):
		new_f= me_faces[len_faces + i]
		new_f.mat = orig_f.mat
		new_f.smooth = orig_f.smooth
		orig_f.sel=False
		new_f.sel= True
		new_f = me_faces[i+len_faces]
		if has_uv:
			new_f.uv = [c for c in reversed(orig_f.uv)]
			new_f.mode = orig_f.mode
			new_f.flag = orig_f.flag
			if orig_f.image:
				new_f.image = orig_f.image
		if has_vcol:
			new_f.col = [c for c in reversed(orig_f.col)]
	# Now add quads between if we wants
	
	if PREF_SKIN_SIDES:
		skin_side_faces= []
		skin_side_faces_orig= []
		# Get edges of faces that only have 1 user - so we can make walls
		edges = {}
		for f in faces_sel:
			f_v= f.v
			for i, edgekey in enumerate(f.edge_keys):
				if edges.has_key(edgekey):
					edges[edgekey]= None
				else:
					edges[edgekey] = f, f_v, i, i-1
		
		# Edges are done. extrude the single user edges.
		for edge_face_data in edges.itervalues():
			if edge_face_data: # != None
				f, f_v, i1, i2 = edge_face_data
				v1i,v2i= f_v[i1].index, f_v[i2].index
				# Now make a new Face
				skin_side_faces.append( (v1i, v2i, vert_mapping[v2i], vert_mapping[v1i]) )
				skin_side_faces_orig.append((f, len(me_faces) + len(skin_side_faces_orig), i1, i2))
		
		me_faces.extend(skin_side_faces)
		
		
		
		# Now assign properties.
		for i, origfData in enumerate(skin_side_faces_orig):
			orig_f, new_f_idx, i1, i2 = origfData
			new_f= me_faces[new_f_idx]
			
			new_f.mat= orig_f.mat
			new_f.smooth= orig_f.smooth
			if has_uv:
				new_f.mode= orig_f.mode
				new_f.flag= orig_f.flag
				if orig_f.image:
					new_f.image= orig_f.image
				
				uv1= orig_f.uv[i1]
				uv2= orig_f.uv[i2]
				new_f.uv= (uv1, uv2, uv2, uv1)
			
			if has_vcol:
				col1= orig_f.col[i1]
				col2= orig_f.col[i2]
				new_f.col= (col1, col2, col2, col1)
	
	if PREF_REM_ORIG:
		me_faces.delete(0, faces_sel)
	
	
	Window.WaitCursor(0)
	if is_editmode:	Window.EditMode(1)
	
	Window.RedrawAll()

if __name__ == '__main__':
	main()