#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Rvk1 to Rvk2'
Blender: 232
Group: 'Animation'
Tip: 'Copy deform data (not surf. subdiv) of active obj to rvk of the 2nd selected obj.'
"""

#----------------------------------------------
# jm soler (c) 05/2004 : 'Rvk1toRvk2'  release under blender artistic licence
#----------------------------------------------
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html 
#----------------------------------------------------
# Copy the rvk (1, or armature, lattice, or
# any mesh deformation except surface
# sbdivision) of the active object to rvk (2) of
# the second selected object. Create rvk or modify
# absolute key if needed.
#----------------------------------------------
# official Page :
# http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rvk1versrvk2.htm
# download the script :
# http://jmsoler.free.fr/util/blenderfile/py/rvk1_torvk2.py
# Communicate upon problems or errors:
# http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender 
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/util/blenderfile/py/rvk1_torvk2.py
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
#  changelog : 
#        - a test on mesh parity between getraw and getrawfromobject  
#          when there is active subsurf division. 
#        - can copy, or not, vertex groups from the original mesh.    
#---------------------------------------------

import Blender
from Blender import NMesh,Draw,Object

def rvk2rvk():
  try:
    SUBMODIF=0
    RVK2=Object.GetSelected()[0]
    RVK1=Object.GetSelected()[1]
    
    FRAME=Blender.Get('curframe')
  
    DATA2=RVK2.getData()
    
    if DATA2.getMode() & NMesh.Modes['SUBSURF'] :
      SUBSURF2=DATA2.getSubDivLevels()
      if SUBSURF2[0]!=0:
         name = "The active object has a subsurf level different from 0 ... %t| Let script do the the modification for you ? %x1| you prefer do it yourself ? %x2 " 
         result = Draw.PupMenu(name)
         if result==1:      
             DATA2.mode=DATA2.mode-NMesh.Modes['SUBSURF']
             SUBMODIF=1 
             DATA2.update()
             RVK2.makeDisplayList() 
             Blender.Redraw()
         else:
             return
              
    RVK2NAME=Object.GetSelected()[0].getName()
    mesh=RVK1.getData()
    meshrvk2=NMesh.GetRawFromObject(RVK2NAME)
    
    name = "Do you want to replace or add vertex groups ? %t| YES %x1| NO ? %x2 " 
    result = Draw.PupMenu(name)

    if result==1:
       GROUPNAME1=mesh.getVertGroupNames() 
       if len(GROUPNAME1)!=0:
          for GROUP1 in GROUPNAME1:
              mesh.removeVertGroup(GROUP1)

       GROUPNAME2=DATA2.getVertGroupNames()
       if len(GROUPNAME2)!=0:
          for GROUP2 in GROUPNAME2:
              mesh.addVertGroup(GROUP2)
              mesh.assignVertsToGroup(GROUP2,DATA2.getVertsFromGroup(GROUP2),1.0,'replace')

    for v in meshrvk2.verts:
       i= meshrvk2.verts.index(v)
       v1=mesh.verts[i]
       for n in range(len(v.co)):
            v1.co[n]=v.co[n]
    
    mesh.update() 
    mesh.insertKey(FRAME,'relative')
    mesh.update()
    RVK1.makeDisplayList() 

    if SUBMODIF==1:
         DATA2.mode=DATA2.mode+NMesh.Modes['SUBSURF']
         SUBMODIF=0
         DATA2.update()
         RVK2.makeDisplayList() 

    Blender.Redraw()
  except:
    print 'problem  : not object selected or not mesh' 
  

rvk2rvk()
