#!BPY
"""
Name: 'Self Shadow VCols (AO)...'
Blender: 245
Group: 'VertexPaint'
Tooltip: 'Generate Fake Ambient Occlusion with vertex colors.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\

Self Shadow

This usript uses the angles between faces to shade the mesh,
and optionaly blur the shading to remove artifacts from spesific edges.
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

from Blender import Scene, Draw, sys, Window, Mathutils, Mesh
import bpy
import BPyMesh


def vertexFakeAO(me, PREF_BLUR_ITERATIONS, PREF_BLUR_RADIUS, PREF_MIN_EDLEN, PREF_CLAMP_CONCAVE, PREF_CLAMP_CONVEX, PREF_SHADOW_ONLY, PREF_SEL_ONLY):
	Window.WaitCursor(1)
	DotVecs = Mathutils.DotVecs
	Ang= Mathutils.AngleBetweenVecs
	
	BPyMesh.meshCalcNormals(me)

	vert_tone= [0.0] * len(me.verts)
	vert_tone_count= [0] * len(me.verts)

	min_tone=0
	max_tone=0

	for i, f in enumerate(me.faces):
		fc= f.cent
		fno = f.no
		
		for v in f.v:
			vno=v.no # get a scaled down normal.
			
			dot= DotVecs(vno, v.co) - DotVecs(vno, fc)
			vert_tone_count[v.index]+=1
			try:
				a= Ang(vno, fno)
			except:
				continue
			
			# Convex
			if dot>0:
				a= min(PREF_CLAMP_CONVEX, a)
				if not PREF_SHADOW_ONLY:
					vert_tone[v.index] += a
			else:
				a= min(PREF_CLAMP_CONCAVE, a)
				vert_tone[v.index] -= a
	
	# average vert_tone_list into vert_tonef
	for i, tones in enumerate(vert_tone):
		if vert_tone_count[i]:
			vert_tone[i] = vert_tone[i] / vert_tone_count[i]


	# BLUR TONE
	edge_lengths= [ ed.length for ed in me.edges]
	
	for i in xrange(PREF_BLUR_ITERATIONS):
		orig_vert_tone= list(vert_tone)
		for ii, ed in enumerate(me.edges):
			i1= ed.v1.index
			i2= ed.v2.index
			l= edge_lengths[ii]
			
			f=1.0
			if l > PREF_MIN_EDLEN and l < PREF_BLUR_RADIUS:
				f= l/PREF_BLUR_RADIUS
				
				len_vert_tone_list_i1 = vert_tone_count[i1]
				len_vert_tone_list_i2 = vert_tone_count[i2]
					
				if not len_vert_tone_list_i1: len_vert_tone_list_i1=1
				if not len_vert_tone_list_i2: len_vert_tone_list_i2=1
				
				val1= (orig_vert_tone[i2]/len_vert_tone_list_i1)/ f
				val2= (orig_vert_tone[i1]/len_vert_tone_list_i2)/ f
				
				vert_tone[i1]+= val1
				vert_tone[i2]+= val2
	

	min_tone= min(vert_tone)
	max_tone= max(vert_tone)
	
	#print min_tone, max_tone
	
	tone_range= max_tone-min_tone
	if max_tone==min_tone:
		return
	
	for f in me.faces:
		if not PREF_SEL_ONLY or f.sel:
			f_col= f.col
			for i, v in enumerate(f):
				col= f_col[i]
				tone= vert_tone[v.index]
				tone= (tone-min_tone)/tone_range
				
				col.r= int(tone*col.r)
				col.g= int(tone*col.g)
				col.b= int(tone*col.b)
	
	Window.WaitCursor(0)

def main():
	sce= bpy.data.scenes.active
	ob= sce.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	PREF_BLUR_ITERATIONS= Draw.Create(1)	
	PREF_BLUR_RADIUS= Draw.Create(0.05)
	PREF_MIN_EDLEN= Draw.Create(0.01)
	PREF_CLAMP_CONCAVE= Draw.Create(90)
	PREF_CLAMP_CONVEX= Draw.Create(20)
	PREF_SHADOW_ONLY= Draw.Create(0)
	PREF_SEL_ONLY= Draw.Create(0)	
	pup_block= [\
	'Post AO Blur',\
	('  Iterations:', PREF_BLUR_ITERATIONS, 0, 40, 'Number times to blur the colors. (higher blurs more)'),\
	('  Blur Radius:', PREF_BLUR_RADIUS, 0.01, 40.0, 'How much distance effects blur transfur (higher blurs more).'),\
	('  Min EdgeLen:', PREF_MIN_EDLEN, 0.00001, 1.0, 'Minimim edge length to blur (very low values can cause errors).'),\
	'Angle Clipping',\
	('  Highlight Angle:', PREF_CLAMP_CONVEX, 0, 180, 'Less then 180 limits the angle used in the tonal range.'),\
	('  Shadow Angle:', PREF_CLAMP_CONCAVE, 0, 180, 'Less then 180 limits the angle used in the tonal range.'),\
	('Shadow Only', PREF_SHADOW_ONLY, 'Dont calculate highlights for convex areas.'),\
	('Sel Faces Only', PREF_SEL_ONLY, 'Only apply to UV/Face selected faces (mix vpain/uvface select).'),\
	]
	
	if not Draw.PupBlock('SelfShadow...', pup_block):
		return
	
	PREF_BLUR_ITERATIONS= PREF_BLUR_ITERATIONS.val
	PREF_BLUR_RADIUS= PREF_BLUR_RADIUS.val
	PREF_MIN_EDLEN= PREF_MIN_EDLEN.val
	PREF_CLAMP_CONCAVE= PREF_CLAMP_CONCAVE.val
	PREF_CLAMP_CONVEX= PREF_CLAMP_CONVEX.val
	PREF_SHADOW_ONLY= PREF_SHADOW_ONLY.val
	PREF_SEL_ONLY= PREF_SEL_ONLY.val
	
	if not me.vertexColors:
		me.vertexColors= 1
	
	t= sys.time()
	vertexFakeAO(me, PREF_BLUR_ITERATIONS, PREF_BLUR_RADIUS, PREF_MIN_EDLEN, PREF_CLAMP_CONCAVE, PREF_CLAMP_CONVEX, PREF_SHADOW_ONLY, PREF_SEL_ONLY)
	print 'done in %.6f' % (sys.time()-t)
if __name__=='__main__':
	main()

