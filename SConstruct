#!/usr/bin/env python
#
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#
# TODO: fix /FORCE:MULTIPLE on windows to get proper debug builds.
# TODO: directory copy functions are far too complicated, see:
#       http://wiki.blender.org/index.php/User:Ideasman42/SConsNotSimpleInstallingFiles

import sys
import os
import os.path
import string
import shutil
import re

# store path to tools and modules
toolpath=os.path.join(".", "build_files", "scons", "tools")
modulespath=os.path.join(".", "build_files", "scons", "Modules")

# needed for importing tools and modules
sys.path.append(toolpath)
sys.path.append(modulespath)

import Blender
import btools

EnsureSConsVersion(1,0,0)

# Before we do anything, let's check if we have a sane os.environ
if not btools.check_environ():
    Exit()

BlenderEnvironment = Blender.BlenderEnvironment
B = Blender

VERSION = btools.VERSION # This is used in creating the local config directories
VERSION_RELEASE_CYCLE = btools.VERSION_RELEASE_CYCLE

### globals ###
platform = sys.platform
quickie = None
quickdebug = None

##### BEGIN SETUP #####

B.possible_types = ['core', 'player', 'player2', 'intern', 'extern', 'system']

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

# bitness stuff
tempbitness = int(B.arguments.get('BF_BITNESS', B.bitness)) # default to bitness found as per starting python
if tempbitness in B.allowed_bitnesses.values() :
    B.bitness = tempbitness

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
vcver = B.arguments.get('MSVS_VERSION', '12.0')

if toolset:
    print "Using " + toolset
    if toolset=='mstoolkit':
        env = BlenderEnvironment(ENV = os.environ)
        env.Tool('mstoolkit', [toolpath])
    else:
        env = BlenderEnvironment(tools=[toolset], ENV = os.environ)
        if env:
            btools.SetupSpawn(env)
else:
    if B.bitness==64 and platform=='win32':
        env = BlenderEnvironment(ENV = os.environ, MSVS_ARCH='amd64', TARGET_ARCH='x86_64', MSVC_VERSION=vcver)
    else:
        env = BlenderEnvironment(ENV = os.environ, TARGET_ARCH='x86', MSVC_VERSION=vcver)

if not env:
    print "Could not create a build environment"
    Exit()

cc = B.arguments.get('CC', None)
cxx = B.arguments.get('CXX', None)
if cc:
    env['CC'] = cc
if cxx:
    env['CXX'] = cxx

if sys.platform=='win32':
    if env['CC'] in ['cl', 'cl.exe']:
        platform = 'win64-vc' if B.bitness == 64 else 'win32-vc'
    elif env['CC'] in ['gcc']:
        platform = 'win64-mingw' if B.bitness == 64 else 'win32-mingw'

if 'mingw' in platform:
    print "Setting custom spawn function"
    btools.SetupSpawn(env)

env.SConscriptChdir(0)

# Remove major kernel version from linux platform.
# After Linus switched kernel to new version model this major version
# shouldn't take much sense for building rules.

if re.match('linux[0-9]+', platform):
    platform = 'linux'

crossbuild = B.arguments.get('BF_CROSS', None)
if crossbuild and platform not in ('win32-vc', 'win64-vc'):
    platform = 'linuxcross'

env['OURPLATFORM'] = platform

configfile = os.path.join("build_files", "scons", "config", platform + "-config.py")

if os.path.exists(configfile):
    print B.bc.OKGREEN + "Using config file: " + B.bc.ENDC + configfile
else:
    print B.bc.FAIL + configfile + " doesn't exist" + B.bc.ENDC

if crossbuild and env['PLATFORM'] != 'win32':
    print B.bc.HEADER+"Preparing for crossbuild"+B.bc.ENDC
    env.Tool('crossmingw', [toolpath])
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

opts = btools.read_opts(env, optfiles, B.arguments)
opts.Update(env)

if sys.platform=='win32':
    if B.bitness==64:
        env.Append(CPPFLAGS=['-DWIN64']) # -DWIN32 needed too, as it's used all over to target Windows generally

if not env['BF_FANCY']:
    B.bc.disable()


# remove install dir so old and new files are not mixed.
# NOTE: only do the scripts directory for now, otherwise is too disruptive for developers
# TODO: perhaps we need an option (off by default) to not do this altogether...
if not env['WITHOUT_BF_INSTALL'] and not env['WITHOUT_BF_OVERWRITE_INSTALL']:
    scriptsDir = os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts')
    if os.path.isdir(scriptsDir):
        print B.bc.OKGREEN + "Clearing installation directory%s: %s" % (B.bc.ENDC, os.path.abspath(scriptsDir))
        shutil.rmtree(scriptsDir)


SetOption('num_jobs', int(env['BF_NUMJOBS']))
print B.bc.OKGREEN + "Build with parallel jobs%s: %s" % (B.bc.ENDC, GetOption('num_jobs'))
print B.bc.OKGREEN + "Build with debug symbols%s: %s" % (B.bc.ENDC, env['BF_DEBUG'])

if 'blenderlite' in B.targets:
    target_env_defs = {}
    target_env_defs['WITH_BF_GAMEENGINE'] = False
    target_env_defs['WITH_BF_CYCLES'] = False
    target_env_defs['WITH_BF_OPENAL'] = False
    target_env_defs['WITH_BF_OPENEXR'] = False
    target_env_defs['WITH_BF_PSD'] = False
    target_env_defs['WITH_BF_OPENMP'] = False
    target_env_defs['WITH_BF_ICONV'] = False
    target_env_defs['WITH_BF_INTERNATIONAL'] = False
    target_env_defs['WITH_BF_OPENJPEG'] = False
    target_env_defs['WITH_BF_FFMPEG'] = False
    target_env_defs['WITH_BF_QUICKTIME'] = False
    target_env_defs['WITH_BF_REDCODE'] = False
    target_env_defs['WITH_BF_DDS'] = False
    target_env_defs['WITH_BF_CINEON'] = False
    target_env_defs['WITH_BF_FRAMESERVER'] = False
    target_env_defs['WITH_BF_HDR'] = False
    target_env_defs['WITH_BF_ZLIB'] = False
    target_env_defs['WITH_BF_SDL'] = False
    target_env_defs['WITH_BF_JPEG'] = False
    target_env_defs['WITH_BF_PNG'] = False
    target_env_defs['WITH_BF_BULLET'] = False
    target_env_defs['WITH_BF_BINRELOC'] = False
    target_env_defs['BF_BUILDINFO'] = False
    target_env_defs['WITH_BF_FLUID'] = False
    target_env_defs['WITH_BF_OCEANSIM'] = False
    target_env_defs['WITH_BF_SMOKE'] = False
    target_env_defs['WITH_BF_BOOLEAN'] = False
    target_env_defs['WITH_BF_REMESH'] = False
    target_env_defs['WITH_BF_PYTHON'] = False
    target_env_defs['WITH_BF_3DMOUSE'] = False
    target_env_defs['WITH_BF_LIBMV'] = False
    target_env_defs['WITH_BF_FREESTYLE'] = False

    # Merge blenderlite, let command line to override
    for k,v in target_env_defs.iteritems():
        if k not in B.arguments:
            env[k] = v

