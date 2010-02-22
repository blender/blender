#!/usr/bin/env python
# $Id$
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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Nathan Letwory.
#
# ***** END GPL LICENSE BLOCK *****
#
# Main entry-point for the SCons building system
# Set up some custom actions and target/argument handling
# Then read all SConscripts and build

import platform as pltfrm

# Need a better way to do this. Automagical maybe is not the best thing, maybe it is.
if pltfrm.architecture()[0] == '64bit':
	bitness = 64
else:
	bitness = 32

import sys
import os
import os.path
import string
import shutil
import glob
import re
from tempfile import mkdtemp

import tools.Blender
import tools.btools
import tools.bcolors

EnsureSConsVersion(1,0,0)

BlenderEnvironment = tools.Blender.BlenderEnvironment
btools = tools.btools
B = tools.Blender

### globals ###
platform = sys.platform
quickie = None
quickdebug = None

##### BEGIN SETUP #####

B.possible_types = ['core', 'player', 'intern', 'extern']

B.binarykind = ['blender' , 'blenderplayer']
##################################
# target and argument validation #
##################################
# XX cheating for BF_FANCY, we check for BF_FANCY before args are validated
use_color = ARGUMENTS.get('BF_FANCY', '1')
if platform=='win32':
	use_color = None

if not use_color=='1':
	B.bc.disable()
	
 #on defaut white Os X terminal, some colors are totally unlegible
if platform=='darwin':
	B.bc.OKGREEN = '\033[34m'
	B.bc.WARNING = '\033[36m'

# arguments
print B.bc.HEADER+'Command-line arguments'+B.bc.ENDC
B.arguments = btools.validate_arguments(ARGUMENTS, B.bc)
btools.print_arguments(B.arguments, B.bc)

# targets
print B.bc.HEADER+'Command-line targets'+B.bc.ENDC
B.targets = btools.validate_targets(COMMAND_LINE_TARGETS, B.bc)
btools.print_targets(B.targets, B.bc)

##########################
# setting up environment #
##########################

# handling cmd line arguments & config file

# first check cmdline for toolset and we create env to work on
quickie = B.arguments.get('BF_QUICK', None)
quickdebug = B.arguments.get('BF_QUICKDEBUG', None)

if quickdebug:
	B.quickdebug=string.split(quickdebug, ',')
else:
	B.quickdebug=[]

if quickie:
	B.quickie=string.split(quickie,',')
else:
	B.quickie=[]
	
toolset = B.arguments.get('BF_TOOLSET', None)
if toolset:
	print "Using " + toolset
	if toolset=='mstoolkit':
		env = BlenderEnvironment(ENV = os.environ)
		env.Tool('mstoolkit', ['tools'])
	else:
		env = BlenderEnvironment(tools=[toolset], ENV = os.environ)
		# xxx commented out, as was supressing warnings under mingw..
		#if env:
		#	btools.SetupSpawn(env)
else:
	if bitness==64 and platform=='win32':
		env = BlenderEnvironment(ENV = os.environ, MSVS_ARCH='amd64')
	else:
		env = BlenderEnvironment(ENV = os.environ)

if not env:
	print "Could not create a build environment"
	Exit()


cc = B.arguments.get('CC', None)
cxx = B.arguments.get('CXX', None)
if cc:
	env['CC'] = cc
if cxx:
	env['CXX'] = cxx

if env['CC'] in ['cl', 'cl.exe'] and sys.platform=='win32':
	if bitness == 64:
		platform = 'win64-vc'
	else:
		platform = 'win32-vc'
elif env['CC'] in ['gcc'] and sys.platform=='win32':
	platform = 'win32-mingw'

env.SConscriptChdir(0)

crossbuild = B.arguments.get('BF_CROSS', None)
if crossbuild and platform not in ('win32-vc', 'win64-vc'):
	platform = 'linuxcross'

env['OURPLATFORM'] = platform

configfile = 'config'+os.sep+platform+'-config.py'

if os.path.exists(configfile):
	print B.bc.OKGREEN + "Using config file: " + B.bc.ENDC + configfile
else:
	print B.bc.FAIL + configfile + " doesn't exist" + B.bc.ENDC

