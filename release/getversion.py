#!/usr/bin/env python
# -*- mode: python; tab-width: 4; indent-tabs-mode: t; -*-
# vim: tabstop=4
# $Id#
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
# The Original Code is Copyright (C) 2008 by the Blender Foundation
# All rights reserved.
#
# The Original Code is: see repository.
#
# Contributor(s): see repository.

# <pep8-80 compliant>

import sys
import os
import re

nanblenderhome = os.getenv("NANBLENDERHOME")

if nanblenderhome == None:
    nanblenderhome = os.path.dirname(os.path.abspath(sys.argv[0])) + "/.."

config = nanblenderhome + "/source/blender/blenkernel/BKE_blender.h"

infile = open(config)

major = None
minor = None

for line in infile.readlines():
    m = re.search("#define BLENDER_VERSION\s+(\d+)", line)
    if m:
        major = m.group(1)
    m = re.search("#define BLENDER_SUBVERSION\s+(\d+)", line)
    if m:
        minor = m.group(1)
    if minor and major:
        major = float(major) / 100.0
        break

infile.close()

# Major was changed to float, but minor is still a string
if minor and major:
    if minor == "0":
        print "%.2f" % major
    else:
        print "%.2f.%s" % (major, minor)
else:
    print "unknownversion"
