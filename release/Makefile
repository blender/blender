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

include nan_definitions.mk

export VERSION := $(shell cat VERSION)

BLENDNAME=blender-$(VERSION)-$(CONFIG_GUESS)$(TYPE)
export DISTDIR=$(NAN_OBJDIR)/$(BLENDNAME)

release: all

all:
    ifeq ($(OS),beos)
	@$(MAKE) pkg TYPE="" TAR="zip -ry9" EXT1=".zip" NOPLUGINS="true"
    endif
    ifeq ($(OS),freebsd)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
    endif
    ifeq ($(OS),irix)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
    endif
    ifeq ($(OS),linux)
      ifeq ($(CPU),alpha)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
      endif
      ifeq ($(CPU),i386)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
	@$(MAKE) pkg TYPE="-static" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
      endif
      ifeq ($(CPU),powerpc)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
	@$(MAKE) pkg TYPE="-static" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
      endif
    endif
    ifeq ($(OS),openbsd)
	@$(MAKE) pkg TYPE="-static" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
    endif
    ifeq ($(OS),solaris)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT1=".tar" \
	    COMPRESS="compress" EXT2=".Z"
    endif
    ifeq ($(OS),windows)
	@$(MAKE) pkg TYPE="" TAR="zip -r9" EXT0=".exe" EXT1=".zip" \
	    NOPLUGINS="true" NOSTRIP="true"
    endif
    ifeq ($(OS),darwin)
	@$(MAKE) pkg TYPE="" TAR="tar cf" EXT0"=.app" EXT1=".tar" \
	    COMPRESS="gzip -f --best" EXT2=".gz"
    endif

# OS independent targets below:

dist: all

package: version makedirs

install: package
	@#echo "****> Install text"
	@cp text/README $(DISTDIR)
	@cp text/*.txt $(DISTDIR)
	@# possible overruling .txt text documents
	@[ ! -d $(CONFIG_GUESS)/text ] || \
	    cp -f $(CONFIG_GUESS)/text/*.txt $(DISTDIR)

# Python scripts removed for 2.14
#@echo "----> Copy python scripts"
#@cp -r python $(DISTDIR)/python
#@rm -fr $(DISTDIR)/python/CVS

	@echo "----> Copy blender$(EXT0) executable"
    ifeq ($(TYPE),-static)
	@cp $(OCGDIR)/bin/blenderstatic$(EXT0) $(DISTDIR)/blender$(EXT0)
    else
    ifeq ($(OS),darwin)
	@cp -r $(OCGDIR)/bin/blender$(EXT0) $(DISTDIR)/blender$(EXT0)
    else
	@cp $(OCGDIR)/bin/blender$(EXT0) $(DISTDIR)/blender$(EXT0)
    endif
    endif
    ifneq ($(NOPLUGINS),true)
	@echo "----> Copy and compile plugins"
	@cp -r plugins $(DISTDIR)/plugins
	@mkdir -p $(DISTDIR)/plugins/include
	@cp ../source/blender/blenpluginapi/*.h $(DISTDIR)/plugins/include/
	@chmod 755 $(DISTDIR)/plugins/bmake
	@$(MAKE) -C $(DISTDIR)/plugins all  > /dev/null || exit 1;
	@rm -fr $(DISTDIR)/plugins/CVS $(DISTDIR)/plugins/*/CVS \
             $(DISTDIR)/plugins/*/*.o
    endif
    ifneq ($(NOSTRIP),true)
	@echo "----> Strip blender executable"
      ifeq ($(OS),darwin)
	@strip $(OCGDIR)/bin/blender$(EXT0)/Contents/MacOS/blender
      else
	@strip $(DISTDIR)/blender$(EXT0)
      endif
    endif
	@[ ! -x $(CONFIG_GUESS)/specific.sh ] || (\
	    echo "**--> Execute specific.sh in $(CONFIG_GUESS)/" && \
	    cd $(CONFIG_GUESS) && ./specific.sh )

pkg: install
	@echo "----> Create distribution file $(BLENDNAME)$(EXT1)"
	@cd $(NAN_OBJDIR) && \
	rm -f  $(VERSION)/$(BLENDNAME)$(EXT1)* && \
	$(TAR) $(VERSION)/$(BLENDNAME)$(EXT1) $(BLENDNAME)
    ifdef COMPRESS
	@echo "----> Compressing distribution to $(BLENDNAME)$(EXT1)$(EXT2)"
	@$(COMPRESS) $(NAN_OBJDIR)/$(VERSION)/$(BLENDNAME)$(EXT1)
    endif
	@#echo "****> Clean up temporary distribution directory"
	@rm -fr $(DISTDIR)
	@echo "****> $(NAN_OBJDIR)/$(VERSION)/$(BLENDNAME)$(EXT1)$(EXT2) is ready"

version: FORCE
	@echo "*---> Create $(BLENDNAME) package"

makedirs: FORCE
	@#echo "****> Create package directory $(VERSION) if necessary"
	@[ -d $(NAN_OBJDIR)/$(VERSION) ] || mkdir $(NAN_OBJDIR)/$(VERSION)
	@#echo "****> Prepare temporary distribution directory"
	@rm -fr $(DISTDIR)
	@mkdir $(DISTDIR)

FORCE:
