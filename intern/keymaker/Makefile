#
# $Id$
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
# blender keyreader-library makefile
#

LIBNAME = blenkey
SOURCEDIR = intern/$(LIBNAME)
DIR = $(OCGDIR)/$(SOURCEDIR)
CSRCS = mt19937int.c key.c

ALLTARGETS = $(OBJS)

include nan_compile.mk

CFLAGS += $(LEVEL_2_C_WARNINGS)

CPPFLAGS += -I$(NAN_OPENSSL)/include -Ipython/

include nan_link.mk

# OBJS is for the library, set by nan_compile.mk
LOADER_OBJS = $(DIR)/$(DEBUG_DIR)keyloader.o

ifneq ($(OS),windows)
    LIBS = $(NAN_OPENSSL)/lib/libcrypto.a
else
    LIBS = $(NAN_OPENSSL)/lib/libeay32.lib
    LIBS += advapi32.lib gdi32.lib
endif

all debug:: link

link: $(DIR)/$(DEBUG_DIR)keyloader

strip:
    ifneq ($(OS),windows)
	strip keyloader
	@ls -la keyloader
    else
	@ls -la keyloader.exe
    endif

install: all debug
	@[ -d $(LCGDIR)/$(LIBNAME) ] || mkdir $(LCGDIR)/$(LIBNAME)
	@[ -d $(LCGDIR)/$(LIBNAME)/include ] || mkdir $(LCGDIR)/$(LIBNAME)/include
	@[ -d $(LCGDIR)/$(LIBNAME)/lib ] || mkdir $(LCGDIR)/$(LIBNAME)/lib
	@../tools/cpifdiff.sh blenkey.h $(LCGDIR)/$(LIBNAME)/include/
	@../tools/cpifdiff.sh $(LIB_a) $(LCGDIR)/$(LIBNAME)/lib/
ifeq ($(OS),darwin)
	ranlib $(LCGDIR)/$(LIBNAME)/lib/lib$(LIBNAME).a
endif

$(DIR)/$(DEBUG_DIR)keyloader: $(LOADER_OBJS) $(LIB_a)
	$(CC) $(LDFLAGS) -o $@ $(LOADER_OBJS) $(LIB_a) $(LIBS)
	
clean::
	$(RM) $(DIR)/key* $(DIR)/debug/key*
	$(RM) $(DIR)/nan* $(DIR)/debug/nan*

tags:
	etags *.c *.h
