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
      currentMat.setMirCol(eval(l[1]), eval(l[2]), eval(l[3]))
    elif l[0] == 'Kd':
      currentMat.setRGBCol(eval(l[1]), eval(l[2]), eval(l[3]))
    elif l[0] == 'Ks':
      currentMat.setSpecCol(eval(l[1]), eval(l[2]), eval(l[3]))
    elif l[0] == 'Ns':
      currentMat.setHardness( int((eval(l[1])*0.51)) )
    elif l[0] == 'd':
      currentMat.setAlpha(eval(l[1]))
    elif l[0] == 'Tr':
      currentMat.setAlpha(eval(l[1]))
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
    
  uvMapList = [(0,0)] # store tuple uv pairs here
  
  # This dummy vert makes life a whole lot easier-
  # pythons index system then aligns with objs, remove later
  vertList = [NMesh.Vert(0, 0, 0)] # store tuple uv pairs here
  
  # Here we store a boolean list of which verts are used or not
  # no we know weather to add them to the current mesh
  # This is an issue with global vertex indicies being translated to per mesh indicies
  # like blenders, we start with a dummy just like the vert.
  # -1 means unused, any other value refers to the local mesh index of the vert.
  usedList = [-1] 
  
  nullMat = getMat(NULL_MAT)
    
  currentMat = nullMat # Use this mat.
  currentImg = NULL_IMG # Null image is a string, otherwise this should be set to an image object.

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
      vertList.append( NMesh.Vert(eval(l[1]), eval(l[2]), eval(l[3]) ) )
      usedList.append(-1) # Ad the moment this vert is not used by any mesh.
      
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
      # Start with a dummy objects so python accepts OBJs 1 is the first index.
      vIdxLs = []
      vtIdxLs = []
      fHasUV = len(uvMapList)-1 # Assume the face has a UV until it sho it dosent, if there are no UV coords then this will start as 0.
      for v in l[1:]:
        # OBJ files can have // or / to seperate vert/texVert/normal
        # this is a bit of a pain but we must deal with it.
        objVert = v.split('/', -1)
         
        # Vert Index - OBJ supports negative index assignment (like python)
        
        vIdxLs.append(eval(objVert[0]))
        if fHasUV:
          # UV
          if len(objVert) == 1:
            vtIdxLs.append(eval(objVert[0])) # Sticky UV coords
          elif objVert[1] != '': # Its possible that theres no texture vert just he vert and normal eg 1//2
            vtIdxLs.append(eval(objVert[1])) # Seperate UV coords
          else:
            fHasUV = 0
          
          # Dont add a UV to the face if its larger then the UV coord list
          # The OBJ file would have to be corrupt or badly written for thi to happen
          # but account for it anyway.
          if vtIdxLs[-1] > len(uvMapList):
            fHasUV = 0
            print 'badly written OBJ file, invalid references to UV Texture coordinates.'
      
      # Quads only, we could import quads using the method below but it polite to import a quad as a quad.
      if len(vIdxLs) == 4:
        for i in [0,1,2,3]:
          if usedList[vIdxLs[i]] == -1:
            mesh.verts.append(vertList[vIdxLs[i]])
            f.v.append(mesh.verts[-1])
            usedList[vIdxLs[i]] = len(mesh.verts)-1
          else:
            f.v.append(mesh.verts[usedList[vIdxLs[i]]])
          
        # UV MAPPING
        if fHasUV:
          for i in [0,1,2,3]:
            f.uv.append( uvMapList[ vtIdxLs[i] ] )
        mesh.faces.append(f) # move the face onto the mesh
        # Apply the current image to the face
        if currentImg != NULL_IMG:
          mesh.faces[-1].image = currentImg
      
      elif len(vIdxLs) >= 3: # This handles tri's and fans
        for i in range(len(vIdxLs)-2):
          f = NMesh.Face()
          mesh, f = applyMat(mesh, f, currentMat)
            
          if usedList[vIdxLs[0]] == -1:
            mesh.verts.append(vertList[vIdxLs[0]])
            f.v.append(mesh.verts[-1])
            usedList[vIdxLs[0]] = len(mesh.verts)-1
          else:
            f.v.append(mesh.verts[usedList[vIdxLs[0]]])
            
          if usedList[vIdxLs[i+1]] == -1:
            mesh.verts.append(vertList[vIdxLs[i+1]])
            f.v.append(mesh.verts[-1])
            usedList[vIdxLs[i+1]] = len(mesh.verts)-1
          else:
            f.v.append(mesh.verts[usedList[vIdxLs[i+1]]])
            
          if usedList[vIdxLs[i+2]] == -1:
            mesh.verts.append(vertList[vIdxLs[i+2]])
            f.v.append(mesh.verts[-1])
            usedList[vIdxLs[i+2]] = len(mesh.verts)-1
          else:
            f.v.append(mesh.verts[usedList[vIdxLs[i+2]]])  
            
          # UV MAPPING
          if fHasUV:
            f.uv.append( uvMapList[ vtIdxLs[0] ] )
            f.uv.append( uvMapList[ vtIdxLs[i+1] ] )
            f.uv.append( uvMapList[ vtIdxLs[i+2] ] )
          mesh.faces.append(f) # move the face onto the mesh
            
          # Apply the current image to the face
          if currentImg != NULL_IMG:
            mesh.faces[-1].image = currentImg
    
    # Object / Group
    elif l[0] == 'o' or l[0] == 'g':
      
      # Reset the used list
      ulIdx = 0
      while ulIdx < len(usedList):
        usedList[ulIdx] = -1
        ulIdx +=1
      
      # Some material stuff
      if mtl_fileName != '':
        load_mtl(DIR, mtl_fileName, mesh)
      
      # Make sure the objects is worth putting
      if len(mesh.verts) > 1:
        mesh.verts.remove(mesh.verts[0])
        ob = NMesh.PutRaw(mesh, fileName + '_' + objectName)
        if ob != None: # Name the object too.
           ob.name = fileName + '_' + objectName
      
      # Make new mesh
      mesh = NMesh.GetRaw()
      # This dummy vert makes life a whole lot easier-
      # pythons index system then aligns with objs, remove later
      mesh.verts.append( NMesh.Vert(0, 0, 0) )
        
      # New mesh name
      objectName = '_'.join(l[1:]) # Use join in case of spaces
      
    
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
      mtl_fileName = l[1]
      
    lIdx+=1

  # Some material stuff
  if mtl_fileName != '':
    load_mtl(DIR, mtl_fileName, mesh)
      
  # We need to do this to put the last object.
  # All other objects will be put alredy
  if len(mesh.verts) > 1:
    mesh.verts.remove(mesh.verts[0])
    ob = NMesh.PutRaw(mesh, fileName + '_' + objectName)
    if ob != None: # Name the object too.
      ob.name = fileName + '_' + objectName

Window.FileSelector(load_obj, 'Import Wavefront OBJ') 
