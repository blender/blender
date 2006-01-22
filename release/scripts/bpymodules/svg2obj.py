# -*- coding: latin-1 -*-
"""
SVG 2 OBJ translater, 0.4.7
Copyright (c) jm soler juillet/novembre 2004-janvier 2006, 
# ---------------------------------------------------------------
    released under GNU Licence 
    for the Blender 2.40 Python Scripts Bundle.
Ce programme est libre, vous pouvez le redistribuer et/ou
le modifier selon les termes de la Licence Publique Générale GNU
publiée par la Free Software Foundation (version 2 ou bien toute
autre version ultérieure choisie par vous).

Ce programme est distribué car potentiellement utile, mais SANS
AUCUNE GARANTIE, ni explicite ni implicite, y compris les garanties
de commercialisation ou d'adaptation dans un but spécifique.
Reportez-vous à la Licence Publique Générale GNU pour plus de détails.

Vous devez avoir reçu une copie de la Licence Publique Générale GNU
en même temps que ce programme ; si ce n'est pas le cas, écrivez à la
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
MA 02111-1307, États-Unis.


This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA    
# ---------------------------------------------------------------

#---------------------------------------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_import_svg.htm
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_import_svg_en.htm
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------------------------------------

-- Concept : translate SVG file in GEO .obj file and try to load it. 
-- Curiousity : the original matrix must be :

                         0.0 0.0 1.0 0.0
                         0.0 1.0 0.0 0.0
                         0.0 0.0 1.0 0.0
                         0.0 0.0 0.0 1.0 

                  and not:
                         1.0 0.0 0.0 0.0
                         0.0 1.0 0.0 0.0
                         0.0 0.0 1.0 0.0
                         0.0 0.0 0.0 1.0 

-- Options :
    SHARP_IMPORT = 0 
            choise between "As is", "Devide by height" and "Devide by width"
    SHARP_IMPORT = 1
            no choise

-- Possible bug : sometime, the new curves object's RotY value 
                  jumps to -90.0 degrees without any reason.

Yet done: 
   M : absolute move to 
   Z : close path
   L : absolute line to  
   C : absolute curve to
   S : absolute curve to with only one handle
   H : absolute horizontal line to  
   V : absolute vertical line to  
   
   l : relative line to     2004/08/03
   c : relative curve to    2004/08/03
   s : relative curve to with only one handle  
   h : relative horizontal line to 
   v : relative vertical line to 

   A : courbe_vers_a, 
   V : ligne_tracee_v,
   H : ligne_tracee_h, 
   Z : boucle_z,
   Q : courbe_vers_q,
   T : courbe_vers_t,
   a : courbe_vers_a, 
   v : ligne_tracee_v,
   h : ligne_tracee_h, 
   z : boucle_z,
   q : courbe_vers_q,

   transfrom for <g> tag 
   transform for <path> tag

Changelog:
      0.1.1 : - control file without extension
      0.2.0 : - improved reading of several data of the same type 
                following the same command (for gimp import)
      0.2.1 : - better choice for viewboxing ( takes the viewbox if found, 
                instead of x,y,width and height              
      0.2.2 : - read compact path data from Illustrator 10             
      0.2.3 : - read a few new relative displacements
      0.2.4 : - better hash for command followed by a lone data 
                (h,v) or uncommun number (a) 
      0.2.5 : - correction for gimp import 
      0.2.6 : - correction for illustrator 10 SVG
      0.2.7 : - correction for inskape 0.40 cvs  SVG
      0.2.8 : - correction for inskape plain SVG
      0.3   : - reading of the transform properties added : 
                    translate
      0.3.1 : - compatibility restored with gimp 
      0.3.2 : - transform properties added (june, 15-16): 
                    scale, 
                    rotate, 
                    matrix, 
                    skew               
              - added a test on __name__ to load the script
                outside from the blender menu  
      0.3.3 : - matrix transform content control  
      0.3.4 : - paths data reading rewritten (19/06/05) 
      0.3.5 : - test on empty curve  (22/06/05)
              - removed overlayed points
      0.3.6 : - rewriting of the bezier point contruction to correct
                a problem in the connection between L type point and
                C or S type point         
      0.3.7 : - code correction for bezier knot in Curveto command when
                the command close a path
      0.3.8 : - code was aded to manage quadratic bezier, 
                Q,q command and T,t commands, as a normal  blender's bezier point 
              - The last modications does not work with gimp 2.0 svg export . 
                corrected too .
      0.3.9 : - Path's A,a  command for ellipse's arc  .
      0.4.0 : - To speed up the function filtre_DATA was removed and text
                variables are changed into numeric variables               
      0.4.1 : - svg, groups and shapes hierarchy  added
              - now transform properties are computed  using a stack  with all
                parented groups
              - removed or replaced useless functions :
              - skewY, skewX transforms
              - radians in rotate transform 
      0.4.2 : - Added functon to translate others shapes in path 
	             rect, line, polyline, polygon
	
      0.4.3 : - various corrections 
                  text font (id property exported by Adobe Illustrator are between coma)
                  function  to code  s tag has been  rewritten 

      0.4.4 : - various corrections      
                to oblige the script to understand a line feed just after 
                a tag . Rarely encountered problem, but it exits in a svg file
                format exported by a outliner script for mesh .

      0.4.5 : - update for CVS only, at least blender 2.38 and upper
                no BezTriple module in older version
                added a createCURVES function to avoid to use
                the OBJ format export/import .

                Perhaps problems with cyclic curves . If a closed curve 
                does not appear closed in blender, enter edit mode select 
                all  knot with Akey,  do a Hkey to set handle type (without 
                this the knot are recalculated) , and finally use the Ckey 
                to close the curve .
                Should work ... not guaranted .

      0.4.6 : - cyclic flag ...

      0.4.7 : - Management of the svgz files . the complete python or the gzip.py 
                file is needed .
                Little improvement of the curve drawing using the createCURVES 
                function 
==================================================================================   
=================================================================================="""

SHARP_IMPORT=0
SCALE=1
scale_=1
DEBUG = 0#print
DEVELOPPEMENT=0
    
