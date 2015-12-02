# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import os
import subprocess
import sys
import shutil

# get builder name
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting builder name\n")
    sys.exit(1)

builder = sys.argv[1]

# we run from build/ directory
blender_dir = os.path.join('..', 'blender.git')

if 'cmake' in builder:
    # cmake

    # Some fine-tuning configuration
    blender_dir = os.path.join('..', blender_dir)
    build_dir = os.path.abspath(os.path.join('..', 'build', builder))
    install_dir = os.path.abspath(os.path.join('..', 'install', builder))
    targets = ['blender']

    chroot_name = None  # If not None command will be delegated to that chroot
    build_cubins = True  # Whether to build Cycles CUDA kernels
    remove_cache = False  # Remove CMake cache to be sure config is totally up-to-date
    remove_install_dir = False  # Remove installation folder before building

    # Config file to be used (relative to blender's sources root)
    cmake_config_file = "build_files/cmake/config/blender_full.cmake"
    cmake_player_config_file = None
    cmake_cuda_config_file = None

    # Set build options.
    cmake_options = ['-DCMAKE_BUILD_TYPE:STRING=Release']

    if builder.startswith('mac'):
        # Set up OSX architecture
        if builder.endswith('x86_64_cmake'):
            cmake_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=x86_64')
        elif builder.endswith('i386_cmake'):
            cmake_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=i386')
        elif builder.endswith('ppc_cmake'):
            cmake_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=ppc')

    elif builder.startswith('win'):
        install_dir = None
        if builder.startswith('win64'):
            cmake_options.append(['-G', '"Visual Studio 12 2013 Win64"'])
        elif builder.startswith('win32'):
            cmake_options.append(['-G', '"Visual Studio 12 2013"'])
            build_cubins = False

    elif builder.startswith('linux'):
        remove_cache = True
        remove_install_dir = True
        cmake_config_file = "build_files/buildbot/config/blender_linux.cmake"
        cmake_player_config_file = "build_files/buildbot/config/blender_player_linux.cmake"
        cmake_cuda_config_file = "build_files/buildbot/config/blender_cuda_linux.cmake"
        if builder.endswith('x86_64_cmake'):
            chroot_name = 'buildbot_squeeze_x86_64'
            build_cubins = False
            targets = ['player', 'blender']
        elif builder.endswith('i386_cmake'):
            chroot_name = 'buildbot_squeeze_i686'
            build_cubins = False
            targets = ['player', 'blender']

    cmake_options.append("-DWITH_CYCLES_CUDA_BINARIES=%d" % (build_cubins))

    if install_dir:
        cmake_options.append("-DCMAKE_INSTALL_PREFIX=%s" % (install_dir))

    cmake_options.append("-C" + os.path.join(blender_dir, cmake_config_file))

    # Prepare chroot command prefix if needed

    if chroot_name:
        chroot_prefix = ['schroot', '-c', chroot_name, '--']
    else:
        chroot_prefix = []

    # Make sure no garbage remained from the previous run
    # (only do it if builder requested this)
    if remove_install_dir:
        if os.path.isdir(install_dir):
            shutil.rmtree(install_dir)

    for target in targets:
        print("Building target %s" % (target))
        # Construct build directory name based on the target
        target_build_dir = build_dir
        if target != 'blender':
            target_build_dir += '_' + target
        # Make sure build directory exists and enter it
        if not os.path.isdir(target_build_dir):
            os.mkdir(target_build_dir)
        os.chdir(target_build_dir)
        # Tweaking CMake options to respect the target
        target_cmake_options = cmake_options[:]
        if target == 'player':
            target_cmake_options.append("-C" + os.path.join(blender_dir, cmake_player_config_file))
        elif target == 'cuda':
            target_cmake_options.append("-C" + os.path.join(blender_dir, cmake_cuda_config_file))
        # Configure the build
        print("CMake options:")
        print(target_cmake_options)
        if remove_cache and os.path.exists('CMakeCache.txt'):
            print("Removing CMake cache")
            os.remove('CMakeCache.txt')
        retcode = subprocess.call(chroot_prefix + ['cmake', blender_dir] + target_cmake_options)
        if retcode != 0:
            print('Condifuration FAILED!')
            sys.exit(retcode)

        if 'win32' in builder:
            command = ['msbuild', 'INSTALL.vcxproj', '/Property:PlatformToolset=v120_xp', '/p:Configuration=Release']
        elif 'win64' in builder:
            command = ['msbuild', 'INSTALL.vcxproj', '/p:Configuration=Release']
        else:
            command = chroot_prefix + ['make', '-s', '-j2', 'install']

        print("Executing command:")
        print(command)
        retcode = subprocess.call(command)

        if retcode != 0:
            sys.exit(retcode)
