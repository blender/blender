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
# Contributor(s): Hans Lambermont
#
# ***** END GPL LICENSE BLOCK *****
# opennl main makefile.
#

include nan_definitions.mk

LIBNAME = opennl
LIBNAME_SLU = superlu
SOURCEDIR = intern/$(LIBNAME)
SOURCEDIR_SLU = intern/$(LIBNAME_SLU)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIR_SLU = $(OCGDIR)/$(SOURCEDIR_SLU)
DIRS = intern superlu

include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_OPENNL) ] || mkdir $(NAN_OPENNL)
	@[ -d $(NAN_OPENNL)/include ] || mkdir $(NAN_OPENNL)/include
	@[ -d $(NAN_OPENNL)/lib ] || mkdir $(NAN_OPENNL)/lib
	@[ -d $(NAN_OPENNL)/lib/debug ] || mkdir $(NAN_OPENNL)/lib/debug
	@../tools/cpifdiff.sh $(DIR)/libopennl.a $(NAN_OPENNL)/lib/
	@../tools/cpifdiff.sh $(DIR)/debug/libopennl.a $(NAN_OPENNL)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_OPENNL)/lib/libopennl.a
	ranlib $(NAN_OPENNL)/lib/debug/libopennl.a
endif
	@../tools/cpifdiff.sh extern/*.h $(NAN_OPENNL)/include/
	@[ -d $(NAN_SUPERLU) ] || mkdir $(NAN_SUPERLU)
	@[ -d $(NAN_SUPERLU)/lib ] || mkdir $(NAN_SUPERLU)/lib
	@[ -d $(NAN_SUPERLU)/lib/debug ] || mkdir $(NAN_SUPERLU)/lib/debug
	@../tools/cpifdiff.sh $(DIR_SLU)/libsuperlu.a $(NAN_SUPERLU)/lib/
	@../tools/cpifdiff.sh $(DIR_SLU)/debug/libsuperlu.a $(NAN_SUPERLU)/lib/debug/
ifeq ($(OS),darwin)
	ranlib $(NAN_SUPERLU)/lib/libsuperlu.a
	ranlib $(NAN_SUPERLU)/lib/debug/libsuperlu.a
endif

