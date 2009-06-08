#!BPY
"""
Name: 'Torus'
Blender: 243
Group: 'AddMesh'
"""
import BPyAddMesh
import Blender
try: from math import cos, sin, pi
except: math = None

def add_torus(PREF_MAJOR_RAD, PREF_MINOR_RAD, PREF_MAJOR_SEG, PREF_MINOR_SEG):
	Vector = Blender.Mathutils.Vector
	RotationMatrix = Blender.Mathutils.RotationMatrix
	verts = []
	faces = []
	i1 = 0
	tot_verts = PREF_MAJOR_SEG * PREF_MINOR_SEG
	for major_index in xrange(PREF_MAJOR_SEG):
		verts_tmp = []
		mtx = RotationMatrix( 360 * float(major_index)/PREF_MAJOR_SEG, 3, 'z' )
		
		for minor_index in xrange(PREF_MINOR_SEG):
			angle = 2*pi*minor_index/PREF_MINOR_SEG
			
			verts.append( Vector(PREF_MAJOR_RAD+(cos(angle)*PREF_MINOR_RAD), 0, (sin(angle)*PREF_MINOR_RAD)) * mtx )
			if minor_index+1==PREF_MINOR_SEG:
				i2 = (major_index)*PREF_MINOR_SEG
				i3 = i1 + PREF_MINOR_SEG
				i4 = i2 + PREF_MINOR_SEG
				
			else:
				i2 = i1 + 1
				i3 = i1 + PREF_MINOR_SEG
				i4 = i3 + 1
			
			if i2>=tot_verts:	i2 = i2-tot_verts
			if i3>=tot_verts:	i3 = i3-tot_verts
			if i4>=tot_verts:	i4 = i4-tot_verts
			
			faces.append( (i3,i4,i2,i1) )
			i1+=1
	
	return verts, faces

def main():
	Draw = Blender.Draw
	PREF_MAJOR_RAD = Draw.Create(1.0)
	PREF_MINOR_RAD = Draw.Create(0.25)
	PREF_MAJOR_SEG = Draw.Create(48)
	PREF_MINOR_SEG = Draw.Create(16)

	if not Draw.PupBlock('Add Torus', [\
	('Major Radius:', PREF_MAJOR_RAD,  0.01, 100, 'Radius for the main ring of the torus'),\
	('Minor Radius:', PREF_MINOR_RAD,  0.01, 100, 'Radius for the minor ring of the torus setting the thickness of the ring'),\
	('Major Segments:', PREF_MAJOR_SEG,  3, 256, 'Number of segments for the main ring of the torus'),\
	('Minor Segments:', PREF_MINOR_SEG,  3, 256, 'Number of segments for the minor ring of the torus'),\
	]):
		return
	
	verts, faces = add_torus(PREF_MAJOR_RAD.val, PREF_MINOR_RAD.val, PREF_MAJOR_SEG.val, PREF_MINOR_SEG.val)
	
	BPyAddMesh.add_mesh_simple('Torus', verts, [], faces)

if cos and sin and pi:
    main()
else:
    Blender.Draw.PupMenu("Error%t|This script requires a full python installation")

