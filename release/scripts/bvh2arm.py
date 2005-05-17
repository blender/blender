#!BPY
"""
Name: 'BVH Empties to Armature'
Blender: 234
Group: 'Animation'
Tooltip: 'Create Armature from Empties created by the BVH import script'
"""
__author__ = " Jean-Baptiste PERIN (jb_perin(at)yahoo.fr)"
__url__ = ("blender", "elysiun",
"BVH 2 ARMATURE, http://www.zoo-logique.org/3D.Blender/index.php3?zoo=dld&rep=zip ",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender") 

__version__ = "2.0"

__bpydoc__ = """ BVH2ARM.py v2.0

Script for generating armature from BVH empties.

This script generates an armature and makes bones
follow empties created by the BVH import script.

Usage:<br>
 - Import a bvh in Blender (File->Import->BVH);<br>
 - Launch this script (Alt-P);<br>
 - Set up variables:<br>
   "hipbonename": the name of the main bone;<br>
   "startframe":  the first frame of your anim;<br>
   "endframe":  the last frame of your anim;<br>
   "decimation": the frequency (in number of frames) to which the armature is updated;<br>
   "scale": to size the created armature.
 - Press "Create Armature".
"""

#----------------------------------------------
# (c) Jean-Baptiste PERIN  octobre 2004, released under Blender Artistic Licence
#    for the Blender 2.34-2.36 Python Scripts Bundle.
#----------------------------------------------



import Blender
from Blender import Mathutils
import math

dicEmptiesRestMatrix= {}
dicEmptiesInvRestMatrix= {}
dicBoneRestMatrix= {}
dicBone={}
dicEmptyChild={}
dicBoneRestInvEmpRest={}
dicEmpRestInvBoneRest={}
restFrame = 1

########################################################################
#
#          UTILITY FUNCTIONS FOR HANDLING BONES AND EMPTIES
#
########################################################################

def names(ob): return ob.getName()

#########
# Cette fonction renvoie la liste des empties attaches a root
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getTree(emp_list, root):
	empties=getAllEmpties()
	chlds = getChildren(root, empties)
	dicEmptyChild[root.getName()]=chlds
	for ch in chlds:
		emp_list.append(ch)
		getTree(emp_list,ch)

#########
# Cette fonction renvoie la liste des empties attaches a root
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getEmpties():
	global hipbonename
	emp_list = []
	root = Blender.Object.Get(hipbonename)
	emp_list.append(root)
	getTree(emp_list, root)
	return emp_list

#########
# Cette fonction renvoie la liste des empties
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getAllEmpties():
	emp_list = []
	objs = Blender.Object.Get()
	for o in objs:
		if o.getType()=="Empty":
			emp_list.append(o)
	return emp_list

#########
# Cette fonction renvoie la liste des empties
# in  : 
# out : emp_list (List of Object) la liste des objets de type "Empty"
#########
def getEmpty(name):
	p = None
	objs = Blender.Object.Get()
	for o in objs:
		if o.getType()=="Empty" and o.getName()==name:
			p = o
	return p

def getChild(emp, emp_list):
	return dicEmptyChild[emp.getName()]


#########
# Cette fonction fournit la liste des enfants d'un empty
# in  : emp (Object) un empty
#       emp_list (List of Object) la liste des empties
# out : children (List of Object) la liste des empties enfants de 'empty'
#########
def getChildren(emp, emp_list):
	children = []
	root_emp = getRootEmpties(emp_list)
	for em in emp_list:
		if (em.getName() != emp.getName()) and (em not in root_emp):
			if (em.getParent().getName() == emp.getName()):
				children.append(em)
	return children



#########
# Cette fonction renvoie la liste des empties n'ayant pas de parent
# in  : emp_list (List) une liste d'empties
# out : root (List) le (ou les) empty n'ayant pas de parent
#########
def getRootEmpties(emp_list):
	root = []
	for em in emp_list:
		if em.getParent() == None:
			root.append(em)
	return root


#########
# Cette fonction renvoie le bone de nom 'name' dans l'armature 'armature'
# in  : armature (Armature) l'armature dans laquelle cherchait le bone
#       name (String) le nom de l'os a chercher
# out : p (Bone) 
#########
#def getBone(armature, name):
#	return (dicBone[name])
	#p = None
	#bones = armature.getBones()
	#for i in bones:
	#	if i.getName() == name:
	#		p = i
	#		break
	#return p


def eraseIPO (objectname):
	object = Blender.Object.Get(objectname)
	lIpo = object.getIpo()
	if lIpo != None:
		nbCurves = lIpo.getNcurves()
 		for i in range(nbCurves):
			nbBezPoints = lIpo.getNBezPoints(i)
			for j in range(nbBezPoints):
				lIpo.delBezPoint(i)




def GetOrCreateIPO(name):

	ipos = Blender.Ipo.Get()
	if name in map(names,ipos):
		myipo = Blender.Ipo.Get(name)
		print name+' exists'
	else:
		myipo = Blender.Ipo.New('Object',name)
		print name+' was created'
	return myipo


def GetOrCreateCurve(ipo, curvename):
	curves = ipo.getCurves()
	if curvename in map(names,curves):
		mycurve = ipo.getCurve(curvename)
		print curvename+' exists'
	else:
		mycurve = ipo.addCurve(curvename)
		print curvename+' was created'
	return mycurve




########################################################################
#
# FUNCTIONS FOR COMPUTING POSITION AND ROTATION OF BONES 
#
########################################################################

def computeRootQuat2(empty, bone):

	M1=dicBoneRestInvEmpRest[bone.getName()].rotationPart()
	M2=dicEmpRestInvBoneRest[bone.getName()].rotationPart()
	emprot = empty.getMatrix('worldspace').rotationPart()
	emprot.transpose()
	mat = M1*emprot*M2
	mat.transpose()
	return (mat.toQuat())

	#emprest = dicEmptiesRestMatrix[empty.getName()].rotationPart()
	#invemprest= dicEmptiesRestMatrix[empty.getName()].rotationPart()
	##invemprest= emprest
	##invemprest.invert()
	##invemprest= dicEmptiesInvRestMatrix[empty.getName()].rotationPart()
	#emprot = empty.getMatrix('worldspace').rotationPart()
	#bonerest = dicBoneRestMatrix[bone.getName()].rotationPart()
	#invbonerest = dicBoneRestMatrix[bone.getName()].rotationPart()
	#invbonerest.invert()
	#T2=emprot*invemprest
	#T2.transpose()
	#mat = bonerest*invemprest*T2*emprest*invbonerest
	#mat.transpose()
	#return (mat.toQuat())




#########
# Cette fonction 
# in  : 
# out : 
#########
def computeRootPos(empty, bone):
	vec = computeScaledPos(empty.getMatrix('worldspace').translationPart()) - dicBoneRestMatrix[bone.getName()].translationPart()
	mat = dicBoneRestMatrix[bone.getName()].rotationPart()
	vec2 =  Mathutils.MatMultVec (mat, vec)
	return vec2


def computeRelativePos(empty,bone):
	vec = computeScaledPos(empty.getMatrix('worldspace').translationPart())	- dicBoneRestMatrix[bone.getName()].translationPart()
	rootempty = getEmpty(hipbonename)
	vec3 = computeScaledPos(rootempty.getMatrix('worldspace').translationPart())
	mat = dicBoneRestMatrix[bone.getName()].rotationPart()
	vec2 =  Mathutils.MatMultVec (mat, vec-vec3)
	return vec2


 #########
# Cette fonction 
# in  : 
# out : 
#########
def computeScaledPos(vec):
	global scalef
	vec2 = Mathutils.Vector([vec[0]*scalef, vec[1]*scalef, vec[2]*scalef])
	return vec2

########################################################################
#
#            FUNCTIONS FOR CREATING AND MOVING ARMATURES
#
########################################################################

#########
# Cette fonction 
# in  : 
# out : 
#########
def createBone (armature, empty, bone, empties):
	children = getChildren(empty, empties)
	for ch in children:
		if len(children) >= 2:
			bonename = empty.getName()[1:len(empty.getName())]+'_'+ch.getName()[1:len(ch.getName())]
		else :
			bonename = empty.getName()[1:len(empty.getName())]
		b=Blender.Armature.Bone.New(bonename)
		b.setHead(computeScaledPos(empty.getMatrix('worldspace').translationPart()))
		b.setTail(computeScaledPos(ch.getMatrix('worldspace').translationPart()))
		#b.setParent(bone)
		matrice = empty.getMatrix('worldspace')
		invmatrice = empty.getMatrix('worldspace')
		invmatrice.invert()
		invmatricet=empty.getMatrix('worldspace')
		invmatricet.invert()
		invmatricet.transpose()
		dicEmptiesRestMatrix[empty.getName()] = matrice
		dicEmptiesInvRestMatrix[empty.getName()] = invmatrice
		armature.addBone(b)
		invbonerest=b.getRestMatrix()
		invbonerest.invert()
		dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
		dicBoneRestInvEmpRest[b.getName()]=b.getRestMatrix()*invmatrice*invmatricet
		dicEmpRestInvBoneRest[b.getName()]=matrice*invbonerest
		dicBone[b.getName()]=b
		createBone(armature, ch, b, empties)

#########
# Cette fonction 
# in  : 
# out : 
#########
def f_createBone (armData, empty, bone, empties):
	bones = armData.getBones()

	def getBone(bonename):
		bone = None
		for b in bones:
			#print b.getName()
			if b.getName() == bonename:
				bone = b
		return bone

	children = getChildren(empty, empties)
	for ch in children:
		if len(children) >= 2:
			bonename = empty.getName()[1:len(empty.getName())]+'_'+ch.getName()[1:len(ch.getName())]
		else :
			bonename = empty.getName()[1:len(empty.getName())]
		#b=Blender.Armature.Bone.New(bonename)
		b=getBone(bonename)
		#b.setHead(empty.getMatrix('worldspace').translationPart())
		#b.setTail(ch.getMatrix('worldspace').translationPart())
		#b.setParent(bone)
		matrice = empty.getMatrix('worldspace')
		invmatrice = empty.getMatrix('worldspace')
		invmatrice.invert()
		invmatricet=empty.getMatrix('worldspace')
		invmatricet.invert()
		invmatricet.transpose()
		dicEmptiesRestMatrix[empty.getName()] = matrice
		dicEmptiesInvRestMatrix[empty.getName()] = invmatrice
		#armature.addBone(b)
		invbonerest=b.getRestMatrix()
		invbonerest.invert()
		dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
		dicBoneRestInvEmpRest[b.getName()]=b.getRestMatrix()*invmatrice*invmatricet
		dicEmpRestInvBoneRest[b.getName()]=matrice*invbonerest
		dicBone[b.getName()]=b
		print "Ajout de ", b.getName(),"  au dictionnaire"
		f_createBone(armData, ch, b, empties)
	

#########
# Cette fonction fabrique une arma
# in  : 
# out : 
#########
def createArmature (rootEmpty, empties):
	armData=Blender.Armature.New('monArmature')
	children = getChildren(rootEmpty, empties)
	for ch in children:
		b=Blender.Armature.Bone.New(rootEmpty.getName()[1:len(rootEmpty.getName())] + ch.getName()[1:len(ch.getName())])
		b.setHead(computeScaledPos(rootEmpty.getMatrix('worldspace').translationPart()))
		b.setTail(computeScaledPos(ch.getMatrix('worldspace').translationPart()))
		armData.addBone(b)
		matrice = ch.getMatrix('worldspace')
		invmatrice = ch.getMatrix('worldspace')
		invmatrice.invert()
		invmatricet=ch.getMatrix('worldspace')
		invmatricet.invert()
		invmatricet.transpose()
		dicEmptiesRestMatrix[rootEmpty.getName()] = matrice
		dicEmptiesInvRestMatrix[rootEmpty.getName()] = invmatrice
		invbonerest=b.getRestMatrix()
		invbonerest.invert()
		dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
		dicBoneRestInvEmpRest[b.getName()]=b.getRestMatrix()*invmatrice*invmatricet
		dicEmpRestInvBoneRest[b.getName()]=matrice*invbonerest
		dicBone[b.getName()]=b
		createBone(armData, ch, b, empties)
	return armData



#########
# Cette fonction fabrique une arma
# in  : 
# out : 
#########
def f_createArmature (rootEmpty, empties, armData):
	bones = armData.getBones()

	def getBone(bonename):
		bone = None
		for b in bones:
			#print b.getName()
			if b.getName() == bonename:
				bone = b
		return bone

	children = getChildren(rootEmpty, empties)
	for ch in children:
		b=getBone(rootEmpty.getName()[1:len(rootEmpty.getName())] + ch.getName()[1:len(ch.getName())])
		matrice = ch.getMatrix('worldspace')
		invmatrice = ch.getMatrix('worldspace')
		invmatrice.invert()
		invmatricet=ch.getMatrix('worldspace')
		invmatricet.invert()
		invmatricet.transpose()
		dicEmptiesRestMatrix[rootEmpty.getName()] = matrice
		dicEmptiesInvRestMatrix[rootEmpty.getName()] = invmatrice
		invbonerest=b.getRestMatrix()
		invbonerest.invert()
		dicBoneRestMatrix[b.getName()] = b.getRestMatrix()
		dicBoneRestInvEmpRest[b.getName()]=b.getRestMatrix()*invmatrice*invmatricet
		dicEmpRestInvBoneRest[b.getName()]=matrice*invbonerest
		dicBone[b.getName()]=b
		print "Ajout de ", b.getName(),"  au dictionnaire"
		
		f_createBone(armData, ch, b, empties)



#########
# Cette fonction 
# in  : 
# out : 
#########
def moveBones(armature, empty, empties):
	children = dicEmptyChild[empty.getName()]
	for ch in children:
		if len(children) >= 2:
			bonename = empty.getName()[1:len(empty.getName())]+'_'+ch.getName()[1:len(ch.getName())]
		else :
			bonename = empty.getName()[1:len(empty.getName())]
		bone = dicBone[bonename]
		#bone.setLoc(computeRootPos(empty,bone))
		bone.setLoc(computeRelativePos(empty,bone))
		bone.setQuat(computeRootQuat2(empty,bone))
		chch = dicEmptyChild[ch.getName()]
		if len(chch) >= 1:
			moveBones(armature, ch, empties)


#########
# Cette fonction 
# in  : 
# out : 
#########
def moveArmature (armature, empties):
	root = Blender.Object.Get(hipbonename)
	children = dicEmptyChild[hipbonename]
	for ch in children:
		b=dicBone[hipbonename[1:len(hipbonename)] + ch.getName()[1:len(ch.getName())]]
		#b.setLoc(computeRootPos(root, b))
		b.setLoc([0.0, 0.0, 0.0])
		b.setQuat(computeRootQuat2(root, b))
		moveBones(armature, ch, empties)


def eraseIPO (objectname):
	object = Blender.Object.Get(objectname)
	lIpo = object.getIpo()
	if lIpo != None:
		nbCurves = lIpo.getNcurves()
 		for i in range(nbCurves):
			nbBezPoints = lIpo.getNBezPoints(i)
			for j in range(nbBezPoints):
				lIpo.delBezPoint(i)


########################################################################
#
#                  MAIN PROGRAM
#
########################################################################

def RemoveEmpties():

	global endframe, startframe,hipbonename

	lesEmpties = getEmpties()
	scn = Blender.Scene.getCurrent()
	#scn.link (armObj)
	for em in lesEmpties:
		eraseIPO (em.getName())
		scn.unlink(em)
	Blender.Redraw()


def Main():

	global endframe, startframe,hipbonename, framedecimation


	print "*****START*****"

	Blender.Set("curframe",restFrame)

	##-----------
	## Positionnement des empties
	##-----------
	em0 = Blender.Object.Get(hipbonename)

	Blender.Redraw()



	##-----------
	## Creation de l'armature et des os
	##-----------

	lesEmpties = getEmpties()
	#print dicEmptyChild	

	#armData = createArmature(em0, lesEmpties)
	objects = Blender.Object.Get()
	if 'OBArmature' in map(names,objects):
		armObj = Blender.Object.Get('OBArmature')
		armData = armObj.getData()
		print 'OBArmature'+' exists'
		eraseIPO ('OBArmature')
		#print armData.getBones()
		f_createArmature(em0, lesEmpties, armData)
	else:
		armData= createArmature(em0, lesEmpties)
		armObj=Blender.Object.New('Armature', 'OBArmature')
		armObj.link(armData)
		scn = Blender.Scene.getCurrent()
		scn.link (armObj)

		print 'OBArmature'+' was created'
	#return myobj
 
	##-----------
	## Creation de l'ipo de l'armature
	##-----------
	lipo = GetOrCreateIPO('BVHIpo')
	armObj.setIpo(lipo)
	curvX =  GetOrCreateCurve(lipo, 'LocX')
	curvY =  GetOrCreateCurve(lipo, 'LocY')
	curvZ =  GetOrCreateCurve(lipo, 'LocZ')


	#armData.drawAxes(1)
	#armData.drawNames(1)

	

	Blender.Redraw()

	##-----------
	## Enregistrement de la position  de l'armature
	##-----------

	bones = armData.getBones()
	for bo in bones:
		bo.setPose([Blender.Armature.Bone.ROT, Blender.Armature.Bone.LOC]) 

	curvX.addBezier((Blender.Get("curframe"), getEmpty(hipbonename).getMatrix('worldspace').translationPart()[0]*scalef))
	curvY.addBezier((Blender.Get("curframe"), getEmpty(hipbonename).getMatrix('worldspace').translationPart()[1]*scalef))
	curvZ.addBezier((Blender.Get("curframe"), getEmpty(hipbonename).getMatrix('worldspace').translationPart()[2]*scalef))
	curvX.setInterpolation('Linear')
	curvX.setExtrapolation('Constant')
	curvY.setInterpolation('Linear')
	curvY.setExtrapolation('Constant')
	curvZ.setInterpolation('Linear')
	curvZ.setExtrapolation('Constant')

	Blender.Redraw()

	Blender.Set("curframe",startframe)
	while endframe >= Blender.Get("curframe"):

		##-----------
		## Positionnement des os
		##-----------

		moveArmature(armData, lesEmpties)


		##-----------
		## Enregistrement de la position  de l'armature
		##-----------

		for bo in bones:
			bo.setPose([Blender.Armature.Bone.ROT, Blender.Armature.Bone.LOC]) 
		curvX.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').translationPart()[0])*scalef))
		curvY.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').translationPart()[1])*scalef))
		curvZ.addBezier((Blender.Get("curframe"), (getEmpty(hipbonename).getMatrix('worldspace').translationPart()[2])*scalef))

		##-----------
		## Passage a la frame suivante
		##-----------
		num_frame = Blender.Get("curframe")+framedecimation
		print num_frame
		Blender.Set("curframe", num_frame)

	Blender.Set("curframe",startframe)
	Blender.Redraw()

	print "*****END*****"


########################################################################
#
#            GUI FUNCTIONS AND VARIABLES
#
########################################################################

EFrame = Blender.Draw.Create(5)
IFrame = Blender.Draw.Create(6)
SFrame2 = Blender.Draw.Create(5)
HBName = Blender.Draw.Create(0)
FrameDecimation = Blender.Draw.Create(5)
ScaleF =  Blender.Draw.Create(0)

Msg = ' '

def event (evt, val):
	if evt == Blender.Draw.ESCKEY:
		Blender.Draw.Exit()
	return

def button_event(evt):
	global EFrame, IFrame, SFrame2, HBName, Msg , FrameDecimation, ScaleF
	global endframe, startframe, insertionframe, hipbonename, framedecimation , scalef
	if evt==1:
		startframe = SFrame2.val
		insertionframe = IFrame.val
		endframe =  EFrame.val
		hipbonename = HBName.val
		framedecimation = FrameDecimation.val
		scalef= eval(str(ScaleF.val))
		print "scalef = ", scalef
		if startframe>=endframe:
			Msg = 'Start frame must be lower than End frame'
			Blender.Draw.PupMenu("ERROR: %s" % Msg)
		else:
			ob = getEmpty(hipbonename)
			if (ob!=None): 
				if ob.getParent()!=None:
					Msg = 'Empty '+hipbonename+ ' is not a root bone.'
					Blender.Draw.PupMenu("ERROR: %s" % Msg)
				else:  
					if (0.0 > scalef):
						Msg = 'Scale factor must be greater than 0'
						Blender.Draw.PupMenu("ERROR: %s" % Msg)
					else:
						#Blender.Draw.Exit()
						Main()
			else:
				Msg = 'Empty '+ hipbonename+ ' not found'
				Blender.Draw.PupMenu("ERROR: %s" % Msg)
				
		#Blender.Draw.Redraw(1)
	elif evt==2:
		hipbonename = HBName.val
		ob = getEmpty(hipbonename)
		if (ob!=None): 
			if ob.getParent()!=None:
				Msg = 'Empty '+hipbonename+ ' is not a root bone.'
				Blender.Draw.PupMenu("ERROR: %s" % Msg)
			else:  
				#Blender.Draw.Exit()
				RemoveEmpties()
		else:
			Msg = 'Empty '+ hipbonename+ ' not found'
			Blender.Draw.PupMenu("ERROR: %s" % Msg)

	#else:
	#	print "evt = ",evt

def GUI():
	global EFrame, SFrame2, HBName, Msg , ScaleF, FrameDecimation
	Blender.BGL.glClearColor(0,0,1,1)
	Blender.BGL.glClear(Blender.BGL.GL_COLOR_BUFFER_BIT)
	Blender.BGL.glColor3f(1,1,1)
	Blender.BGL.glRasterPos2i(20,200)
	Blender.Draw.Text ("BVH 2 ARMATURE v2.0 by Jean-Baptiste PERIN", 'normal')
	HBName = Blender.Draw.String("HipBoneName: ", -1, 20, 175, 250, 20, '_Hips', 100)
	SFrame2 = Blender.Draw.Number("Startframe: ", -1, 20, 150, 250, 20, 1, 1,3000,"")
	EFrame = Blender.Draw.Number("Endframe: ", -1, 20, 125, 250, 20, Blender.Get("endframe"), 1,3000,"")
	#IFrame = Blender.Draw.Number("Insertionframe: ", -1, 20, 100, 250, 20, Blender.Get("staframe"), 1,3000,"")
	FrameDecimation = Blender.Draw.Number("FrameDecimation: ", -1, 20, 75, 250, 20,5, 1,10,'')
	ScaleF = Blender.Draw.Number("Scale: ", -1, 20, 50, 250, 20, 0.03, 0.0, 10.0,  'Scale Factor')
	Blender.Draw.Toggle("Create Armature", 1, 20, 10, 100, 20, 0, "Create Armature")
	#Blender.Draw.Toggle("Remove Empties", 2, 200, 10, 100, 20, 0, "Remove Empties")
	Blender.BGL.glRasterPos2i(20,40)
	Blender.Draw.Text (Msg, 'normal')


Blender.Draw.Register(GUI, event, button_event)
