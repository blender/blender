#!/usr/bin/env bash
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

# A shell script installing/building all needed dependencies to build Blender, for some Linux distributions.

# ----------------------------------------------------------------------------
# Debugging Helpers
#
# Use for developing this script (keep first).

# Useful for debugging this script:
USE_DEBUG_TRAP=${USE_DEBUG_TRAP:-0}
USE_DEBUG_LOG=${USE_DEBUG_LOG:-0}

# Print the line that exits.
if [ $USE_DEBUG_TRAP -ne 0 ]; then
  err_report() {
    echo "Error on line $1"
    exit 1
  }
  trap 'err_report $LINENO' ERR
fi

# Noisy, show every line that runs with it's line number.
if [ $USE_DEBUG_LOG -ne 0 ]; then
  PS4='\e[0;33m$(printf %4d ${LINENO}):\e\033[0m '
  set -x
fi

# ----------------------------------------------------------------------------
# Args and Help Handling

# Parse command line!
ARGS=$( \
getopt \
-o s:i:t:h \
--long source:,install:,tmp:,info:,threads:,help,show-deps,no-sudo,no-build,no-confirm,\
with-all,with-opencollada,with-jack,with-embree,with-oidn,\
ver-ocio:,ver-oiio:,ver-llvm:,ver-osl:,ver-osd:,ver-openvdb:,ver-xr-openxr:,\
force-all,force-python,force-numpy,force-boost,force-tbb,\
force-ocio,force-openexr,force-oiio,force-llvm,force-osl,force-osd,force-openvdb,\
force-ffmpeg,force-opencollada,force-alembic,force-embree,force-oidn,force-usd,\
force-xr-openxr,\
build-all,build-python,build-numpy,build-boost,build-tbb,\
build-ocio,build-openexr,build-oiio,build-llvm,build-osl,build-osd,build-openvdb,\
build-ffmpeg,build-opencollada,build-alembic,build-embree,build-oidn,build-usd,\
build-xr-openxr,\
skip-python,skip-numpy,skip-boost,skip-tbb,\
skip-ocio,skip-openexr,skip-oiio,skip-llvm,skip-osl,skip-osd,skip-openvdb,\
skip-ffmpeg,skip-opencollada,skip-alembic,skip-embree,skip-oidn,skip-usd,\
skip-xr-openxr \
-- "$@" \
)

COMMANDLINE=$@

DISTRO=""
RPM=""
SRC="$HOME/src/blender-deps"
INST="/opt/lib"
TMP="/tmp"
CWD=$PWD
INFO_PATH=$CWD
SCRIPT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Do not install some optional, potentially conflicting libs by default...
WITH_ALL=false

# Do not yet enable opencollada or embree, use --with-opencollada/--with-embree (or --with-all) option to try it.
WITH_OPENCOLLADA=false
WITH_EMBREE=false
WITH_OIDN=false

THREADS=$(nproc)

COMMON_INFO="\"Source code of dependencies needed to be compiled will be downloaded and extracted into '\$SRC'.
Built libs of dependencies needed to be compiled will be installed into '\$INST'.
Please edit \\\$SRC and/or \\\$INST variables at the beginning of this script,
or use --source/--install options, if you want to use other paths!

Number of threads for building: \$THREADS (automatically detected, use --threads=<nbr> to override it).
Full install: \$WITH_ALL (use --with-all option to enable it).
Building OpenCOLLADA: \$WITH_OPENCOLLADA (use --with-opencollada option to enable it).
Building Embree: \$WITH_EMBREE (use --with-embree option to enable it).
Building OpenImageDenoise: \$WITH_OIDN (use --with-oidn option to enable it).

Example:
Full install without OpenCOLLADA: --with-all --skip-opencollada

Use --help to show all available options!\""

ARGUMENTS_INFO="\"COMMAND LINE ARGUMENTS:
    -h, --help
        Show this message and exit.

    --show-deps
        Show main dependencies of Blender (including officially supported versions) and exit.

    -s <path>, --source=<path>
        Use a specific path where to store downloaded libraries sources (defaults to '\$SRC').

    -i <path>, --install=<path>
        Use a specific path where to install built libraries (defaults to '\$INST').

    --tmp=<path>
        Use a specific temp path (defaults to '\$TMP').

    --info=<path>
        Use a specific info path (to store BUILD_NOTES.txt, defaults to '\$INFO_PATH').

    -t n, --threads=n
        Use a specific number of threads when building the libraries (auto-detected as '\$THREADS').

    --no-sudo
        Disable use of sudo (this script won't be able to do much though, will just print needed packages...).

    --no-build
        Do not build (compile) anything, dependencies not installable with the package manager will remain missing.

    --no-confirm
        Disable any interaction with user (suitable for automated run).

    --with-all
        By default, a number of optional and not-so-often needed libraries are not installed.
        This option will try to install them, at the cost of potential conflicts (depending on
        how your package system is set…).
        Note this option also implies all other (more specific) --with-foo options below.

    --with-opencollada
        Build and install the OpenCOLLADA libraries.

    --with-embree
        Build and install the Embree libraries.

    --with-oidn
        Build and install the OpenImageDenoise libraries.

    --with-jack
        Install the jack libraries.

    --ver-ocio=<ver>
        Force version of OCIO library.

    --ver-oiio=<ver>
        Force version of OIIO library.

    --ver-llvm=<ver>
        Force version of LLVM library.

    --ver-osl=<ver>
        Force version of OSL library.

    --ver-osd=<ver>
        Force version of OSD library.

    --ver-openvdb=<ver>
        Force version of OpenVDB library.

    --ver-xr-openxr=<ver>
        Force version of OpenXR-SDK.

    Note about the --ver-foo options:
        It may not always work as expected (some libs are actually checked out from a git rev...), yet it might help
        to fix some build issues (like LLVM mismatch with the version used by your graphic system).

    --build-all
        Force the build of all possible libraries.

    --build-python
        Force the build of Python.

    --build-numpy
        Force the build of NumPy.

    --build-boost
        Force the build of Boost.

    --build-tbb
        Force the build of TBB.

    --build-ocio
        Force the build of OpenColorIO.

    --build-openexr
        Force the build of OpenEXR.

    --build-oiio
        Force the build of OpenImageIO.

    --build-llvm
        Force the build of LLVM.

    --build-osl
        Force the build of OpenShadingLanguage.

    --build-osd
        Force the build of OpenSubdiv.

    --build-openvdb
        Force the build of OpenVDB.

    --build-alembic
        Force the build of Alembic.

    --build-opencollada
        Force the build of OpenCOLLADA.

    --build-embree
        Force the build of Embree.

    --build-oidn
        Force the build of OpenImageDenoise.

    --build-ffmpeg
        Force the build of FFMpeg.

    --build-usd
        Force the build of Universal Scene Description.

    --build-xr-openxr
        Force the build of OpenXR-SDK.

    Note about the --build-foo options:
        * They force the script to prefer building dependencies rather than using available packages.
          This may make things simpler and allow working around some distribution bugs, but on the other hand it will
          use much more space on your hard drive.
        * Please be careful with the Blender building options if you have both 'official' dev packages and
          install_deps' built ones on your system, by default CMake will prefer official packages, which may lead to
          linking issues. Please ensure your CMake configuration always uses all correct library paths.
        * If the “force-built” library is a dependency of others, it will force the build
          of those libraries as well (e.g. --build-boost also implies --build-oiio and --build-osl...).

    --force-all
        Force the rebuild of all built libraries.

    --force-python
        Force the rebuild of Python.

    --force-numpy
        Force the rebuild of NumPy.

    --force-boost
        Force the rebuild of Boost.

    --force-tbb
        Force the rebuild of TBB.

    --force-ocio
        Force the rebuild of OpenColorIO.

    --force-openexr
        Force the rebuild of OpenEXR.

    --force-oiio
        Force the rebuild of OpenImageIO.

    --force-llvm
        Force the rebuild of LLVM.

    --force-osl
        Force the rebuild of OpenShadingLanguage.

    --force-osd
        Force the rebuild of OpenSubdiv.

    --force-openvdb
        Force the rebuild of OpenVDB.

    --force-alembic
        Force the rebuild of Alembic.

    --force-opencollada
        Force the rebuild of OpenCOLLADA.

    --force-embree
        Force the rebuild of Embree.

    --force-oidn
        Force the rebuild of OpenImageDenoise.

    --force-ffmpeg
        Force the rebuild of FFMpeg.

    --force-usd
        Force the rebuild of Universal Scene Description.

    --force-xr-openxr
        Force the rebuild of OpenXR-SDK.

    Note about the --force-foo options:
        * They obviously only have an effect if those libraries are built by this script
          (i.e. if there is no available and satisfactory package)!
        * If the “force-rebuilt” library is a dependency of others, it will force the rebuild
          of those libraries too (e.g. --force-boost will also rebuild oiio and osl...).

    --skip-python
        Unconditionally skip Python installation/building.

    --skip-numpy
        Unconditionally skip NumPy installation/building.

    --skip-boost
        Unconditionally skip Boost installation/building.

    --skip-tbb
        Unconditionally skip TBB installation/building.

    --skip-ocio
        Unconditionally skip OpenColorIO installation/building.

    --skip-openexr
        Unconditionally skip OpenEXR installation/building.

    --skip-oiio
        Unconditionally skip OpenImageIO installation/building.

    --skip-llvm
        Unconditionally skip LLVM installation/building.

    --skip-osl
        Unconditionally skip OpenShadingLanguage installation/building.

    --skip-osd
        Unconditionally skip OpenSubdiv installation/building.

    --skip-openvdb
        Unconditionally skip OpenVDB installation/building.

    --skip-alembic
        Unconditionally skip Alembic installation/building.

    --skip-opencollada
        Unconditionally skip OpenCOLLADA installation/building.

    --skip-Embree
        Unconditionally skip Embree installation/building.

    --skip-oidn
        Unconditionally skip OpenImageDenoise installation/building.

    --skip-ffmpeg
        Unconditionally skip FFMpeg installation/building.

    --skip-usd
        Unconditionally skip Universal Scene Description installation/building.

    --skip-xr-openxr
        Unconditionally skip OpenXR-SDK installation/building.\""

# ----------------------------------------------------------------------------
# Main Vars

DO_SHOW_DEPS=false

SUDO="sudo"

NO_BUILD=false
NO_CONFIRM=false
USE_CXX11=true

CLANG_FORMAT_VERSION_MIN="6.0"

PYTHON_VERSION="3.7.7"
PYTHON_VERSION_MIN="3.7"
PYTHON_VERSION_INSTALLED=$PYTHON_VERSION_MIN
PYTHON_FORCE_BUILD=false
PYTHON_FORCE_REBUILD=false
PYTHON_SKIP=false

NUMPY_VERSION="1.17.0"
NUMPY_VERSION_MIN="1.8"
NUMPY_FORCE_BUILD=false
NUMPY_FORCE_REBUILD=false
NUMPY_SKIP=false

BOOST_VERSION="1.70.0"
BOOST_VERSION_MIN="1.49"
BOOST_FORCE_BUILD=false
BOOST_FORCE_REBUILD=false
BOOST_SKIP=false

TBB_VERSION="2019"
TBB_VERSION_UPDATE="_U9"  # Used for source packages...
TBB_VERSION_MIN="2018"
TBB_FORCE_BUILD=false
TBB_FORCE_REBUILD=false
TBB_SKIP=false

OCIO_VERSION="1.1.0"
OCIO_VERSION_MIN="1.0"
OCIO_FORCE_BUILD=false
OCIO_FORCE_REBUILD=false
OCIO_SKIP=false

OPENEXR_VERSION="2.4.0"
OPENEXR_VERSION_MIN="2.3"
OPENEXR_FORCE_BUILD=false
OPENEXR_FORCE_REBUILD=false
OPENEXR_SKIP=false
_with_built_openexr=false

OIIO_VERSION="1.8.13"
OIIO_VERSION_MIN="1.8.13"
OIIO_VERSION_MAX="99.99.0"  # UNKNOWN currently # Not supported by current OSL...
OIIO_FORCE_BUILD=false
OIIO_FORCE_REBUILD=false
OIIO_SKIP=false

LLVM_VERSION="9.0.1"
LLVM_VERSION_MIN="6.0"
LLVM_VERSION_FOUND=""
LLVM_FORCE_BUILD=false
LLVM_FORCE_REBUILD=false
LLVM_SKIP=false

# OSL needs to be compiled for now!
OSL_VERSION="1.10.9"
OSL_VERSION_MIN=$OSL_VERSION
OSL_FORCE_BUILD=false
OSL_FORCE_REBUILD=false
OSL_SKIP=false

# OpenSubdiv needs to be compiled for now
OSD_VERSION="3.4.3"
OSD_VERSION_MIN=$OSD_VERSION
OSD_FORCE_BUILD=false
OSD_FORCE_REBUILD=false
OSD_SKIP=false

# OpenVDB needs to be compiled for now
OPENVDB_BLOSC_VERSION="1.5.0"

OPENVDB_VERSION="7.0.0"
OPENVDB_VERSION_MIN=$OPENVDB_VERSION
OPENVDB_FORCE_BUILD=false
OPENVDB_FORCE_REBUILD=false
OPENVDB_SKIP=false

# Alembic needs to be compiled for now
ALEMBIC_VERSION="1.7.12"
ALEMBIC_VERSION_MIN=$ALEMBIC_VERSION
ALEMBIC_FORCE_BUILD=false
ALEMBIC_FORCE_REBUILD=false
ALEMBIC_SKIP=false

USD_VERSION="20.02"
USD_FORCE_BUILD=false
USD_FORCE_REBUILD=false
USD_SKIP=false

OPENCOLLADA_VERSION="1.6.68"
OPENCOLLADA_FORCE_BUILD=false
OPENCOLLADA_FORCE_REBUILD=false
OPENCOLLADA_SKIP=false

EMBREE_VERSION="3.10.0"
EMBREE_FORCE_BUILD=false
EMBREE_FORCE_REBUILD=false
EMBREE_SKIP=false

OIDN_VERSION="1.0.0"
OIDN_FORCE_BUILD=false
OIDN_FORCE_REBUILD=false
OIDN_SKIP=false

FFMPEG_VERSION="4.2.3"
FFMPEG_VERSION_MIN="2.8.4"
FFMPEG_FORCE_BUILD=false
FFMPEG_FORCE_REBUILD=false
FFMPEG_SKIP=false
_ffmpeg_list_sep=";"

XR_OPENXR_VERSION="1.0.8"
XR_OPENXR_FORCE_BUILD=false
XR_OPENXR_FORCE_REBUILD=false
XR_OPENXR_SKIP=false

# FFMPEG optional libs.
VORBIS_USE=false
VORBIS_DEV=""
OGG_USE=false
OGG_DEV=""
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
OPUS_USE=false
OPUS_VERSION_MIN=1.1.1
OPUS_DEV=""
MP3LAME_USE=false
MP3LAME_DEV=""
OPENJPEG_USE=false
OPENJPEG_DEV=""

# Whether to use system GLEW or not (OpenSubDiv needs recent glew to work).
NO_SYSTEM_GLEW=false

# Switch to english language, else some things (like check_package_DEB()) won't work!
LANG_BACK=$LANG
LANG=""
export LANG

# ----------------------------------------------------------------------------
# Generic Helpers

BLACK=$(tput setaf 0)
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
LIME_YELLOW=$(tput setaf 190)
POWDER_BLUE=$(tput setaf 153)
BLUE=$(tput setaf 4)
MAGENTA=$(tput setaf 5)
CYAN=$(tput setaf 6)
WHITE=$(tput setaf 7)
BRIGHT=$(tput bold)
NORMAL=$(tput sgr0)
BLINK=$(tput blink)
REVERSE=$(tput smso)
UNDERLINE=$(tput smul)

_echo() {
  if [ "X$1" = "X-n" ]; then
     shift; printf "%s" "$@"
  else
     printf "%s\n" "$@"
  fi
}

ERROR() {
  _echo "${BRIGHT}${RED}ERROR! ${NORMAL}${RED}$@${NORMAL}"
}

WARNING() {
  _echo "${BRIGHT}${YELLOW}WARNING! ${NORMAL}${YELLOW}$@${NORMAL}"
}

INFO() {
  _echo "${GREEN}$@${NORMAL}"
}

PRINT() {
  _echo "$@"
}

# ----------------------------------------------------------------------------
# Args Handling

# Finish parsing the commandline args.
eval set -- "$ARGS"
while true; do
  case $1 in
    -s|--source)
      SRC="$2"; shift; shift; continue
    ;;
    -i|--install)
      INST="$2"; shift; shift; continue
    ;;
    --tmp)
      TMP="$2"; shift; shift; continue
    ;;
    --info)
      INFO_PATH="$2"; shift; shift; continue
    ;;
    -t|--threads)
      THREADS="$2"; shift; shift; continue
    ;;
    -h|--help)
      PRINT ""
      PRINT "USAGE:"
      PRINT ""
      PRINT "`eval _echo "$COMMON_INFO"`"
      PRINT ""
      PRINT "`eval _echo "$ARGUMENTS_INFO"`"
      PRINT ""
      exit 0
    ;;
    --show-deps)
      # We have to defer...
      DO_SHOW_DEPS=true; shift; continue
    ;;
    --no-sudo)
      PRINT ""
      WARNING "--no-sudo enabled, this script might not be able to do much..."
      PRINT ""
      SUDO=""; shift; continue
    ;;
    --no-build)
      PRINT ""
      WARNING "--no-build enabled, this script will not be able to install all dependencies..."
      PRINT ""
      NO_BUILD=true; shift; continue
    ;;
    --no-confirm)
      NO_CONFIRM=true; shift; continue
    ;;
    --with-all)
      WITH_ALL=true; shift; continue
    ;;
    --with-opencollada)
      WITH_OPENCOLLADA=true; shift; continue
    ;;
    --with-embree)
      WITH_EMBREE=true; shift; continue
    ;;
    --with-oidn)
      WITH_OIDN=true; shift; continue
    ;;
    --with-jack)
      WITH_JACK=true; shift; continue;
    ;;
    --ver-ocio)
      OCIO_VERSION="$2"
      OCIO_VERSION_MIN=$OCIO_VERSION
      shift; shift; continue
    ;;
    --ver-oiio)
      OIIO_VERSION="$2"
      OIIO_VERSION_MIN=$OIIO_VERSION
      shift; shift; continue
    ;;
    --ver-llvm)
      LLVM_VERSION="$2"
      LLVM_VERSION_MIN=$LLVM_VERSION
      shift; shift; continue
    ;;
    --ver-osl)
      OSL_VERSION="$2"
      OSL_VERSION_MIN=$OSL_VERSION
      shift; shift; continue
    ;;
    --ver-osd)
      OSD_VERSION="$2"
      OSD_VERSION_MIN=$OSD_VERSION
      shift; shift; continue
    ;;
    --ver-openvdb)
      OPENVDB_VERSION="$2"
      OPENVDB_VERSION_MIN=$OPENVDB_VERSION
      shift; shift; continue
    ;;
    --ver-xr-openxr)
      XR_OPENXR_VERSION="$2"
      XR_OPENXR_VERSION_MIN=$XR_OPENXR_VERSION
      shift; shift; continue
    ;;
    --build-all)
      PYTHON_FORCE_BUILD=true
      NUMPY_FORCE_BUILD=true
      BOOST_FORCE_BUILD=true
      TBB_FORCE_BUILD=true
      OCIO_FORCE_BUILD=true
      OPENEXR_FORCE_BUILD=true
      OIIO_FORCE_BUILD=true
      LLVM_FORCE_BUILD=true
      OSL_FORCE_BUILD=true
      OSD_FORCE_BUILD=true
      OPENVDB_FORCE_BUILD=true
      OPENCOLLADA_FORCE_BUILD=true
      EMBREE_FORCE_BUILD=true
      OIDN_FORCE_BUILD=true
      FFMPEG_FORCE_BUILD=true
      ALEMBIC_FORCE_BUILD=true
      USD_FORCE_BUILD=true
      XR_OPENXR_FORCE_BUILD=true
      shift; continue
    ;;
    --build-python)
      PYTHON_FORCE_BUILD=true
      NUMPY_FORCE_BUILD=true
      shift; continue
    ;;
    --build-numpy)
      PYTHON_FORCE_BUILD=true
      NUMPY_FORCE_BUILD=true
      shift; continue
    ;;
    --build-boost)
      BOOST_FORCE_BUILD=true; shift; continue
    ;;
    --build-tbb)
      TBB_FORCE_BUILD=true; shift; continue
    ;;
    --build-ocio)
      OCIO_FORCE_BUILD=true; shift; continue
    ;;
    --build-openexr)
      OPENEXR_FORCE_BUILD=true; shift; continue
    ;;
    --build-oiio)
      OIIO_FORCE_BUILD=true; shift; continue
    ;;
    --build-llvm)
      LLVM_FORCE_BUILD=true; shift; continue
    ;;
    --build-osl)
      OSL_FORCE_BUILD=true; shift; continue
    ;;
    --build-osd)
      OSD_FORCE_BUILD=true; shift; continue
    ;;
    --build-openvdb)
      OPENVDB_FORCE_BUILD=true; shift; continue
    ;;
    --build-opencollada)
      OPENCOLLADA_FORCE_BUILD=true; shift; continue
    ;;
    --build-embree)
      EMBREE_FORCE_BUILD=true; shift; continue
    ;;
    --build-oidn)
      OIDN_FORCE_BUILD=true; shift; continue
    ;;
    --build-ffmpeg)
      FFMPEG_FORCE_BUILD=true; shift; continue
    ;;
    --build-alembic)
      ALEMBIC_FORCE_BUILD=true; shift; continue
    ;;
    --build-usd)
      USD_FORCE_BUILD=true; shift; continue
    ;;
    --build-xr-openxr)
      XR_OPENXR_FORCE_BUILD=true; shift; continue
    ;;
    --force-all)
      PYTHON_FORCE_REBUILD=true
      NUMPY_FORCE_REBUILD=true
      BOOST_FORCE_REBUILD=true
      TBB_FORCE_REBUILD=true
      OCIO_FORCE_REBUILD=true
      OPENEXR_FORCE_REBUILD=true
      OIIO_FORCE_REBUILD=true
      LLVM_FORCE_REBUILD=true
      OSL_FORCE_REBUILD=true
      OSD_FORCE_REBUILD=true
      OPENVDB_FORCE_REBUILD=true
      OPENCOLLADA_FORCE_REBUILD=true
      EMBREE_FORCE_REBUILD=true
      OIDN_FORCE_REBUILD=true
      FFMPEG_FORCE_REBUILD=true
      ALEMBIC_FORCE_REBUILD=true
      USD_FORCE_REBUILD=true
      XR_OPENXR_FORCE_REBUILD=true
      shift; continue
    ;;
    --force-python)
      PYTHON_FORCE_REBUILD=true
      NUMPY_FORCE_REBUILD=true
      shift; continue
    ;;
    --force-numpy)
      NUMPY_FORCE_REBUILD=true; shift; continue
    ;;
    --force-boost)
      BOOST_FORCE_REBUILD=true; shift; continue
    ;;
    --force-tbb)
      TBB_FORCE_REBUILD=true; shift; continue
    ;;
    --force-ocio)
      OCIO_FORCE_REBUILD=true; shift; continue
    ;;
    --force-openexr)
      OPENEXR_FORCE_REBUILD=true; shift; continue
    ;;
    --force-oiio)
      OIIO_FORCE_REBUILD=true; shift; continue
    ;;
    --force-llvm)
      LLVM_FORCE_REBUILD=true; shift; continue
    ;;
    --force-osl)
      OSL_FORCE_REBUILD=true; shift; continue
    ;;
    --force-osd)
      OSD_FORCE_REBUILD=true; shift; continue
    ;;
    --force-openvdb)
      OPENVDB_FORCE_REBUILD=true; shift; continue
    ;;
    --force-opencollada)
      OPENCOLLADA_FORCE_REBUILD=true; shift; continue
    ;;
    --force-embree)
      EMBREE_FORCE_REBUILD=true; shift; continue
    ;;
    --force-oidn)
      OIDN_FORCE_REBUILD=true; shift; continue
    ;;
    --force-ffmpeg)
      FFMPEG_FORCE_REBUILD=true; shift; continue
    ;;
    --force-alembic)
      ALEMBIC_FORCE_REBUILD=true; shift; continue
    ;;
    --force-usd)
      USD_FORCE_REBUILD=true; shift; continue
    ;;
    --force-xr-openxr)
      XR_OPENXR_FORCE_REBUILD=true; shift; continue
    ;;
    --skip-python)
      PYTHON_SKIP=true; shift; continue
    ;;
    --skip-numpy)
      NUMPY_SKIP=true; shift; continue
    ;;
    --skip-boost)
      BOOST_SKIP=true; shift; continue
    ;;
    --skip-tbb)
      TBB_SKIP=true; shift; continue
    ;;
    --skip-ocio)
      OCIO_SKIP=true; shift; continue
    ;;
    --skip-openexr)
      OPENEXR_SKIP=true; shift; continue
    ;;
    --skip-oiio)
      OIIO_SKIP=true; shift; continue
    ;;
    --skip-llvm)
      LLVM_SKIP=true; shift; continue
    ;;
    --skip-osl)
      OSL_SKIP=true; shift; continue
    ;;
    --skip-osd)
      OSD_SKIP=true; shift; continue
    ;;
    --skip-openvdb)
      OPENVDB_SKIP=true; shift; continue
    ;;
    --skip-opencollada)
      OPENCOLLADA_SKIP=true; shift; continue
    ;;
    --skip-embree)
      EMBREE_SKIP=true; shift; continue
    ;;
    --skip-oidn)
      OIDN_SKIP=true; shift; continue
    ;;
    --skip-ffmpeg)
      FFMPEG_SKIP=true; shift; continue
    ;;
    --skip-alembic)
      ALEMBIC_SKIP=true; shift; continue
    ;;
    --skip-usd)
      USD_SKIP=true; shift; continue
    ;;
    --skip-xr-openxr)
      XR_OPENXR_SKIP=true; shift; continue
    ;;
    --)
      # no more arguments to parse
      break
    ;;
    *)
      PRINT ""
      ERROR "Wrong parameter '$1'; Usage:"
      PRINT ""
      PRINT "`eval _echo "$COMMON_INFO"`"
      PRINT ""
      exit 1
    ;;
  esac
