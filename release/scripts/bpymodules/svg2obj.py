# -*- coding: latin-1 -*-
"""
SVG 2 OBJ translater, 0.5.9b
Copyright (c) jm soler juillet/novembre 2004-mars 2007, 
# ---------------------------------------------------------------
    released under GNU Licence 
    for the Blender 2.42 Python Scripts Bundle.
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

--Old Concept : translate SVG file in GEO .obj file and try to load it. 
	was removed for the Blender 2.4x release. 
		.-- Curiousity : the original matrix must be :
		|
		|                         0.0 0.0 1.0 0.0
		|                         0.0 1.0 0.0 0.0
		|                         0.0 0.0 1.0 0.0
		|                         0.0 0.0 0.0 1.0 
		|
		|                  and not:
		|                         1.0 0.0 0.0 0.0
		|                         0.0 1.0 0.0 0.0
		|                         0.0 0.0 1.0 0.0
		|                         0.0 0.0 0.0 1.0 
		|
		'-- Possible bug : sometime, the new curves object's RotY value 
			                  jumps to -90.0 degrees without any reason.

--Options :
    SHARP_IMPORT = 0 
            choise between "As is", "Devide by height" and "Devide by width"
    SHARP_IMPORT = 1
            no choise



All commands are managed: 
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

   A : curve_to_a, 
   V : draw_line_v,
   H : draw_line_h, 
   Z : close_z,
   Q : curve_to_q,
   T : curve_to_t,
   a : curve_to_a, 
   v : draw_line_v,
   h : draw_line_h, 
   z : close_z,
   q : curve_to_q,

   transfrom for <g> tag 
   transform for <path> tag

The circle, rectangle closed or open polygons lines are managed too. 

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
      0.4.0 : - To speed up the function filter_DATA was removed and text
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
      0.4.7 : - Management of the svgz files . the complete python or the 
                gzip.py file is needed .
                Little improvement of the curve drawing using the createCURVES 
                function 
      0.4.8 : - short modif for a fantasy font case in the OOo svg format 
               ('viewbox' is written 'viewBox', for instance) .
                Note that (at this time, 2006/05/01, 1OOo exports in svg 
                but does not read its own export 
      0.4.9 : - skipped version : private test 
      0.5.0 : - the script worked perfectly with Blender 2.41 but in Blender 
                2.42, use the original svg name file + 'OOO.obj' to
                write a videoscape file made blender crash under window XP when 
                the script loaded it . Curiously, use a more simple 
                name with a sole 'O' solved this problem .
              - script returned errors on open path : corrected 
              - in b2.42, several successive	imports seem to be added to 
                the same original curve . So now the script automaticaly 
                renames the  last group of imported curve with the original 
                name file .
      0.5.1 : - without join option in the internal curve creation function
      0.5.2 : - the createCURVES() function has been cleanded . Now it works
                fine but all bezier curves are joined in the same curve object .
      0.5.3 : - removed two things :
                  1/ the ajustement function to increase speed . 35 % faster :
                      5690 curves and 30254 points in 11 seconds . User should do 
                      a ctrl-a on the object .
                  2/ the import method menu . No reason to choose between the
                     old extern curve creat and the new intern curve creation 
                     this last one is largely faster .
     0.5.4 : - translation of the functions' name + improvment in the dict lookup .
               Quite 15% faster . 9.75 seconds instead of 11 to load the file test . 
               A test was also added  to find the fill style so now the script closes
               these curves even if they are not defined as closed  in the strict path
               commands .  
               The old non used functions have been completely removed .
     0.5.5 : - Modifs for architect users .
     0.5.6 : - Exec was removed from the collect_ATTRIBUTS function .
               Other uses was evaluated.
     0.5.7 : - Wash down of some handle problems. 

     0.5.8 : - 2007/3/9
             Wash down of the last exec and correction of a
             problem with the curve's first beztriple handle 
             which was not recorded at first time . 
            - Added some units managements
            - Correction of the  rotate matrix 
            - Correction of the  skew  matrix 
            - change in the wash_DATA function suggested by cambo 
            - added __slot__ in class Bez, ITEM and CURVE suggested by cambo
            - remove unused properties in class ITEM and CURVE
 
     0.5.9 : - 2007/3/28
             -  many improvements for faster and clearer code suggested by cambo and martin.
                replacement of "%s" statement by str function.
             -  correction of an error in the scale transform management 
             -  correction in the management of the stack transformation that rise an error 
                under python 2.5 but curiously not with  python 2.4

    0.5.9a : - 2007/3/29
             -  Again a lot of minors corrections 
             -  Backward to 0.5.8 of the function that manages float numbers exported
                by the  Adobe Illustrator's SVG.  After a lot of tests it seems that this oldest
                version is also faster too .
             -  correction (bad) on handle management with V and H commands.  
    0.5.9b : - 2007/3/31
            -  one or two minor corrections :
               now the new object curve is added in the current layer.
               short modif in the scale menu...

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
	if os.sep in pathname:
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

def filterFILE(nom):
	"""
	Function  filterFILE

	in  : string  nom , filename
	out : string  t   , if correct filecontaint
	
	read the file's content and try to see if the format
	is correct .
	
	Lit le contenu du fichier et en fait une pre-analyse 
	pour savoir s'il merite d'etre traite .
	"""
	# ----------
	# 0.4.7 
	# ----------
	if nom.upper().endswith('.SVGZ'):
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
	#--------------------		
	#	may be needed in some import case when the 
	# file is saved from a mozilla display
	#--------------------
	t=t.replace(chr(0),'')	
	if not '<SVG' in t.upper(): 
		name = "ERROR: invalid or empty file ... "  # if no %xN int is set, indices start from 1
		result = Blender.Draw.PupMenu(name)
		return "false"
	else:
		return t

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

class Bez(object):
	__slots__ = 'co', 'ha', 'tag' # suggested by cambo, should save memory
	def __init__(self):
		self.co=[]
		self.ha=['C','C']
		self.tag=''

class ITEM(object):
	__slots__ =	'type', 'pntsUV', 'flagUV', 'beziers_knot','fill'
	def __init__(self):
		self.type        =  typBEZIER3D        
		self.pntsUV      =  [0,0]              
		self.flagUV      =  [0,0]              
		self.beziers_knot = []
		self.fill=0
		#self.color=[0.0,0.0,0.0]

class CURVE(object):
	__slots__ =	'type','number_of_items','ITEM'
	def __init__(self):
		self.type            =  objBEZIER        
		self.number_of_items =  0              
		self.ITEM = {}

curves=CURVE()
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
#=====  needed for cyclic definition in AI, EPS forma  ================
#=====================================================================
def test_samelocations(f1,f2):
	EPSILON=0.0001
	if abs(f1[4])- abs(f2[4])< EPSILON and abs(f1[4])- abs(f2[4])>= 0.0\
		and abs(f1[5])-abs(f2[5])< EPSILON and abs(f1[5])-abs(f2[5])>= 0.0 :
		return 1
	else:
		return 0


#--------------------
# 0.4.5 : for blender cvs 2.38 ....
#--------------------
def createCURVES(curves, name):
	"""
	internal curves creation 
	"""
	global SCALE, B, BOUNDINGBOX,scale_
	from Blender import Curve, Object, Scene, BezTriple
	HANDLE={'C':BezTriple.HandleTypes.FREE,'L':BezTriple.HandleTypes.VECT}
	r=BOUNDINGBOX['rec']
	
	if scale_==3:
		SCALE=1.0
	elif scale_==1:
		SCALE=r[2]-r[0]
	elif scale_==2:
		SCALE=r[3]-r[1]
	
	scene = Scene.GetCurrent()
	scene.objects.selected = [] #059b
	c = Curve.New()         #059b
	
	c.setResolu(24)  
	for I,val in curves.ITEM.iteritems():
		bzn=0
		if val.beziers_knot[-1].tag in ['L','l','V','v','H','h'] and\
		test_samelocations(val.beziers_knot[-1].co,val.beziers_knot[0].co):
			del val.beziers_knot[-1]
			#print 'remove last point', rmp
			#rmp+=1 
		for k2 in xrange(0,len(val.beziers_knot)):
			bz= [co for co in val.beziers_knot[k2].co] 
			if bzn==0:
				cp1 =  bz[4],bz[5],0.0 , bz[0],bz[1],0.0, bz[2],bz[3],0.0, 
				beztriple1 = BezTriple.New(cp1)
				bez = c.appendNurb(beztriple1)
				bez[0].handleTypes=(HANDLE[val.beziers_knot[k2].ha[0]],HANDLE[val.beziers_knot[k2].ha[1]])              
				bzn = 1
			else:
				cp2 =  bz[4],bz[5],0.0 , bz[0],bz[1],0.0, bz[2],bz[3],0.0
				beztriple2 = BezTriple.New(cp2)
				beztriple2.handleTypes= (HANDLE[val.beziers_knot[k2].ha[0]],HANDLE[val.beziers_knot[k2].ha[1]])
				bez.append(beztriple2)
		if val.flagUV[0]==1 or val.fill==1:
			#--------------------
			# 0.4.6 : cyclic flag ...
			#--------------------
			bez.flagU += 1
	ob = scene.objects.new(c,name)   #059b
	scene.objects.active = ob        #059b
	ob.setSize(1.0/SCALE,1.0/-SCALE,1.0)
	c.update()


#=====================================================================
#=====      SVG format   :  DEBUT             =========================
#=====================================================================
#--------------------
# 0.5.8, needed with the new 
#        tranform evaluation 
#--------------------
pxUNIT={'pt':1.25,
				'pc':15.0,
				'mm':3.543307,
				'cm':35.43307,
				'in':90.0,	
				'em':1.0, # should be taken from font size 
									# but not currently managed 
				'ex':1.0, # should be taken from font size 
									# but not currently managed 			
				'%':1.0,
				}

#--------------------
# 0.4.2
# 0.5.8, to remove exec 
#--------------------
def rect(prp):
	"""
	build rectangle paths
	"""
	D=[]
	if 'x' not in prp: x=0.0
	else	: x=float(prp['x'])
	if 'y' not in prp: y=0.0
	else	: y=float(prp['y'])
	#--------------------
	# 0.5.8
	#--------------------
	try:
		height=float(prp['height'])
	except:
		pxUNIT['%']=(BOUNDINGBOX['rec'][3]-BOUNDINGBOX['rec'][1])/100.0	
		for key in pxUNIT:#.keys(): 
			if key in prp['height']: 
				height=float(prp['height'].replace(key,''))*pxUNIT[key]
	try:
		width=float(prp['width'])
	except:
		pxUNIT['%']=(BOUNDINGBOX['rec'][2]-BOUNDINGBOX['rec'][0])/100.0
		for key in pxUNIT:#.keys(): 
			if key in prp['width']:
					width=float(prp['width'].replace(key,''))*pxUNIT[key]
	#--------------------
	# 0.5.8, end
	#--------------------
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
	if 'rx' not in prp or 'rx' not in prp: 
		D=['M',str(x),str(y),'h',str(width),'v',str(height),'h',str(-width),'z']
	else :
		rx=float(prp['rx'])
		if 'ry' not in prp : 
			ry=float(prp['rx'])
		else :	ry=float(prp['ry'])
		if 'rx' in prp and prp['rx']<0.0: rx*=-1
		if 'ry' in prp and prp['ry']<0.0: ry*=-1
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

		D=['M',str(x+rx),str(y),
				'h',str(width-2*rx),
				'c',str(rx),'0.0',str(rx),str(ry),str(rx),str(ry),
				'v',str(height-ry),
				'c','0.0',str(ry),str(-rx),str(ry),str(-rx),str(ry),
				'h',str(-width+2*rx),
				'c',str(-rx),'0.0',str(-rx),str(-ry),str(-rx),str(-ry),
				'v',str(-height+ry),
				'c','0.0','0.0','0.0',str(-ry),str(rx),str(-ry),
				'z']                   
			
	return D

#--------------------
# 0.4.2
# 0.5.8, to remove exec 
#--------------------
def circle(prp):
	if 'cx' not in prp: cx=0.0	
	else : cx =float(prp['cx'])
	if 'cy' not in prp: cy=0.0
	else : cy =float(prp['cy'])
	#print 	prp.keys()
	r = float(prp['r'])
	D=['M',str(cx),str(cy+r),                  
			'C',str(cx-r),       str(cy+r*0.552),str(cx-0.552*r),str(cy+r),      str(cx),str(cy+r), 
			'C',str(cx+r*0.552), str(cy+r),      str(cx+r),      str(cy+r*0.552),   str(cx+r),str(cy), 
			'C',str(cx+r),       str(cy-r*0.552),str(cx+r*0.552),str(cy-r),str(cx), str(cy-r),
			'C',str(cx-r*0.552), str(cy-r),      str(cx-r),      str(cy-r*0.552),str(cx-r),str(cy),
			'Z']
	return D

#--------------------
# 0.4.2
# 0.5.8, to remove exec 
#--------------------
def ellipse(prp):
	if 'cx' not in prp: cx=0.0	
	else : cx =float(prp['cx'])
	if 'cy' not in prp: cy=0.0
	else : cy =float(prp['cy'])
	ry = float(prp['rx'])
	rx = float(prp['ry'])
	D=['M',str(cx),str(cy+rx), 
			'C',str(cx-ry),str(cy+rx*0.552),str(cx-0.552*ry),str(cy+rx),str(cx),str(cy+rx),
			'C',str(cx+ry*0.552),str(cy+rx),str(cx+ry),str(cy+rx*0.552),str(cx+ry),str(cy),
			'C',str(cx+ry),str(cy-rx*0.552),str(cx+ry*0.552),str(cy-rx),str(cx),str(cy-rx),
			'C',str(cx-ry*0.552),str(cy-rx),str(cx-ry),str(cy-rx*0.552),str(cx-ry),str(cy),
			'z']
	return D 

#--------------------
# 0.4.2
# 0.5.8, to remove exec 
#--------------------
def line(prp):
	D=['M',str(prp['x1']),str(prp['y1']),
			'L',str(prp['x2']),str(prp['y2'])]
	return D

#--------------------
# 0.4.2
# 0.5.8, to remove exec 
#--------------------    
def polyline(prp):
	if 'points' in  prp:
		points=prp['points'].split(' ')
		np=0
		for p in points:
			if p!='':
				p=p.split(',')
				if np==0:
					D=['M',str(p[0]),str(p[1])]
					np+=1
				else:
					D.append('L');D.append(str(p[0]));D.append(str(p[1]))
		return D
	else:
		return []

#--------------------
# 0.4.2
# 0.5.8, to remove exec 
#--------------------	
def polygon(prp):
	D=polyline(prp)
	if D!=[]:
		D.append('Z')
	return D

#--------------------
# 0.5.8, to remove exec 
#--------------------
OTHERSSHAPES={ 'rect'    :  rect,
							 'line'    :  line, 
							 'polyline':  polyline, 
							 'polygon' :  polygon,
							 'circle'  :  circle,
							 'ellipse' :  ellipse}

#--------------------
# 0.3.9
#--------------------
def calc_arc (cpx,cpy, rx, ry,  ang, fa , fs , x, y) :
	"""
	Calc arc paths
	"""
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
	for i in xrange(n_segs):
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
def curve_to_a(c,D,n0,CP):  #A,a
	global SCALE
	l=[float(D[c[1]+1]),float(D[c[1]+2]),float(D[c[1]+3]),
			int(D[c[1]+4]),int(D[c[1]+5]),float(D[c[1]+6]),float(D[c[1]+7])]
	if c[0]=='a':
		l[5]=l[5] + CP[0]
		l[6]=l[6] + CP[1]
	B=Bez()
	B.co=[ CP[0], CP[1], CP[0], CP[1], CP[0], CP[1] ]             
	B.ha=['C','C']
	B.tag=c[0]
	POINTS= calc_arc (CP[0],CP[1], 
										l[0], l[1], l[2]*(PI / 180.0),
										l[3], l[4], 
										l[5], l[6] )    
	if DEBUG == 1  : print POINTS    
	for p in POINTS :
		B=Bez()
		B.co=[ p[2][0],p[2][1], p[0][0],p[0][1], p[1][0],p[1][1]]             
		B.ha=['C','C']
		B.tag='C'
		BP=curves.ITEM[n0].beziers_knot[-1]
		BP.co[2]=B.co[2]
		BP.co[3]=B.co[3]
		curves.ITEM[n0].beziers_knot.append(B)
	BP=curves.ITEM[n0].beziers_knot[-1]
	BP.co[2]=BP.co[0]
	BP.co[3]=BP.co[1]
	CP=[l[5], l[6]]
	return  curves,n0,CP    

def move_to(c, D, n0,CP, proprietes):
	global DEBUG,TAGcourbe
	l=[float(D[c[1]+1]),float(D[c[1]+2])]
	if c[0]=='m':
		l=[l[0]+CP[0],
				l[1] + CP[1]]
	if n0 in curves.ITEM:
		n0+=1
	CP=[l[0],l[1]] 
	curves.ITEM[n0]=ITEM() 
	proprietes['n'].append(n0)
	B=Bez()
	B.co=[CP[0],CP[1],CP[0],CP[1],CP[0],CP[1]]
	B.ha=['L','C']
	B.tag=c[0]
	curves.ITEM[n0].beziers_knot.append(B)
	if DEBUG==1: print curves.ITEM[n0], CP    
	return  curves,n0,CP     

def close_z(c,D,n0,CP): #Z,z
	curves.ITEM[n0].flagUV[0]=1
	if len(curves.ITEM[n0].beziers_knot)>1:
		#print 	len(curves.ITEM[n0].beziers_knot)
		BP=curves.ITEM[n0].beziers_knot[-1]
		BP0=curves.ITEM[n0].beziers_knot[0]
		if BP.tag in ['c','C','s','S',]: 
			BP.co[2]=BP0.co[2]  #4-5 point prec
			BP.co[3]=BP0.co[3]          
			del curves.ITEM[n0].beziers_knot[0]
	else:
		del curves.ITEM[n0]
		n0-=1 
	return  curves,n0,CP    

def curve_to_q(c,D,n0,CP):  #Q,q
	l=[float(D[c[1]+1]),float(D[c[1]+2]),float(D[c[1]+3]),float(D[c[1]+4])]
	if c[0]=='q':
		l=[l[0]+CP[0], l[1]+CP[1], l[2]+CP[0], l[3]+CP[1]]
	B=Bez()
	B.co=[l[2],  l[3],  l[2],  l[3], l[0], l[1]] #plus toucher au 2-3
	B.ha=['C','C']
	B.tag=c[0]
	BP=curves.ITEM[n0].beziers_knot[-1]
	BP.co[2]=BP.co[0]
	BP.co[3]=BP.co[1]
	curves.ITEM[n0].beziers_knot.append(B)
	if DEBUG==1: print B.co,BP.co
	CP=[l[2],l[3]]
	if DEBUG==1:
		pass 
	if len(D)>c[1]+5 and D[c[1]+5] not in TAGcourbe :
		c[1]+=4
		curve_to_q(c, D, n0,CP)
	return  curves,n0,CP          

def curve_to_t(c,D,n0,CP):  #T,t 
	l=[float(D[c[1]+1]),float(D[c[1]+2])]
	if c[0]=='t':
		l=[l[0]+CP[0], l[1]+CP[1]]         
	B=Bez()
	B.co=[l[0], l[1], l[0], l[1], l[0], l[1]] #plus toucher au 2-3
	B.ha=['C','C']
	B.tag=c[0]
	BP=curves.ITEM[n0].beziers_knot[-1]
	l0=build_SYMETRIC([BP.co[0],BP.co[1],BP.co[4],BP.co[5]])
	if BP.tag in ['q','Q','t','T','m','M']:
		BP.co[2]=l0[2]
		BP.co[3]=l0[3]
	curves.ITEM[n0].beziers_knot.append(B)
	if DEBUG==1: print B.co,BP.co
	CP=[l[0],l[1]]
	if len(D)>c[1]+3 and D[c[1]+3] not in TAGcourbe :
		c[1]+=4
		curve_to_t(c, D, n0,CP)    
	return  curves,n0,CP     

#--------------------
# 0.4.3 : rewritten
#--------------------
def build_SYMETRIC(l):
	X=l[2]-(l[0]-l[2])
	Y=l[3]-(l[1]-l[3])
	return X,Y

def curve_to_s(c,D,n0,CP):  #S,s
	l=[float(D[c[1]+1]),
			float(D[c[1]+2]),
			float(D[c[1]+3]),
			float(D[c[1]+4])]
	if c[0]=='s':
		l=[l[0]+CP[0], l[1]+CP[1], 
			l[2]+CP[0], l[3]+CP[1]]
	B=Bez()
	B.co=[l[2],l[3],l[2],l[3],l[0],l[1]] #plus toucher au 2-3
	B.ha=['C','C']
	B.tag=c[0]
	BP=curves.ITEM[n0].beziers_knot[-1]
	#--------------------
	# 0.4.3
	#--------------------
	BP.co[2],BP.co[3]=build_SYMETRIC([BP.co[4],BP.co[5],BP.co[0],BP.co[1]])
	curves.ITEM[n0].beziers_knot.append(B)
	if DEBUG==1: print B.co,BP.co
	#--------------------
	# 0.4.3
	#--------------------	
	CP=[l[2],l[3]]    
	if len(D)>c[1]+5 and D[c[1]+5] not in TAGcourbe :
		c[1]+=4
		curve_to_c(c, D, n0,CP)       
	return  curves,n0,CP

def curve_to_c(c, D, n0,CP): #c,C
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
	B.ha=['C','C']
	B.tag=c[0]
	BP=curves.ITEM[n0].beziers_knot[-1]
	BP.co[2]=l[0]
	BP.co[3]=l[1]
	BP.ha[1]='C'
	curves.ITEM[n0].beziers_knot.append(B)
	if DEBUG==1: print B.co,BP.co
	CP=[l[4],l[5]]
	if len(D)>c[1]+7 and D[c[1]+7] not in TAGcourbe :
		c[1]+=6
		curve_to_c(c, D, n0,CP)
	return  curves,n0,CP

def draw_line_l(c, D, n0,CP): #L,l
	l=[float(D[c[1]+1]),float(D[c[1]+2])]
	if c[0]=='l':
		l=[l[0]+CP[0],
				l[1]+CP[1]]
	B=Bez()
	B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
	B.ha=['L','L']
	B.tag=c[0]
	BP=curves.ITEM[n0].beziers_knot[-1]
	BP.ha[1]='L'
	curves.ITEM[n0].beziers_knot.append(B)    
	CP=[B.co[0],B.co[1]]
	if len(D)>c[1]+3 and D[c[1]+3] not in TAGcourbe :
		c[1]+=2
		draw_line_l(c, D, n0,CP) #L
	return  curves,n0,CP    

def draw_line_h(c,D,n0,CP): #H,h
	if c[0]=='h':
		l=[float(D[c[1]+1])+float(CP[0]),CP[1]]
	else:
		l=[float(D[c[1]+1]),CP[1]]
	B=Bez()
	B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
	B.ha=['L','L']
	B.tag=c[0]
	#BP=curves.ITEM[n0].beziers_knot[-1]
	#BP.ha[0]='L'
	curves.ITEM[n0].beziers_knot.append(B)    
	CP=[l[0],l[1]]
	return  curves,n0,CP    

def draw_line_v(c,D,n0,CP): #V, v    
	if c[0]=='v':
		l=[CP[0], float(D[c[1]+1])+CP[1]]
	else:
		l=[CP[0], float(D[c[1]+1])]    
		           
	B=Bez()
	B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
	B.ha=['L','L']
	B.tag=c[0]
	#BP=curves.ITEM[n0].beziers_knot[-1]
	#BP.ha[0]='L'
	curves.ITEM[n0].beziers_knot.append(B)    
	CP=[l[0],l[1]]
	return  curves,n0,CP    

Actions=   {     "C" : curve_to_c,
									"A" : curve_to_a, 
									"S" : curve_to_s,
									"M" : move_to,
									"V" : draw_line_v,
									"L" : draw_line_l,
									"H" : draw_line_h,                
									"Z" : close_z,
									"Q" : curve_to_q,
									"T" : curve_to_t,

									"c" : curve_to_c,
									"a" : curve_to_a, 
									"s" : curve_to_s,
									"m" : move_to,
									"v" : draw_line_v,
									"l" : draw_line_l,
									"h" : draw_line_h,                
									"z" : close_z,
									"q" : curve_to_q,
									"T" : curve_to_t
}
     
TAGcourbe=Actions.keys()
TAGtransform=['M','L','C','S','H','V','T','Q']
tagTRANSFORM=0
 
def wash_DATA(ndata):	
	if ndata:
		if DEBUG==1: print ndata
		ndata = ndata.strip()
		if ndata[0]==',':ndata=ndata[1:]
		if ndata[-1]==',':ndata=ndata[:-1]
		#--------------------
		# 0.4.0 : 'e'
		#--------------------
		i = ndata.find('-')
		if i != -1 and ndata[i-1] not in ' ,e':
			ndata=ndata.replace('-',',-')
		ndata=ndata.replace(',,',',')    
		ndata=ndata.replace(' ',',')
		ndata=ndata.split(',')		
		ndata=[i for i in ndata if i]  #059a
				
	return ndata

#--------------------             
# 0.3.4 : - read data rewrittten
#--------------------
def list_DATA(DATA):
	"""
	This function translate a text in a list of 
	correct commandswith the right number of waited 
	values	for each of them .  For example  :
	d="'M0,14.0 z" becomes ['M','0.0','14.0','z'] 
	"""
	# ----------------------------------------
	#  borner les differents segments qui devront etre
	#  traites
	#  pour cela construire une liste avec chaque 
	#  la position de chaqe emplacement tag de type 
	#  commande path...
	# ----------------------------------------
	tagplace=[]
	for d in Actions:
		b1=0
		while True:
			i = DATA.find(d,b1)
			if i==-1: break
			tagplace.append(i)
			b1=i+1
	#------------------------------------------
	# cette liste doit etre traites dans l'ordre
	# d'apparition des tags 
	#------------------------------------------
	tagplace.sort()

	tpn=range(len(tagplace))
	#--------------------
	# 0.3.5 :: short data, only one tag
	#--------------------
	if len(tagplace)-1>0:
		DATA2=[]
		for t in tpn[:-1]: 
			DATA2.append(DATA[tagplace[t]:tagplace[t]+1])    
			ndata=DATA[tagplace[t]+1:tagplace[t+1]]
			if DATA2[-1] not in ['z','Z'] :
				ndata=wash_DATA(ndata)
				DATA2.extend(ndata)
		DATA2.append(DATA[tagplace[t+1]:tagplace[t+1]+1])  
		if DATA2[-1] not in ['z','Z'] and len(DATA)-1>=tagplace[t+1]+1:
			ndata=DATA[tagplace[t+1]+1:]
			ndata=wash_DATA(ndata)
			DATA2.extend(ndata) #059a
	else:
		#--------------------	
		# 0.3.5 : short data,only one tag
		#--------------------
		DATA2=[]
		DATA2.append(DATA[tagplace[0]:tagplace[0]+1])
		ndata=DATA[tagplace[0]+1:]
		ndata=wash_DATA(ndata)
		DATA2.extend(ndata)
	return DATA2    
	 
#----------------------------------------------
# 0.3
# 0.5.8, to remove exec 
#----------------------------------------------
def translate(t):
	tx=t[0]
	ty=t[1]
	return [1, 0, tx], [0, 1, ty],[0,0,1]

#----------------------------------------------
# 0.3.2
# 0.5.8, to remove exec 
#----------------------------------------------
def scale(s):
	sx=s[0]
	if len(s)>1: sy=s[1]
	else: sy=sx
	return [sx, 0, 0], [0, sy, 0],[0,0,1]

#----------------------------------------------
# 0.4.1 : transslate a in radians
# 0.5.8, to remove exec 
#----------------------------------------------
def rotate(t):
	a=t[0]	
	return [cos(a*3.1416/180.0), -sin(a*3.1416/180.0), 0], [sin(a*3.1416/180.0), cos(a*3.1416/180.0),0],[0,0,1]

#----------------------------------------------
# 0.3.2
# 0.5.8, to remove exec 
#----------------------------------------------
def skewx(t):
	a=t[0]
	return [1, tan(a*3.1416/180.0), 0], [0, 1, 0],[0,0,1]

#----------------------------------------------
# 0.4.1
# 0.5.8, to remove exec 
#----------------------------------------------
def skewy(t):
	a=t[0]
	return [1, 0, 0], [tan(a*3.1416/180.0), 1 , 0],[0,0,1]

#----------------------------------------------
# 0.3.2
# 0.5.8, to remove exec 
#----------------------------------------------
def matrix(t):
	a,b,c,d,e,f=t
	return [a,c,e],[b,d,f],[0,0,1]

#--------------------
# 0.5.8, to remove exec 
#--------------------
matrixTRANSFORM={ 'translate':translate,
										'scale':scale,
										'rotate':rotate,
										'skewx':skewx,
										'skewy':skewy,
										'matrix':matrix
									}

#----------------------------------------------
# 0.4.2 : rewritten 
# 0.5.8 : to remove exec uses.
#----------------------------------------------
def control_CONTAINT(txt):
	"""
	the transforms' descriptions can be sole or several
	and separators might be forgotten
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
		
		"""
		t2=t2.split(',')		
		for index, t in enumerate(t2):
				t2[index]=float(t)
		"""
		t2=[float(t) for t in t2.split(',')]		
		
		if val=='rotate' :
			t3=t2
			if len(t3)==3:
				tlist.append(['translate',[t3[1],t3[2]]])
				tlist.append(['rotate',[t3[0]/180.0*3.1416]])
				tlist.append(['translate',[-t3[1],-t3[2]]])
			else:
				tlist.append(['rotate',[t3[0]]])
		else:
			tlist.append([val,t2])         
		t0=t1+1
	return tlist

