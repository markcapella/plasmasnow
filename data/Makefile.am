EXTRA_DIST = plasmasnow.appdata.xml.in plasmasnow.desktop.in
CLEANFILES = plasmasnow.appdata.xml plasmasnow.desktop

plasmasnow.appdata.xml: $(top_srcdir)/data/plasmasnow.appdata.xml.in
	sed "s/plasmasnow_VERSION/$(PACKAGE_VERSION)/;s/plasmasnow_DATE/$(DATE)/" $(top_srcdir)/data/plasmasnow.appdata.xml.in > $@

plasmasnow.desktop: $(top_srcdir)/data/plasmasnow.desktop.in
	sed "s/plasmasnow_VERSION/$(PACKAGE_VERSION)/;s/plasmasnow_DATE/$(DATE)/" $(top_srcdir)/data/plasmasnow.desktop.in > $@

appdatadir = $(datadir)/metainfo
appdata_DATA = plasmasnow.appdata.xml

desktopdir = $(datadir)/applications
desktop_DATA = plasmasnow.desktop
