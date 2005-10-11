#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Dispaint'
Blender: 237
Group: 'Mesh'
Tip: 'use vertex paint color value to modify shape displacing vertices along normal'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_displacementpainting.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "237"

__bpydoc__ = """\
This script displaces mesh vertices according to vertex color values.

Usage:

Select the mesh, enter Edit Mode and run this script to open its GUI.  Options
include setting mode, orientation, size and number of repetitions of the
displacement.  You can enter Vertex Paint mode and alternate applying
displacements and painting parts of the mesh.

Orientation includes vertex normals, local coordinates and noise (you may need
to resize the scripts window to view the noise menu below the "Last Error:"
line.  This menu lets you define noise type from the many options available in
Blender.

Notes:<br>
    The "Create" button will make at any time a copy of the active mesh in its
current state, so you can keep it and continue working on the copy;<br>
    One of the great possible uses of this script is to "raise" terrain from a
subdivided plane, for example, with good control of the process by setting
options, defining orientation and alternating vertex painting with
displacements.
"""

#----------------------------------------------
# jm soler, displacement paint 03/2002 - > 05/2004:  disp_paintf
# Terrain Noise added suugered by Jimmy Haze  
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_displacementpainting.htm
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#--------------------------------------------- 
# ce script est proposé sous licence GPL pour etre associe
# a la distribution de Blender 2.33
#----------------------------------------------
# this script is released under GPL licence
# for the Blender 2.33 scripts package
#----------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003, 2004: Jean-Michel Soler
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
#  09/07/04 : Noise functions based on a piece of script by Jimmy Haze.
# --------------------------------------------------------------------------

import Blender
from Blender import *
from Blender.Draw import *
from Blender.BGL import *
from Blender.Noise import *
from Blender.Scene  import *
from Blender.Window  import *
sc=Scene.getCurrent()

# niveau du deplacement
ng=0.5

# noise default
NOISE=1

# profondeur des couleurs primaires rgb
maxcol=255.0*3

# limitation de la zone de travail sur le
# le materiau numer mat du tableau d'indices 
# des materiaux. Par defaut mat =-1 ce qui signifie 
# que toute la surface est traitee
mat=[]
vindex=[]
ORIName=''
NEWName=''
ERROR=0
TextERROR=''

E_EXIT     = 1
E_MODE     = 2
E_ORIENT   = 3
E_NSIZE    = 4
E_REPEAT   = 5
E_ACTION   = 16
E_CREATE   = 17
E_DOCMAT   = 24
E_MATVAL   = [32,33,34,35,36,37,38,39,40,41,42,43,44]
E_AXESEL   = 45
E_AXESELX  = 46
E_AXESELY  = 47
E_AXESELZ  = 48


E_NOISEME  = 49
E_NOISEH   = 50
E_NOISELAC = 51
E_NOISEOCT = 52
E_NOISEOFF = 53
E_NOISEBAS = 54
E_NOISEVAL=[E_NOISEH,E_NOISELAC,E_NOISEOCT,E_NOISEOFF,E_NOISEBAS]
E_NOISEDIM = 55

E_GETCOLORS = 56
E_UVCOLORS =  57
E_SAVECOLORS = 58
B_SAVECOLORS = 0

E_RESTCOLORS = 60
V_RESTCOL=0
F_RESTCOL=0

BUF_COLORS=[]

RVBA_VALUE=61
RVBA_VERTICES=62
RVBA_FACES=63

ExitTIP="Exit from this script session "
CreateTIP="Create a new copy of the selected shape"
ActionTIP="Do the current selected actions"

UVCOLORSTIP="Get colrs from first available UV image "
GETCOLORSTIP="Get color from textures "
REPEATTIP="Replay the same action with new values ."

def copy_transform(ozero,Obis):
         Obis.setSize(ozero.getSize());
         Obis.setEuler(ozero.getEuler());
         Obis.setLocation(ozero.getLocation())
         return Obis

def traite_face(f):
      global vindexm, ng, NOISE, NOISEDIM
      global H,lacunarity,octaves,offset,basis

      if ORIENTMenu.val==1:
         for z in range(len(f.v)):
            c=0.0
            if vindex[f.v[z].index]!=0:
               c=float(f.col[z].r+f.col[z].b+f.col[z].g)/maxcol*ng/vindex[f.v[z].index]
            else:
              c=0

            f.v[z].co[0]=f.v[z].co[0]+f.v[z].no[0]*c
            f.v[z].co[1]=f.v[z].co[1]+f.v[z].no[1]*c 
            f.v[z].co[2]=f.v[z].co[2]+f.v[z].no[2]*c

      elif ORIENTMenu.val==2:
          for z in range(len(f.v)): 
            c=0.0
            if vindex[f.v[z].index]!=0:
               c=float(f.col[z].r+f.col[z].b+f.col[z].g)/maxcol*ng/vindex[f.v[z].index]
            else:
              c=0
            for t in range(3):
               if TAXEList[1][t].val==1:
                  f.v[z].co[t]=f.v[z].co[t]+c
 
      elif ORIENTMenu.val==3 and NOISE<9:
         for z in range(len(f.v)):
            c=0.0
            if vindex[f.v[z].index]!=0:
               nx=f.v[z].co[0]/NOISEDIM
               ny=f.v[z].co[1]/NOISEDIM
               nz=f.v[z].co[2]/NOISEDIM
               nn = ng * noise((nx,ny,nz),NOISE)
               c=float(f.col[z].r+f.col[z].b+f.col[z].g)/maxcol*nn/vindex[f.v[z].index]
            else:
              c=0
            f.v[z].co[0]=f.v[z].co[0]+f.v[z].no[0]*c
            f.v[z].co[1]=f.v[z].co[1]+f.v[z].no[1]*c
            f.v[z].co[2]=f.v[z].co[2]+f.v[z].no[2]*c

      elif ORIENTMenu.val==3 and NOISE==9:
         for z in range(len(f.v)):
            c=0.0
            if vindex[f.v[z].index]!=0:
               nx=f.v[z].co[0]/NOISEDIM
               ny=f.v[z].co[1]/NOISEDIM
               nz=f.v[z].co[2]/NOISEDIM
               nn = ng * cellNoise((nx,ny,nz))
      	       c=float(f.col[z].r+f.col[z].b+f.col[z].g)/maxcol*nn/vindex[f.v[z].index]
            else:
              c=0
            f.v[z].co[0]=f.v[z].co[0]+f.v[z].no[0]*c
            f.v[z].co[1]=f.v[z].co[1]+f.v[z].no[1]*c
            f.v[z].co[2]=f.v[z].co[2]+f.v[z].no[2]*c

      elif ORIENTMenu.val==3 and NOISE==10:
         for z in range(len(f.v)):
            c=0.0
            if vindex[f.v[z].index]!=0:
               nx=f.v[z].co[0]/NOISEDIM
               ny=f.v[z].co[1]/NOISEDIM
               nz=f.v[z].co[2]/NOISEDIM
               nn = ng * heteroTerrain((nx,ny,nz),H,lacunarity,octaves,offset,basis)
               c=float(f.col[z].r+f.col[z].b+f.col[z].g)/maxcol*nn/vindex[f.v[z].index]
            else:
              c=0
            f.v[z].co[0]=f.v[z].co[0]+f.v[z].no[0]*c
            f.v[z].co[1]=f.v[z].co[1]+f.v[z].no[1]*c
            f.v[z].co[2]=f.v[z].co[2]+f.v[z].no[2]*c

 