if 'cudakernels' in B.targets:
    env['WITH_BF_CYCLES'] = True
    env['WITH_BF_CYCLES_CUDA_BINARIES'] = True
    env['WITH_BF_PYTHON'] = False
    env['WITH_BF_LIBMV'] = False

# Configure paths for automated configuration test programs
env['CONFIGUREDIR'] = os.path.abspath(os.path.normpath(os.path.join(env['BF_BUILDDIR'], "sconf_temp")))
env['CONFIGURELOG'] = os.path.abspath(os.path.normpath(os.path.join(env['BF_BUILDDIR'], "config.log")))

#############################################################################
###################    Automatic configuration for OSX     ##################
#############################################################################

if env['OURPLATFORM']=='darwin':

    import commands
    import subprocess

    command = ["%s"%env['CC'], "--version"]
    line = btools.get_command_output(command)
    ver = re.search(r'[0-9]+(\.[0-9]+[svn]+)+', line) or re.search(r'[0-9]+(\.[0-9]+)+', line) # read the "based on LLVM x.xsvn" version here, not the Apple version
    if ver:
        env['CCVERSION'] = ver.group(0).strip('svn')
    frontend = re.search(r'gcc', line) or re.search(r'clang', line) or re.search(r'llvm-gcc', line)  or re.search(r'icc', line)
    if frontend:
        env['C_COMPILER_ID'] = frontend.group(0)
		
    vendor = re.search(r'Apple', line)
    if vendor:
        C_VENDOR = vendor.group(0)
    else:
        C_VENDOR = 'Open Source'

    print B.bc.OKGREEN + "Using Compiler: " + B.bc.ENDC  +  env['C_COMPILER_ID'] + '-' + env['CCVERSION'] + ' ( ' + C_VENDOR + ' )'

    cmd = 'sw_vers -productVersion'
    MAC_CUR_VER=cmd_res=commands.getoutput(cmd)
    cmd = 'xcodebuild -version'
    cmd_xcode=commands.getoutput(cmd)
    env['XCODE_CUR_VER']=cmd_xcode[6:][:3] # truncate output to major.minor version
    cmd = 'xcodebuild -showsdks'
    cmd_sdk=commands.getoutput(cmd)
    MACOSX_SDK_CHECK=cmd_sdk
    cmd = 'xcode-select --print-path'
    XCODE_SELECT_PATH=commands.getoutput(cmd)
    if XCODE_SELECT_PATH.endswith("/Contents/Developer"):
        XCODE_BUNDLE=XCODE_SELECT_PATH[:-19]
    else:
        XCODE_BUNDLE=XCODE_SELECT_PATH

    print B.bc.OKGREEN + "Detected Xcode version: -- " + B.bc.ENDC + env['XCODE_CUR_VER'] + " --"
    print B.bc.OKGREEN + "Available SDK's: \n" + B.bc.ENDC + MACOSX_SDK_CHECK.replace('\t', '')

    if env['MACOSX_SDK'] == '': # no set sdk, choosing best one found
        if 'OS X 10.10' in MACOSX_SDK_CHECK:
            env['MACOSX_DEPLOYMENT_TARGET'] = '10.6'
            env['MACOSX_SDK']='/Developer/SDKs/MacOSX10.10.sdk'
        elif 'OS X 10.9' in MACOSX_SDK_CHECK:
            env['MACOSX_DEPLOYMENT_TARGET'] = '10.6'
            env['MACOSX_SDK']='/Developer/SDKs/MacOSX10.9.sdk'
        elif 'OS X 10.8' in MACOSX_SDK_CHECK:
            env['MACOSX_DEPLOYMENT_TARGET'] = '10.6'
            env['MACOSX_SDK']='/Developer/SDKs/MacOSX10.8.sdk'
        elif 'OS X 10.7' in MACOSX_SDK_CHECK:
            env['MACOSX_DEPLOYMENT_TARGET'] = '10.6'
            env['MACOSX_SDK']='/Developer/SDKs/MacOSX10.7.sdk'
        elif 'OS X 10.6' in MACOSX_SDK_CHECK:
            env['MACOSX_DEPLOYMENT_TARGET'] = '10.6'
            env['MACOSX_SDK']='/Developer/SDKs/MacOSX10.6.sdk'
        elif 'OS X 10.5' in MACOSX_SDK_CHECK:
            env['MACOSX_DEPLOYMENT_TARGET'] = '10.5'
            env['MACOSX_SDK']='/Developer/SDKs/MacOSX10.5.sdk'
    else:
        env['MACOSX_SDK']='/Developer/SDKs/MacOSX' + env['MACOSX_SDK'] + '.sdk'

    if env['XCODE_CUR_VER'] >= '4.3':  ## since version 4.3, XCode and developer dir are bundled ##
         env['MACOSX_SDK'] = XCODE_BUNDLE + '/Contents/Developer/Platforms/MacOSX.platform' +  env['MACOSX_SDK']

    print B.bc.OKGREEN + "Using OSX SDK :" + B.bc.ENDC + env['MACOSX_SDK']
		
    if not env['WITH_OSX_STATICPYTHON'] == 1:
        # python 3.3 uses Python-framework additionally installed in /Library/Frameworks
        env['BF_PYTHON'] = '/Library/Frameworks/Python.framework/Versions/'
        env['BF_PYTHON_INC'] = env['BF_PYTHON'] + env['BF_PYTHON_VERSION'] + '/include/python' + env['BF_PYTHON_VERSION'] + 'm'
        env['BF_PYTHON_BINARY'] = env['BF_PYTHON'] + env['BF_PYTHON_VERSION'] + '/bin/python' + env['BF_PYTHON_VERSION']
        env['BF_PYTHON_LIB'] = ''
        env['BF_PYTHON_LIBPATH'] = env['BF_PYTHON'] + env['BF_PYTHON_VERSION'] + '/lib/python' + env['BF_PYTHON_VERSION'] + '/config-' + env['BF_PYTHON_VERSION'] +'m'
        env['PLATFORM_LINKFLAGS'] = env['PLATFORM_LINKFLAGS']+['-framework','Python'] # link to python framework

    #Ray trace optimization
    if env['WITH_BF_RAYOPTIMIZATION'] == 1:
        if env['MACOSX_ARCHITECTURE'] == 'x86_64' or env['MACOSX_ARCHITECTURE'] == 'i386':
            env['WITH_BF_RAYOPTIMIZATION'] = 1
        else:
            env['WITH_BF_RAYOPTIMIZATION'] = 0
        if env['MACOSX_ARCHITECTURE'] == 'i386':
            env['BF_RAYOPTIMIZATION_SSE_FLAGS'] = env['BF_RAYOPTIMIZATION_SSE_FLAGS']+['-msse']
        elif env['MACOSX_ARCHITECTURE'] == 'x86_64':
            env['BF_RAYOPTIMIZATION_SSE_FLAGS'] = env['BF_RAYOPTIMIZATION_SSE_FLAGS']+['-msse','-msse2']

    if env['MACOSX_ARCHITECTURE'] == 'x86_64' or env['MACOSX_ARCHITECTURE'] == 'ppc64':
        ARCH_FLAGS = ['-m64']
    else:
        ARCH_FLAGS = ['-m32']

    env.Append(CPPFLAGS=ARCH_FLAGS)

    SDK_FLAGS=['-isysroot',  env['MACOSX_SDK'],'-mmacosx-version-min='+ env['MACOSX_DEPLOYMENT_TARGET'],'-arch',env['MACOSX_ARCHITECTURE']] # always used
    env['PLATFORM_LINKFLAGS'] = ['-mmacosx-version-min='+ env['MACOSX_DEPLOYMENT_TARGET'],'-isysroot', env['MACOSX_SDK'],'-arch',env['MACOSX_ARCHITECTURE']]+ARCH_FLAGS+env['PLATFORM_LINKFLAGS']
    env['CCFLAGS']=SDK_FLAGS+env['CCFLAGS']
    env['CXXFLAGS']=SDK_FLAGS+env['CXXFLAGS']

    #Intel Macs are CoreDuo and Up
    if env['MACOSX_ARCHITECTURE'] == 'i386' or env['MACOSX_ARCHITECTURE'] == 'x86_64':
        env['REL_CCFLAGS'] = env['REL_CCFLAGS']+['-msse','-msse2','-msse3']
        if env['C_COMPILER_ID'] != 'clang' or (env['C_COMPILER_ID'] == 'clang' and env['CCVERSION'] >= '3.3'):
            env['REL_CCFLAGS'] = env['REL_CCFLAGS']+['-ftree-vectorize'] # clang xcode 4 does not accept flag
    else:
        env['CCFLAGS'] =  env['CCFLAGS']+['-fno-strict-aliasing']

    # Intel 64bit Macs are Core2Duo and up
    if env['MACOSX_ARCHITECTURE'] == 'x86_64':
        env['REL_CCFLAGS'] = env['REL_CCFLAGS']+['-mssse3']

    if env['C_COMPILER_ID'] == 'clang' and env['CCVERSION'] >= '3.3':
        env['CCFLAGS'].append('-ftemplate-depth=1024') # only valid for clang bundled with xcode 5

    # 3DconnexionClient.framework, optionally install
    if env['WITH_BF_3DMOUSE'] == 1:
        if not os.path.exists('/Library/Frameworks/3DconnexionClient.framework'):
            env['WITH_BF_3DMOUSE'] = 0
            print B.bc.OKGREEN + "3DconnexionClient install not found, disabling WITH_BF_3DMOUSE" # avoid build errors !
        else:
            env.Append(LINKFLAGS=['-F/Library/Frameworks','-Xlinker','-weak_framework','-Xlinker','3DconnexionClient'])
            env['BF_3DMOUSE_INC'] = '/Library/Frameworks/3DconnexionClient.framework/Headers'
            print B.bc.OKGREEN + "Using 3Dconnexion"

    # Jackmp.framework, optionally install
    if env['WITH_BF_JACK'] == 1:
        if not os.path.exists('/Library/Frameworks/Jackmp.framework'):
            env['WITH_BF_JACK'] = 0
            print B.bc.OKGREEN + "JackOSX install not found, disabling WITH_BF_JACK" # avoid build errors !
        else:
            env.Append(LINKFLAGS=['-F/Library/Frameworks','-Xlinker','-weak_framework','-Xlinker','Jackmp'])
            print B.bc.OKGREEN + "Using Jack"

    if env['WITH_BF_QUICKTIME'] == 1:
        env['PLATFORM_LINKFLAGS'] = env['PLATFORM_LINKFLAGS']+['-framework','QTKit']

    #Defaults openMP to true if compiler handles it ( only gcc 4.6.1 and newer )
    # if your compiler does not have accurate suffix you may have to enable it by hand !
    if env['WITH_BF_OPENMP'] == 1:
        if env['C_COMPILER_ID'] == 'gcc' and env['CCVERSION'] >= '4.6.1' or env['C_COMPILER_ID'] == 'clang' and env['CCVERSION'] >= '3.4' and C_VENDOR != 'Apple':
            env['WITH_BF_OPENMP'] = 1  # multithreading for fluids, cloth, sculpt and smoke
            print B.bc.OKGREEN + "Using OpenMP"
            if env['C_COMPILER_ID'] == 'clang' and env['CCVERSION'] >= '3.4':
                OSX_OMP_LIBPATH = Dir(env.subst(env['LCGDIR'])).abspath
                env.Append(BF_PROGRAM_LINKFLAGS=['-L'+OSX_OMP_LIBPATH+'/openmp/lib','-liomp5'])
                env['CCFLAGS'].append('-I'+OSX_OMP_LIBPATH+'/openmp/include') # include for omp.h
        else:
            env['WITH_BF_OPENMP'] = 0
            print B.bc.OKGREEN + "Disabled OpenMP, not supported by compiler"

    if env['WITH_BF_CYCLES_OSL'] == 1:
        env['WITH_BF_LLVM'] = 1
        OSX_OSL_LIBPATH = Dir(env.subst(env['BF_OSL_LIBPATH'])).abspath
        # we need 2 variants of passing the oslexec with the force_load option, string and list type atm
        if env['C_COMPILER_ID'] == 'gcc' and env['CCVERSION'] >= '4.8' or env['C_COMPILER_ID'] == 'clang' and env['CCVERSION'] >= '3.4':
            env.Append(LINKFLAGS=['-L'+OSX_OSL_LIBPATH,'-loslcomp','-loslexec','-loslquery'])
        else:
            env.Append(LINKFLAGS=['-L'+OSX_OSL_LIBPATH,'-loslcomp','-force_load '+ OSX_OSL_LIBPATH +'/liboslexec.a','-loslquery'])
        env.Append(BF_PROGRAM_LINKFLAGS=['-Xlinker','-force_load','-Xlinker',OSX_OSL_LIBPATH +'/liboslexec.a'])
    else:
        env['WITH_BF_LLVM'] = 0

    if env['WITH_BF_LLVM'] == 0:
        # Due duplicated generic UTF functions, we pull them either from LLVMSupport or COLLADA
        env.Append(BF_OPENCOLLADA_LIB=' UTF')

    # Trying to get rid of eventually clashes, we export some symbols explicite as local
    env.Append(LINKFLAGS=['-Xlinker','-unexported_symbols_list','-Xlinker','./source/creator/osx_locals.map'])
    
    #for < 10.7.sdk, SystemStubs needs to be linked
    if  env['MACOSX_SDK'].endswith("10.6.sdk") or  env['MACOSX_SDK'].endswith("10.5.sdk"):
        env['LLIBS'].append('SystemStubs')

