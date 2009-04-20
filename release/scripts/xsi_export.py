#!BPY


"""
Name: 'SoftImage XSI (.xsi)...'
Blender: 236
Group: 'Export'
Tooltip: 'Export to a SoftImage XSI file'
"""

__author__ = ("Elira")
__url__ = ["Author's site, http://www.creative-realms.net/~elira/blender.html",
"SoftImage's site, www.softimage.com", "blenderartists.org"]
__email__ = ["scripts"]
__version__ = "2005/11/01"


__bpydoc__ = """\
This script exports to the XSI format.

Usage:

Run this script from "File->Export" menu.

Note:<br>
- Updates by Mal Duffin, to assist with XSI to Shockwave 3D conversion.
"""

# $Id: xsi_export.py,v 1.4.6 2005/11/01 
#
#------------------------------------------------------------------------
# XSI exporter for blender 2.36 or above
#
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
#


#
# ---------------------------------------------------------------------------
# XSI Export V 1.4.1 by Elira (at) creative-realms (dot) net
# 
# Updates by Mal Duffin, to assist with XSI to Shockwave 3D conversion
# ---------------------------------------------------------------------------
# 0.0.0 - This header and having blender ID the file.
# 0.1.0 - Output the statis xsi header elements
# 0.2.0 - create a full shell output (no content just structure)
# 0.3.0 - output used materials from the full materials list
# 0.4.0 - output the object model minor data
# 0.5.0 - output the object shape data, storing a uv table
# 0.6.0 - output the triangle lists (uv references stored uv table)
# 0.7.0 - convert output to genuine file writes.
# 1.0.0 - Admit this script exists and wait for flames
# 1.1.0 - Correctly export mesh shapes
# 1.2.0 - Mesh positioning corrected, added back normals
# 1.3.0 - conditionally output uv co-ordinates
# 1.4.0 - export vertex paint colours.
# ---------------------------------------------------------------------------
# 1.4.1 - added basic normal export code, 
#         to get XSI to Shockwave 3D converter working ( Mal Duffin )
# 1.4.2 - invalid mesh checking
#         better normal exporting
#         general code clean up
# 1.4.3 - basic light exporting
#         fix for ambient light being ignored by Shockwave 3D converter
# 1.4.4 - basic camera exporting
# 1.4.5 - exports normals correctly
# 1.4.6 - exports multiple materials per object
# ---------------------------------------------------------------------------
# TO DO
# - Support texturing
#  - for both methods of texturing ( render method, and Game Engine method )
# ---------------------------------------------------------------------------
# add required modules

import Blender
from Blender import sys as bsys
from Blender import Mathutils
from Blender import Lamp
from Blender import Camera
import math



# ---------------------------------------------------------------------------
# globals to make things a lot lot easier
OBJ = []		# the object list
MAT = []		# the materials list 
UVC = []		# uv vert co-ords
UVI = []                # uv vert index
VCC = []		# vert colour co-ords
VCI = []                # vert colour index
FD  = []                # file handle
NORMALS = []			# normal list
mats = []
EXPORT_DIR = ''
WORLD = Blender.World.GetCurrent() 

# ---------------------------------------------------------------------------
# get_path returns the path portion o/wf the supplied filename.
# ---------------------------------------------------------------------------
def get_path(file):
  l=len(file)
  r=0
  for i in range(l, 0, -1):
    if r == 0:
      if file[i-1] == "/" or file[i-1] == "\\":
        r = i
  return file[:r]



# ---------------------------------------------------------------------------
# r2d - radians to degrees
# ---------------------------------------------------------------------------
def r2d(r):
  return round(r*180.0/math.pi,4)



# ---------------------------------------------------------------------------
# d2r - degrees to radians
# ---------------------------------------------------------------------------
def d2r(d):
  return (d*math.pi)/180.0



# ---------------------------------------------------------------------------
# get_filename returns the filename 
# ---------------------------------------------------------------------------
def get_filename(file):
  l=len(file)
  r=0
  for i in range(l, 0, -1):
    if r == 0:
      if file[i-1] == "/" or file[i-1] == "\\":
        r = i
  return file[r:]