done

if [ "$WITH_ALL" = true -a "$OPENCOLLADA_SKIP" = false ]; then
  WITH_OPENCOLLADA=true
fi
if [ "$WITH_ALL" = true -a "$EMBREE_SKIP" = false ]; then
  WITH_EMBREE=true
fi
if [ "$WITH_ALL" = true -a "$OIDN_SKIP" = false ]; then
  WITH_OIDN=true
fi
if [ "$WITH_ALL" = true ]; then
  WITH_JACK=true
fi


WARNING "****WARNING****"
PRINT "If you are experiencing issues building Blender, _*TRY A FRESH, CLEAN BUILD FIRST*_!"
PRINT "The same goes for install_deps itself, if you encounter issues, please first erase everything in $SRC and $INST"
PRINT "(provided obviously you did not add anything yourself in those dirs!), and run install_deps.sh again!"
PRINT "Often, changes in the libs built by this script, or in your distro package, cannot be handled simply, so..."
PRINT ""
PRINT "You may also try to use the '--build-foo' options to bypass your distribution's packages"
PRINT "for some troublesome/buggy libraries..."
PRINT ""
PRINT ""
PRINT "Ran with:"
PRINT "    install_deps.sh $COMMANDLINE"
PRINT ""
PRINT ""


# This has to be done here, because user might force some versions...
PYTHON_SOURCE=( "https://www.python.org/ftp/python/$PYTHON_VERSION/Python-$PYTHON_VERSION.tgz" )
NUMPY_SOURCE=( "https://github.com/numpy/numpy/releases/download/v$NUMPY_VERSION/numpy-$NUMPY_VERSION.tar.gz" )

_boost_version_nodots=`echo "$BOOST_VERSION" | sed -r 's/\./_/g'`
BOOST_SOURCE=( "https://dl.bintray.com/boostorg/release/$BOOST_VERSION/source/boost_$_boost_version_nodots.tar.bz2" )
BOOST_BUILD_MODULES="--with-system --with-filesystem --with-thread --with-regex --with-locale --with-date_time --with-wave --with-iostreams --with-python --with-program_options"

TBB_SOURCE=( "https://github.com/oneapi-src/oneTBB/archive/$TBB_VERSION$TBB_VERSION_UPDATE.tar.gz" )
TBB_SOURCE_CMAKE=( "https://raw.githubusercontent.com/wjakob/tbb/master/CMakeLists.txt" )

OCIO_USE_REPO=false
OCIO_SOURCE=( "https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/v$OCIO_VERSION.tar.gz")
#~ OCIO_SOURCE_REPO=( "https://github.com/imageworks/OpenColorIO.git" )
#~ OCIO_SOURCE_REPO_UID="6de971097c7f552300f669ed69ca0b6cf5a70843"

OPENEXR_USE_REPO=false
OPENEXR_SOURCE=( "https://github.com/AcademySoftwareFoundation/openexr/archive/v$OPENEXR_VERSION.tar.gz" )
OPENEXR_SOURCE_REPO=( "https://github.com/AcademySoftwareFoundation/openexr.git" )
OPENEXR_SOURCE_REPO_UID="0ac2ea34c8f3134148a5df4052e40f155b76f6fb"
#~ OPENEXR_SOURCE=( "https://github.com/openexr/openexr/archive/$OPENEXR_SOURCE_REPO_UID.tar.gz" )

OIIO_USE_REPO=false
OIIO_SOURCE=( "https://github.com/OpenImageIO/oiio/archive/Release-$OIIO_VERSION.tar.gz" )
#~ OIIO_SOURCE_REPO=( "https://github.com/OpenImageIO/oiio.git" )
#~ OIIO_SOURCE_REPO_UID="c9e67275a0b248ead96152f6d2221cc0c0f278a4"

_LLVM_SOURCE_ROOT="https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION"
LLVM_SOURCE=( "$_LLVM_SOURCE_ROOT/llvm-$LLVM_VERSION.src.tar.xz" )
LLVM_CLANG_SOURCE=( "$_LLVM_SOURCE_ROOT/clang-$LLVM_VERSION.src.tar.xz" "$_LLVM_SOURCE_ROOT/cfe-$LLVM_VERSION.src.tar.xz" )

OSL_USE_REPO=false
OSL_SOURCE=( "https://github.com/imageworks/OpenShadingLanguage/archive/Release-$OSL_VERSION.tar.gz" )
#~ OSL_SOURCE_REPO=( "https://github.com/imageworks/OpenShadingLanguage.git" )
#~ OSL_SOURCE_REPO_BRANCH="master"
#~ OSL_SOURCE_REPO_UID="85179714e1bc69cd25ecb6bb711c1a156685d395"
#~ OSL_SOURCE=( "https://github.com/Nazg-Gul/OpenShadingLanguage/archive/Release-1.5.11.tar.gz" )
#~ OSL_SOURCE_REPO=( "https://github.com/mont29/OpenShadingLanguage.git" )
#~ OSL_SOURCE_REPO_UID="85179714e1bc69cd25ecb6bb711c1a156685d395"
#~ OSL_SOURCE_REPO=( "https://github.com/Nazg-Gul/OpenShadingLanguage.git" )
#~ OSL_SOURCE_REPO_UID="7d40ff5fe8e47b030042afb92d0e955f5aa96f48"
#~ OSL_SOURCE_REPO_BRANCH="blender-fixes"

OSD_USE_REPO=false
# Script foo to make the version string compliant with the archive name:
# ${Varname//SearchForThisChar/ReplaceWithThisChar}
OSD_SOURCE=( "https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v${OSD_VERSION//./_}.tar.gz" )
#~ OSD_SOURCE_REPO=( "https://github.com/PixarAnimationStudios/OpenSubdiv.git" )
#~ OSD_SOURCE_REPO_UID="404659fffa659da075d1c9416e4fc939139a84ee"
#~ OSD_SOURCE_REPO_BRANCH="dev"

OPENVDB_USE_REPO=false
OPENVDB_BLOSC_SOURCE=( "https://github.com/Blosc/c-blosc/archive/v${OPENVDB_BLOSC_VERSION}.tar.gz" )
OPENVDB_SOURCE=( "https://github.com/dreamworksanimation/openvdb/archive/v${OPENVDB_VERSION}.tar.gz" )
#~ OPENVDB_SOURCE_REPO=( "https:///dreamworksanimation/openvdb.git" )
#~ OPENVDB_SOURCE_REPO_UID="404659fffa659da075d1c9416e4fc939139a84ee"
#~ OPENVDB_SOURCE_REPO_BRANCH="dev"

ALEMBIC_USE_REPO=false
ALEMBIC_SOURCE=( "https://github.com/alembic/alembic/archive/${ALEMBIC_VERSION}.tar.gz" )
# ALEMBIC_SOURCE_REPO=( "https://github.com/alembic/alembic.git" )
# ALEMBIC_SOURCE_REPO_UID="e6c90d4faa32c4550adeaaf3f556dad4b73a92bb"
# ALEMBIC_SOURCE_REPO_BRANCH="master"

USD_SOURCE=( "https://github.com/PixarAnimationStudios/USD/archive/v${USD_VERSION}.tar.gz" )

OPENCOLLADA_USE_REPO=false
OPENCOLLADA_SOURCE=( "https://github.com/KhronosGroup/OpenCOLLADA/archive/v${OPENCOLLADA_VERSION}.tar.gz" )
#~ OPENCOLLADA_SOURCE_REPO=( "https://github.com/KhronosGroup/OpenCOLLADA.git" )
#~ OPENCOLLADA_REPO_UID="e937c3897b86fc0da53cde97257f5156"
#~ OPENCOLLADA_REPO_BRANCH="master"

EMBREE_USE_REPO=false
EMBREE_SOURCE=( "https://github.com/embree/embree/archive/v${EMBREE_VERSION}.tar.gz" )
#~ EMBREE_SOURCE_REPO=( "https://github.com/embree/embree.git" )
#~ EMBREE_REPO_UID="4a12bfed63c90e85b6eab98b8cdd8dd2a3ba5809"
#~ EMBREE_REPO_BRANCH="master"

OIDN_USE_REPO=false
OIDN_SOURCE=( "https://github.com/OpenImageDenoise/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.src.tar.gz" )
#~ OIDN_SOURCE_REPO=( "https://github.com/OpenImageDenoise/oidn.git" )
#~ OIDN_REPO_UID="dabfd9c80101edae9d25a710160d12d6d963c591"
#~ OIDN_REPO_BRANCH="master"

FFMPEG_SOURCE=( "http://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.bz2" )

XR_OPENXR_USE_REPO=false
XR_OPENXR_SOURCE=("https://github.com/KhronosGroup/OpenXR-SDK/archive/release-${XR_OPENXR_VERSION}.tar.gz")
#~ XR_OPENXR_SOURCE_REPO=("https://github.com/KhronosGroup/OpenXR-SDK-Source.git")
#~ XR_OPENXR_REPO_UID="5292e57fda47561e672fba0a4b6e545c0f25dd8d"
#~ XR_OPENXR_REPO_BRANCH="master"

# C++11 is required now
CXXFLAGS_BACK=$CXXFLAGS
CXXFLAGS="$CXXFLAGS -std=c++11"
export CXXFLAGS

# ----------------------------------------------------------------------------
# Show Dependencies

# Need those to be after we defined versions...
DEPS_COMMON_INFO="\"COMMON DEPENDENCIES:

Those libraries should be available as packages in all recent distributions (optional ones are [between brackets]):

    * Basics of dev environment (cmake, gcc, svn , git, ...).
    * libjpeg, libpng, libtiff, [openjpeg2], [libopenal].
    * libx11, libxcursor, libxi, libxrandr, libxinerama (and other libx... as needed).
    * libsqlite3, libbz2, libssl, libfftw3, libxml2, libtinyxml, yasm, libyaml-cpp.
    * libsdl2, libglew, [libglewmx].\""

DEPS_SPECIFIC_INFO="\"BUILDABLE DEPENDENCIES:

The following libraries will probably not all be available as packages in your distribution
(install_deps will by default try to install packages, and fall back to building missing ones).
You can force install_deps to build those with '--build-all' or relevant 'build-foo' options, see '--help' message.
You may also want to build them yourself (optional ones are [between brackets]):

    * Python $PYTHON_VERSION_MIN (from $PYTHON_SOURCE).
    * [NumPy $NUMPY_VERSION_MIN] (from $NUMPY_SOURCE).
    * Boost $BOOST_VERSION_MIN (from $BOOST_SOURCE, modules: $BOOST_BUILD_MODULES).
    * TBB $TBB_VERSION_MIN (from $TBB_SOURCE).
    * [FFMpeg $FFMPEG_VERSION_MIN (needs libvorbis, libogg, libtheora, libx264, libmp3lame, libxvidcore, libvpx, ...)] (from $FFMPEG_SOURCE).
    * [OpenColorIO $OCIO_VERSION_MIN] (from $OCIO_SOURCE).
    * ILMBase $OPENEXR_VERSION_MIN (from $OPENEXR_SOURCE).
    * OpenEXR $OPENEXR_VERSION_MIN (from $OPENEXR_SOURCE).
    * OpenImageIO $OIIO_VERSION_MIN (from $OIIO_SOURCE).
    * [LLVM $LLVM_VERSION_MIN (with clang)] (from $LLVM_SOURCE, and $LLVM_CLANG_SOURCE).
    * [OpenShadingLanguage $OSL_VERSION_MIN] (from $OSL_SOURCE_REPO, branch $OSL_SOURCE_REPO_BRANCH, commit $OSL_SOURCE_REPO_UID).
    * [OpenSubDiv $OSD_VERSION_MIN] (from $OSD_SOURCE_REPO, branch $OSD_SOURCE_REPO_BRANCH, commit $OSD_SOURCE_REPO_UID).
    * [OpenVDB $OPENVDB_VERSION_MIN] (from $OPENVDB_SOURCE), [Blosc $OPENVDB_BLOSC_VERSION] (from $OPENVDB_BLOSC_SOURCE).
    * [OpenCollada $OPENCOLLADA_VERSION] (from $OPENCOLLADA_SOURCE).
    * [Embree $EMBREE_VERSION] (from $EMBREE_SOURCE).
    * [OpenImageDenoise $OIDN_VERSION] (from $OIDN_SOURCE).
    * [Alembic $ALEMBIC_VERSION] (from $ALEMBIC_SOURCE).
    * [Universal Scene Description $USD_VERSION] (from $USD_SOURCE).
    * [OpenXR-SDK $XR_OPENXR_VERSION] (from $XR_OPENXR_SOURCE).\""

if [ "$DO_SHOW_DEPS" = true ]; then
  PRINT ""
  PRINT "Blender dependencies (libraries needed to build it):"
  PRINT ""
  PRINT "`eval _echo "$DEPS_COMMON_INFO"`"
  PRINT ""
  PRINT "`eval _echo "$DEPS_SPECIFIC_INFO"`"
  PRINT ""
  exit 0
fi

# ----------------------------------------------------------------------------
# Generic Helpers