else:
    python_bin = 'python'
    if builder.find('linux') != -1:
        python_bin = '/opt/lib/python-2.7/bin/python2.7'

    # scons
    os.chdir(blender_dir)
    scons_cmd = [python_bin, 'scons/scons.py']
    scons_options = ['BF_FANCY=False']

    # We're using the same rules as release builder, so tweak
    # build and install dirs
    build_dir = os.path.join('..', 'build', builder)
    install_dir = os.path.join('..', 'install', builder)

    # Clean install directory so we'll be sure there's no
    # residual libs and files remained from the previous install.
    if os.path.isdir(install_dir):
        shutil.rmtree(install_dir)

    buildbot_dir = os.path.dirname(os.path.realpath(__file__))
    config_dir = os.path.join(buildbot_dir, 'config')

    if builder.find('linux') != -1:
        configs = []
        if builder.endswith('linux_glibc211_x86_64_scons'):
            configs = ['user-config-player-glibc211-x86_64.py',
                       'user-config-cuda-glibc211-x86_64.py',
                       'user-config-glibc211-x86_64.py'
                       ]
            chroot_name = 'buildbot_squeeze_x86_64'
            cuda_chroot = 'buildbot_squeeze_x86_64'
        elif builder.endswith('linux_glibc211_i386_scons'):
            configs = ['user-config-player-glibc211-i686.py',
                       'user-config-cuda-glibc211-i686.py',
                       'user-config-glibc211-i686.py']
            chroot_name = 'buildbot_squeeze_i686'

            # use 64bit cuda toolkit, so there'll be no memory limit issues
            cuda_chroot = 'buildbot_squeeze_x86_64'

        # Compilation will happen inside of chroot environment
        prog_scons_cmd = ['schroot', '-c', chroot_name, '--'] + scons_cmd
        cuda_scons_cmd = ['schroot', '-c', cuda_chroot, '--'] + scons_cmd

        common_options = ['BF_INSTALLDIR=' + install_dir] + scons_options

        for config in configs:
            config_fpath = os.path.join(config_dir, config)

            scons_options = []

            if config.find('player') != -1:
                scons_options.append('BF_BUILDDIR=%s_player' % (build_dir))
            elif config.find('cuda') != -1:
                scons_options.append('BF_BUILDDIR=%s_cuda' % (build_dir))
            else:
                scons_options.append('BF_BUILDDIR=%s' % (build_dir))

            scons_options += common_options

            if config.find('player') != -1:
                scons_options.append('blenderplayer')
                cur_scons_cmd = prog_scons_cmd
            elif config.find('cuda') != -1:
                scons_options.append('cudakernels')
                cur_scons_cmd = cuda_scons_cmd

                if config.find('i686') != -1:
                    scons_options.append('BF_BITNESS=32')
                elif config.find('x86_64') != -1:
                    scons_options.append('BF_BITNESS=64')
            else:
                scons_options.append('blender')
                cur_scons_cmd = prog_scons_cmd

            scons_options.append('BF_CONFIG=' + config_fpath)

            retcode = subprocess.call(cur_scons_cmd + scons_options)
            if retcode != 0:
                print('Error building rules with config ' + config)
                sys.exit(retcode)

        sys.exit(0)
    else:
        if builder.find('win') != -1:
            bitness = '32'

            if builder.find('win64') != -1:
                bitness = '64'

            scons_options.append('BF_INSTALLDIR=' + install_dir)
            scons_options.append('BF_BUILDDIR=' + build_dir)
            scons_options.append('BF_BITNESS=' + bitness)
            scons_options.append('WITH_BF_CYCLES_CUDA_BINARIES=True')
            scons_options.append('BF_CYCLES_CUDA_NVCC=nvcc.exe')
            if builder.find('mingw') != -1:
                scons_options.append('BF_TOOLSET=mingw')
            if builder.endswith('vc2013'):
                scons_options.append('MSVS_VERSION=12.0')
                scons_options.append('MSVC_VERSION=12.0')
                scons_options.append('WITH_BF_CYCLES_CUDA_BINARIES=1')
                scons_options.append('BF_CYCLES_CUDA_NVCC=nvcc.exe')
            scons_options.append('BF_NUMJOBS=1')

        elif builder.find('mac') != -1:
            if builder.find('x86_64') != -1:
                config = 'user-config-mac-x86_64.py'
            else:
                config = 'user-config-mac-i386.py'

            scons_options.append('BF_CONFIG=' + os.path.join(config_dir, config))

        if builder.find('win') != -1:
            if not os.path.exists(install_dir):
                os.makedirs(install_dir)
            if builder.endswith('vc2013'):
                dlls = ('msvcp120.dll', 'msvcr120.dll', 'vcomp120.dll')
            if builder.find('win64') == -1:
                dlls_path = '..\\..\\..\\redist\\x86'
            else:
                dlls_path = '..\\..\\..\\redist\\amd64'
            for dll in dlls:
                shutil.copyfile(os.path.join(dlls_path, dll), os.path.join(install_dir, dll))

        retcode = subprocess.call([python_bin, 'scons/scons.py'] + scons_options)

        sys.exit(retcode)