# ---------------------------------------------------------------------------
# find materials returns all materials on an object.
# ---------------------------------------------------------------------------
def get_materials(obj):

  # any materials attached to the object itself
  mats = obj.getMaterials(0)

  if 'Mesh' != obj.type:
    return mats

  # now drop down to the mesh level
  #mesh = Blender.NMesh.GetRaw(obj.data.name)

  mats.extend(obj.getData(mesh=1).materials)

  # return the materials list
  
  # Is this correct!!?? - None materials raise an error otherwise
  # but it might screw up the indicies.. TODO... check the exported files.
  return [m for m in mats if m] 

# ---------------------------------------------------------------------------
# do_header writes out the header data
# ---------------------------------------------------------------------------
def do_header():

  global FD

  # this says which xsi version
  FD.write("xsi 0300txt 0032\n\n")

  # static fileinfo block
  FD.write("SI_FileInfo  {\n")
  FD.write("	\"Blender Scene\",\n")
  FD.write("	\"Blender User\",\n")
  FD.write("	\"Now\",\n")
  FD.write("	\"xsi_export Blender Scene Exporter\",\n")
  FD.write("}\n\n")

  # static scene block
  FD.write("SI_Scene no_name {\n")
  FD.write("	\"FRAMES\",\n")
  FD.write("	0.000000,\n")
  FD.write("	100.000000,\n")
  FD.write("	30.000000,\n")
  FD.write("}\n\n")

  # static co-ordinate system block
  FD.write("SI_CoordinateSystem coord {\n")
  FD.write("	1,\n")
  FD.write("	0,\n")
  FD.write("	1,\n")
  FD.write("	0,\n")
  FD.write("	5,\n")
  FD.write("	2,\n")
  FD.write("}\n\n")

  # static angle block 
  FD.write("SI_Angle  {\n")
  FD.write("	0,\n")
  FD.write("}\n\n")

  # static ambience block 
  if WORLD:	ambient = WORLD.getAmb()
  else:		ambient = 0,0,0

  FD.write("SI_Ambience  {\n")
  FD.write("	%f,\n" % ambient[0])
  FD.write("	%f,\n" % ambient[1])
  FD.write("	%f,\n" % ambient[2])
  FD.write("}\n\n")



# ---------------------------------------------------------------------------
# do_materiallibrary writes out the materials subsection.
# ---------------------------------------------------------------------------
def do_materiallibrary():

  global OBJ, MAT, FD

  # set some flags first
  mnum = 0

  # run through every material, how many used?
  for mat in MAT:
    nmat = mat.name

    # first, is this material on any of the objects.
    f = 0
    for obj in OBJ:
      ml = get_materials(obj)
      for mli in ml:
        nmli = mli.name
        if nmli == nmat:
          f = 1
          mnum += 1
          break
      if f == 1:
        break

  bCreateDefault = 0
  # if none then exit
  if not mnum:
    bCreateDefault = 1
#    return
    
  # get to work create the materiallibrary wrapper and fill.
  FD.write("SI_MaterialLibrary  {\n")
  FD.write("	" + str(mnum) + ",\n")

  # run through every material, write the used ones
  for mat in MAT:
    nmat = mat.name

    # find out if on any object, if so we write.
    f = 0
    for obj in OBJ:
      ml = get_materials(obj)
      for mli in ml:
        nmli = mli.name
        if nmli == nmat:
          do_material(mat)
          f = 1
          break
      if f == 1:
        break

  if bCreateDefault == 1:
    do_material ( 0 )

  # clean up
  FD.write("}\n\n")


def removeSpacesFromName(name):
  name = name.replace ( " ", "_" )
  return name