#############################################################################
###################  End Automatic configuration for OSX   ##################
#############################################################################

if env['WITH_BF_OPENMP'] == 1:
        if env['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
                env['CCFLAGS'].append('/openmp')
        else:
            if env['CC'].endswith('icc'): # to be able to handle CC=/opt/bla/icc case
                env.Append(LINKFLAGS=['-openmp', '-static-intel'])
                env['CCFLAGS'].append('-openmp')
            else:
                env.Append(CCFLAGS=['-fopenmp'])

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

if not env['WITH_BF_GAMEENGINE']:
    env['WITH_BF_PLAYER'] = False

# build without elbeem (fluidsim)?
if env['WITH_BF_FLUID'] == 1:
    env['CPPFLAGS'].append('-DWITH_MOD_FLUID')

# build with ocean sim?
if env['WITH_BF_OCEANSIM'] == 1:
    env['WITH_BF_FFTW3']  = 1  # ocean needs fftw3 so enable it
    env['CPPFLAGS'].append('-DWITH_MOD_OCEANSIM')


if btools.ENDIAN == "big":
    env['CPPFLAGS'].append('-D__BIG_ENDIAN__')
else:
    env['CPPFLAGS'].append('-D__LITTLE_ENDIAN__')

# TODO, make optional (as with CMake)
env['CPPFLAGS'].append('-DWITH_AUDASPACE')
env['CPPFLAGS'].append('-DWITH_AVI')
env['CPPFLAGS'].append('-DWITH_OPENNL')

if env['OURPLATFORM'] not in ('win32-vc', 'win64-vc'):
    env['CPPFLAGS'].append('-DHAVE_STDBOOL_H')

# lastly we check for root_build_dir ( we should not do before, otherwise we might do wrong builddir
B.root_build_dir = env['BF_BUILDDIR']
B.doc_build_dir = os.path.join(env['BF_INSTALLDIR'], 'doc')
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


# ensure python header is found since detection can fail, this could happen
# with _any_ library but since we used a fixed python version this tends to
# be most problematic.
if env['WITH_BF_PYTHON']:
    found_python_h = found_pyconfig_h = False
    for bf_python_inc in env.subst('${BF_PYTHON_INC}').split():
        py_h = os.path.join(Dir(bf_python_inc).abspath, "Python.h")
        if os.path.exists(py_h):
            found_python_h = True
        py_h = os.path.join(Dir(bf_python_inc).abspath, "pyconfig.h")
        if os.path.exists(py_h):
            found_pyconfig_h = True

    if not (found_python_h and found_pyconfig_h):
        print("""\nMissing: Python.h and/or pyconfig.h in "%s"
         Set 'BF_PYTHON_INC' to point to valid include path(s),
         containing Python.h and pyconfig.h for Python version "%s".

         Example: python scons/scons.py BF_PYTHON_INC=../Python/include
              """ % (env.subst('${BF_PYTHON_INC}'), env.subst('${BF_PYTHON_VERSION}')))
        Exit()


if not os.path.isdir ( B.root_build_dir):
    os.makedirs ( B.root_build_dir )
    os.makedirs ( B.root_build_dir + 'source' )
    os.makedirs ( B.root_build_dir + 'intern' )
    os.makedirs ( B.root_build_dir + 'extern' )
    os.makedirs ( B.root_build_dir + 'lib' )
    os.makedirs ( B.root_build_dir + 'bin' )
# # Docs not working with epy anymore
# if not os.path.isdir(B.doc_build_dir) and env['WITH_BF_DOCS']:
#     os.makedirs ( B.doc_build_dir )


###################################
# Ensure all data files are valid #
###################################
if not os.path.isdir ( B.root_build_dir + 'data_headers'):
    os.makedirs ( B.root_build_dir + 'data_headers' )
if not os.path.isdir ( B.root_build_dir + 'data_sources'):
    os.makedirs ( B.root_build_dir + 'data_sources' )
# use for includes
env['DATA_HEADERS'] = os.path.join(os.path.abspath(env['BF_BUILDDIR']), "data_headers")
env['DATA_SOURCES'] = os.path.join(os.path.abspath(env['BF_BUILDDIR']), "data_sources")
def data_to_c(FILE_FROM, FILE_TO, VAR_NAME):
    if os.sep == "\\":
        FILE_FROM = FILE_FROM.replace("/", "\\")
        FILE_TO   = FILE_TO.replace("/", "\\")

    # first check if we need to bother.
    if os.path.exists(FILE_TO):
        if os.path.getmtime(FILE_FROM) < os.path.getmtime(FILE_TO):
            return

    print(B.bc.HEADER + "Generating: " + B.bc.ENDC + "%r" % os.path.basename(FILE_TO))
    fpin = open(FILE_FROM, "rb")
    fpin.seek(0, os.SEEK_END)
    size = fpin.tell()
    fpin.seek(0)

    fpout = open(FILE_TO, "w")
    fpout.write("int  %s_size = %d;\n" % (VAR_NAME, size))
    fpout.write("char %s[] = {\n" % VAR_NAME)

    while size > 0:
        size -= 1
        if size % 32 == 31:
            fpout.write("\n")

        fpout.write("%3d," % ord(fpin.read(1)))
    fpout.write("\n  0};\n\n")

    fpin.close()
    fpout.close()

def data_to_c_simple(FILE_FROM):
	filename_only = os.path.basename(FILE_FROM)
	FILE_TO = os.path.join(env['DATA_SOURCES'], filename_only + ".c")
	VAR_NAME = "datatoc_" + filename_only.replace(".", "_")

	data_to_c(FILE_FROM, FILE_TO, VAR_NAME)


def data_to_c_simple_icon(PATH_FROM):

    # first handle import
    import sys
    path = "source/blender/datatoc"
    if path not in sys.path:
        sys.path.append(path)

    # convert the pixmaps to a png
    import datatoc_icon

    filename_only = os.path.basename(PATH_FROM)
    FILE_TO_PNG = os.path.join(env['DATA_SOURCES'], filename_only + ".png")
    FILE_TO = FILE_TO_PNG + ".c"
    argv = [PATH_FROM, FILE_TO_PNG]
    datatoc_icon.main_ex(argv)

    # then the png to a c file
    data_to_c_simple(FILE_TO_PNG)


if B.targets != ['cudakernels']:
    data_to_c("source/blender/compositor/operations/COM_OpenCLKernels.cl",
              B.root_build_dir + "data_headers/COM_OpenCLKernels.cl.h",
              "datatoc_COM_OpenCLKernels_cl")

    data_to_c_simple("release/datafiles/startup.blend")
    data_to_c_simple("release/datafiles/preview.blend")
    data_to_c_simple("release/datafiles/preview_cycles.blend")

    # --- glsl ---
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_simple_frag.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_simple_vert.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_material.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_material.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_sep_gaussian_blur_frag.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_sep_gaussian_blur_vert.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_vertex.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_vsm_store_frag.glsl")
    data_to_c_simple("source/blender/gpu/shaders/gpu_shader_vsm_store_vert.glsl")
    data_to_c_simple("intern/opencolorio/gpu_shader_display_transform.glsl")

    # --- blender ---
    data_to_c_simple("release/datafiles/bfont.pfb")
    data_to_c_simple("release/datafiles/bfont.ttf")
    data_to_c_simple("release/datafiles/bmonofont.ttf")

    data_to_c_simple("release/datafiles/splash.png")
    data_to_c_simple("release/datafiles/splash_2x.png")

    # data_to_c_simple("release/datafiles/blender_icons16.png")
    # data_to_c_simple("release/datafiles/blender_icons32.png")
    data_to_c_simple_icon("release/datafiles/blender_icons16")
    data_to_c_simple_icon("release/datafiles/blender_icons32")

    data_to_c_simple("release/datafiles/prvicons.png")

    data_to_c_simple("release/datafiles/brushicons/add.png")
    data_to_c_simple("release/datafiles/brushicons/blob.png")
    data_to_c_simple("release/datafiles/brushicons/blur.png")
    data_to_c_simple("release/datafiles/brushicons/clay.png")
    data_to_c_simple("release/datafiles/brushicons/claystrips.png")
    data_to_c_simple("release/datafiles/brushicons/clone.png")
    data_to_c_simple("release/datafiles/brushicons/crease.png")
    data_to_c_simple("release/datafiles/brushicons/darken.png")
    data_to_c_simple("release/datafiles/brushicons/draw.png")
    data_to_c_simple("release/datafiles/brushicons/fill.png")
    data_to_c_simple("release/datafiles/brushicons/flatten.png")
    data_to_c_simple("release/datafiles/brushicons/grab.png")
    data_to_c_simple("release/datafiles/brushicons/inflate.png")
    data_to_c_simple("release/datafiles/brushicons/layer.png")
    data_to_c_simple("release/datafiles/brushicons/lighten.png")
    data_to_c_simple("release/datafiles/brushicons/mask.png")
    data_to_c_simple("release/datafiles/brushicons/mix.png")
    data_to_c_simple("release/datafiles/brushicons/multiply.png")
    data_to_c_simple("release/datafiles/brushicons/nudge.png")
    data_to_c_simple("release/datafiles/brushicons/pinch.png")
    data_to_c_simple("release/datafiles/brushicons/scrape.png")
    data_to_c_simple("release/datafiles/brushicons/smear.png")
    data_to_c_simple("release/datafiles/brushicons/smooth.png")
    data_to_c_simple("release/datafiles/brushicons/snake_hook.png")
    data_to_c_simple("release/datafiles/brushicons/soften.png")
    data_to_c_simple("release/datafiles/brushicons/subtract.png")
    data_to_c_simple("release/datafiles/brushicons/texdraw.png")
    data_to_c_simple("release/datafiles/brushicons/texfill.png")
    data_to_c_simple("release/datafiles/brushicons/texmask.png")
    data_to_c_simple("release/datafiles/brushicons/thumb.png")
    data_to_c_simple("release/datafiles/brushicons/twist.png")
    data_to_c_simple("release/datafiles/brushicons/vertexdraw.png")

    data_to_c_simple("release/datafiles/matcaps/mc01.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc02.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc03.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc04.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc05.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc06.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc07.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc08.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc09.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc10.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc11.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc12.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc13.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc14.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc15.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc16.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc17.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc18.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc19.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc20.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc21.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc22.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc23.jpg")
    data_to_c_simple("release/datafiles/matcaps/mc24.jpg")

##### END DATAFILES ##########

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

if B.targets != ['cudakernels']:
    # Put all auto configuration run-time tests here

    from FindSharedPtr import FindSharedPtr
    from FindUnorderedMap import FindUnorderedMap

    conf = Configure(env)
    conf.env.Append(LINKFLAGS=env['PLATFORM_LINKFLAGS'])
    FindSharedPtr(conf)
    FindUnorderedMap(conf)
    env = conf.Finish()

# End of auto configuration

Export('env')

VariantDir(B.root_build_dir+'/source', 'source', duplicate=0)
SConscript(B.root_build_dir+'/source/SConscript')
VariantDir(B.root_build_dir+'/intern', 'intern', duplicate=0)
SConscript(B.root_build_dir+'/intern/SConscript')
VariantDir(B.root_build_dir+'/extern', 'extern', duplicate=0)
SConscript(B.root_build_dir+'/extern/SConscript')

# now that we have read all SConscripts, we know what
# libraries will be built. Create list of
# libraries to give as objects to linking phase
mainlist = []
for tp in B.possible_types:
    if (not tp == 'player') and (not tp == 'player2') and (not tp == 'system'):
        mainlist += B.create_blender_liblist(env, tp)

if B.arguments.get('BF_PRIORITYLIST', '0')=='1':
    B.propose_priorities()

dobj = B.buildinfo(env, "dynamic") + B.resources
creob = B.creator(env)
thestatlibs, thelibincs = B.setup_staticlibs(env)
thesyslibs = B.setup_syslibs(env)

# Hack to pass OSD libraries to linker before extern_{clew,cuew}
for x in B.create_blender_liblist(env, 'system'):
    thesyslibs.append(os.path.basename(x))
    thelibincs.append(os.path.dirname(x))

if 'blender' in B.targets or not env['WITH_BF_NOBLENDER']:
    blender_progname = "blender"
    if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'win64-vc', 'linuxcross'):
        blender_progname = "blender-app"

        lenv = env.Clone()
        lenv.Append(LINKFLAGS = env['PLATFORM_LINKFLAGS'])
        targetpath = B.root_build_dir + '/blender'
        launcher_obj = [env.Object(B.root_build_dir + 'source/creator/creator/creator_launch_win', ['#source/creator/creator_launch_win.c'])]
        env.BlenderProg(B.root_build_dir, 'blender', [launcher_obj] + B.resources, [], [], 'blender')

    env.BlenderProg(B.root_build_dir, blender_progname, creob + mainlist + thestatlibs + dobj, thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blender')
if env['WITH_BF_PLAYER']:
    playerlist = B.create_blender_liblist(env, 'player')
    playerlist += B.create_blender_liblist(env, 'player2')
    playerlist += B.create_blender_liblist(env, 'intern')
    playerlist += B.create_blender_liblist(env, 'extern')
    env.BlenderProg(B.root_build_dir, "blenderplayer", dobj + playerlist + thestatlibs, thesyslibs, [B.root_build_dir+'/lib'] + thelibincs, 'blenderplayer')

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
            if '_svn' in dn:
                dn.remove('_svn')
            if '.git' in df:
                df.remove('.git')
            dir=env['BF_INSTALLDIR']+dp[len(bundledir):]
            source=[dp+os.sep+f for f in df]
            blenderinstall.append(env.Install(dir=dir,source=source))
else:
    blenderinstall = env.Install(dir=env['BF_INSTALLDIR'], source=B.program_list)

#-- local path = config files in install dir: installdir\VERSION
#- dont do config and scripts for darwin, it is already in the bundle
dotblendlist = []
datafileslist = []
datafilestargetlist = []
dottargetlist = []
scriptinstall = []
cubininstall = []

if env['OURPLATFORM']!='darwin':
    dotblenderinstall = []
    for targetdir,srcfile in zip(dottargetlist, dotblendlist):
        td, tf = os.path.split(targetdir)
        dotblenderinstall.append(env.Install(dir=td, source=srcfile))
    for targetdir,srcfile in zip(datafilestargetlist, datafileslist):
        td, tf = os.path.split(targetdir)
        dotblenderinstall.append(env.Install(dir=td, source=srcfile))

    if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'win64-vc', 'linuxcross'):
        scriptinstall.append(env.InstallAs(env['BF_INSTALLDIR'] + '/blender-app.exe.manifest',
                                           'source/icons/blender.exe.manifest'))

    if env['WITH_BF_PYTHON']:
        #-- local/VERSION/scripts
        scriptpaths=['release/scripts']
        for scriptpath in scriptpaths:
            for dp, dn, df in os.walk(scriptpath):
                if '.git' in df:
                    df.remove('.git')
                if '__pycache__' in dn:  # py3.2 cache dir
                    dn.remove('__pycache__')

                # only for testing builds
                if VERSION_RELEASE_CYCLE == "release" and "addons_contrib" in dn:
                    dn.remove('addons_contrib')

                # do not install freestyle if disabled
                if not env['WITH_BF_FREESTYLE'] and "freestyle" in dn:
                    dn.remove("freestyle")

                dir = os.path.join(env['BF_INSTALLDIR'], VERSION)
                dir += os.sep + os.path.basename(scriptpath) + dp[len(scriptpath):]

                source=[os.path.join(dp, f) for f in df if not f.endswith(".pyc")]
                # To ensure empty dirs are created too
                if len(source)==0 and not os.path.exists(dir):
                    env.Execute(Mkdir(dir))
                scriptinstall.append(env.Install(dir=dir,source=source))
        if env['WITH_BF_CYCLES']:
            # cycles python code
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles')
            source=os.listdir('intern/cycles/blender/addon')
            if '__pycache__' in source: source.remove('__pycache__')
            source=['intern/cycles/blender/addon/'+s for s in source]
            scriptinstall.append(env.Install(dir=dir,source=source))

            # cycles kernel code
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'kernel')
            source=os.listdir('intern/cycles/kernel')
            if '__pycache__' in source: source.remove('__pycache__')
            source.remove('kernel.cpp')
            source.remove('CMakeLists.txt')
            source.remove('svm')
            source.remove('closure')
            source.remove('geom')
            source.remove('shaders')
            source.remove('osl')
            source=['intern/cycles/kernel/'+s for s in source]
            source.append('intern/cycles/util/util_color.h')
            source.append('intern/cycles/util/util_half.h')
            source.append('intern/cycles/util/util_math.h')
            source.append('intern/cycles/util/util_transform.h')
            source.append('intern/cycles/util/util_types.h')
            scriptinstall.append(env.Install(dir=dir,source=source))
            # svm
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'kernel', 'svm')
            source=os.listdir('intern/cycles/kernel/svm')
            if '__pycache__' in source: source.remove('__pycache__')
            source=['intern/cycles/kernel/svm/'+s for s in source]
            scriptinstall.append(env.Install(dir=dir,source=source))
            # closure
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'kernel', 'closure')
            source=os.listdir('intern/cycles/kernel/closure')
            if '__pycache__' in source: source.remove('__pycache__')
            source=['intern/cycles/kernel/closure/'+s for s in source]
            scriptinstall.append(env.Install(dir=dir,source=source))
            # geom
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'kernel', 'geom')
            source=os.listdir('intern/cycles/kernel/geom')
            if '__pycache__' in source: source.remove('__pycache__')
            source=['intern/cycles/kernel/geom/'+s for s in source]
            scriptinstall.append(env.Install(dir=dir,source=source))

            # licenses
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'license')
            source=os.listdir('intern/cycles/doc/license')
            if '__pycache__' in source: source.remove('__pycache__')
            source.remove('CMakeLists.txt')
            source=['intern/cycles/doc/license/'+s for s in source]
            scriptinstall.append(env.Install(dir=dir,source=source))

    if env['WITH_BF_CYCLES']:
        # cuda binaries
        if env['WITH_BF_CYCLES_CUDA_BINARIES']:
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'lib')
            for arch in env['BF_CYCLES_CUDA_BINARIES_ARCH']:
                kernel_build_dir = os.path.join(B.root_build_dir, 'intern/cycles/kernel')
                cubin_file = os.path.join(kernel_build_dir, "kernel_%s.cubin" % arch)
                cubininstall.append(env.Install(dir=dir,source=cubin_file))

        # osl shaders
        if env['WITH_BF_CYCLES_OSL']:
            dir=os.path.join(env['BF_INSTALLDIR'], VERSION, 'scripts', 'addons','cycles', 'shader')

            osl_source_dir = Dir('./intern/cycles/kernel/shaders').srcnode().path
            oso_build_dir = os.path.join(B.root_build_dir, 'intern/cycles/kernel/shaders')

            headers='node_color.h node_fresnel.h node_texture.h oslutil.h stdosl.h'.split()
            source=['intern/cycles/kernel/shaders/'+s for s in headers]
            scriptinstall.append(env.Install(dir=dir,source=source))

            for f in os.listdir(osl_source_dir):
                if f.endswith('.osl'):
                    oso_file = os.path.join(oso_build_dir, f.replace('.osl', '.oso'))
                    scriptinstall.append(env.Install(dir=dir,source=oso_file))

    if env['WITH_BF_OCIO']:
        colormanagement = os.path.join('release', 'datafiles', 'colormanagement')

        for dp, dn, df in os.walk(colormanagement):
            dir = os.path.join(env['BF_INSTALLDIR'], VERSION, 'datafiles')
            dir += os.sep + os.path.basename(colormanagement) + dp[len(colormanagement):]

            source = [os.path.join(dp, f) for f in df if not f.endswith(".pyc")]

            # To ensure empty dirs are created too
            if len(source) == 0:
                env.Execute(Mkdir(dir))

            scriptinstall.append(env.Install(dir=dir,source=source))

    if env['WITH_BF_INTERNATIONAL']:
        internationalpaths=['release' + os.sep + 'datafiles']

        def check_path(path, member):
            return (member in path.split(os.sep))

        po_dir = os.path.join("release", "datafiles", "locale", "po")

        # font files
        for intpath in internationalpaths:
            for dp, dn, df in os.walk(intpath):
                if '.git' in df:
                    df.remove('.git')

                # we only care about release/datafiles/fonts, release/datafiles/locales
                if check_path(dp, "fonts"):
                    pass
                else:
                    continue

                dir = os.path.join(env['BF_INSTALLDIR'], VERSION)
                dir += os.sep + os.path.basename(intpath) + dp[len(intpath):]

                source=[os.path.join(dp, f) for f in df if not f.endswith(".pyc")]
                # To ensure empty dirs are created too
                if len(source)==0:
                    env.Execute(Mkdir(dir))
                scriptinstall.append(env.Install(dir=dir,source=source))

        # .mo files
        for f in os.listdir(po_dir):
            if not f.endswith(".po"):
                continue

            locale_name = os.path.splitext(f)[0]

            mo_file = os.path.join(B.root_build_dir, "locale", locale_name + ".mo")

            dir = os.path.join(env['BF_INSTALLDIR'], VERSION)
            dir = os.path.join(dir, "datafiles", "locale", locale_name, "LC_MESSAGES")
            scriptinstall.append(env.InstallAs(os.path.join(dir, "blender.mo"), mo_file))

        # languages file
        dir = os.path.join(env['BF_INSTALLDIR'], VERSION)
        dir = os.path.join(dir, "datafiles", "locale")
        languages_file = os.path.join("release", "datafiles", "locale", "languages")
        scriptinstall.append(env.InstallAs(os.path.join(dir, "languages"), languages_file))

