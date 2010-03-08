END=

AM_CPPFLAGS = -I$(top_srcdir)/gtkhtml $(GTKHTML_CFLAGS) $(I18N_LIB_CFLAGS)

AM_CFLAGS = $(GAIL_CFLAGS)

noinst_LTLIBRARIES = libgtkhtml-a11y.la

libgtkhtml_a11y_la_SOURCES =				\
	cell.h						\
	cell.c						\
	factory.h					\
	factory.c					\
	html.h						\
	html.c						\
	hyperlink.h					\
	hyperlink.c					\
	image.h						\
	image.c						\
	object.h					\
	object.c					\
	paragraph.h					\
	paragraph.c					\
	table.h						\
	table.c						\
	text.h						\
	text.c						\
	utils.h						\
	utils.c						\
	$(END)

-include $(top_srcdir)/git.mk
