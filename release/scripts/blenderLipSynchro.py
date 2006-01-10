#!BPY

"""
Name: 'BlenderLipSynchro'
Blender: 239
Group: 'Animation'
Tooltip: 'Import phonemes from Papagayo or JLipSync for lip synchronisation'
"""

__author__ = "Dienben: Benoit Foucque dienben_mail@yahoo.fr"
__url__ = ("blenderLipSynchro Blog, http://blenderlipsynchro.blogspot.com/",
"Papagayo (Python), http://www.lostmarble.com/papagayo/index.shtml",
"JLipSync (Java), http://jlipsync.sourceforge.net/")
__version__ = "1.2"
__bpydoc__ = """\
Description:

This script imports Voice Export made by Papagayo or JLipSync and maps the export with your shapes.

Usage:

Import a Papagayo or JLipSync voice export file and link it with your shapes.

Note:<br>
- Naturally, you need files exported from one of the supported lip synching
programs. Check their sites to learn more and download them.

"""

# --------------------------------------------------------------------------
# BlenderLipSynchro
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
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



#il y a 3 etapes
#la deuxieme on charge le dictionnaire de correspondance
#la troisieme on fait le choix des correpondance
#la quatrieme on construit les cles a partir du fichiers frame

#there are 3 stage
#the second one load the mapping dictionnary 
#the tird make the mapping
#the fourth make the key in the IPO Curve

#voici mes differents imports
#the imports
import os
import Blender

from Blender import Ipo
from Blender.Draw import *
from Blender.BGL import *