def paint():
      global MODEMenu, vindex,ng, mat, ORIName, NEWName
      global ERROR, TextERROR
         
      Me=Object.GetSelected()
      if Me!=[]:
         if Me[0].getType()=='Mesh':   
                    
               vindex=[]
               ORIName=Me[0].getData().name
               me=NMesh.GetRaw(Me[0].getData().name)
                  
               try:  
                   for m in me.verts:
                      vindex.append(0)
                     
                   for f in me.faces:
                       for v in f.v:
                               if MODEMenu.val!=2:
                                  if MODEMenu.val==1:    
                                     vindex[v.index]+=1
                                  else:
                                     if v.sel==1:
                                         vindex[v.index]+=1                                       
                               else:
                                  #print mat 
                                  if f.mat in mat:
                                     vindex[v.index]+=1
                   for f in me.faces:
                     if MODEMenu.val==2: 
                       if f.mat in mat:
                          traite_face(f) 
                     else:
                          traite_face(f)
                   #Me[0].link(me)
                   me.update()
                   Me[0].makeDisplayList()
               except:
                  ERROR=2
                  TextERROR='No color on this Object.' 
                  
def NEWMEcreation(obj):
       
      if obj.getType()=='Mesh':
         nomdelobjet="";
         objnumber=-1; namelist=[]
         OBJ=Object.Get()

         for ozero in OBJ:
            if ozero.getType()=='Mesh': 
                namelist.append(ozero.getData().name)

         ozero=obj
         nomdelobjet=ozero.getName()
         Mesh=Blender.NMesh.GetRawFromObject(nomdelobjet)
         name=obj.getData().name
         n=0; name2=name[:];ok=0  

         while ok==0:
             for n0 in namelist:
                 if n0.find(name2)==0:
                    ok=0;name2=name[0:name.find('.')+1]+'%s'%(n+1) 
                 else: ok=1
                 n+=1

         Mesh.name=name2
         Obis = Blender.NMesh.PutRaw(Mesh,name2)
         copy_transform(ozero,Obis)
         Obis.makeDisplayList()

def DOCMat_list(TMATList):
    global mat    
    Me=Object.GetSelected()
    if Me!=[]:
       if Me[0].getType()=='Mesh':
            me=NMesh.GetRaw(Me[0].getData().name) 
            if len(me.materials)!=0: 
                n=0 
                for mat in me.materials:
                    TMATList[1][n][0]=mat.R
                    TMATList[1][n][1]=mat.G
                    TMATList[1][n][2]=mat.B
                    n+=1
                TMATList[0]=n
            else:
              TMATList[0]=0
            return TMATList
 
MOname = "MODE MENU %t|Normal %x1|Material %x2|Selected %x3| Find color %x4"
MOname_doc=["",
            "Displace all vertices",
            "Displace vertices only on selected materials . ",
            "Displace only selected vertices .",
            "Try to find and set selected the vertices with this color."]

ORname = "ORIENT MENU %t|From Normal %x1|Local Axes %x2| Noise %x3"
ORname_doc=["",
            "Use normal orientation to calculate displacement",
            "Use selected axes value to calculate displacement",
            "Blend the color value with Nosie values to calculate the displacement"]

NOname = "NOISE MENU %t|BLENDER %x1|STDPERLIN %x2|\
NEWPERLIN %x3|VORONOI_F1%x4|VORONOI_F2%x5|\
VORONOI_F3%x6|VORONOI_F4%x7|VORONOI_F2F1%x8|\
VORONOI_CRACKLE%x9|CELLNOISE%x10|HETEROTENOISE%x11"

MODEMenu = Create(1)
ORIENTMenu = Create(1)
NOISEMenu = Create(1)

NSIZE = Create(1.0)
TDOCMat = Create(0)
NRepeat = Create(1)

H=1.0
lacunarity=2.0
octaves=5.0
offset=1.0
basis=3

NOISEDIM=4
NOISEDIMbout=Create(NOISEDIM)
HBout=Create(H)
lacunarityBout=Create(lacunarity)
octavesBout=Create(octaves)
offsetBout=Create(offset)
basisBout=Create(basis)