# ---------------------------------------------------------------------------
# do_material writes out this material.
# ---------------------------------------------------------------------------
def do_material(mat):

  global FD

  if mat == 0:
    name = "__default"
    cr = 1.0
    cg = 1.0
    cb = 1.0	
    ca = 1.0
    sp = 0.0
    sr = 0.0
    sg = 0.0
    sb = 0.0
    em = 0.0
    am = 1.0
    sm = 0
  else:
		

    # get the name first
    name = mat.name

   # face colour		r, g, b, a
  # power (spec decay)  fl
  # spec colour		r, g, b
  # emmisive colourm	r, g, b
  # shading model       int   constant, lambert, phong, blinn, shadow, vertex
  # ambient colour	r, g, b

  # get and print the base material block
    cr, cg, cb = mat.getRGBCol()
    ca = mat.getAlpha()

    sp = 0.0
    sr, sg, sb = mat.getSpecCol()
    em = mat.getEmit()
    am = mat.getAmb()
 
  # how do we render this material? start with constant (0)
    sm = 0
    fl = mat.getMode()
    if fl & Blender.Material.Modes['VCOL_PAINT']:
      sm = 5
  

  FD.write("	SI_Material " + removeSpacesFromName(name) + " {\n")
  FD.write("		%f,\n" % cr)
  FD.write("		%f,\n" % cg)
  FD.write("		%f,\n" % cb)
  FD.write("		%f,\n" % ca)
  FD.write("		%f,\n" % sp)
  FD.write("		%f,\n" % sr)
  FD.write("		%f,\n" % sg)
  FD.write("		%f,\n" % sb)
  FD.write("		%f,\n" % em)
  FD.write("		%f,\n" % em)
  FD.write("		%f,\n" % em)
  FD.write("		%d,\n" % sm)
  #FD.write("		%f,\n" % am)
  #FD.write("		%f,\n" % am)
  #FD.write("		%f,\n" % am)
  FD.write("		%f,\n" % cr)
  FD.write("		%f,\n" % cg)
  FD.write("		%f,\n" % cb)

  if mat != 0:
  # if this material has a texture, then add here
    mtex = mat.getTextures()
    for mt in mtex:
      if mt:
        do_texture(mt)

  FD.write("	}\n")



# ---------------------------------------------------------------------------
# do_texture writes out this texture if usable.
# ---------------------------------------------------------------------------
def do_texture(mtex):
  global FD


  # get our texture
  tex = mtex.tex
  tn = tex.name


  # what type of texture, we are limitd
  if tex.type != Blender.Texture.Types.IMAGE:
    return


  FD.write("		SI_Texture2D " + tn + " {\n")

  img = tex.getImage()
  iname = get_filename(img.getFilename())

  FD.write("			\"" + iname + "\",\n")

  # mapping type  ? uv  map wrapped is 4, how to detect?
  # start with a simple xy mapping ie 0
  FD.write("			4,\n")
  
  if img.has_data:	ix, iy = img.getSize()
  else: 			ix, iy = 512,512

  # image width, and height
  FD.write("			%d,\n" % ix)
  FD.write("			%d,\n" % iy)
  # u crop min/max, v crop min/max
  mincu, mincv, maxcu, maxcv = tex.crop
  FD.write("			%d,\n" % ( mincu * ix ) )
  FD.write("			%d,\n" % ( maxcu * ix - 1 ) )
  FD.write("			%d,\n" % ( mincv * iy ) )
  FD.write("			%d,\n" % ( maxcv * iy - 1) )
  # uv swap
  uvs =0
  if (tex.flags & Blender.Texture.Flags.FLIPBLEND):
    uvs = 1
  FD.write("			%d,\n" % uvs )
  # u/v repeat
  if img.has_data:	iru = img.getXRep()
  else:			iru = 1
  FD.write("			%d,\n" % iru )
  if img.has_data:	irv = img.getYRep()
  else:			irv = 1

  FD.write("			%d,\n" % irv )
  # u/v alt - 0, 0
  FD.write("			0,\n" ) 
  FD.write("			0,\n" )
  # u/v scale - 1,1
  FD.write("			1.000000,\n" )
  FD.write("			1.000000,\n" )
  # u/v offset - 0,0
  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )
  # proj mat 4x4 1 0 0 0, 0 1 0 0, 0 0 1 0, 0 0 0 1  is default
  FD.write("			1.000000,\n" )
  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )

  FD.write("			0.000000,\n" )
  FD.write("			1.000000,\n" )
  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )

  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )
  FD.write("			1.000000,\n" )
  FD.write("			0.000000,\n" )

  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )
  FD.write("			0.000000,\n" )
  FD.write("			1.000000,\n" )

  # blending type - 3
  FD.write("			3,\n" )
  # blending - 1
  FD.write("			1.000000,\n" )
  # ambient - 0
  FD.write("			0.000000,\n" )
  # diffuse - 1
  FD.write("			1.000000,\n" )
  # speculara - 0
  FD.write("			0.000000,\n" )
  # transparent - 0
  FD.write("			0.000000,\n" )
  # reflective - 0
  FD.write("			0.000000,\n" )
  # roughness - 0
  FD.write("			0.000000,\n" )
  
  # close off this texture
  FD.write("		}\n")