#ici commencent mes fonctions
#here begin my functions
#cette fonction trace l'interface graphique
#this functions draw the User interface
def trace():
    #voici mes variables pouvant etre modifie
    #my variables
    global fichier_dico,repertoire_dictionaire,iponame,repertoire_phoneme,fichier_text,nbr_phoneme
    global let01, let02, let03, let04,let05, let06, let07, let08, let09, let10
    global let11, let12, let13, let14,let15, let16, let17, let18, let19, let20
    global let21, let22, let23, let24

    global let01selectkey,let02selectkey,let03selectkey,let04selectkey,let05selectkey
    global let06selectkey,let07selectkey,let08selectkey,let09selectkey,let10selectkey,let11selectkey
    global let12selectkey,let13selectkey,let14selectkey,let15selectkey,let16selectkey,let17selectkey
    global let18selectkey,let19selectkey,let20selectkey,let21selectkey,let22selectkey,let23selectkey
    global let24selectkey
    
    glClearColor(0.4,0.5,0.6 ,0.0)
    glClear(GL_COLOR_BUFFER_BIT)

    glColor3d(1,1,1)
    glRasterPos2i(87, 375)
    Text("Blendersynchro V 1.1")
    glColor3d(1,1,1)
    glRasterPos2i(84, 360)
    Text("Programation: Dienben")

    glColor3d(0,0,0)
    glRasterPos2i(13, 342)
    Text("Papagayo File importer")
    glColor3d(0,0,0)
    glRasterPos2i(13, 326)
    Text("Thanks to Chris Clawson and Liubomir Kovatchev")

    glColor3d(1,1,1)
    glRasterPos2i(5, 320)
    Text("_______________________________________________________")
    glColor3d(0,0,0)
    glRasterPos2i(6, 318)
    Text("_______________________________________________________")

    
    if (etape==1):
        #cette etape permet de choisi la correspondance entre les phonemes et les cles
        #this stage offer the possibility to choose the mapping between phonems and shapes

        glColor3d(1,1,1)
        glRasterPos2i(140, 300)
        Text("Objet: "+Blender.Object.GetSelected()[0].getName() )

        glColor3d(1,1,1)
        glRasterPos2i(5, 215)
        Text("Assign phonems to shapes:")

        #on mesure la taille de la liste de phonemes
        #this is the lenght of the phonem list
        nbr_phoneme=len(liste_phoneme)

        #on dessine les listes de choix
        #we draw the choice list

        let01 = String(" ", 4, 5, 185, 30, 16, liste_phoneme[0], 3)
        glColor3d(0,0,0)
        glRasterPos2i(40, 188)
        Text("=")
        let01selectkey = Menu(key_menu, 50, 50, 185, 70, 16, let01selectkey.val)


	let02 = String(" ", 4, 150, 185, 30, 16, liste_phoneme[1], 2)
	glColor3d(0,0,0)
	glRasterPos2i(185, 188)
	Text("=")
	let02selectkey = Menu(key_menu, 51, 195, 185, 70, 16, let02selectkey.val)


	let03 = String(" ", 4, 5, 165, 30, 16, liste_phoneme[2], 2)
	glColor3d(0,0,0)
	glRasterPos2i(40, 168)
	Text("=")
	let03selectkey = Menu(key_menu, 52, 50, 165, 70, 16, let03selectkey.val)


	let04 = String(" ", 4, 150, 165, 30, 16, liste_phoneme[3], 2)
	glColor3d(0,0,0)
	glRasterPos2i(185, 168)
	Text("=")
	let04selectkey = Menu(key_menu, 53, 195, 165, 70, 16, let04selectkey.val)


	let05 = String(" ", 4, 5, 145, 30, 16, liste_phoneme[4], 2)
	glColor3d(0,0,0)
	glRasterPos2i(40, 148)
	Text("=")
	let05selectkey = Menu(key_menu, 54, 50, 145, 70, 16, let05selectkey.val)


	let06 = String(" ", 4, 150, 145, 30, 16, liste_phoneme[5], 2)
	glColor3d(0,0,0)                                
	glRasterPos2i(185, 148)
	Text("=")
	let06selectkey = Menu(key_menu, 55, 195, 145, 70, 16, let06selectkey.val)


	let07 = String(" ", 4, 5, 125, 30, 16, liste_phoneme[6], 2)
	glColor3d(0,0,0)
	glRasterPos2i(40, 128)
	Text("=")
	let07selectkey = Menu(key_menu, 56, 50, 125, 70, 16, let07selectkey.val)

	#
	let08 = String(" ", 4, 150, 125, 30, 16, liste_phoneme[7], 2)
	glColor3d(0,0,0)
	glRasterPos2i(185, 128)
	Text("=")
	let08selectkey = Menu(key_menu, 57, 195, 125, 70, 16,let08selectkey.val)


	let09 = String(" ", 4, 5, 105, 30, 16, liste_phoneme[8], 2)
	glColor3d(0,0,0)
	glRasterPos2i(40, 108)
	Text("=")
	let09selectkey = Menu(key_menu, 58, 50, 105, 70, 16,let09selectkey.val)


	let10 = String(" ", 4, 150, 105, 30, 16, liste_phoneme[9], 2)
	glColor3d(0,0,0)                                
	glRasterPos2i(185, 108)
	Text("=")
	let10selectkey = Menu(key_menu, 59, 195, 105, 70, 16, let10selectkey.val)
	
	#soft_type = 0:Papagayo
	#soft_type = 1:JLipSync
	if (soft_type==1):
		let11 = String(" ", 4, 5, 85, 30, 16, liste_phoneme[10], 2)
		glColor3d(0,0,0)
		glRasterPos2i(40, 88)
		Text("=")
		let11selectkey = Menu(key_menu, 60, 50, 85, 70, 16, let11selectkey.val)

		let12 = String(" ", 4, 150, 85, 30, 16, liste_phoneme[11], 2)
		glColor3d(0,0,0)                                
		Text("=")
		let12selectkey = Menu(key_menu, 61, 195, 85, 70, 16, let12selectkey.val)
	
		let13 = String(" ", 4, 5, 65, 30, 16, liste_phoneme[12], 2)
		glColor3d(0,0,0)
		glRasterPos2i(40, 68)
		Text("=")
		let13selectkey = Menu(key_menu, 62, 50, 65, 70, 16, let13selectkey.val)
	
		let14 = String(" ", 4, 150, 65, 30, 16, liste_phoneme[13], 2)
		glColor3d(0,0,0)
		glRasterPos2i(185, 68)
		Text("=")
		let14selectkey = Menu(key_menu, 63, 195, 65, 70, 16, let14selectkey.val)
	
		let15 = String(" ", 4, 5, 45, 30, 16, liste_phoneme[14], 2)
		glColor3d(0,0,0)
		glRasterPos2i(40, 48)
		Text("=")
		let15selectkey = Menu(key_menu, 64, 50, 45, 70, 16, let15selectkey.val)
	
		let16 = String(" ", 4, 150, 45, 30, 16, liste_phoneme[15], 2)
		glColor3d(0,0,0)                                
		glRasterPos2i(185, 48)
		Text("=")
		let16selectkey = Menu(key_menu, 65, 195, 45, 70, 16, let16selectkey.val)
	
		let17 = String(" ", 4, 295, 185, 30, 16, liste_phoneme[16], 2)
		glColor3d(0,0,0)
		glRasterPos2i(330, 188)
		Text("=")
		let17selectkey = Menu(key_menu, 66, 340, 185, 70, 16, let17selectkey.val)
	
		let18 = String(" ", 4, 440, 185, 70, 16, liste_phoneme[17], 8)
		glColor3d(0,0,0)
		glRasterPos2i(515, 188)
		Text("=")
		let18selectkey = Menu(key_menu, 67, 525, 185, 70, 16, let18selectkey.val)
	
		let19 = String(" ", 4, 295, 165, 30, 16, liste_phoneme[18], 2)
		glColor3d(0,0,0)
		glRasterPos2i(330, 168)
		Text("=")
		let19selectkey = Menu(key_menu, 68, 340, 165, 70, 16, let19selectkey.val)
	
		let20 = String(" ", 4, 440, 165, 70, 16, liste_phoneme[19], 8)
		glColor3d(0,0,0)
		glRasterPos2i(515, 168)
		Text("=")
		let20selectkey = Menu(key_menu, 69, 525, 165, 70, 16, let20selectkey.val)
	
		let21 = String(" ", 4, 295, 145, 30, 16, liste_phoneme[20], 2)
		glColor3d(0,0,0)
		glRasterPos2i(330, 148)
		Text("=")
		let21selectkey = Menu(key_menu, 70, 340, 145, 70, 16, let21selectkey.val)
	
		let22 = String(" ", 4, 440, 145, 70, 16, liste_phoneme[21], 8)
		glColor3d(0,0,0)                                
		glRasterPos2i(515, 148)
		Text("=")
		let22selectkey = Menu(key_menu, 71, 525, 145, 70, 16, let22selectkey.val)
	
		let23 = String(" ", 4, 295, 125, 30, 16, liste_phoneme[22], 2)
		glColor3d(0,0,0)
		glRasterPos2i(330, 128)
		Text("=")
		let23selectkey = Menu(key_menu, 72, 340, 125, 70, 16,let23selectkey.val)
	
		let24 = String(" ", 4, 440, 125, 70, 16, liste_phoneme[23], 8)
		glColor3d(0,0,0)
		glRasterPos2i(515, 128)
		Text("=")
		let24selectkey = Menu(key_menu, 73, 525, 125, 70, 16, let24selectkey.val)
	
	
	Button("Import Text", 3, 155, 5, 145, 22)
	Button("Choose Voice Export", 4, 120, 250, 250, 22)

	
    if (etape==2):
        glColor3d(1,1,1)
        glRasterPos2i(125, 200)
        Text("Operation Completed")
        
    if (etape==0):
        glColor3d(1,1,1)
        glRasterPos2i(125, 200)
        Text("Please select a Mesh'Object and Create all the IPO Curves for your Shapes")
        
    if (etape==3):
    	Button("Papagayo", 5, 155, 250, 250, 22)
	Button("JlipSync", 6, 155, 225, 250, 22)

    glColor3d(1,1,1)
    glRasterPos2i(6, 40)
    Text("_______________________________________________________")
    glColor3d(0,0,0)
    glRasterPos2i(6, 38)
    Text("_______________________________________________________")
	
    Button("Exit", 1, 305, 5, 80, 22)
    
    
	
	
