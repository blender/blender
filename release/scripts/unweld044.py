#!BPY
""" Registration info for Blender menus: <- these words are ignored
Name: 'UnWeld'
Blender: 232
Group: 'Mesh'
Tip: 'unweld all faces from one selected and commun vertex. Made vertex bevelling'
"""

# $Id$
#
# ------------------------------------------
# Un-Weld script 0.4.4 beta
#
# split all faces from one selected vertex
# (c) 2004 J-M Soler released under Blender Artistic License
#----------------------------------------------
# Official Page :
# http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_faces2vertex.htm#exemple 
# Communicate problems and errors on:
# http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender 
#----------------------------------------------
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html
#---------------------------------------------
# Changelog
#----------------------------------------------
# 25/05 :
# -- separate choise, normal (same place) or spread at random, middle of the face
# -- works on several vertices too
# -- Quite vertex bevelling on <<lone>> vertex : create hole in faces around this
# vertex
# 03/06 :
# -- a sort of "bevelled vertex" extrusion controled by horizontal mouse
# displacement. just a beta test to the mouse control.
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_faces2vertex.htm#exemple
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
# Blender Artistic License
#    http://download.blender.org/documentation/html/x21254.html
#--------------------------------------------- 
# Changelog
#----------------------------------------------
#      25/05 :
#           -- separation du choix, normal ou dispersion hasardeuse, milieu de face
#           -- sommets multiples / 
#           -- Quite vertex bevelling on lone vertex : create hole in faces around the
#              vertex 
#     03/06 :
#           -- a sort of "bevelled vertex" extrusion controled by horizontal mouse
#              displacement
#----------------------------------------------

import Blender
from Blender import Noise
from Blender.Draw import *
from Blender.BGL import *

Nr=Noise.random
decal=0.03
t=[0.0,0.0,0.0]
pl=[]
orig=[]

DEBUG = 0
SUBSURF=0
DIM=Create(1.0)

def  Tampon(v,t):
      for n in range(len(v)): t[n]=t[n]+v[n]
      return t
   
def  freeTampon(t):
      for n in range(3): t[n]=0.0
      return t

def  TamponMoyen(t,f):      
      for n in range(3): t[n]/=len(f)
      return t

def  appliqueMoyenne(v,t):
      for n in range(len(v)): v[n]=t[n]
      return v

def docF(f0,f):
      try:
          f0.mat=f.mat
          f0.uv=f.uv 
          f0.col=f.col
          f0.image=f.image
          return f0
      except:
          pass    

def listConnecterFace(me,lebon):
               listf2v={}
               #tri des faces connectees aux sommets selectionnes                           
               for f in me.faces:
                 for v in f.v:
                   if v==lebon:
                      if v.index not in listf2v.keys():
                          listf2v[me.verts.index(v)]=[f]
                      elif f not in listf2v[me.verts.index(v)]:
                         listf2v[me.verts.index(v)].append(f)
               return listf2v

def creerFaceSupplementaire(me,lebon,listf2v):
               global t
               for f in listf2v[lebon.index]:
                  f0=Blender.NMesh.Face()
                  if result==3: t=freeTampon(t)
                  for v in f.v:
                     if result==3: t=Tampon(v,t)
                     if v!=lebon:
                        f0.append(v)
                     else:
                        if result==2:                           
                           nv=Blender.NMesh.Vert(lebon.co[0]+Nr()*decal,lebon.co[1]+Nr()*decal,lebon.co[2]+Nr()*decal)
                        else:
                           nv=Blender.NMesh.Vert(lebon.co[0],lebon.co[1],lebon.co[2])
                        me.verts.append(nv)
                        f0.append(me.verts[me.verts.index(nv)])
                        localise=me.verts.index(nv)                          
                     docF(f0,f)   
                     
                  if result==3:
                         t=TamponMoyen(t,f0.v)
                         me.verts[localise]=appliqueMoyenne(me.verts[localise],t)
                  me.faces.append(f0)                  
               del me.verts[me.verts.index(lebon)]
               for f in listf2v[lebon.index]:
                  del me.faces[me.faces.index(f)]
               return me

