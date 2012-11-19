#!/bin/bash

DISTRO=""
SRC="$HOME/src/blender-deps"
INST="/opt/lib"
CWD=$PWD

# OSL is horror for manual building even
# i would want it to be setteled for manual build first,
# and only then do it automatically
BUILD_OSL=true

THREADS=`cat /proc/cpuinfo | grep cores | uniq | sed -e "s/.*: *\(.*\)/\\1/"`
if [ -z "$THREADS" ]; then
  THREADS=1
fi

PYTHON_VERSION="3.3.0"
PYTHON_VERSION_MIN="3.3"
PYTHON_SOURCE="http://python.org/ftp/python/$PYTHON_VERSION/Python-$PYTHON_VERSION.tar.bz2"

BOOST_VERSION="1.51.0"
_boost_version_nodots=`echo "$BOOST_VERSION" | sed -r 's/\./_/g'`
BOOST_SOURCE="http://sourceforge.net/projects/boost/files/boost/$BOOST_VERSION/boost_$_boost_version_nodots.tar.bz2/download"
BOOST_VERSION_MIN="1.49"

OCIO_VERSION="1.0.7"
OCIO_SOURCE="https://github.com/imageworks/OpenColorIO/tarball/v$OCIO_VERSION"
OCIO_VERSION_MIN="1.0"

OIIO_VERSION="1.1.1"
OIIO_SOURCE="https://github.com/OpenImageIO/oiio/tarball/Release-$OIIO_VERSION"
OIIO_VERSION_MIN="1.1"

LLVM_VERSION="3.1"
LLVM_VERSION_MIN="3.0"
LLVM_VERSION_FOUND=""

# OSL needs to be compiled for now!
OSL_VERSION="1.2.0"
OSL_SOURCE="https://github.com/mont29/OpenShadingLanguage/archive/blender-fixes.tar.gz"

FFMPEG_VERSION="1.0"
FFMPEG_SOURCE="http://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.bz2"
FFMPEG_VERSION_MIN="0.7.6"
_ffmpeg_list_sep=";"

# FFMPEG optional libs.
VORBIS_USE=false
VORBIS_DEV=""
SCHRO_USE=false
SCRHO_DEV=""
THEORA_USE=false
THEORA_DEV=""
XVID_USE=false
XVID_DEV=""
X264_USE=false
X264_DEV=""
X264_VERSION_MIN=0.118
VPX_USE=false
VPX_VERSION_MIN=0.9.7
VPX_DEV=""
MP3LAME_USE=false
MP3LAME_DEV=""
OPENJPEG_USE=false
OPENJPEG_DEV=""

# Switch to english language, else some things (like check_package_DEB()) won't work!
LANG_BACK=$LANG
LANG=""
export LANG

ERROR() {
  echo "${@}"
}

INFO() {
  echo "${@}"
}

