# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
# $Id$
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2002 by Wouter van Heyst
# All rights reserved.
#
# The Original Code is: revision 1.1
#
# Contributor(s): Hans Lambermont, GSR
#
# ***** END GPL LICENSE BLOCK *****
#
# Toplevel Makefile for blender. Bounces make to subdirectories.
# Available targets: 'all' 'debug' 'release'

# If the user wants to override some of the build
# vars they can put it in the file user-def.mk which
# will get included if it exists (please do not commit
# user-def.mk to the revision control server).
sinclude user-def.mk

# To build without openAL, set it as an environment variable,
# or put it uncommented in user-def.mk:
# export NAN_NO_OPENAL=true

export NANBLENDERHOME=$(shell pwd)
MAKEFLAGS=-I$(NANBLENDERHOME)/source --no-print-directory

SOURCEDIR = 
ifeq ($(FREE_WINDOWS),true)
    DIRS ?= dlltool extern intern source po
else
    DIRS ?= extern intern source po
endif

include source/nan_subdirs.mk

.PHONY: release
release:
	@echo "====> $(MAKE) $@ in $(SOURCEDIR)/$@" ;\
	    $(MAKE) -C $@ $@ || exit 1;