#cette fonction sur evenement quite en cas d'ESC
#this functions catch the ESC event and quit
def event(evt,val):
    	if (evt == ESCKEY and not val): Exit()

#cette fonction gere les evenements
#the event functions
def bevent(evt):
	global etape,soft_type,liste_phoneme

	if (evt == 1):
		Exit()
	
               
        elif (evt == 4):
            #c'est le choix de l'export pamela
            #we choose the papagayo export
            Blender.Window.FileSelector(selectionner_export_papagayo,"Choose Export")
            
            
        elif (evt == 3):
            #c'est l'import Papagayo
            #we import 
            lecture_chaine(mon_fichier_export_pamela,dico_phoneme_export_pamela)
            construction_dico_correspondance()
            construction_lipsynchro()
            #on change d'etape
            #we change the stage
            etape=2
         
        elif (evt == 5):
            #we choose papagayo
            soft_type=0
            liste_phoneme=liste_phoneme_papagayo
            etape=1
         
        elif (evt == 6):
            #we choose jlipsync
            soft_type=1
            liste_phoneme=liste_phoneme_jlipsinch
            etape=1
         	
        Blender.Redraw()

#cette fonction recupere le nom et le chemin du fichier dictionnaire
#we catch the name and the path of the dictionnary
def selectionner_export_papagayo(filename):
    global mon_fichier_export_pamela
    #debut
    mon_fichier_export_pamela=filename
    