# ---------------------------------------------------------------------------
# do_model_transform dumps out the transform data
# ---------------------------------------------------------------------------
def do_model_transform(obj):

  global FD

  # now output
  FD.write("		SI_Transform SRT-" + removeSpacesFromName( obj.name ) + " {\n" )

  

  # write out the object size? (scaling)
  FD.write("			%f,\n" % obj.SizeX )
  FD.write("			%f,\n" % obj.SizeY )
  FD.write("			%f,\n" % obj.SizeZ )
  
  # write out the object rotation
  FD.write("			%f,\n" % r2d(obj.RotX) )
  FD.write("			%f,\n" % r2d(obj.RotY) )
  FD.write("			%f,\n" % r2d(obj.RotZ) )

  # this is the position of the object's axis
  FD.write("			%f,\n" % obj.LocX )
  FD.write("			%f,\n" % obj.LocY )
  FD.write("			%f,\n" % obj.LocZ )
  FD.write("		}\n\n")



# ---------------------------------------------------------------------------
# do_model_visibility marks if the model is visible or not???
# ---------------------------------------------------------------------------
def do_model_visibility(obj):

  global FD

  # for now this is a static block
  FD.write("		SI_Visibility  {\n" )
  FD.write("			1,\n" )
  FD.write("		}\n\n" )



# ---------------------------------------------------------------------------
# do_model_material sets the global material for the model
# ---------------------------------------------------------------------------
def do_model_material(obj):

  global FD

  # do we have one?
  ml = get_materials(obj)


  n = 0
  for mli in ml:
    if mli:
      n+=1
      if n == 1:
        mat=mli


  # if no materials just go back
  if n == 0:
    return

  # for now we grab the first material on the list.

  for mat in ml:
    FD.write("		SI_GlobalMaterial  {\n" )
    FD.write("			\"" + removeSpacesFromName(mat.name) + "\",\n" )
    FD.write("			\"NODE\",\n" )
    FD.write("		}\n\n" )


# ---------------------------------------------------------------------------
# do_collect_uv, makes an easy to use list out of the uv data
# todo, remove duplicates and compress the list size, xsi supports this.
# ---------------------------------------------------------------------------
def do_collect_uv(mesh):

  global UVC, UVI

  # reset the uv details first.
  UVI = []
  UVC = []

  #print "Textures..."
  #mtex = mat.getTextures()
  #for mt in mtex:
  #  print mt


  # if no uv data then return
  if not mesh.hasFaceUV():
    return

  # run through all the faces
  j = 0
  for f in mesh.faces:
    for uv in f.uv:
      UVI.append(j)
      UVC.append(uv)
      j+=1
    UVI.append(-1)



# ---------------------------------------------------------------------------
# do_collect_colour, makes an easy to use list out of the colour data
# todo, remove duplicates and compress the list size, xsi supports this.
# ---------------------------------------------------------------------------
def do_collect_colour(mesh):

  global VCC, VCI

  # reset the uv details first.
  VCC = []
  VCI = []

  # if no uv data then return
  if not mesh.hasVertexColours():
    return

  # run through all the faces
  j = 0
  for f in mesh.faces:
    for c in f.col:
      VCI.append(j)
      VCC.append(c)
      j+=1
    VCI.append(-1)