# Check return code of wget for success...
download() {
  declare -a sources=("${!1}")
  sources_count=${#sources[@]}
  error=1

  for (( i=0; $i < $sources_count; i++ ))
  do
    wget -c ${sources[$i]} -O $2
    if [ $? -eq 0 ]; then
      error=0
      break
    fi
  done

  if [ $error -eq 1 ]; then
    ERROR "wget could not find ${sources[@]}, or could not write it to $2, exiting"
    exit 1
  fi
}

version_sanitize() {
  # Remove suffix such as '1.3_RC2', keeping only '1.3'.
  # Needed for numeric comparisons.
  local val=$(sed -r 's/^([^_]+).*/\1/' <<< "$1")
  # Remove trailing punctuation such as '1.0.', keeping only '1.0'.
  val=$(sed -r 's/[[:punct:]]*$//g' <<< "$val")
  echo $val
}

# Return 0 if $1 = $2 (i.e. 1.01.0 = 1.1, but 1.1.1 != 1.1), else 1.
# $1 and $2 should be version numbers made of numbers only.
version_eq() {
  local VER_1=$(version_sanitize "$1")
  local VER_2=$(version_sanitize "$2")
  local IFS='.'

  # Split both version numbers into their numeric elements.
  arr1=( $VER_1 )
  arr2=( $VER_2 )

  ret=1

  count1=${#arr1[@]}
  count2=${#arr2[@]}
  if [ $count2 -ge $count1 ]; then
    _t=$count1
    count1=$count2
    count2=$_t
    arr1=( $VER_2 )
    arr2=( $VER_1 )
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

  return $ret
}

# Return 0 if $1 >= $2, else 1.
# $1 and $2 should be version numbers made of numbers only.
version_ge() {
  version_eq $1 $2
  if [ $? -eq 1 -a $(_echo "$1" "$2" | sort --version-sort | head --lines=1) = "$1" ]; then
    return 1
  else
    return 0
  fi
}

# Return 0 if $3 > $1 >= $2, else 1.
# $1 and $2 should be version numbers made of numbers only.
version_ge_lt() {
  version_ge $1 $3
  if [ $? -eq 0 ]; then
    return 1
  else
    version_ge $1 $2
    return $?
  fi
}

# Return 0 if $1 is into $2 (e.g. 3.3.2 is into 3.3, but not 3.3.0 or 3.3.5), else 1.
# $1 and $2 should be version numbers made of numbers only.
# $1 should be at least as long as $2!
version_match() {
  local VER_1=$(version_sanitize "$1")
  local VER_2=$(version_sanitize "$2")
  local IFS='.'

  # Split both version numbers into their numeric elements.
  arr1=( $VER_1 )
  arr2=( $VER_2 )

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

  return $ret
}

# ----------------------------------------------------------------------------
# Generic compile helpers

prepare_opt() {
  INFO "Ensuring $INST exists and is writable by us"
  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, might be impossible to create install dir..."
  fi
  if [ ! -d  $INST ]; then
    $SUDO mkdir -p $INST
  fi

  if [ ! -w $INST ]; then
    $SUDO chown $USER $INST
    $SUDO chmod 775 $INST
  fi
}

# Check whether the current package needs to be recompiled, based on a dummy file containing a magic number in its name...
magic_compile_check() {
  if [ -f $INST/.$1-magiccheck-$2-$USE_CXX11 ]; then
    return 0
  else
    return 1
  fi
}

magic_compile_set() {
  rm -f $INST/.$1-magiccheck-*
  touch $INST/.$1-magiccheck-$2-$USE_CXX11
}

# Note: should clean nicely in $INST, but not in $SRC, when we switch to a new version of a lib...
_clean() {
  rm -rf `readlink -f $_inst_shortcut`
  # Only remove $_src dir when not using git repo (avoids to re-clone the whole repo every time!!!).
  if [ $_git == false ]; then
    rm -rf $_src
  fi
  rm -rf $_inst
  rm -rf $_inst_shortcut
}

_create_inst_shortcut() {
  rm -f $_inst_shortcut
  ln -s $_inst $_inst_shortcut
}

# ldconfig
run_ldconfig() {
  _lib_path="$INST/$1/lib"
  _lib64_path="$INST/$1/lib64"
  _ldconf_path="/etc/ld.so.conf.d/$1.conf"
  PRINT ""
  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to run ldconfig for $1, you'll have to do it yourself..."
  else
    INFO "Running ldconfig for $1..."
    $SUDO sh -c "/bin/echo -e \"$_lib_path\n$_lib64_path\" > $_ldconf_path"
    $SUDO /sbin/ldconfig  # XXX OpenSuse does not include sbin in command path with sudo!!!
  fi
  PRINT ""
}

# ----------------------------------------------------------------------------
# Build Python

_init_python() {
  _src=$SRC/Python-$PYTHON_VERSION
  _git=false
  _inst=$INST/python-$PYTHON_VERSION
  _inst_shortcut=$INST/python-$PYTHON_VERSION_MIN
}

_update_deps_python() {
  :
}

clean_Python() {
  clean_Numpy
  _init_python
  if [ -d $_inst ]; then
    _update_deps_python
  fi
  _clean
}

compile_Python() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, Python will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  py_magic=1
  _init_python

  # Clean install if needed!
  magic_compile_check python-$PYTHON_VERSION $py_magic
  if [ $? -eq 1 -o "$PYTHON_FORCE_REBUILD" = true ]; then
    clean_Python
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Python-$PYTHON_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_python

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      download PYTHON_SOURCE[@] $_src.tgz

      INFO "Unpacking Python-$PYTHON_VERSION"
      tar -C $SRC -xf $_src.tgz
    fi

    cd $_src

    ./configure --prefix=$_inst --libdir=$_inst/lib --enable-ipv6 \
        --enable-loadable-sqlite-extensions --with-dbmliborder=bdb \
        --with-computed-gotos --with-pymalloc --enable-shared

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "Python--$PYTHON_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set python-$PYTHON_VERSION $py_magic

    cd $CWD
    INFO "Done compiling Python-$PYTHON_VERSION!"
    _is_building=false
  else
    INFO "Own Python-$PYTHON_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-python option."
  fi

  run_ldconfig "python-$PYTHON_VERSION_MIN"
}

# ----------------------------------------------------------------------------
# Build Numpy

_init_numpy() {
  _src=$SRC/numpy-$NUMPY_VERSION
  _git=false
  _inst=$INST/numpy-$NUMPY_VERSION
  _python=$INST/python-$PYTHON_VERSION
  _site=lib/python$PYTHON_VERSION_MIN/site-packages
  _inst_shortcut=$_python/$_site/numpy
}

_update_deps_numpy() {
  :
}

clean_Numpy() {
  _init_numpy
  if [ -d $_inst ]; then
    _update_deps_numpy
  fi
  _clean
}

compile_Numpy() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, Numpy will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  numpy_magic=0
  _init_numpy

  # Clean install if needed!
  magic_compile_check numpy-$NUMPY_VERSION $numpy_magic
  if [ $? -eq 1 -o "$NUMPY_FORCE_REBUILD" = true ]; then
    clean_Numpy
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Numpy-$NUMPY_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_numpy

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      download NUMPY_SOURCE[@] $_src.tar.gz

      INFO "Unpacking Numpy-$NUMPY_VERSION"
      tar -C $SRC -xf $_src.tar.gz
    fi

    cd $_src

    $_python/bin/python3 setup.py install --old-and-unmanageable --prefix=$_inst

    if [ -d $_inst ]; then
      # Can't use _create_inst_shortcut here...
      rm -f $_inst_shortcut
      ln -s $_inst/$_site/numpy $_inst_shortcut
    else
      ERROR "Numpy-$NUMPY_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set numpy-$NUMPY_VERSION $numpy_magic

    cd $CWD
    INFO "Done compiling Numpy-$NUMPY_VERSION!"
    _is_building=false
  else
    INFO "Own Numpy-$NUMPY_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-numpy option."
  fi
}

# ----------------------------------------------------------------------------
# Build Boost

_init_boost() {
  _src=$SRC/boost-$BOOST_VERSION
  _git=false
  _inst=$INST/boost-$BOOST_VERSION
  _inst_shortcut=$INST/boost
}

_update_deps_boost() {
  OIIO_FORCE_REBUILD=true
  OSL_FORCE_REBUILD=true
  OPENVDB_FORCE_REBUILD=true
  ALEMBIC_FORCE_REBUILD=true
  if [ "$_is_building" = true ]; then
    OIIO_FORCE_BUILD=true
    OSL_FORCE_BUILD=true
    OPENVDB_FORCE_BUILD=true
    ALEMBIC_FORCE_BUILD=true
  fi
}

clean_Boost() {
  _init_boost
  if [ -d $_inst ]; then
    _update_deps_boost
  fi
  _clean
}

compile_Boost() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, Boost will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  boost_magic=11

  _init_boost

  # Clean install if needed!
  magic_compile_check boost-$BOOST_VERSION $boost_magic
  if [ $? -eq 1 -o "$BOOST_FORCE_REBUILD" = true ]; then
    clean_Boost
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Boost-$BOOST_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_boost

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading Boost-$BOOST_VERSION"
      mkdir -p $SRC
      download BOOST_SOURCE[@] $_src.tar.bz2
      tar -C $SRC --transform "s,\w*,boost-$BOOST_VERSION,x" -xf $_src.tar.bz2
    fi

    cd $_src
    if [ ! -f $_src/b2 ]; then
      ./bootstrap.sh
    fi
    ./b2 -j$THREADS -a $BOOST_BUILD_MODULES \
         --prefix=$_inst --disable-icu boost.locale.icu=off install
    ./b2 --clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "Boost-$BOOST_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set boost-$BOOST_VERSION $boost_magic

    cd $CWD
    INFO "Done compiling Boost-$BOOST_VERSION!"
    _is_building=false
  else
    INFO "Own Boost-$BOOST_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-boost option."
  fi

  # Just always run it, much simpler this way!
  run_ldconfig "boost"
}

# ----------------------------------------------------------------------------
# Build TBB

_init_tbb() {
  _src=$SRC/TBB-$TBB_VERSION
  _git=false
  _inst=$INST/tbb-$TBB_VERSION
  _inst_shortcut=$INST/tbb
}

_update_deps_tbb() {
  OSD_FORCE_REBUILD=true
  OPENVDB_FORCE_REBUILD=true
  USD_FORCE_REBUILD=true
  EMBREE_FORCE_REBUILD=true
  OIDN_FORCE_REBUILD=true
  if [ "$_is_building" = true ]; then
    OSD_FORCE_BUILD=true
    OPENVDB_FORCE_BUILD=true
    USD_FORCE_BUILD=true
    EMBREE_FORCE_BUILD=true
    OIDN_FORCE_BUILD=true
  fi
}

clean_TBB() {
  _init_tbb
  if [ -d $_inst ]; then
    _update_deps_tbb
  fi
  _clean
}

compile_TBB() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, TBB will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  tbb_magic=0
  _init_tbb

  # Clean install if needed!
  magic_compile_check tbb-$TBB_VERSION $tbb_magic
  if [ $? -eq 1 -o "$TBB_FORCE_REBUILD" = true ]; then
    clean_TBB
  fi

  if [ ! -d $_inst ]; then
    INFO "Building TBB-$TBB_VERSION$TBB_VERSION_UPDATE"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_tbb

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading TBB-$TBB_VERSION$TBB_VERSION_UPDATE"
      mkdir -p $SRC

      download TBB_SOURCE[@] $_src.tar.gz
      INFO "Unpacking TBB-$TBB_VERSION$TBB_VERSION_UPDATE"
      tar -C $SRC --transform "s,(.*/?)oneTBB[^/]*(.*),\1TBB-$TBB_VERSION\2,x" \
          -xf $_src.tar.gz

      INFO

      # Super-hack: Add some cmake builder to tbb... since they don't even have an install target by default, sic.
      download TBB_SOURCE_CMAKE[@] $_src/CMakeLists.txt
      cp $_src/build/vs2013/version_string.ver $_src/build/version_string.ver.in
    fi

    cd $_src

    # Always refresh the whole build!
    if [ -d cmake_build ]; then
      rm -rf cmake_build
    fi
    mkdir cmake_build
    cd cmake_build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_PREFIX_PATH=$_inst"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D TBB_BUILD_SHARED=ON"
    cmake_d="$cmake_d -D TBB_BUILD_STATIC=OFF"
    cmake_d="$cmake_d -D TBB_BUILD_TBBMALLOC=ON"
    cmake_d="$cmake_d -D TBB_BUILD_TBBMALLOC_PROXY=OFF"
    cmake_d="$cmake_d -D TBB_BUILD_TESTS=OFF"

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake $cmake_d -D CMAKE_CXX_FLAGS="$cflags" -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" ..

    make -j$THREADS && make install

    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "TBB-$TBB_VERSION$TBB_VERSION_UPDATE failed to compile, exiting"
      exit 1
    fi

    magic_compile_set tbb-$TBB_VERSION $tbb_magic

    cd $CWD
    INFO "Done compiling TBB-$TBB_VERSION$TBB_VERSION_UPDATE!"
    _is_building=false
  else
    INFO "Own TBB-$TBB_VERSION$TBB_VERSION_UPDATE is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-tbb option."
  fi

  run_ldconfig "tbb"
}

# ----------------------------------------------------------------------------
# Build OCIO

_init_ocio() {
  _src=$SRC/OpenColorIO-$OCIO_VERSION
  if [ "$OCIO_USE_REPO" = true ]; then
    _git=true
  else
    _git=false
  fi
  _inst=$INST/ocio-$OCIO_VERSION
  _inst_shortcut=$INST/ocio
}

_update_deps_ocio() {
  :
}

clean_OCIO() {
  _init_ocio
  if [ -d $_inst ]; then
    _update_deps_ocio
  fi
  _clean
}

compile_OCIO() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenColorIO will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  ocio_magic=2
  _init_ocio

  # Clean install if needed!
  magic_compile_check ocio-$OCIO_VERSION $ocio_magic
  if [ $? -eq 1 -o "$OCIO_FORCE_REBUILD" = true ]; then
    clean_OCIO
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenColorIO-$OCIO_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_ocio

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading OpenColorIO-$OCIO_VERSION"
      mkdir -p $SRC

      if [ "$OCIO_USE_REPO" = true ]; then
        git clone ${OCIO_SOURCE_REPO[0]} $_src
      else
        download OCIO_SOURCE[@] $_src.tar.gz
        INFO "Unpacking OpenColorIO-$OCIO_VERSION"
        tar -C $SRC --transform "s,(.*/?)imageworks-OpenColorIO[^/]*(.*),\1OpenColorIO-$OCIO_VERSION\2,x" \
            -xf $_src.tar.gz
      fi

    fi

    cd $_src

    if [ "$OCIO_USE_REPO" = true ]; then
      # XXX For now, always update from latest repo...
      git pull origin master
      git checkout $OCIO_SOURCE_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_PREFIX_PATH=$_inst"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D OCIO_BUILD_APPS=OFF"
    cmake_d="$cmake_d -D OCIO_BUILD_PYGLUE=OFF"
    cmake_d="$cmake_d -D STOP_ON_WARNING=OFF"

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi
    cflags="$cflags -Wno-error=unused-function -Wno-error=deprecated-declarations"

    cmake $cmake_d -D CMAKE_CXX_FLAGS="$cflags" -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" ..

    make -j$THREADS && make install

    # Force linking against static libs
    rm -f $_inst/lib/*.so*

    # Additional depencencies
    cp ext/dist/lib/libtinyxml.a $_inst/lib
    cp ext/dist/lib/libyaml-cpp.a $_inst/lib

    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenColorIO-$OCIO_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set ocio-$OCIO_VERSION $ocio_magic

    cd $CWD
    INFO "Done compiling OpenColorIO-$OCIO_VERSION!"
    _is_building=false
  else
    INFO "Own OpenColorIO-$OCIO_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-ocio option."
  fi

  run_ldconfig "ocio"
}

# ----------------------------------------------------------------------------
# Build OpenEXR (and ILMBase).

_init_openexr() {
  _src=$SRC/OpenEXR-$OPENEXR_VERSION
  _git=false
  _inst=$INST/openexr-$OPENEXR_VERSION
  _inst_shortcut=$INST/openexr
}

_update_deps_openexr() {
  OIIO_FORCE_REBUILD=true
  ALEMBIC_FORCE_REBUILD=true
  if [ "$_is_building" = true ]; then
    OIIO_FORCE_BUILD=true
    ALEMBIC_FORCE_BUILD=true
  fi
}

clean_OPENEXR() {
  _init_openexr
  if [ -d $_inst ]; then
    _update_deps_openexr
  fi
  _clean
}

compile_OPENEXR() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenEXR will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  openexr_magic=15

  # Clean install if needed!
  magic_compile_check openexr-$OPENEXR_VERSION $openexr_magic
  if [ $? -eq 1 -o "$OPENEXR_FORCE_REBUILD" = true ]; then
    clean_OPENEXR
  fi

  PRINT ""
  _init_openexr

  if [ ! -d $_inst ]; then
    INFO "Building ILMBase-$OPENEXR_VERSION and OpenEXR-$OPENEXR_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_openexr

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading OpenEXR-$OPENEXR_VERSION"
      mkdir -p $SRC

      if [ "$OPENEXR_USE_REPO" = true ]; then
        git clone ${OPENEXR_SOURCE_REPO[0]} $_src
      else
        download OPENEXR_SOURCE[@] $_src.tar.gz
        INFO "Unpacking OpenEXR-$OPENEXR_VERSION"
        tar -C $SRC --transform "s,(.*/?)openexr[^/]*(.*),\1OpenEXR-$OPENEXR_VERSION\2,x" -xf $_src.tar.gz
      fi

    fi

    cd $_src

    if [ "$OPENEXR_USE_REPO" = true ]; then
      # XXX For now, always update from latest repo...
      git pull origin master
      git checkout $OPENEXR_SOURCE_REPO_UID
      git reset --hard
      openexr_src_path="../OpenEXR"
    else
      openexr_src_path=".."
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D CMAKE_INSTALL_DOCDIR=/dev/null"  # Hack, there is no option to disable that currently...
    cmake_d="$cmake_d -D BUILD_SHARED_LIBS=ON"
    cmake_d="$cmake_d -D BUILD_TESTING=OFF"
    cmake_d="$cmake_d -D OPENEXR_BUILD_UTILS=OFF"
    cmake_d="$cmake_d -D PYILMBASE_ENABLE=OFF"
    cmake_d="$cmake_d -D OPENEXR_VIEWERS_ENABLE=OFF"

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake $cmake_d -D CMAKE_BUILD_TYPE=Release -D CMAKE_CXX_FLAGS="$cflags" -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" $openexr_src_path

    make -j$THREADS && make install

    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenEXR-$OPENEXR_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set openexr-$OPENEXR_VERSION $openexr_magic

    cd $CWD
    INFO "Done compiling OpenEXR-$OPENEXR_VERSION!"
    _is_building=false
  else
    INFO "Own OpenEXR-$OPENEXR_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-openexr option."
  fi

  _with_built_openexr=true

  # Just always run it, much simpler this way!
  run_ldconfig "openexr"
}

# ----------------------------------------------------------------------------
# Build OIIO

_init_oiio() {
  _src=$SRC/OpenImageIO-$OIIO_VERSION
  _git=true
  _inst=$INST/oiio-$OIIO_VERSION
  _inst_shortcut=$INST/oiio
}

_update_deps_oiio() {
  OSL_FORCE_REBUILD=true
  if [ "$_is_building" = true ]; then
    OSL_FORCE_BUILD=true
  fi
}

clean_OIIO() {
  _init_oiio
  if [ -d $_inst ]; then
    _update_deps_oiio
  fi
  _clean
}

compile_OIIO() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenImageIO will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  oiio_magic=17
  _init_oiio

  # Clean install if needed!
  magic_compile_check oiio-$OIIO_VERSION $oiio_magic
  if [ $? -eq 1 -o "$OIIO_FORCE_REBUILD" = true ]; then
    clean_OIIO
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenImageIO-$OIIO_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_oiio

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC

      if [ "$OIIO_USE_REPO" = true ]; then
        git clone ${OIIO_SOURCE_REPO[0]} $_src
      else
        download OIIO_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking OpenImageIO-$OIIO_VERSION"
        tar -C $SRC --transform "s,(.*/?)oiio-Release-[^/]*(.*),\1OpenImageIO-$OIIO_VERSION\2,x" -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$OIIO_USE_REPO" = true ]; then
      # XXX For now, always update from latest repo...
      git pull origin master
      # Stick to same rev as windows' libs...
      git checkout $OIIO_SOURCE_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_PREFIX_PATH=$_inst"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D STOP_ON_WARNING=OFF"
    cmake_d="$cmake_d -D BUILDSTATIC=OFF"
    cmake_d="$cmake_d -D LINKSTATIC=OFF"
    cmake_d="$cmake_d -D USE_SIMD=sse2"

    cmake_d="$cmake_d -D OPENEXR_VERSION=$OPENEXR_VERSION"

    if [ "$_with_built_openexr" = true ]; then
      cmake_d="$cmake_d -D ILMBASE_HOME=$INST/openexr"
      cmake_d="$cmake_d -D OPENEXR_HOME=$INST/openexr"
      INFO "ILMBASE_HOME=$INST/openexr"
    fi

    # ptex is only needed when nicholas bishop is ready
    cmake_d="$cmake_d -D USE_PTEX=OFF"

    # Optional tests and cmd tools
    cmake_d="$cmake_d -D USE_QT=OFF"
    cmake_d="$cmake_d -D USE_PYTHON=OFF"
    cmake_d="$cmake_d -D USE_FFMPEG=OFF"
    cmake_d="$cmake_d -D USE_OPENCV=OFF"
    cmake_d="$cmake_d -D BUILD_TESTING=OFF"
    cmake_d="$cmake_d -D OIIO_BUILD_TESTS=OFF"
    cmake_d="$cmake_d -D OIIO_BUILD_TOOLS=OFF"
    cmake_d="$cmake_d -D TXT2MAN="
    #cmake_d="$cmake_d -D CMAKE_EXPORT_COMPILE_COMMANDS=ON"
    #cmake_d="$cmake_d -D CMAKE_VERBOSE_MAKEFILE=ON"

    if [ -d $INST/boost ]; then
      cmake_d="$cmake_d -D BOOST_ROOT=$INST/boost -D Boost_NO_SYSTEM_PATHS=ON"
    fi

    # Looks like we do not need ocio in oiio for now...
#    if [ -d $INST/ocio ]; then
#      cmake_d="$cmake_d -D OCIO_PATH=$INST/ocio"
#    fi
    cmake_d="$cmake_d -D USE_OCIO=OFF"

    cmake_d="$cmake_d -D OIIO_BUILD_CPP11=ON"

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake $cmake_d -D CMAKE_CXX_FLAGS="$cflags" -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" ..

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenImageIO-$OIIO_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set oiio-$OIIO_VERSION $oiio_magic

    cd $CWD
    INFO "Done compiling OpenImageIO-$OIIO_VERSION!"
    _is_building=false
  else
    INFO "Own OpenImageIO-$OIIO_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-oiio option."
  fi

  # Just always run it, much simpler this way!
  run_ldconfig "oiio"
}

# ----------------------------------------------------------------------------
# Build LLVM

