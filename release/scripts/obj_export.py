#!BPY 

""" 
Name: 'Wavefront (*.obj)' 
Blender: 232 
Group: 'Export'
Tooltip: 'Save a Wavefront OBJ File' 
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
def apply_transform(verts, matrix):
	verts.resize4D()
	return Mathutils.VecMultMat(verts, matrix)

#====================================================# 
# Return a 6 deciaml point floating point value      # 
# as a string that dosent have any python chars      # 
#====================================================#  
def saneFloat(float): 
  #return '%(float)b' % vars()  # 6 fp as house.hqx 
  return str('%f' % float) + ' ' 


from Blender import * 

NULL_MAT = '(null)' 

def save_obj(filename): 
    
   file = open(filename, "w") 
    
   # Write Header 
   file.write('# Blender OBJ File: ' + Get('filename') + ' \n') 
   file.write('# www.blender.org\n') 
    
   # Get all meshs 
   for ob in Object.Get(): 
      if ob.getType() == 'Mesh': 
         m = ob.getData() 
         if len(m.verts) > 0: # Make sure there is somthing to write. 
             
            # Set the default mat 
            currentMatName = '' 
             
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
            for f in m.faces: 
               for v in f.v: 
                  # Transform the normal 
                  noTx = apply_transform(v.no, matrix) 
                  noTx.normalize()
                  file.write('vn ') 
                  file.write(saneFloat(noTx[0])) 
                  file.write(saneFloat(noTx[1])) 
                  file.write(saneFloat(noTx[2]) + '\n') 
                               
            uvIdx = 0 
            for f in m.faces: 
               # Check material and change if needed. 
               if len(m.materials) > f.mat: 
                  if currentMatName != m.materials[f.mat].getName(): 
                     currentMatName = m.materials[f.mat].getName() 
                     file.write('usemtl ' + currentMatName + '\n') 
                   
               elif currentMatName != NULL_MAT: 
                  currentMatName = NULL_MAT 
                  file.write('usemtl ' + currentMatName + '\n') 
                
               file.write('f ') 
               for v in f.v: 
                  file.write( str(m.verts.index(v) +1) + '/') # Vert IDX 
                  file.write( str(uvIdx +1) + '/') # UV IDX 
                  file.write( str(uvIdx +1) + ' ') # NORMAL IDX 
                  uvIdx+=1 
               file.write('\n') 
                
   file.close() 

Window.FileSelector(save_obj, 'SELECT OBJ FILE') 
