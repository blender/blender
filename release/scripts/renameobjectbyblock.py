#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Object Name Editor'
Blender: 232
Group: 'Object'
Tip: 'GUI to select and rename objects.'
"""

# ----------------------------------------------------------
# Name OBJECT changer
# (c) 2004 jean-michel soler
# -----------------------------------------------------------
#----------------------------------------------
# Page officielle/offcial page du blender python Name OBJECT changer:
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_renameobjectgui.htm
# Communiquer les problemes et erreurs sur:
# To Communicate problems and errors on:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#--------------------------------------------- 
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html
#---------------------------------------------

CVS=0

import Blender
from Blender import *
from Blender.Draw import *
from Blender.BGL import *


O = Object.Get()
taglist=[]
stringlist=[[],[]]


def renew(taglist):
     global O
     MAJ='ABCDEFGHIJKLMNOPQRSTUVWXYZ'
     O = Object.Get()
     if CVS==0:
        param= [ [p.getName(),O.index(p),str(p.getType())] for p in O ]
     else:
        param= [ [p.getName(),O.index(p),str(p.getType()),O[O.index(p)].isSelected()] for p in O ]
     PARAM={}
     evt=9
     stringlist=[[],[],[]]
     for p in param:
         if p[0] in taglist or len(p)>3:
            if CVS==0:
               PARAM[p[0]]=[Create(1),evt,p[1],p[2],Create(p[0]),evt+1]
            elif p[3]==1:
               PARAM[p[0]]=[Create(1),evt,p[1],p[2],Create(p[0]),evt+1]
            elif p[3]==0:
               PARAM[p[0]]=[Create(0),evt,p[1],p[2],Create(p[0]),evt+1]
            stringlist[0].append(evt+1)
            stringlist[1].append(p[0])
            stringlist[2].append(evt)
         else:   
            PARAM[p[0]]=[Create(0),evt,p[1],p[2],Create(p[0]),evt+1]
            stringlist[0].append(evt+1)
            stringlist[1].append(p[0])
            stringlist[2].append(evt) 
         evt+=2
     return PARAM,stringlist

NEWNAME=Create('Name')

alignment={'BEGIN' : [Create(1),5],
           'END'   : [Create(0),6],
           'POINT' : [Create(0),7],
           'FULL'  : [Create(0),8]}

def rename():
     global NEWNAME, alignment, O, taglist, PARAM, stringlist
     taglist=[]
     for p in PARAM.keys():
        if PARAM[p][0].val==1:
           if alignment['END'][0].val:  
             O[PARAM[p][2]].setName(p+NEWNAME.val) 
             taglist.append(p+NEWNAME.val)
           elif alignment['BEGIN'][0].val:
             O[PARAM[p][2]].setName(NEWNAME.val+p) 
             taglist.append(NEWNAME.val+p)
           elif alignment['FULL'][0].val:
             O[PARAM[p][2]].setName(NEWNAME.val) 
             taglist.append(O[PARAM[p][2]].getName())
     PARAM, stringlist =renew(taglist)
    
PARAM, stringlist = renew(taglist)

def EVENT(evt,val):
   pass

def BUTTON(evt):
   global PARAM  , alignment, O, stringlist, CVS
   if (evt==1):
         Exit()
   elif (evt==2):
         rename()
   elif (evt==3):
         PARAM, stringlist = renew(taglist)

   elif (evt in [5,6,7,8]):    
         for k in alignment.keys():
             if alignment[k][1]!=evt:
                alignment[k][0].val=0


   elif (evt in stringlist[0]):
         O[PARAM[stringlist[1][(evt-9)/2]][2]].setName(PARAM[stringlist[1][(evt-9)/2]][4].val)
         PARAM, stringlist = renew(taglist)

   elif (evt in stringlist[2]):
       try:
         O[PARAM[stringlist[1][(evt-9)/2]][2]].select(PARAM[stringlist[1][(evt-9)/2]][0].val)
       except:
        pass

   Blender.Redraw()

def DRAW():
  global PARAM, O, NEWNAME, alignment
  

  #glColor3f(0.7, 0.7, 0.7)
  glClear(GL_COLOR_BUFFER_BIT)
  glColor3f(0.1, 0.1, 0.15)    

  size=Buffer(GL_FLOAT, 4)
  glGetFloatv(GL_SCISSOR_BOX, size)
  size= size.list
  for s in [0,1,2,3]: size[s]=int(size[s])
  ligne=20

  Button ("Exit",1,20,1,80,ligne)
  Button ("Rename",2,102,1,80,ligne)
  Button ("Renew",3,184,1,80,ligne)

  glRasterPos2f(20, ligne*2-10)
  Text("Object Name Editor")
  NEWNAME=String('Add string : ', 4, 150, ligne*2-16, 150, 18, NEWNAME.val,120 )

  key=alignment.keys()
  key.sort()
  n=150+150+4
  for k in key:
      alignment[k][0]=Toggle(k,alignment[k][1],n,ligne*2-16, 40, 18, alignment[k][0].val)
      n+=40+4

  max=size[3] / 22 -2
  pos   = 1
  decal = 20

  keys=[[PARAM[k][1],k] for k in PARAM.keys()]
  keys.sort()

  
  for p_ in keys:
     p=p_[1] 
     if  pos==max:
         decal+=152
         pos=1
     else:
         pos+=1       
     PARAM[p][0]=Toggle('S',PARAM[p][1],decal,pos*22+22,20,20, PARAM[p][0].val,"Select this one for a group renaming")
     PARAM[p][4]=String('> ',PARAM[p][5],decal+20,pos*22+22,90,20, PARAM[p][4].val,200, "string button to rename immediately but only this object")

     glRasterPos2f(decal+115,pos*22+22)
     Text(PARAM[p][3][:4])
Register(DRAW,EVENT,BUTTON)

