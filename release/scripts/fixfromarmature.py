#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Fix from Armature'
Blender: 232
Group: 'Generators'
Tip: 'Fix armature deformation.'
"""

#----------------------------------------------
# jm soler  05/2004 :   'FixfromArmature'
#----------------------------------------------
# Official Page :
#   http://jmsoler.free.fr/util/blenderfile/py/fixfromarmature.py
# Communicate problems and errors on:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/util/blenderfile/py/fixfromarmature.py
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#--------------------------------------------- 

import Blender
try:
  Ozero=Blender.Object.GetSelected()[0]
  nomdelobjet=Ozero.getName()
  Mesh=Blender.NMesh.GetRawFromObject(nomdelobjet)
  Obis = Blender.Object.New ('Mesh')
  Obis.link(Mesh)
  Obis.setSize(Ozero.getSize())
  Obis.setEuler(Ozero.getEuler())
  Obis.setLocation(Ozero.getMatrix()[3][0:3])
  scene = Blender.Scene.getCurrent()
  scene.link (Obis) 
except:
  print "not a mesh or no object selected"
