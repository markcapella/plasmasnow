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
#include "Flags.h"
#include "birds.h"
#include "docs.h"
#include "doit.h"
#include "safe_malloc.h"
#include "selfrep.h"
#include "Utils.h"
#include "windows.h"
#include "plasmasnow.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

FLAGS Flags;
FLAGS OldFlags;
FLAGS DefaultFlags;
FLAGS VintageFlags;

static void ReadFlags(void);
static void SetDefaultFlags(void);
static void findflag(FILE *f, const char *x, char **value);

static long int S2Int(char *s) // string to integer
{
    return strtol(s, NULL, 0);
}
static long int S2PosInt(char *s) // string to positive integer
{
    int x = S2Int(s);
    if (x < 0) {
        return 0;
    }
    return x;
}

static char *FlagsFile = NULL;
static int FlagsFileAvailable = 1;

void SetDefaultFlags() {
#define DOIT_I(x, d, v) Flags.x = DefaultFlags.x;
#define DOIT_S(x, d, v)                                                        \
    free(Flags.x);                                                             \
    Flags.x = strdup(DefaultFlags.x);
#define DOIT_L(x, d, v) DOIT_I(x, d, v)
    DOITALL;
#include "undefall.inc"
}

// return value:
// -1: error found
// 0: all is well
// 1: did request, program can stop.
#define checkax                                                                \
    {                                                                          \
        if (ax >= argc - 1) {                                                  \
            fprintf(stderr, "** missing parameter for '%s', exiting.\n",       \
                argv[ax]);                                                     \
            return -1;                                                         \
        }                                                                      \
    }

void InitFlags() {
    // to make sure that strings in Flags are malloc'd
#define DOIT_I(x, d, v)                                                        \
    Flags.x = 0;                                                               \
    DefaultFlags.x = d;                                                        \
    VintageFlags.x = v;
#define DOIT_L DOIT_I
#define DOIT_S(x, d, v)                                                        \
    Flags.x = strdup("");                                                      \
    DefaultFlags.x = strdup(d);                                                \
    VintageFlags.x = strdup(v);
    DOITALL;
#include "undefall.inc"
}

#define handlestring(x)                                                        \
    checkax;                                                                   \
    free(Flags.x);                                                             \
    Flags.x = strdup(argv[++ax])