_init_llvm() {
  _src=$SRC/LLVM-$LLVM_VERSION
  _src_clang=$SRC/CLANG-$LLVM_VERSION
  _git=false
  _inst=$INST/llvm-$LLVM_VERSION
  _inst_shortcut=$INST/llvm
}

_update_deps_llvm() {
  OSL_FORCE_REBUILD=true
  if [ "$_is_building" = true ]; then
    OSL_FORCE_BUILD=true
  fi
}

clean_LLVM() {
  _init_llvm
  if [ -d $_inst ]; then
    _update_deps_llvm
  fi
  _clean
}

compile_LLVM() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, LLVM will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  llvm_magic=3
  _init_llvm

  # Clean install if needed!
  magic_compile_check llvm-$LLVM_VERSION $llvm_magic
  if [ $? -eq 1 -o "$LLVM_FORCE_REBUILD" = true ]; then
    clean_LLVM
  fi

  if [ ! -d $_inst ]; then
    INFO "Building LLVM-$LLVM_VERSION (CLANG included!)"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_llvm

    prepare_opt

    if [ ! -d $_src -o true ]; then
      mkdir -p $SRC
      download LLVM_SOURCE[@] "$_src.tar.xz"
      download LLVM_CLANG_SOURCE[@] "$_src_clang.tar.xz"

      INFO "Unpacking LLVM-$LLVM_VERSION"
      tar -C $SRC --transform "s,([^/]*/?)llvm-[^/]*(.*),\1LLVM-$LLVM_VERSION\2,x" \
          -xf $_src.tar.xz
      INFO "Unpacking CLANG-$LLVM_VERSION to $_src/tools/clang"
      # Stupid clang guys renamed 'clang' to 'cfe' for now handle both cases... :(
      tar -C $_src/tools \
          --transform "s,([^/]*/?)(clang|cfe)-[^/]*(.*),\1clang\3,x" \
          -xf $_src_clang.tar.xz

      cd $_src

      cd $CWD

    fi

    cd $_src

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D LLVM_ENABLE_FFI=ON"
    cmake_d="$cmake_d -D LLVM_TARGETS_TO_BUILD=X86"
    cmake_d="$cmake_d -D LLVM_ENABLE_TERMINFO=OFF"

    if [ -d $_FFI_INCLUDE_DIR ]; then
      cmake_d="$cmake_d -D FFI_INCLUDE_DIR=$_FFI_INCLUDE_DIR"
    fi

    cmake $cmake_d ..

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "LLVM-$LLVM_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set llvm-$LLVM_VERSION $llvm_magic

    cd $CWD
    INFO "Done compiling LLVM-$LLVM_VERSION (CLANG included)!"
    _is_building=false
  else
    INFO "Own LLVM-$LLVM_VERSION (CLANG included) is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-llvm option."
  fi
}

# ----------------------------------------------------------------------------
# Build OSL

_init_osl() {
  _src=$SRC/OpenShadingLanguage-$OSL_VERSION
  _git=true
  _inst=$INST/osl-$OSL_VERSION
  _inst_shortcut=$INST/osl
}

_update_deps_osl() {
  :
}

clean_OSL() {
  _init_osl
  if [ -d $_inst ]; then
    _update_deps_osl
  fi
  _clean
}

compile_OSL() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenShadingLanguage will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  osl_magic=21
  _init_osl

  # Clean install if needed!
  magic_compile_check osl-$OSL_VERSION $osl_magic
  if [ $? -eq 1 -o "$OSL_FORCE_REBUILD" = true ]; then
    #~ rm -Rf $_src  # XXX Radical, but not easy to change remote repo fully automatically
    clean_OSL
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenShadingLanguage-$OSL_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_osl

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC

      if [ "$OSL_USE_REPO" = true ]; then
        git clone ${OSL_SOURCE_REPO[0]} $_src
      else
        download OSL_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking OpenShadingLanguage-$OSL_VERSION"
        tar -C $SRC --transform "s,(.*/?)OpenShadingLanguage-[^/]*(.*),\1OpenShadingLanguage-$OSL_VERSION\2,x" \
            -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$OSL_USE_REPO" = true ]; then
      git remote set-url origin ${OSL_SOURCE_REPO[0]}
      # XXX For now, always update from latest repo...
      git pull --no-edit -X theirs origin $OSL_SOURCE_REPO_BRANCH
      # Stick to same rev as windows' libs...
      git checkout $OSL_SOURCE_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D BUILD_TESTING=OFF"
    cmake_d="$cmake_d -D STOP_ON_WARNING=OFF"
    cmake_d="$cmake_d -D BUILDSTATIC=OFF"
    cmake_d="$cmake_d -D OSL_BUILD_PLUGINS=OFF"
    cmake_d="$cmake_d -D OSL_BUILD_TESTS=OFF"
    cmake_d="$cmake_d -D USE_SIMD=sse2"
    cmake_d="$cmake_d -D USE_LLVM_BITCODE=OFF"
    cmake_d="$cmake_d -D USE_PARTIO=OFF"
    cmake_d="$cmake_d -D OSL_BUILD_MATERIALX=OFF"
    cmake_d="$cmake_d -D USE_QT=OFF"

    #~ cmake_d="$cmake_d -D ILMBASE_VERSION=$ILMBASE_VERSION"

    if [ "$_with_built_openexr" = true ]; then
      INFO "ILMBASE_HOME=$INST/openexr"
      cmake_d="$cmake_d -D OPENEXR_ROOT_DIR=$INST/openexr"
      cmake_d="$cmake_d -D ILMBASE_ROOT_DIR=$INST/openexr"
      # XXX Temp workaround... sigh, ILMBase really messed the things up by defining their custom names ON by default :(
    fi

    if [ -d $INST/boost ]; then
      cmake_d="$cmake_d -D BOOST_ROOT=$INST/boost -D Boost_NO_SYSTEM_PATHS=ON"
    fi

    if [ -d $INST/oiio ]; then
      cmake_d="$cmake_d -D OPENIMAGEIO_ROOT_DIR=$INST/oiio"
    fi

    if [ ! -z $LLVM_VERSION_FOUND ]; then
      cmake_d="$cmake_d -D LLVM_VERSION=$LLVM_VERSION_FOUND"
      if [ -d $INST/llvm ]; then
        cmake_d="$cmake_d -D LLVM_DIRECTORY=$INST/llvm"
        cmake_d="$cmake_d -D LLVM_STATIC=ON"
      fi
    fi

    #~ cmake_d="$cmake_d -D CMAKE_EXPORT_COMPILE_COMMANDS=ON"
    #~ cmake_d="$cmake_d -D CMAKE_VERBOSE_MAKEFILE=ON"

    cmake $cmake_d ..

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenShadingLanguage-$OSL_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set osl-$OSL_VERSION $osl_magic

    cd $CWD
    INFO "Done compiling OpenShadingLanguage-$OSL_VERSION!"
    _is_building=false
  else
    INFO "Own OpenShadingLanguage-$OSL_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-osl option."
  fi

  run_ldconfig "osl"
}

# ----------------------------------------------------------------------------
# Build OSD

_init_osd() {
  _src=$SRC/OpenSubdiv-$OSD_VERSION
  _git=true
  _inst=$INST/osd-$OSD_VERSION
  _inst_shortcut=$INST/osd
}

_update_deps_osd() {
  :
}

clean_OSD() {
  _init_osd
  if [ -d $_inst ]; then
    _update_deps_osd
  fi
  _clean
}

compile_OSD() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenSubdiv will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  osd_magic=2
  _init_osd

  # Clean install if needed!
  magic_compile_check osd-$OSD_VERSION $osd_magic
  if [ $? -eq 1 -o "$OSD_FORCE_REBUILD" = true ]; then
    clean_OSD
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenSubdiv-$OSD_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_osd

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC

      if [ "$OSD_USE_REPO" = true ]; then
        git clone ${OSD_SOURCE_REPO[0]} $_src
      else
        download OSD_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking OpenSubdiv-$OSD_VERSION"
        tar -C $SRC --transform "s,(.*/?)OpenSubdiv-[^/]*(.*),\1OpenSubdiv-$OSD_VERSION\2,x" \
            -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$OSD_USE_REPO" = true ]; then
      git remote set-url origin ${OSD_SOURCE_REPO[0]}
      # XXX For now, always update from latest repo...
      git pull --no-edit -X theirs origin $OSD_SOURCE_REPO_BRANCH
      # Stick to same rev as windows' libs...
      git checkout $OSD_SOURCE_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    if [ -d $INST/tbb ]; then
      cmake_d="$cmake_d $cmake_d -D TBB_LOCATION=$INST/tbb"
    fi
    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    # ptex is only needed when nicholas bishop is ready
    cmake_d="$cmake_d -D NO_PTEX=1"
    cmake_d="$cmake_d -D NO_CLEW=1 -D NO_CUDA=1 -D NO_OPENCL=1"
    # maya plugin, docs, tutorials, regression tests and examples are not needed
    cmake_d="$cmake_d -D NO_MAYA=1 -D NO_DOC=1 -D NO_TUTORIALS=1 -D NO_REGRESSION=1 -DNO_EXAMPLES=1"

    cmake $cmake_d ..

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenSubdiv-$OSD_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set osd-$OSD_VERSION $osd_magic

    cd $CWD
    INFO "Done compiling OpenSubdiv-$OSD_VERSION!"
    _is_building=false
  else
    INFO "Own OpenSubdiv-$OSD_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-osd option."
  fi

  run_ldconfig "osd"
}

# ----------------------------------------------------------------------------
# Build Blosc

_init_blosc() {
  _src=$SRC/c-blosc-$OPENVDB_BLOSC_VERSION
  _git=false
  _inst=$INST/blosc-$OPENVDB_BLOSC_VERSION
  _inst_shortcut=$INST/blosc
}

_update_deps_blosc() {
  OPENVDB_FORCE_REBUILD=true
  if [ "$_is_building" = true ]; then
    OPENVDB_FORCE_BUILD=true
  fi
}

clean_BLOSC() {
  _init_blosc
  if [ -d $_inst ]; then
    _update_deps_blosc
  fi
  _clean
}

compile_BLOSC() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, Blosc will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  blosc_magic=0
  _init_blosc

  # Clean install if needed!
  magic_compile_check blosc-$OPENVDB_BLOSC_VERSION $blosc_magic
  if [ $? -eq 1 -o "$OPENVDB_FORCE_REBUILD" = true ]; then
    clean_BLOSC
    rm -rf $_inst
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Blosc-$OPENVDB_BLOSC_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_blosc

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading Blosc-$OPENVDB_BLOSC_VERSION"
      mkdir -p $SRC
      download OPENVDB_BLOSC_SOURCE[@] $_src.tar.gz

      INFO "Unpacking Blosc-$OPENVDB_BLOSC_VERSION"
      tar -C $SRC -xf $_src.tar.gz
    fi

    cd $_src
    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D BUILD_STATIC=OFF"
    cmake_d="$cmake_d -D BUILD_TESTS=OFF"
    cmake_d="$cmake_d -D BUILD_BENCHMARKS=OFF"
    INFO "$cmake_d"

    cmake $cmake_d ..

    make -j$THREADS && make install

    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "Blosc-$OPENVDB_BLOSC_VERSION failed to compile, exiting"
      exit 1
    fi
    cd $CWD
    INFO "Done compiling Blosc-$OPENVDB_BLOSC_VERSION!"
    _is_building=false
  else
    INFO "Own Blosc-$OPENVDB_BLOSC_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib (and openvdb), use the --force-openvdb option."
  fi

  magic_compile_set blosc-$OPENVDB_BLOSC_VERSION $blosc_magic

  run_ldconfig "blosc"
}

# ----------------------------------------------------------------------------
# Build OpenVDB

_init_openvdb() {
  _src=$SRC/openvdb-$OPENVDB_VERSION
  _git=false
  _inst=$INST/openvdb-$OPENVDB_VERSION
  _inst_shortcut=$INST/openvdb
}

_update_deps_openvdb() {
  :
}

clean_OPENVDB() {
  _init_openvdb
  if [ -d $_inst ]; then
    _update_deps_openvdb
  fi
  _clean
}

compile_OPENVDB() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenVDB will not be compiled!"
    return
  fi

  compile_BLOSC
  PRINT ""

  # To be changed each time we make edits that would modify the compiled result!
  openvdb_magic=1
  _init_openvdb

  # Clean install if needed!
  magic_compile_check openvdb-$OPENVDB_VERSION $openvdb_magic
  if [ $? -eq 1 -o "$OPENVDB_FORCE_REBUILD" = true ]; then
    clean_OPENVDB
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenVDB-$OPENVDB_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_openvdb

    prepare_opt

    if [ ! -d $_src -o true ]; then
      mkdir -p $SRC
      download OPENVDB_SOURCE[@] "$_src.tar.gz"

      INFO "Unpacking OpenVDB-$OPENVDB_VERSION"
      tar -C $SRC -xf $_src.tar.gz
    fi

    cd $_src

    #~ if [ "$OPENVDB_USE_REPO" = true ]; then
      #~ git remote set-url origin ${OPENVDB_SOURCE_REPO[0]}
      #~ # XXX For now, always update from latest repo...
      #~ git pull --no-edit -X theirs origin $OPENVDB_SOURCE_REPO_BRANCH
      #~ # Stick to same rev as windows' libs...
      #~ git checkout $OPENVDB_SOURCE_REPO_UID
      #~ git reset --hard
    #~ fi

    # Source builds here
    cd openvdb

    make_d="DESTDIR=$_inst"
    make_d="$make_d HDSO=/usr"

    if [ -d $INST/boost ]; then
      make_d="$make_d BOOST_INCL_DIR=$INST/boost/include BOOST_LIB_DIR=$INST/boost/lib"
    fi
    if [ -d $INST/tbb ]; then
      make_d="$make_d TBB_ROOT=$INST/tbb TBB_USE_STATIC_LIBS=OFF"
    fi

    if [ "$_with_built_openexr" = true ]; then
      make_d="$make_d ILMBASE_INCL_DIR=$INST/openexr/include ILMBASE_LIB_DIR=$INST/openexr/lib"
      make_d="$make_d EXR_INCL_DIR=$INST/openexr/include EXR_LIB_DIR=$INST/openexr/lib"
      INFO "ILMBASE_HOME=$INST/openexr"
    fi

    if [ -d $INST/blosc ]; then
      make_d="$make_d BLOSC_INCL_DIR=$INST/blosc/include BLOSC_LIB_DIR=$INST/blosc/lib"
    fi

    # Build without log4cplus, glfw, python module & docs
    make_d="$make_d LOG4CPLUS_INCL_DIR= GLFW_INCL_DIR= PYTHON_VERSION= DOXYGEN="

    make -j$THREADS lib $make_d install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenVDB-$OPENVDB_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set openvdb-$OPENVDB_VERSION $openvdb_magic

    cd $CWD
    INFO "Done compiling OpenVDB-$OPENVDB_VERSION!"
    _is_building=false
  else
    INFO "Own OpenVDB-$OPENVDB_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-openvdb option."
  fi

  run_ldconfig "openvdb"
}

# ----------------------------------------------------------------------------
# Build Alembic

_init_alembic() {
  _src=$SRC/alembic-$ALEMBIC_VERSION
  _git=false
  _inst=$INST/alembic-$ALEMBIC_VERSION
  _inst_shortcut=$INST/alembic
}

_update_deps_alembic() {
  :
}

clean_ALEMBIC() {
  _init_alembic
  if [ -d $_inst ]; then
    _update_deps_alembic
  fi
  _clean
}

compile_ALEMBIC() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, Alembic will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  alembic_magic=2
  _init_alembic

  # Clean install if needed!
  magic_compile_check alembic-$ALEMBIC_VERSION $alembic_magic
  if [ $? -eq 1 -o "$ALEMBIC_FORCE_REBUILD" = true ]; then
    clean_ALEMBIC
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Alembic-$ALEMBIC_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_alembic

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      download ALEMBIC_SOURCE[@] "$_src.tar.gz"

      INFO "Unpacking Alembic-$ALEMBIC_VERSION"
      tar -C $SRC -xf $_src.tar.gz
    fi

    cd $_src

    cmake_d="-D CMAKE_INSTALL_PREFIX=$_inst"

    if [ -d $INST/boost ]; then
      if [ -d $INST/boost ]; then
        cmake_d="$cmake_d -D BOOST_ROOT=$INST/boost"
      fi
      cmake_d="$cmake_d -D USE_STATIC_BOOST=ON"
    else
      cmake_d="$cmake_d -D USE_STATIC_BOOST=OFF"
    fi

    if [ "$_with_built_openexr" = true ]; then
      cmake_d="$cmake_d -D ILMBASE_ROOT=$INST/openexr"
      cmake_d="$cmake_d -D USE_ARNOLD=OFF"
      cmake_d="$cmake_d -D USE_BINARIES=OFF"
      cmake_d="$cmake_d -D USE_EXAMPLES=OFF"
      cmake_d="$cmake_d -D USE_HDF5=OFF"
      cmake_d="$cmake_d -D USE_MAYA=OFF"
      cmake_d="$cmake_d -D USE_PRMAN=OFF"
      cmake_d="$cmake_d -D USE_PYALEMBIC=OFF"
      cmake_d="$cmake_d -D USE_STATIC_HDF5=OFF"
      cmake_d="$cmake_d -D ALEMBIC_ILMBASE_LINK_STATIC=OFF"
      cmake_d="$cmake_d -D ALEMBIC_SHARED_LIBS=OFF"
      INFO "ILMBASE_ROOT=$INST/openexr"
    fi

    cmake $cmake_d ./
    make -j$THREADS install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "Alembic-$ALEMBIC_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set alembic-$ALEMBIC_VERSION $alembic_magic

    cd $CWD
    INFO "Done compiling Alembic-$ALEMBIC_VERSION!"
    _is_building=false
  else
    INFO "Own Alembic-$ALEMBIC_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-alembic option."
  fi

  run_ldconfig "alembic"
}

#### Build USD ####
_init_usd() {
  _src=$SRC/USD-$USD_VERSION
  _git=false
  _inst=$INST/usd-$USD_VERSION
  _inst_shortcut=$INST/usd
}

_update_deps_usd() {
  :
}

clean_USD() {
  _init_usd
  if [ -d $_inst ]; then
    _update_deps_usd
  fi
  _clean
}

compile_USD() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, USD will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  usd_magic=1
  _init_usd

  # Clean install if needed!
  magic_compile_check usd-$USD_VERSION $usd_magic
  if [ $? -eq 1 -o "$USD_FORCE_REBUILD" = true ]; then
    clean_USD
  fi

  if [ ! -d $_inst ]; then
    INFO "Building USD-$USD_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_usd

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      download USD_SOURCE[@] "$_src.tar.gz"

      INFO "Unpacking USD-$USD_VERSION"
      tar -C $SRC -xf $_src.tar.gz
      patch -d $_src -p1 < $SCRIPT_DIR/patches/usd.diff
    fi

    cd $_src

    cmake_d="-D CMAKE_INSTALL_PREFIX=$_inst"
    # For the reasoning behind these options, please see usd.cmake.
    if [ -d $INST/boost ]; then
      cmake_d="$cmake_d $cmake_d -D BOOST_ROOT=$INST/boost"
    fi

    if [ -d $INST/tbb ]; then
      cmake_d="$cmake_d $cmake_d -D TBB_ROOT_DIR=$INST/tbb"
    fi
    cmake_d="$cmake_d -DPXR_SET_INTERNAL_NAMESPACE=usdBlender"
    cmake_d="$cmake_d -DPXR_ENABLE_PYTHON_SUPPORT=OFF"
    cmake_d="$cmake_d -DPXR_BUILD_IMAGING=OFF"
    cmake_d="$cmake_d -DPXR_BUILD_TESTS=OFF"
    cmake_d="$cmake_d -DBUILD_SHARED_LIBS=ON"
    cmake_d="$cmake_d -DPXR_BUILD_MONOLITHIC=ON"
    cmake_d="$cmake_d -DPXR_BUILD_USD_TOOLS=OFF"
    cmake_d="$cmake_d -DCMAKE_DEBUG_POSTFIX=_d"

    cmake $cmake_d ./
    make -j$THREADS install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "USD-$USD_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set usd-$USD_VERSION $usd_magic

    cd $CWD
    INFO "Done compiling USD-$USD_VERSION!"
    _is_building=false
  else
    INFO "Own USD-$USD_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-usd option."
  fi

  run_ldconfig "usd"
}

# ----------------------------------------------------------------------------
# Build OpenCOLLADA
_init_opencollada() {
  _src=$SRC/OpenCOLLADA-$OPENCOLLADA_VERSION
  _git=true
  _inst=$INST/opencollada-$OPENCOLLADA_VERSION
  _inst_shortcut=$INST/opencollada
}

_update_deps_collada() {
  :
}

clean_OpenCOLLADA() {
  _init_opencollada
  if [ -d $_inst ]; then
    _update_deps_collada
  fi
  _clean
}