noiseTYPE={0:'BLENDER',
           1:'STDPERLIN',
           2:'STDPERLIN',
           3:'NEWPERLIN',
           4:'VORONOI_F1',
           5:'VORONOI_F2',
           6:'VORONOI_F3',
           7:'VORONOI_F2F1',
           8:'VORONOI_CRACKLE',
           9:'CELLNOISE'}        
 
TMATList= [0,[],[]] 

for t in range(16):
    TMATList[1].append([0.0,0.0,0.0])
    TMATList[2].append(Create(0))

TAXEList=[['X','Y','Z'],[]]
for t in range(3):
    TAXEList[1].append(Create(0))

glCr=glRasterPos2d
glCl3=glColor3f
glCl4=glColor4f
glRct=glRectf

def triangle(a,b,c):
  glBegin(GL_TRIANGLES);
  glColor3f(a[2],a[3],a[4])
  glVertex2f(a[0],a[1]);
  glVertex2f(b[0],b[1]);
  glVertex2f(c[0],c[1]);
  glEnd();

def triangleFcolor(a,b,c):
  glBegin(GL_TRIANGLES);
  glColor4f(a[2],a[3],a[4],a[5])
  glVertex2f(a[0],a[1]);
  glColor4f(b[2],b[3],b[4],a[5])
  glVertex2f(b[0],b[1]);
  glColor4f(c[2],c[3],c[4],a[5])
  glVertex2f(c[0],c[1]);
  glEnd();

def Ltriangle(a,b,c,LC=0.5):
  TL=[a,b,c,a] 	
  for v in [0,1,2] :
    glBegin(GL_LINES);
    glColor3f(LC,LC,LC)
    glVertex2f(TL[v][0],TL[v][1]);
    glVertex2f(TL[v+1][0],TL[v+1][1]);
    glEnd();


def carreFcolor(a,b,c,d):
    triangleFcolor(a,b,c)
    triangleFcolor(a,c,d)

RVBA=[Create(255),Create(255),Create(255),Create(255),Create(0)]

#         _*_ p1 
#       _/   \_
#     _/       \_
#    /           \_
# p0*_             /* p2
#   | \_         _/ |
#   |   \_     _/   |
#   |     \_ _/     |
#   |       * p3    |
#   |       |       |
#   *_      |      /* p4
# p6  \_    |    _/ 
#       \_  |  _/   
#         \_|_/     
#           * p5

def flatcolorcube(r,g,b,a,m,x,y):
    h0=60
    v0=40
    A=[x,	y,		(r-m)/255.0,g/255.0,b/255.0,a/255.0]     #p0 
    B=[x+h0,y-v0,	r/255.0,g/255.0,b/255.0,a/255.0]         #p3
    c=[x+h0*2,y,  	r/255.0, g/255.0,  (b-m)/255.0,a/255.0]  #p2
    d=[x+h0,y+v0,	(r-m)/255.0,g/255.0,(b-m)/255.0,a/255.0] #p1
    carreFcolor(A,B,c,d)


    A=[x,y,(r-m)/255.0,g/255.0,b/255.0,a/255.0]				#p0
    B=[x+h0,y-v0,r/255.0,g/255.0,b/255.0,a/255.0]				#p3	
    c=[x+h0,y-v0*2.5,  r/255.0, (g-m)/255.0,  b/255.0,a/255.0]    #p5
    d=[x,y-v0*1.5,(r-m)/255.0,(g-m)/255.0,b/255.0,a/255.0]      #p6
    carreFcolor(A,B,c,d)    

    d=[x+h0,y-v0,r/255.0,g/255.0,b/255.0,a/255.0]			 #p3
    A=[x+h0*2,y,r/255.0,g/255.0,(b-m)/255.0,a/255.0]			 #p2
    B=[x+h0*2,y-v0*1.5,  r/255.0, (g-m)/255.0,(b-m)/255.0,a/255.0] #p4
    c=[x+h0,y-v0*2.5,r/255.0,(g-m)/255.0,b/255.0,a/255.0]        #p5
    carreFcolor(A,B,c,d)    

