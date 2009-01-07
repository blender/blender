# -*- coding: latin-1 -*-
"""
paths_ai2obj.py
# ---------------------------------------------------------------
Copyright (c) jm soler juillet/novembre 2004-april 2007, 
# ---------------------------------------------------------------
    released under GNU Licence 
    for the Blender 2.45 Python Scripts Bundle.
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
#----------------------------------------------
#  
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_import_ai_en.htm
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#----------------------------------------------

#Changelog
#----------------------------------------------
# 0.1.1 : 2004/08/03, bug in boundingbox reading when Value are negative
# 0.1.2 : 2005/06/12, gmove tranformation properties
# 0.1.3 : 2005/06/25, added a __name__ test to use the script alone
# 0.1.4 : 2005/06/25, closepath improvements 
# 0.1.5 : 2005/06/25, ... 
# 0.1.6 : 2005/06/26, warning for compacted file 
                      compatibility increased up to AI 10.0 plain text 
# 0.1.7 : 2005/06/25, two more closepath improvements 
#
# 0.1.8 : 2006/07/03, two more closepath improvements 
# 0.1.9 : 2007/05/06, modif on the method that gets the last object on 
                      the list data
#         2008/03/12, Added character encoding line so french text
#                       does not break python interpreters.

"""
SHARP_IMPORT=0
SCALE=1
NOTHING_TODO=1
AI_VERSION=''

GSTACK          =   []
GSCALE          =   []
GTRANSLATE      =   []

import sys
#oldpath=sys.path
import Blender
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
     f=open(nom,'rU')
     t=f.readlines()
     f.close()
     
     if len(t)>1 and t[0].find('EPSF')==-1: 
          return t   
     else:
         name = "OK?%t| Not a valid file or an empty file ... "  # if no %xN int is set, indices start from 1
         result = Blender.Draw.PupMenu(name)
          
         return 'false'
        
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
               self.type        =  typBEZIER3D,         
               self.pntsUV      =  [0,0]              
               self.resolUV     =  [32,0]             
               self.orderUV     =  [0,0]              
               self.flagUV      =  [0,0]              
               self.Origine     =  [0.0,0.0]
               self.beziers_knot = []

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
              self.ITEM            = {}

courbes=COURBE()

PATTERN={}

BOUNDINGBOX={'rec':[],'coef':1.0}
npat=0
#=====================================================================
#======== name of the curve in teh courbes dictionnary ===============
#=====================================================================
n0=0

#=====================================================================
#====================== current Point ================================
#=====================================================================
CP=[0.0,0.0] #currentPoint


# modifs 12/06/2005       
#=====================================================================
#====================== current transform ============================
#=====================================================================
class transform:
      def __init__(self,matrix=[1,0,01],x=0.0,y=0.0):
          self.matrix=matrix[:]
          self.xy=[x,y]          

def G_move(l,a):
    global GSCALE, GTRANSLATE, GSTACK
    #print GSCALE, GTRANSLATE, GSTACK
    return str((float(l)+GTRANSLATE[a]+GSTACK[-1].xy[a])*GSCALE[a])
# modifs 12/06/2005


#=====================================================================
#===== to compare last position to the original move to displacement =
#=====  needed for cyclic efinition  =================================
#=====================================================================
def test_egalitedespositions(f1,f2):
    if f1[0]==f2[0] and f1[1]==f2[1]:
       return Blender.TRUE
    else:
       return Blender.FALSE


def Open_GEOfile(dir,nom):
    if BLversion>=233:
       in_editmode = Blender.Window.EditMode()
       if in_editmode: Blender.Window.EditMode(0)
       Blender.Load(dir+nom+'OOO.obj', 1)
       BO=Blender.Scene.GetCurrent().objects.active
       BO.RotY=0.0
       BO.RotX=1.57
       BO.makeDisplayList() 
       Blender.Window.RedrawAll()
    else:
       print "Not yet implemented"

