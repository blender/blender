#!BPY
"""
Name: 'Self Shadow VCols (AO)...'
Blender: 241
Group: 'VertexPaint'
Tooltip: 'Generate Fake Ambient Occlusion with vertex colors.'
"""

__author__ = ["Campbell Barton"]
__url__ = ("blender", "elysiun", "http://members.iinet.net.au/~cpbarton/ideasman/")
__version__ = "0.1"
__bpydoc__ = """\

Clean Weight

This Script is to be used only in weight paint mode,
It removes very low weighted verts from the current group with a weight option.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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
import BPyMesh
# reload(BPyMesh)


def vertexFakeAO(me, PREF_BLUR_ITERATIONS, PREF_BLUR_SCALE, PREF_CLAMP_CONCAVE, PREF_CLAMP_CONVEX, PREF_SHADOW_ONLY, PREF_SEL_ONLY):
	Window.WaitCursor(1)
	V=Mathutils.Vector
	M=Mathutils.Matrix
	Ang= Mathutils.AngleBetweenVecs
	
	BPyMesh.meshCalcNormals(me)
		

	vert_tone= [0.0] * len(me.verts)
	vert_tone_count= [0] * len(me.verts)
	
	ed_face_users = [ [] for i in xrange(len(me.edges)) ]

	fcent= [BPyMesh.faceCent(f) for f in me.faces]

	min_tone=0
	max_tone=0

	for i, f in enumerate(me.faces):
		c= fcent[i]
		fno = f.no*0.0001
		for v in f.v:
			#vno=v.no*0.001 # get a scaled down normal.
			vno=v.no # get a scaled down normal.
			
			l1= (c-(v.co-vno)).length
			l2= (c-(v.co+vno)).length
			
			#l2= ((c+fno) - (v.co+vno)).length
			vert_tone_count[v.index]+=1
			if abs(l1-l2) < 0.0000001:
				pass
			else:
				try:
					a= Ang(vno, fno)
				except:
					continue
					
				
				# Convex
				if l1<l2:
					a= min(PREF_CLAMP_CONVEX, a)
					if not PREF_SHADOW_ONLY:
						vert_tone[v.index] += a
				else:
					a= min(PREF_CLAMP_CONCAVE, a)
					vert_tone[v.index] -= a
	
	
	# average vert_tone_list into vert_tonef
	for i, tones in enumerate(vert_tone):
		vert_tone[i] = vert_tone[i] / vert_tone_count[i]



	# BLUR TONE
	edge_lengths= [ ((ed.v1.co-ed.v2.co).length + 1) / PREF_BLUR_SCALE for ed in me.edges]
	
	for i in xrange(PREF_BLUR_ITERATIONS):
		orig_vert_tone= list(vert_tone)
		for ii, ed in enumerate(me.edges):
			i1= ed.v1.index
			i2= ed.v2.index
			l= edge_lengths[ii]
			
			len_vert_tone_list_i1 = vert_tone_count[i1]
			len_vert_tone_list_i2 = vert_tone_count[i2]
			
			if not len_vert_tone_list_i1: len_vert_tone_list_i1=1
			if not len_vert_tone_list_i2: len_vert_tone_list_i2=1
			
			vert_tone[i1]+= (orig_vert_tone[i2]/len_vert_tone_list_i1)/ l
			vert_tone[i2]+= (orig_vert_tone[i1]/len_vert_tone_list_i2)/ l
			

	min_tone= min(vert_tone)
	max_tone= max(vert_tone)
	
	tone_range= max_tone-min_tone
	if max_tone==min_tone:
		return
	SELFLAG= Mesh.FaceFlags.SELECT
	for f in me.faces:
		if not PREF_SEL_ONLY or f.flag & SELFLAG:
			for i, v in enumerate(f.v):
				tone= vert_tone[v.index]
				tone= tone-min_tone
				
				f.col[i].r= f.col[i].g= f.col[i].b= int((tone/tone_range)*255)
	
	Window.WaitCursor(0)

def main():
	scn= Scene.GetCurrent()
	ob= scn.getActiveObject()
	
	if not ob or ob.getType() != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	if not me.faceUV:
		Draw.PupMenu('Error, The active mesh does not have texface/vertex colors. aborting')
		return
	
	PREF_BLUR_ITERATIONS= Draw.Create(0)
	PREF_BLUR_SCALE= Draw.Create(1.0)	
	PREF_CLAMP_CONCAVE= Draw.Create(180)
	PREF_CLAMP_CONVEX= Draw.Create(180)
	PREF_SHADOW_ONLY= Draw.Create(0)
	PREF_SEL_ONLY= Draw.Create(0)	
	pup_block= [\
	'Post AO Blur',\
	('  Iterations:', PREF_BLUR_ITERATIONS, 1, 40, 'Number times to blur the colors. (higher blurs more)'),\
	('  Blur Radius:', PREF_BLUR_SCALE, 0.1, 10.0, 'How much distance effects blur transfur (higher blurs more).'),\
	'Angle Clipping',\
	('  Highlight Angle:', PREF_CLAMP_CONVEX, 0, 180, 'Less then 180 limits the angle used in the tonal range.'),\
	('  Shadow Angle:', PREF_CLAMP_CONCAVE, 0, 180, 'Less then 180 limits the angle used in the tonal range.'),\
	('Shadow Only', PREF_SHADOW_ONLY, 'Dont calculate highlights for convex areas.'),\
	('Sel Faces Only', PREF_SEL_ONLY, 'Only apply to UV/Face selected faces (mix vpain/uvface select).'),\
	]
	
	if not Draw.PupBlock('Clean Selected Meshes...', pup_block):
		return
	
	PREF_BLUR_ITERATIONS= PREF_BLUR_ITERATIONS.val
	PREF_BLUR_SCALE= PREF_BLUR_SCALE.val
	PREF_CLAMP_CONCAVE= PREF_CLAMP_CONCAVE.val
	PREF_CLAMP_CONVEX= PREF_CLAMP_CONVEX.val
	PREF_SHADOW_ONLY= PREF_SHADOW_ONLY.val
	PREF_SEL_ONLY= PREF_SEL_ONLY.val
	
	#t= sys.time()
	vertexFakeAO(me, PREF_BLUR_ITERATIONS, PREF_BLUR_SCALE, PREF_CLAMP_CONCAVE, PREF_CLAMP_CONVEX, PREF_SHADOW_ONLY, PREF_SEL_ONLY)
	#print 'done in %.6f' % (sys.time()-t)
if __name__=='__main__':
	main()

