#!BPY
""" Registration info for Blender menus: <- these words are ignored
Name: 'UVpainter'
Blender: 232
Group: 'UV'
Tip: 'Use vertex paint color value to fill uvmapping.'
"""
# $Id$
#
#----------------------------------------------
# uvpainter script (c) 04/2004  jean-michel soler
# http://jmsoler.free.fr/util/blenderfile/py/UVpaint05.zip
# this script is released under GPL licence
# for the Blender 2.33 scripts distribution
#----------------------------------------------
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
#----------------------------------------------

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

loc0= Blender.sys.dirname(Blender.Get ("filename")) 
loc2=loc0+Blender.sys.dirsep+'test00.tga' 

glCr=glRasterPos2d
glCl3=glColor3f
glCl4=glColor4f
glRct=glRectf

xlimit=0
selmatlist=[]

def triangle(a,b,c):
  glBegin(GL_TRIANGLES);
  glColor3f(a[2],a[3],a[4])
  glVertex2f(a[0],a[1]);
  glColor3f(b[2],b[3],b[4])
  glVertex2f(b[0],b[1]);
  glColor3f(c[2],c[3],c[4])
  glVertex2f(c[0],c[1]);
  glEnd();

def Ltriangle(a,b,c):
  glBegin(GL_LINES);
  glColor3f(1.0,1.0,1.0)
  glVertex2f(a[0],a[1]);
  glVertex2f(b[0],b[1]);
  glVertex2f(c[0],c[1]);
  glEnd();

def carre(a,b,c,d):
    triangle(a,b,c)
    triangle(a,c,d)

def Lcarre(a,b,c,d):
  glBegin(GL_LINES);
  glColor3f(1.0,1.0,1.0)
  glVertex2f(a[0],a[1]);
  glVertex2f(b[0],b[1]);
  glVertex2f(c[0],c[1]);
  glVertex2f(d[0],d[1]);
  glEnd();


   
def transface(f,x,y):
    global xlimit

    

    a=[0,0,0.0, 0.0,0.0,0.0]
    b=[0,0,0.0, 0.0,0.0,0.0]
    c=[0,0,0.0, 0.0,0.0,0.0]
    d=[0,0,0.0, 0.0,0.0,0.0]

    if len(f.v)>=3:
        a[0]=int(f.uv[0][0]*x)
        a[1]=int(f.uv[0][1]*y)

        if a[0]>xlimit:
               xlimit=a[0]

        a[2]=f.col[0].r/255.0
        a[3]=f.col[0].g/255.0
        a[4]=f.col[0].b/255.0

        c[0]=int(f.uv[2][0]*x)
        c[1]=int(f.uv[2][1]*y)

        if c[0]>xlimit:
               xlimit=c[0]

        c[2]=f.col[2].r/255.0
        c[3]=f.col[2].g/255.0
        c[4]=f.col[2].b/255.0

     
        b[0]=int(f.uv[1][0]*x)
        b[1]=int(f.uv[1][1]*y)

        if b[0]>xlimit:
               xlimit=b[0]

        b[2]=f.col[1].r/255.0
        b[3]=f.col[1].g/255.0
        b[4]=f.col[1].b/255.0


    if  len(f.v)==4:     
        d[0]=int(f.uv[3][0]*x)
        d[1]=int(f.uv[3][1]*y)

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
    global LINE,xlimit,MMENU,XLIMIT,xwin,xlimit

    if ME.getType()=='Mesh':
       me=GetRaw(ME.getData().name)
       
       if MMENU.val==1:
          se=me.faces

       elif MMENU.val==3:
          se=me.getSelectedFaces()

       elif MMENU.val==2:
          se=extract_faces(me,2) 

       xlimit=0
       for f in se:
         a,b,c,d=transface(f,x,y)
         if len(f.v)==4:
            triangle(a,b,c)
            triangle(a,c,d)
         elif len(f.v)==3:
            triangle(a,b,c)

       if LINE.val==1:
         for f in se:
           a,b,c,d=transface(f,x,y)
           if len(f.v)==4:
              Lcarre(a,b,c,d)
           elif len(f.v)==3:
              Ltriangle(a,b,c)

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
    im = Buffer(GL_BYTE,[dx*(dy+1),4])
    glReadPixels(x0,y0,dx,dy,GL_RGBA, GL_BYTE,im); 
    print len(im), dx*dy, dx, dy, len(im)/dy    
    write_tgafile(loc2,im,dx,dy+1,4)

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

# coming soon : "|Bone %x4", perhaps in uvpainter v0.5
 
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
    global XLIMIT,selmatlist,xwin

    size=Buffer(GL_FLOAT, 4)
    glGetFloatv(GL_SCISSOR_BOX, size)
    size= size.list

    for s in [0,1,2,3]: size[s]=int(size[s])
    
    n0=32
    x0=size[0]
    y0=size[1]
    
    x=size[2]
    y=size[3]
   
    xwin=x
    ywin=y
   

    glClear(GL_COLOR_BUFFER_BIT)

    glShadeModel(GL_SMOOTH)
    SelecMESH=Blender.Object.GetSelected()
    if SelecMESH!=[]:
       if SelecMESH[0].getType()=='Mesh':
            affiche_mesh(SelecMESH[0],int(y*NSIZE.val),int(y*NSIZE.val-n0-2))

    glColor3f(0.0,0.0,0.0)
    glRectf(4,size[3],555,size[3]-32 )

    glColor3f(1.0,1.0,1.0)

    glRasterPos2f(8, size[3]-13)
    Text("uvpainter v0.5")
     
    glRasterPos2f(8, size[3]-28)
    Text("Jm Soler, 05/2004")

    Button("ReDraw"      ,16 ,290-118+61 ,size[3]-30  ,60 ,13)
    Button("Exit"        ,1  ,250-122+63 ,size[3]-30  ,38 ,13)
    Button("Save"        ,6  ,250-16+61  ,size[3]-30  ,40 ,13)

    NSIZE= Slider("Sc:",4   ,290-118+61  ,size[3]-15 , 102, 13, NSIZE.val, 0.1,1.5,0,"SIZE.")
    LINE=Toggle("line",   5   ,250-122+63   ,size[3]-15 , 38, 13, LINE.val, "Draw lines")

    glRasterPos2f(250-130  ,size[3]-13,)
    Text("Mode")

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

    

def event(evt, val):    
    if (evt== QKEY and not val): Exit()

def bevent(evt):
    global LINE,NSIZE,n0,x0,y0,y,TEXT, loc2
    global TMATList, selmatlist, TDOCMat,XLIMIT
    global xlimit

    if   (evt== 1):
        Exit()

    elif   (evt== 16):
         pass

    elif   (evt== 4):
       ng=NSIZE.val

    elif   (evt== 6):
       if XLIMIT.val==1:
          xi=xwin
       else:
          xi=xlimit

       save(x0,y0,xi+2,int(y*NSIZE.val-n0))

    elif (evt== 7):
       if exist(TEXT.val):
            loc2=TEXT.val
       else:
            TEXT.val=loc2
 
    elif (evt== 24) or (evt in [32,33,34,35,36,37,38,39,40,41,42,43,44]):
       SELMat_list() 


    Blender.Redraw()

Register(draw, event, bevent)