def curve_FILL(Courbe,proprietes):
	for n in proprietes['n']:
		if n in Courbe and 'fill:#' in proprietes['style']:
			Courbe[n].fill=1

#----------------------------------------------
# 0.4.1 : apply transform stack
#----------------------------------------------
def curve_TRANSFORM(Courbe,proprietes):
	# 1/ unpack the STACK
	#   create a matrix for each transform    
	ST=[]
	for st in proprietes['stack'] :
		if st and  type(st)==list:
			for t in st:
				code =	control_CONTAINT(t)
				a,b,c=matrixTRANSFORM[code[0][0]](code[0][1][:])
				T=Mathutils.Matrix(a,b,c)
				ST.append(T)
		elif st :
			code =	control_CONTAINT(st)
			a,b,c=matrixTRANSFORM[code[0][0]](code[0][1][:])
			T=Mathutils.Matrix(a,b,c)
			ST.append(T)              
	if 'transform' in proprietes:
			for trans in control_CONTAINT(proprietes['transform']):
				#--------------------
				# 0.5.8, to remove exec 
				#--------------------
				a,b,c=matrixTRANSFORM[trans[0].strip()](trans[1][:])  #059
				T=Mathutils.Matrix(a,b,c)
				ST.append(T)
	ST.reverse()
	for n in proprietes['n']:
		if n in Courbe:
			for bez0 in Courbe[n].beziers_knot:
				bez=bez0.co
				for b in [0,2,4]:
					for t in ST:
						v=t * Mathutils.Vector([bez[b],bez[b+1],1.0]) #059a
						bez[b]=v[0]
						bez[b+1]=v[1]          