# ---------------------------------------------------------------------------
# do_mesh_shape outputs the shape data
# ---------------------------------------------------------------------------
def do_mesh_shape(obj):

  global UVC, UVI, VCC, VCI, FD, NORMALS

  # Grab the mesh itself
  mesh = obj.data

  # get the world matrix
  matrix = obj.getMatrix('worldspace')

  # we need to decide about vertex and uv details first.
  do_collect_uv(mesh)
  do_collect_colour(mesh)

  # output the shell
  elements=2
  if len(UVC):
    elements+=1
  if len(VCC):
    elements+=1
  FD.write("			SI_Shape SHP-" + removeSpacesFromName ( obj.name ) + "-ORG {\n" )
  FD.write("				%d,\n" % elements )
  FD.write("				\"ORDERED\",\n\n" )

  # vertices first
  FD.write("				%d,\n" % len(mesh.verts) )
  FD.write("				\"POSITION\",\n" )
  for v in mesh.verts:
    FD.write("				%f,%f,%f,\n" % (v.co[0], v.co[1], v.co[2]) )
  FD.write("\n")


  print "  MESH NAME = " + mesh.name

  NORMALS = []
  for f in mesh.faces:
    NORMALS.append ( f.no )
  for v in mesh.verts:
    aTemp = [v.no[0], v.no[1], v.no[2]]
    NORMALS.append ( aTemp )


  FD.write("				%d,\n" % len(NORMALS) )
  FD.write("				\"NORMAL\",\n" )							
				
  for n in NORMALS:
	FD.write("				%f,%f,%f,\n" % ( n[0], n[1], n[2] ) )

  # if vertex colour data then process
  if mesh.hasVertexColours():

    # put out the co-ord header
    FD.write("				%d,\n" % len(VCC) )
    FD.write("				\"COLOR\",\n" )

    # now output them
    for vc in VCC:
      FD.write("				%f,%f,%f,%f,\n" % (vc.r/255.0, vc.g/255.0, vc.b/255.0, vc.a/255.0) )



  # if uv data then process
  if mesh.hasFaceUV():
    # put out the co-ord header
    FD.write("				%d,\n" % len(UVC) )
    FD.write("				\"TEX_COORD_UV\",\n" )

    # now output them
    for uv in UVC:
      FD.write("				%f,%f\n" % (uv[0], uv[1]) )

  # close off
  FD.write("			}\n" )
 


# ---------------------------------------------------------------------------
# do_mesh_faces outputs the faces data
# ---------------------------------------------------------------------------
def do_mesh_faces(obj):

  global FD, UVI, VCI, mats

  # do we have a texture?
  ml = get_materials(obj)
  n = 0
  for mli in ml:
    if mli:
      n+=1
      if n == 1:
        mat=mli

  # Grab the mesh itself
  # mesh = Blender.NMesh.GetRaw(obj.data.name)
  
  # mesh = Blender.NMesh.GetRawFromObject(obj.name)

  mesh = obj.data



  tris = []
  normalX = []
  mats = []
  for f in mesh.faces:
    tris.extend ( triangulate_face(f) )
    aVal = triangulate_normals(mesh,f)

    for v in aVal:
      normalX.append ( v )


  triangles = len(tris)

  if n == 0:
    FD.write("			SI_TriangleList " + removeSpacesFromName(obj.name) + " {\n")
    FD.write("				%d,\n" % triangles)

    ostring="				\"NORMAL"
    if len(VCI):
      ostring += "|COLOR"
    if len(UVC):
      ostring += "|TEX_COORD_UV"
    ostring += "\",\n"
    FD.write(ostring)

    FD.write("				\"\",\n\n")

    for t in tris:
      FD.write("				%d,%d,%d,\n" % (t[0], t[2], t[1]))

    FD.write("\n")

    for n in normalX:
      FD.write("				%d,%d,%d,\n" % ( n[0], n[1], n[2] ) )

  # finally close this triangle list off
    FD.write("			}\n\n")



  print "total materials"
  print ml

  for mIndex in range (0,len(ml)):
    mat = ml[mIndex]
    print "checking materials"
    print mat

    aTriCount = 0
    for tIndex in range ( 0, len ( tris ) ):
      aMat = mats[tIndex]
      if aMat == mIndex:
        aTriCount = aTriCount + 1	

  #
  # output the shell
    FD.write("			SI_TriangleList " + removeSpacesFromName(obj.name) + " {\n")
  #  FD.write("				%d,\n" % triangles)
    FD.write("				%d,\n" % aTriCount)

    ostring="				\"NORMAL"
    if len(VCI):
      ostring += "|COLOR"
    if len(UVC):
      ostring += "|TEX_COORD_UV"
    ostring += "\",\n"
    FD.write(ostring)


    FD.write("				\"" + removeSpacesFromName ( mat.name ) + "\",\n\n")

