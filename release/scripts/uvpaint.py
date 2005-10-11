#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'UVpainter'
Blender: 232
Group: 'UV'
Tip: 'Use vertex paint color value to fill uvmapping'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_uvpainting.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.8 08/2005"

__bpydoc__ = """\
This script "paints" uv-mappings with the model's vertex colors.

Usage:

With this script you can export uv-maps filled with vertex colors to TARGA
(.tga) images.  To use it the mesh must have proper uv coordinates assigned
in UV Face Select Mode.  And to fill the projected faces with color, the mesh
can also have material(s) and vertex colors painted on it.

The script has a GUI with a preview of the results and options like drawing
lines or not, defining size, etc.  You can paint vertex colors in the mesh and
see the uv-map updated in the script's window.

Notes:<br>
    Material's rgb color is also used to fill the uv-map;<br>
    If there are no vertex colors or texture faces in the mesh and you press
the "Make" VColors button in the edit mesh buttons win, the current light setup
is saved as vertex colors for the model;<br>
    Check the script's homepage for example images.

Short keys  Documentation

KEYS 
M : dipslay GUI Menu
D : Set/Unset Documentation
S : Save current window content
       in a tga file
Q or ESC : Exit
T : Set/Unset Transparency
L : Set/Unset lines
E : Set/Unset outline
B : Set lines color to Black 
W : Set lines color to white
ARROW   : displace model on 
         UP/DOWN/LEFT/RIGHT side
PADPLUS : increase ZOOM 
PADMINUS   : decrease  ZOOM 	
HOME    : cancel display modifs

Mouse button
RIGHTMOUSE : same as  arrows
	
"""

# $Id$
#
#----------------------------------------------
# uvpainter script (c) 04/2004  jean-michel soler
# http://jmsoler.free.fr/util/blenderfile/py/UVpaint05.zip
# this script is released under GPL licence
# for the Blender 2.33 scripts distribution
#----------------------------------------------
# Official page :
# http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_uvpainting.htm
# Communicate problems and errors on:
# http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender 
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_uvpainting.htm
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#--------------------------------------------- 
# ce script est proposé sous licence GPL pour etre associe
# a la distribution de Blender 2.33 et suivant
# --------------------------------------------------------------------------
# this script is released under GPL licence
# for the Blender 2.33 scripts package
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) 2003, 2004: Jean-Michel Soler 
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

import Blender
from Blender.Draw import *
from Blender.BGL import *
from Blender.NMesh import *

try:
  import nt
  os=nt
except:
   import posix
   os=posix

def exist(path):
       try:
        pth=os.stat(Blender.sys.dirname(path))  
       except:
        return 0
       return 1 

loc0= Blender.Get ("filename").replace('\\','/')
loc0=loc0[:loc0.rfind('/')]
	
loc2=loc0+'/'+'test00.tga' 


mouse_x,mouse_y=0,0
mouse_xr=1
mouse_yr=1
POS=[0,0]
ANC=[0,0]
XY=[0,0]
size=[]
sel=0
X,Y=0,0
TRANSP,EMPTY,DOCU=0,0,0
MENU, SAVE =1,0

glCr=glRasterPos2d
glCl3=glColor3f
glCl4=glColor4f
glRct=glRectf

LC=1.0

xlimit=0
selmatlist=[]
LIM=[0.0,0.0,0.0,0.0]
NOLIM=1
if not NOLIM : LIM=[-1.0,1.0,-1.0,1.0]

TR=0.8

def Doc(size):
    S0,S1=40,50
	
    a=[S0,size[3]-S1, .8,.8,.8]
    b=[S0*7,size[3]-S1, .8,0.8,0.8]
    c=[S0*7,size[3]-S1*7, 0.8,0.8,0.8]
    d=[S0,size[3]-S1*7, 0.8,0.8,0.8]
    Tcarre(a,b,c,d,0.8)
    Lcarre(a,b,c,d,0.0)
    DOC=[' ',
'Documentation',
' ',
'KEYS ',
'M : dipslay GUI Menu',
'D : Set/Unset Documentation',
'S : Save current window content',
'       in a tga file',	
'Q or ESC : Exit',
'T : Set/Unset Transparency',
'L : Set/Unset lines',	
'E : Set/Unset outline',
'B : Set lines color to Black ',
'W : Set lines color to white',
'ARROW   : displace model on ',
'         UP/DOWN/LEFT/RIGHT side' 	,
'PADPLUS : increase ZOOM ',
'PADMINUS   : decrease  ZOOM ',	
'HOME    : cancel display modifs',
' ',
'Mouse button',	
'RIGHTMOUSE : same as  arrows',
]
    glColor3f(0.0,0.0,0.0)
    for D in DOC :
      glRasterPos2f(S0+8, size[3]-S1-13*DOC.index(D))
      Text(D)

