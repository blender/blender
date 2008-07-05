#!BPY

"""
Name: 'Bake Constraints'
Blender: 246
Group: 'Animation'
Tooltip: 'Bake a Constrained object/rig to IPOs'
Fillename: 'Bake_Constraint.py'
"""

__author__ = "Roger Wickes (rogerwickes(at)yahoo.com)"
__script__ = "Animation Bake Constraints"
__version__ = "0.7"
__url__ = ["Communicate problems and errors, http://www.blenderartists.com/forum/private.php?do=newpm to PapaSmurf"]
__email__= ["Roger Wickes, rogerwickes@yahoo.com", "scripts"]
__bpydoc__ = """\

bake_constraints

This script bakes the real-world LocRot of an object (the net effect of any constraints - 
(Copy, Limit, Track, Follow, - that affect Location, Rotation)
(usually one constrained to match another's location and/or Tracked to another)
and creates a clone with a set of Ipo Curves named Ipo<objname>
These curves control a non-constrained object and thus make it mimic the constrained object
Actions can be then be edited without the need for the drivers/constraining objects

Developed for use with MoCap data, where a bone is constrained to point at an empty
moving through space and time. This records the actual locrot of the armature
so that the motion can be edited, reoriented, scaled, and used as NLA Actions

see also wiki Scripts/Manual/ Tutorial/Motion Capture <br>

Usage: <br>
 - Select the reference Object(s) you want to bake <br>
 - Set the frame range to bake in the Anim Panel <br>
 - Set the test code (if you want a self-test) in the RT field in the Anim Panel <br>
	-- Set RT:1 to create a test armature <br>
	-- Set RT: up to 100 for more debug messages and status updates <br>
<br>
 - Run the script <br>
 - The clone copy of the object is created and it has an IPO curve assigned to it. <br>
 - The clone shadows the object by an offset locrot (see usrDelta) <br>
 - That Object has Ipo Location and Rotation curves that make the clone mimic the movement <br>
	of the selected object, but without using constraints. <br>
 - If the object was an Armature, the clone's bones move identically in relation to the <br>
	original armature, and an Action is created that drives the bone movements. <br>

Version History:
	0.1: bakes Loc Rot for a constrained object
	0.2: bakes Loc and Rot for the bones within Armature object
	0.3: UI for setting options
	0.3.1 add manual to script library
	0.4: bake multiple objects
	0.5: root bone worldspace rotation
	0.6: re-integration with BPyArmature
	0.7: bakes parents and leaves clones selected
    
License, Copyright, and Attribution:
  by Roger WICKES  May 2008, released under Blender Artistic Licence to Public Domain
    feel free to add to any Blender Python Scripts Bundle.
 Thanks to Jean-Baptiste PERIN, IdeasMan42 (Campbell Barton), Basil_Fawlty/Cage_drei (Andrew Cruse)
 much lifted/learned from blender.org/documentation/245PytonDoc and wiki
 some modules based on c3D_Import.py, PoseLib16.py and IPO/Armature code examples e.g. camera jitter

Pseudocode:
	Initialize
	If at least one object is selected
		For each selected object,
			create a cloned object
			remove any constraints on the clone
			create or reset an ipo curve named like the object
			for each frame
				 set the clone's locrot key based on the reference object
			if it's an armature,
				create an action (which is an Ipo for each bone)
				for each frame of the animation
					for each bone in the armature
						set the key
	Else you're a smurf

Test Conditions and Regressions:
	1. (v0.1) Non-armatures (the cube), with ipo curve and constraints at the object level
	2. armatures, with ipo curve and constraints at the object level
	3. armatures, with bones that have ipo curves and constraints
	4. objects without parents, children with unselected parents, select children first.
  
Naming conventions:
	arm = a specific objec type armature
	bone = bones that make up the skeleton of an armature

	ob = object, an instance of an object type
	ebone = edit bone, a bone in edit mode
	pbone = pose bone, a posed bone in an object
	tst = testing, self-test routines
	usr = user-entered or designated stuff  
"""
########################################