import sys
from math import cos,sin,tan, atan2, pi, ceil
PI=pi
import Blender
from Blender import Mathutils
BLversion=Blender.Get('version')

try:
    import nt
    os=nt
    os.sep='\\'

except:    
    import posix
    os=posix
    os.sep='/'
    
def isdir(path):
    try:
        st = os.stat(path)
        return 1 
    except:
        return 0
    
def split(pathname):
         if pathname.find(os.sep)!=-1:
             k0=pathname.split(os.sep)
         else:
            if os.sep=='/':
                k0=pathname.split('\\')
            else:
                k0=pathname.split('/') 

         directory=pathname.replace(k0[len(k0)-1],'')
         Name=k0[len(k0)-1]
         return directory, Name
        
def join(l0,l1):        
     return  l0+os.sep+l1
    
os.isdir=isdir
os.split=split
os.join=join

def filtreFICHIER(nom):
     """
     Function  filtreFICHIER

     in  : string  nom , filename
     out : string  t   , if correct filecontaint 

     Lit le contenu du fichier et en fait une pre-analyse 
     pour savoir s'il merite d'etre traite .
     """
     # ----------
     # 0.4.7 
     # ----------
     if nom.upper().find('.SVGZ')!=-1:
         try :
             import gzip 
             tz=gzip.GzipFile(nom)
             t=tz.read()
         except:
             name = "ERROR: fail to import gzip module or gzip error ... "  
             result = Blender.Draw.PupMenu(name)
             return "false"
     else:    
        f=open(nom,'rU')
        t=f.read()
        f.close()
     # ----------
     # 0.4.7  : end 
     # ----------
     
     # -----------------
     #  pre-format ...
     # -----------------
     # --------------------
     # 0.4.4  '\r','' -->  '\r',' ' 
     #        '\n','' -->  '\n',' ' 
     #--------------------
     t=t.replace('\r',' ')
     t=t.replace('\n',' ')
     t=t.replace('svg:','')
     
     if t.upper().find('<SVG')==-1 :
         name = "ERROR: invalid or empty file ... "  # if no %xN int is set, indices start from 1
         result = Blender.Draw.PupMenu(name)
         return "false"
     else:
          return t

     """
     elif  t.upper().find('<PATH')==-1 and\
           t.upper().find('<RECT')==-1 and\
           t.upper().find('<LINE')==-1 and\
           t.upper().find('<POLYLINE')==-1 	:
         name = "ERROR: there's no Path in this file ... "  # if no %xN int is set, indices start from 1
         result = Blender.Draw.PupMenu(name)
         return "false"
     """


#===============================
# Data
#===============================


#===============================
# Blender Curve Data
#===============================
objBEZIER=0
objSURFACE=5
typBEZIER3D=1  #3D
typBEZIER2D=9  #2D

class Bez:
      def __init__(self):
           self.co=[]
           self.ha=[0,0]
           self.tag=''

class ITEM:
      def __init__(self):
               self.type        =  typBEZIER3D        
               self.pntsUV      =  [0,0]              
               self.resolUV     =  [32,0]            
               self.orderUV     =  [0,0]             
               self.flagUV      =  [0,0]              
               self.Origine     =  [0.0,0.0]
               self.beziers_knot = []
               self.fill=0
               self.closed=0
               self.color=[0.0,0.0,0.0]

class COURBE:
      def __init__(self):
              self.magic_number='3DG3'              
              self.type            =  objBEZIER        
              self.number_of_items =  0              
              self.ext1_ext2       =  [0,0]             
              self.matrix          =  """0.0 0.0 1.0 0.0
0.0 1.0 0.0 0.0
0.0 0.0 1.0 0.0
0.0 0.0 0.0 1.0 """ 
              self.ITEM = {}


courbes=COURBE()
PATTERN={}
BOUNDINGBOX={'rec':[],'coef':1.0}
npat=0
#=====================================================================
#======== name of the curve in the curves dictionnary ===============
#=====================================================================
n0=0

#=====================================================================
#====================== current Point ================================
#=====================================================================
CP=[0.0,0.0] #currentPoint

#=====================================================================
#===== to compare last position to the original move to displacement =
#=====  needed for cyclic definition inAI, EPS forma  ================
#=====================================================================
def test_egalitedespositions(f1,f2):
    if f1[0]==f2[0] and f1[1]==f2[1]:
       return Blender.TRUE
    else:
       return Blender.FALSE


def Open_GEOfile(dir,nom):
    global SCALE,BOUNDINGBOX, scale_
    if BLversion>=233:
       Blender.Load(dir+nom+'OOO.obj', 1)
       BO=Blender.Object.Get()

       BO[-1].RotY=3.1416
       BO[-1].RotZ=3.1416
       BO[-1].RotX=3.1416/2.0
       
       if scale_==1:
          BO[-1].LocY+=BOUNDINGBOX['rec'][3]
       else:
         BO[-1].LocY+=BOUNDINGBOX['rec'][3]/SCALE
 
       BO[-1].makeDisplayList() 
       Blender.Window.RedrawAll()
    else:
       print "Not yet implemented"