#fonction de lecture de la liste frame phoneme
#we read the frame and phonems
def lecture_chaine(fichier,liste):
    mon_fichier=open(fichier)
        
    #je lis la premiere ligne qui contiens la version de moho
    #first, we read the moho version
    mon_fichier.readline()

    #je lis jusqu'a la fin
    #then we read until the end of the file
    while 1:
        ma_ligne=mon_fichier.readline()
        if ma_ligne=='':
            break
        decoup=ma_ligne.split()
        liste[decoup[0]]=decoup[1]
    print liste




#fonction qui construit la liste dictionnaire simple
#we make the dictionnary
def construction_dictionnaire_phoneme():
    index_liste=0
     #je transforme mon dictionnaire en list de tulpes
     #we transform the list in tulpes
    ma_liste=dico_phoneme.items()
    #je parcours ma liste a la recherche d'elements non existant
    #we read the list to find non existing elements
    print dico_phoneme
    for index in range(len(ma_liste)):
        if ma_liste[index][1] not in liste_phoneme:
            liste_phoneme[index_liste:index_liste]=[ma_liste[index][1]]
            index_liste=index_liste+1
    print liste_phoneme
     


#cette fonction recupere les courbes cible 
#this functon catch the IPO curve
def recuperation_courbe():
	global key_menu,dico_key

	#on recupere le nom des shapes
	#we catch the shapes
	key=Blender.Object.GetSelected()[0].getData().getKey().getBlocks()
	for n in range(len(key)):
		#on vire la première cle (en effet basic n'est pas une cle en tant que telle)
		#we threw away the basic shapes
		if (n>0):
			key_menu=key_menu+key[n].name + " %x" + str(n-1) + "|"
			dico_key[str(n-1)]=Blender.Object.GetSelected()[0].getData().getKey().getIpo().getCurves()[n-1]
	
	
	print "dico_key"
	print dico_key
	print 'end dico_key'

#cette fonction construit un dictionnaire de correspondance entre les phonemes prononces et les cles a utiliser
#we make the dictionnary for the mapping between shapes and phonems
def construction_dico_correspondance():
	global dico_correspondance
	#je parcours les phonemes
	#we read the phonems
	dico_correspondance[liste_phoneme[0]]=dico_key[str(let01selectkey.val)]
	dico_correspondance[liste_phoneme[1]]=dico_key[str(let02selectkey.val)]
	dico_correspondance[liste_phoneme[2]]=dico_key[str(let03selectkey.val)]
	dico_correspondance[liste_phoneme[3]]=dico_key[str(let04selectkey.val)]
	dico_correspondance[liste_phoneme[4]]=dico_key[str(let05selectkey.val)]
	dico_correspondance[liste_phoneme[5]]=dico_key[str(let06selectkey.val)]
	dico_correspondance[liste_phoneme[6]]=dico_key[str(let07selectkey.val)]
	dico_correspondance[liste_phoneme[7]]=dico_key[str(let08selectkey.val)]
	dico_correspondance[liste_phoneme[8]]=dico_key[str(let09selectkey.val)]
	dico_correspondance[liste_phoneme[9]]=dico_key[str(let10selectkey.val)]
	
	if (soft_type==1):
		dico_correspondance[liste_phoneme[10]]=dico_key[str(let11selectkey.val)]
		dico_correspondance[liste_phoneme[11]]=dico_key[str(let12selectkey.val)]
		dico_correspondance[liste_phoneme[12]]=dico_key[str(let13selectkey.val)]
		dico_correspondance[liste_phoneme[13]]=dico_key[str(let14selectkey.val)]
		dico_correspondance[liste_phoneme[14]]=dico_key[str(let15selectkey.val)]
		dico_correspondance[liste_phoneme[15]]=dico_key[str(let16selectkey.val)]
		dico_correspondance[liste_phoneme[16]]=dico_key[str(let17selectkey.val)]
		dico_correspondance[liste_phoneme[17]]=dico_key[str(let18selectkey.val)]
		dico_correspondance[liste_phoneme[18]]=dico_key[str(let19selectkey.val)]
		dico_correspondance[liste_phoneme[19]]=dico_key[str(let20selectkey.val)]
		dico_correspondance[liste_phoneme[20]]=dico_key[str(let21selectkey.val)]
		dico_correspondance[liste_phoneme[21]]=dico_key[str(let22selectkey.val)]
		dico_correspondance[liste_phoneme[22]]=dico_key[str(let23selectkey.val)]
		dico_correspondance[liste_phoneme[23]]=dico_key[str(let24selectkey.val)]
		
	print dico_correspondance