def col_egal2col(col,RVBA):
    eps=RVBA[4].val
    if ( (RVBA[0].val-col[0]>=0 and RVBA[0].val-col[0]<=eps) and
         (RVBA[1].val-col[1]>=0 and RVBA[1].val-col[1]<=eps) and
         (RVBA[2].val-col[2]>=0 and RVBA[2].val-col[2]<=eps) and       
	       (RVBA[3].val-col[3]>=0 and RVBA[3].val-col[3]<=eps) ) :
       #print 'ok',col, [RVBA[n].val-col[n] for n in 0,1,2,3]
       return 1
    else:
       #print 'not',col, [RVBA[n].val-col[n] for n in 0,1,2,3]
       return 0

def select_bycolors(TYPE,RVBA):
  global RVBA_VERTICES, RVBA_FACES
  SEL = Blender.NMesh.FaceFlags['SELECT']
  try: 
    ME=Blender.Scene.getCurrent().getActiveObject().getData()
    VC={}
    for f in ME.faces:
        for v in f.v:
             try:
	             VC[v].append(f)
             except:
                 VC[v]=[f]
             #print '.', 
    for C in VC.iteritems():
        color=[0,0,0]
        for f in C[1]:
            col=f.col[f.v.index(C[0])]
            col=[col.r,col.g,col.b,col.a]
            if col_egal2col(col,RVBA):
               if TYPE== RVBA_VERTICES: 
                  C[0].sel=1
               else:
                  f.sel=1
                  f.flag |= SEL
        #VC[C[0]].append(color[:])
    ME.update()		
  except:
	  pass 
	
