#!BPY 
 
""" 
Name: 'Wavefront (*.obj)' 
Blender: 232 
Group: 'Import' 
Tooltip: 'Load a Wavefront OBJ File' 
 """ 

# -------------------------------------------------------------------------- 
# OBJ Import v0.9 by Campbell Barton (AKA Ideasman) 
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
 
WHITESPACE = [' ', '\n', '\r', '\t', '\f', '\v'] # used for the split function. 
NULL_MAT = '(null)' # Name for mesh's that have no mat set. 

MATLIMIT = 16 

#==============================================# 
# Strips the slashes from the back of a string # 
#==============================================# 
def stripPath(path): 
   for CH in range(len(path), 0, -1): 
      if path[CH-1] == "/" or path[CH-1] == "\\": 
         path = path[CH:] 
         break 
   return path 
    
#====================================================# 
# Strips the prefix off the name before writing      # 
#====================================================# 
def stripName(name): # name is a string 
   prefixDelimiter = '.' 
   return name[ : name.find(prefixDelimiter) ] 

#================================================================# 
# Replace module deps 'string' for join and split #              # 
# - Split splits a string into a list, and join does the reverse # 
#================================================================# 
def split(splitString, WHITESPACE): 
   splitList = [] 
   charIndex = 0 
   while charIndex < len(splitString): 
      # Skip white space 
      while charIndex < len(splitString): 
         if splitString[charIndex] in WHITESPACE:          
            charIndex += 1 
         else: 
            break 
       
      # Gather text that is not white space and append to splitList 
      startWordCharIndex = charIndex 
      while charIndex < len(splitString): 
         if splitString[charIndex] in  WHITESPACE: break 
         charIndex += 1 
          
      # Now we have the first and last chars we can append the word to the list 
      if charIndex != startWordCharIndex: 
         splitList.append(splitString[startWordCharIndex:charIndex]) 
             
   return splitList 

#===============================# 
# Join list items into a string # 
#===============================# 
def join(joinList): 
   joinedString = "" 
   for listItem in joinList: 
      joinedString = joinedString + ' ' + str(listItem) 

   # Remove the first space 
   joinedString = joinedString[1:] 
   return joinedString 


from Blender import * 