#    FD.write("				\"\",\n\n")


    for tIndex in range ( 0, len ( tris ) ):
      aMat = mats[tIndex]
      if mIndex == aMat:
        t = tris[tIndex]
        FD.write("				%d,%d,%d,\n" % (t[0], t[2], t[1]))

    FD.write("\n")



#    for n in normalX:
    for tIndex in range ( 0, len ( tris ) ):
      aMat = mats[tIndex]
      if mIndex == aMat:
        n = normalX[tIndex]
        FD.write("				%d,%d,%d,\n" % ( n[0], n[1], n[2] ) )



  # if we have it, put out the colour vertex list
  #  ostring = "				"
  #  for i in range(len(VCI)):
    # if a -1 its end of line, write.
  #    if VCI[i] == -1:
  #      ostring = ostring + "\n"
  #      FD.write(ostring)
  #      ostring="				"
  #    else:
  #      ostring = ostring + "%d," % VCI[i]

  # The final set is to work out the uv list, its one set per face
  #  ostring = "				"
  #  for i in range(len(UVI)):
  #    # if a -1 its end of line, write.
  #    if UVI[i] == -1:
  #      ostring = ostring + "\n"
  #      FD.write(ostring)
  #      ostring="				"
  #    else:
  #      ostring = ostring + "%d," % UVI[i]

  # finally close this triangle list off
    FD.write("			}\n\n")


def getNormalInfo(mesh, faceInfo):
  global NORMALS
  aNL = []
  for fi in faceInfo:
    aN = []

    aFace = mesh.faces[fi[0]]

    print aFace

    if (aFace.smooth):
      aN.append ( NORMALS.index ( aFace.v.no[0] ) )
      aN.append ( NORMALS.index ( aFace.v.no[1] ) )
      aN.append ( NORMALS.index ( aFace.v.no[2] ) )
    else:
      aN.append ( NORMALS.index ( aFace.no ) )
      aN.append ( NORMALS.index ( aFace.no ) )
      aN.append ( NORMALS.index ( aFace.no ) )

#      aN.append ( NORMALS.index ( mesh.faces[fi[0]].no ) )
#      aN.append ( NORMALS.index ( mesh.faces[fi[0]].no ) )
#      aN.append ( NORMALS.index ( mesh.faces[fi[0]].no ) )
    
    aNL.append ( aN )
  return aNL
 


# copy of code to triangulate mesh
##################################
def triangulate_face(f):    
  if len(f.v) <= 3:
    #newFaces = [ [f.v[0].index, f.v[1].index, f.v[2].index] ]
    newFaces = [ [f.v[0].index, f.v[2].index, f.v[1].index] ]
    mats.append ( f.materialIndex )
  else:
    #newFaces = [ [f.v[0].index, f.v[1].index, f.v[2].index] ]
    #newFaces.append ( [f.v[3].index, f.v[0].index, f.v[2].index] )
    newFaces = [ [f.v[0].index, f.v[2].index, f.v[1].index] ]
    newFaces.append ( [f.v[3].index, f.v[2].index, f.v[0].index] )
    mats.append ( f.materialIndex )
    mats.append ( f.materialIndex )

  return newFaces

# copy of code to triangulate mesh
##################################
def triangulate_normals(mesh, f): 
 
  if len(f.v) <= 3:
    if f.smooth:	
      n1 = get_normal_index ( mesh, [f.v[0].no[0], f.v[0].no[1], f.v[0].no[2]] )
      n2 = get_normal_index ( mesh, [f.v[1].no[0], f.v[1].no[1], f.v[1].no[2]] )
      n3 = get_normal_index ( mesh, [f.v[2].no[0], f.v[2].no[1], f.v[2].no[2]] )
      newNormals = [[ n1, n2, n3 ]]
    else:
      n1 = get_normal_index ( mesh, [f.no[0], f.no[1], f.no[2]] )
      newNormals = [[ n1, n1, n1 ]]	  
  else:
    if f.smooth:
      n1 = get_normal_index ( mesh, [f.v[0].no[0], f.v[0].no[1], f.v[0].no[2]] )
      n2 = get_normal_index ( mesh, [f.v[1].no[0], f.v[1].no[1], f.v[1].no[2]] )
      n3 = get_normal_index ( mesh, [f.v[2].no[0], f.v[2].no[1], f.v[2].no[2]] )
      n4 = get_normal_index ( mesh, [f.v[3].no[0], f.v[3].no[1], f.v[3].no[2]] )
      newNormals = [ [ n1, n2, n3 ] ]
      newNormals.append ( [ n4, n1, n3 ] )

