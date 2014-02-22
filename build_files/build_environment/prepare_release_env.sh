#!/bin/sh

#
# This script will prepare build environment with the same settings as release environment
#
# It will install two chroot environments:
#   - /home/buildbot_squeeze_i686
#   - /home/buildbot_squeeze_x86_64
# which are used for 32bit and 64bit
#
# This sctipt will also create folder /home/sources where all dependent libraries sources are
# downloading and compiling.
#
# Release builder scripts are stored in /home/sources/release-builder
# See build_all.sh script for usage details
#
# This script was tested on debian squeeze and wheezy, should work on ubuntu as well
# It wouldn't work on other distros
#
# TODO:
# - It's still required manual copying of build configuration files to /home/sources/release-builder/config
# - OSL is not set up yet
#

set -e

NO_COLOR='\033[0m'
EWHITE='\033[1;37m'
ERED='\033[1;31m'

CONFIRM="--i-really-do-know-what-im-doing"

ERROR() {
  echo ${ERED}${@}${NO_COLOR}
}

INFO() {
  echo ${EWHITE}${@}${NO_COLOR}
}

if [ $# != 1 ]; then
  ERROR "Usage: $0 $CONFIRM"
  exit 1
fi

if [ "$1" != "$CONFIRM" ]; then
  ERROR "Usage: $0 $CONFIRM"
  exit 1
fi

DEBIAN_BRANCH="squeeze"
DEBIAN_MIRROR="http://ftp.de.debian.org/debian"
USER_ID=1000

# For now it's always /home, so we can setup schroot to map /sources to the same
# path at both host and chroot systems (which is currently needed for release building script)
ENV_PATH="/home"

AMD64_PATH="$ENV_PATH/buildbot_${DEBIAN_BRANCH}_x86_64"
I686_PATH="$ENV_PATH/buildbot_${DEBIAN_BRANCH}_i686"
SOURCES_PATH="$ENV_PATH/sources"

THREADS=$(nproc)

# Force vpx be installed from the backports
VPX_V="1.0.0-2~bpo60+1"

BINUTILS_V="2.22"
BINUTILS_FV="2.22-7.1"

GCC_V="4.7_4.7.1"
GCC_FV="4.7_4.7.1-7"

OPENAL_V="1.14"

DPKG_V="1.16.8"

DEBHELPER_V="9"
DEBHELPER_FV="9.20120909"

JEMALLOC_V="3.1.0"
SPNAV_V="0.2.2"
FFMPEG_V="1.0"
BOOST_V="1_51_0"
PYTHON_V="3.3.0"
PYTHIN_V_SHORT="3.3"
OIIO_V="1.0.9"
OCIO_V="1.0.7"
MESA_V="8.0.5"

OPENSSL_V="0.9.8o"
OPENSSL_FV="0.9.8o-4squeeze13"

CUDA_V="4.2.9"
CUDA_DISTR="ubuntu10.04"
CUDA_32="cudatoolkit_${CUDA_V}_linux_32_${CUDA_DISTR}.run"
CUDA_64="cudatoolkit_${CUDA_V}_linux_64_${CUDA_DISTR}.run"

INSTALL_RELEASE_BUILDER() {
  SOURCES_PATH=$1

  RB=$SOURCES_PATH/release-builder
  if [ ! -d $RB ]; then
    INFO "Installing release building scripts"

    mkdir -p $RB

    cat << EOF > $RB/Readme.txt
This directory contains scrips needed for automated release archive preparation

config/: storage of scons configs for different platforms

build_all.sh: script asks version to add to archive name and revision to compile,
              when script finished, there'll be 32 and 64 bit archives in current directory
              better to run this script from this directory

do_build_all.sh: uses environment variables set by build_all.sh script (or other scripts)
                 and launches compilation inside chroot environments

chroot_compile.py: runs compilation process with giver parameters in chroots
compile.py: script runs inside chroot environment and prepares archive

blender-buildenv.tar.bz2: archive, received from Ken Hughes when i've been preparing
                          new environment to make it as close to old one as it's possible

Hope all this would help you.

-Sergey-
EOF
  
    cat << EOF > $RB/build_all.sh
#!/bin/sh

echo -n "version: "
read version
echo -n "svn revision (blank to latest, 0 to keep unchanged): "
read revision

export version
export revision

d=\`dirname \${0}\`
\${d}/do_build_all.sh
EOF
    chmod +x $RB/build_all.sh

    cat << EOF > $RB/build_all-test.sh
#!/bin/sh

d=\`dirname \${0}\`
\${d}/do_build_all.sh
EOF
    chmod +x $RB/build_all-test.sh

    cat << EOF > $RB/chroot-compile.py
#!/usr/bin/env python

import sys
import os
import platform

from optparse import OptionParser

# This could be passed through options, but does we actually need this?
bl_source = '/home/sources/blender'
build_dir = '/home/sources/blender-build/'
install_dir = '/home/sources/blender-install/'
with_player = True

# Default config
curr_arch = platform.architecture()[0]
def_arch = 'x86_64' if curr_arch == '64bit' else 'i686'
builder_dir = os.path.dirname(os.path.realpath(__file__))

# XXX: bad thing!
# builder_dir = builder_dir.replace("sources-new", "sources")

def_cores = 1
if hasattr(os, 'sysconf'):
    if 'SC_NPROCESSORS_ONLN' in os.sysconf_names:
        def_cores = os.sysconf('SC_NPROCESSORS_ONLN')

# Per-architecture chroot name
chroots = { 'i686': 'buildbot_squeeze_i686',
            'x86_64': 'buildbot_squeeze_x86_64'}

# Parse command line
op = OptionParser()
op.add_option('--tag', default = None)
op.add_option('--branch', default = None)
op.add_option('--arch', default = def_arch)
op.add_option('--cores', default = def_cores)
op.add_option('--bl-version', default = 'UNDEFINED')
op.add_option('--no-clean', default = False)
(opts, args) = op.parse_args()

if opts.arch not in chroots:
    print('Error: No configured machine gound to build ' + 
          '{0} version' . format(opts.arch))
    sys.exit(1)

chroot = chroots[opts.arch]

if opts.tag:
    bl_source = '/home/sources/blender-tags/' + opts.tag
elif opts.branch:
    bl_source = '/home/sources/blender-branches/' + opts.branch

if not os.path.isdir(bl_source):
    print('Uname to find directory with sources: ' + bl_source)
    sys.exit(1)

print('Building {0} version, machine is {1}' . format(opts.bl_version, opts.arch))

# Assume builder directory is binded to the same location in
# chroot environments
compiler = os.path.join(builder_dir, 'compile.py')

cmd = 'schroot -c %s -d /home/sources/release-builder --' % (chroot)
cmd += ' python %s' % (compiler)
cmd += ' --bl-version=%s' % (opts.bl_version)
cmd += ' --bl-source=%s' % (bl_source)
cmd += ' --arch=%s' % (opts.arch)
cmd += ' --build-dir=%s' % (build_dir)
cmd += ' --install-dir=%s' % (install_dir)

if opts.no_clean:
    cmd += ' --no-clean=1'

if with_player:
    cmd += ' --with-player=1'

#if opts.branch:
#    cmd += ' --use-new-ffmpeg=1'

result = os.system(cmd)
if result != 0:
    print('compiler script exited with errcode: %s' % (result))
    sys.exit(1)
EOF
    chmod +x $RB/chroot-compile.py

    cat << EOF > $RB/compile.py
#!/usr/bin/env python

import platform
import sys
import os
import shutil

from optparse import OptionParser

# Default config
curr_arch = platform.architecture()[0]
def_arch = 'x86_64' if curr_arch == '64bit' else 'i686'
builder_dir = os.path.dirname(os.path.realpath(__file__))

def_cores = 1
if hasattr(os, 'sysconf'):
    if 'SC_NPROCESSORS_ONLN' in os.sysconf_names:
        def_cores = os.sysconf('SC_NPROCESSORS_ONLN')

# Parse command line
op = OptionParser()
op.add_option('--arch', default = def_arch)
op.add_option('--cores', default = def_cores)
op.add_option('--no-clean', default = False)
op.add_option('--bl-version', default = 'UNKNOWN')
op.add_option('--bl-source', default = '')
op.add_option('--config-dir', default = '')
op.add_option('--build-dir', default = '')
op.add_option('--install-dir', default = '')
op.add_option('--with-player', default = False)
#op.add_option('--use-new-ffmpeg', default = False)
(opts, args) = op.parse_args()

if opts.config_dir == '':
    opts.config_dir = os.path.join(builder_dir, 'config')

# Initial directory checking (could be problems with permissions)
if not os.path.isdir(opts.bl_source):
    print('Blender\'s source tree not found: %s' % (opts.bl_source))
    sys.exit(1)

if not os.path.isdir(opts.config_dir):
    print('Directory with configuration files not found: %s' % (opts.config_dir))
    sys.exit(1)

if not os.path.isdir(os.path.dirname(opts.build_dir)):
    print('Build directory can\'t be reached: %s' % (opts.build_dir))
    sys.exit(1)

if not os.path.isdir(os.path.dirname(opts.install_dir)):
    print('Install directory can\'t be reached: %s' % (opts.install_dir))
    sys.exit(1)

# Detect glibc version
libc = [name for name in os.listdir('/lib') if 'libc.so.' in name]
if len(libc) == 0:
    print('Could not find "/lib/libc.so.*": cannot determine glibc version')
    sys.exit(-1)

if len(libc) > 1:
    print('warning: found more than one "/lib/libc.so.*": '+
          'using %s' % (libc[0]))

glibc = 'glibc' + os.readlink('/lib/' + libc[0])[5:][:-3].replace('.', '')
glibc = glibc[:8]

# Full name for archive
full_name = 'blender-%s-linux-%s-%s' % (opts.bl_version, glibc, opts.arch)
build_dir = os.path.join(opts.build_dir, full_name)
install_dir = os.path.join(opts.install_dir, full_name)
scons = os.path.join(opts.bl_source, 'scons', 'scons.py')
scons_cmd = 'python %s -C %s' % (scons, opts.bl_source)
config = os.path.join(opts.config_dir, 'user-config-' + glibc + '-' + opts.arch + '.py')

if not os.path.isfile(config):
    print('Configuration file not found: %s' % (config))
    sys.exit(1)

# Clean build directory
if not opts.no_clean:
    print('Cleaning up build directory...')
    os.system('%s BF_BUILDDIR=%s clean ' % (scons_cmd, build_dir))

# Clean install directory
if os.path.isdir(install_dir):
    shutil.rmtree(install_dir)

flags = ""

# Switch to newer libraries if needed
#if opts.use_new_ffmpeg:
#    print("Using new ffmpeg-0.8.1")
#    flags += " BF_FFMPEG='/home/sources/staticlibs/ffmpeg-0.8'"

# Build blenderplayer first
# (to be sure all stuff needed for blender would copied automatically)
if opts.with_player:
    player_config = os.path.join(opts.config_dir,
        'user-config-player-' + glibc + '-' + opts.arch + '.py')

    if not os.path.isfile(player_config):
        print('Player configuration file not found: %s' % (player_config))
        sys.exit(1)

    cmd  = '%s -j %d blenderplayer ' % (scons_cmd, opts.cores + 1)
    cmd += ' BF_BUILDDIR=%s' % (build_dir + '-player')
    cmd += ' BF_INSTALLDIR=%s' % (install_dir)
    cmd += ' BF_CONFIG=%s' % (player_config)
    cmd += flags

    result = os.system(cmd)
    if result != 0:
        print('Compilation failed, exit code is %d' % (result))
        sys.exit(-1)

# Build blender itself
cmd  = '%s -j %d  blender ' % (scons_cmd, opts.cores + 1)
cmd += ' BF_BUILDDIR=%s' % (build_dir)
cmd += ' BF_INSTALLDIR=%s' % (install_dir)
cmd += ' BF_CONFIG=%s' % (config)
cmd += flags

result = os.system(cmd)
if result != 0:
    print('Compilation failed, exit code is %d' % (result))
    sys.exit(-1)

blender = os.path.join(install_dir, 'blender')
blenderplayer = blender + 'player'

if not os.path.exists(blender):
    print('scons completed successfully but blender executable missing')
    sys.exit(1)

if opts.with_player and not os.path.exists(blenderplayer):
    print('scons completed successfully but blenderplayer executable missing')
    sys.exit(1)

# compile python modules
#result = os.system('%s --background --python %s/source/tools/compile_scripts.py' % (blender, opts.bl_source))
#if result != 0:
#    print('Python modules compilation failed, exit code is %d' % (result))
#    sys.exit(-1)

print('build successful')

os.system('strip -s %s %s' % (blender, blenderplayer))

# Copy all texts needed for release
release_texts = os.path.join(opts.bl_source, 'release', 'text', '*')
release_txt = os.path.join(install_dir, 'release_%s.txt' % (opts.bl_version))

os.system('cp -r %s %s' % (release_texts, install_dir))

if os.path.exists(release_txt):
    print 'RELEASE TEXT FOUND'
else:
    print 'WARNING! RELEASE TEXT NOT FOUND!'

# TODO: copy plugins data when ready

# Add software gl libraries and scripts
mesa_arch = None

if opts.arch == 'x86_64':
    mesa_arch = 'mesalibs64.tar.bz2'
elif opts.arch == 'i686':
    mesa_arch = 'mesalibs32.tar.bz2'

if mesa_arch is not None:
    mesalibs = os.path.join(builder_dir, 'extra', mesa_arch)
    software_gl = os.path.join(builder_dir, 'extra', 'blender-softwaregl')

    os.system('tar -xpf %s -C %s' % (mesalibs, install_dir))
    os.system('cp %s %s' % (software_gl, install_dir))
    os.system('chmod 755 %s' % (os.path.join(install_dir, 'blender-softwaregl')))

# Pack release archive
print("Building Dynamic Tarball")
os.system('tar -C %s -cjf %s.tar.bz2 %s ' % (opts.install_dir,
                                             full_name, full_name))

print('Done.')
EOF
    chmod +x $RB/compile.py

    cat << EOF > $RB/do_build_al.sh
#!/bin/sh

SOURCES="/home/sources"

#opts="--cores=1 "
opts=""

if [ "x\${tag}" != "x" ]; then
  echo "Getting tagged source tree..."
  d="\${SOURCES}/blender-tags/\${tag}"
  opts="\${opts} --tag=\${tag}"
  if [ ! -d \${d} ]; then
    svn co https://svn.blender.org/svnroot/bf-blender/tags/\${tag}/blender/@\${revision}  \${d}
  else
    svn up -r \${revision} \${d}
  fi
elif [ "x\${branch}" != "x" ]; then
  echo "Getting branched source tree..."
  d="\${SOURCES}/blender-branches/\${branch}"
  opts="\${opts} --branch=\${branch}"
  if [ ! -d \${d} ]; then
    if [ "x\${revision}" != "x" ]; then
      svn co https://svn.blender.org/svnroot/bf-blender/branches/\${branch}/@\${revision}  \${d}
    else
      svn co https://svn.blender.org/svnroot/bf-blender/branches/\${branch}/  \${d}
    fi
  else
    if [ "x\${revision}" != "x" ]; then
      svn up -r \${revision} \${d}
    else
      svn up \${d}
    fi
  fi
else
  if [ "x\${revision}" != "x" ]; then
    if [ "x\${revision}" != "x0" ]; then
      svn up -r \${revision} \${SOURCES}/blender
    else
      svn up \${SOURCES}/blender
    fi
  fi
fi

if [ "x\${tag}" != "x" ]; then
  b="\${SOURCES}/blender-tags/\${tag}"
elif [ "x\${branch}" != "x" ]; then
  b="\${SOURCES}/blender-branches/\${branch}"
else
  b="\${SOURCES}/blender"
fi

if [ "x\${addons_revision}" != "x" ]; then
  d="\${b}/release/scripts/addons"

  if [ "x\${addons_revision}" != "x0" ]; then
    svn up -r \${addons_revision} \${d}
  else
    svn up \${d}
  fi
fi

if [ "x\${locale_revision}" != "x" ]; then
  d="\${b}/release/datafiles/locale"

  if [ "x\${locale_revision}" != "x0" ]; then
    svn up -r \${locale_revision} \${d}
  else
    svn up \${d}
  fi
fi

if [ -z "\$version" ]; then
  version=r\`/usr/bin/svnversion \$SOURCES/blender\`
fi

cd extra
./update-libs.sh
cd ..

python chroot-compile.py \${opts} --arch=x86_64 --bl-version \${version}  # --no-clean=1
python chroot-compile.py \${opts} --arch=i686 --bl-version \${version}  # --no-clean=1
EOF
    chmod +x $RB/do_build_al.sh

    mkdir -p $RB/extra

    cat << EOF > $RB/extra/blender-softwaregl
#!/bin/sh

BF_DIST_BIN=\`dirname "\$0"\`
BF_PROGRAM="blender" # BF_PROGRAM=\`basename "\$0"\`-bin
exitcode=0

LD_LIBRARY_PATH=\${BF_DIST_BIN}/lib:\${LD_LIBRARY_PATH}

if [ -n "\$LD_LIBRARYN32_PATH" ]; then
    LD_LIBRARYN32_PATH=\${BF_DIST_BIN}/lib:\${LD_LIBRARYN32_PATH}
fi
if [ -n "\$LD_LIBRARYN64_PATH" ]; then
    LD_LIBRARYN64_PATH=\${BF_DIST_BIN}/lib:\${LD_LIBRARYN64_PATH}
fi
if [ -n "\$LD_LIBRARY_PATH_64" ]; then
    LD_LIBRARY_PATH_64=\${BF_DIST_BIN}/lib:\${LD_LIBRARY_PATH_64}
fi

# Workaround for half-transparent windows when compiz is enabled
XLIB_SKIP_ARGB_VISUALS=1

export LD_LIBRARY_PATH LD_LIBRARYN32_PATH LD_LIBRARYN64_PATH LD_LIBRARY_PATH_64 LD_PRELOAD XLIB_SKIP_ARGB_VISUALS

"\$BF_DIST_BIN/\$BF_PROGRAM" \${1+"\$@"}
exitcode=\$?
exit \$exitcode
EOF
    chmod +x $RB/extra/blender-softwaregl

    cat << EOF > $RB/extra/do_update-libs.sh
#!/bin/sh

BITS=\$1
V="\`readlink /opt/lib/mesa | sed -r 's/mesa-//'\`"
TMP=\`mktemp -d\`
N="mesalibs\$1-\$V"

if [ ! -f \$N.tar.bz2 ]; then
  mkdir -p \$TMP/lib
  cp -P /opt/lib/mesa/lib/libGL* \$TMP/lib
  strip -s \$TMP/lib/*
  tar -C \$TMP -cf \$N.tar lib
  bzip2 \$N.tar

  rm -f mesalibs\$BITS.tar,bz2
  ln -s \$N.tar.bz2 mesalibs\$BITS.tar.bz2
fi

rm -rf \$TMP
EOF
    chmod +x $RB/extra/do_update-libs.sh

    cat << EOF > $RB/extra/update-libs.sh
#!/bin/sh

P="/home/sources/release-builder/extra"
CHROOT_PREFIX="buildbot_squeeze_"
CHROOT32="\${CHROOT_PREFIX}i686"
CHROOT64="\${CHROOT_PREFIX}x86_64"
RUN32="schroot -c \$CHROOT32 -d \$P"
RUN64="schroot -c \$CHROOT64 -d \$P"

\$RUN64 ./do_update-libs.sh 64
\$RUN32 ./do_update-libs.sh 32
EOF
    chmod +x $RB/extra/update-libs.sh

    mkdir -p $RB/config
    cp $SOURCES_PATH/blender/build_files/buildbot/config/* $RB/config

  fi
}

INSTALL_SOURCES() {
  SOURCES_PATH=$1

  if [ ! -d "$SOURCES_PATH" ]; then
    INFO "Creating sources directory"
    mkdir -p "$SOURCES_PATH"
  fi

  if [ ! -d "$SOURCES_PATH/backport/binutils" ]; then
    INFO "Downloading binutils"
    mkdir -p "$SOURCES_PATH/backport/binutils"
    wget -c $DEBIAN_MIRROR/pool/main/b/binutils/binutils_$BINUTILS_FV.diff.gz \
            $DEBIAN_MIRROR/pool/main/b/binutils/binutils_$BINUTILS_FV.dsc \
            $DEBIAN_MIRROR/pool/main/b/binutils/binutils_$BINUTILS_V.orig.tar.gz \
        -P "$SOURCES_PATH/backport/binutils"
  fi

  if [ ! -d "$SOURCES_PATH/backport/gcc-4.7" ]; then
    INFO "Downloading gcc-4.7"
    mkdir -p "$SOURCES_PATH/backport/gcc-4.7"
    wget -c $DEBIAN_MIRROR/pool/main/g/gcc-4.7/gcc-$GCC_FV.diff.gz \
            $DEBIAN_MIRROR/pool/main/g/gcc-4.7/gcc-$GCC_FV.dsc \
            $DEBIAN_MIRROR/pool/main/g/gcc-4.7/gcc-$GCC_V.orig.tar.gz \
        -P "$SOURCES_PATH/backport/gcc-4.7"
  fi

  if [ ! -d "$SOURCES_PATH/backport/openssl" ]; then
    INFO "Downloading openssl"
    mkdir -p "$SOURCES_PATH/backport/openssl"
    wget -c $DEBIAN_MIRROR/pool/main/o/openssl/openssl_$OPENSSL_FV.debian.tar.gz \
            $DEBIAN_MIRROR/pool/main/o/openssl/openssl_$OPENSSL_FV.dsc \
            $DEBIAN_MIRROR/pool/main/o/openssl/openssl_$OPENSSL_V.orig.tar.gz \
        -P "$SOURCES_PATH/backport/openssl"
  fi

  # JeMalloc
  J="$SOURCES_PATH/packages/jemalloc-$JEMALLOC_V"
  if [ ! -d "$J" ]; then
    INFO "Downloading jemalloc-$JEMALLOC_V"
    wget -c http://www.canonware.com/download/jemalloc/jemalloc-$JEMALLOC_V.tar.bz2 -P "$SOURCES_PATH/packages"
    tar -C "$SOURCES_PATH/packages" -xf "$J.tar.bz2"
    cat << EOF > "$J/0config.sh"
#!/bin/sh

./configure CC="gcc-4.7 -Wl,--as-needed" CXX="g++-4.7 -Wl,--as-needed" LDFLAGS="-pthread -static-libgcc" --prefix=/opt/lib/jemalloc-$JEMALLOC_V
EOF
    chmod +x "$J/0config.sh"
  fi

  # Spnav
  S="$SOURCES_PATH/packages/libspnav-$SPNAV_V"
  if [ ! -d "$S" ]; then
    wget -c http://downloads.sourceforge.net/project/spacenav/spacenav%20library%20%28SDK%29/libspnav%200.2.2/libspnav-$SPNAV_V.tar.gz \
        -P "$SOURCES_PATH/packages"
    tar -C "$SOURCES_PATH/packages" -xf "$S.tar.gz"
    cat << EOF > "$S/0config.sh"
#!/bin/sh

./configure --prefix=/opt/lib/libspnav-$SPNAV_V
EOF
    chmod +x "$S/0config.sh"
  fi

  # FFmpeg
  F="$SOURCES_PATH/packages/ffmpeg-$FFMPEG_V"
  if [ ! -d "$F" ]; then
    INFO "Downloading FFmpeg-$FFMPEG_V"
    wget -c http://ffmpeg.org/releases/ffmpeg-$FFMPEG_V.tar.bz2 -P "$SOURCES_PATH/packages"
    tar -C "$SOURCES_PATH/packages" -xf "$F.tar.bz2"
    cat << EOF > "$F/0config.sh"
#!/bin/sh

./configure \\
    --cc="/usr/bin/gcc-4.7 -Wl,--as-needed" \\
    --extra-ldflags="-pthread -static-libgcc" \\
    --prefix=/opt/lib/ffmpeg-$FFMPEG_V \\
    --enable-static \\
    --enable-avfilter \\
    --disable-vdpau \\
    --disable-bzlib \\
    --disable-libgsm \\
    --enable-libschroedinger \\
    --disable-libspeex \\
    --enable-libtheora \\
    --enable-libvorbis \\
    --enable-pthreads \\
    --enable-zlib \\
    --enable-libvpx \\
    --enable-stripping \\
    --enable-runtime-cpudetect  \\
    --disable-vaapi \\
    --enable-libopenjpeg \\
    --disable-libfaac \\
    --disable-nonfree \\
    --enable-gpl \\
    --disable-postproc \\
    --disable-x11grab \\
    --enable-libmp3lame \\
    --disable-librtmp \\
    --enable-libx264 \\
    --enable-libxvid \\
    --disable-libopencore-amrnb \\
    --disable-libopencore-amrwb \\
    --disable-libdc1394 \\
    --disable-version3 \\
    --disable-outdev=sdl \\
    --disable-outdev=alsa \\
    --disable-indev=sdl \\
    --disable-indev=alsa \\
    --disable-indev=jack \\
    --disable-indev=lavfi

#    --enable-debug
#    --disable-optimizations
#    --disable-ffplay
EOF
    chmod +x "$F/0config.sh"
  fi

  # Boost
  B="$SOURCES_PATH/packages/boost_$BOOST_V"
  if [ ! -d "$B" ]; then
    INFO "Downloading Boost-$BOOST_V"
    b_d=`echo "$BOOST_V" | sed -r 's/_/./g'`
    wget -c http://sourceforge.net/projects/boost/files/boost/$b_d/boost_$BOOST_V.tar.bz2/download -O "$B.tar.bz2"
    tar -C "$SOURCES_PATH/packages" -xf "$B.tar.bz2"
  fi

  # Python
  P="$SOURCES_PATH/packages/Python-$PYTHON_V"
  if [ ! -d "$P" ]; then
    INFO "Downloading Python-$PYTHON_V"
    wget -c http://python.org/ftp/python/$PYTHON_V/Python-$PYTHON_V.tar.bz2 -P "$SOURCES_PATH/packages"
    tar -C "$SOURCES_PATH/packages" -xf "$P.tar.bz2"
    cat << EOF > "$P/0config.sh"
#!/bin/sh

# NOTE: this sounds strange, but make sure /dev/shm/ is writable by your user,
#        otherwise syncronization primitives wouldn't be included into python
if [[ "\`stat -c '%a' /dev/shm/\`" != "777" ]]; then
  echo "Error checking syncronization primitives"
  exit 1
fi

./configure --prefix=/opt/lib/python-$PYTHON_V \\
  --enable-ipv6 \\
  --enable-loadable-sqlite-extensions \\
  --with-dbmliborder=bdb \\
  --with-wide-unicode \\
  --with-computed-gotos \\
  --with-pymalloc
EOF
    chmod +x "$P/0config.sh"
  fi

  # OpenImageIO
  O="$SOURCES_PATH/packages/OpenImageIO-$OIIO_V"
  if [ ! -d "$O" ]; then
    INFO "Downloading OpenImageIO-$OIIO_V"
    wget -c https://github.com/OpenImageIO/oiio/tarball/Release-$OIIO_V -O "$O.tar.gz"
    tar -C "$SOURCES_PATH/packages" -xf "$O.tar.gz"
    mv $SOURCES_PATH/packages/OpenImageIO-oiio* $O
    mkdir $O/build
    cat << EOF > "$O/build/prepare.sh"
#!/bin/sh

if file /bin/cp | grep -q '32-bit'; then
  cflags="-fPIC -m32 -march=i686"
else
  cflags="-fPIC"
fi

cmake \\
    -D CMAKE_BUILD_TYPE=Release \\
    -D CMAKE_PREFIX_PATH=/opt/lib/oiio-$OIIO_V \\
    -D CMAKE_INSTALL_PREFIX=/opt/lib/oiio-$OIIO_V \\
    -D BUILDSTATIC=ON \\
    -D USE_JASPER=OFF \\
    -D CMAKE_CXX_FLAGS:STRING="\${cflags}" \\
    -D CMAKE_C_FLAGS:STRING="\${cflags}" \\
    -D CMAKE_EXE_LINKER_FLAGS='-lgcc_s -lgcc' \\
    -D BOOST_ROOT=/opt/lib/boost \\
    ../src
EOF
    chmod +x "$O/build/prepare.sh"
  fi

  # OpenColorIO
  O="$SOURCES_PATH/packages/OpenColorIO-$OCIO_V"
  if [ ! -d "$O" ]; then
    INFO "Downloading OpenColorIO-$OCIO_V"
    wget -c http://github.com/imageworks/OpenColorIO/tarball/v$OCIO_V -O "$O.tar.gz"
    tar -C "$SOURCES_PATH/packages" -xf "$O.tar.gz"
    mv $SOURCES_PATH/packages/imageworks-OpenColorIO* $O
    mkdir $O/build
    cat << EOF > "$O/build/prepare.sh"
#!/bin/sh

if file /bin/cp | grep -q '32-bit'; then
  cflags="-fPIC -m32 -march=i686"
else
  cflags="-fPIC"
fi

cmake \\
  -D CMAKE_BUILD_TYPE=Release \\
  -D CMAKE_PREFIX_PATH=/opt/lib/ocio-1.0.7 \\
  -D CMAKE_INSTALL_PREFIX=/opt/lib/ocio-1.0.7 \\
  -D BUILDSTATIC=ON \\
  -D CMAKE_CXX_FLAGS:STRING="\${cflags}" \\
  -D CMAKE_C_FLAGS:STRING="\${cflags}" \\
  -D CMAKE_EXE_LINKER_FLAGS='-lgcc_s -lgcc' \\
  ..
EOF

    chmod +x "$O/build/prepare.sh"
  fi

  # Mesa
  M="$SOURCES_PATH/packages/Mesa-$MESA_V"
  if [ ! -d "$M" ]; then
    INFO "Downloading Mesa-$MESA_V"
    wget -c ftp://ftp.freedesktop.org/pub/mesa/$MESA_V/MesaLib-$MESA_V.tar.bz2 -P "$SOURCES_PATH/packages"
    tar -C "$SOURCES_PATH/packages" -xf "$SOURCES_PATH/packages/MesaLib-$MESA_V.tar.bz2"
    cat << EOF > "$M/0config.sh"
#!/bin/sh

OPTS="--with-driver=xlib \\
  --disable-driglx-direct \\
  --disable-egl \\
  --enable-gallium-gbm=no \\
  --enable-gallium-egl=no \\
  --enable-gallium-llvm=no \\
  --with-gallium-drivers=swrast \\
  --with-dri-drivers=swrast \\
  --prefix=/opt/lib/mesa-$MESA_V"

if file /bin/cp | grep -q '32-bit'; then
  ./configure CC="gcc-4.7 -Wl,--as-needed" CXX="g++-4.7 -Wl,--as-needed" LDFLAGS="-pthread -static-libgcc" \${OPTS} --enable-32-bit #--build=i486-linux-gnu
else
  ./configure CC="gcc-4.7 -Wl,--as-needed" CXX="g++-4.7 -Wl,--as-needed" LDFLAGS="-pthread -static-libgcc" \${OPTS}
fi

EOF
    chmod +x "$M/0config.sh"
  fi

  # OpenAL
  O="$SOURCES_PATH/packages/openal-soft-$OPENAL_V"
  if [ ! -d "$O" ]; then
    INFO "Downloading OpenAL-$OPENAL_V"
    wget -c http://kcat.strangesoft.net/openal-releases/openal-soft-$OPENAL_V.tar.bz2 -P "$SOURCES_PATH/packages"
    tar -C "$SOURCES_PATH/packages" -xf "$SOURCES_PATH/packages/openal-soft-$OPENAL_V.tar.bz2"
    cat << EOF > "$O/build-openal.sh"
#!/bin/sh

DEB_CMAKE_OPTIONS="-DCMAKE_VERBOSE_MAKEFILE=ON \\
    -DCMAKE_INSTALL_PREFIX=/opt/lib/openal-$OPENAL_V \\
    -DCMAKE_BUILD_TYPE:String=Release \\
    -DALSOFT_CONFIG=ON \\
    -DLIBTYPE=STATIC .. "

BUILD_TREE=./build-tree

rm -rf "\${BUILD_TREE}"
mkdir -p "\${BUILD_TREE}"
cd "\${BUILD_TREE}"

sh -c "cmake \`echo \$DEB_CMAKE_OPTIONS\`"
make -j$THREADS
make install
EOF
    chmod +x "$O/build-openal.sh"
  fi

  # OpenCollada
  O="$SOURCES_PATH/packages/opencollada"
  if [ ! -d "$O" ]; then
    INFO "Checking out OpenCollada sources"
    svn co http://opencollada.googlecode.com/svn/trunk $O

    cat << EOF > "$O/build_all.sh"
#!/bin/sh

scons RELEASE=0 NOVALIDATION=1 XMLPARSER=libxmlnative PCRENATIVE=1 SHAREDLIB=0 -j ${THREADS} --clean
scons RELEASE=1 NOVALIDATION=1 XMLPARSER=libxmlnative PCRENATIVE=1 SHAREDLIB=0 -j ${THREADS} --clean

scons RELEASE=0 NOVALIDATION=1 XMLPARSER=libxmlnative PCRENATIVE=1 SHAREDLIB=0 -j ${THREADS}
scons RELEASE=1 NOVALIDATION=1 XMLPARSER=libxmlnative PCRENATIVE=1 SHAREDLIB=0 -j ${THREADS}
EOF

    cat << EOF > "$O/prepare_lib-libxml.sh"
#!/bin/bash

src="./COLLADAStreamWriter/include
./COLLADABaseUtils/include
./COLLADABaseUtils/include/Math
./COLLADAFramework/include
./GeneratedSaxParser/include
./COLLADASaxFrameworkLoader/include
./COLLADASaxFrameworkLoader/include/generated14
./COLLADASaxFrameworkLoader/include/generated15"

if [ -z \$1 ]; then
  arch="x86_64"
else
  arch=\$1
fi

libs="./GeneratedSaxParser/lib/posix/\${arch}/releaselibxml/libGeneratedSaxParser.a
./Externals/MathMLSolver/lib/posix/\${arch}/release/libMathMLSolver.a
./COLLADABaseUtils/lib/posix/\${arch}/release/libOpenCOLLADABaseUtils.a
./COLLADAFramework/lib/posix/\${arch}/release/libOpenCOLLADAFramework.a
./COLLADASaxFrameworkLoader/lib/posix/\${arch}/releaselibxmlNovalidation/libOpenCOLLADASaxFrameworkLoader.a
./COLLADAStreamWriter/lib/posix/\${arch}/release/libOpenCOLLADAStreamWriter.a
./Externals/UTF/lib/posix/\${arch}/release/libUTF.a
./common/libBuffer/lib/posix/\${arch}/release/libbuffer.a
./common/libftoa/lib/posix/\${arch}/release/libftoa.a"

#./Externals/pcre/lib/posix/\${arch}/release/libpcre.a
#./Externals/LibXML/lib/posix/\${arch}/release/libxml.a

debug_libs="./GeneratedSaxParser/lib/posix/\${arch}/debuglibxml/libGeneratedSaxParser.a
./Externals/MathMLSolver/lib/posix/\${arch}/debug/libMathMLSolver.a
./COLLADABaseUtils/lib/posix/\${arch}/debug/libOpenCOLLADABaseUtils.a
./COLLADAFramework/lib/posix/\${arch}/debug/libOpenCOLLADAFramework.a
./COLLADASaxFrameworkLoader/lib/posix/\${arch}/debuglibxmlNovalidation/libOpenCOLLADASaxFrameworkLoader.a
./COLLADAStreamWriter/lib/posix/\${arch}/debug/libOpenCOLLADAStreamWriter.a
./Externals/UTF/lib/posix/\${arch}/debug/libUTF.a
./common/libBuffer/lib/posix/\${arch}/debug/libbuffer.a
./common/libftoa/lib/posix/\${arch}/debug/libftoa.a"

#./Externals/pcre/lib/posix/\${arch}/debug/libpcre.a
#./Externals/LibXML/lib/posix/\${arch}/debug/libxml.a

d="opencollada-libxml"
rm -rf \${d}
mkdir -p \${d}/include

for i in \${src}; do
  mkdir -p \${d}/include/\${i}
  cp \${i}/*.h \${d}/include/\${i}
done

mkdir \${d}/lib
for i in \${libs}; do
  echo "" > /dev/null
  cp \${i} \${d}/lib
done

for i in \${debug_libs}; do
  f=\`basename \${i}\`
  o=\${f/\\.a/_d.a}
  cp \${i} \${d}/lib/\${o}
done

rm -rf /opt/lib/opencollada
mv \${d} /opt/lib/opencollada
chown -R root:staff /opt/lib/opencollada
EOF

    chmod +x "$O/build_all.sh"
    chmod +x "$O/prepare_lib-libxml.sh"
  fi

  # Blender
  B="$SOURCES_PATH/blender"
  if [ ! -d "$B" ]; then
    INFO "Checking out Blender sources"
    svn co https://svn.blender.org/svnroot/bf-blender/trunk/blender $B
  fi

  # CUDA Toolkit
  C=$SOURCES_PATH/cudatoolkit
  if [ ! -f "$C/$CUDA_32" ]; then
    INFO "Downloading CUDA 32bit toolkit"
    mkdir -p $C
    wget -c http://developer.download.nvidia.com/compute/cuda/4_2/rel/toolkit/$CUDA_32 -P $C
  fi

  if [ ! -f "$C/$CUDA_64" ]; then
    INFO "Downloading CUDA 64bit toolkit"
    mkdir -p $C
    wget -c http://developer.download.nvidia.com/compute/cuda/4_2/rel/toolkit/$CUDA_64 -P $C
  fi

  if [ ! -f $SOURCES_PATH/Readme.txt ]; then

    cat << EOF > $SOURCES_PATH/Readme.txt
This directory contains different things needed for Blender builds

blender/: directory with blender's svnsnapshot

blender-build/, blender-install/: build and install directories for
                                  automated release creation

buildbot-i686-slave/,
buildbot-x86_64-slave/: buildbot slave environments for automated builds
                        (maybe it'll be better to move them to /home?)

staticlibs/: set of static libs. Mostly needed to make static linking prioretized
             under dynamic linking

release-builder/: all stuff needed for release archives preparation

Hope all this would help you.

-Sergey-
EOF
  fi

  INSTALL_RELEASE_BUILDER $SOURCES_PATH
}

DO_BACKPORT() {
  CHROOT_ARCH=$1
  CHROOT_PATH=$2

  RUN="chroot $CHROOT_PATH"
  P="/home/sources/backport"

  # Backport fresh binutils
  if [ `$RUN dpkg-query -W -f='${Version}\n' binutils | grep -c $BINUTILS_V` -eq "0" ]; then
    INFO "Backporting binutils"
    B="$P/binutils/binutils-$BINUTILS_V"
    pkg="$P/binutils/binutils_${BINUTILS_FV}_amd64.deb"

    if [ ! -d "$CHROOT_PATH/$B" ]; then
      INFO "Unpacking binutils"
      $RUN dpkg-source -x "$P/binutils/binutils_$BINUTILS_FV.dsc" "$B"
    fi

    if [ "$CHROOT_ARCH" = "i386" ]; then
      pkg=`echo "$pkg" | sed -r 's/amd64/i386/g'`
    fi

    if [ ! -f "$CHROOT_PATH/$pkg" ]; then
      INFO "Compiling binutils"
      sed -ie 's/with_check := yes/with_check := no/' "$CHROOT_PATH/$B/debian/rules"
      $RUN sh -c "cd '$B' && dpkg-buildpackage -rfakeroot -j$THREADS"
    fi

    INFO "Installing binutils"
    $RUN dpkg -i "$pkg"

    INFO "Cleaning binutils"
    $RUN sh -c "cd '$B' && fakeroot debian/rules clean"
  fi

  # Install fresh gcc
  if [ `$RUN dpkg-query -W -f='${Status}\n' gcc-4.7 2> /dev/null | grep -c installed` -eq "0" ]; then
    INFO "Backporting gcc-4.7"
    G="$P/gcc-4.7/gcc-$GCC_V"

    pkg="cpp-4.7_4.7.1-7_amd64.deb  gcc-4.7-base_4.7.1-7_amd64.deb \
          libstdc++6-4.7-dev_4.7.1-7_amd64.deb libstdc++6_4.7.1-7_amd64.deb libgcc1_4.7.1-7_amd64.deb \
          libgomp1_4.7.1-7_amd64.deb libitm1_4.7.1-7_amd64.deb libquadmath0_4.7.1-7_amd64.deb \
          gcc-4.7_4.7.1-7_amd64.deb g++-4.7_4.7.1-7_amd64.deb"

    if [ ! -d "$CHROOT_PATH/$G" ]; then
      INFO "Unpacking gcc-4.7"
      $RUN dpkg-source -x "$P/gcc-4.7/gcc-$GCC_FV.dsc" "$G"
    fi

    if [ "$CHROOT_ARCH" = "i386" ]; then
      pkg=`echo "$pkg" | sed -r 's/amd64/i386/g'`
    fi

    ok=true
    for x in `echo "$pkg"`; do
      if [ ! -f "$CHROOT_PATH/$P/gcc-4.7/$x" ]; then
        ok=false
        break;
      fi
    done

    if ! $ok; then
      INFO "Compiling gcc-4.7"
      sed -ie 's/#with_check := disabled by hand/with_check := disabled by hand/' "$CHROOT_PATH/$G/debian/rules.defs"
      sed -ie 's/dpkg-dev (>= 1.16.0~ubuntu4)/dpkg-dev (>= 1.15.8)/' "$CHROOT_PATH/$G/debian/control"
      sed -ie 's/doxygen (>= 1.7.2)/doxygen (>= 1.7.1)/' "$CHROOT_PATH/$G/debian/control"
      sed -ie 's/libmpfr-dev (>= 3.0.0-9~)/libmpfr-dev (>= 3.0.0)/' "$CHROOT_PATH/$G/debian/control"
      sed -ie 's/libc6-dev (>= 2.13-5)/libc6-dev (>= 2.11.3)/' "$CHROOT_PATH/$G/debian/control"
      sed -ie 's/libgmp-dev (>= 2:5.0.1~)/libgmp3-dev (>= 2:4.3.2)/' "$CHROOT_PATH/$G/debian/control"
      $RUN sh -c "cd '$G' && dpkg-buildpackage -rfakeroot"
    fi

    inst=""
    for x in `echo "$pkg"`; do
      inst="$inst $P/gcc-4.7/$x"
    done

    INFO "Installing gcc-4.7"
    $RUN dpkg -i $inst

    INFO "Cleaning gcc-4.7"
    $RUN sh -c "cd '$G' && fakeroot debian/rules clean"
  fi

  # Backport OpenSSL
  if [ ! -f $CHROOT_PATH/usr/lib/libssl_pic.a ]; then
    INFO "Backporting OpenSSL"
    O="$P/openssl/openssl-$OPENSSL_V"

    pkg="libssl-dev_0.9.8o-4squeeze13_amd64.deb libssl0.9.8_0.9.8o-4squeeze13_amd64.deb  openssl_0.9.8o-4squeeze13_amd64.deb"

    if [ ! -d "$CHROOT_PATH/$O" ]; then
      INFO "Unpacking OpenSSL"
      $RUN dpkg-source -x "$P/openssl/openssl_$OPENSSL_FV.dsc" "$O"
    fi

    if [ "$CHROOT_ARCH" = "i386" ]; then
      pkg=`echo "$pkg" | sed -r 's/amd64/i386/g'`
    fi

    ok=true
    for x in `echo "$pkg"`; do
      if [ ! -f "$CHROOT_PATH/$P/openssl/$x" ]; then
        ok=false
        break;
      fi
    done

    if ! $ok; then
      INFO "Compiling OpenSSL"
      sed -ie 's/#\s*mv debian\/tmp\/usr\/lib\/libcrypto.a debian\/tmp\/usr\/lib\/libcrypto_pic.a/	mv debian\/tmp\/usr\/lib\/libcrypto.a debian\/tmp\/usr\/lib\/libcrypto_pic.a/' "$CHROOT_PATH/$O/debian/rules"
      sed -ie 's/#\s*mv debian\/tmp\/usr\/lib\/libssl.a debian\/tmp\/usr\/lib\/libssl_pic.a/	mv debian\/tmp\/usr\/lib\/libssl.a debian\/tmp\/usr\/lib\/libssl_pic.a/' "$CHROOT_PATH/$O/debian/rules"
      cat << EOF > $CHROOT_PATH/$O/debian/libssl-dev.files
usr/lib/libssl.so
usr/lib/libcrypto.so
usr/lib/libssl.a
usr/lib/libcrypto.a
usr/lib/libssl_pic.a
usr/lib/libcrypto_pic.a
usr/lib/pkgconfig
usr/include
usr/share/man/man3
EOF
      $RUN sh -c "cd '$O' && dpkg-buildpackage -rfakeroot -j$THREADS"
    fi

    inst=""
    for x in `echo "$pkg"`; do
      inst="$inst $P/openssl/$x"
    done

    INFO "Installing OpenSSL"
    $RUN dpkg -i $inst

    echo "openssl hold" | $RUN dpkg --set-selections
    echo "libssl-dev hold" | $RUN dpkg --set-selections
    echo "libssl0.9.8 hold" | $RUN dpkg --set-selections

    INFO "Cleaning OpenSSL"
    $RUN sh -c "cd '$O' && fakeroot debian/rules clean"
  fi
}

DO_COMPILE() {
  CHROOT_ARCH=$1
  CHROOT_PATH=$2

  RUN="chroot $CHROOT_PATH"
  P="/home/sources/packages"
  L="$CHROOT_PATH/opt/lib"

  # JeMalloc
  if [ ! -d "$L/jemalloc-$JEMALLOC_V" ]; then
    INFO "Copmiling jemalloc-$JEMALLOC_V"
    $RUN sh -c "cd '$P/jemalloc-$JEMALLOC_V' && ./0config.sh && make clean && make -j$THREADS && make install && make clean"

    rm -f "$L/jemalloc"
    ln -s "jemalloc-$JEMALLOC_V" "$L/jemalloc"
  fi

  # libspnav
  if [ ! -d "$L/libspnav-$SPNAV_V" ]; then
    INFO "Copmiling libspnav-$SPNAV_V"
    mkdir -p "$L/libspnav-$SPNAV_V/lib"
    mkdir -p "$L/libspnav-$SPNAV_V/include"
    $RUN sh -c "cd '$P/libspnav-$SPNAV_V' && ./0config.sh && make clean && make -j$THREADS && make install && make clean"

    rm -f "$L/libspnav"
    ln -s "libspnav-$SPNAV_V" "$L/libspnav"
  fi

  # FFmpeg
  if [ ! -d "$L/ffmpeg-$FFMPEG_V" ]; then
    INFO "Copmiling ffmpeg-$FFMPEG_V"
    $RUN sh -c "cd '$P/ffmpeg-$FFMPEG_V' && ./0config.sh && make clean && make -j$THREADS && make install && make clean"

    rm -f "$L/ffmpeg"
    ln -s "ffmpeg-$FFMPEG_V" "$L/ffmpeg"
  fi

  # Boost
  V=`echo $BOOST_V | sed -r 's/_/./g'`
  if [ ! -d "$L/boost-$V" ]; then
    INFO "Copmiling boost-$V"
    $RUN sh -c "cd '$P/boost_$BOOST_V' && ./bootstrap.sh && ./b2 --clean && ./b2 install --prefix='/opt/lib/boost-$V' && ./b2 --clean"

    rm -f "$L/boost"
    ln -s "boost-$V" "$L/boost"
  fi

  # OCIO
  if [ ! -d "$L/ocio-$OCIO_V" ]; then
    INFO "Copmiling ocio-$OCIO_V"
    $RUN sh -c "cd '$P/OpenColorIO-$OCIO_V/build' && ./prepare.sh && make clean && make -j$THREADS && make install && make clean"

    # Force linking against sttaic libs
    rm -f $L/ocio-$OCIO_V/lib/*.so*

    # Additional depencencies
    cp $CHROOT_PATH/$P/OpenColorIO-$OCIO_V/build/ext/dist/lib/libtinyxml.a $L/ocio-$OCIO_V/lib
    cp $CHROOT_PATH/$P/OpenColorIO-$OCIO_V/build/ext/dist/lib/libyaml-cpp.a $L/ocio-$OCIO_V/lib

    rm -f "$L/ocio"
    ln -s "ocio-$OCIO_V" "$L/ocio"
  fi

  # OIIO
  if [ ! -d "$L/oiio-$OIIO_V" ]; then
    INFO "Copmiling oiio-$OIIO_V"
    $RUN sh -c "cd '$P/OpenImageIO-$OIIO_V/build' && ./prepare.sh && make clean && make -j$THREADS && make install && make clean"

    rm -f "$L/oiio"
    ln -s "oiio-$OIIO_V" "$L/oiio"
  fi

  # Python
  if [ ! -d "$L/python-$PYTHON_V" ]; then
    INFO "Copmiling Python-$PYTHON_V"

    cat << EOF > $CHROOT_PATH/$P/Python-$PYTHON_V/Modules/Setup.local
_md5 md5module.c

_sha1 sha1module.c
_sha256 sha256module.c
_sha512 sha512module.c
EOF

    sed -ie "s/libraries = \['ssl', 'crypto'\]/libraries = ['ssl_pic', 'crypto_pic', 'z']/" "$P/Python-$PYTHON_V/setup.py"

    $RUN sh -c "cd '$P/Python-$PYTHON_V' && ./0config.sh && make clean && make -j$THREADS && make install && make clean"

    rm -f "$L/python-$PYTHIN_V_SHORT"
    ln -s "python-$PYTHON_V" "$L/python-$PYTHIN_V_SHORT"
  fi

  # Mesa
  if [ ! -d "$L/mesa-$MESA_V" ]; then
    INFO "Copmiling Mesa-$MESA_V"

    $RUN sh -c "cd '$P/Mesa-$MESA_V' && ./0config.sh && make clean && make -j$THREADS && make install && make clean"

    rm -f "$L/mesa"
    ln -s "mesa-$MESA_V" "$L/mesa"
  fi

  # OpenAL
  if [ ! -d "$L/openal-$OPENAL_V" ]; then
    INFO "Copmiling openal-$OPENAL_V"

    $RUN sh -c "cd '$P/openal-soft-$OPENAL_V' && ./build-openal.sh"

    rm -f "$L/openal"
    ln -s "openal-$OPENAL_V" "$L/openal"
  fi

  # OpenCollada
  if [ ! -d "$L/opencollada" ]; then
    INFO "Copmiling opencollada"

    cat << EOF > "$CHROOT_PATH/$P/opencollada/collada.patch"
Index: common/libBuffer/include/CommonBuffer.h
===================================================================
--- common/libBuffer/include/CommonBuffer.h	(revision 876)
+++ common/libBuffer/include/CommonBuffer.h	(working copy)
@@ -12,6 +12,7 @@
 #define __COMMON_BUFFER_H__
 
 #include "CommonIBufferFlusher.h"
+#include "COLLADABUPlatform.h"
 
 namespace Common
 {
Index: common/libBuffer/src/CommonLogFileBufferFlusher.cpp
===================================================================
--- common/libBuffer/src/CommonLogFileBufferFlusher.cpp	(revision 876)
+++ common/libBuffer/src/CommonLogFileBufferFlusher.cpp	(working copy)
@@ -10,6 +10,34 @@
 
 #include "CommonLogFileBufferFlusher.h"
 
+#include <stdio.h>
+#include <errno.h>
+
+#ifndef _WIN32
+FILE *_wfopen(const wchar_t *path, const char *mode)
+{
+	const wchar_t *src = path;
+	char *path_mbs;
+	int n;
+	FILE *file;
+
+	n = (int)wcsrtombs(NULL, &src, 0, NULL);
+
+	if (n < 0)
+		return NULL;
+
+	path_mbs = (char *)malloc(n + 1);
+	wcsrtombs(path_mbs, &path, n, NULL);
+	path_mbs[n] = 0;
+
+	file = fopen(path_mbs, mode);
+
+	free(path_mbs);
+
+	return file;
+}
+#endif
+
 namespace Common
 {
 	//--------------------------------------------------------------------
@@ -35,7 +63,7 @@
 #ifdef _WIN32
 		mError = (int)_wfopen_s( &stream, fileName, L"wb" );
 #else
-		stream = _wfopen( fileName, L"wb" );
+		stream = _wfopen( fileName, "wb" );
 		mError = stream ? 0 : errno;
 #endif
 		if ( !mError )
@@ -65,7 +93,7 @@
 #else
 		if ( mUseWideFileName )
 		{
-			stream = _wfopen( mWideFileName.c_str(), L"a" );
+			stream = _wfopen( mWideFileName.c_str(), "a" );
 		}
 		else
 		{
Index: common/libBuffer/SConscript
===================================================================
--- common/libBuffer/SConscript	(revision 876)
+++ common/libBuffer/SConscript	(working copy)
@@ -11,7 +11,7 @@
 targetPath = outputDir + libName
 
 
-incDirs = ['include/', '../libftoa/include']
+incDirs = ['include/', '../libftoa/include', '../../COLLADABaseUtils/include/', '../../Externals/UTF/include']
 
 
 src = []
EOF

    # We're building in a chroot, architecture of host system would be used by scons
    collada_arch="x86_64"

    $RUN sh -c "cd '$P/opencollada' && svn revert . -R && cat collada.patch | patch -p0 && ./build_all.sh && ./prepare_lib-libxml.sh  $collada_arch"
  fi
}

ADD_REPO() {
  CHROOT_PATH=$1
  DESC=$2
  REPO=$3
  C="$CHROOT_PATH/etc/apt/sources.list"
  RUN="chroot $CHROOT_PATH"

  if [ `cat "$C" | grep -c "$REPO"` -eq "0" ]; then
    INFO "Adding repo $DESC"
    echo "" >> $C
    echo "deb $REPO" >> $C
    echo "deb-src $REPO" >> $C

    INFO "Updating packages list"
    $RUN apt-get update
  fi
}

INSTALL_CHROOT() {
  CHROOT_ARCH=$1
  CHROOT_PATH=$2

  RUN="chroot $CHROOT_PATH"

  # Install fresh debian to a chroot
  if [ ! -d "$CHROOT_PATH" ]; then

    INFO "Installing Debian ${DEBIAN_BRANCH} to ${CHROOT_PATH}"
    debootstrap --arch "${CHROOT_ARCH}" "${DEBIAN_BRANCH}" "${CHROOT_PATH}" "${DEBIAN_MIRROR}"
  fi

  # Configure users and groups

  if [ `cat ${CHROOT_PATH}/etc/group | grep -c developers` -eq "0" ]; then
    INFO "Creating gorup 'developers'"
    $RUN groupadd -g 7001 developers
  fi

  if [ `mount | grep -c "$CHROOT_PATH/dev"` -eq "0" ]; then
    INFO "Mounting devices from host system to chroot"

    mount -t proc none $CHROOT_PATH/proc
    mount -t auto -o bind /dev $CHROOT_PATH/dev
    mount -t devpts -o mode=0620 none $CHROOT_PATH/dev/pts
  fi

  # Configure apt and install packages

  if [ ! -f ${CHROOT_PATH}/etc/apt/apt.conf ]; then
    INFO "Setting up apt to not use recommended packages (saves disk space)"

    cat << EOF > "${CHROOT_PATH}/etc/apt/apt.conf"
APT {
  Default-Release "${DEBIAN_BRANCH}";
  Install-Recommends "0";
};
EOF
  fi

  ADD_REPO $CHROOT_PATH "mirror.yandex.ru" "http://mirror.yandex.ru/debian-multimedia/ squeeze main non-free"
  ADD_REPO $CHROOT_PATH "backports.debian.org" "http://backports.debian.org/debian-backports squeeze-backports main non-free"

  $RUN apt-get upgrade

  $RUN apt-get install -y --force-yes deb-multimedia-keyring libx264-dev libxvidcore4-dev libmp3lame-dev

  if [ `$RUN dpkg-query -W -f='${Status}\n' locales | grep -c not-installed` -eq "1" ]; then
    INFO "Configuring locales"
    $RUN apt-get install -y locales
    $RUN localedef -i en_US -f UTF-8 en_US.UTF-8
  fi

  INFO "Installing packages from repository"

  $RUN apt-get install -y mc gcc g++ cmake python dpkg-dev build-essential autoconf bison \
      flex gettext texinfo dejagnu quilt file lsb-release zlib1g-dev fakeroot debhelper \
      g++-multilib  libtool autoconf2.64 automake  gawk lzma patchutils gperf sharutils \
      libcloog-ppl-dev libmpc-dev libmpfr-dev libgmp3-dev autogen realpath chrpath doxygen \
      graphviz gsfonts-x11 texlive-latex-base libelfg0-dev libx11-dev yasm libopenjpeg-dev \
      libschroedinger-dev libtheora-dev libvorbis-dev libvpx-dev=$VPX_V \
      libopenexr-dev libpng-dev libjpeg-dev libtiff-dev python-dev libbz2-dev libreadline-dev \
      libsqlite3-dev liblzma-dev libncurses5-dev xutils-dev libxext-dev python-libxml2 \
      libglu1-mesa-dev libfftw3-dev libfreetype6-dev libsdl1.2-dev libopenal-dev libjack-dev \
      libxi-dev portaudio19-dev po4a subversion scons libpcre3-dev libexpat1-dev sudo \
      expect bc

  if [ $CHROOT_ARCH = "amd64" ]; then
    $RUN apt-get install -y libc6-dev-i386 lib32gcc1
  fi

  # Configure sources directory
  if [ ! -d "$CHROOT_PATH/home/sources" ]; then
    INFO "Creating sources directory"
    $RUN mkdir "/home/sources"
    $RUN chmod 775 /home/sources
    $RUN chown root:developers /home/sources
  fi

  # Bind directory from host system
  if [ ! -d "$CHROOT_PATH/home/sources/backport" ]; then
    INFO "Binding sources directory from host system to chroot"
    mount -o bind "$SOURCES_PATH" "$CHROOT_PATH/home/sources"
  fi

  if [ "`$RUN getent passwd $USER_ID`" = "" ]; then
    INFO "Adding default user to chroot"
    login=`getent passwd $USER_ID | cut -d : -f 1`
    $RUN useradd -d "/home/$login" -G developers,sudo -m -u $USER_ID "$login"
  fi

  # Backport packages
  DO_BACKPORT "$CHROOT_ARCH" "$CHROOT_PATH"

  # Set default compiler to gcc-4.7
  if [ `readlink "$CHROOT_PATH/usr/bin/gcc"` != "gcc-4.7" ]; then
    INFO "Setting gcc-4.7 as default compiler"
    rm -f $CHROOT_PATH/usr/bin/gcc
    rm -f $CHROOT_PATH/usr/bin/g++
    ln -s gcc-4.7 $CHROOT_PATH/usr/bin/gcc
    ln -s g++-4.7 $CHROOT_PATH/usr/bin/g++
  fi

  # Compile packages
  DO_COMPILE "$CHROOT_ARCH" "$CHROOT_PATH"

  # Install CUDA toolkit
  if [ ! -d "$CHROOT_PATH/usr/local/cuda-$CUDA_V" ]; then
        INFO "Installing CUDA toolkit"

        if [ "$CHROOT_ARCH" = "amd64" ]; then
          C="cudatoolkit_${CUDA_V}_linux_64_${CUDA_DISTR}.run"
        else
          C="cudatoolkit_${CUDA_V}_linux_32_${CUDA_DISTR}.run"
        fi

        rm -f $CHROOT_PATH/usr/local/cuda

        chmod +x $CHROOT_PATH//home/sources/cudatoolkit/$C

        $RUN /usr/bin/expect <<EOF
        spawn /home/sources/cudatoolkit/$C
        expect "Enter install path"
        send "\n"
        expect "Installation Complete"
EOF

    mv $CHROOT_PATH/usr/local/cuda $CHROOT_PATH/usr/local/cuda-$CUDA_V
    ln -s cuda-$CUDA_V $CHROOT_PATH/usr/local/cuda
    sudo ln -s /usr/bin/gcc-4.4 $CHROOT_PATH/usr/local/cuda/bin/gcc
  fi

  # Change permissions
  INFO "Changing permissions on sources"
  login=`$RUN getent passwd $USER_ID | cut -d : -f 1`
  for x in /home/sources /opt/lib; do
    $RUN chmod g+w -R $x
    $RUN chown "$login:developers" -R $x
  done
}

if ! which debootstrap > /dev/null 2>&1; then
  ERROR "debootstrap command not found, can not create chroot environment"
  ERROR "Use apt-get install debootstrap to install debootstrap"
  exit 1
fi

if [ -z "$ENV_PATH" ]; then
  ERROR "Incorrect environment directory is set"
  exit 1
fi

INSTALL_SOURCES "$SOURCES_PATH"
INSTALL_CHROOT amd64 "$AMD64_PATH"
INSTALL_CHROOT i386 "$I686_PATH"

INFO "Configurtion of build environment is completed!"
echo "Add this lines to your /etc/fstab:"
echo

for x in $I686_PATH $AMD64_PATH; do
  echo "none           $x/proc         proc      auto              0    0"
  echo "/dev           $x/dev          auto      bind,auto         0    0"
  echo "none           $x/dev/pts      devpts    mode=0620,auto    0    0"
  echo "/home/sources  $x/home/sources auto      bind,auto         0    0"
  echo
done

echo "Add this lines to your /etc/schroot/schroot.conf:"
echo
login=`getent passwd $USER_ID | cut -d : -f 1`

for x in $I686_PATH $AMD64_PATH; do
  echo [`basename $x`]
  echo "description=Linux buildbot environment"
  echo "directory=$x"
  echo "users=$login"
  echo "root-groups=root"
  echo
done
