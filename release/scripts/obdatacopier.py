#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Data Copier'
Blender: 232
Group: 'Object'
Tip: 'Copy data from active object to other selected ones.'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_lampdatacopier.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.1.1"

__bpydoc__ = """\
Use "Data Copier" to copy attributes from the active object to other selected ones of
its same type.

This script is still in an early version but is already useful for copying
attributes for some types of objects like lamps and cameras.

Usage:

Select the objects that will be updated, select the object whose data will
be copied (they must all be of the same type, of course), then run this script.
Toggle the buttons representing the attributes to be copied and press "Copy".
"""

# ----------------------------------------------------------
# Object DATA copier 0.1.1
# (c) 2004 jean-michel soler
# -----------------------------------------------------------
#----------------------------------------------
# Page officielle/official page du blender python Object DATA copier:
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_lampdatacopier.htm
# Communiquer les problemes et erreurs sur:
# To Communicate problems and errors on:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html
#---------------------------------------------

import Blender
from Blender import *
from Blender.Draw import *
from Blender.BGL import *


O = Object.GetSelected()

def renew():
     global O
     MAJ='ABCDEFGHIJKLMNOPQRSTUVWXYZ'
     O = Object.GetSelected()
     param= [ p for p in dir(O[0].getData()) if (p.find('set')!=0 and p.find('get')!=0 and (MAJ.find(p[0])==-1 or (p in ['R','G','B']))) ]  
     PARAM={}
     evt=4
     doc='doc' 
     for p in param:
         try:
           if p=='mode':
              try:
                 exec "doc=str(%s.Modes)+' ; value : %s'"%( O[0].getType(), str(O[0].getData().mode) )
              except:
                 exec """doc= '%s'+' value = '+ str(O[0].getData().%s)"""%(p,p) 
           elif p=='type':
               try:
                 exec "doc=str(%s.Types)+' ; value : %s'"%( O[0].getType(), str(O[0].getData().type) )
               except:
                 exec """doc= '%s'+' value = '+ str(O[0].getData().%s)"""%(p,p) 
           else:
             exec """doc= '%s'+' value = '+ str(O[0].getData().%s)"""%(p,p)
             if doc.find('built-in')!=-1:
                exec """doc= 'This is a function ! Doc = '+ str(O[0].getData().%s.__doc__)"""%(p)
         except:    
             doc='Doc...' 
         PARAM[p]=[Create(0),evt,doc]
         evt+=1
     return PARAM

def copy():
   global PARAM
   OBJECT=None
   TYPE=None

   for O in Blender.Object.GetSelected():
      if O.getType()!='Mesh' and O.getType()!='Empty' :
          if OBJECT==None and TYPE==None:
               OBJECT=O.getData()
               TYPE= O.getType()

          elif O.getType()==TYPE:
            for p in PARAM.keys():
               if  PARAM[p][0].val==1:
                  try:
                    exec "O.getData().%s=OBJECT.%s"%(p,p) 
                  except:
                    errormsg = "Type Error|It's not possible to copy %s to %s types." % (p,TYPE)
                    Blender.Draw.PupMenu(errormsg)

PARAM= renew()

def EVENT(evt,val):
   pass

def BUTTON(evt):
   global PARAM   
   if (evt==1):
         Exit()

   if (evt==2):

         copy()
         Blender.Redraw()

   if (evt==3):
         PARAM= renew()
         Blender.Redraw()

def DRAW():
  global PARAM, O
  glColor3f(0.7, 0.7, 0.7)
  glClear(GL_COLOR_BUFFER_BIT)
  glColor3f(0.1, 0.1, 0.15)    

  size=Buffer(GL_FLOAT, 4)
  glGetFloatv(GL_SCISSOR_BOX, size)
  size= size.list
  for s in [0,1,2,3]: size[s]=int(size[s])
  ligne=20

  Button ("Exit",1,20,4,80,ligne)
  Button ("Copy",2,102,4,80,ligne)
  Button ("renew",3,184,4,80,ligne)

  glRasterPos2f(20, ligne*2-8)
  Text(O[0].getType()+" DATA copier")


  max=size[3] / 22 -2
  pos   = 1
  decal = 20
  key=PARAM.keys()
  key.sort()
  for p in key:
     if  pos==max:
         decal+=102
         pos=1
     else:
         pos+=1       
     PARAM[p][0]=Toggle(p,
                      PARAM[p][1],
                      decal,
                      pos*22+22,
                      100,
                      20, 
                      PARAM[p][0].val,str(PARAM[p][2]))

  
Register(DRAW,EVENT,BUTTON)