def filter(d):
	for nn in d:
		if nn not in '0123456789.': #059a
			d=d.replace(nn,"")
	return d

def get_BOUNDBOX(BOUNDINGBOX,SVG):
	if 'viewbox' not in SVG:
		h=float(filter(SVG['height']))
		if DEBUG==1 : print 'h : ',h
		w=float(filter(SVG['width']))
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

#----------------------------------------------
# 0.4.1 : attributs ex : 'id=', 'transform=', 'd=' ...
#----------------------------------------------
def collect_ATTRIBUTS(data):
	#----------------------------------------------
	# 0.4.8 : short modif for a fantasy font case  
	#         in the OOo svg format ('viewbox'  is 
	#         written 'viewBox', for instance)
	#----------------------------------------------
	data=data.replace('  ',' ').lower()
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
			ELEM[id]=data[t0+2:t2].replace('\\','/')
		else:
			ELEM[id]=[]
			ELEM[id].append(t0+2)
			ELEM[id].append(t2)
		ct=data.count('="',t2)
	return ELEM

# --------------------------------------------
# 0.4.1 : to avoid to use sax and ths xml  
#         tools of the complete python
# --------------------------------------------
def build_HIERARCHY(t):
	global CP, curves, SCALE, DEBUG, BOUNDINGBOX, scale_, tagTRANSFORM

	TRANSFORM=0
	t=t.replace('\t',' ')
	while t.find('  ')!=-1: t=t.replace('  ',' ')
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
					#print t[t0:t1]
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
				if '</'+NOM in t: #.find('</'+NOM)>-1:
					balise=BALISES[-1]
				else:
					balise=BALISES[-2]
			if balise=='E' or balise=='O':
				proprietes=collect_ATTRIBUTS(t[t0:t1+ouvrante])
				if  balise=='O' and 'transform' in proprietes:
					STACK.append(proprietes['transform'])
					TRANSFORM+=1   
				elif balise=='O' : 
					STACK.append(None)
				proprietes['stack']=STACK[:]
				D=[] 
				if proprietes['TYPE'] in ['path'] and (proprietes['d'][1]-proprietes['d'][0]>1):
					D=list_DATA(t[proprietes['d'][0]+t0:proprietes['d'][1]+t0])
				elif proprietes['TYPE'] in OTHERSSHAPES:
					#--------------------
					# 0.5.8, to remove exec 
					#--------------------
					D=OTHERSSHAPES[proprietes['TYPE']](proprietes)
				if len(D)>0:
					cursor=0
					proprietes['n']=[]
					for cell in D: 
						if DEBUG==2 : print 'cell : ',cell ,' --'                   
						if len(cell)>=1 and cell[0] in TAGcourbe:
							#--------------------
							# 0.5.8, to remove exec 
							#--------------------
							if cell[0] in ['m','M']: 
								curves,n0,CP=Actions[cell]([cell,cursor], D, n0,CP,proprietes)
							else: 
								curves,n0,CP=Actions[cell]([cell,cursor], D, n0,CP)
						cursor+=1
					if TRANSFORM>0 or 'transform' in proprietes :
						curve_TRANSFORM(curves.ITEM,proprietes)
					if 'style' in proprietes :
						curve_FILL(curves.ITEM,proprietes)
				elif proprietes['TYPE'] in ['svg'] :
					#print  'proprietes.keys()',proprietes.keys()
					BOUNDINGBOX = get_BOUNDBOX(BOUNDINGBOX,proprietes)
		else:
			#--------------------
			# 0.4.4 
			#--------------------
			break
		t1+=1
		t0=t1             