def Ttriangle(a,b,c):
  glBegin(GL_TRIANGLES);
  glColor4f(a[2],a[3],a[4],TR)
  glVertex2f(a[0],a[1]);
  glColor4f(b[2],b[3],b[4],TR)
  glVertex2f(b[0],b[1]);
  glColor4f(c[2],c[3],c[4],TR)
  glVertex2f(c[0],c[1]);
  glEnd();

def Ftriangle(a,b,c):
  glBegin(GL_TRIANGLES);
  glColor3f(a[2],a[3],a[4])
  glVertex2f(a[0],a[1]);
  glColor3f(b[2],b[3],b[4])
  glVertex2f(b[0],b[1]);
  glColor3f(c[2],c[3],c[4])
  glVertex2f(c[0],c[1]);
  glEnd();

def Ltriangle(a,b,c,LC=0.5):
  TL=[a,b,c,a] 	
  for v in [0,1,2] :
    glBegin(GL_LINES);
    glColor4f(LC,LC,LC,TR+0.2)
    glVertex2f(TL[v][0],TL[v][1]);
    glVertex2f(TL[v+1][0],TL[v+1][1]);
    glEnd();


def Tcarre(a,b,c,d,LC=1.0):
    Ttriangle(a,b,c)
    Ttriangle(a,c,d)

def Fcarre(a,b,c,d,LC=1.0):
    Ftriangle(a,b,c)
    Ftriangle(a,c,d)

def Lcarre(a,b,c,d,LC=0.5):
  TL=[a,b,c,d,a] 	
  for v in [0,1,2,3] :
    glBegin(GL_LINES);
    glColor4f(LC,LC,LC,TR+0.2)
    glVertex2f(TL[v][0],TL[v][1]);
    glVertex2f(TL[v+1][0],TL[v+1][1]);
    glEnd();

   
def transface(f,x,y,u=0.0, v=0.0):
    global xlimit, LIM  
    global mouse_xr,sel, ANC, X,Y
    global mouse_yr, POS, XY,size
    global mouse_x,  mouse_y

    mouse_x=mouse_xr-size[0]
    mouse_y=mouse_yr-size[1]

    if sel==1:
      POS=[mouse_x-ANC[0],mouse_y-ANC[1]]
    u,v=POS

    a=[0,0,0.0, 0.0,0.0,0.0]
    b=[0,0,0.0, 0.0,0.0,0.0]
    c=[0,0,0.0, 0.0,0.0,0.0]
    d=[0,0,0.0, 0.0,0.0,0.0]

    if len(f.v)>=3:
        a[0]=int((f.uv[0][0]-LIM[1])*x+u)
        a[1]=int((f.uv[0][1]-LIM[3])*y+v)

        if a[0]>xlimit:
               xlimit=a[0]

        a[2]=f.col[0].r/255.0
        a[3]=f.col[0].g/255.0
        a[4]=f.col[0].b/255.0

        c[0]=int((f.uv[2][0]-LIM[1])*x+u)
        c[1]=int((f.uv[2][1]-LIM[3])*y+v)

        if c[0]>xlimit:
               xlimit=c[0]

        c[2]=f.col[2].r/255.0
        c[3]=f.col[2].g/255.0
        c[4]=f.col[2].b/255.0

     
        b[0]=int((f.uv[1][0]-LIM[1])*x+u)
        b[1]=int((f.uv[1][1]-LIM[3])*y+v)

        if b[0]>xlimit:
               xlimit=b[0]

        b[2]=f.col[1].r/255.0
        b[3]=f.col[1].g/255.0
        b[4]=f.col[1].b/255.0


    if  len(f.v)==4:     
        d[0]=int((f.uv[3][0]-LIM[1])*x+u)
        d[1]=int((f.uv[3][1]-LIM[3])*y+v)

        if d[0]>xlimit:
               xlimit=d[0]

        d[2]=f.col[3].r/255.0
        d[3]=f.col[3].g/255.0
        d[4]=f.col[3].b/255.0
    else:
        d=0


    #print a,b,c
    return a,b,c,d


