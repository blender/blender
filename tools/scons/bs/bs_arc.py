# archive targets ('scons release')
# and extra functions
#
# Mac OS: appit
# Unices: zipit -> .tar.gz
# Windows: zipit -> .zip
# 

import os
import sys
import string
import bs_dirs

import bs_globals

def add2arc(arc, file):
	"""
	Add file to arc. For win32 arc is a Zipfile, for unices it's a Tarfile
	"""
	if sys.platform == 'win32':
		arc.write(file)
	else:
		arc.add(file)

def appit(target, source, env):
	if sys.platform == 'darwin':
		import shutil
		import commands
		import os.path
						
		target = 'blender' 
		sourceinfo = "source/darwin/%s.app/Contents/Info.plist"%target
		targetinfo = "%s.app/Contents/Info.plist"%target

		cmd = '%s.app'%target
		if os.path.isdir(cmd):
			shutil.rmtree('%s.app'%target)
		shutil.copytree("source/darwin/%s.app"%target, '%s.app'%target)
		cmd = "cat %s | sed s/VERSION/`cat release/VERSION`/ | sed s/DATE/`date +'%%Y-%%b-%%d'`/ > %s"%(sourceinfo,targetinfo)
		commands.getoutput(cmd)
		cmd = 'cp %s %s.app/Contents/MacOS/%s'%(target, target, target)
		commands.getoutput(cmd)
		if  bs_globals.user_options_dict['BUILD_BINARY'] == 'debug':
			print "building debug"
		else :
			cmd = 'strip -u -r %s.app/Contents/MacOS/%s'%(target, target)
			commands.getoutput(cmd)
		cmd = '%s.app/Contents/Resources/'%target
		shutil.copy('bin/.blender/.bfont.ttf', cmd)
		shutil.copy('bin/.blender/.Blanguages', cmd)
		cmd = 'cp -R bin/.blender/locale %s.app/Contents/Resources/'%target
		commands.getoutput(cmd)	
		cmd = 'mkdir %s.app/Contents/MacOS/.blender'%target
		commands.getoutput(cmd)
		cmd = 'cp -R release/bpydata %s.app/Contents/MacOS/.blender'%target
		commands.getoutput(cmd)
		cmd = 'cp -R release/scripts %s.app/Contents/MacOS/.blender/'%target
		commands.getoutput(cmd)
		cmd = 'cp -R release/plugins %s.app/Contents/Resources/'%target 
		commands.getoutput(cmd)
		cmd = 'chmod +x  %s.app/Contents/MacOS/%s'%(target, target)
		commands.getoutput(cmd)
		cmd = 'find %s.app -name CVS -prune -exec rm -rf {} \;'%target
		commands.getoutput(cmd)
		cmd = 'find %s.app -name .DS_Store -exec rm -rf {} \;'%target
		commands.getoutput(cmd)
		
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			target = 'blenderplayer' 
			sourceinfo = "source/darwin/%s.app/Contents/Info.plist"%target
			targetinfo = "%s.app/Contents/Info.plist"%target

			cmd = '%s.app'%target
			if os.path.isdir(cmd):
				shutil.rmtree('%s.app'%target)
			shutil.copytree("source/darwin/%s.app"%target, '%s.app'%target)
			cmd = "cat %s | sed s/VERSION/`cat release/VERSION`/ | sed s/DATE/`date +'%%Y-%%b-%%d'`/ > %s"%(sourceinfo,targetinfo)
			commands.getoutput(cmd)
			cmd = 'cp %s %s.app/Contents/MacOS/%s'%(target, target, target)
			commands.getoutput(cmd)
			if  bs_globals.user_options_dict['BUILD_BINARY'] == 'debug':
				print "building debug player"
			else :
				cmd = 'strip -u -r %s.app/Contents/MacOS/%s'%(target, target)
				commands.getoutput(cmd)
			cmd = '%s.app/Contents/Resources/'%target
			shutil.copy('bin/.blender/.bfont.ttf', cmd)
			shutil.copy('bin/.blender/.Blanguages', cmd)
			cmd = 'cp -R bin/.blender/locale %s.app/Contents/Resources/'%target
			commands.getoutput(cmd)
			cmd = 'cp -R release/bpydata %s.app/Contents/MacOS/.blender'%target
			commands.getoutput(cmd)
			cmd = 'cp -R release/scripts %s.app/Contents/MacOS/.blender/'%target
			commands.getoutput(cmd)
			cmd = 'cp -R release/plugins %s.app/Contents/Resources/'%target 
			commands.getoutput(cmd)
			cmd = 'chmod +x  %s.app/Contents/MacOS/%s'%(target, target)
			commands.getoutput(cmd)
			cmd = 'find %s.app -name CVS -prune -exec rm -rf {} \;'%target
			commands.getoutput(cmd)
			cmd = 'find %s.app -name .DS_Store -exec rm -rf {} \;'%target
			commands.getoutput(cmd)
		
	else:
		print "This target is for the Os X platform only"

