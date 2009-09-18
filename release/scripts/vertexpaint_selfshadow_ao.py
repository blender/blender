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


def vertexFakeAO(me, PREF_BLUR_ITERATIONS, PREF_BLUR_STRENGTH, PREF_CLAMP_CONCAVE, PREF_CLAMP_CONVEX, PREF_SHADOW_ONLY, PREF_SEL_ONLY):
	Window.WaitCursor(1)
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
			
			dot= vno.dot(v.co) - vno.dot(fc)
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

	
	# Below we use edges to blur along so the edges need counting, not the faces
	vert_tone_count=	[0] *	len(me.verts)
	for ed in me.edges:
		vert_tone_count[ed.v1.index] += 1
		vert_tone_count[ed.v2.index] += 1


	# Blur tone
	blur		= PREF_BLUR_STRENGTH
	blur_inv	= 1.0 - PREF_BLUR_STRENGTH
	
	for i in xrange(PREF_BLUR_ITERATIONS):
		
		# backup the original tones
		orig_vert_tone= list(vert_tone)
		
		for ed in me.edges:
			
			i1= ed.v1.index
			i2= ed.v2.index
		
			val1= (orig_vert_tone[i2]*blur) +  (orig_vert_tone[i1]*blur_inv)
			val2= (orig_vert_tone[i1]*blur) +  (orig_vert_tone[i2]*blur_inv)
			
			# Apply the ton divided by the number of faces connected
			vert_tone[i1]+= val1 / max(vert_tone_count[i1], 1)
			vert_tone[i2]+= val2 / max(vert_tone_count[i2], 1)
	

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
	PREF_BLUR_STRENGTH= Draw.Create(0.5)
	PREF_CLAMP_CONCAVE= Draw.Create(90)
	PREF_CLAMP_CONVEX= Draw.Create(20)
	PREF_SHADOW_ONLY= Draw.Create(0)
	PREF_SEL_ONLY= Draw.Create(0)	
	pup_block= [\
	'Post AO Blur',\
	('Strength:', PREF_BLUR_STRENGTH, 0, 1, 'Blur strength per iteration'),\
	('Iterations:', PREF_BLUR_ITERATIONS, 0, 40, 'Number times to blur the colors. (higher blurs more)'),\
	'Angle Clipping',\
	('Highlight Angle:', PREF_CLAMP_CONVEX, 0, 180, 'Less then 180 limits the angle used in the tonal range.'),\
	('Shadow Angle:', PREF_CLAMP_CONCAVE, 0, 180, 'Less then 180 limits the angle used in the tonal range.'),\
	('Shadow Only', PREF_SHADOW_ONLY, 'Dont calculate highlights for convex areas.'),\
	('Sel Faces Only', PREF_SEL_ONLY, 'Only apply to UV/Face selected faces (mix vpain/uvface select).'),\
	]
	
	if not Draw.PupBlock('SelfShadow...', pup_block):
		return
	
	if not me.vertexColors:
		me.vertexColors= 1
	
	t= sys.time()
	vertexFakeAO(me,	PREF_BLUR_ITERATIONS.val, \
						PREF_BLUR_STRENGTH.val, \
						PREF_CLAMP_CONCAVE.val, \
						PREF_CLAMP_CONVEX.val, \
						PREF_SHADOW_ONLY.val, \
						PREF_SEL_ONLY.val)
	
	if ob.modifiers:
		me.update()
	
	print 'done in %.6f' % (sys.time()-t)
if __name__=='__main__':
	main()