def extract_faces(me,MENU):
    global  TMATList, selmatlist 
    if MENU==2:
       listf=[]
       for f in me.faces:
          if f.mat in selmatlist:
              listf.append(f)
       return listf

def affiche_mesh(ME,x,y):
    global LINE,xlimit,MMENU,XLIMIT,xwin,xlimit,LC
    global LIM, EMPTY,TRANSP
    if not NOLIM : LIM=[-1.0,1.0,-1.0,1.0]
	
    if ME.getType()=='Mesh':
       me=ME.getData()
       if MMENU.val==1:
          se=me.faces
       elif MMENU.val==3:
          se=[s for s in me.faces if s in me.getSelectedFaces() or s.sel]
       elif MMENU.val==2:
           se=extract_faces(me,2) 
       if not NOLIM : 
         for s in se:
           for u in s.uv:
	            if u[0] >LIM[0] : LIM[0]=u[0]
	            if u[0] <LIM[1] : LIM[1]=u[0]
	            if u[1] >LIM[2] : LIM[2]=u[1]
	            if u[1] <LIM[3] : LIM[3]=u[1]		          		
       xlimit=0
       for f in se:
         a,b,c,d=transface(f,x,y)
         if not EMPTY and not TRANSP: 
           if len(f.v)==4:
               Ftriangle(a,b,c)
               Ftriangle(a,c,d)
           elif len(f.v)==3:
               Ftriangle(a,b,c)
         elif not EMPTY : 
           if len(f.v)==4:
               Ttriangle(a,b,c)
               Ttriangle(a,c,d)
           elif len(f.v)==3:
               Ttriangle(a,b,c)

       if LINE.val==1 or EMPTY:
         for f in se:
           a,b,c,d=transface(f,x,y)
           if len(f.v)==4:
              Lcarre(a,b,c,d,LC)
           elif len(f.v)==3:
              Ltriangle(a,b,c,LC)
         if XLIMIT.val==0:
            Lcarre([1,1],[1,y-2],[xlimit+2,y-2],[xlimit+2,1]) 
         else:
            Lcarre([1,1],[1,y-2],[xwin-2,y-2],[xwin-2,1]) 
        
def write_tgafile(loc2,bitmap,width,height,profondeur): 

                  f=open(loc2,'wb') 
                  Origine_en_haut_a_gauche=32 
                  Origine_en_bas_a_gauche=0 
                  Data_Type_2=2 
                  RVB=profondeur*8 
                  RVBA=32 
                  entete0=[] 
                  for t in range(18): 
                    entete0.append(chr(0)) 

                  entete0[2]=chr(Data_Type_2) 
                  entete0[13]=chr(width/256) 
                  entete0[12]=chr(width % 256) 
                  entete0[15]=chr(height/256) 
                  entete0[14]=chr(height % 256) 
                  entete0[16]=chr(RVB) 
                  entete0[17]=chr(Origine_en_bas_a_gauche) 

                  #Origine_en_haut_a_gauche 

                  for t in entete0: 
                    f.write(t) 

                  for t in bitmap:

                    for c in [2,1,0,3]:
                        #print t[c]%256      
                        f.write(chr(t[c]*2)) 
                  f.close() 


def save(x0,y0,dx,dy):
    global SAVE
    im = Buffer(GL_BYTE,[dx*(dy+1),4])
    glReadPixels(x0,y0,dx,dy,GL_RGBA, GL_BYTE,im); 
    print len(im), dx*dy, dx, dy, len(im)/dy    
    write_tgafile(loc2,im,dx,dy+1,4)
    SAVE=0
    Blender.Redraw()

def DOCMat_list(TMATList,ME):
    me=Blender.NMesh.GetRaw(ME.getData().name) 
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

def SELMat_list():
      global TMATList,selmatlist
      Me=Blender.Object.GetSelected()
      if Me!=[]:
         if Me[0].getType()=='Mesh':   
            TMATList=DOCMat_list(TMATList,Me[0])
            selmatlist=[]
            for TMat in TMATList[2]:
               if TMat.val==1.0:
                  selmatlist.append(TMATList[2].index(TMat))  
            ERROR=0
         else:
            ERROR=1
            TextERROR='Selected Object is not a mesh.'  
      else:
          ERROR=1
          TextERROR='No Selected Object.'  
      
