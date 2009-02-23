#!BPY
"""
Name: 'Copy from Material...'
Blender: 242
Group: 'VertexPaint'
Tooltip: 'Writes material diffuse color as vertex colors.'
"""

__author__ = "Campbell Barton"
__url__ = ("www.blender.org", "blenderartists.org")
__version__ = "1.0"

__bpydoc__ = """\
This script copies material colors to vertex colors.
Optionaly you can operate on all faces and many objects as well as multiplying with the current color.
"""


from Blender import *

def matcol(mat):
	'''
	Returns the material color as a tuple 3 from 0 to 255
	'''
	if mat:
		return \
		int(mat.R*255),\
		int(mat.G*255),\
		int(mat.B*255)
	else:
		return None

def mat2vcol(PREF_SEL_FACES_ONLY, PREF_ACTOB_ONLY, PREF_MULTIPLY_COLOR):
	scn= Scene.GetCurrent()
	if PREF_ACTOB_ONLY:
		obs= [scn.getActiveObject()]
	else:
		obs= Object.GetSelected()
		ob= scn.getActiveObject()
		if ob not in obs:
			obs.append(ob)
	
	
	for ob in obs:
		if ob.type != 'Mesh':
			continue
		
		me= ob.getData(mesh=1)
		
		try:
			me.vertexColors=True
		except: # no faces
			continue
		
		matcols= [matcol(mat) for mat in me.materials]
		len_matcols= len(matcols)
		
		for f in me.faces:
			if not PREF_SEL_FACES_ONLY or f.sel:
				f_mat= f.mat
				if f_mat < len_matcols:
					mat= matcols[f.mat]
					if mat:
						if PREF_MULTIPLY_COLOR:
							for c in f.col:
								c.r= int(((c.r/255.0) * (mat[0]/255.0)) * 255.0)
								c.g= int(((c.g/255.0) * (mat[1]/255.0)) * 255.0)
								c.b= int(((c.b/255.0) * (mat[2]/255.0)) * 255.0)
						else:
							for c in f.col:
								c.r=mat[0]
								c.g=mat[1]
								c.b=mat[2]
		me.update()

def main():
	# Create the variables.
	
	PREF_SEL_FACES_ONLY= Draw.Create(1)
	PREF_ACTOB_ONLY= Draw.Create(1)
	PREF_MULTIPLY_COLOR = Draw.Create(0)
	
	pup_block = [\
	('Sel Faces Only', PREF_SEL_FACES_ONLY, 'Only apply to selected faces.'),\
	('Active Only', PREF_ACTOB_ONLY, 'Operate on all selected objects.'),\
	('Multiply Existing', PREF_MULTIPLY_COLOR, 'Multiplies material color with existing.'),\
	]
	
	if not Draw.PupBlock('VCols from Material', pup_block):
		return
	
	PREF_SEL_FACES_ONLY= PREF_SEL_FACES_ONLY.val
	PREF_ACTOB_ONLY= PREF_ACTOB_ONLY.val
	PREF_MULTIPLY_COLOR= PREF_MULTIPLY_COLOR.val

	mat2vcol(PREF_SEL_FACES_ONLY, PREF_ACTOB_ONLY, PREF_MULTIPLY_COLOR)

if __name__=='__main__':
	main()