import Blender
from   Blender import *
from   Blender.Mathutils import *
import struct
import string
import bpy
import BPyMessages
import BPyArmature
# reload(BPyArmature)
from   BPyArmature import getBakedPoseData

Vector= Blender.Mathutils.Vector
Euler= Blender.Mathutils.Euler
Matrix= Blender.Mathutils.Matrix #invert() function at least
RotationMatrix = Blender.Mathutils.RotationMatrix
TranslationMatrix= Blender.Mathutils.TranslationMatrix
Quaternion = Blender.Mathutils.Quaternion
Vector = Blender.Mathutils.Vector
POSE_XFORM= [Blender.Object.Pose.LOC, Blender.Object.Pose.ROT]

#=================
# Global Variables
#=================

# set senstitivity for displaying debug/console messages. 0=none, 100=max
# then call debug(num,string) to conditionally display status/info in console window
MODE=Blender.Get('rt')  #execution mode: 0=run normal, 1=make test armature
DEBUG=Blender.Get('rt')   #how much detail on internal processing for user to see. range 0-100
BATCH=False #called from command line? is someone there? Would you like some cake?

#there are two coordinate systems, the real, or absolute 3D space,
# and the local relative to a parent.
COORDINATE_SYSTEMS = ['local','real']
COORD_LOCAL = 0
COORD_REAL = 1

# User Settings - Change these options manually or via GUI (future TODO)
usrCoord = COORD_REAL # what the user wants
usrParent = False # True=clone keeps original parent, False = clone's parent is the clone of the original parent (if cloned)
usrFreeze = 2 #2=yes, 0=no. Freezes shadow object in place at current frame as origin
# delta is amount to offset/change from the reference object. future set in a ui, so technically not a constant
usrDelta = [10,10,0,0,0,0] #order specific - Loc xyz Rot xyz
usrACTION = True # Offset baked Action frames to start at frame 1

CURFRAME = 'curframe' #keyword to use when getting the frame number that the scene is presently on
ARMATURE = 'Armature' #en anglais
BONE_SPACES = ['ARMATURESPACE','BONESPACE']
		# 'ARMATURESPACE' - this matrix of the bone in relation to the armature
		# 'BONESPACE' - the matrix of the bone in relation to itself

#Ipo curves created are prefixed with a name, like Ipo_ or Bake_ followed by the object/bone name
#bakedArmName = "b." #used for both the armature class and object instance
usrObjectNamePrefix= ""
#ipoBoneNamePrefix  = ""
# for example, if on entry an armature named Man was selected, and the object prefix was "a."
#  on exit an armature and an IPO curve named a.Man exists for the object as a whole
# if that armature had bones (spine, neck, arm) and the bone prefix was "a."
#  the bones and IPO curves will be (a.spine, a.neck, a.arm)

R2D = 18/3.1415  # radian to grad
BLENDER_VERSION = Blender.Get('version')

# Gets the current scene, there can be many scenes in 1 blend file. 
scn = Blender.Scene.GetCurrent()

#=================
# Methods
#=================
########################################
def debug(num,msg): #use log4j or just console here.
	if DEBUG >= num:
		if BATCH == False:
			print 'debug:             '[:num/10+7]+msg
		#TODO: else write out to file (runs faster if it doesnt have to display details)
	return

########################################
def error(str):
	debug(0,'ERROR: '+str)
	if BATCH == False:
		Draw.PupMenu('ERROR%t|'+str)
	return

########################################
def getRenderInfo():
	context=scn.getRenderingContext() 
	staframe = context.startFrame()
	endframe = context.endFrame()
	if endframe<staframe: endframe=staframe
	curframe = Blender.Get(CURFRAME)
	debug(90,'Scene is on frame %i and frame range is %i to %i' % (curframe,staframe,endframe))
	return (staframe,endframe,curframe)