#      newNormals = [[ n1, n3, n2 ]]
#      newNormals.append ( [ n4, n3, n1 ] )
    else:
      n1 = get_normal_index ( mesh, [f.no[0], f.no[1], f.no[2]] )
      newNormals = [[ n1, n1, n1 ]]
      newNormals.append ( [ n1, n1, n1 ] )

  return newNormals



##################################
def get_normal_index(mesh,normal):
  global NORMALS

  indx=NORMALS.index(normal)
  return indx


# ---------------------------------------------------------------------------
# do_model_mesh outputs the shape/triangelist wrapper block
# ---------------------------------------------------------------------------
def do_model_mesh(obj):

  global FD

  # output the shell
  FD.write("		SI_Mesh MSH-" + removeSpacesFromName(obj.name) + " {\n")

  # todo, add calc normals and calc uv here
  # these can be used in both the following sections.

  # next the shape
  do_mesh_shape(obj)

  # finally the trangle list
  do_mesh_faces(obj)

  # finally close this mesh off
  FD.write("		}\n\n")



# ---------------------------------------------------------------------------
# do_model actually outputs a mesh model
# ---------------------------------------------------------------------------
def do_model(obj):

  global FD

  # we only want meshes for now.
  if 'Mesh' != obj.type:
    return

  # check if the mesh is valid
  if validMesh(obj) <> 0:
	  print "INVALID MESH " + obj.name
	  return


  print "Exporting model " + obj.name

  # start model
  FD.write("	SI_Model MDL-" + removeSpacesFromName(obj.name) + " {\n")

  # do transform
  do_model_transform(obj)

  # do visibility
  do_model_visibility(obj)

  # do global material
  do_model_material(obj)

  # do the mesh
  do_model_mesh(obj)

  # close this model
  FD.write("	}\n")

#
# check for invalid mesh ( faces that have < 3 vertices )
#

def validMesh (obj):
  mesh = obj.data
  for f in mesh.faces:
    if len(f.v) < 3:
      print "MESH HAS FACES WITH < 3 VERTICES"
      return 1
  if len (mesh.faces) == 0:
    print "MESH HAS NO FACES"
    return 1
	
  return 0

# ---------------------------------------------------------------------------
# do_models is the process which allows us to write out a bunch of models
# ---------------------------------------------------------------------------
def do_models():

  global OBJ, MAT, FD

  #create the full scene wrapper object
  FD.write("SI_Model MDL-SceneRoot {\n")
  FD.write("	SI_Transform SRT-SceneRoot {\n" )
  FD.write("		1.000000,\n")
  FD.write("		1.000000,\n")
  FD.write("		1.000000,\n")
  FD.write("		-90.000000,\n")
  FD.write("		0.000000,\n")
  FD.write("		0.000000,\n")
  FD.write("		0.000000,\n")
  FD.write("		0.000000,\n")
  FD.write("		0.000000,\n")
  FD.write("	}\n\n")

  # now process the actual selected meshes themselves
  for obj in OBJ:
    do_model(obj)
 
  for obj in OBJ:
    do_light(obj)

  for obj in OBJ:
    do_camera(obj)

  do_light_ambient ()

  # finally close off the model list
  FD.write("}\n")


