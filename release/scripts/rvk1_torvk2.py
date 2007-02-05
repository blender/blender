#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Rvk1 to Rvk2'
Blender: 234
Group: 'Mesh'
Tip: 'Copy deform data (not surf. subdiv) of active obj to rvk of the 2nd selected obj'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rvk1versrvk2.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "2007/02"

__bpydoc__ = """\
"RVK1 to RVK2" copies deform data (except EDGESPLIT,DECIMATE,SUBSURF,BOOLEAN, 
BUILD,MIRROR,ARRAY) of the active object to the RVK (relative vertex key) of
the other selected object.

It is presupposed that the second mesh object is built exactly like the first
one. In fact, it is better to use a true copy with at least one basic shape
key.

The new version of this scrit (Blender 2.43) manages the modifier changes.
There are a lot of modifiers but only the ones which just deforms the shape
can be used : LATTICE, CURVE, WAVE, ARMATURE. You can unset these modifiers
from the script. 

Usage:

Select the object that will receive the rvk info, then select the deformed
object, enter Edit Mode and run this script from the "Mesh->Scripts" menu of
the 3d View.  If the active object has subsurf turned on and nonzero subdiv
level, the script will ask if it should change that.  Before copying data to
the rvk it will also ask whether it should replace or add a new vertex group.

 
"""

#----------------------------------------------
# jm soler (c) 04/02/2007 : 'Rvk1toRvk2'  release under blender artistic licence
#----------------------------------------------
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html 
#----------------------------------------------------
# Copy the rvk (1, or armature, lattice, or
# any mesh deformation except surface
# sbdivision) of the active object to rvk (2) of
# the second selected object. Create rvk or modify
# absolute key if needed.
#----------------------------------------------
# official Page :
# http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rvk1versrvk2.htm
# download the script :
# http://jmsoler.free.fr/util/blenderfile/py/rvk1_torvk2.py
# Communicate upon problems or errors:
# http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender 
#----------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/util/blenderfile/py/rvk1_torvk2.py
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
#---------------------------------------------

import Blender
from Blender import NMesh,Draw,Object,Modifier

def Value(t):
	exec "t=Modifier.Types.%s"%t
 	return t

