
noinst_LTLIBRARIES = libmcfilemanager.la

libmcfilemanager_la_SOURCES = \
	achown.c achown.h \
	boxes.c boxes.h \
	chmod.c chmod.h \
	chown.c chown.h \
	cmd.c cmd.h \
	command.c command.h \
	complete.c \
	dir.c dir.h \
	ext.c ext.h \
	file.c file.h \
	filegui.c filegui.h \
	filenot.c \
	fileopctx.c fileopctx.h \
	find.c find.h \
	hotlist.c hotlist.h \
	info.c info.h \
	layout.c layout.h \
	listmode.c listmode.h \
	midnight.h midnight.c \
	option.c option.h \
	panelize.c panelize.h \
	panel.c panel.h \
	tree.c tree.h \
	treestore.c treestore.h \
	usermenu.c usermenu.h

#if USE_MOUNTLIST
libmcfilemanager_la_SOURCES += mountlist.c mountlist.h
#endif

libmcfilemanager_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\"

libmcfilemanager_la_LIBADD = ../../lib/libmc.la
