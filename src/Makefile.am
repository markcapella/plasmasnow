# -copyright-
#-# 
#-# plasmasnow: let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
#-# 
#-# This program is free software: you can redistribute it and/or modify
#-# it under the terms of the GNU General Public License as published by
#-# the Free Software Foundation, either version 3 of the License, or
#-# (at your option) any later version.
#-# 
#-# This program is distributed in the hope that it will be useful,
#-# but WITHOUT ANY WARRANTY; without even the implied warranty of
#-# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#-# GNU General Public License for more details.
#-# 
#-# You should have received a copy of the GNU General Public License
#-# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#-# 
SUBDIRS = Pixmaps

TOCC     = $(top_srcdir)/src/tocc.sh
TOASCII  = $(top_srcdir)/src/toascii.sh

gamesdir = $(exec_prefix)/games
games_PROGRAMS = plasmasnow

# Qt Dev package
QT_CDEFINES  = -DQT_NO_DEBUG -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB
QT_CFLAGS    = -pipe -O2 -Wall -Wextra -D_REENTRANT -fPIC $(QT_CDEFINES)
QT_CINCLUDES = -I/usr/include/x86_64-linux-gnu/qt5 -I/usr/include/x86_64-linux-gnu/qt5/QtCore \
	-I/usr/include/x86_64-linux-gnu/qt5/QtGui -I/usr/include/x86_64-linux-gnu/qt5/QtWidgets
QT_LIBS      = /usr/lib/x86_64-linux-gnu/libQt5Core.so /usr/lib/x86_64-linux-gnu/libQt5Gui.so \
	/usr/lib/x86_64-linux-gnu/libQt5Widgets.so

plasmasnow_CPPFLAGS    = $(QT_CFLAGS) $(QT_CINCLUDES) $(GTK_CFLAGS) $(X11_CFLAGS) $(GSL_CFLAGS) -DLOCALEDIR=\"$(LOCALEDIR)\" \
		    -DLANGUAGES='"$(LANGUAGES)"'
plasmasnow_LDADD       = libxdo.a $(GTK_LIBS) $(QT_LIBS) $(X11_LIBS) $(GSL_LIBS) $(LIBINTL)
libxdo_a_CPPFLAGS = $(X11_CFLAGS)

if USE_NLS
   LANGUAGES != for l in `sed 's/\#.*//' $(top_srcdir)/po/LINGUAS`; do echo $$l; done
   plasmasnow_CPPFLAGS += -DENABLE_NLS=1
else
   LANGUAGES = 
endif

if MAKESELFREP
plasmasnow_CPPFLAGS += -DSELFREP
endif

noinst_LIBRARIES = libxdo.a
libxdo_a_SOURCES = xdo.c xdo_search.c xdo.h xdo_util.h xdo_version.h

plasmasnow_SOURCES = clocks.c ixpm.c main.c fallensnow.c wmctrl.c docs.c \
		clocks.h ixpm.h docs.h fallensnow.h pixmaps.h \
		pixmaps.c version.h wmctrl.h plasmasnow.h windows.c windows.h\
		flags.c flags.h csvpos.c csvpos.h \
		dsimple.c dsimple.h clientwin.c clientwin.h \
		test1.sh \
		doit.h ui.c ui.h ui.glade \
		debug.h \
		mainstub.cpp mainstub.h \
		birds.c kdtree.c birds.h kdtree.h birdglobals.h doitb.h \
		hashtable.cpp hashtable.h \
		Santa.c Santa.h utils.c utils.h \
		scenery.c scenery.h snow.c snow.h \
		colorPicker.cpp \
		meteor.c meteor.h wind.c wind.h \
		stars.c stars.h blowoff.c blowoff.h \
		treesnow.c treesnow.h loadmeasure.c loadmeasure.h \
		snow_includes.h vroot.h \
		moon.c moon.h buttons.h undefall.inc \
		transwindow.c transwindow.h \
		selfrep.c selfrep.h \
		aurora.c aurora.h \
		spline_interpol.c spline_interpol.h \
		safe_malloc.c safe_malloc.h mygettext.h gettext.h 

nodist_plasmasnow_SOURCES = ui_xml.h snow_includes.h

if MAKESELFREP
tarfile_inc = tarfile.inc
else
tarfile_inc =
endif

BUILT_SOURCES = ui_xml.h snow_includes.h $(tarfile_inc)

tarfile = $(top_builddir)/$(PACKAGE)-$(VERSION).tar.gz

$(tarfile):
	echo "Dummy tarfile." > $@

if MAKESELFREP
tarfile.inc: $(tarfile) $(TOASCII)
	@echo "Converting tarfile:"
	ls -l $(tarfile)
	export CC="$(CC)"; export CFLAGS="$(CFLAGS)"; $(TOASCII) < $(tarfile) > $@ 
else
	tarfile.inc: 
	echo "No selfrep compiled in" | $(TOASCII) > $@
endif


EXTRA_DIST = gen_snow_includes.sh gen_ui_xml.sh tocc.sh toascii.sh xdo-copyright 

ui_xml.h: ui.glade $(top_srcdir)/src/gen_ui_xml.sh
	@echo ">>> ui.glade Creating $@ from $<"
	$(top_srcdir)/src/gen_ui_xml.sh $(top_srcdir)

snow_includes.h: Pixmaps
	@echo ">>> Pixmaps Creating $@"
	$(top_srcdir)/src/gen_snow_includes.sh $(top_srcdir)


TESTS = test1.sh

changelog.inc: $(top_srcdir)/ChangeLog $(TOCC)
	$(TOCC) < $(top_srcdir)/ChangeLog > $@

plasmasnow-docs.$(OBJEXT): changelog.inc

plasmasnow-selfrep.$(OBJEXT): $(tarfile_inc)

appicondir=$(datadir)/pixmaps
appicon_DATA=Pixmaps/plasmasnow.svg Pixmaps/plasmapicker.png

man6_MANS = plasmasnow.6
plasmasnow.6: plasmasnow
	./plasmasnow -manpage > $@

CLEANFILES = plasmasnow.6 ui_xml.h snow_includes.h plasmasnow_out_2 plasmasnow_out_3 \
	     changelog.inc tarfile.inc toascii toascii.c