compile_OpenCOLLADA() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenCOLLADA will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled results!
  opencollada_magic=9
  _init_opencollada

  # Clean install if needed!
  magic_compile_check opencollada-$OPENCOLLADA_VERSION $opencollada_magic
  if [ $? -eq 1 -o "$OPENCOLLADA_FORCE_REBUILD" = true ]; then
    clean_OpenCOLLADA
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenCOLLADA-$OPENCOLLADA_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_collada

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      if [ "$OPENCOLLADA_USE_REPO" = true ]; then
        git clone $OPENCOLLADA_SOURCE_REPO $_src
      else
        download OPENCOLLADA_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking OpenCOLLADA-$OPENCOLLADA_VERSION"
        tar -C $SRC -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$OPENCOLLADA_USE_REPO" = true ]; then
      git pull origin $OPENCOLLADA_REPO_BRANCH

      # Stick to same rev as windows' libs...
      git checkout $OPENCOLLADA_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D USE_EXPAT=OFF"
    cmake_d="$cmake_d -D USE_LIBXML=ON"
    # XXX Does not work!
#    cmake_d="$cmake_d -D USE_STATIC=OFF"
    cmake_d="$cmake_d -D USE_STATIC=ON"

    cmake $cmake_d ../

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenCOLLADA-$OPENCOLLADA_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set opencollada-$OPENCOLLADA_VERSION $opencollada_magic

    cd $CWD
    INFO "Done compiling OpenCOLLADA-$OPENCOLLADA_VERSION!"
    _is_building=false
  else
    INFO "Own OpenCOLLADA-$OPENCOLLADA_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-opencollada option."
  fi
}

# ----------------------------------------------------------------------------
# Build Embree

_init_embree() {
  _src=$SRC/embree-$EMBREE_VERSION
  _git=true
  _inst=$INST/embree-$EMBREE_VERSION
  _inst_shortcut=$INST/embree
}

_update_deps_embree() {
  :
}

clean_Embree() {
  _init_embree
  if [ -d $_inst ]; then
    _update_deps_embree
  fi
  _clean
}

compile_Embree() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, Embree will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled results!
  embree_magic=10
  _init_embree

  # Clean install if needed!
  magic_compile_check embree-$EMBREE_VERSION $embree_magic
  if [ $? -eq 1 -o "$EMBREE_FORCE_REBUILD" = true ]; then
    clean_Embree
  fi

  if [ ! -d $_inst ]; then
    INFO "Building Embree-$EMBREE_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_embree

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      if [ "EMBREE_USE_REPO" = true ]; then
        git clone $EMBREE_SOURCE_REPO $_src
      else
        download EMBREE_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking Embree-$EMBREE_VERSION"
        tar -C $SRC -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$EMBREE_USE_REPO" = true ]; then
      git pull origin $EMBREE_REPO_BRANCH

      # Stick to same rev as windows' libs...
      git checkout $EMBREE_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D EMBREE_ISPC_SUPPORT=OFF"
    cmake_d="$cmake_d -D EMBREE_TUTORIALS=OFF"
    cmake_d="$cmake_d -D EMBREE_STATIC_LIB=ON"
    cmake_d="$cmake_d -D EMBREE_RAY_MASK=ON"
    cmake_d="$cmake_d -D EMBREE_FILTER_FUNCTION=ON"
    cmake_d="$cmake_d -D EMBREE_BACKFACE_CULLING=OFF"
    cmake_d="$cmake_d -D EMBREE_MAX_ISA=AVX2"

    cmake_d="$cmake_d -D EMBREE_TASKING_SYSTEM=TBB"
    if [ -d $INST/tbb ]; then
      make_d="$make_d EMBREE_TBB_ROOT=$INST/tbb"
    fi

    cmake $cmake_d ../

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "Embree-$EMBREE_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set embree-$EMBREE_VERSION $embree_magic

    cd $CWD
    INFO "Done compiling Embree-$EMBREE_VERSION!"
    _is_building=false
  else
    INFO "Own Embree-$EMBREE_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-embree option."
  fi
}

# ----------------------------------------------------------------------------
# Build OpenImageDenoise

_init_oidn() {
  _src=$SRC/oidn-$OIDN_VERSION
  _git=true
  _inst=$INST/oidn-$OIDN_VERSION
  _inst_shortcut=$INST/oidn
}

_update_deps_oidn() {
  :
}

clean_oidn() {
  _init_oidn
  if [ -d $_inst ]; then
    _update_deps_oidn
  fi
  _clean
}

compile_OIDN() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenImageDenoise will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled results!
  oidn_magic=9
  _init_oidn

  # Clean install if needed!
  magic_compile_check oidn-$OIDN_VERSION $oidn_magic
  if [ $? -eq 1 -o "$OIDN_FORCE_REBUILD" = true ]; then
    clean_oidn
  fi

  if [ ! -d $_inst ]; then
    INFO "Building OpenImageDenoise-$OIDN_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_oidn

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC
      if [ "OIDN_USE_REPO" = true ]; then
        git clone $OIDN_SOURCE_REPO $_src
      else
        download OIDN_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking OpenImageDenoise-$OIDN_VERSION"
        tar -C $SRC -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$OIDN_USE_REPO" = true ]; then
      git pull origin $OIDN_REPO_BRANCH

      # Stick to same rev as windows' libs...
      git checkout $OIDN_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D WITH_EXAMPLE=OFF"
    cmake_d="$cmake_d -D WITH_TEST=OFF"
    cmake_d="$cmake_d -D OIDN_STATIC_LIB=ON"

    if [ -d $INST/tbb ]; then
      make_d="$make_d TBB_ROOT=$INST/tbb"
    fi

    cmake $cmake_d ../

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "OpenImageDenoise-$OIDN_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set oidn-$OIDN_VERSION $oidn_magic

    cd $CWD
    INFO "Done compiling OpenImageDenoise-$OIDN_VERSION!"
    _is_building=false
  else
    INFO "Own OpenImageDenoise-$OIDN_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-oidn option."
  fi

  run_ldconfig "oidn"
}

# ----------------------------------------------------------------------------
# Build FFMPEG

_init_ffmpeg() {
  _src=$SRC/ffmpeg-$FFMPEG_VERSION
  _inst=$INST/ffmpeg-$FFMPEG_VERSION
  _inst_shortcut=$INST/ffmpeg
}

_update_deps_ffmpeg() {
  :
}

clean_FFmpeg() {
  _init_ffmpeg
  if [ -d $_inst ]; then
    _update_deps_ffmpeg
  fi
  _clean
}

compile_FFmpeg() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, ffmpeg will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  ffmpeg_magic=5
  _init_ffmpeg

  # Clean install if needed!
  magic_compile_check ffmpeg-$FFMPEG_VERSION $ffmpeg_magic
  if [ $? -eq 1 -o "$FFMPEG_FORCE_REBUILD" = true ]; then
    clean_FFmpeg
  fi

  if [ ! -d $_inst ]; then
    INFO "Building ffmpeg-$FFMPEG_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_ffmpeg

    prepare_opt

    if [ ! -d $_src ]; then
      INFO "Downloading ffmpeg-$FFMPEG_VERSION"
      mkdir -p $SRC
      download FFMPEG_SOURCE[@] "$_src.tar.bz2"

      INFO "Unpacking ffmpeg-$FFMPEG_VERSION"
      tar -C $SRC -xf $_src.tar.bz2
    fi

    cd $_src

    extra=""

    if [ "$VORBIS_USE" = true ]; then
      extra="$extra --enable-libvorbis"
    fi

    if [ "$THEORA_USE" = true ]; then
      extra="$extra --enable-libtheora"
    fi

    if [ "$XVID_USE" = true ]; then
      extra="$extra --enable-libxvid"
    fi

    if [ "$X264_USE" = true ]; then
      extra="$extra --enable-libx264"
    fi

    if [ "$VPX_USE" = true ]; then
      extra="$extra --enable-libvpx"
    fi

    if [ "$OPUS_USE" = true ]; then
      extra="$extra --enable-libopus"
    fi

    if [ "$MP3LAME_USE" = true ]; then
      extra="$extra --enable-libmp3lame"
    fi

    if [ "$OPENJPEG_USE" = true ]; then
      extra="$extra --enable-libopenjpeg"
    fi

    ./configure --cc="gcc -Wl,--as-needed" \
        --extra-ldflags="-pthread -static-libgcc" \
        --prefix=$_inst --enable-static \
        --disable-ffplay --disable-doc \
        --enable-gray \
        --enable-avfilter --disable-vdpau \
        --disable-bzlib --disable-libgsm --disable-libspeex \
        --enable-pthreads --enable-zlib --enable-stripping --enable-runtime-cpudetect \
        --disable-vaapi --disable-nonfree --enable-gpl \
        --disable-postproc --disable-librtmp --disable-libopencore-amrnb \
        --disable-libopencore-amrwb --disable-libdc1394 --disable-version3 --disable-outdev=sdl \
        --disable-libxcb \
        --disable-outdev=xv --disable-indev=sndio --disable-outdev=sndio \
        --disable-outdev=alsa --disable-indev=sdl --disable-indev=alsa --disable-indev=jack \
        --disable-indev=lavfi $extra

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "FFmpeg-$FFMPEG_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set ffmpeg-$FFMPEG_VERSION $ffmpeg_magic

    cd $CWD
    INFO "Done compiling ffmpeg-$FFMPEG_VERSION!"
    _is_building=false
  else
    INFO "Own ffmpeg-$FFMPEG_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-ffmpeg option."
  fi
}

# ----------------------------------------------------------------------------
# Build OpenXR SDK

_init_xr_openxr_sdk() {
  _src=$SRC/XR-OpenXR-SDK-$XR_OPENXR_VERSION
  _git=true
  _inst=$INST/xr-openxr-sdk-$XR_OPENXR_VERSION
  _inst_shortcut=$INST/xr-openxr-sdk
}

_update_deps_xr_openxr_sdk() {
  :
}

clean_XR_OpenXR_SDK() {
  _init_xr_openxr_sdk
  if [ -d $_inst ]; then
    _update_deps_xr_openxr_sdk
  fi
  _clean
}

compile_XR_OpenXR_SDK() {
  if [ "$NO_BUILD" = true ]; then
    WARNING "--no-build enabled, OpenXR will not be compiled!"
    return
  fi

  # To be changed each time we make edits that would modify the compiled result!
  xr_openxr_magic=2
  _init_xr_openxr_sdk

  # Clean install if needed!
  magic_compile_check xr-openxr-$XR_OPENXR_VERSION $xr_openxr_magic
  if [ $? -eq 1 -o "$XR_OPENXR_FORCE_REBUILD" = true ]; then
    clean_XR_OpenXR_SDK
  fi

  if [ ! -d $_inst ]; then
    INFO "Building XR-OpenXR-SDK-$XR_OPENXR_VERSION"
    _is_building=true

    # Rebuild dependencies as well!
    _update_deps_xr_openxr_sdk

    prepare_opt

    if [ ! -d $_src ]; then
      mkdir -p $SRC

      if [ "$XR_OPENXR_USE_REPO" = true ]; then
        git clone $XR_OPENXR_SOURCE_REPO $_src
      else
        download XR_OPENXR_SOURCE[@] "$_src.tar.gz"
        INFO "Unpacking XR-OpenXR-SDK-$XR_OPENXR_VERSION"
        tar -C $SRC --transform "s,(.*/?)OpenXR-SDK-[^/]*(.*),\1XR-OpenXR-SDK-$XR_OPENXR_VERSION\2,x" \
            -xf $_src.tar.gz
      fi
    fi

    cd $_src

    if [ "$XR_OPENXR_USE_REPO" = true ]; then
      git pull origin $XR_OPENXR_REPO_BRANCH

      # Stick to same rev as windows' libs...
      git checkout $XR_OPENXR_REPO_UID
      git reset --hard
    fi

    # Always refresh the whole build!
    if [ -d build ]; then
      rm -rf build
    fi
    mkdir build
    cd build

    # Keep flags in sync with XR_OPENXR_SDK_EXTRA_ARGS in xr_openxr.cmake!
    cmake_d="-D CMAKE_BUILD_TYPE=Release"
    cmake_d="$cmake_d -D CMAKE_INSTALL_PREFIX=$_inst"
    cmake_d="$cmake_d -D BUILD_FORCE_GENERATION=OFF"
    cmake_d="$cmake_d -D BUILD_LOADER=ON"
    cmake_d="$cmake_d -D DYNAMIC_LOADER=OFF"
    cmake_d="$cmake_d -D BUILD_WITH_WAYLAND_HEADERS=OFF"
    cmake_d="$cmake_d -D BUILD_WITH_XCB_HEADERS=OFF"
    cmake_d="$cmake_d -D BUILD_WITH_XLIB_HEADERS=ON"
    cmake_d="$cmake_d -D BUILD_WITH_SYSTEM_JSONCPP=OFF"

    cmake $cmake_d "-DCMAKE_CXX_FLAGS=-DDISABLE_STD_FILESYSTEM=1" ..

    make -j$THREADS && make install
    make clean

    if [ -d $_inst ]; then
      _create_inst_shortcut
    else
      ERROR "XR-OpenXR-SDK-$XR_OPENXR_VERSION failed to compile, exiting"
      exit 1
    fi

    magic_compile_set xr-openxr-$XR_OPENXR_VERSION $xr_openxr_magic

    cd $CWD
    INFO "Done compiling XR-OpenXR-SDK-$XR_OPENXR_VERSION!"
    _is_building=false
  else
    INFO "Own XR-OpenXR-SDK-$XR_OPENXR_VERSION is up to date, nothing to do!"
    INFO "If you want to force rebuild of this lib, use the --force-xr-openxr option."
  fi

  run_ldconfig "xr-openxr-sdk"
}


# ----------------------------------------------------------------------------
# Install on DEB-like

get_package_version_DEB() {
    dpkg-query -W -f '${Version}' $1 | sed -r 's/([0-9]+:)?(([0-9]+\.?)+([0-9]+)).*/\2/'
}

