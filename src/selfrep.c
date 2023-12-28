/* -copyright-
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
*/
#include "selfrep.h"
#include "mygettext.h"
#include "utils.h"
#include <pthread.h>
#include <stdio.h>

void selfrep() {

#ifdef SELFREP
    unsigned char tarfile[] = {
        #include "tarfile.inc"
    };

    if (sizeof(tarfile) > 1000 && isatty(fileno(stdout))) {
        printf(_("Not sending tar file to terminal.\n"));
        printf(_("Try redirecting to a file (e.g: %s,\n"),
            "plasmasnow -selfrep > plasmasnow.tar.gz)");
        printf(_("or use a %s"), "pipe (e.g: plasmasnow -selfrep | tar zxf -).\n");
    } else {
        ssize_t rc = mywrite(fileno(stdout), tarfile, sizeof(tarfile));
        if (rc < 0) {
            fprintf(stderr, "plasmasnow: %s",
                _("Problems encountered during production of the tar ball.\n"));
        }
    }

#else
    // Since the -selfrep flag is not recognized in this case,
    // the following is somewhat superfluous:
    printf(_("Self replication is not compiled in.\n"));
#endif
}