def DOCBONEMENU(TBONEMENU):
    pass

# ----------
# uvpaint1
# ----------
NSIZE=Create(1.0)
# ----------
# uvpaint2
# ----------
LINE=Create(0)
# ----------
# uvpaint3
# ----------
TEXT=Create(loc2)
# ----------
# uvpaint4
# ----------
TMENU="MODE MENU %t|All %x1|Material %x2|Selected %x3"
# ----------
# uvpaint4
# ----------
# coming soon : "|vertex group %x4", perhaps in uvpainter v0.5
LCOLOR= Create(64)
SAVE=0

MMENU=Create(3)
TDOCMat = Create(0)
# ----------
TMATList= [0,[],[]] 
for t in range(16):
    TMATList[1].append([0.0,0.0,0.0])
    TMATList[2].append(Create(0))
# ----------
TDOCMat = Create(1)
# ----------
TBONEMENU= Create(1) 
# ----------

XLIMIT=Create(0)

y=0
x=0
x0=0
y0=0
xwin=0

n0=32

def draw():
    global NSIZE,LINE,x0,y0,y,x,TEXT,MMENU,TDOCMat
    global XLIMIT,selmatlist,xwin, LCOLOR, SAVE
    global mouse_xr,sel, ANC, X,Y, TRANSP, DOCU
    global mouse_yr, POS, XY,size, TRANSP,EMPTY
    global MENU, SAVE

    size=Buffer(GL_FLOAT, 4)
    glGetFloatv(GL_SCISSOR_BOX, size)
    size=[int(s) for s in size ]
    
    n0=32
    x0=size[0]
    y0=size[1]
    
    x=size[2]
    y=size[3]
   
    xwin=x
    ywin=y
   

    glClear(GL_COLOR_BUFFER_BIT)
    glShadeModel(GL_SMOOTH)
    #transparence  
    if TRANSP : 
       glEnable(GL_BLEND)
       glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)

    SelecMESH=Blender.Object.GetSelected()
    if SelecMESH!=[] and SelecMESH[0].getType()=='Mesh':
       affiche_mesh(SelecMESH[0],int(y*NSIZE.val),int(y*NSIZE.val-2))

    if MENU:
        glColor3f(0.0,0.0,0.0)
        glRectf(4,size[3],555,size[3]-32 )
        glColor3f(1.0,1.0,1.0)
       
        glRasterPos2f(8, size[3]-13)
        Text("uvpainter v0.8")
         
        glRasterPos2f(8, size[3]-28)
        Text("Jm Soler, 08/2005")

        Button("ReDraw"      ,16 ,290-118+61 ,size[3]-30  ,60 ,13)
        Button("Exit"        ,1  ,250-122+63 ,size[3]-30  ,38 ,13)
        Button("Save"        ,6  ,250-16+61  ,size[3]-30  ,40 ,13)

        NSIZE= Slider("Sc:",4   ,290-118+61  ,size[3]-15 , 102, 13, NSIZE.val, 0.1,5.0,0,"SIZE.")
        LINE=Toggle("line",   5   ,250-122+63   ,size[3]-15 , 38, 13, LINE.val, "Draw lines")

        glRasterPos2f(250-130  ,size[3]-13,)
        Text("Mode")

        LCOLOR=Number("", 50, 250-77,size[3]-15,18,13,LCOLOR.val,1,64,'Line color, 64-lighter, 1-darker.')

        MMENU= Menu(TMENU    ,2  ,250-130, size[3]-30, 63, 13, MMENU.val, "MODE menu.")

        if MMENU.val==1 or MMENU.val==3:
           glRasterPos2f( 250-16+61+42+80,size[3]-13)
           if XLIMIT.val:
                  xl=xwin
           else:
                  xl=xlimit

           Text("x :"+"%d"%(xl+2))  

           glRasterPos2f(250-16+61+42+65*2,size[3]-13)
           Text("y :"+"%d"%(y-n0+1))  

           TEXT=String("to:",   7   , 278+61  ,size[3]-28 , 213, 13, TEXT.val, 256, "Draw lines")
           if XLIMIT.val==1:
              limit='winlimit'
           else:
              limit='maxXlimit'
           XLIMIT=Toggle(limit, 9   , 250-16+61+42   ,size[3]-15 , 60, 13, XLIMIT.val, "to save picture from x max uv limit, or x window max limit")

        if MMENU.val==2:
           TDOCMat=Toggle("doc"     ,24,250-130+35  ,size[3]-13 , 28, 13, TDOCMat.val)   
           if TDOCMat.val==1:
                 SELMat_list()  
                 for t in range(TMATList[0]):
                     glCl3(TMATList[1][t][0],
                           TMATList[1][t][1],
                           TMATList[1][t][2]) 
                     glRct((293-16+61)+t*20,
                           size[3]-13,
                           (293-16+61)+t*20+20,
                           size[3]-30,)
                     TMATList[2][t]=Toggle("%s"%t , 32+t ,(293-16+61)+t*20  ,size[3]-13  ,20 , 13,TMATList[2][t].val)

    #else:
	#    save()

    if DOCU:
         Doc(size)
		