def load_obj(file): 
   # This gets a mat or creates one of the requested name if none exist. 
   def getMat(matName): 
      # Make a new mat 
      try: 
         return Material.Get(matName) 
      except: 
         return Material.New(matName) 
    
   def applyMat(mesh, f, mat): 
      # Check weather the 16 mat limit has been met. 
      if len( mesh.materials ) >= MATLIMIT: 
         print 'Warning, max material limit reached, using an existing material' 
         return mesh, f 
       
      mIdx = 0 
      for m in mesh.materials: 
         if m.getName() == mat.getName(): 
            break 
         mIdx+=1 
       
      if mIdx == len(mesh.materials): 
        mesh.addMaterial(mat) 
       
      f.mat = mIdx 
      return mesh, f 
    
   # Get the file name with no path or .obj 
   fileName = stripName( stripPath(file) ) 
    
   fileLines = open(file, 'r').readlines() 
    
   mesh = NMesh.GetRaw() # new empty mesh 
    
   objectName = 'mesh' # If we cant get one, use this 
    
   uvMapList = [] # store tuple uv pairs here 
    
   nullMat = getMat(NULL_MAT) 
    
   currentMat = nullMat # Use this mat. 
    
   # Main loop 
   lIdx = 0 
   while lIdx < len(fileLines): 
      l = split(fileLines[lIdx], WHITESPACE) 
       
      # Detect a line that will be idnored 
      if len(l) == 0: 
         pass 
      elif l[0] == '#' or len(l) == 0: 
         pass 
      # VERTEX 
      elif l[0] == 'v': 
         # This is a new vert, make a new mesh 
         mesh.verts.append( NMesh.Vert(eval(l[1]), eval(l[2]), eval(l[3]) ) ) 
          
      elif l[0] == 'vn':
         pass
          
      elif l[0] == 'vt':
         # This is a new vert, make a new mesh
         uvMapList.append( (eval(l[1]), eval(l[2])) )
          
      elif l[0] == 'f': 
          
         # Make a face with the correct material. 
         f = NMesh.Face() 
         mesh, f = applyMat(mesh, f, currentMat) 
          
         # Set up vIdxLs : Verts 
         # Set up vtIdxLs : UV 
         vIdxLs = []
         vtIdxLs = []
         for v in l[1:]: 
            objVert = split( v, ['/'] )
             
            # VERT INDEX
            vIdxLs.append(eval(objVert[0]) -1)
            # UV
            if len(objVert) == 1:
               vtIdxLs.append(eval(objVert[0]) -1) # Sticky UV coords
            else:
               vtIdxLs.append(eval(objVert[1]) -1) # Seperate UV coords
          
         # Quads only, we could import quads using the method below but it polite to import a quad as a quad.f 
         if len(vIdxLs) == 4:
            f.v.append(mesh.verts[vIdxLs[0]])
            f.v.append(mesh.verts[vIdxLs[1]])
            f.v.append(mesh.verts[vIdxLs[2]])
            f.v.append(mesh.verts[vIdxLs[3]])
            # UV MAPPING
            if uvMapList:
               if vtIdxLs[0] < len(uvMapList):
                  f.uv.append( uvMapList[ vtIdxLs[0] ] )
               if vtIdxLs[1] < len(uvMapList):
                  f.uv.append( uvMapList[ vtIdxLs[1] ] )
               if vtIdxLs[2] < len(uvMapList):
                  f.uv.append( uvMapList[ vtIdxLs[2] ] )
               if vtIdxLs[3] < len(uvMapList):
                  f.uv.append( uvMapList[ vtIdxLs[3] ] )
            mesh.faces.append(f) # move the face onto the mesh

         elif len(vIdxLs) >= 3: # This handles tri's and fans
            for i in range(len(vIdxLs)-2):
               f = NMesh.Face()
               mesh, f = applyMat(mesh, f, currentMat)
               f.v.append(mesh.verts[vIdxLs[0]])
               f.v.append(mesh.verts[vIdxLs[i+1]])
               f.v.append(mesh.verts[vIdxLs[i+2]])
               # UV MAPPING
               if uvMapList:
                  if vtIdxLs[0] < len(uvMapList):
                     f.uv.append( uvMapList[ vtIdxLs[0] ] )
                  if vtIdxLs[1] < len(uvMapList):
                     f.uv.append( uvMapList[ vtIdxLs[i+1] ] )
                  if vtIdxLs[2] < len(uvMapList):
                     f.uv.append( uvMapList[ vtIdxLs[i+2] ] )
                
               mesh.faces.append(f) # move the face onto the mesh
       
      # is o the only vert/face delimeter?
      # if not we could be screwed.
      elif l[0] == 'o':
         # Make sure the objects is worth puttong
         if len(mesh.verts) > 0:
            NMesh.PutRaw(mesh, fileName + '_' + objectName)
         # Make new mesh
         mesh = NMesh.GetRaw()
          
         # New mesh name
         objectName = join(l[1:]) # Use join in case of spaces
          
         # New texture list 
         uvMapList = []
       
      elif l[0] == 'usemtl':
         if l[1] == '(null)':
            currentMat = getMat(NULL_MAT)
         else:
            currentMat = getMat(join(l[1:])) # Use join in case of spaces
             
      lIdx+=1 

   # We need to do this to put the last object. 
   # All other objects will be put alredy 
   if len(mesh.verts) > 0: 
      NMesh.PutRaw(mesh, fileName + '_' + objectName) 

Window.FileSelector(load_obj, 'SELECT OBJ FILE') 
