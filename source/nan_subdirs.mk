# -*- mode: gnumakefile; tab-width: 8; indent-tabs-mode: t; -*-
# vim: tabstop=8
#
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
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): GSR
#
# ***** END GPL LICENSE BLOCK *****
#
# Bounce make to subdirectories.
# Set DIRS, SOURCEDIR. Optionally also reacts on DIR, TESTDIRS.
#

default: all

# do not add install here. install target can only be used in intern/
# top level Makefiles
all debug clean::
ifdef quicky
	@for i in $(quicky); do \
	   echo "====> $(MAKE) $@ in $$i";\
	   $(MAKE) -C $$i $@ quicky= || exit 1;\
	done
	$(MAKE) -C source link || exit 1
	@echo "${quicky}"
else
    ifdef DIR
	@# Make sure object toplevels are there
	@[ -d $(NAN_OBJDIR) ] || mkdir -p $(NAN_OBJDIR)
	@[ -d $(LCGDIR) ] || mkdir -p $(LCGDIR)
	@[ -d $(OCGDIR) ] || mkdir -p $(OCGDIR)
	@[ -d $(OCGDIR)/intern ] || mkdir -p $(OCGDIR)/intern
	@[ -d $(OCGDIR)/extern ] || mkdir -p $(OCGDIR)/extern
	@# Create object directory
	@[ -d $(DIR) ] || mkdir -p $(DIR)
    endif
    ifdef SOURCEDIR
	@for i in $(DIRS); do \
	    echo "====> $(MAKE) $@ in $(SOURCEDIR)/$$i" ;\
	    $(MAKE) -C $$i $@ || exit 1; \
	done
    else
	@for i in $(DIRS); do \
	    echo "====> $(MAKE) $@ in $$i" ;\
	    $(MAKE) -C $$i $@ || exit 1; \
	done
    endif
endif

test::
    ifdef TESTDIRS
	@for i in $(TESTDIRS); do \
	    echo "====> $(MAKE) $@ in $(SOURCEDIR)/$$i" ;\
	    $(MAKE) -C $$i $@ || exit 1; \
	done
    endif