def draw():
    global MODEMenu, NSIZE, TDOCMat,TMATList, TAXEList
    global mat, ORIName, NEWName, ORIENTMenu 
    global NRepeat, ERROR, TextERROR , NOISE, NOISEMenu
    global NOISEDIMbout,NOISEDIM, RVBA,RVB_VALUE, RVBA_VERTICES 
    global HBout,lacunarityBout,octavesBout,offsetBout,basisBout
    global noiseTYPE, ExitTIP, CreateTIP, ActionTIP, E_GETCOLORS
    global E_UVCOLORS, UVCOLORSTIP, GETCOLORSTIP, REPEATTIP,RVBA_FACES
    global E_SAVECOLORS, B_SAVECOLORS, E_RESTCOLORS, MOname_doc, ORname_doc

    size=Buffer(GL_FLOAT, 4)
    glGetFloatv(GL_SCISSOR_BOX, size)
    size= size.list

    for s in [0,1,2,3]: size[s]=int(size[s])
	   
    glClearColor(0.72,0.72,0.72,1.0)
    glClear(GL_COLOR_BUFFER_BIT)

    glColor3f(0.66,0.66,0.66)
    glRectf(4,size[3]-4,404,size[3]-32 )

    glColor3f(0.76,0.76,0.76)
    glRectf(4,size[3]-32,404,size[3]-294 )

    triangle([4+9,size[3],0.72,0.72,0.72],
             [4,size[3],],
             [4,size[3]-9])

    triangle([404-9,size[3],0.72,0.72,0.72],
             [404,size[3],],
             [404,size[3]-9])

    triangle([404,size[3]-294,.72,0.72,0.72],
             [404,size[3]-294+9,],
             [404-9,size[3]-294])

    triangle([4,size[3]-294,.72,0.72,0.72],
             [4,size[3]-294+9,],
             [4+9,size[3]-294])

    glColor3f(1.0,1.0,1.0)
    glRasterPos2f(20, size[3]-15)
    Text("Script Python de displacement painting")

    glRasterPos2f(20, size[3]-28)
    Text("Jean-michel Soler, Aout 2005")

   
    n0=70
    n1=55
    if MODEMenu.val<4 : 
       Button("Create"                ,E_CREATE  ,5  ,size[3]-n0+11  ,60 ,20,CreateTIP)
       Button("Action"                ,E_ACTION  ,5  ,size[3]-n0-11  ,60 ,20,ActionTIP)
       NRepeat=Number("repeat"        ,E_REPEAT   ,5  ,size[3]-n0-56     ,75 ,20, NRepeat.val,1,10,REPEATTIP)    

    Button("Exit"                  ,E_EXIT    ,5  ,size[3]-n0-32  ,60 ,20,ExitTIP)   
    Button("Tex colors"            ,E_GETCOLORS   ,5  ,size[3]-n0-80  ,75 ,20,GETCOLORSTIP)
    Button("UV colors"             ,E_UVCOLORS    ,5  ,size[3]-n0-102  ,75 ,20,UVCOLORSTIP)
    if B_SAVECOLORS :
         Button("Rest colors"             ,E_RESTCOLORS    ,5  ,size[3]-n0-146  ,75 ,20,UVCOLORSTIP)
    else:
         Button("Save colors"           ,E_SAVECOLORS   ,5  ,size[3]-n0-124  ,75 ,20,GETCOLORSTIP)


    
    glColor3f(0.0,0.0,0.0)
    glRasterPos2f(80  ,size[3]-n0+24)
    Text("MODE")

    MODEMenu= Menu(MOname,          E_MODE  ,80  ,size[3]-n0 ,100,20, MODEMenu.val, MOname_doc[MODEMenu.val])

    if MODEMenu.val==2:
       TDOCMat=Toggle("Doc Mat"     ,E_DOCMAT  ,180  ,size[3]-n0 ,60 ,20,TDOCMat.val)    
       if TDOCMat.val==1:
             #print TMATList 
             for t in range(TMATList[0]):
                 glCl3(TMATList[1][t][0],
                       TMATList[1][t][1],
                       TMATList[1][t][2]) 
                 
                 if t<=7:
                    glRct(80+t*40,
                    size[3]-n0-60,
                    80+t*40+40,
                    size[3]-n0-60+40)
                    TMATList[2][t]=Toggle("%s"%(t+1) , 32+t ,80+t*40+5  ,size[3]-n0-50  ,30 , 20,TMATList[2][t].val)
                 else: 
                    glRct(80+(t-8)*40,
                    size[3]-n0-50-50,
                    80+(t-8)*40+40,
                    size[3]-n0-60)
                    TMATList[2][t]=Toggle("%s"%(t+1) , 32+t ,80+(t-8)*40+5  ,size[3]-n0-45*2  ,30 , 20,TMATList[2][t].val)
                 
    glColor3f(1.0,0.3,0.0)
    glRasterPos2f(80+40+5  ,size[3]-n0-110)
    if ERROR>1:
         Text('Last error : '+TextERROR)
    else:
         Text('Last error :                      ')

    glColor3f(0.0,0.0,0.0)
    glRasterPos2f(240  ,size[3]-n0+24)

    if MODEMenu.val<4:
            Text("ORIENTATION")
            ORIENTMenu= Menu(ORname,        E_ORIENT    ,240  ,size[3]-n0 ,100,20, ORIENTMenu.val, ORname_doc[ORIENTMenu.val])
            if ORIENTMenu.val==2 :
               for t in [0,1]:
                  TAXEList[1][t]=Toggle("%s"%TAXEList[0][t],
                                 E_AXESEL+t, 
                                 240+100+t*30+2 , size[3]-n0+10  ,28 , 18,
                                 TAXEList[1][t].val)
                  TAXEList[1][2]=Toggle("%s"%TAXEList[0][2],
                                 E_AXESEL+2, 
                                 int(240+100+.5*30+2) , size[3]-n0-10  ,28 , 18,
                                 TAXEList[1][2].val)
            if ORIENTMenu.val==3 :
               glRasterPos2f(240  ,size[3]-n0-120-4)
               Text("NOISE")
               NOISEMenu= Menu(NOname,         E_NOISEME    , 240  ,size[3]-n0-148 ,110,20, NOISEMenu.val, "NOISE menu.")
               NOISEDIMbout=Number(" Dim: "     ,E_NOISEDIM   , 240  ,size[3]-n0-172 ,110,20, NOISEDIMbout.val, 1,100)
               if NOISEMenu.val==11:
                  basisBout=Slider(noiseTYPE[basisBout.val],   
                                                      E_NOISEBAS  ,40  ,size[3]-n0-178 ,175,20, basisBout.val, 0,9,)
                  HBout= Slider("H",                  E_NOISEH    ,40  ,size[3]-n0-198 ,175,20, HBout.val, -2.0,+2.0,0,)
                  lacunarityBout=Slider("lacunarity", E_NOISELAC  ,40  ,size[3]-n0-218 ,175,20, lacunarityBout.val, -4.0,+4.0,0,)
                  octavesBout=Slider("octave",        E_NOISEOCT  ,219  ,size[3]-n0-198 ,175,20, octavesBout.val, -10.0,+10.0,0,)
                  offsetBout=Slider("offset",         E_NOISEOFF  ,219 ,size[3]-n0-218 ,175,20, offsetBout.val, -5.0,+5.0,0,)
            NSIZE= Slider("Disp Size",      E_NSIZE  ,80  ,size[3]-n0-20 ,260,20, NSIZE.val, -4.0,+4.0,0,"SIZE.")


    else:
        # degrades de couleurs
        glShadeModel(GL_SMOOTH)
        #transparence  
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

        RVBA[0]=Slider("Red  :",      RVBA_VALUE , 105  ,size[3]-n0-25 ,280,20, RVBA[0].val, 0,255,0,"")
        RVBA[1]=Slider("Green  :",    RVBA_VALUE , 105  ,size[3]-n0-47 ,280,20, RVBA[1].val, 0,255,0,"")
        RVBA[2]=Slider("Blue  :",     RVBA_VALUE , 105  ,size[3]-n0-69 ,280,20, RVBA[2].val, 0,255,0,"")
        RVBA[3]=Slider("Alpha  :",    RVBA_VALUE , 105  ,size[3]-n0-91 ,150,20, RVBA[3].val, 0,255,0,"")
        RVBA[4]=Slider("margin  :",   RVBA_VALUE , 105  ,size[3]-n0-113 ,150,20, RVBA[4].val, 0,255,0,"")
        flatcolorcube(RVBA[0].val,
                      RVBA[1].val,
                      RVBA[2].val,
                      RVBA[3].val,
                      RVBA[4].val,
                      270,size[3]-n0-120)
        
        Button("Vertex"                ,RVBA_VERTICES  ,5  ,size[3]-n0-148  ,75 ,20,CreateTIP)
        Button("Faces"                 ,RVBA_FACES  ,5  ,size[3]-n0-169  ,75 ,20,ActionTIP)

        