########################################
def sortObjects(obs): #returns a list of objects sorted based on parent dependency
	obClones= [] 
	while len(obClones) < len(obs):
		for ob in obs:
			if not ob in obClones:
				par= ob.getParent()
				#if no parent, or the parent is not scheduled to be cloned
				if par==None:
					obClones.append(ob) # add the independent
				elif par not in obs: # parent will not be cloned
					obClones.append(ob) # add the child
				elif par in obClones: # is it on the list?
					obClones.append(ob) # add the child
				# parent may be a child, so it will be caught next time thru
	debug(100,'clone object order: \n%s' % obClones)
	return obClones # ordered list of (ob, par) tuples

########################################
def sortBones(xbones): #returns a sorted list of bones that should be added,sorted based on parent dependency
#  while there are bones to add,
#    look thru the list of bones we need to add
#      if we have not already added this bone
#         if it does not have a parent
#           add it
#          else, it has a parent
#           if we already added it's parent
#               add it now.
#           else #we need to keep cycling and catch its parent
#         else it is a root bone
#           add it
#       else skip it, it's already in there
#     endfor
#  endwhile
	xboneNames=[]
	for xbone in xbones: xboneNames.append(xbone.name)
	debug (80,'reference bone order: \n%s' % xboneNames)
	eboneNames=[]
	while len(eboneNames) < len(xboneNames):
		for xbone in xbones:
			if not xbone.name in eboneNames:
				if not xbone.parent:
					eboneNames.append(xbone.name)
				else:
					if xbone.parent.name in eboneNames:
						eboneNames.append(xbone.name)
					#else skip it
				#endif
			#else prego
		#endfor
	#endwhile
	debug (80,'clone bone order: \n%s' % eboneNames)
	return eboneNames

########################################
def dupliArmature(ob): #makes a copy in current scn of the armature used by ob and its bones
	ob_mat = ob.matrixWorld
	ob_data = ob.getData()
	debug(49,'Reference object uses %s' % ob_data)
	arm_ob = Armature.Get(ob_data.name) #the armature used by the passed object
	
	arm = Blender.Armature.New()
	debug(20,'Cloning Armature %s to create %s' % (arm_ob.name, arm.name))
	arm.drawType = Armature.STICK #set the draw type

	arm.makeEditable() #enter editmode

	# for each bone in the object's armature,
	xbones=ob.data.bones.values()
	usrSpace = 0 #0=armature, 1=local
	space=[BONE_SPACES[usrSpace]][0] 

	#we have to make a list of bones, then figure out our parents, then add to the arm
	#when creating a child, we cannot link to a parent if it does not yet exist in our armature 
	ebones = [] #list of the bones I want to create for my arm

	eboneNames = sortBones(xbones)

	i=0
	# error('bones sorted. continue?')
	for abone in eboneNames: #set all editable attributes to fully define the bone.
		for bone in xbones: 
			if bone.name == abone: break  # get the reference bone
		ebone = Armature.Editbone() #throw me a bone, bone-man!
		ebones.append(ebone) #you're on my list, buddy

		ebone.name = bone.name
		ebone.headRadius = bone.headRadius
		ebone.tailRadius = bone.tailRadius
		ebone.weight = bone.weight
		ebone.options = bone.options
		
		ebone.head = bone.head[space] #dictionary lookups
		ebone.tail = bone.tail[space]
		ebone.matrix = bone.matrix[space]
		ebone.roll = bone.roll[space]

		debug(30,'Generating new %s as child of %s' % (bone,bone.parent))
		if bone.hasParent(): 
#      parent=bone.parent.name
#      debug(100,'looking for %s' % parent)
#      for parbone in xbones: if parbone.name == parent: break # get the parent bone
#      ebone.parent = arm.bones[ebones[j].name]
			ebone.parent = arm.bones[bone.parent.name]
