lib_LTLIBRARIES = libgstrawvideosrc.la

noinst_HEADERS = gstrawvideosrc.h
libgstrawvideosrc_la_SOURCES = gstrawvideosrc.c plugin.c
libgstrawvideosrc_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(GST_VIDEO_CFLAGS)
libgstrawvideosrc_la_LIBADD = \
        $(GST_LIBS) \
        $(GST_BASE_LIBS) \
        $(GST_VIDEO_LIBS) \
        $(NULL)

libdir = ${exec_prefix}/lib/gstreamer-1.0