def collecte_edge(listf2v,me,lebon):
      back=0
      edgelist = []
      vertlist = []
      if DEBUG : print listf2v    
      for face in listf2v[lebon.index]:
          if len(face.v) == 4:
              vlist = [0,1,2,3,0]
          elif len(face.v) == 3:
              vlist = [0,1,2,0]
          else:
              vlist = [0,1]
          for i in range(len(vlist)-1):              
              vert0 = min(face.v[vlist[i]].index,face.v[vlist[i+1]].index)
              vert1 = max(face.v[vlist[i]].index,face.v[vlist[i+1]].index)              
              edgeinlist = 0
              if vert0==lebon.index or vert1==lebon.index:                 
                 for edge in edgelist:
                    if ((edge[0]==vert0) and (edge[1]==vert1)):
                        edgeinlist = 1
                        edge[2] = edge[2]+1
                        edge.append(me.faces.index(face))
                        break                  
                 if edgeinlist==0:
                    edge = [vert0,vert1,1,me.faces.index(face)]
                    edgelist.append(edge)
                    
      for  edge in edgelist:
        #print edge
        if len(edge)==4:
                   del edgelist[edgelist.index(edge)]
                           
      edges=len(edgelist)
      if DEBUG : print 'number of edges : ',edges," Edge list : " ,edgelist    
      return edges, edgelist     

MouseClickG= Blender.Draw.LEFTMOUSE
MouseClickD= Blender.Draw.RIGHTMOUSE
MouseClickM= Blender.Draw.MIDDLEMOUSE

mouse_x=1
mouse_y=1

x=1
y=1

debut=0

def D():
  size=Buffer(GL_FLOAT, 4)
  glGetFloatv(GL_SCISSOR_BOX, size)
  size= size.list 
  glColor3f(0.1, 0.1, 0.15)
  glRasterPos2f(10, size[3]-16)
  Text("Quit = Qkey")
  glRasterPos2f(10, size[3]-36)
  Text("Mouse to the Rigth = Increase")
  glRasterPos2f(10, size[3]-56)
  Text("Mouse to the Left = Decrease")

def E(evt,val):
 global mouse_x,x,pl,orig,me,debut
 global mouse_y,y, MouseClickG,MouseClickD,MouseClickM
 if (evt== QKEY): Exit()
 
 if (evt == MOUSEX): 
       mouse_x = val
       pos=x-mouse_x
       x=mouse_x
       if pos==0:
          pos=1
       deplace(pl,orig,abs(pos)/pos)
 
 if (evt == MOUSEY): mouse_y = val

def BE(evt):
 pass


def deplace(pl,orig,n):
    global me, OBJECT
    for p in pl:
       #print p, orig,len(me.verts) 
       me.verts[p].co[0]+=n*orig[0]
       me.verts[p].co[1]+=n*orig[1]
       me.verts[p].co[2]+=n*orig[2]
    me.update()
    Blender.Redraw()


