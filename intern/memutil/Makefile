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
# Contributor(s): Hans Lambermont
#
# ***** END GPL/BL DUAL LICENSE BLOCK *****
# memutil main makefile.
#

include nan_definitions.mk

LIBNAME = memutil
SOURCEDIR = intern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
DIRS = intern
#not yet TESTDIRS = test
 
include nan_subdirs.mk

install: all debug
	@[ -d $(NAN_MEMUTIL) ] || mkdir $(NAN_MEMUTIL)
	@[ -d $(NAN_MEMUTIL)/include ] || mkdir $(NAN_MEMUTIL)/include
	@[ -d $(NAN_MEMUTIL)/lib ] || mkdir $(NAN_MEMUTIL)/lib
	@[ -d $(NAN_MEMUTIL)/lib/debug ] || mkdir $(NAN_MEMUTIL)/lib/debug
	@../tools/cpifdiff.sh $(DIR)/libmemutil.a $(NAN_MEMUTIL)/lib/
	@../tools/cpifdiff.sh $(DIR)/debug/libmemutil.a $(NAN_MEMUTIL)/lib/debug
	@../tools/cpifdiff.sh *.h $(NAN_MEMUTIL)/include/

