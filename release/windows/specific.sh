#!/bin/sh
#
# $Id$
#
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
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL/BL DUAL LICENSE BLOCK *****
#
# OS specific stuff for the package, only to be executed by ../Makefile

# Create ^M in readme.txt
awk '{printf("%s\r\n", $0);}' $DISTDIR/README > $DISTDIR/Readme.txt
rm -f $DISTDIR/README

# Create ^M in copyright.txt
awk '{printf("%s\r\n", $0);}' $DISTDIR/copyright.txt > $DISTDIR/aCopyright.txt
rm -f $DISTDIR/copyright.txt
mv -f $DISTDIR/aCopyright.txt $DISTDIR/Copyright.txt
# PS. the whole aCopyright kludge is because of windows being braindead

# Add Python DLL to package
# Stupid windows needs the . removed :
PVERS=`echo $NAN_PYTHON_VERSION | sed 's/\.//'`
cp -f $NAN_PYTHON/lib/python$PVERS.dll $DISTDIR/python$PVERS.dll
chmod +x $DISTDIR/python$PVERS.dll

# Add fmod DLL to package
# cp -f $NAN_FMOD/lib/fmod.dll $DISTDIR/fmod.dll
# chmod +x $DISTDIR/fmod.dll

# Add the Help.url to the ditribution
cp -f extra/Help.url $DISTDIR/

# make the installer package with NSIS
NSIS="$PROGRAMFILES/NSIS/makensis.exe"
if (`test -x "$NSIS"`) then
    cd installer
    TEMPFILE=00.blender_tmp.nsi
    DISTDIR=`cygpath -m $DISTDIR`
    # make a installer config for this release
    cat 00.blender.nsi | sed "s|VERSION|$VERSION|g" | sed "s|DISTDIR|$DISTDIR|g" | sed "s|SHORTVERS|$PVERS|g" > $TEMPFILE
    "$NSIS" $TEMPFILE
    rm $TEMPFILE
fi