def create_GEOtext(courbes):
    global SCALE, B, BOUNDINGBOX,scale_

    r=BOUNDINGBOX['rec']
    if scale_==1:
       SCALE=1.0
    elif scale_==2:
       SCALE=r[2]-r[0]
    elif scale_==3:
       SCALE=r[3]-r[1]
 
    t=[]
    t.append(courbes.magic_number+'\n')
    t.append(str(courbes.type)+'\n')
    t.append(str(courbes.number_of_items)+'\n')
    t.append(str(courbes.ext1_ext2[0])+' '+str(courbes.ext1_ext2[1])+'\n')
    t.append(courbes.matrix+'\n')
    
    for k in courbes.ITEM.keys():
        t.append("%s\n"%courbes.ITEM[k].type)
        t.append("%s %s \n"%(courbes.ITEM[k].pntsUV[0],courbes.ITEM[k].pntsUV[1]))
        t.append("%s %s \n"%(courbes.ITEM[k].resolUV[0],courbes.ITEM[k].resolUV[1]))
        t.append("%s %s \n"%(courbes.ITEM[k].orderUV[0],courbes.ITEM[k].orderUV[1]))
        t.append("%s %s \n"%(courbes.ITEM[k].flagUV[0],courbes.ITEM[k].flagUV[1]))

        flag =0#courbes.ITEM[k].flagUV[0]

        for k2 in range(flag,len(courbes.ITEM[k].beziers_knot)):
           #k1 =courbes.ITEM[k].beziers_knot[k2]
           k1=ajustement(courbes.ITEM[k].beziers_knot[k2], SCALE)
           
           t.append("%4f 0.0 %4f \n"%(k1[4],k1[5]))
           t.append("%4f 0.0 %4f \n"%(k1[0],k1[1]))
           t.append("%4f 0.0 %4f \n"%(k1[2],k1[3]))
           t.append(str(courbes.ITEM[k].beziers_knot[k2].ha[0])+' '+str(courbes.ITEM[k].beziers_knot[k2].ha[1])+'\n')

    return t

def save_GEOfile(dir,nom,t):
     f=open(dir+nom+'OOO.obj','w')
     f.writelines(t)
     f.close()

#--------------------
# 0.4.5 : for blender cvs 2.38 ....
#--------------------
def createCURVES(courbes):
    global SCALE, B, BOUNDINGBOX,scale_
    from Blender import Curve, Object, Scene, BezTriple

    r=BOUNDINGBOX['rec']
    if scale_==1:
       SCALE=1.0
    elif scale_==2:
       SCALE=r[2]-r[0]
    elif scale_==3:
       SCALE=r[3]-r[1]

    [o.select(0) for o in Object.Get()]
    for I in courbes.ITEM:
        c = Curve.New()
        # ----------
        # 0.4.7 
        # ----------
        c.setResolu(24)  
        scene = Scene.getCurrent()
        ob = Object.New('Curve')
        ob.link(c)
        scene.link(ob)
        ob.select(1)
        bzn=0
        #for b in courbes.ITEM[I].beziers_knot:
        for k2 in range(0,len(courbes.ITEM[I].beziers_knot)):
            bz=ajustement(courbes.ITEM[I].beziers_knot[k2], SCALE)
            #bz=k1
            if bzn==0:
              cp1 =  bz[4],bz[5],0.0 , bz[0],bz[1],0.0, bz[2],bz[3],0.0, 
              beztriple1 = BezTriple.New(cp1)
              bez = c.appendNurb(beztriple1)
              
              bzn = 1
            else:
              cp2 =  bz[4],bz[5],0.0 , bz[0],bz[1],0.0, bz[2],bz[3],0.0
              beztriple2 = BezTriple.New(cp2)
              bez.append(beztriple2)

        if courbes.ITEM[I].flagUV[0]==1 :
          #--------------------
          # 0.4.6 : cyclic flag ...
          #--------------------
           bez.flagU += 1
           


#=====================================================================
#=====      SVG format   :  DEBUT             =========================
#=====================================================================
#--------------------
# 0.4.2
#--------------------
OTHERSSHAPES=['rect','line', 'polyline', 'polygon','circle','ellipse']

#--------------------
# 0.4.2
#--------------------
def rect(prp):
  D=[]
  if 'x' not in prp.keys(): x=0.0
  else	: x=float(prp['x'])
  if 'y' not in prp.keys(): y=0.0
  else	: y=float(prp['y'])
		
  height=float(prp['height'])
  width=float(prp['width'])
	
  """
   normal rect

   x,y 
                  h1       
       *----------*
       |          |
       |          | 
       |          |
       *----------* v1
       h2
  """
  if 'rx' not in prp.keys() or 'rx' not in prp.keys(): 
     exec   """D=['M','%s','%s','h','%s','v','%s','h','%s','z']"""%(x,y,width,height,-width)	
  else :
     rx=float(prp['rx'])
     if 'ry' not in prp.keys()  : 
	    ry=float(prp['rx'])
     else :	ry=float(prp['ry'])
     if 'rx' in prp.keys() and prp['rx']<0.0: rx*=-1
     if 'ry' in prp.keys() and prp['ry']<0.0: ry*=-1
	
     """
   rounded corner
      
   x,y     M         h1       
       ---*----------*  
         /            \  
        /              \
    v2 *                * c1
       |                |
       |                |   
       |                |
    c3 *                * v2
        \              /
         \            /   
          *----------*  
          h2         c2
     """
     exec   """D=['M','%s','%s',
                  'h','%s',
                  'c','%s','%s','%s','%s','%s','%s',
                  'v','%s',
                  'c','%s','%s','%s','%s','%s','%s',
                  'h','%s',
                  'c','%s','%s','%s','%s','%s','%s',
                  'v','%s',
                  'c','%s','%s','%s','%s','%s','%s',
                  'z']"""%(x+rx,y,
                           width-2*rx,
                           rx,0.0,rx,ry,rx,ry,
                            height-ry,
                           0.0,ry,-rx,ry,-rx,ry,
                           -width+2*rx,
                           -rx,0.0,-rx,-ry,-rx,-ry,
                           -height+ry,
                           0.0,0.0,0.0,-ry,rx,-ry
 )	
                       
  return D

#--------------------
# 0.4.2
#--------------------
def circle(prp):
   if 'cx' not in prp.keys(): cx=0.0	
   else : cx =float(prp['cx'])
   if 'cy' not in prp.keys(): cy=0.0
   else : cy =float(prp['cy'])
   r = float(prp['r'])
   exec """D=['M','%s','%s',                  
              'C','%s','%s','%s','%s','%s','%s',
              'C','%s','%s','%s','%s','%s','%s',
              'C','%s','%s','%s','%s','%s','%s',
              'C','%s','%s','%s','%s','%s','%s',
              'Z']"""%( 
                      cx,cy+r, 
                      cx-r,cy+r*0.552, cx-0.552*r,cy+r, cx,cy+r,
                      cx+r*0.552,cy+r, cx+r,cy+r*0.552, cx+r,cy,
                      cx+r,cy-r*0.552,  cx+r*0.552,cy-r,   cx,cy-r,
                      cx-r*0.552,cy-r, cx-r,cy-r*0.552, cx-r,cy
                      )
   return D 
   