#-- icons
if env['OURPLATFORM']=='linux':
    iconlist = []
    icontargetlist = []

    for tp, tn, tf in os.walk('release/freedesktop/icons'):
        for f in tf:
            iconlist.append(os.path.join(tp, f))
            icontargetlist.append( os.path.join(*([env['BF_INSTALLDIR']] + tp.split(os.sep)[2:] + [f])) )

    iconinstall = []
    for targetdir,srcfile in zip(icontargetlist, iconlist):
        td, tf = os.path.split(targetdir)
        iconinstall.append(env.Install(dir=td, source=srcfile))

    scriptinstall.append(env.Install(dir=env['BF_INSTALLDIR'], source='release/bin/blender-thumbnailer.py'))

# dlls for linuxcross
# TODO - add more libs, for now this lets blenderlite run
if env['OURPLATFORM']=='linuxcross':
    dir=env['BF_INSTALLDIR']
    source = []

    if env['WITH_BF_OPENMP']:
        source += ['../lib/windows/pthreads/lib/pthreadGC2.dll']

    scriptinstall.append(env.Install(dir=dir, source=source))

textlist = []
texttargetlist = []
for tp, tn, tf in os.walk('release/text'):
    for f in tf:
        textlist.append(tp+os.sep+f)

# Font licenses
textlist.append('release/datafiles/LICENSE-bfont.ttf.txt')
if env['WITH_BF_INTERNATIONAL']:
    textlist += ['release/datafiles/LICENSE-droidsans.ttf.txt', 'release/datafiles/LICENSE-bmonofont-i18n.ttf.txt']

