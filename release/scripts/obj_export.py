#!BPY

"""
Name: 'Wavefront (.obj)...'
Blender: 232
Group: 'Export'
Tooltip: 'Save a Wavefront OBJ File'
"""

__author__ = "Campbell Barton"
__url__ = ["blender", "elysiun"]
__version__ = "0.9"

__bpydoc__ = """\
This script is an exporter to OBJ file format.

Usage:

Run this script from "File->Export" menu to export all meshes.
"""


# --------------------------------------------------------------------------
# OBJ Export v0.9 by Campbell Barton (AKA Ideasman)
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


#===============================================#
# Strips the slashes from the front of a string #
#===============================================#
def stripPath(path):
	for CH in range(len(path), 0, -1):
		if path[CH-1] == "/" or path[CH-1] == "\\":
			path = path[CH:]
			break
	return path

#==================#
# Apply Transform  #
#==================#
def apply_transform(vert, matrix):
	vertCopy = Mathutils.CopyVec(vert)
	vertCopy.resize4D()
	return Mathutils.VecMultMat(vertCopy, matrix)

from Blender import *

NULL_MAT = '(null)'
NULL_IMG = '(null)'

def save_mtl(filename):
	file = open(filename, "w")
	for mat in Material.Get():
		
		file.write('newmtl %s\n' % (mat.getName())) # Define a new material

		# Hardness, convert blenders 1-511 to MTL's 
		file.write('Ns %s\n' % ((mat.getHardness()-1) * 1.9607843137254901 ) )
		
		col = mat.getRGBCol()
		# Diffuse
		file.write('Kd %s %s %s\n' % (col[0], col[1], col[2]))
		
		col = mat.getMirCol()
		# Ambient, uses mirror colour,
		file.write('Ka %s %s %s\n' % (col[0], col[1], col[2]))
		
		col = mat.getSpecCol()
		# Specular
		file.write('Ks %s %s %s\n' % (col[0],col[1], col[2]))

		# Alpha (dissolve)
		file.write('d %s\n' % (mat.getAlpha()))
		
		# illum, 0 to disable lightng, 2 is normal.
		if mat.getMode() & Material.Modes['SHADELESS']:
			file.write('illum 0\n') # ignore lighting
		else:
			file.write('illum 2\n') # light normaly
		
		# End OF Mat
		file.write('\n') # new line
		
	file.close()

def save_obj(filename):
	time1 = sys.time()
	# First output all material
	mtlfilename = filename[:-4] + '.mtl'
	save_mtl(mtlfilename)

	file = open(filename, "w")

	# Write Header
	file.write('# Blender OBJ File: %s\n' % (Get('filename')))
	file.write('# www.blender.org\n')

	# Tell the obj file what material file to use.
	file.write('mtllib %s\n' % (stripPath(mtlfilename)))

	# Initialize totals, these are updated each object
	totverts = totuvco = 0

	# Get all meshs
	for ob in Object.Get():
		if ob.getType() != 'Mesh':
			continue
		m = NMesh.GetRawFromObject(ob.name)
    
		# remove any edges, is not written back to the mesh so its not going to
		# modify the open file.
		for f in m.faces:
			if len(f.v) < 3:
				mesh.faces.remove(f)
  
		if len(m.faces) == 0: # Make sure there is somthing to write.
			continue #dont bother with this mesh.
  
		# Set the default mat
		currentMatName = NULL_MAT
		currentImgName = NULL_IMG
  
		#file.write('o ' + ob.getName() + '_' + m.name + '\n') # Write Object name
		file.write('o %s_%s\n' % (ob.getName(), m.name)) # Write Object name
  
		# Works 100% Yay
		matrix = ob.getMatrix('worldspace')
  
		# Vert
		for v in m.verts:
			# Transform the vert
			vTx = apply_transform(v.co, matrix)
			file.write('v %s %s %s\n' % (vTx[0], vTx[1], vTx[2]))
  
		# UV
		for f in m.faces:
			for uvIdx in range(len(f.v)):
				if f.uv:
					file.write('vt %s %s 0.0\n' % (f.uv[uvIdx][0], f.uv[uvIdx][1]))
				else:
					file.write('vt 0.0 0.0 0.0\n')
  
		# NORMAL
		for f1 in m.faces:
			for v in f1.v:
				# Transform the normal
				noTx = apply_transform(v.no, matrix)
				noTx.normalize()
				file.write('vn %s %s %s\n' % (noTx[0], noTx[1], noTx[2]))

		uvIdx = 0
		for f in m.faces:
			# Check material and change if needed.
			if len(m.materials) > f.mat:
				if currentMatName != m.materials[f.mat].getName():
					currentMatName = m.materials[f.mat].getName()
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
					file.write( 'usemat %s\n' % (stripPath(currentImgName)))
        
			elif currentImgName != NULL_IMG: # Not using an image so set to NULL_IMG
				currentImgName = NULL_IMG
				# Set a new image for all following faces
				file.write( 'usemat %s\n' % (stripPath(currentImgName)))

			file.write('f ')
			for v in f.v:
				file.write( '%s/%s/%s ' % (m.verts.index(v) + totverts+1, uvIdx+totuvco+1, uvIdx+totuvco+1))

				uvIdx+=1
			file.write('\n')
    
		# Make the indicies global rather then per mesh
		totverts += len(m.verts)
		totuvco += uvIdx
	file.close()
	print "obj export time: ", sys.time() - time1

Window.FileSelector(save_obj, 'Export Wavefront OBJ', newFName('obj'))
