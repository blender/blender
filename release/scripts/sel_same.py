#!BPY

"""
Name: 'Select Same Faces'
Blender: 232
Group: 'UV'
Tooltip: 'Select faces if attributes match the active.'
"""

# $Id$
#
#===============================================#
# Sel Same script 1.0 by Campbell Barton        #
# email me ideasman@linuxmail.org               #
#===============================================#


# -------------------------------------------------------------------------- 
# Sel Same Face 1.0 By Campbell Barton (AKA Ideasman)
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



from Blender import *
from Blender.Mathutils import DotVecs, Vector
from math import sqrt


#====================================#
# Sanity checks                      #
#====================================#
def error(str):
	Draw.PupMenu('ERROR%t|'+str)
af = None 
selection = Object.GetSelected()
if len(selection) == 0:
  error('No object selected')
else:
  object = Object.GetSelected()[0]
  if object.getType() != 'Mesh':
    error('Not a mesh')
  else:
    mesh = object.getData()
    
    # We have a mesh so find AF.
    for f in mesh.faces:
      if f.flag & NMesh.FaceFlags['ACTIVE']:
        af = f

if af == None:
  error('no active face')
  
else: # Okay everything seems sane
  
  #=====================================
  # Popup menu to select the functions #
  #====================================#
  method = Draw.PupMenu(\
  'Select Same as Active%t|\
  Material|\
  UV Image|\
  Face Mode|\
  Vertex Colours|\
  UV CO-Ords|\
  Area|\
  Proportions|\
  Normal|\
  Co-Planer|')
  
  if method != -1:
    #================================================#
    # Do we add, seb or set to the existing face sel #
    #================================================#
    faceOp = Draw.PupMenu(\
    'Active Face Match%t|\
    Add to Selection|\
    Subtract From Selection |\
    Overwrite Selection|\
    Overwrite Selection Inverse|')
    
    if faceOp != -1:
      
      def setFSel(f):
        if faceOp == 1 or faceOp == 3:
          f.flag |= NMesh.FaceFlags['SELECT'] # will set selection
        elif faceOp == 2 or faceOp ==4:
          f.flag &=~NMesh.FaceFlags['SELECT'] # will unselect, note the '~' to invert bitflags
      
      def setFUnSel(f):
        if faceOp == 3:
          f.flag &=~NMesh.FaceFlags['SELECT'] # will unselect, note the '~' to invert bitflags
        elif faceOp == 4:
          f.flag |= NMesh.FaceFlags['SELECT'] # will set selection
      
      
      
      #================#
      # Math functions #
      #================#
      def compare(f1, f2, limit):
        if f1 + limit > f2 and f1 - limit < f2:
          return 1
        return 0
      
      def compare2(v1, v2, limit):
        if v1[0] + limit > v2[0] and v1[0] - limit < v2[0]:
            if v1[1] + limit > v2[1] and v1[1] - limit < v2[1]:
              return 1
        return 0
      
      def compare3(v1, v2, limit):
        if v1[0] + limit > v2[0] and v1[0] - limit < v2[0]:
            if v1[1] + limit > v2[1] and v1[1] - limit < v2[1]:
              if v1[2] + limit > v2[2] and v1[2] - limit < v2[2]:
                return 1
        return 0
      
      def colCompare(v1, v2, limit):
        # Simple test first
        if v1.r == v2.r:
          if v1.g == v2.g:
            if v1.b == v2.b:
              return 1  
        # Now a test that uses the limit.
        limit = int(limit * 255)
        if v1.r + limit >= v2.r and v1.r - limit <= v2.r:
          if v1.g + limit >= v2.g and v1.g - limit <= v2.g:
            if v1.b + limit >= v2.b and v1.b - limit <= v2.b:
              return 1
        return 0
      
      # Makes sure face 2 has all the colours of face 1
      def faceColCompare(f1, f2, limit):
        avcolIdx = 0
        while avcolIdx < len(f1.col):
          match = 0
      
          vcolIdx = 0
          while vcolIdx < len(f2.col):
            if colCompare(f1.col[avcolIdx], f2.col[vcolIdx], limit):
              match = 1
              break
            vcolIdx += 1
          if match == 0: # premature exit if a motch not found
            return 0
          avcolIdx += 1
        return 1
      
      
      
      # Makes sure face 2 has matching UVs within the limit.
      def faceUvCompare(f1, f2, limit):
        for auv in f1.uv:
          match = 0
          for uv in f2.uv:
            if compare2(auv, uv, limit):
              match = 1
              break
          if match == 0: # premature exit if a motch not found
            return 0
        return 1
      
      
      def measure(v1, v2):
        return Mathutils.Vector([v1[0]-v2[0], v1[1] - v2[1], v1[2] - v2[2]]).length
      
      def triArea2D(v1, v2, v3):
        e1 = measure(v1, v2)  
        e2 = measure(v2, v3)  
        e3 = measure(v3, v1)  
        p = e1+e2+e3
        return 0.25 * sqrt(p*(p-2*e1)*(p-2*e2)*(p-2*e3))
      #====================#
      # End Math Functions #
      #====================#
      
      
      
      #=============================#
      # Blender functions/shortcuts #
      #=============================#
      def getLimit(text):
        return Draw.PupFloatInput(text, 0.1, 0.0, 1.0, 0.1, 3)
      
      def faceArea(f):
        if len(f.v) == 4:
          return triArea2D(f.v[0].co, f.v[1].co, f.v[2].co) + triArea2D(f.v[0].co, f.v[2].co, f.v[3].co)
        elif len(f.v) == 3:
          return triArea2D(f.v[0].co, f.v[1].co, f.v[2].co)
        
      def getEdgeLengths(f):
        if len(f.v) == 4:
          return (measure(f.v[0].co, f.v[1].co), measure(f.v[1].co, f.v[2].co), measure(f.v[2].co, f.v[3].co) , measure(f.v[3].co, f.v[0].co) )
        elif len(f.v) == 3:
          return (measure(f.v[0].co, f.v[1].co), measure(f.v[1].co, f.v[2].co), measure(f.v[2].co, f.v[0].co) )
      
      
      def faceCent(f):
        x = y = z = 0
        for v in f.v:
          x += v.co[0]
          y += v.co[1]
          z += v.co[2]
        x = x/len(f.v)
        y = y/len(f.v)
        z = z/len(f.v)
        return Vector([x, y, z])
      
      #========================================#
      # Should we bother computing this faces  #
      #========================================#
      def fShouldCompare(f):
        # Only calculate for faces that will be affected.
        if faceOp == 1 and f.flag == 1:
          return 0
        elif faceOp == 0 and f.flag == 0:
          return 0
        elif f.flag == 64: # Ignore hidden
          return 0
        return 1
        
      #=======================================#
      # Sel same funcs as called by the menus #
      #=======================================#
      def get_same_mat():
        for f in mesh.faces:
          if fShouldCompare(f):
            if af.mat == f.mat: setFSel(f)
            else:               setFUnSel(f)
      
      def get_same_image():
        for f in mesh.faces:
          if fShouldCompare(f):
            if af.image == f.image: setFSel(f)
            else:                   setFUnSel(f)
      
      def get_same_mode():
        for f in mesh.faces:
          if fShouldCompare(f):
            if af.mode == f.mode: setFSel(f)
            else:                 setFUnSel(f)
      
      def get_same_vcol(limit):
        for f in mesh.faces:
          if fShouldCompare(f):
            if faceColCompare(f, af, limit) and faceColCompare(af, f, limit) :
              setFSel(f)
            else:
              setFUnSel(f)
      
      def get_same_uvco(limit):
        for f in mesh.faces:
          if fShouldCompare(f):
            if faceUvCompare(af, f, limit): setFSel(f)
            else:                           setFUnSel(f)
      
      def get_same_area(limit):
        afArea = faceArea(af)
        limit = limit * afArea # Make the lomot proportinal to the 
        for f in mesh.faces:
          if fShouldCompare(f):
            if compare(afArea, faceArea(f), limit): setFSel(f)
            else:                                   setFUnSel(f)
      
        
      def get_same_prop(limit):
        
        # Here we get the perimeter and use it for a proportional limit modifier.
        afEdgeLens = getEdgeLengths(af)
        perim = 0
        for e in afEdgeLens:
          perim += e
      
        limit = limit * perim
        for f in mesh.faces:
          if fShouldCompare(f):
            for ae in afEdgeLens:
              match = 0
              for e in getEdgeLengths(f):
                if compare(ae, e, limit):
                  match = 1
                  break
              if not match:
                break
           
            if match: setFSel(f)
            else:     setFUnSel(f)
      
      def get_same_normal(limit):    
        limit = limit * 2
        for f in mesh.faces:
          if fShouldCompare(f):
            if compare3(af.no, f.no, limit): setFSel(f)
            else:                            setFUnSel(f)
      
      def get_same_coplaner(limit):
        nlimit = limit * 2 # * 1 # limit for normal test
        climit = limit * 3 # limit for coplaner test.
        afCent = faceCent(af)
        for f in mesh.faces:
          if fShouldCompare(f):
            match = 0
            if compare3(af.no, f.no, nlimit):
              fCent = faceCent(f)
              if abs(DotVecs(Vector([af.no[0], af.no[1], af.no[2]]), afCent ) - DotVecs(Vector([af.no[0], af.no[1], af.no[2]]), fCent )) <= climit:
                match = 1
            if match:
              setFSel(f)
            else:
              setFUnSel(f)
      #=====================#
      # End Sel same funcs  #
      #=====================#
      
      limit = 1 # some of these dont use the limit so it needs to be set, to somthing.
      # act on the menu item selected
      if method == 1: # Material
        get_same_mat()
      elif method == 2: # UV Image
        get_same_image()
      elif method == 3: # mode
        get_same_mode()
      elif method == 4: # vertex colours
        limit = getLimit('vert col limit: ')
        if limit != None:
          get_same_vcol(limit)
      elif method == 5: # UV-coords
        limit = getLimit('uv-coord limit: ')
        if limit != None:
          get_same_uvco(limit)
      elif method == 6: # area
        limit = getLimit('area limit: ')
        if limit != None:
          get_same_area(limit)
      elif method == 7: # proportions
        limit = getLimit('proportion limit: ')
        if limit != None:
          get_same_prop(limit)
      elif method == 8: # normal
        limit = getLimit('normal limit: ')
        if limit != None:
          get_same_normal(limit)
      elif method == 9: # coplaner
        limit = getLimit('coplaner limit: ')
        if limit != None:
          get_same_coplaner(limit)
      
      # If limit is not set then dont bother
      if limit != None:
        mesh.update()
