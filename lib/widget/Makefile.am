
noinst_LTLIBRARIES = libmcwidget.la

libmcwidget_la_SOURCES = \
	button.c button.h \
	buttonbar.c buttonbar.h \
	check.c check.h \
	dialog.c dialog.h \
	dialog-switch.c dialog-switch.h \
	gauge.c gauge.h \
	groupbox.c groupbox.h \
	hline.c hline.h \
	history.c history.h \
	input.c input.h \
	listbox-window.c listbox-window.h \
	listbox.c listbox.h \
	label.c label.h \
	menu.c menu.h \
	quick.c quick.h \
	radio.c radio.h \
	widget-common.c widget-common.h \
	wtools.c wtools.h

libmcwidget_la_CFLAGS = -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(PCRE_CFLAGS) \
	-DDATADIR=\""$(pkgdatadir)/"\" -DLOCALEDIR=\""$(localedir)"\" 