#    else:
#       ebone.parent = None
		debug(30,'Generating new editbone %s as child of %s' % (ebone,ebone.parent))
		arm.bones[ebone.name] = ebone # i would have expected an append or add function, but this works

	debug (100,'arm.bones: \n%s' % arm.bones)
	debug (20,'Cloned %i bones now in armature %s' %(len(arm.bones),arm.name)) 

	myob = scn.objects.new(arm) #interestingly, object must be created before 
	arm.update()                #armature can be saved
	debug(40,'dupArm finished %s instanced as object %s' % (arm.name,myob.getName()))
	print ob.matrix
	print myob.matrix
	
	return myob
########################################
def scrub(): # scrubs to startframe
	staFrame,endFrame,curFrame = getRenderInfo()

	# eye-candy, go from current to start, fwd or back
	if not BATCH:
		debug(100, "Positioning to start...")
		frameinc=(staFrame-curFrame)/10
		if abs(frameinc) >= 1:
			for i in range(10):
				curFrame+=frameinc
				Blender.Set(CURFRAME,curFrame) # computes the constrained location of the 'real' objects
				Blender.Redraw()
	Blender.Set(CURFRAME, staFrame)
	return

########################################
def bakeBones(ref_ob,arm_ob): #copy pose from ref_ob to arm_ob
	scrub()
	staFrame,endFrame,curFrame = getRenderInfo()
	act = getBakedPoseData(ref_ob, staFrame, endFrame, ACTION_BAKE = True, ACTION_BAKE_FIRST_FRAME = usrACTION) # bake the pose positions of the reference ob to the armature ob
	arm_ob.action = act
	scrub()  
	
	# user comprehension feature - change action name and channel ipo names to match the names of the bone they drive
	debug (80,'Renaming each action ipo to match the bone they pose')
	act.name = arm_ob.name
	arm_channels = act.getAllChannelIpos()
	pose= arm_ob.getPose()
	pbones= pose.bones.values() #we want the bones themselves, not the dictionary lookup
	for pbone in pbones:
		debug (100,'Channel listing for %s: %s' % (pbone.name,arm_channels[pbone.name] ))
		ipo=arm_channels[pbone.name]
		ipo.name = pbone.name # since bone names are unique within an armature, the pose names can be the same since they are within an Action

	return

########################################
def getOrCreateCurve(ipo, curvename):
	"""
	Retrieve or create a Blender Ipo Curve named C{curvename} in the C{ipo} Ipo
	Either an ipo curve named C{curvename} exists before the call then this curve is returned,
	Or such a curve doesn't exist before the call .. then it is created into the c{ipo} Ipo and returned 
	"""
	try:
		mycurve = ipo.getCurve(curvename)
		if mycurve != None:
			pass
		else:
			mycurve = ipo.addCurve(curvename)
	except:
		mycurve = ipo.addCurve(curvename)
	return mycurve

########################################
def eraseCurve(ipo,numCurves):
	debug(90,'Erasing %i curves for %' % (numCurves,ipo.GetName()))
	for i in range(numCurves):
		nbBezPoints= ipo.getNBezPoints(i)
		for j in range(nbBezPoints):
			ipo.delBezPoint(i)
	return

########################################
def resetIPO(ipo):
	debug(60,'Resetting ipo curve named %s' %ipo.name)
	numCurves = ipo.getNcurves() #like LocX, LocY, etc
	if numCurves > 0:
		eraseCurve(ipo, numCurves) #erase data if one exists
	return

