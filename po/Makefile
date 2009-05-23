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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA	 02111-1307, USA.
#
# The Original Code is Copyright (C) 2002 by Stichting Blender Foundation,
# Amsterdam, the Netherlands.
# All rights reserved.
#
# The Original Code is: revision 1.3
#
# Contributor(s): Wouter van Heyst, GSR
#
# ***** END GPL LICENSE BLOCK *****
#
# po Makefile for blender. Compiles the translations in the place
# where release can pick them up.

PO_FILES = $(wildcard *.po)

LINGUAS = $(basename $(PO_FILES))

SOURCEDIR = blender/po

include nan_definitions.mk

ifeq ($(OS), darwin)
  DIR = $(OCGDIR)/bin/blender.app/Contents/Resources/locale/
else
  DIR = $(OCGDIR)/bin/.blender/locale/
endif

LINGUAS_DEST= $(foreach LINGUA, $(LINGUAS),$(DIR)$(LINGUA)/LC_MESSAGES/blender.mo)

$(DIR)%/LC_MESSAGES/blender.mo: %.po
	mkdir -p $(@D)
	msgfmt -o $@ $<
	@cmp $@ $(NANBLENDERHOME)/bin/.blender/locale/$(basename $<)/LC_MESSAGES/blender.mo \
          || ( echo Mismatch between generated and commited $(basename $<).mo catalog && \
               rm -f $@ && false )

all debug:: $(LINGUAS_DEST)
#	Just trigger the deps

clean::
	rm -rf $(DIR)