def VertexBevel(result):
              global t,pl, orig,me,  SUBSURF
              unique=0
              for v in me.verts:
                  if v.sel==1:
                     lebon=v   
                     unique+=1

              if  unique==1:
                    edges=0
                    edgelist=[]
                    vertlist=[]
                    orig=lebon.no[:]
                    listf2v=listConnecterFace(me,lebon)
                    edges, edgelist = collecte_edge(listf2v,me,lebon)
                    for f in listf2v[lebon.index]:
                       f0=Blender.NMesh.Face()
                       for v in f.v:
                          if v!=lebon:
                             f0.append(v)
                          else:
                              nv=Blender.NMesh.Vert(lebon.co[0],lebon.co[1],lebon.co[2])                           
                              nv.sel=1 
                              me.verts.append(nv)
                              f0.append(me.verts[me.verts.index(nv)])
                              for e in edgelist:
                                 if e[-1]==me.faces.index(f) or e[-2]==me.faces.index(f):
                                     if me.verts.index(nv) not in e:
                                        e.insert(0,me.verts.index(nv))
                       docF(f0,f)               
                       me.faces.append(f0)                       
                       vertlist.append([me.verts.index(nv),me.faces.index(f)])
                    for e in  edgelist :
                        del e[e.index(lebon.index)]
                        f0=Blender.NMesh.Face()
                        for n in range(3): 
                           f0.v.append(me.verts[e[n]])
                        me.faces.append(f0);
                     
                    for ve in vertlist:
                         t=freeTampon(t)
                         for v in me.faces[ve[1]].v:
                              t=Tampon(v,t)                     
                         t=TamponMoyen(t,me.faces[ve[1]].v)
                         ve.append(t[:])  	
                         me.verts[ve[0]]=appliqueMoyenne(me.verts[ve[0]],t)

                    def swap(x,y):
                        return y,x

                    p=[[edgelist[0][0],edgelist[0][1]]] 
                    while len(p)!=len(edgelist): 
                      for n in range(1,len(edgelist)) :
                           if p[-1][1]== edgelist[n][0]:
                              p.append([edgelist[n][0],edgelist[n][1]])
                              n+=1
                           elif p[-1][1]== edgelist[n][1]:
                              edgelist[n][0],edgelist[n][1]=swap(edgelist[n][0],edgelist[n][1])
                              p.append([edgelist[n][0],edgelist[n][1]])
                              n+=1
                    if len(p)%2==0:
                          P0=p[:(len(p))/2] ; P1=p[len(p)/2:]; P1.reverse()
                          for s in range(len(P0)-1):
                             f0=Blender.NMesh.Face()
                             table=[P0[s][0],P0[s][1],P1[s+1][0],P1[s+1][1]]
                             for t in table:f0.v.append(me.verts[t])
                             me.faces.append(f0) 
                    elif len(p) >3 :   
                          P0=p[:(len(p)-1)/2];P1=p[(len(p)-1)/2:-1]; P1.reverse()
                          for s in range(len(P0)-1):
                             f0=Blender.NMesh.Face()
                             table=[P0[s][0],P0[s][1],P1[s+1][0],P1[s+1][1]]
                             for t in table:f0.v.append(me.verts[t])
                             me.faces.append(f0) 
                          f0=Blender.NMesh.Face()
                          table=[p[-1][0],P0[0][0],P1[-1][1]]
                          for t in table:f0.v.append(me.verts[t])
                          me.faces.append(f0) 

                    elif len(p) ==3 :   
                          if DEBUG :print P0,P1      
                          f0=Blender.NMesh.Face()
                          table=[p[0][0],p[0][1],p[1][1]]
                          for t in table:f0.v.append(me.verts[t])
                          me.faces.append(f0) 

                    for f in listf2v[lebon.index]:
                       del me.faces[me.faces.index(f)]  
                    del me.verts[me.verts.index(lebon)]
                    me.update()

                    if me.mode&Blender.NMesh.Modes['SUBSURF']:
                       me.mode-=Blender.NMesh.Modes['SUBSURF']
                       SUBSURF=1 
                       me.update()
                       OBJECT[0].makeDisplayList() 

                    if result==5:
                       pl=[]
                       for s in me.verts:
                           if s.sel==1:
                              pl.append(s.index)
                       Blender.Draw.Register(D,E,BE)

                    """
                    if SUBSURF==1 :
                       me.mode+=Blender.NMesh.Modes['SUBSURF']
                       me.update()
                       OBJECT[0].makeDisplayList() 
                    """  
              else:
                  name = " It could leave only one selected vertex %t | ok %x1 ?"
                  result = Blender.Draw.PupMenu(name)

OBJECT=Blender.Object.GetSelected()

if len(OBJECT)!=0:
   if  OBJECT[0].getType()=='Mesh':
       name = "Un-Weld, OK ? %t| unbind points %x1| with noise ? %x2| middle face ? %x3| static bevel vertex ? %x4| moving bevel vertex ? %x5|"
       result = Blender.Draw.PupMenu(name)
       if result:
           me=OBJECT[0].getData()
           unique=0
           if result<4:
              #tri des sommets selectionnes
              for v in me.verts:
                  if v.sel==1:
                    lebon=v    
                    if DEBUG : print lebon
                    #dictionnaires des faces connectees aux sommets selectionnes
                    listf2v=listConnecterFace(me,lebon)
                    me=creerFaceSupplementaire(me,lebon,listf2v)
                    OBJECT[0].link(me)
                    OBJECT[0].makeDisplayList()
           else:
               VertexBevel(result)
               OBJECT[0].makeDisplayList()
                    
   else:
       name = "Nothing to do! Are you sure ?"
       result = Blender.Draw.PupMenu(name)

                                   	
