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
NULL_IMG = '(null)' # Name for mesh's that have no mat set.

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
def getImg(img_fileName):
  for i in Image.Get():
    if i.filename == img_fileName:
      return i
   
  # if we are this far it means the image hasnt been loaded.
  try:
    return Image.Load(img_fileName)
  except:
    print "unable to open", img_fileName
    return



#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def load_mat_image(mat, img_fileName, type, mesh):
  try:
    image = Image.Load(img_fileName)
  except:
    print "unable to open", img_fileName
    return

  texture = Texture.New(type)
  texture.setType('Image')
  texture.image = image

  # adds textures to faces (Textured/Alt-Z mode)
  # Only apply the diffuse texture to the face if the image has not been set with the inline usemat func.
  if type == 'Kd':
    for f in mesh.faces:
      if mesh.materials[f.mat].name == mat.name:
        
        # the inline usemat command overides the material Image
        if not f.image:
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
  # Remove ./
  if mtl_file[:2] == './':
    mtl_file= mtl_file[2:]
    
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
      currentMat.setMirCol(float(l[1]), float(l[2]), float(l[3]))
    elif l[0] == 'Kd':
      currentMat.setRGBCol(float(l[1]), float(l[2]), float(l[3]))
    elif l[0] == 'Ks':
      currentMat.setSpecCol(float(l[1]), float(l[2]), float(l[3]))
    elif l[0] == 'Ns':
      currentMat.setHardness( int((float(l[1])*0.51)) )
    elif l[0] == 'd':
      currentMat.setAlpha(float(l[1]))
    elif l[0] == 'Tr':
      currentMat.setAlpha(float(l[1]))
    elif l[0] == 'map_Ka':
      img_fileName = dir + l[1]
      load_mat_image(currentMat, img_fileName, 'Ka', mesh)
    elif l[0] == 'map_Ks':
      img_fileName = dir + l[1]
      load_mat_image(currentMat, img_fileName, 'Ks', mesh)
    elif l[0] == 'map_Kd':
      img_fileName = dir + l[1]
      load_mat_image(currentMat, img_fileName, 'Kd', mesh)
    lIdx+=1