########################################
def resetIPOs(ob): #resets all IPO curvess assocated with an object and its bones
	debug(30,'Resetting any ipo curves linked to %s' %ob.getName())
	ipo = ob.getIpo() #may be None
	ipoName = ipo.getName() #name of the IPO that guides/controls this object
	debug(70,'Object IPO is %s' %ipoName)
	try:
		ipo = Ipo.Get(ipoName)
	except:
		ipo = Ipo.New('Object', ipoName)
	resetIPO(ipo)
	if ob.getType() == ARMATURE:
		arm_data=ob.getData()
		bones=arm_data.bones.values()
		for bone in bones:
			#for each bone: get the name and check for a Pose IPO
			debug(10,'Processing '+ bone.name)
	return

########################################
def parse(string,delim):
	index = string.find(delim) # -1 if not found, else pointer to delim
	if index+1:  return string[:index]
	return string

########################################
def newIpo(ipoName): #add a new Ipo object to the Blender scene
	ipo=Blender.Ipo.New('Object',ipoName)

	ipo.addCurve('LocX')
	ipo.addCurve('LocY')
	ipo.addCurve('LocZ')
	ipo.addCurve('RotX')
	ipo.addCurve('RotY')
	ipo.addCurve('RotZ')
	return ipo

########################################
def makeUpaName(type,name): #i know this exists in Blender somewhere...
	debug(90,'Making up a new %s name using %s as a basis.' % (type,name))
	name = (parse(name,'.'))
	if type == 'Ipo':
		ipoName = name # maybe we get lucky today
		ext = 0
		extlen = 3 # 3 digit extensions, like hello.002
		success = False
		while not(success):
			try:
				debug(100,'Trying %s' % ipoName)
				ipo = Ipo.Get(ipoName)
				#that one exists if we get here. add on extension and keep trying
				ext +=1
				if ext>=10**extlen: extlen +=1 # go to more digits if 999 not found
				ipoName = '%s.%s' % (name, str(ext).zfill(extlen))
			except: # could not find it
				success = True
		name=ipoName 
	else:
		debug (0,'FATAL ERROR: I dont know how to make up a new %s name based on %s' % (type,ob))
		return None
	return name

########################################
def createIpo(ob): #create an Ipo and curves and link them to this object
	#first, we have to create a unique name
	#try first with just the name of the object to keep things simple.
	ipoName = makeUpaName('Ipo',ob.getName()) # make up a name for a new Ipo based on the object name
	debug(20,'Ipo and LocRot curves called %s' % ipoName)
	ipo=newIpo(ipoName)
	ob.setIpo(ipo) #link them
	return ipo

########################################
def getLocLocal(ob):
	key = [
			ob.LocX, 
			ob.LocY, 
			ob.LocZ,
			ob.RotX*R2D, #get the curves in this order
			ob.RotY*R2D, 
			ob.RotZ*R2D
			]
	return key

########################################
def getLocReal(ob):
	obMatrix = ob.matrixWorld #Thank you IdeasMan42
	loc = obMatrix.translationPart()
	rot = obMatrix.toEuler()
	key = [
			loc.x,
			loc.y,
			loc.z,
			rot.x/10,
			rot.y/10,
			rot.z/10
			]
	return key

########################################
def getLocRot(ob,space):
	if space in xrange(len(COORDINATE_SYSTEMS)):
		if space == COORD_LOCAL:
			key = getLocLocal(ob)
			return key
		elif space == COORD_REAL:
			key = getLocReal(ob)
			return key
		else: #hey, programmers make mistakes too.
			debug(0,'Fatal Error: getLoc called with %i' % space)
	return

########################################
def getCurves(ipo):
	ipos = [
			ipo[Ipo.OB_LOCX],
			ipo[Ipo.OB_LOCY],
			ipo[Ipo.OB_LOCZ],
			ipo[Ipo.OB_ROTX], #get the curves in this order
			ipo[Ipo.OB_ROTY],
			ipo[Ipo.OB_ROTZ]
			]
	return ipos

########################################
def addPoint(time,keyLocRot,ipos):
	if BLENDER_VERSION < 245:
		debug(0,'WARNING: addPoint uses BezTriple')
	for i in range(len(ipos)):
		point = BezTriple.New() #this was new with Blender 2.45 API
		point.pt = (time, keyLocRot[i])
		point.handleTypes = [1,1]

		ipos[i].append(point)
	return ipos