if crossbuild and env['PLATFORM'] != 'win32':
	print B.bc.HEADER+"Preparing for crossbuild"+B.bc.ENDC
	env.Tool('crossmingw', ['tools'])
	# todo: determine proper libs/includes etc.
	# Needed for gui programs, console programs should do without it

	# Now we don't need this option to have console window
	# env.Append(LINKFLAGS=['-mwindows'])

userconfig = B.arguments.get('BF_CONFIG', 'user-config.py')
# first read platform config. B.arguments will override
optfiles = [configfile]
if os.path.exists(userconfig):
	print B.bc.OKGREEN + "Using user-config file: " + B.bc.ENDC + userconfig
	optfiles += [userconfig]
else:
	print B.bc.WARNING + userconfig + " not found, no user overrides" + B.bc.ENDC

opts = btools.read_opts(optfiles, B.arguments)
opts.Update(env)

if not env['BF_FANCY']:
	B.bc.disable()


# remove install dir so old and new files are not mixed.
# NOTE: only do the scripts directory for now, otherwise is too disruptive for developers
# TODO: perhaps we need an option (off by default) to not do this altogether...
if not env['WITHOUT_BF_INSTALL'] and not env['WITHOUT_BF_OVERWRITE_INSTALL']:
	scriptsDir = env['BF_INSTALLDIR'] + os.sep + '.blender' + os.sep + 'scripts'
	if os.path.isdir(scriptsDir):
		print B.bc.OKGREEN + "Clearing installation directory%s: %s" % (B.bc.ENDC, os.path.abspath(scriptsDir))
		shutil.rmtree(scriptsDir)


SetOption('num_jobs', int(env['BF_NUMJOBS']))
print B.bc.OKGREEN + "Build with parallel jobs%s: %s" % (B.bc.ENDC, GetOption('num_jobs'))

# BLENDERPATH is a unix only option to enable typical style paths this is
# spesifically a data-dir, which is used a lot but cant replace BF_INSTALLDIR
# because the blender binary is installed in $BF_INSTALLDIR/bin/blender

if env['WITH_BF_FHS']:
	BLENDERPATH = os.path.join(env['BF_INSTALLDIR'], 'share', 'blender', env['BF_VERSION'])
else:
	BLENDERPATH = env['BF_INSTALLDIR']