textinstall = env.Install(dir=env['BF_INSTALLDIR'], source=textlist)

if  env['OURPLATFORM']=='darwin':
        allinstall = [blenderinstall, textinstall]
elif env['OURPLATFORM']=='linux':
        allinstall = [blenderinstall, dotblenderinstall, scriptinstall, textinstall, iconinstall, cubininstall]
else:
        allinstall = [blenderinstall, dotblenderinstall, scriptinstall, textinstall, cubininstall]

if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'win64-vc', 'linuxcross'):
    dllsources = []

    # Used when linking to libtiff was dynamic
    # keep it here until compilation on all platform would be ok
    # dllsources += ['${BF_TIFF_LIBPATH}/${BF_TIFF_LIB}.dll']

    if env['OURPLATFORM'] != 'linuxcross':
        # pthreads library is already added
        dllsources += ['${BF_PTHREADS_LIBPATH}/${BF_PTHREADS_LIB}.dll']

    if env['WITH_BF_SDL']:
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
        dllsources += env['BF_FFMPEG_DLL'].split()

    # Since the thumb handler is loaded by Explorer, architecture is
    # strict: the x86 build fails on x64 Windows. We need to ship
    # both builds in x86 packages.
    if B.bitness == 32:
        dllsources.append('${LCGDIR}/thumbhandler/lib/BlendThumb.dll')
    dllsources.append('${LCGDIR}/thumbhandler/lib/BlendThumb64.dll')

    if env['WITH_BF_OCIO']:
        if not env['OURPLATFORM'] in ('win32-mingw', 'linuxcross'):
            dllsources.append('${LCGDIR}/opencolorio/bin/OpenColorIO.dll')

        else:
            dllsources.append('${LCGDIR}/opencolorio/bin/libOpenColorIO.dll')

    dllsources.append('#source/icons/blender.exe.manifest')

    windlls = env.Install(dir=env['BF_INSTALLDIR'], source = dllsources)
    allinstall += windlls

