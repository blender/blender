# nsis target
#
# this file has the NSIS related functions

import os
import string
import sys

import bs_dirs
import bs_globals

def donsis(env, target, source):
	"""
	Create a Windows installer with NSIS
	"""
	print
	print "Creating the Windows installer"
	print
	
	startdir = os.getcwd()
	
	if bs_dirs.preparedist()==0:
		print "check output for error"
		return
	
	os.chdir("release/windows/installer")
	
	nsis = open("00.sconsblender.nsi", 'r')
	nsis_cnt = str(nsis.read())
	nsis.close()
	
	# do root
	rootlist = []
	rootdir = os.listdir(startdir + "\\dist")
	for rootitem in rootdir:
		if os.path.isdir(startdir + "\\dist\\" + rootitem) == 0:
			rootlist.append("File " + startdir + "\\dist\\" + rootitem)
	rootstring = string.join(rootlist, "\n  ")
	rootstring += "\n\n"
	nsis_cnt = string.replace(nsis_cnt, "[ROOTDIRCONTS]", rootstring)
	
	# do delete items
	delrootlist = []
	for rootitem in rootdir:
		if os.path.isdir(startdir + "\\dist\\" + rootitem) == 0:
			delrootlist.append("Delete $INSTDIR\\" + rootitem)
	delrootstring = string.join(delrootlist, "\n ")
	delrootstring += "\n"
	nsis_cnt = string.replace(nsis_cnt, "[DELROOTDIRCONTS]", delrootstring)
	
	# do scripts
	scriptlist = []
	scriptdir = os.listdir(startdir + "\\dist\\.blender\\scripts")
	for scriptitem in scriptdir:
		if os.path.isdir(startdir + "\\dist\\.blender\\scripts\\" + scriptitem) == 0:
			scriptlist.append("File " + startdir + "\\dist\\.blender\\scripts\\" + scriptitem)
	scriptstring = string.join(scriptlist, "\n  ")
	scriptstring += "\n\n"
	nsis_cnt = string.replace(nsis_cnt, "[SCRIPTCONTS]", scriptstring)
	
	# do bpycontents
	bpydatalist = []
	bpydatadir = os.listdir(startdir + "\\dist\\.blender\\bpydata")
	for bpydataitem in bpydatadir:
		if os.path.isdir(startdir + "\\dist\\.blender\\bpydata\\" + bpydataitem) == 0:
			bpydatalist.append("File " + startdir + "\\dist\\.blender\\bpydata\\" + bpydataitem)
	bpydatastring = string.join(bpydatalist, "\n  ")
	bpydatastring += "\n\n"
	nsis_cnt = string.replace(nsis_cnt, "[BPYCONTS]", bpydatastring)
	
	# do dotblender
	dotblendlist = []
	dotblenddir = os.listdir(startdir+"\\dist\\.blender")
	for dotblenditem in dotblenddir:
		if os.path.isdir(startdir + "\\dist\\.blender\\" + dotblenditem) == 0:
			dotblendlist.append("File " + startdir + "\\dist\\.blender\\" + dotblenditem)
	dotblendstring = string.join(dotblendlist, "\n  ")
	dotblendstring += "\n\n"
	nsis_cnt = string.replace(nsis_cnt, "[DOTBLENDERCONTS]", dotblendstring)
	
	# do language files
	langlist = []
	langfiles = []
	langdir = os.listdir(startdir + "\\dist\\.blender\\locale")
	for langitem in langdir:
		if os.path.isdir(startdir + "\\dist\\.blender\\locale\\" + langitem) == 1:
			langfiles.append("SetOutPath $BLENDERHOME\\.blender\\locale\\" + langitem + "\\LC_MESSAGES")
			langfiles.append("File " + startdir + "\\dist\\.blender\\locale\\" + langitem + "\\LC_MESSAGES\\blender.mo")
	langstring = string.join(langfiles, "\n  ")
	langstring += "\n\n"
	nsis_cnt = string.replace(nsis_cnt, "[LANGUAGECONTS]", langstring)
	
	# var replacements
	nsis_cnt = string.replace(nsis_cnt, "DISTDIR", startdir + "\\dist")
	nsis_cnt = string.replace(nsis_cnt, "SHORTVER", bs_globals.shortversion)
	nsis_cnt = string.replace(nsis_cnt, "VERSION", bs_globals.version)
	
	new_nsis = open("00.blender_tmp.nsi", 'w')
	new_nsis.write(nsis_cnt)
	new_nsis.close()
	
	sys.stdout = os.popen("makensis 00.blender_tmp.nsi", 'w')
	
	os.chdir(startdir)
	
def BlenderNSIS(target):
	"""
	Entry for creating Windows installer
	"""
	if sys.platform == 'win32':
		inst_env = bs_globals.init_env.Copy()
		nsis_inst = inst_env.Command('nsisinstaller', 'blender$PROGSUFFIX', donsis)
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			inst_env.Depends(nsis_inst, 'blenderplayer$PROGSUFFIX')
		inst_env.Alias("wininst", nsis_inst)