def on_MESH():
    Me=Object.GetSelected()
    if Me!=[] and Me[0].getType()=='Mesh':                 
       editmode = Window.EditMode()   
       if editmode: Window.EditMode(0)
       return 1,Me[0].getData()
    else: 
	   return 0, None

def event(evt, val):    
    if (evt== QKEY and not val): Exit()

def bevent(evt):
    global MODEMenu, NSIZE, ng, TMATList
    global mat, ORIENTMenu, NRepeat, TAXEList 
    global ERROR,TextERROR, NOISE, NOISEMenu, NOISEDIMbout,NOISEDIM
    global HBout,lacunarityBout,octavesBout,offsetBout,basisBout
    global H,lacunarity,octaves,offset,basis, E_RESTCOLORS, RVBA_VERTICES
    global E_GETCOLORS, E_UVCOLORS, E_SAVECOLORS, B_SAVECOLORS
    global V_RESTCOLORS, F_RESTCOLORS, BUF_COLORS, RVBA, RVBA_FACES

    if   (evt== E_EXIT):
        Exit()
    elif   (evt== E_ACTION):
       for n in range(NRepeat.val):
          paint()
    elif   (evt== E_NSIZE):
       ng=NSIZE.val
    elif   (evt== E_DOCMAT) or (evt in E_MATVAL):
      Me=Object.GetSelected()

      if Me!=[]:
         if Me[0].getType()=='Mesh':   
            TMATList=DOCMat_list(TMATList)
            mat=[]
            for TMat in TMATList[2]:
               if TMat.val==1.0:
                   mat.append(TMATList[2].index(TMat))  
            ERROR=0
         else:
            ERROR=1
            TextERROR='Selected Object is not a mesh.'    
      else:
          ERROR=1
          TextERROR='No Selected Object.'  
    elif   (evt== E_CREATE):
           NEWMEcreation(Blender.Object.GetSelected()[0])
           Blender.Draw.Redraw()
           ERROR=1
           TextERROR='No Selected Object.'
    elif   (evt== E_NOISEME):
       NOISE=NOISEMenu.val-1
    elif   (evt in E_NOISEVAL):
       H=HBout.val
       lacunarity=lacunarityBout.val
       octaves=octavesBout.val
       offset=offsetBout.val
       basis=basisBout.val
    elif (evt== E_NOISEDIM):
           NOISEDIM=NOISEDIMbout.val
    elif (evt == E_GETCOLORS):
            OK,MESH=on_MESH()
            if OK: MESH.update(1,0,1)
    elif (evt == E_UVCOLORS):
       OK,MESH=on_MESH()
       if OK and MESH.hasFaceUV():
          for f in MESH.faces:
              if f.image:
                 im=Blender.Image.Get(f.image.name)
                 break
          imX,imY = im.getMaxXY()
          for f in MESH.faces:  
              for uv in  f.uv:
                 color=[int(c*255.0) for c in im.getPixelF(abs(uv[0]*imX%imX), abs(uv[1]*imY%imY))]
                 f.col[f.uv.index(uv)].r=color[0]
                 f.col[f.uv.index(uv)].g=color[1]
                 f.col[f.uv.index(uv)].b=color[2]
                 f.col[f.uv.index(uv)].a=color[3]
          MESH.update()
    elif (evt == E_SAVECOLORS):
          OK,MESH=on_MESH()
          print OK, MESH
          if OK and (MESH.hasFaceUV() or MESH.hasVertexColours()):
             F_RESTCOLORS=1
             for f in MESH.faces:
               b=[MESH.faces.index(f)]  
               for c in  f.col:
                   b.append([c.r,c.g,c.b,c.a])
               BUF_COLORS.append(b) 
             B_SAVECOLORS  =  1
          else: 
             B_SAVECOLORS  =  0
    elif (evt == E_RESTCOLORS):
          OK,MESH=on_MESH()
          print  F_RESTCOLORS, len(BUF_COLORS),len(MESH.faces)
          if  OK and F_RESTCOLORS==1 and len(BUF_COLORS)==len(MESH.faces):
             for b in  BUF_COLORS:
                ncol=0
                for c in  MESH.faces[b[0]].col :
                    print b[ncol+1]
                    c.r,c.g,c.b,c.a= b[ncol+1]
                    ncol+=1
             F_RESTCOLORS=0
             B_SAVECOLORS = 0
             BUF_COLORS=[]
             MESH.update()
    elif (evt == RVBA_VERTICES or evt == RVBA_FACES):
        select_bycolors(evt,RVBA)
    Blender.Draw.Redraw()
Register(draw, event, bevent)