if env['WITH_BF_OPENMP'] == 1:
		if env['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
				env['CCFLAGS'].append('/openmp')
				env['CPPFLAGS'].append('/openmp')
				env['CXXFLAGS'].append('/openmp')
		else:
			if env['CC'].endswith('icc'): # to be able to handle CC=/opt/bla/icc case
				env.Append(LINKFLAGS=['-openmp', '-static-intel'])
				env['CCFLAGS'].append('-openmp')
				env['CPPFLAGS'].append('-openmp')
				env['CXXFLAGS'].append('-openmp')
			else:
				env.Append(CCFLAGS=['-fopenmp']) 
				env.Append(CPPFLAGS=['-fopenmp'])
				env.Append(CXXFLAGS=['-fopenmp'])

if env['WITH_GHOST_COCOA'] == True:
	env.Append(CFLAGS=['-DGHOST_COCOA']) 
	env.Append(CXXFLAGS=['-DGHOST_COCOA'])
	env.Append(CPPFLAGS=['-DGHOST_COCOA'])
    
if env['USE_QTKIT'] == True:
	env.Append(CFLAGS=['-DUSE_QTKIT']) 
	env.Append(CXXFLAGS=['-DUSE_QTKIT'])
	env.Append(CPPFLAGS=['-DUSE_QTKIT'])

#check for additional debug libnames

if env.has_key('BF_DEBUG_LIBS'):
	B.quickdebug += env['BF_DEBUG_LIBS']

printdebug = B.arguments.get('BF_LISTDEBUG', 0)

if len(B.quickdebug) > 0 and printdebug != 0:
	print B.bc.OKGREEN + "Buildings these libs with debug symbols:" + B.bc.ENDC
	for l in B.quickdebug:
		print "\t" + l

# remove stdc++ from LLIBS if we are building a statc linked CXXFLAGS
if env['WITH_BF_STATICCXX']:
	if 'stdc++' in env['LLIBS']:
		env['LLIBS'].remove('stdc++')
	else:
		print '\tcould not remove stdc++ library from LLIBS, WITH_BF_STATICCXX may not work for your platform'

# check target for blenderplayer. Set WITH_BF_PLAYER if found on cmdline
if 'blenderplayer' in B.targets:
	env['WITH_BF_PLAYER'] = True

if 'blendernogame' in B.targets:
	env['WITH_BF_GAMEENGINE'] = False

if 'blenderlite' in B.targets:
	target_env_defs = {}
	target_env_defs['WITH_BF_GAMEENGINE'] = False
	target_env_defs['WITH_BF_OPENAL'] = False
	target_env_defs['WITH_BF_OPENEXR'] = False
	target_env_defs['WITH_BF_ICONV'] = False
	target_env_defs['WITH_BF_INTERNATIONAL'] = False
	target_env_defs['WITH_BF_OPENJPEG'] = False
	target_env_defs['WITH_BF_FFMPEG'] = False
	target_env_defs['WITH_BF_QUICKTIME'] = False
	target_env_defs['WITH_BF_REDCODE'] = False
	target_env_defs['WITH_BF_DDS'] = False
	target_env_defs['WITH_BF_ZLIB'] = False
	target_env_defs['WITH_BF_SDL'] = False
	target_env_defs['WITH_BF_JPEG'] = False
	target_env_defs['WITH_BF_PNG'] = False
	target_env_defs['WITH_BF_BULLET'] = False
	target_env_defs['WITH_BF_BINRELOC'] = False
	target_env_defs['BF_BUILDINFO'] = False
	target_env_defs['BF_NO_ELBEEM'] = True
	target_env_defs['WITH_BF_PYTHON'] = False
	
	# Merge blenderlite, let command line to override
	for k,v in target_env_defs.iteritems():
		if k not in B.arguments:
			env[k] = v

# disable elbeem (fluidsim) compilation?
if env['BF_NO_ELBEEM'] == 1:
	env['CPPFLAGS'].append('-DDISABLE_ELBEEM')
	env['CXXFLAGS'].append('-DDISABLE_ELBEEM')
	env['CCFLAGS'].append('-DDISABLE_ELBEEM')

if env['WITH_BF_SDL'] == False and env['OURPLATFORM'] in ('win32-vc', 'win32-ming', 'win64-vc'):
	env['PLATFORM_LINKFLAGS'].remove('/ENTRY:mainCRTStartup')
	env['PLATFORM_LINKFLAGS'].append('/ENTRY:main')

# lastly we check for root_build_dir ( we should not do before, otherwise we might do wrong builddir
B.root_build_dir = env['BF_BUILDDIR']
B.doc_build_dir = os.path.join(BLENDERPATH, 'doc')
if not B.root_build_dir[-1]==os.sep:
	B.root_build_dir += os.sep
if not B.doc_build_dir[-1]==os.sep:
	B.doc_build_dir += os.sep
	
# We do a shortcut for clean when no quicklist is given: just delete
# builddir without reading in SConscripts
do_clean = None
if 'clean' in B.targets:
	do_clean = True

if not quickie and do_clean:
	if os.path.exists(B.doc_build_dir):
		print B.bc.HEADER+'Cleaning doc dir...'+B.bc.ENDC
		dirs = os.listdir(B.doc_build_dir)
		for entry in dirs:
			if os.path.isdir(B.doc_build_dir + entry) == 1:
				print "clean dir %s"%(B.doc_build_dir+entry)
				shutil.rmtree(B.doc_build_dir+entry)
			else: # remove file
				print "remove file %s"%(B.doc_build_dir+entry)
				os.remove(B.root_build_dir+entry)
	if os.path.exists(B.root_build_dir):
		print B.bc.HEADER+'Cleaning build dir...'+B.bc.ENDC
		dirs = os.listdir(B.root_build_dir)
		for entry in dirs:
			if os.path.isdir(B.root_build_dir + entry) == 1:
				print "clean dir %s"%(B.root_build_dir+entry)
				shutil.rmtree(B.root_build_dir+entry)
			else: # remove file
				print "remove file %s"%(B.root_build_dir+entry)
				os.remove(B.root_build_dir+entry)
		for confile in ['extern/ffmpeg/config.mak', 'extern/x264/config.mak',
				'extern/xvidcore/build/generic/platform.inc', 'extern/ffmpeg/include']:
			if os.path.exists(confile):
				print "clean file %s"%confile
				if os.path.isdir(confile):
					for root, dirs, files in os.walk(confile):
						for name in files:
							os.remove(os.path.join(root, name))
				else:
					os.remove(confile)
		print B.bc.OKGREEN+'...done'+B.bc.ENDC
	else:
		print B.bc.HEADER+'Already Clean, nothing to do.'+B.bc.ENDC
	Exit()

if not os.path.isdir ( B.root_build_dir):
	os.makedirs ( B.root_build_dir )
	os.makedirs ( B.root_build_dir + 'source' )
	os.makedirs ( B.root_build_dir + 'intern' )
	os.makedirs ( B.root_build_dir + 'extern' )
	os.makedirs ( B.root_build_dir + 'lib' )
	os.makedirs ( B.root_build_dir + 'bin' )
if not os.path.isdir(B.doc_build_dir) and env['WITH_BF_DOCS']:
	os.makedirs ( B.doc_build_dir )

Help(opts.GenerateHelpText(env))

# default is new quieter output, but if you need to see the 
# commands, do 'scons BF_QUIET=0'
bf_quietoutput = B.arguments.get('BF_QUIET', '1')
if env['BF_QUIET']:
	B.set_quiet_output(env)
else:
	if toolset=='msvc':
		B.msvc_hack(env)

print B.bc.HEADER+'Building in: ' + B.bc.ENDC + os.path.abspath(B.root_build_dir)
env.SConsignFile(B.root_build_dir+'scons-signatures')
B.init_lib_dict()

##### END SETUP ##########

Export('env')

BuildDir(B.root_build_dir+'/intern', 'intern', duplicate=0)
SConscript(B.root_build_dir+'/intern/SConscript')
BuildDir(B.root_build_dir+'/extern', 'extern', duplicate=0)
SConscript(B.root_build_dir+'/extern/SConscript')
BuildDir(B.root_build_dir+'/source', 'source', duplicate=0)
SConscript(B.root_build_dir+'/source/SConscript')

# now that we have read all SConscripts, we know what
# libraries will be built. Create list of
# libraries to give as objects to linking phase
mainlist = []
for tp in B.possible_types:
	if not tp == 'player':
		mainlist += B.create_blender_liblist(env, tp)

if B.arguments.get('BF_PRIORITYLIST', '0')=='1':
	B.propose_priorities()

dobj = B.buildinfo(env, "dynamic") + B.resources
thestatlibs, thelibincs = B.setup_staticlibs(env)
thesyslibs = B.setup_syslibs(env)

if 'blender' in B.targets or not env['WITH_BF_NOBLENDER']:
	env.BlenderProg(B.root_build_dir, "blender", dobj + mainlist, [], thestatlibs + thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blender')
if env['WITH_BF_PLAYER']:
	playerlist = B.create_blender_liblist(env, 'player')
	playerlist += B.create_blender_liblist(env, 'intern')
	playerlist += B.create_blender_liblist(env, 'extern')
	env.BlenderProg(B.root_build_dir, "blenderplayer", dobj + playerlist, [], thestatlibs + thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blenderplayer')

##### Now define some targets


#------------ INSTALL

#-- binaries
blenderinstall = []
if  env['OURPLATFORM']=='darwin':
	for prg in B.program_list:
		bundle = '%s.app' % prg[0]
		bundledir = os.path.dirname(bundle)
		for dp, dn, df in os.walk(bundle):
			if '.svn' in dn:
				dn.remove('.svn')
			dir=env['BF_INSTALLDIR']+dp[len(bundledir):]
			source=[dp+os.sep+f for f in df]
			blenderinstall.append(env.Install(dir=dir,source=source))
else:
	if env['WITH_BF_FHS']:	dir= os.path.join(env['BF_INSTALLDIR'], 'bin')
	else:					dir= env['BF_INSTALLDIR']
	
	blenderinstall = env.Install(dir=dir, source=B.program_list)

#-- .blender
#- dont do .blender and scripts for darwin, it is already in the bundle
dotblendlist = []
dottargetlist = []
scriptinstall = []

if  env['OURPLATFORM']!='darwin':
		for dp, dn, df in os.walk('bin/.blender'):
			if '.svn' in dn:
				dn.remove('.svn')
			
			for f in df:
				if not env['WITH_BF_INTERNATIONAL']:
					if 'locale' in dp:
						continue
					if f == '.Blanguages':
						continue
				if not env['WITH_BF_FREETYPE']:
					if f.endswith('.ttf'):
						continue
				
				dotblendlist.append(os.path.join(dp, f))
				if env['WITH_BF_FHS']:	dir= os.path.join(*([BLENDERPATH] + dp.split(os.sep)[2:]))	# skip bin/.blender
				else:					dir= os.path.join(*([BLENDERPATH] + dp.split(os.sep)[1:]))	# skip bin
				
				# print dir+ os.sep + f
				print dir
				dottargetlist.append(dir + os.sep + f)
					

		dotblenderinstall = []
		for targetdir,srcfile in zip(dottargetlist, dotblendlist):
			td, tf = os.path.split(targetdir)
			dotblenderinstall.append(env.Install(dir=td, source=srcfile))
		
		if env['WITH_BF_PYTHON']:
			#-- .blender/scripts
			scriptpaths=['release/scripts']
			for scriptpath in scriptpaths:
				for dp, dn, df in os.walk(scriptpath):
					if '.svn' in dn:
						dn.remove('.svn')
					
					if env['WITH_BF_FHS']:		dir = BLENDERPATH
					else:						dir = os.path.join(env['BF_INSTALLDIR'], '.blender')				
					dir += os.sep + os.path.basename(scriptpath) + dp[len(scriptpath):]
					
					source=[os.path.join(dp, f) for f in df if f[-3:]!='pyc']
					scriptinstall.append(env.Install(dir=dir,source=source))

#-- icons
if env['OURPLATFORM']=='linux2':
	iconlist = []
	icontargetlist = []

	for tp, tn, tf in os.walk('release/freedesktop/icons'):
		if '.svn' in tn:
			tn.remove('.svn')
		for f in tf:
			iconlist.append(os.path.join(tp, f))
			icontargetlist.append( os.path.join(*([BLENDERPATH] + tp.split(os.sep)[2:] + [f])) )

	iconinstall = []
	for targetdir,srcfile in zip(icontargetlist, iconlist):
		td, tf = os.path.split(targetdir)
		iconinstall.append(env.Install(dir=td, source=srcfile))

# dlls for linuxcross
# TODO - add more libs, for now this lets blenderlite run
if env['OURPLATFORM']=='linuxcross':
	dir=env['BF_INSTALLDIR']
	source = []

	if env['WITH_BF_OPENMP']:
		source += ['../lib/windows/pthreads/lib/pthreadGC2.dll']

	scriptinstall.append(env.Install(dir=dir, source=source))

#-- plugins
pluglist = []
plugtargetlist = []
for tp, tn, tf in os.walk('release/plugins'):
	if '.svn' in tn:
		tn.remove('.svn')
	df = tp[8:] # remove 'release/'
	for f in tf:
		pluglist.append(os.path.join(tp, f))
		plugtargetlist.append( os.path.join(BLENDERPATH, df, f) )


# header files for plugins
pluglist.append('source/blender/blenpluginapi/documentation.h')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'documentation.h'))
pluglist.append('source/blender/blenpluginapi/externdef.h')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'externdef.h'))
pluglist.append('source/blender/blenpluginapi/floatpatch.h')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'floatpatch.h'))
pluglist.append('source/blender/blenpluginapi/iff.h')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'iff.h'))
pluglist.append('source/blender/blenpluginapi/plugin.h')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'plugin.h'))
pluglist.append('source/blender/blenpluginapi/util.h')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'util.h'))
pluglist.append('source/blender/blenpluginapi/plugin.DEF')
plugtargetlist.append(os.path.join(BLENDERPATH, 'plugins', 'include', 'plugin.def'))

