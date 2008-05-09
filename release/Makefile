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
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****

include nan_definitions.mk

export VERSION := $(shell ./getversion.py)

BLENDNAME=blender-$(VERSION)-$(CONFIG_GUESS)-py$(NAN_PYTHON_VERSION)$(TYPE)
export DISTDIR=$(NAN_OBJDIR)/$(BLENDNAME)
export CONFDIR=$(DISTDIR)/.blender

ifeq ($(OS),beos)
   TAR="zip"
   TARFLAGS="-ry9"
   EXT0=""
   EXT1=".zip"
   COMPRESS=""
   EXT2=""
   NOPLUGINS?=true
endif

ifeq ($(OS),$(findstring $(OS), "freebsd irix linux openbsd solaris"))
   TAR="tar"
   TARFLAGS="cf"
   EXT0=""
   EXT1=".tar"
   COMPRESS="bzip2"
   COMPRESSFLAGS="-f"
   EXT2=".bz2"
   ifeq ($(OS), solaris)
     ifeq ($(CPU), i386)
       NOPLUGINS?=true
     endif
   endif
endif

ifeq ($(OS),windows)
   TAR="zip"
   TARFLAGS="-r9"
   EXT0=".exe"
   EXT1=".zip"
   NOPLUGINS?=true
   NOSTRIP?=true
endif

ifeq ($(OS),darwin)
   TAR="tar"
   TARFLAGS="cf"
   EXT0=".app"
   EXT1=".tar"
   COMPRESS="bzip2"
   COMPRESSFLAGS="-f"
   EXT2=".bz2"
endif

release: all

all:
	@$(MAKE) pkg TYPE=""
ifeq ($(WITH_BF_STATICOPENGL), true)
	@$(MAKE) pkg TYPE="-static" 
endif

# OS independent targets below:

dist: all

package: version makedirs

install: package
	@#echo "****> Install text"
	@cp text/blender.html $(DISTDIR)
	@cp text/*.txt $(DISTDIR)
	@cp text/*.pdf $(DISTDIR)
    ifeq ($(FREEDESKTOP), true)
	@#echo "****> Install freedesktop icons"
	@mkdir $(DISTDIR)/icons
	@mkdir $(DISTDIR)/icons/16x16
	@cp freedesktop/icons/16x16/blender.png $(DISTDIR)/icons/16x16
	@mkdir $(DISTDIR)/icons/22x22
	@cp freedesktop/icons/22x22/blender.png $(DISTDIR)/icons/22x22
	@mkdir $(DISTDIR)/icons/32x32
	@cp freedesktop/icons/32x32/blender.png $(DISTDIR)/icons/32x32
	@mkdir $(DISTDIR)/icons/scalable
	@cp freedesktop/icons/scalable/blender.svg $(DISTDIR)/icons/scalable
    endif
	@echo "----> Make Config dir .blender"
	@mkdir -p $(CONFDIR)
	@# possible overruling .txt text documents
	@[ ! -d $(CONFIG_GUESS)/text ] || \
	    cp -f $(CONFIG_GUESS)/text/*.txt $(DISTDIR)
#on OS X the contents of the .blender dir is already inside the bundle
    ifneq ($(OS), darwin)
	@[ ! -d $(OCGDIR)/bin/.blender ] || \
		cp -r $(OCGDIR)/bin/.blender $(DISTDIR)
	@cp $(NANBLENDERHOME)/bin/.blender/.Blanguages $(CONFDIR)
	@cp $(NANBLENDERHOME)/bin/.blender/.bfont.ttf $(CONFDIR)
    endif
	@echo "----> Copy blender$(EXT0) executable"
    ifeq ($(TYPE),-static)
	@cp $(OCGDIR)/bin/blenderstatic$(EXT0) $(DISTDIR)/blender$(EXT0)
    else
    ifeq ($(OS),darwin)
	@cp -r $(OCGDIR)/bin/blender$(EXT0) $(DISTDIR)/Blender$(EXT0)
    else
	@cp $(OCGDIR)/bin/blender$(EXT0) $(DISTDIR)/blender$(EXT0)
    endif
	@if [ -f $(OCGDIR)/bin/blenderplayer$(EXTO) ]; then \
		cp $(OCGDIR)/bin/blenderplayer$(EXTO) \
			$(DISTDIR)/blenderplayer$(EXTO) ; \
	fi
    endif

ifneq ($(NOPLUGINS),true)
	@echo "----> Copy and compile plugins"
	@cp -r plugins $(DISTDIR)/plugins
	@mkdir -p $(DISTDIR)/plugins/include
	@cp ../source/blender/blenpluginapi/*.h $(DISTDIR)/plugins/include/
	@chmod 755 $(DISTDIR)/plugins/bmake
	@$(MAKE) -C $(DISTDIR)/plugins all  > /dev/null || exit 1;
	@rm -f $(DISTDIR)/plugins/*/*.o

#on OS X the plugins move to the installation directory
    ifneq ($(OS),darwin)
	@mkdir -p $(CONFDIR)/plugins/sequence
	@mkdir -p $(CONFDIR)/plugins/texture
	@mv $(DISTDIR)/plugins/sequence/*.so $(CONFDIR)/plugins/sequence
	@mv $(DISTDIR)/plugins/texture/*.so $(CONFDIR)/plugins/texture
    endif
endif

	@echo "----> Copy python infrastructure"
	@[ ! -d scripts ] || cp -r scripts $(CONFDIR)/scripts

    ifeq ($(OS),darwin)
	@echo "----> Move .blender to .app/Contents/MacOS/"
	@rm -fr $(DISTDIR)/blender$(EXT0)/Contents/MacOS/.blender
	@mv $(DISTDIR)/.blender $(DISTDIR)/blender$(EXT0)/Contents/MacOS/
    endif

    ifneq ($(NOSTRIP),true)
	@echo "----> Strip blender executable"
      ifeq ($(OS),darwin)
	@strip -x $(DISTDIR)/blender$(EXT0)/Contents/MacOS/blender
      else
	@strip -x $(DISTDIR)/blender$(EXT0)
	@if [ -f $(DISTDIR)/blenderplayer$(EXTO) ]; then \
           strip -x $(DISTDIR)/blender$(EXT0) ; \
	fi
      endif
    endif
	@[ ! -x $(CONFIG_GUESS)/specific.sh ] || (\
	    echo "**--> Execute specific.sh in $(CONFIG_GUESS)/" && \
	    cd $(CONFIG_GUESS) && ./specific.sh )
	@echo "----> Cleaning .svn metadata directories"
	@find $(DISTDIR) -type d -name ".svn" | xargs rm -fr

pkg: install
	@echo "----> Create distribution file $(BLENDNAME)$(EXT1)"
	@#enable the next sleep if you get 'tar file changed while reading'
	@#sleep 10
	rm -f  $(NAN_OBJDIR)/$(VERSION)/$(BLENDNAME)$(EXT1)*
	@cd $(NAN_OBJDIR) && $(TAR) $(TARFLAGS) $(VERSION)/$(BLENDNAME)$(EXT1) $(BLENDNAME)
    ifdef COMPRESS
	@echo "----> Compressing distribution to $(BLENDNAME)$(EXT1)$(EXT2)"
	@$(COMPRESS) $(COMPRESSFLAGS) $(NAN_OBJDIR)/$(VERSION)/$(BLENDNAME)$(EXT1)
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