def create_GEOtext(courbes):
    global SCALE, B, BOUNDINGBOX
    r=BOUNDINGBOX['rec']

    if SCALE==1:
       SCALE=1.0
    elif SCALE==2:
       SCALE=r[2]-r[0]
    elif SCALE==3:
       SCALE=r[3]-r[1]
 
    t=[]
    t.append(courbes.magic_number+'\n')
    t.append(str(courbes.type)+'\n')
    t.append(str(courbes.number_of_items)+'\n')
    t.append(str(courbes.ext1_ext2[0])+' '+str(courbes.ext1_ext2[1])+'\n')
    t.append(courbes.matrix+'\n')
    
    for k in courbes.ITEM.keys():
        if len(courbes.ITEM[k].beziers_knot)>1 :
            t.append("%s\n"%courbes.ITEM[k].type)
            t.append("%s %s \n"%(courbes.ITEM[k].pntsUV[0],courbes.ITEM[k].pntsUV[1]))
            t.append("%s %s \n"%(courbes.ITEM[k].resolUV[0],courbes.ITEM[k].resolUV[1]))
            t.append("%s %s \n"%(courbes.ITEM[k].orderUV[0],courbes.ITEM[k].orderUV[1]))
            t.append("%s %s \n"%(courbes.ITEM[k].flagUV[0],courbes.ITEM[k].flagUV[1]))

            flag =courbes.ITEM[k].flagUV[0]

            for k2 in range(len(courbes.ITEM[k].beziers_knot)):
               #print k2 
               k1 =courbes.ITEM[k].beziers_knot[k2]
               t.append("%4f 0.0 %4f \n"%(float(k1.co[2])/SCALE,float(k1.co[3])/SCALE))               
               t.append("%4f 0.0 %4f \n"%(float(k1.co[4])/SCALE,float(k1.co[5])/SCALE))
               t.append("%4f 0.0 %4f \n"%(float(k1.co[0])/SCALE,float(k1.co[1])/SCALE))
               
               t.append(str(k1.ha[0])+' '+str(k1.ha[1])+'\n')
    return t

def save_GEOfile(dir,nom,t):
     f=open(dir+nom+'OOO.obj','w')
     f.writelines(t)
     f.close()
     #warning = "REMINDER : %t | Do not forget to rename your blender file NOW ! %x1"
     #result = Blender.Draw.PupMenu(warning)


#=====================================================================
#=====      AI format   :  DEBUT             =========================
#=====================================================================
def mouvement_vers(l,n0,CP):
    if n0 in courbes.ITEM.keys():
       n0+=1

    CP=[l[-3].replace('d',''),l[-2]] 
    courbes.ITEM[n0]=ITEM() 
    courbes.ITEM[n0].Origine=[l[-3].replace('d',''),l[-2]] 
    
    B=Bez()
    B.co=[CP[0],CP[1],CP[0],CP[1],CP[0],CP[1]]
    B.ha=[0,0]
    B.tag=l[-1]

    courbes.ITEM[n0].beziers_knot.append(B)       

    return  courbes,n0,CP     
       
def courbe_vers_c(l,l2, n0,CP): #c,C

    B=Bez()
    B.co=[l[4],l[5],l[2],l[3],l[4],l[5]]
    B.tag=l[-1]
    B.ha=[0,0]
        
    BP=courbes.ITEM[n0].beziers_knot[-1]
    
    BP.co[0]=l[0]
    BP.co[1]=l[1]

    courbes.ITEM[n0].beziers_knot.append(B)

    CP=[B.co[4],B.co[5]]
    return  courbes,n0,CP
     
     
def courbe_vers_v(l,n0,CP): #v-V

    B=Bez()
    B.tag=l[-1]
    B.co=[l[2],l[3],l[0],l[1],l[2],l[3]]
    B.ha=[0,0]

    courbes.ITEM[n0].beziers_knot.append(B) 
   
    CP=[B.co[4],B.co[5]]    
    return  courbes,n0,CP
         
def courbe_vers_y(l,n0,CP): #y
    B=Bez()
    B.tag=l[-1]
    B.co=[l[2],l[3],l[2],l[3],l[2],l[3]]
    B.ha=[0,0]
        
    BP=courbes.ITEM[n0].beziers_knot[-1]
    BP.co[0]=l[0]
    BP.co[1]=l[1]   
     
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[B.co[4],B.co[5]] 
    return  courbes,n0,CP
     
      
def ligne_tracee_l(l,n0,CP):
    B=Bez()
    B.tag=l[-1]
    B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
    B.ha=[0,0]

    BP=courbes.ITEM[n0].beziers_knot[-1]    

    courbes.ITEM[n0].beziers_knot.append(B)
    CP=[B.co[4],B.co[5]]
    return  courbes,n0,CP    

def ligne_fermee(l,n0,CP):
    courbes.ITEM[n0].flagUV[0]=1
    
    if len(courbes.ITEM[n0].beziers_knot)>1:
        BP=courbes.ITEM[n0].beziers_knot[-1]
        BP0=courbes.ITEM[n0].beziers_knot[0]
           
        if BP.tag not in ['l','L']: 
           BP.co[0]=BP0.co[0]  #4-5 point prec
           BP.co[1]=BP0.co[1]
        
    del courbes.ITEM[n0].beziers_knot[0]
    return  courbes,n0,CP    

def passe(l,n0,CP):
    return  courbes,n0,CP

Actions=   {     "C" : courbe_vers_c,
                 "c" : courbe_vers_c,
                 "V" : courbe_vers_v,
                 "v" : courbe_vers_v,
                 "Y" : courbe_vers_y,
                 "y" : courbe_vers_y,
                 "m" : mouvement_vers,
                 "l" : ligne_tracee_l,
                 "L" : ligne_tracee_l,
                 "F" :	passe,                      
                 "f" : ligne_fermee,
                 "B" :	passe,
                 "b" : ligne_fermee,
	               "S" :	passe,
                 "s" : ligne_fermee,
                 "N" : ligne_fermee,
	               "n" :	passe,
                 }
     