def scan_FILE(nom):
	global CP, curves, SCALE, DEBUG, BOUNDINGBOX, scale_, tagTRANSFORM
	dir,name=split(nom)
	name=name.split('.')
	result=0
	#Choise=1
	t1=Blender.sys.time()
	t=filterFILE(nom)
	if t!='false':
		Blender.Window.EditMode(0)
		if not SHARP_IMPORT:
			warning = "Select Size : %t|  Scale on Width %x1|   Scale on Height %x2| As is (caution may be large)  %x3" 
			scale_ = Blender.Draw.PupMenu(warning)
		t1=Blender.sys.time()
		# 0.4.1 : to avoid to use sax and the xml  
		#         tools of the complete python
		build_HIERARCHY(t)
		r=BOUNDINGBOX['rec']
		"""
		if scale_==3:
			SCALE=1.0
		elif scale==1:
			SCALE=r[2]-r[0]
		elif scale_==2:
			SCALE=r[3]-r[1]
		"""	
	curves.number_of_items=len(curves.ITEM)
	for k, val in curves.ITEM.iteritems():
		val.pntsUV[0] =len(val.beziers_knot)
	if curves.number_of_items>0 : #and Choise==1 :
		#--------------------
		# 0.4.5 and 0.4.9 
		#--------------------
		createCURVES(curves, name[0])
	else:
		pass
	print ' elapsed time : ',Blender.sys.time()-t1
	Blender.Redraw()

#=====================================================================
#====================== SVG format mouvements ========================
#=====================================================================
def functionSELECT(nom):
	scan_FILE(nom)


if __name__=='__main__':
	Blender.Window.FileSelector (functionSELECT, 'SELECT an .SVG FILE', '*.svg')