########################################
def bakeFrames(ob,myipo): #bakes an object in a scene, returning the IPO containing the curves
	myipoName = myipo.getName()
	debug(20,'Baking frames for scene %s object %s to ipo %s' % (scn.getName(),ob.getName(),myipoName))
	ipos = getCurves(myipo)
	#TODO: Gui setup idea: myOffset
	# reset action to start at frame 1 or at location
	myOffset=0 #=1-staframe
	#loop through frames in the animation. Often, there is rollup and the mocap starts late
	staframe,endframe,curframe = getRenderInfo()
	for frame in range(staframe, endframe+1):
		debug(80,'Baking Frame %i' % frame)
		#tell Blender to advace to frame
		Blender.Set(CURFRAME,frame) # computes the constrained location of the 'real' objects
		if not BATCH: Blender.Redraw() # no secrets, let user see what we are doing
			
		#using the constrained Loc Rot of the object, set the location of the unconstrained clone. Yea! Clones are FreeMen
		key = getLocRot(ob,usrCoord) #a key is a set of specifed exact channel values (LocRotScale) for a certain frame
		key = [a+b for a,b in zip(key, usrDelta)] #offset to the new location

		myframe= frame+myOffset
		Blender.Set(CURFRAME,myframe)
		
		time = Blender.Get('curtime') #for BezTriple
		ipos = addPoint(time,key,ipos) #add this data at this time to the ipos
		debug(100,'%s %i %.3f %.2f %.2f %.2f %.2f %.2f %.2f' % (myipoName, myframe, time, key[0], key[1], key[2], key[3], key[4], key[5]))
	# eye-candy - smoothly rewind the animation, showing now how the clone match moves
	if endframe-staframe <400 and not BATCH:
		for frame in range (endframe,staframe,-1): #rewind
			Blender.Set(CURFRAME,frame) # computes the constrained location of the 'real' objects
			Blender.Redraw()
	Blender.Set(CURFRAME,staframe)
	Blender.Redraw()

	return ipos

########################################
def duplicateLinked(ob):
	obType = ob.type
	debug(10,'Duplicating %s Object named %s' % (obType,ob.getName()))
	scn.objects.selected = [ob]
##      rdw: simplified by just duplicating armature. kept code as reference for creating armatures
##        disadvantage is that you cant have clone as stick and original as octahedron
##        since they share the same Armature. User can click Make Single User button.      
##      if obType == ARMATURE: #build a copy from scratch
##        myob= dupliArmature(ob)
##      else:
	Blender.Object.Duplicate() # Duplicate linked, including pose constraints.
	myobs = Object.GetSelected() #duplicate is top on the list
	myob = myobs[0]
	if usrParent == False:
		myob.clrParent(usrFreeze)     
	debug(20,'=myob= was created as %s' % myob.getName())
	return myob

########################################
def removeConstraints(ob):
	for const in ob.constraints:
		debug(90,'removed %s => %s' % (ob.name, const))
		ob.constraints.remove(const)
	return
    
########################################
def removeConstraintsOb(ob): # from object or armature
	debug(40,'Removing constraints from '+ob.getName())
	if BLENDER_VERSION > 241: #constraints module not available before 242
		removeConstraints(ob)
		if ob.getType() == ARMATURE:
			pose = ob.getPose()
			for pbone in pose.bones.values():
				#bone = pose.bones[bonename]
				removeConstraints(pbone)
		#should also check if it is a deflector?
	return

########################################
def deLinkOb(type,ob): #remove linkages
	if type == 'Ipo':
		success = ob.clearIpo() #true=there was one
		if success: debug(80,'deLinked Ipo curve to %s' % ob.getName())
	return

