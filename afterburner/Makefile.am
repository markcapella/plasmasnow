# -copyright-
#-# 
#-# plasmasnow: Let it snow on your desktop
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

# To enable the selfrep option:
# after everything has been made, we replace the dummy tarfile with the
# tarfile obtained with 'make dist'
# Then, we run a 'make' from the top directory to recompile selfrep.c and
# relink app. When running this make, we must do nothing here.

tarfile = $(top_builddir)/$(PACKAGE)-$(VERSION).tar.gz

if MAKESELFREP
makeselfrep = 1
else
makeselfrep =
endif

if USE_NLS
use_nls = 1
endif

# it would be nice to skip all-local when running make check

all-local: 
	if test -z "$(AFTERBURNER)" ; then                          \
		if test "$(makeselfrep)" ; then                         \
			echo "Creating tar file $(tarfile) ..." ;           \
			$(MAKE) -C "$(top_builddir)" AFTERBURNER=1 dist ;   \
			echo "Remaking app, using real tar file ..." ;    \
			ls -l "$(tarfile)" ;                                \
			$(MAKE) -C "$(top_builddir)" AFTERBURNER=1 ;        \
		fi ;                                                    \
		if test "$(use_nls)" ; then                             \
			echo "Creating gmo files ... $(top_builddir)/po" ;  \
			$(MAKE) -C "$(top_builddir)/po" update-gmo ;        \
		fi ;                                                    \
	fi

install:
	@echo "Nothing to install here"

CLEANFILES = $(tarfile)