if env['OURPLATFORM'] == 'win64-mingw':
    dllsources = []

    if env['WITH_BF_PYTHON']:
        if env['BF_DEBUG']:
            dllsources.append('${BF_PYTHON_LIBPATH}/${BF_PYTHON_DLL}_d.dll')
        else:
            dllsources.append('${BF_PYTHON_LIBPATH}/${BF_PYTHON_DLL}.dll')

    if env['WITH_BF_FFMPEG']:
        dllsources += env['BF_FFMPEG_DLL'].split()

    if env['WITH_BF_OPENAL']:
        dllsources.append('${LCGDIR}/openal/lib/OpenAL32.dll')
        dllsources.append('${LCGDIR}/openal/lib/wrap_oal.dll')

    if env['WITH_BF_SNDFILE']:
        dllsources.append('${LCGDIR}/sndfile/lib/libsndfile-1.dll')

    if env['WITH_BF_SDL']:
        dllsources.append('${LCGDIR}/sdl/lib/SDL.dll')

    if(env['WITH_BF_OPENMP']):
        dllsources.append('${LCGDIR}/binaries/libgomp-1.dll')

    if env['WITH_BF_OCIO']:
        dllsources.append('${LCGDIR}/opencolorio/bin/libOpenColorIO.dll')

    dllsources.append('${LCGDIR}/thumbhandler/lib/BlendThumb64.dll')
    dllsources.append('${LCGDIR}/binaries/libgcc_s_sjlj-1.dll')
    dllsources.append('${LCGDIR}/binaries/libwinpthread-1.dll')
    dllsources.append('${LCGDIR}/binaries/libstdc++-6.dll')
    dllsources.append('#source/icons/blender.exe.manifest')

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

buildslave_action = env.Action(btools.buildslave, btools.buildslave_print)
buildslave_cmd = env.Command('buildslave_exec', None, buildslave_action)
buildslave_alias = env.Alias('buildslave', buildslave_cmd)

Depends(buildslave_cmd, allinstall)

cudakernels_action = env.Action(btools.cudakernels, btools.cudakernels_print)
cudakernels_cmd = env.Command('cudakernels_exec', None, cudakernels_action)
cudakernels_alias = env.Alias('cudakernels', cudakernels_cmd)

cudakernel_dir = os.path.join(os.path.abspath(os.path.normpath(B.root_build_dir)), 'intern/cycles/kernel')
cuda_kernels = []

for x in env['BF_CYCLES_CUDA_BINARIES_ARCH']:
    cubin = os.path.join(cudakernel_dir, 'kernel_' + x + '.cubin')
    cuda_kernels.append(cubin)

Depends(cudakernels_cmd, cuda_kernels)
Depends(cudakernels_cmd, cubininstall)

Default(B.program_list)

if not env['WITHOUT_BF_INSTALL']:
        Default(installtarget)