def rvk2rvk():  
	POSSMOD_list=['EDGESPLIT',
	              'DECIMATE',
		            'SUBSURF', 
             	  'BOOLEAN', 
	              'BUILD',
		            'MIRROR', 
			          'ARRAY']
			
	AUTHMOD_list=['LATTICE',
	              'CURVE', 
	              'WAVE',
	              'ARMATURE']
	
	MODIFIERS=0
	BMOD=[['Possible Modifiers'],
        ['Allowed Modifiers']]

	#	=================================================================
	# at leat 2 objects ===============================================
	#	=================================================================
	if len(Object.GetSelected())>1 :
			RVK1=Object.GetSelected()[0]
			RVK2=Object.GetSelected()[1]
			# =============================================================
			# must be 2 meshes ============================================
			# =============================================================
			if RVK1.getType()=='Mesh' and RVK2.getType()=='Mesh': 
				FRAME=Blender.Get('curframe') 
				DATA2=RVK2.getData()
				print DATA2.getKey()
				# ============================================================
				# at least the second must have a shape key ==================
				# ============================================================
				if DATA2.getKey():
							# ======================================================
							# in case of modifiers use =============================
							# ======================================================
							if RVK1.modifiers:
									MODIFIERS=1
									POSSMOD=[Value(t) for t in POSSMOD_list]
									AUTHMOD=[Value(t) for t in AUTHMOD_list]
									print 'POSSMOD:',POSSMOD,'\nAUTHMOD:', AUTHMOD
									MODRVK1=RVK1.modifiers
									block = []
									
									# ===================================================
									# ===  Bloc Menu Modifiers ===1 doc =================
									# ===================================================
									m=0
									for mod in  MODRVK1: 
										print mod.type
										if mod.type in POSSMOD:
											BMOD[0].append([Draw.Create(0),mod.type,
																					m,
																					POSSMOD_list[POSSMOD.index(mod.type)],
											                    mod[Modifier.Settings.RENDER]==1,
											                    mod[Modifier.Settings.EDITMODE]==1
											  									])
										elif mod.type in AUTHMOD:
											BMOD[1].append([Draw.Create(1),
											               mod.type,
											                    m,
																					AUTHMOD_list[AUTHMOD.index(mod.type)],
											                    mod[Modifier.Settings.RENDER]==1,
											                    mod[Modifier.Settings.EDITMODE]==1
											                    ])
										m+=1
									
									# ===================================================
									# ===  Bloc Menu Modifiers ===2 display =============
									# ===================================================
									block.append(BMOD[1][0])
									for	B in BMOD[1][1:]:
										block.append((B[3],B[0],""))
									
									block.append(BMOD[0][0])
									block.append("not alredy implemented")
									block.append("in this script.")
									for	B in BMOD[0][1:]:
										block.append((B[3],B[0],""))
									retval = Blender.Draw.PupBlock("MESH 2 RVK", block)
									
									# ===================================================
									# ===  unset Modifiers  =============================
									# ===================================================	
									for	B in BMOD[0][1:]:
										print B[2]
										MODRVK1[B[2]][Modifier.Settings.RENDER]=0
									for	B in BMOD[1]:
										if not B[1]:
											MODRVK1[B[2]][Modifier.Settings.RENDER]=0
																	
									# ===================================================
									# ===  update Modifiers =============================
									# ===================================================
									RVK1.makeDisplayList()									
									
							# =======================================================
							# ===  get deformed mesh ================================
							# =======================================================
							RVK1NAME=Object.GetSelected()[0].getName()
							meshrvk1=NMesh.GetRawFromObject(RVK1NAME)  
							print len(meshrvk1.verts)
							
							# =======================================================
							# ===  get normal mesh for vertex group =================
							# =======================================================
							DATA1=RVK1.getData()
							
							# =======================================================
							# ===  get destination mesh  ============================
							# =======================================================
							DATA2=RVK2.getData()
							print len(meshrvk1.verts)
							print len(DATA2.verts)							
							# ========================================================
							# ===== is there the same number of vertices =============
							# ========================================================
							if len(meshrvk1.verts)==len(DATA2.verts): 
								name = "Do you want to replace or add vertex groups ? %t| YES %x1| NO ? %x2 " 
								result = Draw.PupMenu(name)
								if result==1:       
									# =====================================================
									# ===== Do we save vertex groups ?  ===================
									# =====================================================
									GROUPNAME2=DATA2.getVertGroupNames() 
									if len(GROUPNAME2)!=0:
										for GROUP2 in GROUPNAME2:
											DATA2.removeVertGroup(GROUP2)
									GROUPNAME1=DATA1.getVertGroupNames()
									if len(GROUPNAME1)!=0:
										for GROUP1 in GROUPNAME1:
											DATA2.addVertGroup(GROUP1)
											DATA2.assignVertsToGroup(GROUP1,DATA1.getVertsFromGroup(GROUP1),1.0,'replace')
								# ========================================================
								# ===== now copy the vertices coords =====================
								# ========================================================
								for v in meshrvk1.verts:
									i= meshrvk1.verts.index(v) 
									v1=DATA2.verts[i]
									for n in [0,1,2]:
										v1.co[n]=v.co[n]
								DATA2.update() 
								DATA2.insertKey(FRAME,'relative')
								DATA2.update()
								RVK2.makeDisplayList()
								if MODIFIERS:
									# ===================================================
									# ===  unset Modifiers  =============================
									# ===================================================
									for	B in BMOD[0][1:]:
										MODRVK1[B[2]][Modifier.Settings.RENDER]|=B[-2]
									for	B in BMOD[1]:
										if not B[1]:
											MODRVK1[B[2]][Modifier.Settings.RENDER]|=B[-2]
							else:
								name = "Meshes Objects must the same number of vertices %t| Ok. %x1" 
								result = Draw.PupMenu(name)
								return
				else:
							name = "Second Object must have  at least a shape key %t| Ok. %x1" 
							result = Draw.PupMenu(name)
							return
			else:	
				name = "Object must be Meshes %t| Ok. %x1" 
				result = Draw.PupMenu(name)
				return 
	else :
		name = "At least 2 Meshes as to be selected %t| Ok. %x1" 
		result = Draw.PupMenu(name)
		return
	Blender.Redraw()  
EDITMODE=Blender.Window.EditMode()
Blender.Window.EditMode(0)
rvk2rvk()
Blender.Window.EditMode(EDITMODE)