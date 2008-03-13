# -*- coding: latin-1 -*-
"""
#----------------------------------------------
# (c) jm soler juillet 2004, 
#----------------------------------------------
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

"""

# ---------------------------------------------------------------
#    last update : 07/05/2007
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_import_gimp.htm
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
# Modification History:
# 2008-03-12  Added character encoding line so french text does not break
#   python interpreters.
#---------------------------------------------

SHARP_IMPORT=0
SCALE=1

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
     f=open(nom,'r')
     t=f.readlines()
     f.close()
     if len(t)==1 and t[0].find('\r'):
              t=t[0].split('\r')
     if len(t)>1 and t[1].find('#POINTS:')==0: 
          return t   
     else:
         warning = "OK?%t| Not a valid file or an empty file ... "  # if no %xN int is set, indices start from 1
         result = Blender.Draw.PupMenu(warning)
         return "false"

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

      def echo(self):
          #print 'co = ', self.co 
          #print 'ha = ', self.ha
          pass
           
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
0.0 0.0 0.0 1.0 """ #- right-handed object matrix. Used to determine position, rotation and size
              self.ITEM            = {}

courbes=COURBE()
PATTERN={}
BOUNDINGBOX={'rec':[],'coef':1.0}
npat=0
#=====================================================================
#======== name of the curve in the courbes dictionnary ===============
#=====================================================================
n0=0

#=====================================================================
#====================== current Point ================================
#=====================================================================
CP=[0.0,0.0] #currentPoint

def MINMAX(b):
   global BOUNDINGBOX
   r=BOUNDINGBOX['rec']
   for m in range(0,len(b)-2,2):
        #print m, m+1  , len(b)-1
        #print b[m], r, r[0]
        if float(b[m])<r[0]: 
           r[0]=float(b[m])

        if float(b[m])>r[2]: r[2]=float(b[m])

        if float(b[m+1])<r[1]: r[1]=float(b[m+1])
        if float(b[m+1])>r[3]: r[3]=float(b[m+1])
 
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
       Blender.Load(dir+nom+'OOO.obj', 1)
       BO=Blender.Scene.GetCurrent().objects.active
       BO.LocZ=1.0
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

        t.append("%s\n"%courbes.ITEM[k].type)

        t.append("%s %s \n"%(courbes.ITEM[k].pntsUV[0],courbes.ITEM[k].pntsUV[1]))
        t.append("%s %s \n"%(courbes.ITEM[k].resolUV[0],courbes.ITEM[k].resolUV[1]))
        t.append("%s %s \n"%(courbes.ITEM[k].orderUV[0],courbes.ITEM[k].orderUV[1]))
        t.append("%s %s \n"%(courbes.ITEM[k].flagUV[0],courbes.ITEM[k].flagUV[1]))

        flag =0#courbes.ITEM[k].flagUV[0]

        for k2 in range(flag,len(courbes.ITEM[k].beziers_knot)):
           k1 =courbes.ITEM[k].beziers_knot[k2]
           t.append("%4f 0.0 %4f \n"%(float(k1.co[0])/SCALE,float(k1.co[1])/SCALE))
           t.append("%4f 0.0 %4f \n"%(float(k1.co[4])/SCALE,float(k1.co[5])/SCALE))
           t.append("%4f 0.0 %4f \n"%(float(k1.co[2])/SCALE,float(k1.co[3])/SCALE))
           t.append(str(k1.ha[0])+' '+str(k1.ha[1])+'\n')
    return t

def save_GEOfile(dir,nom,t):
     f=open(dir+nom+'OOO.obj','w')
     f.writelines(t)
     f.close()
     #warning = "REMINDER : %t | Do not forget to rename your blender file NOW ! %x1"
     #result = Blender.Draw.PupMenu(warning)


#=====================================================================
#=====      GIMP format   :  DEBUT           =========================
#=====================================================================
CLOSED=0

def mouvement_vers(l,l1,l2,n0):
    global BOUNDINGBOX, CP
    if l[1] == '3' :
         n0+=1
         courbes.ITEM[n0]=ITEM() 
         courbes.ITEM[n0].Origine=[l[-3],l[-1],] 
         courbes.ITEM[n0-1].beziers_knot[0].co[0]=CP[0]
         courbes.ITEM[n0-1].beziers_knot[0].co[1]=CP[1]
         CP=[l2[-3],  l2[-1]]

    elif l[1]=='1' and (n0 not in courbes.ITEM.keys()): 
       courbes.ITEM[n0]=ITEM() 
       courbes.ITEM[n0].Origine=[l[-3],l[-1],] 
       CP=[l2[-3],  l2[-1]]
    
    B=Bez()
    B.co=[ CP[0],CP[1], 
           l1[-3],  l1[-1], 
           l[-3],   l[-1]]

    CP=[l2[-3],  l2[-1]]

    if BOUNDINGBOX['rec']==[]:
       BOUNDINGBOX['rec']=[float(l2[-3]),  float(l2[-1]), float(l[-3]), float(l[-1])]
    B.ha=[0,0]    

    """
    if len( courbes.ITEM[n0].beziers_knot)>=1:     
       courbes.ITEM[n0].beziers_knot[-1].co[2]=l1[-3]
       courbes.ITEM[n0].beziers_knot[-1].co[3]=l1[-1]
    """

    MINMAX(B.co)    
    courbes.ITEM[n0].beziers_knot.append(B)  
    return  courbes,n0      
     
Actions=   { "1" : mouvement_vers,
             "3" : mouvement_vers    }
     
TAGcourbe=Actions.keys()
             
def scan_FILE(nom):
  global CP, courbes, SCALE, MAX, MIN, CLOSED
  dir,name=split(nom)
  name=name.split('.')
  #print name
  n0=0
  result=0 
  t=filtreFICHIER(nom)
  if t!="false":
     if not SHARP_IMPORT:
         warning = "Select Size : %t| As is %x1 | Scale on Height %x2| Scale on Width %x3" 
         SCALE = Blender.Draw.PupMenu(warning)
     npat=0
     l=0
     while l <len(t)-1 :
       #print 'len(t)',len(t)        
       t[l].replace('\n','') 
       if t[l][0]!='%':
            l0=t[l].split()               
            #print l0[0], l0[1]
            if l0[0]=='TYPE:' and l0[1] in TAGcourbe:
                   #print l0[0], l0[1],
                   l1=t[l+1].split()                
                   l2=t[l+2].split()                
                   courbes,n0=Actions[l0[1]](l0,l1,l2,n0)
            elif l0[0]=='#Point':
                POINTS= int(l0[0])
            elif l0[0]=='CLOSED:' and l0[1]=='1':
                CLOSED=1
       l=l+1;                     
     
     courbes.number_of_items=len(courbes.ITEM.keys())

     courbes.ITEM[n0].beziers_knot[0].co[0]=CP[0]
     courbes.ITEM[n0].beziers_knot[0].co[1]=CP[1]

     for k in courbes.ITEM.keys():
        #print k  
        if CLOSED == 1:
           B=Bez()
           B.co=courbes.ITEM[k].beziers_knot[0].co[:]
           B.ha=courbes.ITEM[k].beziers_knot[0].ha[:]
           B.echo()
           courbes.ITEM[k].beziers_knot.append(B)  
           courbes.ITEM[k].flagUV[0]=1
           courbes.ITEM[k].pntsUV[0] =len(courbes.ITEM[k].beziers_knot)

     if courbes.number_of_items>0:
       t=create_GEOtext(courbes)
       save_GEOfile(dir,name[0],t)
       Open_GEOfile(dir,name[0])
       # 0.1.8 ---------------------------------
       Blender.Object.Get()[-1].setName(name[0])
       # 0.1.8 --------------------------------- 

     else:
        pass

#=====================================================================
#====================== GIMP Path format mouvements =========================
#=====================================================================
#=========================================================          
# une sorte de contournement qui permet d'utiliser la fonction
# et de documenter les variables Window.FileSelector
#=========================================================
def fonctionSELECT(nom):
    scan_FILE(nom)

if __name__=="__main__":
    Blender.Window.FileSelector (fonctionSELECT, 'SELECT GIMP FILE')
    