#cette fonction ajoute un points a la cle donnee a la frame donnee
#we add a point to the IPO curve Target
def ajoute_point(cle,frame,valeur):
    cle.setInterpolation('Linear')
    cle.addBezier((frame,valeur))
    cle.Recalc()

#cette fonction parcours le dictionnaire des frame à ajouter et construit les points
#we add all the point to the IPO Curve
def construction_lipsynchro():
    print "je construit"
    doublet_old=""
    #construction de la liste des frame
    cpt=0
    liste_frame=[]
    for frame in dico_phoneme_export_pamela:
        liste_frame.append(int(frame))
        cpt=cpt+1
    liste_frame.sort()
    print "listeframe"
    print liste_frame
    print "fini"
    
    for doublet in liste_frame:
        ajoute_point(dico_correspondance[dico_phoneme_export_pamela[str(doublet)]],doublet,1)
        if (doublet_old==""):
            ajoute_point(dico_correspondance[dico_phoneme_export_pamela[str(doublet)]],(doublet-2),0)
        if (doublet_old!=''):
            if (dico_correspondance[dico_phoneme_export_pamela[str(doublet)]]!=dico_correspondance[dico_phoneme_export_pamela[doublet_old]]):
                print "doublet:"+str(doublet)
                print "doublet old:"+doublet_old                
                ajoute_point(dico_correspondance[dico_phoneme_export_pamela[doublet_old]],(int(doublet_old)+2),0)
                ajoute_point(dico_correspondance[dico_phoneme_export_pamela[str(doublet)]],(doublet-2),0)
        doublet_old=str(doublet)
        

#end of my functions we begin the execution       
#je commence l execution-----------------------------------------------------------------------------------------------
#voici mes variables

#declaration et instanciation
#decleration and instanciation
#ce sont les repertoires
repertoire_dictionaire=Create('C:/')
repertoire_phoneme=Create('c:/')

#ce sont ls fichiers
fichier_dico=Create("sample.mot")
fichier_text=Create("")

#voici mon objet de travail
objet_travail=Create(0)

#my soft type
soft_type=1

#voici la liste des phoneme effectivement utilise
#the phonems'list
liste_phoneme_papagayo=['AI','E','O','U','FV','L','WQ','MBP','etc','rest']
liste_phoneme_jlipsinch=['A','B','C','Closed','D','E','F','G','I','K','L','M','N','O','P','Q','R','S','SH','T','TH','U','V','W']

liste_phoneme=liste_phoneme_jlipsinch
#voici mon dictionnaire des frames o
dico_phoneme_export_pamela = Create(0)
dico_phoneme_export_pamela={}



#voici mes cle
key_menu=""
dico_key={}

#voici mes ipo
dico_bloc={}
iponame = Create(0)

#voici mon dictionnaire de correspondance
dico_correspondance={}

try:
	#on verifie est bien une mesh et qu'il a des courbes
	if ((Blender.Object.GetSelected()[0].getType()=='Mesh')):
		#on verifie que l'objet a bien toute ses Courbes
		if (len(Blender.Object.GetSelected()[0].getData().getKey().getBlocks())-1==Blender.Object.GetSelected()[0].getData().getKey().getIpo().getNcurves()):
			etape=3
			#on lance la creation du dictionnaire
			recuperation_courbe()
		else:
			print "not the good number of IPO Curve"
			etape = 0
	else:
		print "error: bad object Type:"
		print Blender.Object.GetSelected()[0].getType()
		etape = 0
except:
	print 'error: exception'
	etape = 0


#voici le fichier dictionnaire
mon_fichier_dico=""

#voici le fichier export pamela
mon_fichier_export_pamela=""


let01selectkey = Create(0)
let02selectkey = Create(0)
let03selectkey = Create(0)
let04selectkey = Create(0)
let05selectkey = Create(0)
let06selectkey = Create(0)
let07selectkey = Create(0)
let08selectkey = Create(0)
let09selectkey = Create(0)
let10selectkey = Create(0)
let11selectkey = Create(0)
let12selectkey = Create(0)
let13selectkey = Create(0)
let14selectkey = Create(0)
let15selectkey = Create(0)
let16selectkey = Create(0)
let17selectkey = Create(0)
let18selectkey = Create(0)
let19selectkey = Create(0)
let20selectkey = Create(0)
let21selectkey = Create(0)
let22selectkey = Create(0)
let23selectkey = Create(0)
let24selectkey = Create(0)


Register (trace,event,bevent)