#--------------------
# 0.4.2
#--------------------
def ellipse(prp):
   if 'cx' not in prp.keys(): cx=0.0	
   else : cx =float(prp['cx'])
   if 'cy' not in prp.keys(): cy=0.0
   else : cy =float(prp['cy'])
   ry = float(prp['rx'])
   rx = float(prp['ry'])

   exec """D=['M','%s','%s',                  
              'C','%s','%s','%s','%s','%s','%s',
              'C','%s','%s','%s','%s','%s','%s',
              'C','%s','%s','%s','%s','%s','%s',
              'C','%s','%s','%s','%s','%s','%s',
              'Z']"""%( 
                      cx,cy+rx, 
                      cx-ry,cy+rx*0.552, cx-0.552*ry,cy+rx, cx,cy+rx,
                      cx+ry*0.552,cy+rx, cx+ry,cy+rx*0.552, cx+ry,cy,
                      cx+ry,cy-rx*0.552,  cx+ry*0.552,cy-rx,   cx,cy-rx,
                      cx-ry*0.552,cy-rx, cx-ry,cy-rx*0.552, cx-ry,cy
                      )
   return D 
#--------------------
# 0.4.2
#--------------------
def line(prp):
  exec   """D=['M','%s','%s',
                  'L','%s','%s']"""%(prp['x1'],prp['y1'], prp['x2'],prp['y2'])
  return D
#--------------------
# 0.4.2
#--------------------    
def polyline(prp):
 if 'points' in  prp.keys():
    #print prp['points']
    points=prp['points'].split(' ')
    #print points
    np=0
    for p in points:
     if p!='':
        p=p.split(',')
        if np==0:
           exec "D=['M','%s','%s']"%(p[0],p[1])
           np+=1
        else: 
           exec """D.append('L');D.append('%s');D.append('%s')"""%(p[0],p[1])
    #print D
    return D
 else:
    return []

#--------------------
# 0.4.2
#--------------------	
def polygon(prp):
    D=polyline(prp)
    if D!=[]:
        D.append('Z')
    return D

    
#--------------------
# 0.3.9
#--------------------
def calc_arc (cpx,cpy, rx, ry,  ang, fa , fs , x, y) :
    rx=abs(rx)
    ry=abs(ry)
    px=abs((cos(ang)*(cpx-x)+sin(ang)*(cpy-y))*0.5)**2.0
    py=abs((cos(ang)*(cpy-y)-sin(ang)*(cpx-x))*0.5)**2.0
    pl=px/(rx**2.0)+py/(ry**2.0 )
    if pl>1.0:
        pl=pl**0.5;rx*=pl;ry*=pl
    x0=(cos(ang)/rx)*cpx+(sin(ang)/rx)*cpy
    y0=(-sin(ang)/ry)*cpx+(cos(ang)/ry)*cpy
    x1=(cos(ang)/rx)*x+(sin(ang)/rx)*y
    y1=(-sin(ang)/ry)*x+(cos(ang)/ ry)*y    
    d=(x1-x0)*(x1-x0)+(y1-y0)*(y1-y0)  
    if abs(d)>0.0 :sq=1.0/d-0.25
    else: sq=-0.25
    if sq<0.0 :sq=0.0
    sf=sq**0.5
    if fs==fa :sf=-sf
    xc=0.5*(x0+x1)-sf*(y1-y0)
    yc=0.5*(y0+y1)+sf*(x1-x0)
    ang_0=atan2(y0-yc,x0-xc)
    ang_1=atan2(y1-yc,x1-xc)
    ang_arc=ang_1-ang_0;
    if (ang_arc < 0.0 and fs==1) :
        ang_arc += 2.0 * PI
    elif (ang_arc>0.0 and fs==0) :
        ang_arc-=2.0*PI
    n_segs=int(ceil(abs(ang_arc*2.0/(PI*0.5+0.001))))
    P=[]
    for i in range(n_segs):
        ang0=ang_0+i*ang_arc/n_segs
        ang1=ang_0+(i+1)*ang_arc/n_segs
        ang_demi=0.25*(ang1-ang0)
        t=2.66666*sin(ang_demi)*sin(ang_demi)/sin(ang_demi*2.0)
        x1=xc+cos(ang0)-t*sin(ang0)
        y1=yc+sin(ang0)+t*cos(ang0)
        x2=xc+cos(ang1)
        y2=yc+sin(ang1)
        x3=x2+t*sin(ang1)
        y3=y2-t*cos(ang1)
        P.append([[(cos(ang)*rx)*x1+(-sin(ang)*ry)*y1,
               (sin(ang)*rx)*x1+(cos(ang)*ry)*y1],
              [(cos(ang)*rx)*x3+(-sin(ang)*ry)*y3,
               (sin(ang)*rx)*x3+(cos(ang)*ry)*y3],
              [(cos(ang)*rx)*x2+(-sin(ang)*ry)*y2,
               (sin(ang)*rx)*x2+(cos(ang)*ry)*y2]])
    return P


#--------------------
# 0.3.9
#--------------------
def courbe_vers_a(c,D,n0,CP):  #A,a
    global SCALE

    l=[float(D[c[1]+1]),float(D[c[1]+2]),float(D[c[1]+3]),
        int(D[c[1]+4]),int(D[c[1]+5]),float(D[c[1]+6]),float(D[c[1]+7])]
    if c[0]=='a':
       l[5]=l[5] + CP[0]
       l[6]=l[6] + CP[1]

    B=Bez()
    B.co=[ CP[0], CP[1], CP[0], CP[1], CP[0], CP[1] ]             
    B.ha=[0,0]
    B.tag=c[0]
 
    POINTS= calc_arc (CP[0],CP[1], 
                      l[0], l[1], l[2]*(PI / 180.0),
                      l[3], l[4], 
                      l[5], l[6] )

    if DEBUG == 1  : print POINTS
    
    for p in POINTS :
        B=Bez()
        B.co=[ p[2][0],p[2][1], p[0][0],p[0][1], p[1][0],p[1][1]]             
        B.ha=[0,0]
        B.tag=c[0]
        BP=courbes.ITEM[n0].beziers_knot[-1]
        BP.co[2]=B.co[2]
        BP.co[3]=B.co[3]
        courbes.ITEM[n0].beziers_knot.append(B)
           
    BP=courbes.ITEM[n0].beziers_knot[-1]
    BP.co[2]=BP.co[0]
    BP.co[3]=BP.co[1]

    CP=[l[5], l[6]]
    return  courbes,n0,CP    