plugininstall = []
for targetdir,srcfile in zip(plugtargetlist, pluglist):
	td, tf = os.path.split(targetdir)
	plugininstall.append(env.Install(dir=td, source=srcfile))

textlist = []
texttargetlist = []
for tp, tn, tf in os.walk('release/text'):
	if '.svn' in tn:
		tn.remove('.svn')
	for f in tf:
		textlist.append(tp+os.sep+f)

textinstall = env.Install(dir=BLENDERPATH, source=textlist)

if  env['OURPLATFORM']=='darwin':
		allinstall = [blenderinstall, plugininstall, textinstall]
elif env['OURPLATFORM']=='linux2':
		allinstall = [blenderinstall, dotblenderinstall, scriptinstall, plugininstall, textinstall, iconinstall]
else:
		allinstall = [blenderinstall, dotblenderinstall, scriptinstall, plugininstall, textinstall]

if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'win64-vc', 'linuxcross'):
	dllsources = []

	if not env['OURPLATFORM'] in ('win32-mingw', 'win64-vc', 'linuxcross'):
		# For MinGW and linuxcross static linking will be used
		dllsources += ['${LCGDIR}/gettext/lib/gnu_gettext.dll']

	#currently win64-vc doesn't appear to have libpng.dll
	if env['OURPLATFORM'] != 'win64-vc':
		dllsources += ['${BF_PNG_LIBPATH}/libpng.dll',
				'${BF_ZLIB_LIBPATH}/zlib.dll']

	dllsources += ['${BF_TIFF_LIBPATH}/${BF_TIFF_LIB}.dll']

	if env['OURPLATFORM'] != 'linuxcross':
		# pthreads library is already added
		dllsources += ['${BF_PTHREADS_LIBPATH}/${BF_PTHREADS_LIB}.dll']

	if env['WITH_BF_SDL']:
		if env['OURPLATFORM'] == 'win64-vc':
			pass # we link statically already to SDL on win64
		else:
			dllsources.append('${BF_SDL_LIBPATH}/SDL.dll')

	if env['WITH_BF_PYTHON']:
		if env['BF_DEBUG']:
			dllsources.append('${BF_PYTHON_LIBPATH}/${BF_PYTHON_DLL}_d.dll')
		else:
			dllsources.append('${BF_PYTHON_LIBPATH}/${BF_PYTHON_DLL}.dll')

	if env['WITH_BF_ICONV']:
		if env['OURPLATFORM'] == 'win64-vc':
			pass # we link statically to iconv on win64
		elif not env['OURPLATFORM'] in ('win32-mingw', 'linuxcross'):
			#gettext for MinGW and cross-compilation is compiled staticly
			dllsources += ['${BF_ICONV_LIBPATH}/iconv.dll']

	if env['WITH_BF_OPENAL']:
		dllsources.append('${LCGDIR}/openal/lib/OpenAL32.dll')
		dllsources.append('${LCGDIR}/openal/lib/wrap_oal.dll')

	if env['WITH_BF_SNDFILE']:
		dllsources.append('${LCGDIR}/sndfile/lib/libsndfile-1.dll')

	if env['WITH_BF_FFMPEG']:
		dllsources += ['${BF_FFMPEG_LIBPATH}/avcodec-52.dll',
					'${BF_FFMPEG_LIBPATH}/avformat-52.dll',
					'${BF_FFMPEG_LIBPATH}/avdevice-52.dll',
					'${BF_FFMPEG_LIBPATH}/avutil-50.dll',
					'${BF_FFMPEG_LIBPATH}/swscale-0.dll']

	if env['WITH_BF_JACK']:
		dllsources += ['${LCGDIR}/jack/lib/libjack.dll']
	windlls = env.Install(dir=env['BF_INSTALLDIR'], source = dllsources)
	allinstall += windlls

