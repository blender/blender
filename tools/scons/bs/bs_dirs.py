# functions used for dir handling / preperation / cleaning

import os
import string
import sys
import bs_globals

def cleanCVS():
	"""
	walks the dist dir and removes all CVS dirs
	"""
	
	try:
		import shutil
	except:
		print "no shutil available"
		print "make sure you use python 2.3"
		print
		return 0
	
	startdir = os.getcwd()
	
	for root, dirs, files in os.walk("dist", topdown=False):
		for name in dirs:
			if name in ['CVS']:
				if os.path.isdir(root + "/" + name):
					shutil.rmtree(root + "/" + name)
	
	os.chdir(startdir)
	
	return 1

def preparedist():
	"""
	Prepare a directory for creating either archives or the installer
	"""
	
	try:
		import shutil
		import time
		import stat
	except:
		print "no shutil available"
		print "make sure you use python 2.3"
		print
		return 0
	
	startdir = os.getcwd()
	
	if os.path.isdir("dist") == 0:
		os.makedirs("dist")
	else:
		shutil.rmtree("dist") # make sure we don't get old cruft
		os.makedirs("dist")
	
	# first copy binaries
	
	if sys.platform == 'win32' or sys.platform == 'cygwin':
		shutil.copy("blender.exe", "dist/blender.exe")
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			shutil.copy("blenderplayer.exe", "dist/blenderplayer.exe")
		shutil.copy("../lib/windows/python/lib/python23.dll", "dist/python23.dll")
		shutil.copy("../lib/windows/sdl/lib/SDL.dll", "dist/SDL.dll")
		shutil.copy("../lib/windows/gettext/lib/gnu_gettext.dll", "dist/gnu_gettext.dll")
	elif sys.platform in ['linux2', 'linux-i386', 'freebsd4', 'freebsd5']:
		shutil.copy("blender", "dist/blender")
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			shutil.copy("blenderplayer", "dist/blenderplayer")
	else:
		print "update preparedist() for your platform!"
		return 0
	
	# now copy .blender and necessary extras for it
	if os.path.isdir("dist/.blender"):
		shutil.rmtree("dist/.blender")
	os.chdir("bin")
	shutil.copytree(".blender/", "../dist/.blender")
	os.chdir(startdir)
	if os.path.isdir("dist/.blender/scripts"):
		shutil.rmtree("dist/.blender/scripts")
	if os.path.isdir("dist/.blender/bpydata"):
		shutil.rmtree("dist/.blender/bpydata")
		
	os.makedirs("dist/.blender/bpydata")
	shutil.copy("release/bpydata/readme.txt", "dist/.blender/bpydata/readme.txt")
	shutil.copy("release/bpydata/KUlang.txt", "dist/.blender/bpydata/KUlang.txt")
	
	os.chdir("release")
	shutil.copytree("scripts/", "../dist/.blender/scripts")
	
	# finally copy auxiliaries (readme, license, etc.)
	if sys.platform == 'win32':
		shutil.copy("windows/extra/Help.url", "../dist/Help.url")
		shutil.copy("windows/extra/Python23.zip", "../dist/Python23.zip")
		shutil.copy("windows/extra/zlib.pyd", "../dist/zlib.pyd")
	shutil.copy("text/copyright.txt", "../dist/copyright.txt")
	shutil.copy("text/blender.html", "../dist/blender.html")
	shutil.copy("text/GPL-license.txt", "../dist/GPL-license.txt")
	shutil.copy("text/Python-license.txt", "../dist/Python-license.txt")
	
	reltext = "release_" + string.join(bs_globals.version.split("."), '') + ".txt"
	shutil.copy("text/" + reltext, "../dist/" + reltext)
	
	os.chdir(startdir)
	
	if cleanCVS()==0:
		return 0
	return 1

def finalisedist(zipname):
	"""
	Fetch the package created and remove temp dir
	"""
	
	try:
		import shutil
	except:
		print "no shutil available"
		print "make sure you use python 2.3"
		print
		return 0
	
	#shutil.copy("dist/" + zipname, zipname)
	#shutil.rmtree("dist")
	
	return 1