def mouvement_vers(c, D, n0,CP, proprietes):
    global DEBUG,TAGcourbe

    #l=filtre_DATA(c,D,2)
    l=[float(D[c[1]+1]),float(D[c[1]+2])]

    if c[0]=='m':
       l=[l[0]+CP[0],
           l[1] + CP[1]]

    if n0 in courbes.ITEM.keys():
       n0+=1

    CP=[l[0],l[1]] 
    courbes.ITEM[n0]=ITEM() 
    courbes.ITEM[n0].Origine=[l[0],l[1]] 

    proprietes['n'].append(n0)
    #print 'prop et item',proprietes['n'], courbes.ITEM.keys()

    B=Bez()
    B.co=[CP[0],CP[1],CP[0],CP[1],CP[0],CP[1]]
    B.ha=[0,0]
    B.tag=c[0]
    courbes.ITEM[n0].beziers_knot.append(B)
    
    if DEBUG==1: print courbes.ITEM[n0], CP    

    return  courbes,n0,CP     
    
def boucle_z(c,D,n0,CP): #Z,z
    #print c, 'close'
    #print courbes.ITEM[n0].beziers_knot
    courbes.ITEM[n0].flagUV[0]=1
    #print 'len(courbes.ITEM[n0].beziers_knot)',len(courbes.ITEM[n0].beziers_knot)
    if len(courbes.ITEM[n0].beziers_knot)>1:
        BP=courbes.ITEM[n0].beziers_knot[-1]
        BP0=courbes.ITEM[n0].beziers_knot[0]
        if BP.tag in ['c','C','s','S']: 
           BP.co[2]=BP0.co[2]  #4-5 point prec
           BP.co[3]=BP0.co[3]
           del courbes.ITEM[n0].beziers_knot[0]
    else:
     del courbes.ITEM[n0]

     n0-=1 
    return  courbes,n0,CP    

def courbe_vers_q(c,D,n0,CP):  #Q,q
    l=[float(D[c[1]+1]),float(D[c[1]+2]),float(D[c[1]+3]),float(D[c[1]+4])]
    if c[0]=='q':
       l=[l[0]+CP[0], l[1]+CP[1], l[2]+CP[0], l[3]+CP[1]]
    B=Bez()
    B.co=[l[2],  l[3],  l[2],  l[3], l[0], l[1]] #plus toucher au 2-3
    B.ha=[0,0]
    B.tag=c[0]
    BP=courbes.ITEM[n0].beziers_knot[-1]
    BP.co[2]=BP.co[0]
    BP.co[3]=BP.co[1]
    courbes.ITEM[n0].beziers_knot.append(B)
    if DEBUG==1: print B.co,BP.co

    CP=[l[2],l[3]]
    if DEBUG==1:
       pass 
    if len(D)>c[1]+5 and D[c[1]+5] not in TAGcourbe :
        c[1]+=4
        courbe_vers_q(c, D, n0,CP)
    return  courbes,n0,CP          

def courbe_vers_t(c,D,n0,CP):  #T,t 
    l=[float(D[c[1]+1]),float(D[c[1]+2])]
    if c[0]=='t':
       l=[l[0]+CP[0], l[1]+CP[1]]
          
    B=Bez()
    B.co=[l[0], l[1], l[0], l[1], l[0], l[1]] #plus toucher au 2-3
    B.ha=[0,0]
    B.tag=c[0]

    BP=courbes.ITEM[n0].beziers_knot[-1]

    l0=contruit_SYMETRIC([BP.co[0],BP.co[1],BP.co[4],BP.co[5]])

    if BP.tag in ['q','Q','t','T','m','M']:
       BP.co[2]=l0[2]
       BP.co[3]=l0[3]

    courbes.ITEM[n0].beziers_knot.append(B)
    if DEBUG==1: print B.co,BP.co

    CP=[l[0],l[1]]
    if len(D)>c[1]+3 and D[c[1]+3] not in TAGcourbe :
        c[1]+=4
        courbe_vers_t(c, D, n0,CP)    
    return  courbes,n0,CP     

#--------------------
# 0.4.3 : rewritten
#--------------------
def contruit_SYMETRIC(l):
    X=l[2]-(l[0]-l[2])
    Y=l[3]-(l[1]-l[3])
    return X,Y

def courbe_vers_s(c,D,n0,CP):  #S,s
    l=[float(D[c[1]+1]),
       float(D[c[1]+2]),
       float(D[c[1]+3]),
       float(D[c[1]+4])]
    if c[0]=='s':
       l=[l[0]+CP[0], l[1]+CP[1], 
          l[2]+CP[0], l[3]+CP[1]]
    B=Bez()
    B.co=[l[2],l[3],l[2],l[3],l[0],l[1]] #plus toucher au 2-3
    B.ha=[0,0]
    B.tag=c[0]
    BP=courbes.ITEM[n0].beziers_knot[-1]
    #--------------------
    # 0.4.3
    #--------------------
    BP.co[2],BP.co[3]=contruit_SYMETRIC([BP.co[4],BP.co[5],BP.co[0],BP.co[1]])
    courbes.ITEM[n0].beziers_knot.append(B)
    if DEBUG==1: print B.co,BP.co
    #--------------------
    # 0.4.3
    #--------------------	
    CP=[l[2],l[3]]    
    if len(D)>c[1]+5 and D[c[1]+5] not in TAGcourbe :
        c[1]+=4
        courbe_vers_c(c, D, n0,CP)       
    return  courbes,n0,CP
       
