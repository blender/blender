#!BPY

"""
Name: 'Wavefront (.obj)...'
Blender: 232
Group: 'Import'
Tooltip: 'Load a Wavefront OBJ File'
"""

# $Id$
#
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

NULL_MAT = '(null)' # Name for mesh's that have no mat set.

MATLIMIT = 16 # This isnt about to change but probably should not be hard coded.

DIR = ''

#==============================================#
# Return directory, where is file              #
#==============================================#
def pathName(path,name):
  length=len(path)
  for CH in range(1, length):
     if path[length-CH:] == name:
        path = path[:length-CH]
        break
  return path

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


from Blender import *

#==================================================================================#
# This gets a mat or creates one of the requested name if none exist.              #
#==================================================================================#
def getMat(matName):
   # Make a new mat
   try:
      return Material.Get(matName)
   except:
      return Material.New(matName)

#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def load_image(mat, img_fileName, type, mesh):
   try:
      image = Image.Load(img_fileName)
   except:
      print "unable to open", img_fileName
      return

   texture = Texture.New(type)
   texture.setType('Image')
   texture.image = image

   # adds textures to faces (Textured/Alt-Z mode)
   for f in mesh.faces:
      if mesh.materials[f.mat].name == mat.name:
         f.image = image

   # adds textures for materials (rendering)
   if type == 'Ka':
      mat.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.CMIR)
   if type == 'Kd':
      mat.setTexture(1, texture, Texture.TexCo.UV, Texture.MapTo.COL)
   if type == 'Ks':
      mat.setTexture(2, texture, Texture.TexCo.UV, Texture.MapTo.SPEC)

#==================================================================================#
# This function loads materials from .mtl file (have to be defined in obj file)    #
#==================================================================================#
def load_mtl(dir, mtl_file, mesh):
   mtl_fileName = dir + mtl_file
   try:
      fileLines= open(mtl_fileName, 'r').readlines()
   except:
      print "unable to open", mtl_fileName
      return

   lIdx=0
   while lIdx < len(fileLines):
      l = fileLines[lIdx].split()

      # Detect a line that will be ignored
      if len(l) == 0:
         pass
      elif l[0] == '#' or len(l) == 0:
         pass
      elif l[0] == 'newmtl':
         currentMat = getMat(' '.join(l[1:]))
      elif l[0] == 'Ka':
         currentMat.setMirCol(eval(l[1]), eval(l[2]), eval(l[3]))
      elif l[0] == 'Kd':
         currentMat.setRGBCol(eval(l[1]), eval(l[2]), eval(l[3]))
      elif l[0] == 'Ks':
         currentMat.setSpecCol(eval(l[1]), eval(l[2]), eval(l[3]))
      elif l[0] == 'Ns':
         currentMat.setEmit(eval(l[1])/100.0)
      elif l[0] == 'd':
         currentMat.setAlpha(eval(l[1]))
      elif l[0] == 'Tr':
         currentMat.setAlpha(eval(l[1]))
      elif l[0] == 'map_Ka':
         img_fileName = dir + l[1]
         load_image(currentMat, img_fileName, 'Ka', mesh)
      elif l[0] == 'map_Kd':
         img_fileName = dir + l[1]
         load_image(currentMat, img_fileName, 'Kd', mesh)
      elif l[0] == 'map_Ks':
         img_fileName = dir + l[1]
         load_image(currentMat, img_fileName, 'Ks', mesh)
      lIdx+=1

#==================================================================================#
# This loads data from .obj file                                                   #
#==================================================================================#
def load_obj(file):
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
   
   mtl_fileName = ''
   
   DIR = pathName(file, stripPath(file))

   fileLines = open(file, 'r').readlines()
     
   mesh = NMesh.GetRaw() # new empty mesh
     
   objectName = 'mesh' # If we cant get one, use this
     
   uvMapList = [] # store tuple uv pairs here
     
   nullMat = getMat(NULL_MAT)
     
   currentMat = nullMat # Use this mat.
     
   # Main loop
   lIdx = 0
   while lIdx < len(fileLines):
      l = fileLines[lIdx].split()
       
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
            # OBJ files can have // or / to seperate vert/texVert/normal
            # this is a bit of a pain but we must deal with it.
            # Well try // first and if that has a len of 1 then we'll try /
            objVert = v.split('//', -1)
            if len(objVert) == 1:
              objVert = objVert[0].split('/', -1)
             
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
         objectName = '_'.join(l[1:]) # Use join in case of spaces
           
         # New texture list
         uvMapList = []
       
      elif l[0] == 'usemtl':
         if l[1] == '(null)':
            currentMat = getMat(NULL_MAT)
         else:
            currentMat = getMat(' '.join(l[1:])) # Use join in case of spaces
       
      elif l[0] == 'mtllib':
         mtl_fileName = l[1]
             
      lIdx+=1

   # Some material stuff
   if mtl_fileName != '':
      load_mtl(DIR, mtl_fileName, mesh)
       
   # We need to do this to put the last object.
   # All other objects will be put alredy
   if len(mesh.verts) > 0:
      NMesh.PutRaw(mesh, fileName + '_' + objectName)

Window.FileSelector(load_obj, 'Import OBJ')