########################################
def bakeObject(ob): #bakes the core object locrot and assigns the Ipo to a Clone
	if ob != None:  
		# Clone the object - duplicate it, clean the clone, and create an ipo curve for the clone
		myob = duplicateLinked(ob)  #clone it
		myob.name= usrObjectNamePrefix + ob.getName()
		removeConstraintsOb(myob)   #my object is a free man
		deLinkOb('Ipo',myob)        #kids, it's not nice to share. you've been lied to
		if ob.getType() != ARMATURE: # baking armatures is based on bones, not object
			myipo = createIpo(myob)     #create own IPO and curves for the clone object
			ipos = bakeFrames(ob,myipo) #bake the locrot for this obj for the scene frames
	return myob
    
########################################
def bake(ob,par): #bakes an object of any type, linking it to parent
	debug(0,'Baking %s object %s' % (ob.getType(), ob))
	clone = bakeObject(ob) #creates and bakes the object motion
	if par!= None:
		par.makeParent([clone])  
		debug(20,"assigned object to parent %s" % par)
	if ob.getType() == ARMATURE:
##              error('Object baked. Continue with bones?')
		bakeBones(ob,clone) #go into the bones and copy from -> to in frame range
	#future idea: bakeMesh (net result of Shapekeys, Softbody, Cloth, Fluidsim,...)
	return clone
    
########################################
def tstCreateArm(): #create a test armature in scene
	# rip-off from http://www.blender.org/documentation/245PythonDoc/Pose-module.html - thank you!

	debug(0,'Making Test Armature')
	# New Armature
	arm_data= Armature.New('myArmature')
	print arm_data
	arm_ob = scn.objects.new(arm_data)
	arm_data.makeEditable()

	# Add 4 bones
	ebones = [Armature.Editbone(), Armature.Editbone(), Armature.Editbone(), Armature.Editbone()]

	# Name the editbones
	ebones[0].name = 'Bone.001'
	ebones[1].name = 'Bone.002'
	ebones[2].name = 'Bone.003'
	ebones[3].name = 'Bone.004'

	# Assign the editbones to the armature
	for eb in ebones:
		arm_data.bones[eb.name]= eb

	# Set the locations of the bones
	ebones[0].head= Mathutils.Vector(0,0,0)
	ebones[0].tail= Mathutils.Vector(0,0,1) #tip
	ebones[1].head= Mathutils.Vector(0,0,1)
	ebones[1].tail= Mathutils.Vector(0,0,2)
	ebones[2].head= Mathutils.Vector(0,0,2)
	ebones[2].tail= Mathutils.Vector(0,0,3)
	ebones[3].head= Mathutils.Vector(0,0,3)
	ebones[3].tail= Mathutils.Vector(0,0,4)

	ebones[1].parent= ebones[0]
	ebones[2].parent= ebones[1]
	ebones[3].parent= ebones[2]

	arm_data.update()
	# Done with editing the armature

	# Assign the pose animation
	arm_pose = arm_ob.getPose()

	act = arm_ob.getAction()
	if not act: # Add a pose action if we dont have one
		act = Armature.NLA.NewAction()
		act.setActive(arm_ob)

	xbones=arm_ob.data.bones.values()
	pbones = arm_pose.bones.values()

	frame = 1
	for pbone in pbones: # set bones to no rotation
		pbone.quat[:] = 1.000,0.000,0.000,0.0000
		pbone.insertKey(arm_ob, frame, Object.Pose.ROT)

	# Set a different rotation at frame 25
	pbones[0].quat[:] = 1.000,0.1000,0.2000,0.20000
	pbones[1].quat[:] = 1.000,0.6000,0.5000,0.40000
	pbones[2].quat[:] = 1.000,0.1000,0.3000,0.40000
	pbones[3].quat[:] = 1.000,-0.2000,-0.3000,0.30000

	frame = 25
	for i in xrange(4):
		pbones[i].insertKey(arm_ob, frame, Object.Pose.ROT)

	pbones[0].quat[:] = 1.000,0.000,0.000,0.0000
	pbones[1].quat[:] = 1.000,0.000,0.000,0.0000
	pbones[2].quat[:] = 1.000,0.000,0.000,0.0000
	pbones[3].quat[:] = 1.000,0.000,0.000,0.0000

	frame = 50      
	for pbone in pbones: # set bones to no rotation
		pbone.quat[:] = 1.000,0.000,0.000,0.0000
		pbone.insertKey(arm_ob, frame, Object.Pose.ROT)

	return arm_ob

