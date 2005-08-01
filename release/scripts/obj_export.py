#!BPY

"""
Name: 'Wavefront (.obj)...'
Blender: 232
Group: 'Export'
Tooltip: 'Save a Wavefront OBJ File'
"""

__author__ = "Campbell Barton, Jiri Hnidek"
__url__ = ["blender", "elysiun"]
__version__ = "0.9"

__bpydoc__ = """\
This script is an exporter to OBJ file format.

Usage:

Run this script from "File->Export" menu to export all meshes.
"""


# --------------------------------------------------------------------------
# OBJ Export v0.9b by Campbell Barton (AKA Ideasman)
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

#==================================================#
# New name based on old with a different extension #
#==================================================#
def newFName(ext):
	return Get('filename')[: -len(Get('filename').split('.', -1)[-1]) ] + ext


from Blender import *

NULL_MAT = '(null)'
NULL_IMG = '(null)' # from docs at http://astronomy.swin.edu.au/~pbourke/geomformats/obj/ also could be 'off'

def save_mtl(filename):
	file = open(filename, "w")
	file.write('# Blender MTL File: %s\n' % (Get('filename')))
	for mat in Material.Get():
		file.write('newmtl %s\n' % (mat.getName())) # Define a new material
		file.write('Ns %s\n' % ((mat.getHardness()-1) * 1.9607843137254901 ) ) # Hardness, convert blenders 1-511 to MTL's 
		file.write('Kd %.6f %.6f %.6f\n' % tuple(mat.getRGBCol())) # Diffuse
		file.write('Ka %.6f %.6f %.6f\n' % tuple(mat.getMirCol())) # Ambient, uses mirror colour,
		file.write('Ks %.6f %.6f %.6f\n' % tuple(mat.getSpecCol())) # Specular
		file.write('Ni %.6f\n' % mat.getIOR()) # Refraction index
		file.write('d %.6f\n' % mat.getAlpha()) # Alpha (obj uses 'd' for dissolve)
		
		# illum, 0 to disable lightng, 2 is normal.
		if mat.getMode() & Material.Modes['SHADELESS']:
			file.write('illum 0\n\n') # ignore lighting
		else:
			file.write('illum 2\n\n') # light normaly		
	file.close()

def save_obj(filename):
	time1 = sys.time()
	scn = Scene.GetCurrent()
	# First output all material
	mtlfilename = '%s.mtl' % '.'.join(filename.split('.')[:-1])
	save_mtl(mtlfilename)

	file = open(filename, "w")
	
	# Write Header
	file.write('# Blender OBJ File: %s\n' % (Get('filename')))
	file.write('# www.blender.org\n')

	# Tell the obj file what material file to use.
	file.write('mtllib %s\n' % ( mtlfilename.split('\\')[-1].split('/')[-1] ))

	# Initialize totals, these are updated each object
	totverts = totuvco = 0
	
	globalUVCoords = {}
	
	# Get all meshs
	for ob in scn.getChildren():
		if ob.getType() != 'Mesh':
			continue
		m = NMesh.GetRawFromObject(ob.name)
		m.transform(ob.matrix)
		
		if not m.faces: # Make sure there is somthing to write
			continue #dont bother with this mesh.
		
		faces = [ f for f in m.faces if len(f) > 2 ]
		materials = m.materials
		
		# Sort by Material so we dont over context switch in the obj file.
		if len(materials) > 1:
			faces.sort(lambda a,b: cmp(a.mat, b.mat))
		
		# Set the default mat
		currentMatName = NULL_MAT
		currentImgName = NULL_IMG
		
		file.write('o %s_%s\n' % (ob.getName(), m.name)) # Write Object name
		
		# Vert
		for v in m.verts:
			file.write('v %.6f %.6f %.6f\n' % tuple(v.co))
	
		# UV
		if m.hasFaceUV():
			for f in faces:
				for uv in f.uv:
					uvKey = '%.6f %.6f' % uv
					try:
						dummy = globalUVCoords[uvKey]
					except KeyError:
						totuvco +=1 # 1 based index.
						globalUVCoords[uvKey] = totuvco
						file.write('vt %s 0.0\n' % uvKey)
						
		# NORMAL
		for v in m.verts:
			file.write('vn %.6f %.6f %.6f\n' % tuple(v.no))
		
		uvIdx = 0
		for f in faces:
			# Check material and change if needed.
			if len(materials) > 0:
				if currentMatName != materials[f.mat].getName():
					currentMatName = materials[f.mat].getName()
					file.write('usemtl %s\n' % (currentMatName))
			
			elif currentMatName != NULL_MAT:
				currentMatName = NULL_MAT
				file.write('usemtl %s\n' % (currentMatName))
		
			# UV IMAGE
			# If the face uses a different image from the one last set then add a usemap line.
			if f.image:
				if f.image.filename != currentImgName:
					currentImgName = f.image.filename
					# Set a new image for all following faces
					file.write( 'usemap %s\n' % currentImgName.split('\\')[-1].split('/')[-1] )
				
			elif currentImgName != NULL_IMG: # Not using an image so set to NULL_IMG
				currentImgName = NULL_IMG
				# Set a ne w image for all following faces
				file.write( 'usemap %s\n' % currentImgName) # No splitting needed.
			
			file.write('f')
			if m.hasFaceUV():
				for vi, v in enumerate(f.v):
					uvIdx = globalUVCoords[ '%.6f %.6f' % f.uv[vi] ]
					i = v.index + totverts + 1
					file.write( ' %d/%d/%d' % (i, uvIdx, i)) # vert, uv, normal
					
			else: # No UV's
				for v in f.v:
					file.write( ' %d' % (v.index + totverts+1))
			file.write('\n')
		
		# Make the indicies global rather then per mesh
		totverts += len(m.verts)
	file.close()
	print "obj export time: %.2f" % (sys.time() - time1)

Window.FileSelector(save_obj, 'Export Wavefront OBJ', newFName('obj'))
