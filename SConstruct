#!/usr/bin/env python
# $Id$
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
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
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: none of this file.
#
# Contributor(s): Nathan Letwory.
#
# ***** END GPL/BL DUAL LICENSE BLOCK *****
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

import tools.Blender
import tools.btools
import tools.bcolors

BlenderEnvironment = tools.Blender.BlenderEnvironment
btools = tools.btools
B = tools.Blender

### globals ###
platform = sys.platform
quickie = None

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

crossbuild = B.arguments.get('BF_CROSS', None)
if crossbuild and platform!='win32':
    platform = 'linuxcross'

env['OURPLATFORM'] = platform

configfile = B.arguments.get('BF_CONFIG', 'config'+os.sep+platform+'-config.py')

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

# first read platform config. B.arguments will override
optfiles = [configfile]
if os.path.exists('user-config.py'):
    print B.bc.OKGREEN + "Using config file: " + B.bc.ENDC + 'user-config.py'
    optfiles += ['user-config.py']
else:
    print B.bc.WARNING + 'user-config.py' + " not found, no user overrides" + B.bc.ENDC

opts = btools.read_opts(optfiles, B.arguments)
opts.Update(env)

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
    print B.bc.HEADER+'Cleaning...'+B.bc.ENDC
    dirs = os.listdir(B.root_build_dir)
    for dir in dirs:
        if os.path.isdir(B.root_build_dir + dir) == 1:
            print "clean dir %s"%(B.root_build_dir+dir)
            shutil.rmtree(B.root_build_dir+dir)
    print B.bc.OKGREEN+'...done'+B.bc.ENDC
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
if bf_quietoutput=='1':
    B.set_quiet_output(env)
else:
    if toolset=='msvc':
        B.msvc_hack(env)

print B.bc.HEADER+'Building in '+B.bc.ENDC+B.root_build_dir
env.SConsignFile(B.root_build_dir+'scons-signatures')
B.init_lib_dict()

##### END SETUP ##########

Export('env')
#Export('root_build_dir') # this one is still needed for makesdna
##TODO: improve makesdna usage

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

dobj = B.buildinfo(env, "dynamic")
thestatlibs, thelibincs = B.setup_staticlibs(env)
thesyslibs = B.setup_syslibs(env)

env.BlenderProg(B.root_build_dir, "blender", dobj + mainlist + thestatlibs, [], thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blender')
if env['WITH_BF_PLAYER']:
    playerlist = B.create_blender_liblist(env, 'player')
    env.BlenderProg(B.root_build_dir, "blenderplayer", dobj + playerlist + thestatlibs, [], thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blenderplayer')

##### Now define some targets


#------------ INSTALL

blenderinstall = env.Install(dir=env['BF_INSTALLDIR'], source=B.program_list)

#-- .blender
dotblendlist = []
dottargetlist = []
for dp, dn, df in os.walk('bin/.blender'):
    if 'CVS' in dn:
        dn.remove('CVS')
    for f in df:
        dotblendlist.append(dp+os.sep+f)
        dottargetlist.append(env['BF_INSTALLDIR']+dp[3:]+os.sep+f)

dotblenderinstall = []
for targetdir,srcfile in zip(dottargetlist, dotblendlist):
    td, tf = os.path.split(targetdir)
    dotblenderinstall.append(env.Install(dir=td, source=srcfile))

#-- .blender/scripts
scriptlist = glob.glob('release/scripts/*.py')
scriptinstall = env.Install(dir=env['BF_INSTALLDIR']+'/.blender/scripts', source=scriptlist)

#-- plugins
pluglist = []
plugtargetlist = []
for tp, tn, tf in os.walk('release/plugins'):
    if 'CVS' in tn:
        tn.remove('CVS')
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
    for f in tf:
        textlist.append(tp+os.sep+f)

textinstall = env.Install(dir=env['BF_INSTALLDIR'], source=textlist)

allinstall = [blenderinstall, dotblenderinstall, scriptinstall, plugininstall, textinstall]

if env['OURPLATFORM'] == 'win32-vc':
    windlls = env.Install(dir=env['BF_INSTALLDIR'], source = ['#../lib/windows/gettext/lib/gnu_gettext.dll',
                        '#../lib/windows/png/lib/libpng.dll',
                        '#../lib/windows/python/lib/python24.dll',
                        '#../lib/windows/sdl/lib/SDL.dll',
                        '#../lib/windows/zlib/lib/zlib.dll'])
    allinstall += windlls

installtarget = env.Alias('install', allinstall)
bininstalltarget = env.Alias('install-bin', blenderinstall)

if env['WITH_BF_PLAYER']:
    blenderplayer = env.Alias('blenderplayer', B.program_list)
    Depends(blenderplayer,installtarget)

if not env['WITH_BF_GAMEENGINE']:
    blendernogame = env.Alias('blendernogame', B.program_list)
    Depends(blendernogame,installtarget)


Default(B.program_list)
Default(installtarget)

#------------ RELEASE
# TODO: zipup the installation

#------------ BLENDERPLAYER
# TODO: build stubs and link into blenderplayer

#------------ EPYDOC
# TODO: run epydoc