TAGcourbe=Actions.keys()
                 
def pik_pattern(t,l):
    global npat, PATTERN, BOUNDINGBOX, AI_VERSION
    while t[l].find('%%EndSetup')!=0:
          if t[l].find('%%Creator: Adobe Illustrator(R)')!=-1:
               print  t[l]
               AI_VERSION=t[l].split()[-1]
               print  AI_VERSION
			
          if t[l].find('%%BoundingBox:')!=-1:
             t[l]=t[l][t[l].find(':')+1:]    
             l0=t[l].split()
             BOUNDINGBOX['rec']=[float(l0[-4]),float(l0[-3]),float(l0[-2]),float(l0[-1])]
             r=BOUNDINGBOX['rec']
             BOUNDINGBOX['coef']=(r[3]-r[1])/(r[2]-r[0])
          #print l,
          if  t[l].find('BeginPattern')!=-1:
              nomPattern=t[l][t[l].find('(')+1:t[l].find(')')]
              PATTERN[nomPattern]={}

          if  t[l].find('BeginPatternLayer')!=-1:
               npat+=1
               PATTERN[nomPattern][npat]=[]
               while t[l].find('EndPatternLayer')==-1:
                     #print t[l] 
                     PATTERN[nomPattern][npat].append(l) 
                     l+=1   
          if l+1<len(t):
             l=l+1
          else:
             return 1,l
    return 1,l
             
def scan_FILE(nom):
  global CP, courbes, SCALE, NOTHING_TODO
  dir,name=split(nom)
  name=name.split('.')
  n0=0
  result=0
  t=filtreFICHIER(nom)
  
  if nom.upper().find('.AI')!=-1 and t!='false':
      if not SHARP_IMPORT:
            warning = "Select Size : %t| As is %x1 | Scale on Height %x2| Scale on Width %x3" 
            SCALE = Blender.Draw.PupMenu(warning)
         
      npat=0
      l=0
      do=0
      while l <len(t)-1 :
           if not do:
              do,l=pik_pattern(t,l)
           #print 'len(t)',len(t)        
           t[l].replace('\n','')
           if t[l].find('%%EOF')==0:
              break 
           if t[l][0]!='%':
                l0=t[l].split()
                #print l0  
                if l0[0][0] in ['F','f','N','n','B','b']:
                   l3=l0[0][0]
                   courbes,n0,CP=Actions[l3](l3,n0,CP)
                   l0[0]=l0[1:]
	
                if l0[-1] in TAGcourbe:
                    NOTHING_TODO=0
                    if l0[-1] in ['C','c']:
                       l2=t[l+1].split()
                       courbes,n0,CP=Actions[l0[-1]](l0,l2,n0,CP)
                    else: 
                       courbes,n0,CP=Actions[l0[-1]](l0,n0,CP)
           l=l+1; #print l                    
      t=[]
      
            
      courbes.number_of_items=len(courbes.ITEM.keys())      
      for k in courbes.ITEM.keys():
              courbes.ITEM[k].pntsUV[0] =len(courbes.ITEM[k].beziers_knot)
              

      if courbes.number_of_items>0:
         if len(PATTERN.keys() )>0:
            #print len(PATTERN.keys() )
            warning = "Pattern list (for info not used): %t| "
            p0=1
            for P in PATTERN.keys():
                warning+="%s %%x%s|"%(P,p0)
                p0+=1 
            Padd = Blender.Draw.PupMenu(warning)
            
         t=create_GEOtext(courbes)
         save_GEOfile(dir,name[0],t)

         # 0.1.8 ---------------------------------  
         # [O.select(0) for O in Blender.Scene.getCurrent().getChildren()]
         # 0.1.8 ---------------------------------

         Open_GEOfile(dir,name[0])

         # 0.1.8 ---------------------------------
         Blender.Object.Get()[-1].setName(name[0])
         # 0.1.8 --------------------------------- 

      else:
          pass
#=====================================================================
#====================== AI format mouvements =========================
#=====================================================================
#=========================================================          
# une sorte de contournement qui permet d'utiliser la fonction
# et de documenter les variables Window.FileSelector
#=========================================================
def fonctionSELECT(nom):
    global NOTHING_TODO,AI_VERSION
    scan_FILE(nom)
    if NOTHING_TODO==1:
            warning = "AI  %s compatible file "%AI_VERSION+" but nothing to do ? %t| Perhaps a compacted file ... "
            NOTHING = Blender.Draw.PupMenu(warning)

if __name__=="__main__":
    Blender.Window.FileSelector (fonctionSELECT, 'SELECT AI FILE')
#sys.path=oldpath       