# ---------------------------------------------------------------------------
# do_light actually outputs a light model
# ---------------------------------------------------------------------------
def do_light(obj):

  global FD

  # we only want lights for now.
  if 'Lamp' != obj.type:
    return

  print "Exporting light " + obj.name

  aLampType = 1

  lmpName=Lamp.Get(obj.getData(name_only=1))
  lmpType=lmpName.getType()

  if lmpType == Lamp.Types.Lamp:
    aLampType = 0
  elif lmpType == Lamp.Types.Spot:
    aLampType = 0
  elif lmpType == Lamp.Types.Sun:
    aLampType = 1
  else:
    aLampType = 0

  # start model
  FD.write("	SI_Light " + removeSpacesFromName(obj.name) + " {\n")

  # do type
  FD.write("		%d,\n" % aLampType)

  lampName= obj.data
  colour = lampName.col

  # do color
  FD.write("		%f,\n" % colour[0] )
  FD.write("		%f,\n" % colour[1] )
  FD.write("		%f,\n" % colour[2] )

  # do position

  FD.write("		%f,\n" % obj.LocX )
  FD.write("		%f,\n" % obj.LocY )
  FD.write("		%f,\n" % obj.LocZ )


  # close this model
  FD.write("	}\n")


# ---------------------------------------------------------------------------
# do_light actually outputs a light model
# ---------------------------------------------------------------------------
def do_camera(obj):

  global FD

  # we only want cameras for now.
  if 'Camera' != obj.type:
    return

  print "Exporting camera " + obj.name



  # start model
  FD.write("	SI_Camera " + removeSpacesFromName(obj.name) + " {\n")


  cameraName=obj.data
  
  # colour = cameraName.col

  # do position

  FD.write("		%f,\n" % obj.LocX )
  FD.write("		%f,\n" % obj.LocY )
  FD.write("		%f,\n" % obj.LocZ )

  # looking at

  FD.write("		%f,\n" % 0.0 )
  FD.write("		%f,\n" % 0.0 )
  FD.write("		%f,\n" % 0.0 )

  # roll
  FD.write("		%f,\n" % 0.0 )

  aLens = cameraName.getLens()

  # field of view
  FD.write("		%f,\n" % aLens )

  # near plane
  FD.write("		%f,\n" % 1.0 )

  # far plane
  FD.write("		%f,\n" % 10000000.0 )


  # close this model
  FD.write("	}\n")



# ---------------------------------------------------------------------------
# write out the ambient light ( for Shockwave 3D converter )
# ---------------------------------------------------------------------------

def do_light_ambient():
  if WORLD:	ambient = WORLD.getAmb()
  else:		ambient = 0,0,0

  FD.write("	SI_Light ambient_sw3d {\n")

  FD.write("		9,\n")
  FD.write("		%f,\n" % ambient[0])
  FD.write("		%f,\n" % ambient[1])
  FD.write("		%f,\n" % ambient[2])
  FD.write("		0.00000000,\n")
  FD.write("		0.00000000,\n")
  FD.write("		0.00000000,\n")

  FD.write("	}\n")



# ---------------------------------------------------------------------------
# export_xsi is the wrapper function to process the loading of an xsi model.
# ---------------------------------------------------------------------------
def export_xsi(filename):

  global OBJ, MAT, FD, EXPORT_DIR

  # safety check
  if filename.find('.xsi', -4) <= 0:
    print "XSI not found"
    filename += '.xsi'


  export_dir = bsys.dirname(filename)
  if export_dir != EXPORT_DIR:
    EXPORT_DIR = export_dir   

  # open our output  
  FD = open(filename, 'w')

  # get the selected objects, otherwise get them all
  #OBJ = Blender.Object.GetSelected()
  #if not OBJ:
  
  OBJ = list(Blender.Scene.GetCurrent().objects) #Blender.Object.Get()

  # we need some objects, if none specified stop
  if not OBJ:
    return

  # if any exist, grab the materials
  MAT = Blender.Material.Get()

  # output the header data
  do_header()

  # output the materials used by the selected objects.
  do_materiallibrary()

  # we punch out the models, that is, the meshes themselves
  do_models()


  # finally close our file
  FD.close()



# ---------------------------------------------------------------------------
# Lets trigger it off now
# Blender.Window.FileSelector(export_xsi, 'Export SoftImage XSI')

fname = bsys.makename(ext=".xsi")
if EXPORT_DIR <> '':
  fname = bsys.join(EXPORT_DIR, bsys.basename(fname))

Blender.Window.FileSelector(export_xsi, "Export SoftImage XSI", fname)