installtarget = env.Alias('install', allinstall)
bininstalltarget = env.Alias('install-bin', blenderinstall)

nsisaction = env.Action(btools.NSIS_Installer, btools.NSIS_print)
nsiscmd = env.Command('nsisinstaller', None, nsisaction)
nsisalias = env.Alias('nsis', nsiscmd)

if 'blender' in B.targets:
	blenderexe= env.Alias('blender', B.program_list)
	Depends(blenderexe,installtarget)

if env['WITH_BF_PLAYER']:
	blenderplayer = env.Alias('blenderplayer', B.program_list)
	Depends(blenderplayer,installtarget)

if not env['WITH_BF_GAMEENGINE']:
	blendernogame = env.Alias('blendernogame', B.program_list)
	Depends(blendernogame,installtarget)

if 'blenderlite' in B.targets:
	blenderlite = env.Alias('blenderlite', B.program_list)
	Depends(blenderlite,installtarget)

Depends(nsiscmd, allinstall)

Default(B.program_list)

if not env['WITHOUT_BF_INSTALL']:
		Default(installtarget)

#------------ EPYDOC
if env['WITH_BF_DOCS']:
    try:
        import epydoc
    except ImportError:
        epydoc = None

    if epydoc:
        SConscript('source/gameengine/PyDoc/SConscript')
    else:
        print "No epydoc install detected, Python API and Gameengine API Docs will not be generated "