def event(evt, val):  
    global mouse_x, x, mouse_y, y, LC
    global LCOLOR, DOCU, MENU, SAVE
    global mouse_xr,sel, ANC, X,Y ,NSIZE
    global mouse_yr, POS, XY,size, TRANSP,EMPTY

    if (evt== QKEY or evt== ESCKEY and not val): 
      Exit()
    elif (evt== TKEY and not val): 
       TRANSP=abs(TRANSP-1)
    elif (evt== EKEY and not val): 
       EMPTY=abs(EMPTY-1)
    elif (evt== MKEY and not val): 
       MENU=abs(MENU-1)
    elif (evt== SKEY and not val): 
       SAVE=abs(MENU-1)
    elif (evt== WKEY and not val): 
       LC=1.0
       LCOLOR.val=64
    elif (evt== BKEY and not val): 
       LC=0.0
       LCOLOR.val=1	
    elif (evt==LEFTARROWKEY and not val) :
         POS[0]-=10
    elif (evt==RIGHTARROWKEY and not val) :
         POS[0]+=10
    elif (evt==UPARROWKEY and not val) :
         POS[1]+=10
    elif (evt==DOWNARROWKEY and not val) :
         POS[1]-=10
    elif (evt==PADMINUS and not val) :
         NSIZE.val-=0.1
    elif (evt==PADPLUSKEY and not val) :
         NSIZE.val+=0.1
    elif (evt==LKEY and not val) :
	     LINE.val=abs(LINE.val-1)
    elif (evt==HOMEKEY and not val) :
         NSIZE.val=1.0
         POS=[0,0]
    elif (evt==DKEY and not val) :
	     DOCU=abs(DOCU-1)          
    elif (evt == MOUSEX): mouse_xr = val
    elif (evt == MOUSEY): mouse_yr = val
	
    elif (evt == RIGHTMOUSE and val):
        if (sel==0 ) :
           ANC=[(mouse_xr-size[0])-POS[0],(mouse_yr-size[1])-POS[1]]
           sel=1        
    elif (evt == RIGHTMOUSE and not val):
        sel=0
        ANC=0,0
    Redraw(1)
   

def bevent(evt):
    global LINE,NSIZE,n0,x0,y0,y,TEXT, loc2
    global TMATList, selmatlist, TDOCMat,XLIMIT
    global xlimit,LCOLOR,LC,SAVE

    if   (evt== 1):
        Exit()

    elif   (evt== 16):
         pass

    elif   (evt== 4):
       ng=NSIZE.val

    elif   (evt== 6):
       if XLIMIT.val==1:
        xi=xwin
        save(x0,y0,xi+2,int(y-n0))
       else:
        xi=xlimit
        save(x0,y0,xi+2,int(y*NSIZE.val)-n0)

    elif (evt== 7):
       if exist(TEXT.val):
            loc2=TEXT.val
       else:
            TEXT.val=loc2
 
    elif (evt== 24) or (evt in [32,33,34,35,36,37,38,39,40,41,42,43,44]):
       SELMat_list()

    elif  (evt== 50):
       LC=float(LCOLOR.val)/64.0


    Blender.Redraw()

Register(draw, event, bevent)
