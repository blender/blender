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

BlenderEnvironment = tools.Blender.BlenderEnvironment
btools = tools.btools
B = tools.Blender

### globals ###
platform = sys.platform
quickie = None
quickdebug = None
nsis_build = None

##### BEGIN SETUP #####

B.possible_types = ['core', 'common', 'blender', 'intern',
                    'international', 'game', 'game2',
                    'player', 'player2', 'system']

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
        if env:
            btools.SetupSpawn(env)
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
    platform = 'win32-vc'
elif env['CC'] in ['gcc'] and sys.platform=='win32':
    platform = 'win32-mingw'

env.SConscriptChdir(0)

crossbuild = B.arguments.get('BF_CROSS', None)
if crossbuild and platform!='win32':
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
    env.Append(LINKFLAGS=['-mwindows'])

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

# disable elbeem (fluidsim) compilation?
if env['BF_NO_ELBEEM'] == 1:
    env['CPPFLAGS'].append('-DDISABLE_ELBEEM')
    env['CXXFLAGS'].append('-DDISABLE_ELBEEM')
    env['CCFLAGS'].append('-DDISABLE_ELBEEM')

if env['WITH_BF_OPENMP'] == 1:
        if env['OURPLATFORM']=='win32-vc':
                env['CCFLAGS'].append('/openmp')
                env['CPPFLAGS'].append('/openmp')
                env['CXXFLAGS'].append('/openmp')
        else:
            if env['CC'] == 'icc':
                env.Append(LINKFLAGS=['-openmp', '-static-intel'])
                env['CCFLAGS'].append('-openmp')
                env['CPPFLAGS'].append('-openmp')
                env['CXXFLAGS'].append('-openmp')
            else:
                env['CCFLAGS'].append('-fopenmp')
                env['CPPFLAGS'].append('-fopenmp')
                env['CXXFLAGS'].append('-fopenmp')

#check for additional debug libnames

if env.has_key('BF_DEBUG_LIBS'):
    B.quickdebug += env['BF_DEBUG_LIBS']

printdebug = B.arguments.get('BF_LISTDEBUG', 0)

# see if this linux distro has libalut