def courbe_vers_c(c, D, n0,CP): #c,C
    l=[float(D[c[1]+1]),float(D[c[1]+2]),float(D[c[1]+3]),
       float(D[c[1]+4]),float(D[c[1]+5]),float(D[c[1]+6])]
    if c[0]=='c':
       l=[l[0]+CP[0],
          l[1]+CP[1],
          l[2]+CP[0],
          l[3]+CP[1],
          l[4]+CP[0],
          l[5]+CP[1]]
    B=Bez()
    B.co=[l[4],
          l[5],
          l[4],
          l[5],
          l[2],
          l[3]] #plus toucher au 2-3
    B.ha=[0,0]
    B.tag=c[0]
    BP=courbes.ITEM[n0].beziers_knot[-1]
    BP.co[2]=l[0]
    BP.co[3]=l[1]
    courbes.ITEM[n0].beziers_knot.append(B)
    if DEBUG==1: print B.co,BP.co
    CP=[l[4],l[5]]
    if len(D)>c[1]+7 and D[c[1]+7] not in TAGcourbe :
        c[1]+=6
        courbe_vers_c(c, D, n0,CP)
    return  courbes,n0,CP
    
    
def ligne_tracee_l(c, D, n0,CP): #L,l
    l=[float(D[c[1]+1]),float(D[c[1]+2])]
    if c[0]=='l':
       l=[l[0]+CP[0],
          l[1]+CP[1]]
    B=Bez()
    B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
    B.ha=[0,0]
    B.tag=c[0]
    BP=courbes.ITEM[n0].beziers_knot[-1]
    if BP.tag in ['c','C','s','S','m','M']:
       BP.co[2]=B.co[4]
       BP.co[3]=B.co[5]
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[B.co[0],B.co[1]]
    if len(D)>c[1]+3 and D[c[1]+3] not in TAGcourbe :
        c[1]+=2
        ligne_tracee_l(c, D, n0,CP) #L
    return  courbes,n0,CP    
    
    
def ligne_tracee_h(c,D,n0,CP): #H,h
    if c[0]=='h':
       l=[float(D[c[1]+1])+float(CP[0]),CP[1]]
    else:
       l=[float(D[c[1]+1]),CP[1]]
    B=Bez()
    B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
    B.ha=[0,0]
    B.tag=c[0]
    BP=courbes.ITEM[n0].beziers_knot[-1]
    if BP.tag in ['c','C','s','S','m','M']:
        BP.co[2]=B.co[4]
        BP.co[3]=B.co[5]
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[l[0],l[1]]
    return  courbes,n0,CP    

def ligne_tracee_v(c,D,n0,CP): #V, v    
    if c[0]=='v':
       l=[CP[0], float(D[c[1]+1])+CP[1]]
    else:
       l=[CP[0], float(D[c[1]+1])]               
    B=Bez()
    B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
    B.ha=[0,0]
    B.tag=c[0]
    BP=courbes.ITEM[n0].beziers_knot[-1]
    if BP.tag in ['c','C','s','S','m','M']:
        BP.co[2]=B.co[4]
        BP.co[3]=B.co[5]
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[l[0],l[1]]
    return  courbes,n0,CP    
     
Actions=   {     "C" : courbe_vers_c,
                 "A" : courbe_vers_a, 
                 "S" : courbe_vers_s,
                 "M" : mouvement_vers,
                 "V" : ligne_tracee_v,
                 "L" : ligne_tracee_l,
                 "H" : ligne_tracee_h,                
                 "Z" : boucle_z,
                 "Q" : courbe_vers_q,
                 "T" : courbe_vers_t,

                 "c" : courbe_vers_c,
                 "a" : courbe_vers_a, 
                 "s" : courbe_vers_s,
                 "m" : mouvement_vers,
                 "v" : ligne_tracee_v,
                 "l" : ligne_tracee_l,
                 "h" : ligne_tracee_h,                
                 "z" : boucle_z,
                 "q" : courbe_vers_q,
                 "T" : courbe_vers_t
}
     
TAGcourbe=Actions.keys()
TAGtransform=['M','L','C','S','H','V','T','Q']
tagTRANSFORM=0
 
def wash_DATA(ndata):
   if ndata!='':
       while ndata[0]==' ': 
           ndata=ndata[1:]
       while ndata[-1]==' ': 
           ndata=ndata[:-1]
       if ndata[0]==',':ndata=ndata[1:]
       if ndata[-1]==',':ndata=ndata[:-1]
       #--------------------
       # 0.4.0 : 'e'
       #--------------------
       if ndata.find('-')!=-1 and ndata[ndata.find('-')-1] not in [' ', ',', 'e']:
          ndata=ndata.replace('-',',-')
       ndata=ndata.replace(',,',',')    
       ndata=ndata.replace(' ',',')
       ndata=ndata.split(',')
       for n in ndata :
          if n=='' : ndata.remove(n)	
   return ndata

#--------------------             
# 0.3.4 : - reading data rewrittten
#--------------------
def list_DATA(DATA):
    """
    cette fonction doit retourner une liste proposant
    une suite correcte de commande avec le nombre de valeurs
    attendu pour chacune d'entres-elles .
    Par exemple :
    d="'M0,14.0 z" devient ['M','0.0','14.0','z'] 
    """
    tagplace=[]
    for d in Actions.keys():
        b1=0
        b2=len(DATA)
        while DATA.find(d,b1,b2)!=-1 :
            tagplace.append(DATA.find(d,b1,b2))
            b1=DATA.find(d,b1,b2)+1
    tagplace.sort()
    tpn=range(len(tagplace)-1)
    #--------------------
    # 0.3.5 :: short data,only one tag
    #--------------------
    if len(tagplace)-1>0:
       DATA2=[]
       for t in tpn: 
          DATA2.append(DATA[tagplace[t]:tagplace[t]+1])    
          ndata=DATA[tagplace[t]+1:tagplace[t+1]]
          if DATA2[-1] not in ['z','Z'] :
             ndata=wash_DATA(ndata)
             for n in ndata : DATA2.append(n)       
       DATA2.append(DATA[tagplace[t+1]:tagplace[t+1]+1])   
       if DATA2[-1] not in ['z','Z'] and len(DATA)-1>=tagplace[t+1]+1:
          ndata=DATA[tagplace[t+1]+1:-1]
          ndata=wash_DATA(ndata)
          for n in ndata : DATA2.append(n)
    else:
        #--------------------	
        # 0.3.5 : short data,only one tag
        #--------------------
        DATA2=[]
        DATA2.append(DATA[tagplace[0]:tagplace[0]+1])
        ndata=DATA[tagplace[0]+1:]
        ndata=wash_DATA(ndata)
        for n in ndata : DATA2.append(n)
    return DATA2    