########################################
def tstMoveOb(ob): # makes a simple LocRot animation of object in the scene
	anim = [
		#Loc      Rot/10
		#
		( 0,0,0, 0, 0, 0), #frame 1 origin
		( 1,0,0, 0, 0, 0), #frame 2
		( 1,1,0, 0, 0, 0),
		( 1,1,1, 0, 0, 0),
		( 1,1,1,4.5,  0,  0),
		( 1,1,1,4.5,4.5,  0),
		( 1,1,1,4.5,4.5,4.5)
		]
	space = COORD_LOCAL
	ipo = createIpo(ob) #create an Ipo and curves for this object
	ipos = getCurves(ipo)
	
	# span this motion over the currently set anim range
	# to set points, i need time but do not know how it is computed, so will have to advance the animation
	staframe,endframe,curframe = getRenderInfo()

	frame = staframe #x position of new ipo datapoint. set to staframe if you want a match
	frameDelta=(endframe-staframe)/(len(anim)) #accomplish the animation in frame range
	for key in anim: #effectively does a getLocRot()
		#tell Blender to advace to frame
		Blender.Set('curframe',frame) # computes the constrained location of the 'real' objects
		time = Blender.Get('curtime')

		ipos = addPoint(time,key,ipos) #add this data at this time to the ipos

		debug(100,'%s %i %.3f %.2f %.2f %.2f %.2f %.2f %.2f' % (ipo.name, frame, time, key[0], key[1], key[2], key[3], key[4], key[5]))
		frame += frameDelta
	Blender.Set(CURFRAME,curframe) # reset back to where we started
	return
#=================
# Program Template
#=================
########################################
def main():
	# return code set via rt button in Blender Buttons Scene Context Anim panel
	if MODE == 1: #create test armature #1
		ob = tstCreateArm()      # make test arm and select it
		tstMoveOb(ob)
		scn.objects.selected = [ob]

	obs= Blender.Object.GetSelected() #scn.objects.selected
	obs= sortObjects(obs)
	debug(0,'Baking %i objects' % len(obs))

	if len(obs) >= 1:   # user might have multiple objects selected
		i= 0
		clones=[] # my clone army
		for ob in obs:
			par= ob.getParent()
			if not usrParent:
				if par in obs:
					par= clones[obs.index(par)]
			clones.append(bake(ob,par))
		scn.objects.selected = clones
	else:
		error('Please select at least one object')
	return

########################################
def benchmark(): # This lets you benchmark (time) the script's running duration 
	Window.WaitCursor(1) 
	t = sys.time() 
	debug(60,'%s began at %.0f' %(__script__,sys.time()))

	# Run the function on the active scene
	in_editmode = Window.EditMode()
	if in_editmode: Window.EditMode(0)

	main() 

	if in_editmode: Window.EditMode(1)
	
	# Timing the script is a good way to be aware on any speed hits when scripting 
	debug(0,'%s Script finished in %.2f seconds' % (__script__,sys.time()-t) )
	Window.WaitCursor(0) 
	return

########################################
# This lets you can import the script without running it 
if __name__ == '__main__': 
	debug(0, "------------------------------------")
	debug(0, "%s %s Script begins with mode=%i debug=%i batch=%s" % (__script__,__version__,MODE,DEBUG,BATCH))
	benchmark()