if env['OURPLATFORM'] == 'linux2' :
    if env['WITH_BF_OPENAL']:
        mylib_test_source_file = """
        #include "AL/alut.h"
        int main(int argc, char **argv)
        {
            alutGetMajorVersion();
            return 0;
        }
        """

        def CheckFreeAlut(context,env):
            context.Message( B.bc.OKGREEN + "Linux platform detected:\n  checking for FreeAlut... " + B.bc.ENDC )
            env['LIBS'] = 'alut'
            result = context.TryLink(mylib_test_source_file, '.c')
            context.Result(result)
            return result

        env2 = env.Copy( LIBPATH = env['BF_OPENAL'] ) 
        sconf_temp = mkdtemp()
        conf = Configure( env2, {'CheckFreeAlut' : CheckFreeAlut}, sconf_temp, '/dev/null' )
        if conf.CheckFreeAlut( env2 ):
            env['BF_OPENAL_LIB'] += ' alut'
        del env2
        root = ''
        for root, dirs, files in os.walk(sconf_temp, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        if root: os.rmdir(root)

if len(B.quickdebug) > 0 and printdebug != 0:
    print B.bc.OKGREEN + "Buildings these libs with debug symbols:" + B.bc.ENDC
    for l in B.quickdebug:
        print "\t" + l

# check target for blenderplayer. Set WITH_BF_PLAYER if found on cmdline
if 'blenderplayer' in B.targets:
    env['WITH_BF_PLAYER'] = True

if 'blendernogame' in B.targets:
    env['WITH_BF_GAMEENGINE'] = False

# lastly we check for root_build_dir ( we should not do before, otherwise we might do wrong builddir
#B.root_build_dir = B.arguments.get('BF_BUILDDIR', '..'+os.sep+'build'+os.sep+platform+os.sep)
B.root_build_dir = env['BF_BUILDDIR']
env['BUILDDIR'] = B.root_build_dir
if not B.root_build_dir[-1]==os.sep:
    B.root_build_dir += os.sep
    
# We do a shortcut for clean when no quicklist is given: just delete
# builddir without reading in SConscripts
do_clean = None
if 'clean' in B.targets:
    do_clean = True

if not quickie and do_clean:
    if os.path.exists(B.root_build_dir):
        print B.bc.HEADER+'Cleaning...'+B.bc.ENDC
        dirs = os.listdir(B.root_build_dir)
        for entry in dirs:
            if os.path.isdir(B.root_build_dir + entry) == 1:
                print "clean dir %s"%(B.root_build_dir+entry)
                shutil.rmtree(B.root_build_dir+entry)
            else: # remove file
                print "remove file %s"%(B.root_build_dir+entry)
                os.remove(B.root_build_dir+entry)
        for confile in ['extern/ffmpeg/config.mak', 'extern/x264/config.mak',
                'extern/xvidcore/build/generic/platform.inc']:
            if os.path.exists(confile):
                print "clean file %s"%confile
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

Help(opts.GenerateHelpText(env))

# default is new quieter output, but if you need to see the 
# commands, do 'scons BF_QUIET=0'
bf_quietoutput = B.arguments.get('BF_QUIET', '1')
if env['BF_QUIET']:
    B.set_quiet_output(env)
else:
    if toolset=='msvc':
        B.msvc_hack(env)

print B.bc.HEADER+'Building in '+B.bc.ENDC+B.root_build_dir
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
    if not tp == 'player' and not tp == 'player2':
        mainlist += B.create_blender_liblist(env, tp)

if B.arguments.get('BF_PRIORITYLIST', '0')=='1':
    B.propose_priorities()

dobj = B.buildinfo(env, "dynamic") + B.resources
thestatlibs, thelibincs = B.setup_staticlibs(env)
thesyslibs = B.setup_syslibs(env)

env.BlenderProg(B.root_build_dir, "blender", dobj + mainlist + thestatlibs, [], thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blender')
if env['WITH_BF_PLAYER']:
    playerlist = B.create_blender_liblist(env, 'player')
    env.BlenderProg(B.root_build_dir, "blenderplayer", dobj + playerlist + thestatlibs, [], thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blenderplayer')

##### Now define some targets


#------------ INSTALL

#-- binaries
blenderinstall = []
if  env['OURPLATFORM']=='darwin':
    for prg in B.program_list:
        bundle = '%s.app' % prg[0]
        bundledir = os.path.dirname(bundle)
        for dp, dn, df in os.walk(bundle):
            if 'CVS' in dn:
                dn.remove('CVS')
            if '.svn' in dn:
                dn.remove('.svn')
            dir=env['BF_INSTALLDIR']+dp[len(bundledir):]
            source=[dp+os.sep+f for f in df]
            blenderinstall.append(env.Install(dir=dir,source=source))
else:
    blenderinstall = env.Install(dir=env['BF_INSTALLDIR'], source=B.program_list)

#-- .blender
#- dont do .blender and scripts for darwin, it is already in the bundle
dotblendlist = []
dottargetlist = []
scriptinstall = []

if  env['OURPLATFORM']!='darwin':
        for dp, dn, df in os.walk('bin/.blender'):
            if 'CVS' in dn:
                dn.remove('CVS')
            if '.svn' in dn:
                dn.remove('.svn')
            for f in df:
                dotblendlist.append(dp+os.sep+f)
                dottargetlist.append(env['BF_INSTALLDIR']+dp[3:]+os.sep+f)

        dotblenderinstall = []
        for targetdir,srcfile in zip(dottargetlist, dotblendlist):
            td, tf = os.path.split(targetdir)
            dotblenderinstall.append(env.Install(dir=td, source=srcfile))
        
        #-- .blender/scripts    
        scriptpath='release/scripts'
        for dp, dn, df in os.walk(scriptpath):
            if 'CVS' in dn:
                dn.remove('CVS')
            if '.svn' in dn:
                dn.remove('.svn')
            dir=env['BF_INSTALLDIR']+'/.blender/scripts'+dp[len(scriptpath):]
            source=[dp+os.sep+f for f in df]
            scriptinstall.append(env.Install(dir=dir,source=source))

#-- plugins
pluglist = []
plugtargetlist = []
for tp, tn, tf in os.walk('release/plugins'):
    if 'CVS' in tn:
        tn.remove('CVS')
    if '.svn' in tn:
        tn.remove('.svn')
    for f in tf:
        pluglist.append(tp+os.sep+f)
        plugtargetlist.append(env['BF_INSTALLDIR']+tp[7:]+os.sep+f)

plugininstall = []
for targetdir,srcfile in zip(plugtargetlist, pluglist):
    td, tf = os.path.split(targetdir)
    plugininstall.append(env.Install(dir=td, source=srcfile))

textlist = []
texttargetlist = []
for tp, tn, tf in os.walk('release/text'):
    if 'CVS' in tn:
        tn.remove('CVS')
    if '.svn' in tn:
        tn.remove('.svn')
    for f in tf:
        textlist.append(tp+os.sep+f)

textinstall = env.Install(dir=env['BF_INSTALLDIR'], source=textlist)

if  env['OURPLATFORM']=='darwin':
        allinstall = [blenderinstall, plugininstall, textinstall]
else:
        allinstall = [blenderinstall, dotblenderinstall, scriptinstall, plugininstall, textinstall]

if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw'):
    dllsources = ['${LCGDIR}/gettext/lib/gnu_gettext.dll',
                        '${LCGDIR}/png/lib/libpng.dll',
                        '#release/windows/extra/python25.zip',
                        '#release/windows/extra/zlib.pyd',
                        '${LCGDIR}/sdl/lib/SDL.dll',
                        '${LCGDIR}/zlib/lib/zlib.dll',
                        '${LCGDIR}/tiff/lib/libtiff.dll']
    if env['BF_DEBUG']:
        dllsources.append('${LCGDIR}/python/lib/${BF_PYTHON_LIB}_d.dll')
    else:
        dllsources.append('${LCGDIR}/python/lib/${BF_PYTHON_LIB}.dll')
    if env['OURPLATFORM'] == 'win32-mingw':
        dllsources += ['${LCGDIR}/pthreads/lib/pthreadGC2.dll']
    else:
        dllsources += ['${LCGDIR}/pthreads/lib/pthreadVC2.dll']
    if env['WITH_BF_ICONV']:
        dllsources += ['${LCGDIR}/iconv/lib/iconv.dll']
    if env['WITH_BF_FFMPEG']:
        dllsources += ['${LCGDIR}/ffmpeg/lib/avcodec-51.dll',
                        '${LCGDIR}/ffmpeg/lib/avformat-52.dll',
                        '${LCGDIR}/ffmpeg/lib/avdevice-52.dll',
                        '${LCGDIR}/ffmpeg/lib/avutil-49.dll',
                        '${LCGDIR}/ffmpeg/lib/libfaad-0.dll',
                        '${LCGDIR}/ffmpeg/lib/libfaac-0.dll',
                        '${LCGDIR}/ffmpeg/lib/libmp3lame-0.dll',
                        '${LCGDIR}/ffmpeg/lib/libx264-59.dll',
                        '${LCGDIR}/ffmpeg/lib/xvidcore.dll',
                        '${LCGDIR}/ffmpeg/lib/swscale-0.dll']
    windlls = env.Install(dir=env['BF_INSTALLDIR'], source = dllsources)
    allinstall += windlls

installtarget = env.Alias('install', allinstall)
bininstalltarget = env.Alias('install-bin', blenderinstall)

nsisaction = env.Action(btools.NSIS_Installer, btools.NSIS_print)
nsiscmd = env.Command('nsisinstaller', None, nsisaction)
nsisalias = env.Alias('nsis', nsiscmd)

if env['WITH_BF_PLAYER']:
    blenderplayer = env.Alias('blenderplayer', B.program_list)
    Depends(blenderplayer,installtarget)

if not env['WITH_BF_GAMEENGINE']:
    blendernogame = env.Alias('blendernogame', B.program_list)
    Depends(blendernogame,installtarget)

Depends(nsiscmd, allinstall)

Default(B.program_list)

if not env['WITHOUT_BF_INSTALL']:
        Default(installtarget)

#------------ RELEASE
# TODO: zipup the installation

#------------ BLENDERPLAYER
# TODO: build stubs and link into blenderplayer

#------------ EPYDOC
# TODO: run epydoc