# 0.3
def translate(tx=None,ty=None):
    return [1, 0, tx], [0, 1, ty],[0,0,1]
# 0.3.2
def scale(sx=None,sy=None):
    if sy==None: sy=sx
    return [sx, 0, 0], [0, sy, 0],[0,0,1]
# 0.4.1 : transslate a in radians
def rotate(a):
    return [cos(a*3.1416/90.0), -sin(a*3.1416/90.0), 0], [sin(a*3.1416/90.0), cos(a*3.1416/90.0),0],[0,0,1]
# 0.3.2
def skewX(a):
    return [1, tan(a), 0], [0, 1, 0],[0,0,1]
# 0.4.1
def skewY(a):
    return [1, 0, 0], [tan(a), 1 , 0],[0,0,1]
# 0.3.2
def matrix(a,b,c,d,e,f):
    return [a,c,e],[b,d,f],[0,0,1]

# 0.4.2 : rewritten 
def control_CONTAINT(txt):
    """
    les descriptions de transformation peuvent être seules ou plusieurs et
    les séparateurs peuvent avoir été oubliés
    """
    t0=0
    tlist=[]
    while txt.count(')',t0)>0:
        t1=txt.find(')',t0)
        nt0=txt[t0:t1+1]
        t2=nt0[nt0.find('(')+1:-1]
        val=nt0[:nt0.find('(')]
        while t2.find('  ')!=-1:
                 t2=t2.replace('  ',' ')
        t2=t2.replace(' ',',')
        if t2.find('e'):
              t3=t2.split(',')
              t2='' 
              for t in t3 :
                   t=str(float(t)) 
              t2=str(t3).replace(']','').replace('[','').replace('\'','')
        if val=='rotate' :
           t3=t2.split(',')
           if len(t3)==3:
              tlist.append('translate('+t3[1]+','+t3[2]+')')
              tlist.append('rotate('+t3[0]+')')
              tlist.append('translate(-'+t3[1]+',-'+t3[2]+')')
        else:
              tlist.append(val+'('+t2+')')
        t0=t1+1
    return tlist

# 0.4.1 : apply transform stack
def courbe_TRANSFORM(Courbe,proprietes):
    # 1/ deplier le STACK
    #   créer une matrice pour chaque transformation    
    ST=[]
    #print proprietes['stack'] 
    for st in proprietes['stack'] :
        if st and  type(st)==list:
          for t in st:
               exec "a,b,c=%s;T=Mathutils.Matrix(a,b,c)"%control_CONTAINT(t)[0]
               ST.append(T)
        elif st :
           exec "a,b,c=%s;T=Mathutils.Matrix(a,b,c)"%control_CONTAINT(st)[0]
           ST.append(T)              
    if 'transform' in proprietes.keys():
        for trans in control_CONTAINT(proprietes['transform']):
           exec """a,b,c=%s;T=Mathutils.Matrix(a,b,c)"""%trans
           ST.append(T)
           #print ST
    ST.reverse()
    for n in proprietes['n']:
     if n in Courbe.keys():
        for bez0 in Courbe[n].beziers_knot:
          bez=bez0.co
          for b in [0,2,4]:
              for t in ST:
                 v=Mathutils.Vector([bez[b],bez[b+1],1.0])
                 #v=Mathutils.MatMultVec(t, v)
                 v=t * v
                 bez[b]=v[0]
                 bez[b+1]=v[1]          

def filtre(d):
    for nn in d:
       if '0123456789.'.find(nn)==-1:
          d=d.replace(nn,"")
    return d

def get_BOUNDBOX(BOUNDINGBOX,SVG):
    if 'viewbox' not in SVG.keys():
        h=float(filtre(SVG['height']))
        if DEBUG==1 : print 'h : ',h
        w=float(filtre(SVG['width']))
        if DEBUG==1 : print 'w :',w
        BOUNDINGBOX['rec']=[0.0,0.0,w,h]
        r=BOUNDINGBOX['rec']
        BOUNDINGBOX['coef']=w/h       
    else:
        viewbox=SVG['viewbox'].split()
        BOUNDINGBOX['rec']=[float(viewbox[0]),float(viewbox[1]),float(viewbox[2]),float(viewbox[3])]
        r=BOUNDINGBOX['rec']
        BOUNDINGBOX['coef']=(r[2]-r[0])/(r[3]-r[1])       
    return BOUNDINGBOX

# 0.4.1 : attributs ex : 'id=', 'transform=', 'd=' ...
def collecte_ATTRIBUTS(data):
    data=data.replace('  ',' ')
    ELEM={'TYPE':data[1:data.find(' ')]}
    t1=len(data)
    t2=0
    ct=data.count('="')
    while ct>0:
        t0=data.find('="',t2)
        t2=data.find(' ',t2)+1
        id=data[t2:t0]
        t2=data.find('"',t0+2)
        if id!='d':
           exec "ELEM[id]=\"\"\"%s\"\"\""%(data[t0+2:t2].replace('\\','/'))
        else:
              exec "ELEM[id]=[%s,%s]"%(t0+2,t2) 
        ct=data.count('="',t2)
    return ELEM