# Return 0 if $1 = $2 (i.e. 1.01.0 = 1.1, but 1.1.1 != 1.1), else 1.
# $1 and $2 should be version numbers made of numbers only.
version_eq() {
  backIFS=$IFS
  IFS='.'

  # Split both version numbers into their numeric elements.
  arr1=( $1 )
  arr2=( $2 )

  ret=1

  count1=${#arr1[@]}
  count2=${#arr2[@]}
  if [ $count2 -ge $count1 ]; then
    _t=$count1
    count1=$count2
    count2=$_t
    arr1=( $2 )
    arr2=( $1 )
  fi

  ret=0
  for (( i=0; $i < $count2; i++ ))
  do
    if [ $(( 10#${arr1[$i]} )) -ne $(( 10#${arr2[$i]} )) ]; then
      ret=1
      break
    fi
  done

  for (( i=$count2; $i < $count1; i++ ))
  do
    if [ $(( 10#${arr1[$i]} )) -ne 0 ]; then
      ret=1
      break
    fi
  done

  IFS=$backIFS
  return $ret
}

# Return 0 if $1 >= $2, else 1.
# $1 and $2 should be version numbers made of numbers only.
version_ge() {
  version_eq $1 $2
  if [ $? -eq 1 -a $(echo -e "$1\n$2" | sort --version-sort | head --lines=1) = "$1" ]; then
    return 1
  else
    return 0
  fi
}

# Return 0 if $1 is into $2 (e.g. 3.3.2 is into 3.3, but not 3.3.0 or 3.3.5), else 1.
# $1 and $2 should be version numbers made of numbers only.
# $1 should be at least as long as $2!
version_match() {
  backIFS=$IFS
  IFS='.'

  # Split both version numbers into their numeric elements.
  arr1=( $1 )
  arr2=( $2 )

  ret=1

  count1=${#arr1[@]}
  count2=${#arr2[@]}
  if [ $count1 -ge $count2 ]; then
    ret=0
    for (( i=0; $i < $count2; i++ ))
    do
      if [ $(( 10#${arr1[$i]} )) -ne $(( 10#${arr2[$i]} )) ]; then
        ret=1
        break
      fi
    done
  fi

  IFS=$backIFS
  return $ret
}

detect_distro() {
  if [ -f /etc/debian_version ]; then
    DISTRO="DEB"
  elif [ -f /etc/redhat-release ]; then
    DISTRO="RPM"
  elif [ -f /etc/SuSE-release ]; then
    DISTRO="SUSE"
  fi
}

prepare_opt() {
  INFO "Ensuring $INST exists and is writable by us"
  sudo mkdir -p $INST
  sudo chown $USER $INST
  sudo chmod 775 $INST
}

# Check whether the current package needs to be recompiled, based on a dummy file containing a magic number in its name...
magic_compile_check() {
  if [ -f $INST/.$1-magiccheck-$2 ]; then
    return 0
  else
    return 1
  fi
}

magic_compile_set() {
  rm -f $INST/.$1-magiccheck-*
  touch $INST/.$1-magiccheck-$2
}

compile_Python() {
  # To be changed each time we make edits that would modify the compiled result!
  py_magic=0

  _src=$SRC/Python-$PYTHON_VERSION
  _inst=$INST/python-$PYTHON_VERSION

  # Clean install if needed!
  magic_compile_check python-$PYTHON_VERSION $py_magic
  if [ $? -eq 1 ]; then
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Python-$PYTHON_VERSION"

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      wget -c $PYTHON_SOURCE -O $_src.tar.bz2

      INFO "Unpacking Python-$PYTHON_VERSION"
      tar -C $SRC -xf $_src.tar.bz2
    fi

    cd $_src

    ./configure --prefix=$_inst --enable-ipv6 \
        --enable-loadable-sqlite-extensions --with-dbmliborder=bdb \
        --with-computed-gotos --with-pymalloc

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      rm -f $INST/python-3.3
      ln -s python-$PYTHON_VERSION $INST/python-3.3
    else
      ERROR "Python--$PYTHON_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set python-$PYTHON_VERSION $py_magic

    cd $CWD
  else
    INFO "Own Python-$PYTHON_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, delete the '$_src' and '$_inst' directories."
  fi
}

compile_Boost() {
  # To be changed each time we make edits that would modify the compiled result!
  boost_magic=7

  _src=$SRC/boost-$BOOST_VERSION
  _inst=$INST/boost-$BOOST_VERSION

  # Clean install if needed!
  magic_compile_check boost-$BOOST_VERSION $boost_magic
  if [ $? -eq 1 ]; then
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Boost-$BOOST_VERSION"

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading Boost-$BOOST_VERSION"
      mkdir -p $SRC
      wget -c $BOOST_SOURCE -O $_src.tar.bz2
      tar -C $SRC --transform "s,(.*/?)boost_1_[^/]+(.*),\1boost-$BOOST_VERSION\2,x" -xf $_src.tar.bz2
    fi

    cd $_src
    if [ ! -f $_src/b2 ]; then
      ./bootstrap.sh
    fi
    ./b2 -j$THREADS -a --with-system --with-filesystem --with-thread --with-regex --with-locale --with-date_time \
         --prefix=$_inst --disable-icu boost.locale.icu=off install
    ./b2 --clean

    if [ -d $_inst ]; then
      rm -f $INST/boost
      ln -s boost-$BOOST_VERSION $INST/boost
    else
      ERROR "Boost-$BOOST_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set boost-$BOOST_VERSION $boost_magic

    cd $CWD
  else
    INFO "Own Boost-$BOOST_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, delete the '$_src' and '$_inst' directories."
  fi
}

compile_OCIO() {
  # To be changed each time we make edits that would modify the compiled result!
  ocio_magic=1

  _src=$SRC/OpenColorIO-$OCIO_VERSION
  _inst=$INST/ocio-$OCIO_VERSION

  # Clean install if needed!
  magic_compile_check ocio-$OCIO_VERSION $ocio_magic
  if [ $? -eq 1 ]; then
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenColorIO-$OCIO_VERSION"

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading OpenColorIO-$OCIO_VERSION"
      mkdir -p $SRC
      wget -c $OCIO_SOURCE -O $_src.tar.gz

      INFO "Unpacking OpenColorIO-$OCIO_VERSION"
      tar -C $SRC --transform "s,(.*/?)imageworks-OpenColorIO[^/]*(.*),\1OpenColorIO-$OCIO_VERSION\2,x" \
          -xf $_src.tar.gz
    fi

    cd $_src
    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi    
    mkdir build
    cd build

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake -D CMAKE_BUILD_TYPE=Release \
          -D CMAKE_PREFIX_PATH=$_inst \
          -D CMAKE_INSTALL_PREFIX=$_inst \
          -D CMAKE_CXX_FLAGS="$cflags" \
          -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" \
          -D OCIO_BUILD_APPS=OFF \
          ..

    make -j$THREADS && make install

    # Force linking against static libs
    rm -f $_inst/lib/*.so*

    # Additional depencencies
    cp ext/dist/lib/libtinyxml.a $_inst/lib
    cp ext/dist/lib/libyaml-cpp.a $_inst/lib

    make clean

    if [ -d $_inst ]; then
      rm -f $INST/ocio
      ln -s ocio-$OCIO_VERSION $INST/ocio
    else
      ERROR "OpenColorIO-$OCIO_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set ocio-$OCIO_VERSION $ocio_magic

    cd $CWD
  else
    INFO "Own OpenColorIO-$OCIO_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, delete the '$_src' and '$_inst' directories."
  fi
}

compile_OIIO() {
  # To be changed each time we make edits that would modify the compiled result!
  oiio_magic=5

  _src=$SRC/OpenImageIO-$OIIO_VERSION
  _inst=$INST/oiio-$OIIO_VERSION

  # Clean install if needed!
  magic_compile_check oiio-$OIIO_VERSION $oiio_magic
  if [ $? -eq 1 ]; then
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenImageIO-$OIIO_VERSION"

    prepare_opt

    if [ ! -d $_src ]; then
      wget -c $OIIO_SOURCE -O "$_src.tar.gz"

      INFO "Unpacking OpenImageIO-$OIIO_VERSION"
      tar -C $SRC --transform "s,(.*/?)OpenImageIO-oiio[^/]*(.*),\1OpenImageIO-$OIIO_VERSION\2,x" \
          -xf $_src.tar.gz

      cd $_src

      # XXX Ugly patching hack!
      cat << EOF | patch -p1
diff --git a/src/libutil/SHA1.cpp b/src/libutil/SHA1.cpp
index b9e6c8b..c761185 100644
--- a/src/libutil/SHA1.cpp
+++ b/src/libutil/SHA1.cpp
@@ -8,9 +8,9 @@
 
 // If compiling with MFC, you might want to add #include "StdAfx.h"
 
+#include "SHA1.h"
 #include "hash.h"
 #include "dassert.h"
-#include "SHA1.h"
 
 #ifdef SHA1_UTILITY_FUNCTIONS
 #define SHA1_MAX_FILE_BUFFER 8000
EOF

    fi

    cd $_src
    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi    
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release \
             -D CMAKE_PREFIX_PATH=$_inst \
             -D CMAKE_INSTALL_PREFIX=$_inst \
             -D BUILDSTATIC=ON"

    if [ -d $INST/boost ]; then
      cmake_d="$cmake_d -D BOOST_ROOT=$INST/boost -D Boost_NO_SYSTEM_PATHS=ON"
    fi

    # Looks like we do not need ocio in oiio for now...
#    if [ -d $INST/ocio ]; then
#      cmake_d="$cmake_d -D OCIO_PATH=$INST/ocio"
#    fi

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake $cmake_d -D CMAKE_CXX_FLAGS="$cflags" -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" ../src

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      rm -f $INST/oiio
      ln -s oiio-$OIIO_VERSION $INST/oiio
    else
      ERROR "OpenImageIO-$OIIO_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set oiio-$OIIO_VERSION $oiio_magic

    cd $CWD
  else
    INFO "Own OpenImageIO-$OIIO_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, delete the '$_src' and '$_inst' directories."
  fi
}

compile_OSL() {
  # To be changed each time we make edits that would modify the compiled result!
  osl_magic=5

  _src=$SRC/OpenShadingLanguage-$OSL_VERSION
  _inst=$INST/osl-$OSL_VERSION

  # Clean install if needed!
  magic_compile_check osl-$OSL_VERSION $osl_magic
  if [ $? -eq 1 ]; then
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenShadingLanguage-$OSL_VERSION"

    prepare_opt

    if [ ! -d $_src ]; then
      # XXX Using git on my own repo for now, looks like archives are not updated immediately... :/
#      wget -c $OSL_SOURCE -O "$_src.tar.gz"

#      INFO "Unpacking OpenShadingLanguage-$OSL_VERSION"
#      tar -C $SRC --transform "s,(.*/?)OpenShadingLanguage-[^/]*(.*),\1OpenShadingLanguage-$OSL_VERSION\2,x" \
#          -xf $_src.tar.gz
      git clone https://github.com/mont29/OpenShadingLanguage.git $_src
      cd $_src
      git checkout blender-fixes
      cd $CWD
    fi

    cd $_src
    # XXX For now, always update from latest repo...
    git checkout .

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi    
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D BUILDSTATIC=ON"
    cmake_d="$cmake_d -D BUILD_TESTING=OFF"

    if [ -d $INST/boost ]; then
      cmake_d="$cmake_d -D BOOST_ROOT=$INST/boost -D Boost_NO_SYSTEM_PATHS=ON"
    fi

    if [ -d $INST/oiio ]; then
      cmake_d="$cmake_d -D OPENIMAGEIOHOME=$INST/oiio"
    fi

    if [ ! -z $LLVM_VERSION_FOUND ]; then
      cmake_d="$cmake_d -D LLVM_VERSION=$LLVM_VERSION_FOUND"
    fi

    cmake $cmake_d ../src

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      rm -f $INST/osl
      ln -s osl-$OSL_VERSION $INST/osl
    else
      ERROR "OpenShadingLanguage-$OSL_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set osl-$OSL_VERSION $osl_magic

    cd $CWD
  else
    INFO "Own OpenShadingLanguage-$OSL_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, delete the '$_src' and '$_inst' directories."
  fi
}

compile_FFmpeg() {
  # To be changed each time we make edits that would modify the compiled result!
  ffmpeg_magic=0

  _src=$SRC/ffmpeg-$FFMPEG_VERSION
  _inst=$INST/ffmpeg-$FFMPEG_VERSION

  # Clean install if needed!
  magic_compile_check ffmpeg-$FFMPEG_VERSION $ffmpeg_magic
  if [ $? -eq 1 ]; then
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building ffmpeg-$FFMPEG_VERSION"

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading ffmpeg-$FFMPEG_VERSION"
      wget -c $FFMPEG_SOURCE -O "$_src.tar.bz2"

      INFO "Unpacking ffmpeg-$FFMPEG_VERSION"
      tar -C $SRC -xf $_src.tar.bz2
    fi

    cd $_src

    extra=""

    if $VORBIS_USE; then
      extra="$extra --enable-libvorbis"
    fi

    if $THEORA_USE; then
      extra="$extra --enable-libtheora"
    fi

    if $SCHRO_USE; then
      extra="$extra --enable-libschroedinger"
    fi

    if $XVID_USE; then
      extra="$extra --enable-libxvid"
    fi

    if $X264_USE; then
      extra="$extra --enable-libx264"
    fi

    if $VPX_USE; then
      extra="$extra --enable-libvpx"
    fi

    if $MP3LAME_USE; then
      extra="$extra --enable-libmp3lame"
    fi

    if $OPENJPEG_USE; then
      extra="$extra --enable-libopenjpeg"
    fi

    ./configure --cc="gcc -Wl,--as-needed" --extra-ldflags="-pthread -static-libgcc" \
        --prefix=$_inst --enable-static --enable-avfilter --disable-vdpau \
        --disable-bzlib --disable-libgsm --disable-libspeex \
        --enable-pthreads --enable-zlib --enable-stripping --enable-runtime-cpudetect \
        --disable-vaapi  --disable-libfaac --disable-nonfree --enable-gpl \
        --disable-postproc --disable-x11grab  --disable-librtmp  --disable-libopencore-amrnb \
        --disable-libopencore-amrwb --disable-libdc1394 --disable-version3  --disable-outdev=sdl \
        --disable-outdev=alsa --disable-indev=sdl --disable-indev=alsa --disable-indev=jack \
        --disable-indev=lavfi $extra

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      rm -f $INST/ffmpeg
      ln -s ffmpeg-$FFMPEG_VERSION $INST/ffmpeg
    else
      ERROR "FFmpeg-$FFMPEG_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set ffmpeg-$FFMPEG_VERSION $ffmpeg_magic

    cd $CWD
  else
    INFO "Own ffmpeg-$FFMPEG_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, delete the '$_src' and '$_inst' directories."
  fi
}

get_package_version_DEB() {
    dpkg-query -W -f '${Version}' $1 | sed -r 's/.*:\s*([0-9]+:)(([0-9]+\.?)+).*/\2/'
}

check_package_DEB() {
  r=`apt-cache policy $1 | grep -c 'Candidate:'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_version_match_DEB() {
  v=`apt-cache policy $1 | grep 'Candidate:' | sed -r 's/.*:\s*([0-9]+:)(([0-9]+\.?)+).*/\2/'`

  if [ -z "$v" ]; then
    return 1
  fi

  version_match $v $2
  return $?
}

check_package_version_ge_DEB() {
  v=`apt-cache policy $1 | grep 'Candidate:' | sed -r 's/.*:\s*([0-9]+:)?(([0-9]+\.?)+).*/\2/'`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge $v $2
  return $?
}

install_DEB() {
  INFO ""
  INFO "Installing dependencies for DEB-based distribution"
  INFO "Source code of dependencies needed to be compiled will be downloaded and extracted into $SRC"
  INFO "Built libs of dependencies needed to be compiled will be installed into $INST"
  INFO "Please edit \$SRC and/or \$INST variables at the begining of this script if you want to use other paths!"
  INFO ""

  sudo apt-get update
# XXX Why in hell? Let's let this stuff to the user's responsability!!!
#  sudo apt-get -y upgrade

  # These libs should always be available in debian/ubuntu official repository...
  OPENJPEG_DEV="libopenjpeg-dev"
  SCHRO_DEV="libschroedinger-dev"
  VORBIS_DEV="libvorbis-dev"
  THEORA_DEV="libtheora-dev"

  sudo apt-get install -y gawk cmake scons gcc g++ libjpeg-dev libpng-dev libtiff-dev \
    libfreetype6-dev libx11-dev libxi-dev wget libsqlite3-dev libbz2-dev libncurses5-dev \
    libssl-dev liblzma-dev libreadline-dev $OPENJPEG_DEV libopenexr-dev libopenal-dev \
    libglew-dev yasm $SCHRO_DEV $THEORA_DEV $VORBIS_DEV libsdl1.2-dev \
    libfftw3-dev libjack-dev python-dev patch

  OPENJPEG_USE=true
  SCHRO_USE=true
  VORBIS_USE=true
  THEORA_USE=true

  # Grmpf, debian is libxvidcore-dev and ubuntu libxvidcore4-dev!
  XVID_DEV="libxvidcore-dev"
  check_package_DEB $XVID_DEV
  if [ $? -eq 0 ]; then
    sudo apt-get install -y $XVID_DEV
    XVID_USE=true
  else
    XVID_DEV="libxvidcore4-dev"
    check_package_DEB $XVID_DEV
    if [ $? -eq 0 ]; then
      sudo apt-get install -y $XVID_DEV
      XVID_USE=true
    fi
  fi

  MP3LAME_DEV="libmp3lame-dev"
  check_package_DEB $MP3LAME_DEV
  if [ $? -eq 0 ]; then
    sudo apt-get install -y $MP3LAME_DEV
    MP3LAME_USE=true
  fi

  X264_DEV="libx264-dev"
  check_package_version_ge_DEB $X264_DEV $X264_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo apt-get install -y $X264_DEV
    X264_USE=true
  fi

  VPX_DEV="libvpx-dev"
  check_package_version_ge_DEB $VPX_DEV $VPX_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo apt-get install -y $VPX_DEV
    VPX_USE=true
  fi

  check_package_DEB libspnav-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libspnav-dev
  fi

  check_package_DEB python3.3-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y python3.3-dev
  else
    compile_Python
  fi

  check_package_version_ge_DEB libboost-dev $BOOST_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libboost-dev

    boost_version=$(echo `get_package_version_DEB libboost-dev` | sed -r 's/^([0-9]+\.[0-9]+).*/\1/')

    check_package_DEB libboost-locale$boost_version-dev
    if [ $? -eq 0 ]; then
      sudo apt-get install -y libboost-locale$boost_version-dev libboost-filesystem$boost_version-dev \
                              libboost-regex$boost_version-dev libboost-system$boost_version-dev \
                              libboost-thread$boost_version-dev
    else
      compile_Boost
    fi
  else
    compile_Boost
  fi

  check_package_version_ge_DEB libopencolorio-dev $OCIO_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libopencolorio-dev
  else
    compile_OCIO
  fi

  check_package_version_ge_DEB libopenimageio-dev $OIIO_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libopenimageio-dev
  else
    compile_OIIO
  fi

  if $BUILD_OSL; then
    have_llvm=false

    check_package_DEB llvm-$LLVM_VERSION-dev
    if [ $? -eq 0 ]; then
      sudo apt-get install -y llvm-$LLVM_VERSION-dev
      have_llvm=true
      LLVM_VERSION_FOUND=$LLVM_VERSION
    else
      check_package_DEB llvm-$LLVM_VERSION_MIN-dev
      if [ $? -eq 0 ]; then
        sudo apt-get install -y llvm-$LLVM_VERSION_MIN-dev
        have_llvm=true
        LLVM_VERSION_FOUND=$LLVM_VERSION_MIN
      fi
    fi

    if $have_llvm; then
      sudo apt-get install -y clang flex bison libtbb-dev git
      # No package currently!
      compile_OSL
    fi
  fi

#  XXX Debian features libav packages as ffmpeg, those are not really compatible with blender code currently :/
#      So for now, always build our own ffmpeg.
#  check_package_DEB ffmpeg
#  if [ $? -eq 0 ]; then
#    sudo apt-get install -y ffmpeg
#    ffmpeg_version=`get_package_version_DEB ffmpeg`
#    INFO "ffmpeg version: $ffmpeg_version"
#    if [ ! -z "$ffmpeg_version" ]; then
#      if  dpkg --compare-versions $ffmpeg_version gt 0.7.2; then
#        sudo apt-get install -y libavfilter-dev libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev libswscale-dev
#      else
#        compile_FFmpeg
#      fi
#    fi
#  fi
  compile_FFmpeg
}

get_package_version_RPM() {
  yum info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'
}

check_package_RPM() {
  r=`yum info $1 | grep -c 'Summary'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_version_match_RPM() {
  v=`yum info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'`

  if [ -z "$v" ]; then
    return 1
  fi

  version_match $v $2
  return $?
}

check_package_version_ge_RPM() {
  v=`yum info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge $v $2
  return $?
}

install_RPM() {
  INFO ""
  INFO "Installing dependencies for RPM-based distribution"
  INFO "Source code of dependencies needed to be compiled will be downloaded and extracted into $SRC"
  INFO "Built libs of dependencies needed to be compiled will be installed into $INST"
  INFO "Please edit \$SRC and/or \$INST variables at the begining of this script if you want to use other paths!"
  INFO ""

  sudo yum -y update

  # These libs should always be available in debian/ubuntu official repository...
  OPENJPEG_DEV="openjpeg-devel"
  SCHRO_DEV="schroedinger-devel"
  VORBIS_DEV="libvorbis-devel"
  THEORA_DEV="libtheora-devel"

  sudo yum -y install gawk gcc gcc-c++ cmake scons libpng-devel libtiff-devel \
    freetype-devel libX11-devel libXi-devel wget libsqlite3x-devel ncurses-devel \
    readline-devel $OPENJPEG_DEV openexr-devel openal-soft-devel \
    glew-devel yasm $SCHRO_DEV $THEORA_DEV $VORBIS_DEV SDL-devel \
    fftw-devel lame-libs jack-audio-connection-kit-devel libspnav-devel \
    libjpeg-devel patch python-devel

  OPENJPEG_USE=true
  SCHRO_USE=true
  VORBIS_USE=true
  THEORA_USE=true

  X264_DEV="x264-devel"
  check_package_version_ge_RPM $X264_DEV $X264_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo yum install -y $X264_DEV
    X264_USE=true
  fi

  XVID_DEV="xvidcore-devel"
  check_package_RPM $XVID_DEV
  if [ $? -eq 0 ]; then
    sudo yum install -y $XVID_DEV
    XVID_USE=true
  fi

  VPX_DEV="libvpx-devel"
  check_package_version_ge_RPM $VPX_DEV $VPX_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo yum install -y $VPX_DEV
    VPX_USE=true
  fi

  MP3LAME_DEV="lame-devel"
  check_package_RPM $MP3LAME_DEV
  if [ $? -eq 0 ]; then
    sudo yum install -y $MP3LAME_DEV
    MP3LAME_USE=true
  fi

  check_package_version_match_RPM python3-devel $PYTHON_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo yum install -y python3-devel
  else
    compile_Python
  fi

  check_package_version_ge_RPM boost-devel $BOOST_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo yum install -y boost-devel
  else
    compile_Boost
  fi

  check_package_version_ge_RPM OpenColorIO-devel $OCIO_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo yum install -y OpenColorIO-devel
  else
    compile_OCIO
  fi

  check_package_version_ge_RPM OpenImageIO-devel $OIIO_VERSION_MIN
  if [ $? -eq 0 ]; then
    sudo yum install -y OpenImageIO-devel
  else
    compile_OIIO
  fi

  if $BUILD_OSL; then
    have_llvm=false

    check_package_RPM llvm-$LLVM_VERSION-devel
    if [ $? -eq 0 ]; then
      sudo yum install -y llvm-$LLVM_VERSION-devel
      have_llvm=true
      LLVM_VERSION_FOUND=$LLVM_VERSION
    else
      check_package_RPM llvm-$LLVM_VERSION_MIN-devel
      if [ $? -eq 0 ]; then
        sudo yum install -y llvm-$LLVM_VERSION_MIN-devel
        have_llvm=true
        LLVM_VERSION_FOUND=$LLVM_VERSION_MIN
      else
        check_package_version_ge_RPM llvm-devel $LLVM_VERSION_MIN
        if [ $? -eq 0 ]; then
          sudo yum install -y llvm-devel
          have_llvm=true
          LLVM_VERSION_FOUND=`get_package_version_RPM llvm-devel`
        fi
      fi
    fi

    if $have_llvm; then
      sudo yum install -y flex bison clang tbb-devel git
      # No package currently!
      compile_OSL
    fi
  fi

  # Always for now, not sure which packages should be installed
  compile_FFmpeg
}

check_package_SUSE() {
  r=`zypper info $1 | grep -c 'Summary'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_version_SUSE() {
  v=`zypper info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'`

  # for now major and minor versions only (as if x.y, not x.y.z)
  r=`echo $v | grep -c $2`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

install_SUSE() {
  INFO ""
  INFO "Installing dependencies for SuSE-based distribution"
  INFO "Source code of dependencies needed to be compiled will be downloaded and extracted into $SRC"
  INFO "Built libs of dependencies needed to be compiled will be installed into $INST"
  INFO "Please edit \$SRC and/or \$INST variables at the begining of this script if you want to use other paths!"
  INFO ""

  sudo zypper --non-interactive update --auto-agree-with-licenses

  sudo zypper --non-interactive install --auto-agree-with-licenses \
    gcc gcc-c++ libSDL-devel openal-soft-devel libpng12-devel libjpeg62-devel \
    libtiff-devel OpenEXR-devel yasm libtheora-devel libvorbis-devel cmake \
    scons patch

  check_package_version_SUSE python3-devel 3.3.
  if [ $? -eq 0 ]; then
    sudo zypper --non-interactive install --auto-agree-with-licenses python3-devel
  else
    compile_Python
  fi

  # can not see boost_locale in repo, so let's build own boost
  compile_Boost

  # this libraries are also missing in the repo
  compile_OCIO
  compile_OIIO
  compile_FFmpeg
}

print_info_ffmpeglink_DEB() {
  dpkg -L $_packages | grep -e ".*\/lib[^\/]\+\.so" | gawk '{ printf(nlines ? "'"$_ffmpeg_list_sep"'%s" : "%s", gensub(/.*lib([^\/]+)\.so/, "\\1", "g", $0)); nlines++ }'
}

print_info_ffmpeglink_RPM() {
  rpm -ql $_packages | grep -e ".*\/lib[^\/]\+\.so" | gawk '{ printf(nlines ? "'"$_ffmpeg_list_sep"'%s" : "%s", gensub(/.*lib([^\/]+)\.so/, "\\1", "g", $0)); nlines++ }'
}

print_info_ffmpeglink() {
  # This func must only print a ';'-separated list of libs...
  if [ -z "$DISTRO" ]; then
    ERROR "Failed to detect distribution type"
    exit 1
  fi

  # Create list of packages from which to get libs names...
  _packages=""

  if $THEORA_USE; then
    _packages="$_packages $THEORA_DEV"
  fi

  if $VORBIS_USE; then
    _packages="$_packages $VORBIS_DEV"
  fi

  if $XVID_USE; then
    _packages="$_packages $XVID_DEV"
  fi

  if $VPX_USE; then
    _packages="$_packages $VPX_DEV"
  fi

  if $MP3LAME_USE; then
    _packages="$_packages $MP3LAME_DEV"
  fi

  if $X264_USE; then
    _packages="$_packages $X264_DEV"
  fi

  if $OPENJPEG_USE; then
    _packages="$_packages $OPENJPEG_DEV"
  fi

  if $SCHRO_USE; then
    _packages="$_packages $SCHRO_DEV"
  fi

  if [ "$DISTRO" = "DEB" ]; then
    print_info_ffmpeglink_DEB
  elif [ "$DISTRO" = "RPM" ]; then
    print_info_ffmpeglink_RPM
  # XXX TODO!
  else INFO "<Could not determine additional link libraries needed for ffmpeg, replace this by valid list of libs...>"
#  elif [ "$DISTRO" = "SUSE" ]; then
#    print_info_ffmpeglink_SUSE
  fi
}

print_info() {
  INFO ""
  INFO "If you're using CMake add this to your configuration flags:"

  if [ -d $INST/boost ]; then
    INFO "  -D BOOST_ROOT=$INST/boost"
    INFO "  -D Boost_NO_SYSTEM_PATHS=ON"
  fi

  if [ -d $INST/osl ]; then
    INFO "  -D CYCLES_OSL=$INST/osl"
    INFO "  -D WITH_CYCLES_OSL=ON"
    INFO "  -D LLVM_VERSION=$LLVM_VERSION_FOUND"
  fi

  if [ -d $INST/ffmpeg ]; then
    INFO "  -D WITH_CODEC_FFMPEG=ON"
    INFO "  -D FFMPEG=$INST/ffmpeg"
    INFO "  -D FFMPEG_LIBRARIES='avformat;avcodec;avutil;avdevice;swscale;rt;`print_info_ffmpeglink`'"
  fi

  INFO ""
  INFO "If you're using SCons add this to your user-config:"

  if [ -d $INST/python-3.3 ]; then
    INFO "BF_PYTHON = '$INST/python-3.3'"
    INFO "BF_PYTHON_ABI_FLAGS = 'm'"
  fi

  if [ -d $INST/ocio ]; then
    INFO "BF_OCIO = '$INST/ocio'"
  fi

  if [ -d $INST/oiio ]; then
    INFO "BF_OIIO = '$INST/oiio'"
  fi

  if [ -d $INST/boost ]; then
    INFO "BF_BOOST = '$INST/boost'"
  fi

  if [ -d $INST/ffmpeg ]; then
    INFO "BF_FFMPEG = '$INST/ffmpeg'"
    _ffmpeg_list_sep=" "
    INFO "BF_FFMPEG_LIB = 'avformat avcodec swscale avutil avdevice `print_info_ffmpeglink`'"
  fi

  INFO ""
  INFO ""
  INFO "WARNING: If this script had to build boost into $INST, and you are dynamically linking "
  INFO "         blender against it, you will have to run those commands as root user:"
  INFO ""
  INFO "    echo \"$INST/boost/lib\" > /etc/ld.so.conf.d/boost.conf"
  INFO "    ldconfig"
  INFO ""
}

# Detect distributive type used on this machine
detect_distro

if [ -z "$DISTRO" ]; then
  ERROR "Failed to detect distribution type"
  exit 1
elif [ "$DISTRO" = "DEB" ]; then
  install_DEB
elif [ "$DISTRO" = "RPM" ]; then
  install_RPM
elif [ "$DISTRO" = "SUSE" ]; then
  install_SUSE
fi

print_info

# Switch back to user language.
LANG=LANG_BACK
export LANG