#==================================================================================#
# This loads data from .obj file                                                   #
#==================================================================================#
def load_obj(file):
  time1 = sys.time()
  def applyMat(mesh, f, mat):
    # Check weather the 16 mat limit has been met.
    if len( meshList[objectName][0].materials ) >= MATLIMIT:
      print 'Warning, max material limit reached, using an existing material'
      return meshList[objectName][0]
      
    mIdx = 0
    for m in meshList[objectName][0].materials:
      if m.getName() == mat.getName():
         break
      mIdx+=1
      
    if mIdx == len(mesh.materials):
      meshList[objectName][0].addMaterial(mat)
      
    f.mat = mIdx
    return f
  
  
  
  # Get the file name with no path or .obj
  fileName = stripName( stripPath(file) )
  
  mtl_fileName = ''
  
  DIR = pathName(file, stripPath(file))

  fileLines = open(file, 'r').readlines()
  
  # Here we store a boolean list of which verts are used or not
  # no we know weather to add them to the current mesh
  # This is an issue with global vertex indicies being translated to per mesh indicies
  # like blenders, we start with a dummy just like the vert.
  # -1 means unused, any other value refers to the local mesh index of the vert.
  
  # objectName has a char in front of it that determins weather its a group or object.
  # We ignore it when naming the object.
  objectName = 'omesh' # If we cant get one, use this

  meshList = {}
  meshList[objectName] = (NMesh.GetRaw(), [-1]) # Mesh/meshList[objectName][1]

  uvMapList = [(0,0)] # store tuple uv pairs here
  
  # This dummy vert makes life a whole lot easier-
  # pythons index system then aligns with objs, remove later
  vertList = [NMesh.Vert(0, 0, 0)] # store tuple uv pairs here
  
  nullMat = getMat(NULL_MAT)
    
  currentMat = nullMat # Use this mat.
  currentImg = NULL_IMG # Null image is a string, otherwise this should be set to an image object.\
  currentSmooth = 0
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
      vertList.append( NMesh.Vert(float(l[1]), float(l[2]), float(l[3]) ) )
      meshList[objectName][1].append(-1) # Ad the moment this vert is not used by any meshList[objectName][0].
      
    elif l[0] == 'vn':
      pass
      
    elif l[0] == 'vt':
      # This is a new vert, make a new mesh
      uvMapList.append( (float(l[1]), float(l[2])) )
      
    elif l[0] == 'f':
        
      # Make a face with the correct material.
      f = NMesh.Face()
      f = applyMat(meshList[objectName][0], f, currentMat)
      f.smooth = currentSmooth
      if currentImg != NULL_IMG: f.image = currentImg

      # Set up vIdxLs : Verts
      # Set up vtIdxLs : UV
      # Start with a dummy objects so python accepts OBJs 1 is the first index.
      vIdxLs = []
      vtIdxLs = []
      fHasUV = len(uvMapList)-1 # Assume the face has a UV until it sho it dosent, if there are no UV coords then this will start as 0.
      for v in l[1:]:
        # OBJ files can have // or / to seperate vert/texVert/normal
        # this is a bit of a pain but we must deal with it.
        objVert = v.split('/', -1)
         
        # Vert Index - OBJ supports negative index assignment (like python)
        
        vIdxLs.append(int(objVert[0]))
        if fHasUV:
          # UV
          if len(objVert) == 1:
            vtIdxLs.append(int(objVert[0])) # Sticky UV coords
          elif objVert[1] != '': # Its possible that theres no texture vert just he vert and normal eg 1//2
            vtIdxLs.append(int(objVert[1])) # Seperate UV coords
          else:
            fHasUV = 0
          
          # Dont add a UV to the face if its larger then the UV coord list
          # The OBJ file would have to be corrupt or badly written for thi to happen
          # but account for it anyway.
	  if len(vtIdxLs) > 0:
            if vtIdxLs[-1] > len(uvMapList):
              fHasUV = 0
              print 'badly written OBJ file, invalid references to UV Texture coordinates.'
      
      # Quads only, we could import quads using the method below but it polite to import a quad as a quad.
      if len(vIdxLs) == 4:
        for i in [0,1,2,3]:
          if meshList[objectName][1][vIdxLs[i]] == -1:
            meshList[objectName][0].verts.append(vertList[vIdxLs[i]])
            f.v.append(meshList[objectName][0].verts[-1])
            meshList[objectName][1][vIdxLs[i]] = len(meshList[objectName][0].verts)-1
          else:
            f.v.append(meshList[objectName][0].verts[meshList[objectName][1][vIdxLs[i]]])
          
        # UV MAPPING
        if fHasUV:
          for i in [0,1,2,3]:
            f.uv.append( uvMapList[ vtIdxLs[i] ] )

        if f.v > 0:
          f = applyMat(meshList[objectName][0], f, currentMat)
          if currentImg != NULL_IMG:
            f.image = currentImg        
          meshList[objectName][0].faces.append(f) # move the face onto the mesh
          if len(meshList[objectName][0].faces[-1]) > 0:
            meshList[objectName][0].faces[-1].smooth = currentSmooth

      
      elif len(vIdxLs) >= 3: # This handles tri's and fans
        for i in range(len(vIdxLs)-2):
          f = NMesh.Face()
          f = applyMat(meshList[objectName][0], f, currentMat)
          for ii in [0, i+1, i+2]:
            if meshList[objectName][1][vIdxLs[ii]] == -1:
              meshList[objectName][0].verts.append(vertList[vIdxLs[ii]])
              f.v.append(meshList[objectName][0].verts[-1])
              meshList[objectName][1][vIdxLs[ii]] = len(meshList[objectName][0].verts)-1
            else:
              f.v.append(meshList[objectName][0].verts[meshList[objectName][1][vIdxLs[ii]]])
          
	  if f.v > 0:
            f = applyMat(meshList[objectName][0], f, currentMat)
            if currentImg != NULL_IMG:
	      f.image = currentImg        
            meshList[objectName][0].faces.append(f) # move the face onto the mesh
            if len(meshList[objectName][0].faces[-1]) > 0:
              meshList[objectName][0].faces[-1].smooth = currentSmooth


          # UV MAPPING
          if fHasUV:
            f.uv.append( uvMapList[ vtIdxLs[0] ] )
            f.uv.append( uvMapList[ vtIdxLs[i+1] ] )
            f.uv.append( uvMapList[ vtIdxLs[i+2] ] )
      
    # Face smoothing.
    elif l[0] == 's':
      if l[1] == 'off': currentSmooth = 0
      else: currentSmooth = 1
      # print "smoothing", currentSmooth
    
    # Object / Group
    elif l[0] == 'o' or l[0] == 'g':
      
      # This makes sure that if an object and a group have the same name then
      # they are not put into the same object.
      if l[0] == 'o':
        newObjectName = 'ob_' + '_'.join(l[1:])
      elif l[0] == 'g':
        newObjectName = 'gp_' + '_'.join(l[1:])
      
      if newObjectName == '':
        objectName = 'ob_mesh'
      else:
        objectName = newObjectName
      
      # If we havnt written to this mesh before then do so.
      if objectName not in meshList.keys():
        meshList[objectName] = (NMesh.GetRaw(), [-1])
        meshList[objectName][0].verts.append( NMesh.Vert(0, 0, 0) )
        
        while len(meshList[objectName][1]) != len(vertList):
          meshList[objectName][1].append(-1)
      if len(l) > 2:
        print l
    
    # Material
    elif l[0] == 'usemtl':
      if l[1] == '(null)':
        currentMat = getMat(NULL_MAT)
      else:
        currentMat = getMat(' '.join(l[1:])) # Use join in case of spaces
    
    elif l[0] == 'usemat':
      if l[1] == '(null)':
        currentImg = NULL_IMG
      else:
        currentImg = getImg(DIR + ' '.join(l[1:])) # Use join in case of spaces 
     
     
    elif l[0] == 'mtllib':
      mtl_fileName = ' '.join(l[1:])
      
    lIdx+=1
  
  #==============================================#
  # Write all meshs in the dictionary            #
  #==============================================#  
  for mk in meshList.keys():
    # Applies material properties to materials alredy on the mesh as well as Textures.
    if mtl_fileName != '':
      load_mtl(DIR, mtl_fileName, meshList[mk][0])
    if len(meshList[mk][0].verts) >1:
      meshList[mk][0].verts.remove(meshList[mk][0].verts[0])
      ob = NMesh.PutRaw(meshList[mk][0], mk)
      if ob.name != None:
        ob.name = mk

  print "obj import time: ", sys.time() - time1

Window.FileSelector(load_obj, 'Import Wavefront OBJ')
