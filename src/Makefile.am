# -copyright-
#-# 
#-# plasmasnow: Let it snow on your desktop
#-# Copyright (C) 1984,1988,1990,1993-1995,2000-2001 Rick Jansen
#-# 	      2019,2020,2021,2022,2023 Willem Vermin
#-#          2024 Mark Capella
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

# /usr/local
gamesdir = $(exec_prefix)/games
games_PROGRAMS = plasmasnow

# Qt Dev package
QT_CDEFINES  = -DQT_NO_DEBUG -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB
QT_CFLAGS    = -pipe -D_REENTRANT -fPIC $(QT_CDEFINES)
QT_CINCLUDES = -I/usr/include/x86_64-linux-gnu/qt5 \
	-I/usr/include/x86_64-linux-gnu/qt5/QtCore \
	-I/usr/include/x86_64-linux-gnu/qt5/QtGui \
	-I/usr/include/x86_64-linux-gnu/qt5/QtWidgets
QT_LIBS      = /usr/lib/x86_64-linux-gnu/libQt5Core.so \
	/usr/lib/x86_64-linux-gnu/libQt5Gui.so \
	/usr/lib/x86_64-linux-gnu/libQt5Widgets.so

plasmasnow_CPPFLAGS = $(QT_CFLAGS) $(QT_CINCLUDES) $(GTK_CFLAGS) \
	$(X11_CFLAGS) $(GSL_CFLAGS) -DLOCALEDIR=\"$(LOCALEDIR)\" \
	-DLANGUAGES='"$(LANGUAGES)"'
plasmasnow_LDADD = libxdo.a $(GTK_LIBS) $(QT_LIBS) $(X11_LIBS) \
	-lXfixes $(GSL_LIBS) $(LIBINTL) 
libxdo_a_CPPFLAGS = $(X11_CFLAGS)

if USE_NLS
   LANGUAGES != for l in `sed 's/\#.*//' \
		$(top_srcdir)/po/LINGUAS`; do echo $$l; done
   plasmasnow_CPPFLAGS += -DENABLE_NLS=1
else
   LANGUAGES = 
endif

if MAKESELFREP
plasmasnow_CPPFLAGS += -DSELFREP
endif

noinst_LIBRARIES = libxdo.a
libxdo_a_SOURCES = xdo.c xdo_search.c xdo.h xdo_util.h xdo_version.h

plasmasnow_SOURCES = \
		Application.c aurora.c birds.c blowoff.c clientwin.c \
		clocks.c colorPicker.cpp csvpos.c docs.c dsimple.c \
		fallensnow.c Flags.c hashtable.cpp ixpm.c kdtree.c \
		Lights.cpp loadmeasure.c MainWindow.c mainstub.cpp \
		meteor.c MsgBox.cpp moon.c pixmaps.c safe_malloc.c \
		Santa.c scenery.c selfrep.c snow.c spline_interpol.c \
		Stars.c StormWindow.c treesnow.c ui.glade Utils.c \
		wind.c windows.c wmctrl.c

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

install-data-local:
	cp 'Pixmaps/plasmasnow.png' /usr/share/icons/hicolor/48x48/apps/
	cp 'Pixmaps/plasmasnowbox.png' /usr/share/icons/hicolor/48x48/apps/
	cp 'Pixmaps/plasmasnowpicker.png' /usr/share/icons/hicolor/48x48/apps/

uninstall-local:
	rm -f /usr/share/icons/hicolor/48x48/apps/plasmasnow.png
	rm -f /usr/share/icons/hicolor/48x48/apps/plasmasnowbox.png
	rm -f /usr/share/icons/hicolor/48x48/apps/plasmasnowpicker.png

TESTS = test1.sh

changelog.inc: $(top_srcdir)/ChangeLog $(TOCC)
	$(TOCC) < $(top_srcdir)/ChangeLog > $@

plasmasnow-docs.$(OBJEXT): changelog.inc

plasmasnow-selfrep.$(OBJEXT): $(tarfile_inc)

# usr/local/share
appicondir=$(datadir)/pixmaps
appicon_DATA=Pixmaps/plasmasnow.png \
	Pixmaps/plasmasnowpicker.png

man6_MANS = plasmasnow.6
plasmasnow.6: plasmasnow
	./plasmasnow -manpage > $@

CLEANFILES = plasmasnow.6 ui_xml.h snow_includes.h \
	plasmasnow_out_2 plasmasnow_out_3 \
	changelog.inc tarfile.inc toascii toascii.c