check_package_DEB() {
  r=`apt-cache show $1 | grep -c 'Package:'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_installed_DEB() {
  r=`dpkg-query -W -f='${Status}' $1 | grep -c "install ok"`

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

check_package_version_ge_lt_DEB() {
  v=`apt-cache policy $1 | grep 'Candidate:' | sed -r 's/.*:\s*([0-9]+:)?(([0-9]+\.?)+).*/\2/'`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge_lt $v $2 $3
  return $?
}

install_packages_DEB() {
  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to run apt-get install for $@, you'll have to do it yourself..."
  else
    $SUDO apt-get install -y $@
    if [ $? -ge 1 ]; then
      ERROR "apt-get failed to install requested packages, exiting."
      exit 1
    fi
  fi
}

install_DEB() {
  PRINT ""
  INFO "Installing dependencies for DEB-based distribution"
  PRINT ""
  PRINT "`eval _echo "$COMMON_INFO"`"
  PRINT ""

  if [ "$NO_CONFIRM" = false ]; then
    read -p "Do you want to continue (Y/n)?"
    [ "$(echo ${REPLY:=Y} | tr [:upper:] [:lower:])" != "y" ] && exit
  fi

  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to run apt-get update, you'll have to do it yourself..."
  else
    $SUDO apt-get update
  fi

  # These libs should always be available in debian/ubuntu official repository...
  VORBIS_DEV="libvorbis-dev"
  OGG_DEV="libogg-dev"
  THEORA_DEV="libtheora-dev"

  _packages="gawk cmake cmake-curses-gui build-essential libjpeg-dev libpng-dev libtiff-dev \
             git libfreetype6-dev libx11-dev flex bison libxxf86vm-dev \
             libxcursor-dev libxi-dev wget libsqlite3-dev libxrandr-dev libxinerama-dev \
             libbz2-dev libncurses5-dev libssl-dev liblzma-dev libreadline-dev \
             libopenal-dev libglew-dev yasm $THEORA_DEV $VORBIS_DEV $OGG_DEV \
             libsdl2-dev libfftw3-dev patch bzip2 libxml2-dev libtinyxml-dev libjemalloc-dev"
             # libglewmx-dev  (broken in deb testing currently...)

  VORBIS_USE=true
  OGG_USE=true
  THEORA_USE=true

  PRINT ""
  # We need openjp2, libopenjpeg is an old version
  OPENJPEG_DEV="libopenjp2-7-dev"
  check_package_DEB $OPENJPEG_DEV
  if [ $? -eq 0 ]; then
    _packages="$_packages $OPENJPEG_DEV"
    OPENJPEG_USE=true
  fi

  PRINT ""
  # Some not-so-old distro (ubuntu 12.4) do not have it, do not fail in this case, just warn.
  YAMLCPP_DEV="libyaml-cpp-dev"
  check_package_DEB $YAMLCPP_DEV
  if [ $? -eq 0 ]; then
    _packages="$_packages $YAMLCPP_DEV"
  else
    PRINT ""
    WARNING "libyaml-cpp-dev not found, you may have to install it by hand to get Blender compiling..."
    PRINT ""
  fi

  PRINT ""
  CLANG_FORMAT="clang-format"
  check_package_version_ge_DEB $CLANG_FORMAT $CLANG_FORMAT_VERSION_MIN
  if [ $? -eq 0 ]; then
    _packages="$_packages $CLANG_FORMAT"
  else
    PRINT ""
    WARNING "clang-format $CLANG_FORMAT_VERSION_MIN or higher not found, this is NOT needed to get Blender compiling..."
    PRINT ""
  fi

  if [ "$WITH_JACK" = true ]; then
    _packages="$_packages libspnav-dev"
    # Only install jack if jack2 is not already installed!
    JACK="libjack-dev"
    JACK2="libjack-jackd2-dev"
    check_package_installed_DEB $JACK2
    if [ $? -eq 0 ]; then
      _packages="$_packages $JACK2"
    else
      _packages="$_packages $JACK"
    fi
  fi

  PRINT ""
  install_packages_DEB $_packages

  PRINT""
  LIBSNDFILE_DEV="libsndfile1-dev"
  check_package_DEB $LIBSNDFILE_DEV
  if [ $? -eq 0 ]; then
    install_packages_DEB $LIBSNDFILE_DEV
  fi

  PRINT ""
  X264_DEV="libx264-dev"
  check_package_version_ge_DEB $X264_DEV $X264_VERSION_MIN
  if [ $? -eq 0 ]; then
    install_packages_DEB $X264_DEV
    X264_USE=true
  fi

  if [ "$WITH_ALL" = true ]; then
    PRINT ""
    XVID_DEV="libxvidcore-dev"
    check_package_DEB $XVID_DEV
    if [ $? -eq 0 ]; then
      install_packages_DEB $XVID_DEV
      XVID_USE=true
    fi

    PRINT ""
    MP3LAME_DEV="libmp3lame-dev"
    check_package_DEB $MP3LAME_DEV
    if [ $? -eq 0 ]; then
      install_packages_DEB $MP3LAME_DEV
      MP3LAME_USE=true
    fi

    PRINT ""
    VPX_DEV="libvpx-dev"
    check_package_version_ge_DEB $VPX_DEV $VPX_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB $VPX_DEV
      VPX_USE=true
    fi

    PRINT ""
    OPUS_DEV="libopus-dev"
    check_package_version_ge_DEB $OPUS_DEV $OPUS_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB $OPUS_DEV
      OPUS_USE=true
    fi
  fi

  # Check cmake/glew versions and disable features for older distros.
  # This is so Blender can at least compile.
  PRINT ""
  _cmake=`get_package_version_DEB cmake`
  version_ge $_cmake "2.8.10"
  if [ $? -eq 1 ]; then
    version_ge $_cmake "2.8.8"
    if [ $? -eq 1 ]; then
      WARNING "OpenVDB and OpenCOLLADA disabled because cmake-$_cmake is not enough"
      OPENVDB_SKIP=true
      OPENCOLLADA_SKIP=true
    else
      WARNING "OpenVDB disabled because cmake-$_cmake is not enough"
      OPENVDB_SKIP=true
    fi
  fi

  PRINT ""
  _glew=`get_package_version_DEB libglew-dev`
  if [ -z $_glew ]; then
    # Stupid virtual package in Ubuntu 12.04 doesn't show version number...
    _glew=`apt-cache showpkg libglew-dev|tail -n1|awk '{print $2}'|sed 's/-.*//'`
  fi
  version_ge $_glew "1.9.0"
  if [ $? -eq 1 ]; then
    version_ge $_glew "1.7.0"
    if [ $? -eq 1 ]; then
      WARNING "OpenSubdiv disabled because GLEW-$_glew is not enough"
      WARNING "Blender will not use system GLEW library"
      OSD_SKIP=true
      NO_SYSTEM_GLEW=true
    else
      WARNING "OpenSubdiv will compile with GLEW-$_glew but with limited capability"
      WARNING "Blender will not use system GLEW library"
      NO_SYSTEM_GLEW=true
    fi
  fi


  PRINT ""
  _do_compile_python=false
  if [ "$PYTHON_SKIP" = true ]; then
    WARNING "Skipping Python/NumPy installation, as requested..."
  elif [ "$PYTHON_FORCE_BUILD" = true ]; then
    INFO "Forced Python/NumPy building, as requested..."
    _do_compile_python=true
  else
    check_package_version_ge_DEB python3-dev $PYTHON_VERSION_MIN
    if [ $? -eq 0 ]; then
      PYTHON_VERSION_INSTALLED=$(echo `get_package_version_DEB python3-dev` | sed -r 's/^([0-9]+\.[0-9]+).*/\1/')

      install_packages_DEB python3-dev
      clean_Python
      PRINT ""
      if [ "$NUMPY_SKIP" = true ]; then
        WARNING "Skipping NumPy installation, as requested..."
      else
        check_package_DEB python3-numpy
        if [ $? -eq 0 ]; then
          install_packages_DEB python3-numpy
        else
          WARNING "Sorry, using python package but no valid numpy package available!" \
                  "    Use --build-numpy to force building of both Python and NumPy."
        fi
      fi
    else
      _do_compile_python=true
    fi
  fi

  if $_do_compile_python; then
    install_packages_DEB libffi-dev
    compile_Python
    PRINT ""
    if [ "$NUMPY_SKIP" = true ]; then
      WARNING "Skipping NumPy installation, as requested..."
    else
      compile_Numpy
    fi
  fi


  PRINT ""
  if [ "$BOOST_SKIP" = true ]; then
    WARNING "Skipping Boost installation, as requested..."
  elif [ "$BOOST_FORCE_BUILD" = true ]; then
    INFO "Forced Boost building, as requested..."
    compile_Boost
  else
    check_package_version_ge_DEB libboost-dev $BOOST_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB libboost-dev

      boost_version=$(echo `get_package_version_DEB libboost-dev` | sed -r 's/^([0-9]+\.[0-9]+).*/\1/')

      install_packages_DEB libboost-{filesystem,iostreams,locale,regex,system,thread,wave,program-options}$boost_version-dev
      clean_Boost
    else
      compile_Boost
    fi
  fi


  PRINT ""
  if [ "$TBB_SKIP" = true ]; then
    WARNING "Skipping TBB installation, as requested..."
  elif [ "$TBB_FORCE_BUILD" = true ]; then
    INFO "Forced TBB building, as requested..."
    compile_TBB
  else
    check_package_version_ge_DEB libtbb-dev $TBB_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB libtbb-dev
      clean_TBB
    else
      compile_TBB
    fi
  fi


  PRINT ""
  if [ "$OCIO_SKIP" = true ]; then
    WARNING "Skipping OpenColorIO installation, as requested..."
  elif [ "$OCIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenColorIO building, as requested..."
    compile_OCIO
  else
    # XXX Always force build of own OCIO, until linux distro guys update their package to default libyaml-cpp ver (0.5)!
    #check_package_version_ge_DEB libopencolorio-dev $OCIO_VERSION_MIN
    #if [ $? -eq 0 ]; then
      #install_packages_DEB libopencolorio-dev
      #clean_OCIO
    #else
      compile_OCIO
    #fi
  fi


  PRINT ""
  if [ "$OPENEXR_SKIP" = true ]; then
    WARNING "Skipping ILMBase/OpenEXR installation, as requested..."
  elif [ "$OPENEXR_FORCE_BUILD" = true ]; then
    INFO "Forced ILMBase/OpenEXR building, as requested..."
    compile_OPENEXR
  else
    check_package_version_ge_DEB libopenexr-dev $OPENEXR_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB libopenexr-dev
      OPENEXR_VERSION=`get_package_version_DEB libopenexr-dev`
      ILMBASE_VERSION=$OPENEXR_VERSION
      clean_OPENEXR
    else
      compile_OPENEXR
    fi
  fi


  PRINT ""
  if [ "$OIIO_SKIP" = true ]; then
    WARNING "Skipping OpenImageIO installation, as requested..."
  elif [ "$OIIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenImageIO building, as requested..."
    compile_OIIO
  else
    # XXX Debian Testing / Ubuntu 16.04 pulls in WAY too many deps (gtk2/opencv ?!) incl. OCIO build against libyaml-cpp0.3 so build for now...
    #check_package_version_ge_lt_DEB libopenimageio-dev $OIIO_VERSION_MIN $OIIO_VERSION_MAX
    #if [ $? -eq 0 -a "$_with_built_openexr" = false ]; then
    #  install_packages_DEB libopenimageio-dev
    #  clean_OIIO
    #else
      compile_OIIO
    #fi
  fi


  PRINT ""
  have_llvm=false
  _do_compile_llvm=false
  if [ "$LLVM_SKIP" = true ]; then
    WARNING "Skipping LLVM installation, as requested (this also implies skipping OSL!)..."
    OSL_SKIP=true
  elif [ "$LLVM_FORCE_BUILD" = true ]; then
    INFO "Forced LLVM building, as requested..."
    _do_compile_llvm=true
  else
    check_package_DEB clang-$LLVM_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB llvm-$LLVM_VERSION_MIN-dev clang-$LLVM_VERSION_MIN
      have_llvm=true
      LLVM_VERSION_FOUND=$LLVM_VERSION_MIN
      clean_LLVM
    else
      _do_compile_llvm=true
    fi
  fi

  if [ "$_do_compile_llvm" = true ]; then
    install_packages_DEB libffi-dev
    # LLVM can't find the debian ffi header dir
    _FFI_INCLUDE_DIR=`dpkg -L libffi-dev | grep -e ".*/ffi.h" | sed -r 's/(.*)\/ffi.h/\1/'`
    PRINT ""
    compile_LLVM
    have_llvm=true
    LLVM_VERSION_FOUND=$LLVM_VERSION
  fi


  PRINT ""
  _do_compile_osl=false
  if [ "$OSL_SKIP" = true ]; then
    WARNING "Skipping OpenShadingLanguage installation, as requested..."
  elif [ "$OSL_FORCE_BUILD" = true ]; then
    INFO "Forced OpenShadingLanguage building, as requested..."
    _do_compile_osl=true
  else
    # No package currently!
    _do_compile_osl=true
  fi

  if [ "$_do_compile_osl" = true ]; then
    if [ "$have_llvm" = true ]; then
      PRINT ""
      compile_OSL
    else
      WARNING "No LLVM available, cannot build OSL!"
    fi
  fi


  PRINT ""
  if [ "$OSD_SKIP" = true ]; then
    WARNING "Skipping OpenSubdiv installation, as requested..."
  elif [ "$OSD_FORCE_BUILD" = true ]; then
    INFO "Forced OpenSubdiv building, as requested..."
    compile_OSD
  else
    # No package currently!
    PRINT ""
    compile_OSD
  fi

  PRINT ""
  if [ "$OPENVDB_SKIP" = true ]; then
    WARNING "Skipping OpenVDB installation, as requested..."
  elif [ "$OPENVDB_FORCE_BUILD" = true ]; then
    INFO "Forced OpenVDB building, as requested..."
    compile_OPENVDB
  else
    check_package_version_ge_DEB libopenvdb-dev $OPENVDB_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_DEB libopenvdb-dev libblosc-dev
      clean_OPENVDB
    else
      compile_OPENVDB
    fi
  fi

  PRINT ""
  if [ "$ALEMBIC_SKIP" = true ]; then
    WARNING "Skipping Alembic installation, as requested..."
  elif [ "$ALEMBIC_FORCE_BUILD" = true ]; then
    INFO "Forced Alembic building, as requested..."
    compile_ALEMBIC
  else
    compile_ALEMBIC
  fi

  PRINT ""
  if [ "$USD_SKIP" = true ]; then
    WARNING "Skipping USD installation, as requested..."
  elif [ "$USD_FORCE_BUILD" = true ]; then
    INFO "Forced USD building, as requested..."
    compile_USD
  else
    compile_USD
  fi

  if [ "$WITH_OPENCOLLADA" = true ]; then
    _do_compile_collada=false
    PRINT ""
    if [ "$OPENCOLLADA_SKIP" = true ]; then
      WARNING "Skipping OpenCOLLADA installation, as requested..."
    elif [ "$OPENCOLLADA_FORCE_BUILD" = true ]; then
      INFO "Forced OpenCollada building, as requested..."
      _do_compile_collada=true
    else
      # No package currently!
      _do_compile_collada=true
    fi

    if [ "$_do_compile_collada" = true ]; then
      install_packages_DEB libpcre3-dev
      # Find path to libxml shared lib...
      _XML2_LIB=`dpkg -L libxml2-dev | grep -e ".*/libxml2.so"`
      # No package
      PRINT ""
      compile_OpenCOLLADA
    fi
  fi

  if [ "$WITH_EMBREE" = true ]; then
    _do_compile_embree=false
    PRINT ""
    if [ "$EMBREE_SKIP" = true ]; then
      WARNING "Skipping Embree installation, as requested..."
    elif [ "$EMBREE_FORCE_BUILD" = true ]; then
      INFO "Forced Embree building, as requested..."
      _do_compile_embree=true
    else
      # No package currently!
      _do_compile_embree=true
    fi

    if [ "$_do_compile_embree" = true ]; then
      compile_Embree
    fi
  fi

  if [ "$WITH_OIDN" = true ]; then
    _do_compile_oidn=false
    PRINT ""
    if [ "$OIDN_SKIP" = true ]; then
      WARNING "Skipping OpenImgeDenoise installation, as requested..."
    elif [ "$OIDN_FORCE_BUILD" = true ]; then
      INFO "Forced OpenImageDenoise building, as requested..."
      _do_compile_oidn=true
    else
      # No package currently!
      _do_compile_oidn=true
    fi

    if [ "$_do_compile_oidn" = true ]; then
      compile_OIDN
    fi
  fi

  PRINT ""
  if [ "$FFMPEG_SKIP" = true ]; then
    WARNING "Skipping FFMpeg installation, as requested..."
  elif [ "$FFMPEG_FORCE_BUILD" = true ]; then
    INFO "Forced FFMpeg building, as requested..."
    compile_FFmpeg
  else
    # XXX Debian Testing / Ubuntu 16.04 finally includes FFmpeg, so check as usual
    check_package_DEB ffmpeg
    if [ $? -eq 0 ]; then
      check_package_version_ge_DEB ffmpeg $FFMPEG_VERSION_MIN
      if [ $? -eq 0 ]; then
        install_packages_DEB libavdevice-dev
        clean_FFmpeg
      else
        compile_FFmpeg
      fi
    else
      compile_FFmpeg
    fi
  fi

  PRINT ""
  if [ "$XR_OPENXR_SKIP" = true ]; then
    WARNING "Skipping OpenXR-SDK installation, as requested..."
  elif [ "$XR_OPENXR_FORCE_BUILD" = true ]; then
    INFO "Forced OpenXR-SDK building, as requested..."
    compile_XR_OpenXR_SDK
  else
    # No package currently!
    PRINT ""
    compile_XR_OpenXR_SDK
  fi
}


# ----------------------------------------------------------------------------
# Install on RPM-like

rpm_flavour() {
  if [ -f /etc/redhat-release ]; then
    if [ "`grep '[6-7]\.' /etc/redhat-release`" ]; then
      RPM="RHEL"
    else
      RPM="FEDORA"
    fi
  elif [ -f /etc/SuSE-release ]; then
    RPM="SUSE"
  fi
}

get_package_version_RPM() {
  rpm_flavour
  if [ "$RPM" = "RHEL" ]; then
    yum info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'
  elif [ "$RPM" = "FEDORA" ]; then
    dnf info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'
  elif [ "$RPM" = "SUSE" ]; then
    zypper info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+).*/\1/'
  fi
}

check_package_RPM() {
  rpm_flavour
  if [ "$RPM" = "RHEL" ]; then
    r=`yum info $1 | grep -c 'Summary'`
  elif [ "$RPM" = "FEDORA" ]; then
    r=`dnf info $1 | grep -c 'Summary'`
  elif [ "$RPM" = "SUSE" ]; then
    r=`zypper info $1 | grep -c 'Summary'`
  fi

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_version_match_RPM() {
  v=`get_package_version_RPM $1`

  if [ -z "$v" ]; then
    return 1
  fi

  version_match $v $2
  return $?
}

check_package_version_ge_RPM() {
  v=`get_package_version_RPM $1`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge $v $2
  return $?
}

check_package_version_ge_lt_RPM() {
  v=`get_package_version_RPM $1`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge_lt $v $2 $3
  return $?
}

install_packages_RPM() {
  rpm_flavour
  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to install $@, you'll have to do it yourself..."
  fi
  if [ "$RPM" = "RHEL" ]; then
    $SUDO yum install -y $@
    if [ $? -ge 1 ]; then
      ERROR "yum failed to install requested packages, exiting."
      exit 1
    fi

  elif [ "$RPM" = "FEDORA" ]; then
    $SUDO dnf install -y $@
    if [ $? -ge 1 ]; then
      ERROR "dnf failed to install requested packages, exiting."
      exit 1
    fi

  elif [ "$RPM" = "SUSE" ]; then
    $SUDO zypper --non-interactive install --auto-agree-with-licenses $@
    if [ $? -ge 1 ]; then
      ERROR "zypper failed to install requested packages, exiting."
      exit 1
    fi
  fi
}

install_RPM() {
  PRINT ""
  INFO "Installing dependencies for RPM-based distribution"
  PRINT ""
  PRINT "`eval _echo "$COMMON_INFO"`"
  PRINT ""

  if [ "$NO_CONFIRM" = false ]; then
    read -p "Do you want to continue (Y/n)?"
    [ "$(echo ${REPLY:=Y} | tr [:upper:] [:lower:])" != "y" ] && exit
  fi

  # Enable non-free repositories for all flavours
  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to install third party repositories, you'll have to do it yourself..."
  else
    rpm_flavour
    if [ "$RPM" = "FEDORA" ]; then
      _fedora_rel="`egrep "[0-9]{1,}" /etc/fedora-release -o`"
      $SUDO dnf -y install --nogpgcheck \
      http://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$_fedora_rel.noarch.rpm \
      http://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$_fedora_rel.noarch.rpm

      $SUDO dnf -y update

    elif [ "$RPM" = "RHEL" ]; then
      if [ "`grep '[^.]6\.' /etc/redhat-release`" ]; then
        ERROR "Building with GCC 4.4 is not supported!"
        exit 1
      else
        $SUDO yum -y install --nogpgcheck \
        http://download.fedoraproject.org/pub/epel/7/$(uname -i)/e/epel-release-7-6.noarch.rpm \
        http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-5.el7.nux.noarch.rpm

        $SUDO yum -y update
      fi

    elif [ "$RPM" = "SUSE" ]; then
      # Packman repo now includes name in link...
      _suse_rel="`grep -w VERSION /etc/os-release | sed 's/[^0-9.]*//g'`"
      _suse_name="`grep -w NAME /etc/os-release | gawk '{print $2}' | sed 's/\"//'`"
      if [ $_suse_name ]; then
        _suse_rel="${_suse_name}_${_suse_rel}"
      fi

      PRINT ""
      INFO "About to add 'packman' repository from http://packman.inode.at/suse/openSUSE_$_suse_rel/"
      INFO "This is only needed if you do not already have a packman repository enabled..."
      read -p "Do you want to add this repo (Y/n)?"
      if [ "$(echo ${REPLY:=Y} | tr [:upper:] [:lower:])" == "y" ]; then
        INFO "    Installing packman..."
        $SUDO zypper ar -f -n packman http://ftp.gwdg.de/pub/linux/misc/packman/suse/openSUSE_$_suse_rel/ packman
        INFO "    Done."
      else
        INFO "    Skipping packman installation."
      fi
      $SUDO zypper --non-interactive --gpg-auto-import-keys update --auto-agree-with-licenses
    fi
  fi

  # These libs should always be available in fedora/suse official repository...
  OPENJPEG_DEV="openjpeg2-devel"
  VORBIS_DEV="libvorbis-devel"
  OGG_DEV="libogg-devel"
  THEORA_DEV="libtheora-devel"

  _packages="gcc gcc-c++ git make cmake tar bzip2 xz findutils flex bison \
             libtiff-devel libjpeg-devel libpng-devel sqlite-devel fftw-devel SDL2-devel \
             libX11-devel libXi-devel libXcursor-devel libXrandr-devel libXinerama-devel \
             wget ncurses-devel readline-devel $OPENJPEG_DEV openal-soft-devel \
             glew-devel yasm $THEORA_DEV $VORBIS_DEV $OGG_DEV patch \
             libxml2-devel yaml-cpp-devel tinyxml-devel jemalloc-devel"

  OPENJPEG_USE=true
  VORBIS_USE=true
  OGG_USE=true
  THEORA_USE=true

  if [ "$RPM" = "FEDORA" -o "$RPM" = "RHEL" ]; then
    _packages="$_packages freetype-devel"

    if [ "$WITH_JACK" = true ]; then
      _packages="$_packages jack-audio-connection-kit-devel"
    fi

    PRINT ""
    install_packages_RPM $_packages

    PRINT ""
    X264_DEV="x264-devel"
    check_package_version_ge_RPM $X264_DEV $X264_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM $X264_DEV
      X264_USE=true
    fi

    if [ "$WITH_ALL" = true ]; then
      PRINT ""
      XVID_DEV="xvidcore-devel"
      check_package_RPM $XVID_DEV
      if [ $? -eq 0 ]; then
        install_packages_RPM $XVID_DEV
        XVID_USE=true
      fi

      PRINT ""
      MP3LAME_DEV="lame-devel"
      check_package_RPM $MP3LAME_DEV
      if [ $? -eq 0 ]; then
        install_packages_RPM $MP3LAME_DEV
        MP3LAME_USE=true
      fi
    fi

  elif [ "$RPM" = "SUSE" ]; then
    _packages="$_packages freetype2-devel"

    PRINT ""
    install_packages_RPM $_packages

    PRINT ""
    X264_DEV="libx264-devel"
    check_package_version_ge_RPM $X264_DEV $X264_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM $X264_DEV
      X264_USE=true
    fi

    if [ "$WITH_ALL" = true ]; then
      PRINT ""
      XVID_DEV="libxvidcore-devel"
      check_package_RPM $XVID_DEV
      if [ $? -eq 0 ]; then
        install_packages_RPM $XVID_DEV
        XVID_USE=true
      fi

      PRINT ""
      MP3LAME_DEV="libmp3lame-devel"
      check_package_RPM $MP3LAME_DEV
      if [ $? -eq 0 ]; then
        install_packages_RPM $MP3LAME_DEV
        MP3LAME_USE=true
      fi
    fi
  fi

  PRINT""
  LIBSNDFILE_DEV="libsndfile-devel"
  check_package_RPM $LIBSNDFILE_DEV
  if [ $? -eq 0 ]; then
    install_packages_RPM $LIBSNDFILE_DEV
  fi

  if [ "$WITH_ALL" = true ]; then
    PRINT ""
    VPX_DEV="libvpx-devel"
    check_package_version_ge_RPM $VPX_DEV $VPX_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM $VPX_DEV
      VPX_USE=true
    fi

    PRINT ""
    install_packages_RPM libspnav-devel

    PRINT ""
    OPUS_DEV="libopus-devel"
    check_package_version_ge_RPM $OPUS_DEV $OPUS_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM $OPUS_DEV
      OPUS_USE=true
    fi
  fi

  PRINT ""
  CLANG_FORMAT="clang"  # Yeah, on fedora/suse clang-format is part of main clang package...
  check_package_version_ge_RPM $CLANG_FORMAT $CLANG_FORMAT_VERSION_MIN
  if [ $? -eq 0 ]; then
    install_packages_RPM $CLANG_FORMAT
  else
    PRINT ""
    WARNING "clang-format $CLANG_FORMAT_VERSION_MIN or higher not found, this is NOT needed to get Blender compiling..."
    PRINT ""
  fi

  PRINT ""
  _do_compile_python=false
  if [ "$PYTHON_SKIP" = true ]; then
    WARNING "Skipping Python installation, as requested..."
  elif [ "$PYTHON_FORCE_BUILD" = true ]; then
    INFO "Forced Python/NumPy building, as requested..."
    _do_compile_python=true
  else
    check_package_version_ge_RPM python3-devel $PYTHON_VERSION_MIN
    if [ $? -eq 0 ]; then
      PYTHON_VERSION_INSTALLED=$(echo `get_package_version_RPM python3-devel` | sed -r 's/^([0-9]+\.[0-9]+).*/\1/')

      install_packages_RPM python3-devel
      clean_Python
      PRINT ""
      if [ "$NUMPY_SKIP" = true ]; then
        WARNING "Skipping NumPy installation, as requested..."
      else
        check_package_version_ge_RPM python3-numpy $NUMPY_VERSION_MIN
        if [ $? -eq 0 ]; then
          install_packages_RPM python3-numpy
        else
          WARNING "Sorry, using python package but no valid numpy package available!" \
                  "    Use --build-numpy to force building of both Python and NumPy."
        fi
      fi
    else
      _do_compile_python=true
    fi
  fi

  if [ "$_do_compile_python" = true ]; then
    install_packages_RPM libffi-devel
    compile_Python
    PRINT ""
    if [ "$NUMPY_SKIP" = true ]; then
      WARNING "Skipping NumPy installation, as requested..."
    else
      compile_Numpy
    fi
  fi


  PRINT ""
  _do_compile_boost=false
  if [ "$BOOST_SKIP" = true ]; then
    WARNING "Skipping Boost installation, as requested..."
  elif [ "$BOOST_FORCE_BUILD" = true ]; then
    INFO "Forced Boost building, as requested..."
    _do_compile_boost=true
  else
    check_package_version_ge_RPM boost-devel $BOOST_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM boost-devel
      clean_Boost
    else
      _do_compile_boost=true
    fi
  fi

  if [ "$_do_compile_boost" = true ]; then
    if [ "$RPM" = "SUSE" ]; then
      install_packages_RPM gcc-fortran
    else
      install_packages_RPM libquadmath-devel bzip2-devel
    fi
    PRINT ""
    compile_Boost
  fi


  PRINT ""
  if [ "$TBB_SKIP" = true ]; then
    WARNING "Skipping TBB installation, as requested..."
  elif [ "$TBB_FORCE_BUILD" = true ]; then
    INFO "Forced TBB building, as requested..."
    compile_TBB
  else
    check_package_version_ge_RPM tbb-devel $TBB_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM tbb-devel
      clean_TBB
    else
      compile_TBB
    fi
  fi


  PRINT ""
  if [ "$OCIO_SKIP" = true ]; then
    WARNING "Skipping OpenColorIO installation, as requested..."
  elif [ "$OCIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenColorIO building, as requested..."
    compile_OCIO
  else
    if [ "$RPM" = "SUSE" ]; then
      check_package_version_ge_RPM OpenColorIO-devel $OCIO_VERSION_MIN
      if [ $? -eq 0 ]; then
        install_packages_RPM OpenColorIO-devel
        clean_OCIO
      else
        compile_OCIO
      fi
    # XXX Fedora/RHEL OCIO still depends on libyaml-cpp v0.3 even when system default is v0.5!
    else
      compile_OCIO
    fi
  fi

  PRINT ""
  if [ "$OPENEXR_SKIP" = true ]; then
    WARNING "Skipping ILMBase/OpenEXR installation, as requested..."
  elif [ "$OPENEXR_FORCE_BUILD" = true ]; then
    INFO "Forced ILMBase/OpenEXR building, as requested..."
    compile_OPENEXR
  else
    check_package_version_ge_RPM openexr-devel $OPENEXR_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM openexr-devel
      OPENEXR_VERSION=`get_package_version_RPM openexr-devel`
      ILMBASE_VERSION=$OPENEXR_VERSION
      clean_OPENEXR
    else
      compile_OPENEXR
    fi
  fi

  PRINT ""
  if [ "$OIIO_SKIP" = true ]; then
    WARNING "Skipping OpenImageIO installation, as requested..."
  elif [ "$OIIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenImageIO building, as requested..."
    compile_OIIO
  else
    # XXX RPM distros pulls in too much and depends on old libs, so better to build for now...
    #check_package_version_ge_lt_RPM OpenImageIO-devel $OIIO_VERSION_MIN $OIIO_VERSION_MAX
    #if [ $? -eq 0 -a $_with_built_openexr == false ]; then
    #  install_packages_RPM OpenImageIO-devel
    #  clean_OIIO
    #else
      compile_OIIO
    #fi
  fi


  PRINT ""
  have_llvm=false
  _do_compile_llvm=false
  if [ "$LLVM_SKIP" = true ]; then
    WARNING "Skipping LLVM installation, as requested (this also implies skipping OSL!)..."
    OSL_SKIP=true
  elif [ "$LLVM_FORCE_BUILD" = true ]; then
    INFO "Forced LLVM building, as requested..."
    _do_compile_llvm=true
  else
    if [ "$RPM" = "SUSE" ]; then
      CLANG_DEV="llvm-clang-devel"
    else
      CLANG_DEV="clang-devel"
    fi
    check_package_version_match_RPM $CLANG_DEV $LLVM_VERSION
    if [ $? -eq 0 ]; then
      install_packages_RPM llvm-devel $CLANG_DEV
      have_llvm=true
      LLVM_VERSION_FOUND=$LLVM_VERSION
      clean_LLVM
    else
      _do_compile_llvm=true
    fi
  fi

  if [ "$_do_compile_llvm" = true ]; then
    install_packages_RPM libffi-devel
    # LLVM can't find the fedora ffi header dir...
    _FFI_INCLUDE_DIR=`rpm -ql libffi-devel | grep -e ".*/ffi.h" | sed -r 's/(.*)\/ffi.h/\1/'`
    PRINT ""
    compile_LLVM
    have_llvm=true
    LLVM_VERSION_FOUND=$LLVM_VERSION
  fi


  PRINT ""
  _do_compile_osl=false
  if [ "$OSL_SKIP" = true ]; then
    WARNING "Skipping OpenShadingLanguage installation, as requested..."
  elif [ "$OSL_FORCE_BUILD" = true ]; then
    INFO "Forced OpenShadingLanguage building, as requested..."
    _do_compile_osl=true
  else
    # No package currently!
    _do_compile_osl=true
  fi

  if [ "$_do_compile_osl" = true ]; then
    if [ "$have_llvm" = true ]; then
      PRINT ""
      compile_OSL
    else
      WARNING "No LLVM available, cannot build OSL!"
    fi
  fi


  PRINT ""
  if [ "$OSD_SKIP" = true ]; then
    WARNING "Skipping OpenSubdiv installation, as requested..."
  elif [ "$OSD_FORCE_BUILD" = true ]; then
    INFO "Forced OpenSubdiv building, as requested..."
    compile_OSD
  else
    # No package currently!
    compile_OSD
  fi


  PRINT ""
  if [ "$OPENVDB_SKIP" = true ]; then
    WARNING "Skipping OpenVDB installation, as requested..."
  elif [ "$OPENVDB_FORCE_BUILD" = true ]; then
    INFO "Forced OpenVDB building, as requested..."
    compile_OPENVDB
  else
    # No package currently!
    compile_OPENVDB
  fi

  PRINT ""
  if [ "$ALEMBIC_SKIP" = true ]; then
    WARNING "Skipping Alembic installation, as requested..."
  elif [ "$ALEMBIC_FORCE_BUILD" = true ]; then
    INFO "Forced Alembic building, as requested..."
    compile_ALEMBIC
  else
    # No package currently!
    compile_ALEMBIC
  fi

  PRINT ""
  if [ "$USD_SKIP" = true ]; then
    WARNING "Skipping USD installation, as requested..."
  elif [ "$USD_FORCE_BUILD" = true ]; then
    INFO "Forced USD building, as requested..."
    compile_USD
  else
    compile_USD
  fi

  if [ "$WITH_OPENCOLLADA" = true ]; then
    PRINT ""
    _do_compile_collada=false
    if [ "$OPENCOLLADA_SKIP" = true ]; then
      WARNING "Skipping OpenCOLLADA installation, as requested..."
    elif [ "$OPENCOLLADA_FORCE_BUILD" = true ]; then
      INFO "Forced OpenCollada building, as requested..."
      _do_compile_collada=true
    else
      # No package...
      _do_compile_collada=true
    fi

    if [ "$_do_compile_collada" = true ]; then
      install_packages_RPM pcre-devel
      # Find path to libxml shared lib...
      _XML2_LIB=`rpm -ql libxml2-devel | grep -e ".*/libxml2.so"`
      PRINT ""
      compile_OpenCOLLADA
    fi
  fi

  if [ "$WITH_EMBREE" = true ]; then
    PRINT ""
    _do_compile_embree=false
    if [ "$OPENCOLLADA_SKIP" = true ]; then
      WARNING "Skipping Embree installation, as requested..."
    elif [ "$EMBREE_FORCE_BUILD" = true ]; then
      INFO "Forced Embree building, as requested..."
      _do_compile_embree=true
    else
      # No package...
      _do_compile_embree=true
    fi

    if [ "$_do_compile_embree" = true ]; then
      compile_Embree
    fi
  fi

  if [ "$WITH_OIDN" = true ]; then
    _do_compile_oidn=false
    PRINT ""
    if [ "$OIDN_SKIP" = true ]; then
      WARNING "Skipping OpenImgeDenoise installation, as requested..."
    elif [ "$OIDN_FORCE_BUILD" = true ]; then
      INFO "Forced OpenImageDenoise building, as requested..."
      _do_compile_oidn=true
    else
      # No package currently!
      _do_compile_oidn=true
    fi

    if [ "$_do_compile_oidn" = true ]; then
      compile_OIDN
    fi
  fi

  PRINT ""
  if [ "$FFMPEG_SKIP" = true ]; then
    WARNING "Skipping FFMpeg installation, as requested..."
  elif [ "$FFMPEG_FORCE_BUILD" = true ]; then
    INFO "Forced FFMpeg building, as requested..."
    compile_FFmpeg
  else
    check_package_version_ge_RPM ffmpeg-devel $FFMPEG_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_RPM ffmpeg ffmpeg-devel
      clean_FFmpeg
    else
      compile_FFmpeg
    fi
  fi

  PRINT ""
  if [ "$XR_OPENXR_SKIP" = true ]; then
    WARNING "Skipping OpenXR-SDK installation, as requested..."
  elif [ "$XR_OPENXR_FORCE_BUILD" = true ]; then
    INFO "Forced OpenXR-SDK building, as requested..."
    compile_XR_OpenXR_SDK
  else
    # No package currently!
    compile_XR_OpenXR_SDK
  fi
}


# ----------------------------------------------------------------------------
# Install on ARCH-like

get_package_version_ARCH() {
  pacman -Si $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+?(([0-9]+\.?)+).*/\1/'
}

check_package_ARCH() {
  r=`pacman -Si $1 | grep -c 'Description'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_version_match_ARCH() {
  v=`get_package_version_ARCH $1`

  if [ -z "$v" ]; then
    return 1
  fi

  version_match $v $2
  return $?
}

check_package_version_ge_ARCH() {
  v=`get_package_version_ARCH $1`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge $v $2
  return $?
}

check_package_version_ge_lt_ARCH() {
  v=`get_package_version_ARCH $1`

  if [ -z "$v" ]; then
    return 1
  fi

  version_ge_lt $v $2 $3
  return $?
}

install_packages_ARCH() {
  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to run pacman for $@, you'll have to do it yourself..."
  else
    $SUDO pacman -S --needed --noconfirm $@
    if [ $? -ge 1 ]; then
      ERROR "pacman failed to install requested packages, exiting."
      exit 1
    fi
  fi
}

install_ARCH() {
  PRINT ""
  INFO "Installing dependencies for ARCH-based distribution"
  PRINT ""
  PRINT "`eval _echo "$COMMON_INFO"`"
  PRINT ""

  if [ "$NO_CONFIRM" = false ]; then
    read -p "Do you want to continue (Y/n)?"
    [ "$(echo ${REPLY:=Y} | tr [:upper:] [:lower:])" != "y" ] && exit
  fi

  # Check for sudo...
  if [ $SUDO ]; then
    if [ ! -x "/usr/bin/sudo" ]; then
      PRINT ""
      ERROR "This script requires sudo but it is not installed."
      PRINT "Please setup sudo according to:"
      PRINT "https://wiki.archlinux.org/index.php/Sudo"
      PRINT "and try again."
      PRINT ""
      exit
    fi
  fi

  if [ ! $SUDO ]; then
    WARNING "--no-sudo enabled, impossible to run pacman -Sy, you'll have to do it yourself..."
  else
    $SUDO pacman -Sy
  fi

  # These libs should always be available in arch official repository...
  OPENJPEG_DEV="openjpeg2"
  VORBIS_DEV="libvorbis"
  OGG_DEV="libogg"
  THEORA_DEV="libtheora"

  BASE_DEVEL="base-devel"

  # Avoid conflicts when gcc-multilib is installed
  pacman -Qi gcc-multilib &>/dev/null
  if [ $? -eq 0 ]; then
    BASE_DEVEL=`pacman -Sgq base-devel | sed -e 's/^gcc$/gcc-multilib/g' | paste -s -d' '`
  fi

  _packages="$BASE_DEVEL git cmake \
             libxi libxcursor libxrandr libxinerama glew libpng libtiff wget openal \
             $OPENJPEG_DEV $VORBIS_DEV $OGG_DEV $THEORA_DEV yasm sdl2 fftw \
             libxml2 yaml-cpp tinyxml python-requests jemalloc"

  OPENJPEG_USE=true
  VORBIS_USE=true
  OGG_USE=true
  THEORA_USE=true

  if [ "$WITH_ALL" = true ]; then
    _packages="$_packages libspnav"
  fi

  if [ "$WITH_JACK" = true ]; then
    _packages="$_packages jack2"
  fi

  PRINT ""
  install_packages_ARCH $_packages

  PRINT""
  LIBSNDFILE_DEV="libsndfile"
  check_package_ARCH $LIBSNDFILE_DEV
  if [ $? -eq 0 ]; then
    install_packages_ARCH $LIBSNDFILE_DEV
  fi

  PRINT ""
  X264_DEV="x264"
  check_package_version_ge_ARCH $X264_DEV $X264_VERSION_MIN
  if [ $? -eq 0 ]; then
    install_packages_ARCH $X264_DEV
    X264_USE=true
  fi

  if [ "$WITH_ALL" = true ]; then
    PRINT ""
    XVID_DEV="xvidcore"
    check_package_ARCH $XVID_DEV
    if [ $? -eq 0 ]; then
      install_packages_ARCH $XVID_DEV
      XVID_USE=true
    fi

    PRINT ""
    MP3LAME_DEV="lame"
    check_package_ARCH $MP3LAME_DEV
    if [ $? -eq 0 ]; then
      install_packages_ARCH $MP3LAME_DEV
      MP3LAME_USE=true
    fi

    PRINT ""
    VPX_DEV="libvpx"
    check_package_version_ge_ARCH $VPX_DEV $VPX_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH $VPX_DEV
      VPX_USE=true
    fi

    PRINT ""
    OPUS_DEV="opus"
    check_package_version_ge_ARCH $OPUS_DEV $OPUS_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH $OPUS_DEV
      OPUS_USE=true
    fi
  fi


  PRINT ""
  CLANG_FORMAT="clang"  # Yeah, on arch clang-format is part of main clang package...
  check_package_version_ge_ARCH $CLANG_FORMAT $CLANG_FORMAT_VERSION_MIN
  if [ $? -eq 0 ]; then
    install_packages_ARCH $CLANG_FORMAT
  else
    PRINT ""
    WARNING "clang-format $CLANG_FORMAT_VERSION_MIN or higher not found, this is NOT needed to get Blender compiling..."
    PRINT ""
  fi


  PRINT ""
  _do_compile_python=false
  if [ "$PYTHON_SKIP" = true ]; then
    WARNING "Skipping Python installation, as requested..."
  elif [ "$PYTHON_FORCE_BUILD" = true ]; then
    INFO "Forced Python/NumPy building, as requested..."
    _do_compile_python=true
  else
    check_package_version_ge_ARCH python $PYTHON_VERSION_MIN
    if [ $? -eq 0 ]; then
      PYTHON_VERSION_INSTALLED=$(echo `get_package_version_ARCH python` | sed -r 's/^([0-9]+\.[0-9]+).*/\1/')

      install_packages_ARCH python
      clean_Python
      PRINT ""
      if [ "$NUMPY_SKIP" = true ]; then
        WARNING "Skipping NumPy installation, as requested..."
      else
        check_package_version_ge_ARCH python-numpy $NUMPY_VERSION_MIN
        if [ $? -eq 0 ]; then
          install_packages_ARCH python-numpy
        else
          WARNING "Sorry, using python package but no valid numpy package available!" \
                  "Use --build-numpy to force building of both Python and NumPy."
        fi
      fi
    else
      _do_compile_python=true
    fi
  fi

  if [ "$_do_compile_python" = true ]; then
    install_packages_ARCH libffi
    compile_Python
    PRINT ""
    if [ "$NUMPY_SKIP" = true ]; then
      WARNING "Skipping NumPy installation, as requested..."
    else
      compile_Numpy
    fi
  fi


  PRINT ""
  if [ "$BOOST_SKIP" = true ]; then
    WARNING "Skipping Boost installation, as requested..."
  elif [ "$BOOST_FORCE_BUILD" = true ]; then
    INFO "Forced Boost building, as requested..."
    compile_Boost
  else
    check_package_version_ge_ARCH boost $BOOST_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH boost
      clean_Boost
    else
      compile_Boost
    fi
  fi


  PRINT ""
  if [ "$TBB_SKIP" = true ]; then
    WARNING "Skipping TBB installation, as requested..."
  elif [ "$TBB_FORCE_BUILD" = true ]; then
    INFO "Forced TBB building, as requested..."
    compile_TBB
  else
    check_package_version_ge_ARCH intel-tbb $TBB_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH intel-tbb
      clean_TBB
    else
      compile_TBB
    fi
  fi


  PRINT ""
  if [ "$OCIO_SKIP" = true ]; then
    WARNING "Skipping OpenColorIO installation, as requested..."
  elif [ "$OCIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenColorIO building, as requested..."
    compile_OCIO
  else
    check_package_version_ge_ARCH opencolorio $OCIO_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH opencolorio
      clean_OCIO
    else
      compile_OCIO
    fi
  fi


  PRINT ""
  if [ "$OPENEXR_SKIP" = true ]; then
    WARNING "Skipping ILMBase/OpenEXR installation, as requested..."
  elif [ "$OPENEXR_FORCE_BUILD" = true ]; then
    INFO "Forced ILMBase/OpenEXR building, as requested..."
    compile_OPENEXR
  else
    check_package_version_ge_ARCH openexr $OPENEXR_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH openexr
      OPENEXR_VERSION=`get_package_version_ARCH openexr`
      ILMBASE_VERSION=$OPENEXR_VERSION
      clean_OPENEXR
    else
      compile_OPENEXR
    fi
  fi


  PRINT ""
  if [ "$OIIO_SKIP" = true ]; then
    WARNING "Skipping OpenImageIO installation, as requested..."
  elif [ "$OIIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenImageIO building, as requested..."
    compile_OIIO
  else
    check_package_version_ge_lt_ARCH openimageio $OIIO_VERSION_MIN $OIIO_VERSION_MAX
    if [ $? -eq 0 ]; then
      install_packages_ARCH openimageio
      clean_OIIO
    else
      compile_OIIO
    fi
  fi


  PRINT ""
  have_llvm=false
  _do_compile_llvm=false
  if [ "$LLVM_SKIP" = true ]; then
    WARNING "Skipping LLVM installation, as requested (this also implies skipping OSL!)..."
    OSL_SKIP=true
  elif [ "$LLVM_FORCE_BUILD" = true ]; then
    INFO "Forced LLVM building, as requested..."
    _do_compile_llvm=true
  else
    check_package_version_match_ARCH llvm $LLVM_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH llvm clang
      have_llvm=true
      LLVM_VERSION=`get_package_version_ARCH llvm`
      LLVM_VERSION_FOUND=$LLVM_VERSION
      clean_LLVM
    else
      _do_compile_llvm=true
    fi
  fi

  if [ "$_do_compile_llvm" = true ]; then
    install_packages_ARCH libffi
    # LLVM can't find the arch ffi header dir...
    _FFI_INCLUDE_DIR=`pacman -Ql libffi | grep -e ".*/ffi.h" | awk '{print $2}' | sed -r 's/(.*)\/ffi.h/\1/'`
    PRINT ""
    compile_LLVM
    have_llvm=true
    LLVM_VERSION_FOUND=$LLVM_VERSION
  fi


  PRINT ""
  _do_compile_osl=false
  if [ "$OSL_SKIP" = true ]; then
    WARNING "Skipping OpenShadingLanguage installation, as requested..."
  elif [ "$OSL_FORCE_BUILD" = true ]; then
    INFO "Forced OpenShadingLanguage building, as requested..."
    _do_compile_osl=true
  else
    # XXX Compile for now due to requirement of LLVM 3.4 ...
    #check_package_version_ge_ARCH openshadinglanguage $OSL_VERSION_MIN
    #if [ $? -eq 0 ]; then
    #  install_packages_ARCH openshadinglanguage
    #  clean_OSL
    #else
      _do_compile_osl=true
    #fi
  fi

  if [ "$_do_compile_osl" = true ]; then
    if [ "$have_llvm" = true ]; then
      PRINT ""
      compile_OSL
    else
      WARNING "No LLVM available, cannot build OSL!"
    fi
  fi


  PRINT ""
  if [ "$OSD_SKIP" = true ]; then
    WARNING "Skipping OpenSubdiv installation, as requested..."
  elif [ "$OSD_FORCE_BUILD" = true ]; then
    INFO "Forced OpenSubdiv building, as requested..."
    compile_OSD
  else
    check_package_version_ge_ARCH opensubdiv $OSD_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH opensubdiv
      clean_OSD
    else
      compile_OSD
    fi
  fi


  PRINT ""
  if [ "$OPENVDB_SKIP" = true ]; then
    WARNING "Skipping OpenVDB installation, as requested..."
  elif [ "$OPENVDB_FORCE_BUILD" = true ]; then
    INFO "Forced OpenVDB building, as requested..."
    compile_OPENVDB
  else
    check_package_version_ge_ARCH openvdb $OPENVDB_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH openvdb
      clean_OPENVDB
    else
      compile_OPENVDB
    fi
  fi

  PRINT ""
  if [ "$ALEMBIC_SKIP" = true ]; then
    WARNING "Skipping Alembic installation, as requested..."
  elif [ "$ALEMBIC_FORCE_BUILD" = true ]; then
    INFO "Forced Alembic building, as requested..."
    compile_ALEMBIC
  else
    compile_ALEMBIC
  fi

  PRINT ""
  if [ "$USD_SKIP" = true ]; then
    WARNING "Skipping USD installation, as requested..."
  elif [ "$USD_FORCE_BUILD" = true ]; then
    INFO "Forced USD building, as requested..."
    compile_USD
  else
    compile_USD
  fi

  if [ "$WITH_OPENCOLLADA" = true ]; then
    PRINT ""
    _do_compile_collada=false
    if [ "$OPENCOLLADA_SKIP" = true ]; then
      WARNING "Skipping OpenCOLLADA installation, as requested..."
    elif [ "$OPENCOLLADA_FORCE_BUILD" = true ]; then
      INFO "Forced OpenCollada building, as requested..."
      _do_compile_collada=true
    else
      check_package_ARCH opencollada
      if [ $? -eq 0 ]; then
        install_packages_ARCH opencollada
        clean_OpenCOLLADA
      else
        _do_compile_collada=true
      fi
    fi

    if [ "$_do_compile_collada" = true ]; then
      install_packages_ARCH pcre
      # Find path to libxml shared lib...
      _XML2_LIB=`pacman -Ql libxml2 | grep -e ".*/libxml2.so$" | gawk '{print $2}'`
      PRINT ""
      compile_OpenCOLLADA
    fi
  fi

  if [ "$WITH_EMBREE" = true ]; then
    PRINT ""
    _do_compile_embree=false
    if [ "$EMBREE_SKIP" = true ]; then
      WARNING "Skipping Embree installation, as requested..."
    elif [ "$EMBREE_FORCE_BUILD" = true ]; then
      INFO "Forced Embree building, as requested..."
      _do_compile_embree=true
    else
      check_package_ARCH embree
      if [ $? -eq 0 ]; then
        install_packages_ARCH embree
        clean_Embree
      else
        _do_compile_embree=true
      fi
    fi

    if [ "$_do_compile_embree" = true ]; then
      compile_Embree
    fi
  fi

  if [ "$WITH_OIDN" = true ]; then
    _do_compile_oidn=false
    PRINT ""
    if [ "$OIDN_SKIP" = true ]; then
      WARNING "Skipping OpenImgeDenoise installation, as requested..."
    elif [ "$OIDN_FORCE_BUILD" = true ]; then
      INFO "Forced OpenImageDenoise building, as requested..."
      _do_compile_oidn=true
    else
      # No package currently!
      _do_compile_oidn=true
    fi

    if [ "$_do_compile_oidn" = true ]; then
      compile_OIDN
    fi
  fi

  PRINT ""
  if [ "$FFMPEG_SKIP" = true ]; then
    WARNING "Skipping FFMpeg installation, as requested..."
  elif [ "$FFMPEG_FORCE_BUILD" = true ]; then
    INFO "Forced FFMpeg building, as requested..."
    compile_FFmpeg
  else
    check_package_version_ge_ARCH ffmpeg $FFMPEG_VERSION_MIN
    if [ $? -eq 0 ]; then
      install_packages_ARCH ffmpeg
      clean_FFmpeg
    else
      compile_FFmpeg
    fi
  fi

  PRINT ""
  if [ "$XR_OPENXR_SKIP" = true ]; then
    WARNING "Skipping OpenXR-SDK installation, as requested..."
  elif [ "$XR_OPENXR_FORCE_BUILD" = true ]; then
    INFO "Forced OpenXR-SDK building, as requested..."
    compile_XR_OpenXR_SDK
  else
    # No package currently!
    compile_XR_OpenXR_SDK
  fi
}


# ----------------------------------------------------------------------------
# Install on other distro (very limited!)

install_OTHER() {
  PRINT ""
  WARNING "Attempt to build main dependencies for other linux distributions."
  PRINT ""
  PRINT "`eval _echo "$COMMON_INFO"`"
  PRINT ""

  ERROR "Failed to detect distribution type."
  PRINT ""
  PRINT "Your distribution is not supported by this script, you'll have to install dependencies and"
  PRINT "dev packages yourself. However, this script can still attempt to build main (complex) libraries for you,"
  PRINT "if you use '--build-foo' options (you can try '--build-all' one first)."
  PRINT ""
  PRINT "Quite obviously, it assumes dependencies from those libraries are already available, otherwise please"
  PRINT "install them (you can also use error messages printed out by build process to find missing libraries...)."
  PRINT ""
  PRINT "`eval _echo "$DEPS_COMMON_INFO"`"
  PRINT ""
  PRINT "`eval _echo "$DEPS_SPECIFIC_INFO"`"
  PRINT ""

  if [ "$NO_CONFIRM" = false ]; then
    read -p "Do you want to continue (Y/n)?"
    [ "$(echo ${REPLY:=Y} | tr [:upper:] [:lower:])" != "y" ] && exit
  fi

  PRINT ""
  if [ "$PYTHON_SKIP" = true ]; then
    WARNING "Skipping Python/NumPy installation, as requested..."
  elif [ "$PYTHON_FORCE_BUILD" = true ]; then
    INFO "Forced Python/NumPy building, as requested..."
    compile_Python
    PRINT ""
    if [ "$NUMPY_SKIP" = true ]; then
      WARNING "Skipping NumPy installation, as requested..."
    else
      compile_Numpy
    fi
  fi


  PRINT ""
  if [ "$BOOST_SKIP" = true ]; then
    WARNING "Skipping Boost installation, as requested..."
  elif [ "$BOOST_FORCE_BUILD" = true ]; then
    INFO "Forced Boost building, as requested..."
    compile_Boost
  fi


  PRINT ""
  if [ "$TBB_SKIP" = true ]; then
    WARNING "Skipping TBB installation, as requested..."
  elif [ "$TBB_FORCE_BUILD" = true ]; then
    INFO "Forced TBB building, as requested..."
    compile_TBB
  fi


  PRINT ""
  if [ "$OCIO_SKIP" = true ]; then
    WARNING "Skipping OpenColorIO installation, as requested..."
  elif [ "$OCIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenColorIO building, as requested..."
    compile_OCIO
  fi


  PRINT ""
  if [ "$OPENEXR_SKIP" = true ]; then
    WARNING "Skipping ILMBase/OpenEXR installation, as requested..."
  elif [ "$OPENEXR_FORCE_BUILD" = true ]; then
    INFO "Forced ILMBase/OpenEXR building, as requested..."
    compile_OPENEXR
  fi


  PRINT ""
  if [ "$OIIO_SKIP" = true ]; then
    WARNING "Skipping OpenImageIO installation, as requested..."
  elif [ "$OIIO_FORCE_BUILD" = true ]; then
    INFO "Forced OpenImageIO building, as requested..."
    compile_OIIO
  fi


  PRINT ""
  have_llvm=false
  if [ "$LLVM_SKIP" = true ]; then
    WARNING "Skipping LLVM installation, as requested (this also implies skipping OSL!)..."
  elif [ "$LLVM_FORCE_BUILD" = true ]; then
    INFO "Forced LLVM building, as requested..."
    compile_LLVM
    have_llvm=true
    LLVM_VERSION_FOUND=$LLVM_VERSION
  fi


  PRINT ""
  if [ "$OSL_SKIP" = true ]; then
    WARNING "Skipping OpenShadingLanguage installation, as requested..."
  elif [ "$OSL_FORCE_BUILD" = true ]; then
    INFO "Forced OpenShadingLanguage building, as requested..."
    if [ "$have_llvm" = true ]; then
      PRINT ""
      compile_OSL
    else
      WARNING "No LLVM available, cannot build OSL!"
    fi
  fi


  PRINT ""
  if [ "$OSD_SKIP" = true ]; then
    WARNING "Skipping OpenSubdiv installation, as requested..."
  elif [ "$OSD_FORCE_BUILD" = true ]; then
    INFO "Forced OpenSubdiv building, as requested..."
    compile_OSD
  fi


  if [ "$WITH_OPENCOLLADA" = true ]; then
    PRINT ""
    if [ "$OPENCOLLADA_SKIP" = true ]; then
      WARNING "Skipping OpenCOLLADA installation, as requested..."
    elif [ "$OPENCOLLADA_FORCE_BUILD" = true ]; then
      INFO "Forced OpenCollada building, as requested..."
      compile_OpenCOLLADA
    fi
  fi

  if [ "$WITH_EMBREE" = true ]; then
    PRINT ""
    if [ "$EMBREE_SKIP" = true ]; then
      WARNING "Skipping Embree installation, as requested..."
    elif [ "$EMBREE_FORCE_BUILD" = true ]; then
      INFO "Forced Embree building, as requested..."
      compile_Embree
    fi
  fi

  if [ "$WITH_OIDN" = true ]; then
    PRINT ""
    if [ "$OIDN_SKIP" = true ]; then
      WARNING "Skipping OpenImgeDenoise installation, as requested..."
    elif [ "$OIDN_FORCE_BUILD" = true ]; then
      INFO "Forced OpenImageDenoise building, as requested..."
      compile_OIDN
    fi
  fi

  PRINT ""
  if [ "$FFMPEG_SKIP" = true ]; then
    WARNING "Skipping FFMpeg installation, as requested..."
  elif [ "$FFMPEG_FORCE_BUILD" = true ]; then
    INFO "Forced FFMpeg building, as requested..."
    compile_FFmpeg
  fi

  PRINT ""
  if [ "$XR_OPENXR_SKIP" = true ]; then
    WARNING "Skipping OpenXR-SDK installation, as requested..."
  elif [ "$XR_OPENXR_FORCE_BUILD" = true ]; then
    INFO "Forced OpenXR-SDK building, as requested..."
    compile_XR_OpenXR_SDK
  fi
}

# ----------------------------------------------------------------------------
# Printing User Info

print_info_ffmpeglink_DEB() {
  dpkg -L $_packages | grep -e ".*\/lib[^\/]\+\.so" | gawk '{ printf(nlines ? "'"$_ffmpeg_list_sep"'%s" : "%s", gensub(/.*lib([^\/]+)\.so/, "\\1", "g", $0)); nlines++ }'
}

print_info_ffmpeglink_RPM() {
  rpm -ql $_packages | grep -e ".*\/lib[^\/]\+\.so" | gawk '{ printf(nlines ? "'"$_ffmpeg_list_sep"'%s" : "%s", gensub(/.*lib([^\/]+)\.so/, "\\1", "g", $0)); nlines++ }'
}

print_info_ffmpeglink_ARCH() {
  pacman -Ql $_packages | grep -e ".*\/lib[^\/]\+\.so$" | gawk '{ printf(nlines ? "'"$_ffmpeg_list_sep"'%s" : "%s", gensub(/.*lib([^\/]+)\.so/, "\\1", "g", $0)); nlines++ }'
}

print_info_ffmpeglink() {
  # This func must only print a ';'-separated list of libs...
  if [ -z "$DISTRO" ]; then
    ERROR "Failed to detect distribution type"
    exit 1
  fi

  # Create list of packages from which to get libs names...
  _packages=""

  if [ "$THEORA_USE" = true ]; then
    _packages="$_packages $THEORA_DEV"
  fi

  if [ "$VORBIS_USE" = true ]; then
    _packages="$_packages $VORBIS_DEV"
  fi

  if [ "$OGG_USE" = true ]; then
    _packages="$_packages $OGG_DEV"
  fi

  if [ "$XVID_USE" = true ]; then
    _packages="$_packages $XVID_DEV"
  fi

  if [ "$VPX_USE" = true ]; then
    _packages="$_packages $VPX_DEV"
  fi

  if [ "$OPUS_USE" = true ]; then
    _packages="$_packages $OPUS_DEV"
  fi

  if [ "$MP3LAME_USE" = true ]; then
    _packages="$_packages $MP3LAME_DEV"
  fi

  if [ "$X264_USE" = true ]; then
    _packages="$_packages $X264_DEV"
  fi

  if [ "$OPENJPEG_USE" = true ]; then
    _packages="$_packages $OPENJPEG_DEV"
  fi

  if [ "$DISTRO" = "DEB" ]; then
    print_info_ffmpeglink_DEB
  elif [ "$DISTRO" = "RPM" ]; then
    print_info_ffmpeglink_RPM
  elif [ "$DISTRO" = "ARCH" ]; then
    print_info_ffmpeglink_ARCH
  # XXX TODO!
  else
    PRINT "<Could not determine additional link libraries needed for ffmpeg, replace this by valid list of libs...>"
  fi
}

print_info() {
  PRINT ""
  PRINT ""
  PRINT "Ran with:"
  PRINT "    install_deps.sh $COMMANDLINE"
  PRINT ""
  PRINT ""
  PRINT "If you're using CMake add this to your configuration flags:"

  _buildargs="-U *SNDFILE* -U PYTHON* -U *BOOST* -U *Boost* -U *TBB*"
  _buildargs="$_buildargs -U *OPENCOLORIO* -U *OPENEXR* -U *OPENIMAGEIO* -U *LLVM* -U *CYCLES*"
  _buildargs="$_buildargs -U *OPENSUBDIV* -U *OPENVDB* -U *COLLADA* -U *FFMPEG* -U *ALEMBIC* -U *USD*"

  _1="-D WITH_CODEC_SNDFILE=ON"
  PRINT "  $_1"
  _buildargs="$_buildargs $_1"

  _1="-D PYTHON_VERSION=$PYTHON_VERSION_INSTALLED"
  PRINT "  $_1"
  _buildargs="$_buildargs $_1"
  if [ -d "$INST/python-$PYTHON_VERSION_INSTALLED" ]; then
    _1="-D PYTHON_ROOT_DIR=$INST/python-$PYTHON_VERSION_INSTALLED"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
  fi

  if [ -d $INST/boost ]; then
    _1="-D BOOST_ROOT=$INST/boost"
    _2="-D Boost_NO_SYSTEM_PATHS=ON"
    PRINT "  $_1"
    PRINT "  $_2"
    _buildargs="$_buildargs $_1 $_2"
  fi

  if [ -d $INST/tbb ]; then
    _1="-D TBB_ROOT_DIR=$INST/tbb"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
  fi

  if [ "$OCIO_SKIP" = false ]; then
    _1="-D WITH_OPENCOLORIO=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/ocio ]; then
      _1="-D OPENCOLORIO_ROOT_DIR=$INST/ocio"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ -d $INST/openexr ]; then
    _1="-D OPENEXR_ROOT_DIR=$INST/openexr"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
  fi

  if [ -d $INST/oiio ]; then
    _1="-D WITH_OPENIMAGEIO=ON"
    _2="-D OPENIMAGEIO_ROOT_DIR=$INST/oiio"
    PRINT "  $_1"
    PRINT "  $_2"
    _buildargs="$_buildargs $_1 $_2"
  fi

  if [ "$OSL_SKIP" = false ]; then
    _1="-D WITH_CYCLES_OSL=ON"
    _2="-D WITH_LLVM=ON"
    _3="-D LLVM_VERSION=$LLVM_VERSION_FOUND"
    PRINT "  $_1"
    PRINT "  $_2"
    PRINT "  $_3"
    _buildargs="$_buildargs $_1 $_2 $_3"
    if [ -d $INST/osl ]; then
      _1="-D OSL_ROOT_DIR=$INST/osl"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
    if [ -d $INST/llvm ]; then
      _1="-D LLVM_ROOT_DIR=$INST/llvm"
      _2="-D LLVM_STATIC=ON"
      PRINT "  $_1"
      PRINT "  $_2"
      _buildargs="$_buildargs $_1 $_2"
    fi
  else
    _1="-D WITH_CYCLES_OSL=OFF"
    _2="-D WITH_LLVM=OFF"
    PRINT "  $_1"
    PRINT "  $_2"
    _buildargs="$_buildargs $_1 $_2"
  fi

  if [ "$OSD_SKIP" = false ]; then
    _1="-D WITH_OPENSUBDIV=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/osd ]; then
      _1="-D OPENSUBDIV_ROOT_DIR=$INST/osd"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$OPENVDB_SKIP" = false ]; then
    _1="-D WITH_OPENVDB=ON"
    _2="-D WITH_OPENVDB_BLOSC=ON"
    PRINT "  $_1"
    PRINT "  $_2"
    _buildargs="$_buildargs $_1 $_2"
    if [ -d $INST/openvdb ]; then
      _1="-D OPENVDB_ROOT_DIR=$INST/openvdb"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
    if [ -d $INST/blosc ]; then
      _1="-D BLOSC_ROOT_DIR=$INST/blosc"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$WITH_OPENCOLLADA" = true ]; then
    _1="-D WITH_OPENCOLLADA=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/opencollada ]; then
      _1="-D OPENCOLLADA_ROOT_DIR=$INST/opencollada"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$WITH_EMBREE" = true ]; then
    _1="-D WITH_CYCLES_EMBREE=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/embree ]; then
      _1="-D EMBREE_ROOT_DIR=$INST/embree"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$WITH_OIDN" = true ]; then
    _1="-D WITH_OPENIMAGEDENOISE=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/oidn ]; then
      _1="-D OPENIMAGEDENOISE_ROOT_DIR=$INST/oidn"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$WITH_JACK" = true ]; then
    _1="-D WITH_JACK=ON"
    _2="-D WITH_JACK_DYNLOAD=ON"
    PRINT "  $_1"
    PRINT "  $_2"
    _buildargs="$_buildargs $_1 $_2"
  fi

  if [ "$ALEMBIC_SKIP" = false ]; then
    _1="-D WITH_ALEMBIC=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/alembic ]; then
      _1="-D ALEMBIC_ROOT_DIR=$INST/alembic"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$USD_SKIP" = false ]; then
    _1="-D WITH_USD=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/usd ]; then
      _1="-D USD_ROOT_DIR=$INST/usd"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$NO_SYSTEM_GLEW" = true ]; then
    _1="-D WITH_SYSTEM_GLEW=OFF"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
  fi

  if [ "$FFMPEG_SKIP" = false ]; then
    _1="-D WITH_CODEC_FFMPEG=ON"
    _2="-D FFMPEG_LIBRARIES='avformat;avcodec;avutil;avdevice;swscale;swresample;lzma;rt;`print_info_ffmpeglink`'"
    PRINT "  $_1"
    PRINT "  $_2"
    _buildargs="$_buildargs $_1 $_2"
    if [ -d $INST/ffmpeg ]; then
      _1="-D FFMPEG=$INST/ffmpeg"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  if [ "$XR_OPENXR_SKIP" = false ]; then
    _1="-D WITH_XR_OPENXR=ON"
    PRINT "  $_1"
    _buildargs="$_buildargs $_1"
    if [ -d $INST/xr-openxr-sdk ]; then
      _1="-D XR_OPENXR_SDK_ROOT_DIR=$INST/xr-openxr-sdk"
      PRINT "  $_1"
      _buildargs="$_buildargs $_1"
    fi
  fi

  PRINT ""
  PRINT "Or even simpler, just run (in your blender-source dir):"
  PRINT "  make -j$THREADS BUILD_CMAKE_ARGS=\"$_buildargs\""

  PRINT ""
  PRINT "Or in all your build directories:"
  PRINT "  cmake $_buildargs ."
}

# ----------------------------------------------------------------------------
# "Main"

# Detect distribution type used on this machine
if [ -f /etc/debian_version ]; then
  DISTRO="DEB"
  install_DEB
elif [ -f /etc/arch-release ]; then
  DISTRO="ARCH"
  install_ARCH
elif [ -f /etc/redhat-release -o /etc/SuSE-release ]; then
  DISTRO="RPM"
  install_RPM
else
  DISTRO="OTHER"
  install_OTHER
fi

print_info | tee $INFO_PATH/BUILD_NOTES.txt
PRINT ""
PRINT "This information has been written to $INFO_PATH/BUILD_NOTES.txt"
PRINT ""

# Switch back to user language.
LANG=LANG_BACK
export LANG

CXXFLAGS=$CXXFLAGS_BACK
export CXXFLAGS