// argument is positive long int, set Flags.y to argument
#define handle_ia(x, y)                                                        \
    else if (!strcmp(arg, #x)) do {                                            \
        checkax;                                                               \
        Flags.y = S2PosInt(argv[++ax]);                                        \
    }                                                                          \
    while (0)

// argument is long int, set Flags.y to argument
#define handle_im(x, y)                                                        \
    else if (!strcmp(arg, #x)) do {                                            \
        checkax;                                                               \
        Flags.y = S2Int(argv[++ax]);                                           \
    }                                                                          \
    while (0)

// argument is char*, set Flags.y to argument
#define handle_is(x, y)                                                        \
    else if (!strcmp(arg, #x)) do {                                            \
        handlestring(y);                                                       \
    }                                                                          \
    while (0)

// set Flags.y to z
#define handle_iv(x, y, z)                                                     \
    else if (!strcmp(arg, #x)) do {                                            \
        Flags.y = z;                                                           \
    }                                                                          \
    while (0)

int HandleFlags(int argc, char *argv[]) {
    SetDefaultFlags();

    char *arg;
    int ax;

    for (int pass = 1; pass <= 2; pass++) {
        if (pass == 2) {
            if (Flags.Defaults || Flags.NoConfig) {
                break;
            }
            ReadFlags();
        }
        for (ax = 1; ax < argc; ax++) {
            arg = argv[ax];
            if (!strcmp(arg, "-bg")) {
                Flags.BlackBackground = 0;
            }

            //  ------------------- handled in main, so not needed here
            //  --------------------
            if (!strcmp(arg, "-h") || !strcmp(arg, "-help")) {
                docs_usage(0);
                return 1;
            } else if (!strcmp(arg, "-H") || !strcmp(arg, "-manpage")) {
                docs_usage(1);
                return 1;
            } else if (!strcmp(arg, "-v") || !strcmp(arg, "-version")) {
                logAppVersion();
                return 1;
            } else if (!strcmp(arg, "-changelog")) {
                displayPlasmaSnowDocumentation();
                return 1;
            }
#ifdef SELFREP
            else if (!strcmp(arg, "-selfrep")) {
                selfrep();
                return 1;
            }
#endif
            //  ------------------- end of handled in main --------------------
            else if (strcmp(arg, "-nokeepsnow") == 0) {
                Flags.NoKeepSnow = 1;
                Flags.NoKeepSnowOnWindows = 1;
                Flags.NoKeepSnowOnBottom = 1;
                Flags.NoKeepSnowOnTrees = 1;

            } else if (strcmp(arg, "-keepsnow") == 0) {
                Flags.NoKeepSnow = 0;
                Flags.NoKeepSnowOnWindows = 0;
                Flags.NoKeepSnowOnBottom = 0;
                Flags.NoKeepSnowOnTrees = 0;

            } else if (strcmp(arg, "-vintage") == 0) {
#define DOIT_I(x, d, v) Flags.x = VintageFlags.x;
#define DOIT_L DOIT_I
#define DOIT_S(x, d, v)                                                        \
    free(Flags.x);                                                             \
    Flags.x = strdup(VintageFlags.x);

                DOITALL

#include "undefall.inc"

            } else if (strcmp(arg, "-desktop") == 0) {
                Flags.Desktop = 1;

            } else if (strcmp(arg, "-auroraleft") == 0) {
                Flags.AuroraLeft = 1;
                Flags.AuroraMiddle = 0;
                Flags.AuroraRight = 0;

            } else if (strcmp(arg, "-auroramiddle") == 0) {
                Flags.AuroraLeft = 0;
                Flags.AuroraMiddle = 1;
                Flags.AuroraRight = 0;

            } else if (strcmp(arg, "-auroraright") == 0) {
                Flags.AuroraLeft = 0;
                Flags.AuroraMiddle = 0;
                Flags.AuroraRight = 1;
            }

            handle_ia(-allworkspaces, AllWorkspaces);

            handle_ia(-aurora, Aurora);
            handle_ia(-auroraspeed, AuroraSpeed);
            handle_ia(-aurorabrightness, AuroraBrightness);
            handle_ia(-aurorawidth, AuroraWidth);
            handle_ia(-auroraheight, AuroraHeight);
            handle_ia(-aurorabase, AuroraBase);

            handle_ia(-blowofffactor, BlowOffFactor);
            handle_ia(-cpuload, CpuLoad);
            handle_ia(-doublebuffer, useDoubleBuffers);
            handle_ia(-flakecountmax, FlakeCountMax);
            handle_ia(-id, WindowId);
            handle_ia(-window - id, WindowId);
            handle_ia(--window - id, WindowId);
            handle_ia(-maxontrees, MaxOnTrees);
            handle_ia(-meteorfrequency, MeteorFrequency);
            handle_ia(-moon, Moon);
            handle_ia(-mooncolor, MoonColor);
            handle_ia(-moonspeed, MoonSpeed);
            handle_ia(-moonsize, MoonSize);
            handle_ia(-halo, Halo);
            handle_ia(-halobrightness, HaloBright);
            handle_im(-offsets, OffsetS);
            handle_im(-offsetw, OffsetW);
            handle_im(-offsetx, OffsetX);
            handle_im(-offsety, OffsetY);
            handle_ia(-santa, SantaSize);
            handle_ia(-santaspeedfactor, SantaSpeedFactor);
            handle_ia(-santascale, SantaScale);
            handle_ia(-scale, Scale);
            handle_ia(-snowflakes, SnowFlakesFactor);
            handle_ia(-snowspeedfactor, SnowSpeedFactor);
            handle_ia(-snowsize, SnowSize);
            handle_ia(-ssnowdepth, MaxScrSnowDepth);
            handle_ia(-stars, NStars);
            handle_ia(-stopafter, StopAfter);
            handle_ia(-theme, mAppTheme);
            handle_ia(-treefill, TreeFill);
            handle_ia(-treescale, TreeScale);
            handle_ia(-trees, DesiredNumberOfTrees);
            handle_ia(-whirlfactor, WhirlFactor);
            handle_ia(-whirltimer, WhirlTimer);
            handle_ia(-wsnowdepth, MaxWinSnowDepth);
            handle_ia(-ignoretop, IgnoreTop);
            handle_ia(-ignorebottom, IgnoreBottom);
            handle_ia(-transparency, Transparency);

            handle_im(-screen, Screen);
            handle_ia(-outline, Outline);

            handle_is(display, DisplayName);
            handle_is(sc, SnowColor);
            handle_is(sc2, SnowColor2);

            handle_is(-birdscolor, BirdsColor);
            handle_is(-tc, TreeColor);
            handle_is(-treetype, TreeType);
            handle_is(-bg, BackgroundFile);
            handle_is(-lang, Language);

            handle_iv(-defaults, Defaults, 1);
            handle_iv(-noblowsnow, BlowSnow, 0);
            handle_iv(-blowsnow, BlowSnow, 1);
            handle_iv(-noconfig, NoConfig, 1);
            handle_iv(-hidemenu, HideMenu, 1);
            handle_iv(-noisy, Noisy, 1);
            handle_iv(-nokeepsnowonscreen, NoKeepSnowOnBottom, 1);
            handle_iv(-keepsnowonscreen, NoKeepSnowOnBottom, 0);
            handle_iv(-nokeepsnowontrees, NoKeepSnowOnTrees, 1);
            handle_iv(-keepsnowontrees, NoKeepSnowOnTrees, 0);
            handle_iv(-nokeepsnowonwindows, NoKeepSnowOnWindows, 1);
            handle_iv(-keepsnowonwindows, NoKeepSnowOnWindows, 0);
            handle_iv(-nomenu, NoMenu, 1);
            handle_iv(-nometeors, NoMeteors, 1);
            handle_iv(-meteors, NoMeteors, 0);
            handle_iv(-norudolph, Rudolf, 0);
            handle_iv(-showrudolph, Rudolf, 1);
            handle_iv(-nosanta, NoSanta, 1);
            handle_iv(-root, ForceRoot, 1);
            handle_iv(--root, ForceRoot, 1);
            handle_iv(-showsanta, NoSanta, 0);
            handle_iv(-snow, NoSnowFlakes, 0);
            handle_iv(-nosnow, NoSnowFlakes, 1);
            handle_iv(-nosnowflakes, NoSnowFlakes, 1);
            handle_iv(-notrees, NoTrees, 1);
            handle_iv(-showtrees, NoTrees, 0);
            handle_iv(-nowind, NoWind, 1);
            handle_iv(-wind, NoWind, 0);
            handle_iv(-xwininfo, XWinInfoHandling, 1);
            handle_iv(-treeoverlap, Overlap, 1);
            handle_iv(-notreeoverlap, Overlap, 0);

            // birds:

            handle_ia(-anarchy, Anarchy);

            handle_ia(-birdsspeed, BirdsSpeed);
            handle_ia(-disweight, DisWeight);
            handle_ia(-focuscentre, AttrFactor);
            handle_ia(-followneighbours, FollowWeight);
            handle_ia(-followsanta, FollowSanta);
            handle_ia(-nbirds, Nbirds);
            handle_ia(-neighbours, Neighbours);
            handle_ia(-prefdistance, PrefDistance);
            handle_ia(-showbirds, ShowBirds);
            handle_ia(-showattr, ShowAttrPoint);
            handle_ia(-viewingdistance, ViewingDistance);
            handle_ia(-birdsscale, BirdsScale);
            handle_ia(-attrspace, AttrSpace);

            else {
                fprintf(stderr, "** unknown flag: '%s', exiting.\n", argv[ax]);
                fprintf(stderr, " Try: plasmasnow -h\n");
                return -1;
            }
        }
    }
    if ((Flags.SantaSize < 0) || (Flags.SantaSize > MAXSANTA)) {
        printf("** Maximum Santa is %d\n", MAXSANTA);
        return -1;
    }
    if (!strcmp(Flags.TreeType, "all")) {
        free(Flags.TreeType);
        Flags.TreeType = (char *)malloc(1 + 2 + sizeof(DefaultFlags.TreeType));
        Flags.TreeType = strdup("0,");
        strcat(Flags.TreeType, DefaultFlags.TreeType);
    }
    if (Flags.SnowSize > 40) {
        printf("snowsize brought back from %d to 40\n", Flags.SnowSize);
        Flags.SnowSize = 40;
    }
    return 0;
}
#undef checkax
#undef handlestring
#undef handle_iv
#undef handle_is
#undef handle_ia

static void makeflagsfile() {
    if (FlagsFile != NULL || FlagsFileAvailable == 0) {
        return;
    }
    char *h = getenv("HOME");
    if (h == NULL) {
        FlagsFileAvailable = 0;
        printf("Warning: cannot create or read $HOME/%s\n", FLAGSFILE);
        return;
    }
    FlagsFile = strdup(h);
    FlagsFile = (char *)realloc(
        FlagsFile, strlen(FlagsFile) + 1 + strlen(FLAGSFILE) + 1);
    strcat(FlagsFile, "/");
    strcat(FlagsFile, FLAGSFILE);
    P("FlagsFile: %s\n", FlagsFile);
}

void findflag(FILE *f, const char *x, char **value) {
    char *line = NULL;
    char *flag = NULL;

    *value = NULL;
    rewind(f);
    while (1) {
        if (line) {
            free(line);
            line = NULL;
        }
        if (flag) {
            free(flag);
            flag = NULL;
        }
        size_t n = 0;
        int m = getline(&line, &n, f);
        if (m < 0) {
            break;
        }
        flag = (char *)malloc((strlen(line) + 1) * sizeof(char));
        m = sscanf(line, "%s", flag);
        if (strcmp(flag, x)) {
            continue;
        }
        if (m == EOF || m == 0) {
            continue;
        }
        char *rest = line + strlen(flag);
        char *p;
        p = rest;
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            p++;
        }
        rest = p;
        p = &line[strlen(line) - 1];
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            p--;
        }
        *(p + 1) = 0;
        *value = strdup(rest);
        break;
    }
    if (line) {
        free(line);
        line = NULL;
    }
    if (flag) {
        free(flag);
        line = NULL;
    }
}

void ReadFlags() {
    FILE *f;
    long int intval;
    makeflagsfile();
    if (!FlagsFileAvailable) {
        return;
    }
    f = fopen(FlagsFile, "r");
    if (f == NULL) {
        I("Cannot read %s\n", FlagsFile);
        return;
    }
    char *value = NULL;
    ;
#define DOIT_I(x, d, v)                                                        \
    findflag(f, #x, &value);                                                   \
    if (value) {                                                               \
        intval = strtol(value, NULL, 0);                                       \
        Flags.x = intval;                                                      \
        free(value);                                                           \
        value = NULL;                                                          \
    }

#define DOIT_L(x, d, v) DOIT_I(x, d, v)
#define DOIT_S(x, d, v)                                                        \
    findflag(f, #x, &value);                                                   \
    if (value) {                                                               \
        free(Flags.x);                                                         \
        Flags.x = strdup(value);                                               \
        free(value);                                                           \
        value = NULL;                                                          \
    }

    DOIT;
#include "undefall.inc"
    fclose(f);
}

void WriteFlags() {
    FILE *f;
    makeflagsfile();
    if (!FlagsFileAvailable) {
        return;
    }
    f = fopen(FlagsFile, "w");
    if (f == NULL) {
        I("Cannot write %s\n", FlagsFile);
        return;
    }
#define DOIT_I(x, d, v) fprintf(f, "%s %d\n", #x, Flags.x);
#define DOIT_L(x, d, v) fprintf(f, "%s %ld\n", #x, Flags.x);
#define DOIT_S(x, d, v) fprintf(f, "%s %s\n", #x, Flags.x);
    DOIT;
#include "undefall.inc"
    fclose(f);
}