def zipit(env, target, source):
	try:
		if sys.platform == 'win32':
			import zipfile
		else:
			import tarfile
	except:
		if sys.platform == 'win32':
			print "no zipfile module found"
		else:
			print "no tarfile module found"
			print "make sure you use python 2.3"
		print
		return
	
	import shutil
	import glob
	import time
	
	startdir = os.getcwd()
	pf=""
	zipext = ""
	zipname = ""
	
	today = time.strftime("%Y%m%d", time.gmtime()) # get time in the form 20040714
	
	if bs_dirs.preparedist()==0:
		print "check output for error"
		return
	
	if sys.platform == 'win32':
		zipext += ".zip"
		pf = "windows"
	elif sys.platform == 'linux2' or sys.platform == 'linux-i386':
		zipext += ".tar.gz"
		pf = "linux"
	elif sys.platform == 'freebsd4':
		zipext += ".tar.gz"
		pf = "freebsd4"
	elif sys.platform == 'freebsd5':
		zipext += ".tar.gz"
		pf = "freebsd5"
	elif sys.platform == 'cygwin':
		zipext += ".tar.gz"
		pf = "cygwin"
	
	if bs_globals.user_options_dict['BUILD_BINARY'] == 'release':
		blendname = "blender-" + bs_globals.version + "-" + bs_globals.config_guess
	else:
		blendname = "bf_blender_" + pf + "_" + today
	
	zipname = blendname + zipext

	if os.path.isdir(blendname):
		shutil.rmtree(blendname)
	shutil.move(startdir + os.sep + "dist", blendname)

	print
	if sys.platform == 'win32':
		print "Create the zip!"
	else:
		print "Create the tarball!"
	print
	
	if sys.platform == 'win32':
		thezip = zipfile.ZipFile(zipname, 'w', zipfile.ZIP_DEFLATED)
	else:
		thezip = tarfile.open(zipname, 'w:gz')
	
	for root, dirs, files in os.walk(blendname, topdown=False):
		for name in files:
			if name in [zipname]:
				print "skipping self"
			else:
				file = root + "/" + name
				print "adding: " + file
				add2arc(thezip, file)
	
	thezip.close()
	
	os.chdir(startdir)
	shutil.move(blendname, startdir + os.sep + "dist")
	
	if bs_dirs.finalisedist(zipname)==0:
		print "encountered an error in finalisedist"
		print
		return
	
	print
	print "Blender has been successfully packaged"
	print "You can find the file %s in the root source directory"%zipname
	print

def printadd(env, target, source):
	"""
	Print warning message if platform hasn't been added to zipit() yet
	"""
	
	print
	print "############"
	print 
	print "Make sure zipit() works for your platform:"
	print "  - binaries to copy (naming?)"
	print "  - possible libraries?"
	print "  - archive format?"
	print
	print "/Nathan Letwory (jesterKing)"
	print

def BlenderRelease(target):
	"""
	Make a Release package (tarball, zip, bundle).
	
	target = Name of package to make (string)
	eg: BlenderRelease('blender')
	"""
	
	if sys.platform == 'darwin':
		app_env = bs_globals.init_env.Copy()
		Mappit = app_env.Command('appit', bs_globals.appname, appit)
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			app_env.Depends(Mappit, bs_globals.playername)
		app_env.Alias("release", Mappit)
	elif sys.platform in ['win32', 'linux2', 'linux-i386', 'freebsd4', 'freebsd5','cygwin']:
		release_env = bs_globals.init_env.Copy()
		releaseit = release_env.Command('blenderrelease', bs_globals.appname, zipit)
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			release_env.Depends(releaseit, bs_globals.playername)
		release_env.Alias("release", releaseit)
	else:
		release_env = bs_globals.init_env.Copy()
		releaseit = release_env.Command('blender.tar.gz', bs_globals.appname, printadd)
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			release_env.Depends(releaseit, bs_globals.playername)
		release_env.Alias("release", releaseit)
