"""
#----------------------------------------------
# (c) jm soler juillet 2004, released under Blender Artistic Licence 
#    for the Blender 2.34 Python Scripts Bundle.
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_import_ai.htm
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#
# 0.1.1 : 2004/08/03, bug in boudingbox reading when Value are negative
#
"""
SHARP_IMPORT=0
SCALE=1
DEVELOPPEMENT=0

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

     if len(t)>1: 
          return t   
     else:
         name = "OK?%t| Not a valid file or an empty file ... "  # if no %xN int is set, indices start from 1
         result = Draw.PupMenu(name)
          
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
       BO=Blender.Object.Get()
       BO[-1].RotY=0.0
       BO[-1].makeDisplayList() 
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

        flag =courbes.ITEM[k].flagUV[0]

        for k2 in range(flag,len(courbes.ITEM[k].beziers_knot)):
           k1 =courbes.ITEM[k].beziers_knot[k2]
           t.append("%4f 0.0 %4f \n"%(float(k1.co[0])/SCALE,float(k1.co[1])/SCALE))
           t.append("%4f 0.0 %4f \n"%(float(k1.co[2])/SCALE,float(k1.co[3])/SCALE))
           t.append("%4f 0.0 %4f \n"%(float(k1.co[4])/SCALE,float(k1.co[5])/SCALE))
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
       #if  test_egalitedespositions(courbes.ITEM[n0].Origine,CP):
       #   courbes.ITEM[n0].flagUV[0]=1 
       n0+=1
    else:
       CP=[l[-3].replace('d',''),l[-2]] 
    #i=
    courbes.ITEM[n0]=ITEM() 
    courbes.ITEM[n0].Origine=[l[-3].replace('d',''),l[-2]] 
    
    B=Bez()
    B.co=[CP[0],CP[1],CP[0],CP[1],CP[0],CP[1]]
    B.ha=[0,0]    
    courbes.ITEM[n0].beziers_knot.append(B)       

    return  courbes,n0,CP     
       
def courbe_vers_c(l,l2, n0,CP): #c,C

    B=Bez()
    B.co=[l[2],l[3],l[4],l[5],l[0],l[1]]

    if len(courbes.ITEM[n0].beziers_knot)==1:
       CP=[l[0],l[1]]
       courbes.ITEM[n0].Origine=[l[0],l[1]]

    if l[-1]=='C':  
        B.ha=[2,2]
    else:
        B.ha=[0,0]

    courbes.ITEM[n0].beziers_knot.append(B)
    if len(l2)>1 and l2[-1] in Actions.keys():
       B.co[-2]=l2[0]
       B.co[-1]=l2[1]
    else:
       #B.co[-2]=courbes.ITEM[n0].beziers_knot[-1].co[0]
       #B.co[-1]=courbes.ITEM[n0].beziers_knot[-].co[1]            

       B.co[-2]=CP[0]
       B.co[-1]=CP[1]
    return  courbes,n0,CP
     
     
def courbe_vers_v(l,n0,CP): #v-V
    B=Bez()
    B.co=[l[2],l[3],l[0],l[1],l[2],l[3]]
    if l[-1]=='V':  
        B.ha=[2,2]
    else:
        B.ha=[0,0]
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[l[0],l[1]]    
    return  courbes,n0,CP
         
def courbe_vers_y(l,n0,CP): #y
    B=Bez()
    B.co=[l[2],l[3],l[0],l[1],l[2],l[3]]
    if l[-1]=='Y':  
        B.ha=[2,2]
    else:
        B.ha=[0,0]
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[l[2],l[3]]        
    return  courbes,n0,CP
     
      
def ligne_tracee_l(l,n0,CP):
    B=Bez()
    B.co=[l[0],l[1],l[0],l[1],l[0],l[1]]
    if l[-1]=='L':  
        B.ha=[2,2]
    else:
        B.ha=[0,0]
    courbes.ITEM[n0].beziers_knot.append(B)    
    CP=[l[0],l[1]]
    return  courbes,n0,CP    
     
Actions=   {     "C" : courbe_vers_c,
                 "c" : courbe_vers_c,
                 "V" : courbe_vers_v,
                 "v" : courbe_vers_v,
                 "Y" : courbe_vers_y,
                 "y" : courbe_vers_y,
                 "m" : mouvement_vers,
                 "l" : ligne_tracee_l,
                 "L" : ligne_tracee_l}
     
TAGcourbe=Actions.keys()
                 
def pik_pattern(t,l):
    global npat, PATTERN, BOUNDINGBOX
    while t[l].find('%%EndSetup')!=0:   
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
  global CP, courbes, SCALE
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
           if t[l][0]!='%':
                l0=t[l].split()
                if l0[-1] in TAGcourbe:
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

         if  test_egalitedespositions(courbes.ITEM[k].Origine,
                                      [courbes.ITEM[k].beziers_knot[-1].co[-2],
                                       courbes.ITEM[k].beziers_knot[-1].co[-1]]):
              courbes.ITEM[k].flagUV[0]=1 
              courbes.ITEM[k].pntsUV[0] -=1

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
         Open_GEOfile(dir,name[0])
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
    scan_FILE(nom)

if DEVELOPPEMENT==1:
    Blender.Window.FileSelector (fonctionSELECT, 'SELECT AI FILE')
#sys.path=oldpath       
