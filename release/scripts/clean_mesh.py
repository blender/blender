#!BPY

"""
Name: 'Cleanup Meshes'
Blender: 234
Group: 'Mesh'
Tooltip: 'Clean unused data from all selected meshes'
"""

# $Id$
#
# -------------------------------------------------------------------------- 
# Mesh Cleaner 1.0 By Campbell Barton (AKA Ideasman)
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


# Made by Ideasman/Campbell 2004/04/25 - ideasman@linuxmail.org

import Blender
from Blender import *
from math import sqrt

VRemNum = ERemNum = FRemNum = 0 # Remember for statistics


#================#
# Math functions #
#================#
def compare(f1, f2, limit):
  if f1 + limit > f2 and f1 - limit < f2:
    return 1
  return 0

def measure(v1, v2):
  return Mathutils.Vector([v1[0]-v2[0], v1[1] - v2[1], v1[2] - v2[2]]).length

def triArea2D(v1, v2, v3):
  e1 = measure(v1, v2)  
  e2 = measure(v2, v3)  
  e3 = measure(v3, v1)  
  p = e1+e2+e3
  return 0.25 * sqrt(p*(p-2*e1)*(p-2*e2)*(p-2*e3))


#=============================#
# Blender functions/shortcuts #
#=============================#
def error(str):
	Draw.PupMenu('ERROR%t|'+str)

def getLimit(text):
  return Draw.PupFloatInput(text, 0.001, 0.0, 1.0, 0.1, 3)

def faceArea(f):
  if len(f.v) == 4:
    return triArea2D(f.v[0].co, f.v[1].co, f.v[2].co) + triArea2D(f.v[0].co, f.v[2].co, f.v[3].co)
  elif len(f.v) == 3:
    return triArea2D(f.v[0].co, f.v[1].co, f.v[2].co)



#================#
# Mesh functions #
#================#
def delFreeVert(mesh):
  global VRemNum
  usedList = eval('[' + ('False,' * len(mesh.verts) )+ ']')
  # Now tag verts that areused
  for f in mesh.faces:
    for v in f.v:
      usedList[mesh.verts.index(v)] = True
  vIdx = 0
  for bool in usedList:
    if bool == False:
      mesh.verts.remove(mesh.verts[vIdx])
      vIdx -= 1
      VRemNum += 1
    vIdx += 1
  mesh.update()


def delEdge(mesh):
  global ERemNum
  fIdx = 0
  while fIdx < len(mesh.faces):
    if len(mesh.faces[fIdx].v) == 2:
      mesh.faces.remove(mesh.faces[fIdx])
      ERemNum += 1
      fIdx -= 1
    fIdx +=1
  mesh.update()

def delEdgeLen(mesh, limit):
  global ERemNum
  fIdx = 0
  while fIdx < len(mesh.faces):
    if len(mesh.faces[fIdx].v) == 2:
      if measure(mesh.faces[fIdx].v[0].co, mesh.faces[fIdx].v[1].co) <= limit:
        mesh.faces.remove(mesh.faces[fIdx])
        ERemNum += 1
        fIdx -= 1
    fIdx +=1	
  mesh.update()

def delFaceArea(mesh, limit):
  global FRemNum
  fIdx = 0
  while fIdx < len(mesh.faces):
    if len(mesh.faces[fIdx].v) > 2:
      if faceArea(mesh.faces[fIdx]) <= limit:
        mesh.faces.remove(mesh.faces[fIdx])
        FRemNum += 1
        fIdx -= 1
    fIdx +=1
  mesh.update()


#====================#
# Make a mesh list   #
#====================#

is_editmode = Window.EditMode()
if is_editmode: Window.EditMode(0)

meshList = []
if len(Object.GetSelected()) > 0:
  for ob in Object.GetSelected():
    if ob.getType() == 'Mesh':
      meshList.append(ob.getData())


#====================================#
# Popup menu to select the functions #
#====================================#
if len(meshList) == 0:
  error('no meshes in selection')
else:
  method = Draw.PupMenu(\
  'Clean Mesh, Remove...%t|\
  Verts: free standing|\
  Edges: not in a face|\
  Edges: below a length|\
  Faces: below an area|%l|\
  All of the above|')
  
  if method >= 3:
    limit = getLimit('threshold: ')

  if method != -1:
    for mesh in meshList:
      if method == 1:
        delFreeVert(mesh)
      elif method == 2:
        delEdge(mesh)
      elif method == 3:
        delEdgeLen(mesh, limit)
      elif method == 4:
        delFaceArea(mesh, limit)
      elif method == 6: # All of them
        delFaceArea(mesh, limit)
        delEdge(mesh)
        delFreeVert(mesh)
      
      mesh.update(0)
      Redraw()

if is_editmode: Window.EditMode(1)

if method != -1:
  Draw.PupMenu('Removed from ' + str(len(meshList)) +' Mesh(es)%t|' + 'Verts:' + str(VRemNum) + ' Edges:' + str(ERemNum) + ' Faces:' + str(FRemNum))
