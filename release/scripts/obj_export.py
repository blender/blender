#!BPY

"""
Name: 'Wavefront (.obj)...'
Blender: 232
Group: 'Export'
Tooltip: 'Save a Wavefront OBJ File'
"""

# $Id$
#
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


#================================================#
# Gets the world matrix of an object             #
# by multiplying by parents mat's recursively    #
# This only works in some simple situations,     #
# needs work....                                 #
#================================================#
def getWorldMat(ob):
   mat = ob.getMatrix()
   p = ob.getParent()
   if p != None:
      mat = mat + getWorldMat(p)
   return mat
  
#==================#
# Apply Transform  #
#==================#
def apply_transform(vert, matrix):
	vertCopy = Mathutils.CopyVec(vert)
	vertCopy.resize4D()
	return Mathutils.VecMultMat(vertCopy, matrix)

#====================================================#
# Return a 6 deciaml point floating point value      #
# as a string that dosent have any python chars      #
#====================================================#
def saneFloat(float):
  #return '%(float)b' % vars()  # 6 fp as house.hqx
  return str('%f' % float) + ' '


from Blender import *

NULL_MAT = '(null)'
NULL_IMG = '(null)'

def save_mtl(filename):
	file = open(filename, "w")
	for mat in Material.Get():
		
		file.write('newmtl ' + mat.getName() + '\n') # Define a new material
		
		file.write('Ns ' + saneFloat((mat.getHardness()-1) * 1.9607843137254901 ) + '\n') # Hardness, convert blenders 1-511 to MTL's 
		
		col = mat.getRGBCol()
		file.write('Kd ' + saneFloat(col[0]) + saneFloat(col[1])  + saneFloat(col[2]) +'\n') # Diffuse
		
		col = mat.getMirCol()
		file.write('Ka ' + saneFloat(col[0]) + saneFloat(col[1])  + saneFloat(col[2]) + '\n') # Ambient, uses mirror colour,
		
		col = mat.getSpecCol()
		file.write('Ks ' + saneFloat(col[0]) + saneFloat(col[1])  + saneFloat(col[2]) +'\n') # Specular
		
		file.write('d ' + saneFloat(mat.getAlpha()) +'\n') # Alpha (dissolve)
		
		# illum, 0 to disable lightng, 2 is normal.
		file.write('illum ')
		if mat.getMode() & Material.Modes['SHADELESS']:
			file.write('0\n') # ignore lighting
		else:
			file.write('2\n') # light normaly
		
		# End OF Mat
		file.write('\n') # new line
		
	file.close()



def save_obj(filename):
   
   # First output all material
   mtlfilename = filename[:-4] + '.mtl'
   save_mtl(mtlfilename)
   
   
   
   file = open(filename, "w")
   
   
   # Write Header
   file.write('# Blender OBJ File: ' + Get('filename') + ' \n')
   file.write('# www.blender.org\n')
   
   # Tell the obj file what file to use.
   file.write('mtllib ' + stripPath(mtlfilename) + ' \n')   
   
   
   # Get all meshs
   for ob in Object.Get():
      if ob.getType() == 'Mesh':
         m = ob.getData()
         if len(m.verts) > 0: # Make sure there is somthing to write.
           
            # Set the default mat
            currentMatName = NULL_MAT
            currentImgName = NULL_IMG
           
            file.write('o ' + ob.getName() + '_' + m.name + '\n') # Write Object name
           
            # Dosent work properly,
            matrix = getWorldMat(ob)
           
            # Vert
            for v in m.verts:
               # Transform the vert
               vTx = apply_transform(v.co, matrix)
              
               file.write('v ')
               file.write(saneFloat(vTx[0]))
               file.write(saneFloat(vTx[1]))
               file.write(saneFloat(vTx[2]) + '\n')
            
            # UV
            for f in m.faces:
               if len(f.v) > 2:
                  for uvIdx in range(len(f.v)):
                     file.write('vt ')
                     if f.uv:
                        file.write(saneFloat(f.uv[uvIdx][0]))
                        file.write(saneFloat(f.uv[uvIdx][1]))
                     else:
                        file.write('0.0 ')
                        file.write('0.0 ')
                       
                     file.write('0.0' + '\n')
            
            # NORMAL
            for f1 in m.faces:
               if len(f1.v) > 2:
                  for v in f1.v:
                     # Transform the normal
                     noTx = apply_transform(v.no, matrix)
                     noTx.normalize()
                     file.write('vn ')
                     file.write(saneFloat(noTx[0]))
                     file.write(saneFloat(noTx[1]))
                     file.write(saneFloat(noTx[2]) + '\n')
            
            uvIdx = 0
            for f in m.faces:
               if len(f.v) > 2:
                  # Check material and change if needed.
                  if len(m.materials) > f.mat:
                     if currentMatName != m.materials[f.mat].getName():
                        currentMatName = m.materials[f.mat].getName()
                        file.write('usemtl ' + currentMatName + '\n')
                    
                  elif currentMatName != NULL_MAT:
                     currentMatName = NULL_MAT
                     file.write('usemtl ' + currentMatName + '\n')
                  
                  # UV IMAGE
                  # If the face uses a different image from the one last set then add a usemap line.
                  if f.image:
                    if f.image.filename != currentImgName:
                      currentImgName = f.image.filename
                      file.write( 'usemat ' + stripPath(currentImgName) +'\n') # Set a new image for all following faces
                      
                  elif currentImgName != NULL_IMG: # Not using an image so set to NULL_IMG
                    currentImgName = NULL_IMG
                    file.write( 'usemat ' + stripPath(currentImgName) +'\n') # Set a new image for all following faces
                  
                  file.write('f ')
                  for v in f.v:
                     file.write( str(m.verts.index(v) +1) + '/') # Vert IDX
                     file.write( str(uvIdx +1) + '/') # UV IDX
                     file.write( str(uvIdx +1) + ' ') # NORMAL IDX
                     uvIdx+=1
                  file.write('\n')
              
   file.close()

Window.FileSelector(save_obj, 'Export OBJ', newFName('obj'))
