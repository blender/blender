# bincopy target
import sys
import os
import shutil
import bs_globals

def docopyit(env, target, source):
	"""
	Copy the blender binaries to a specified location
	"""
	if bs_globals.docopy==0 or bs_globals.copyto=='':
		print "The bincopy target has been activated with corrupt data"
		sys.exit()
		
	blender = 'blender'
	blenderplayer = 'blenderplayer'
	
	# make sure bs_globals.copyto exists
	if os.path.isdir(bs_globals.copyto) == 0:
		os.makedirs(bs_globals.copyto)
		
	if sys.platform in ['win32', 'cygwin']:
		blender = 'blender.exe'
		blenderplayer = 'blenderplayer.exe'
	
	shutil.copy(blender, bs_globals.copyto + os.sep + blender)
	if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
		shutil.copy(blenderplayer, bs_globals.copyto + os.sep + blenderplayer)

def BlenderCopy(target):
	#~ if sys.platform == 'darwin':
		#~ copy_env = bs_globals.init_env.Copy()
		#~ Mappit = app_env.Command('appit', bs_globals.appname, appit)
		#~ if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			#~ app_env.Depends(Mappit, bs_globals.playername)
		#~ app_env.Alias("release", Mappit)
	if sys.platform in ['win32', 'linux2', 'linux-i386', 'freebsd4', 'freebsd5','cygwin']:
		copy_env = bs_globals.init_env.Copy()
		copyit = copy_env.Command('blendercopy', bs_globals.appname, docopyit)
		if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
			copy_env.Depends(copyit, bs_globals.playername)
		copy_env.Alias("bincopy", copyit)
	else:
		print "Check the scons implementation for bincopy, copydo if everything is setup correctly for your platform"