# --------------------------------------------
# 0.4.1 : to avoid to use sax and ths xml  
#         tools of the complete python
# --------------------------------------------
def contruit_HIERARCHIE(t):
    global CP, courbes, SCALE, DEBUG, BOUNDINGBOX, scale_, tagTRANSFORM
    TRANSFORM=0
    t=t.replace('\t',' ')
    while t.find('  ')!=-1:
          t=t.replace('  ',' ')
    n0=0      
    t0=t1=0
    baliste=[]
    balisetype=['?','?','/','/','!','!']
    BALISES=['D',  #DECL_TEXTE',
             'D',  #DECL_TEXTE',
             'F',  #FERMANTE',
             'E',  #ELEM_VIDE',
             'd',  #DOC',
             'R',  #REMARQUES',
             'C',  #CONTENU',
             'O'   #OUVRANTE'
             ]
    STACK=[]
    while t1<len(t) and t0>-1:
      t0=t.find('<',t0)
      t1=t.find('>',t0)
      ouvrante=0
      #--------------------
      # 0.4.4 , add 'else:' and 'break' to the 'if' statement
      #--------------------
      if t0>-1 and t1>-1:
          if t[t0+1] in balisetype:
             b=balisetype.index(t[t0+1])
             if t[t0+2]=='-': 
               b=balisetype.index(t[t0+1])+1
             balise=BALISES[b]
             if b==2:
                 parent=STACK.pop(-1)
                 if parent!=None and TRANSFORM>0:
                     TRANSFORM-=1
          elif t[t1-1] in balisetype:
            balise=BALISES[balisetype.index(t[t1-1])+1]
          else:
            t2=t.find(' ',t0)  
            if t2>t1: t2=t1
            ouvrante=1
            NOM=t[t0+1:t2]
            if t.find('</'+NOM)>-1:
               balise=BALISES[-1]
            else:
               balise=BALISES[-2]
          if balise=='E' or balise=='O':
             proprietes=collecte_ATTRIBUTS(t[t0:t1+ouvrante])
             if  balise=='O' and 'transform' in proprietes.keys():
                 STACK.append(proprietes['transform'])
                 TRANSFORM+=1   
             elif balise=='O' : 
                 STACK.append(None)
             proprietes['stack']=STACK[:]
             D=[] 
             if proprietes['TYPE'] in ['path'] and (proprietes['d'][1]-proprietes['d'][0]>1):
                 D=list_DATA(t[proprietes['d'][0]+t0:proprietes['d'][1]+t0])
             elif proprietes['TYPE'] in OTHERSSHAPES:
                 exec "D=%s(proprietes)"% proprietes['TYPE']
             if len(D)>0:
                 cursor=0
                 proprietes['n']=[]
                 for cell in D: 
                   if DEBUG==2 : print 'cell : ',cell ,' --'                   
                   if len(cell)>=1 and cell[0] in TAGcourbe:
                       prop=''
                       if cell[0] in ['m','M']: 
	                             prop=',proprietes'
                       exec """courbes,n0,CP=Actions[cell]([cell,cursor], D, n0,CP%s)"""%prop
                   cursor+=1
                 if TRANSFORM>0 or 'transform' in proprietes.keys() :
                     courbe_TRANSFORM(courbes.ITEM,proprietes)
             elif proprietes['TYPE'] in ['svg'] :
                   BOUNDINGBOX = get_BOUNDBOX(BOUNDINGBOX,proprietes)          
      else:
         #--------------------
         # 0.4.4 
         #--------------------
         break
      t1+=1
      t0=t1             
                        
def scan_FILE(nom):
  global CP, courbes, SCALE, DEBUG, BOUNDINGBOX, scale_, tagTRANSFORM
  dir,name=split(nom)
  name=name.split('.')
  result=0
  t=filtreFICHIER(nom)
  if t!='false':
     Blender.Window.EditMode(0)
     if not SHARP_IMPORT:
         warning = "Select Size : %t| As is %x1 | Scale on Height %x2| Scale on Width %x3" 
         scale_ = Blender.Draw.PupMenu(warning)
     # 0.4.1 : to avoid to use sax and the xml  
     #         tools of the complete python
     contruit_HIERARCHIE(t)
     r=BOUNDINGBOX['rec']
     if scale_==1:
        SCALE=1.0
     elif scale==2:
        SCALE=r[2]-r[0]
     elif scale_==3:
        SCALE=r[3]-r[1]
  courbes.number_of_items=len(courbes.ITEM.keys())
  for k in courbes.ITEM.keys():
     courbes.ITEM[k].pntsUV[0] =len(courbes.ITEM[k].beziers_knot)

  #--------------------
  # 0.4.5
  #--------------------
  CVS=2
  if BLversion>=238 : 
     warning = "CVS version you can import as : %t| Blender internal, experimental ? %x1 |  Old proofed method, import using Blender OBJ format  %x2" 
     CVS=Blender.Draw.PupMenu(warning)

  if courbes.number_of_items>0 and CVS==2:
     if len(PATTERN.keys() )>0:
        if DEBUG == 3 : print len(PATTERN.keys() )
     t=create_GEOtext(courbes)
     save_GEOfile(dir,name[0],t)
     Open_GEOfile(dir,name[0])

  elif courbes.number_of_items>0 and CVS==1 :
	   #--------------------
     # 0.4.5
     #--------------------
	   createCURVES(courbes)
	
  else:
     pass      
    
def  ajustement(v,s):
     
     a,b,c,d,e,f=float(v.co[0]),float(v.co[1]),float(v.co[2]),float(v.co[3]),float(v.co[4]),float(v.co[5])
     return [a/s,-b/s,c/s,-d/s,e/s,-f/s]

#=====================================================================
#====================== SVG format mouvements ========================
#=====================================================================

#=====================================================================
# une sorte de contournement qui permet d'utiliser la fonction
# et de documenter les variables Window.FileSelector
#=====================================================================
def fonctionSELECT(nom):
    scan_FILE(nom)

if __name__=='__main__':
   Blender.Window.FileSelector (fonctionSELECT, 'SELECT a .SVG FILE')