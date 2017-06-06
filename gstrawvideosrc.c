/* GStreamer
 * Copyright (C) 2017 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * gstrawvideosrc.c:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:element-filesrc
 * @see_also: #GstRawVideoSrc
 *
 * Read data from a file in the local file system.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 rawvideosrc location=
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>
#include "gstrawvideosrc.h"
#include <stdio.h>
#include <gst/video/video-format.h>
#include <gst/video/video-frame.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef G_OS_WIN32
#include <io.h>                 /* lseek, open, close, read */
/* On win32, stat* default to 32 bit; we need the 64-bit
 * variants, so explicitly define it that way. */
#undef stat
#define stat __stat64
#undef fstat
#define fstat _fstat64
#undef lseek
#define lseek _lseeki64
#undef off_t
#define off_t guint64
/* Prevent stat.h from defining the stat* functions as
 * _stat*, since we're explicitly overriding that */
#undef _INC_STAT_INL
#endif
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef __BIONIC__               /* Android */
#undef lseek
#define lseek lseek64
#undef fstat
#define fstat fstat64
#undef off_t
#define off_t guint64
#endif

#include <errno.h>
#include <string.h>


static const char gst_raw_video_src_caps_str[] = "video/x-raw,"
    "framerate = (fraction)[0/1, 2147483647/1],"
    "width = (int)[1, 2147483647]," "height = (int)[1, 2147483647]";

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (gst_raw_video_src_caps_str));

#ifndef S_ISREG
#define S_ISREG(mode) ((mode)&_S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(x) (0)
#endif
#ifndef O_BINARY
#define O_BINARY (0)
#endif

/* Copy of glib's g_open due to win32 libc/cross-DLL brokenness: we can't
 * use the 'file descriptor' opened in glib (and returned from this function)
 * in this library, as they may have unrelated C runtimes. */
static int
gst_open (const gchar * filename, int flags, int mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  int retval;
  int save_errno;

  if (wfilename == NULL) {
    errno = EINVAL;
    return -1;
  }

  retval = _wopen (wfilename, flags, mode);
  save_errno = errno;

  g_free (wfilename);

  errno = save_errno;
  return retval;
#elif defined (__BIONIC__)
  return open (filename, flags | O_LARGEFILE, mode);
#else
  return open (filename, flags, mode);
#endif
}

GST_DEBUG_CATEGORY_STATIC (gst_raw_video_src_debug);
#define GST_CAT_DEFAULT gst_raw_video_src_debug

/* FileSrc signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_BLOCKSIZE       4*1024

enum
{
  PROP_0,
  PROP_RAW_LOCATION,
};

static void gst_raw_video_src_finalize (GObject * object);

static void gst_raw_video_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_raw_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_raw_video_src_start (GstBaseSrc * basesrc);
static gboolean gst_raw_video_src_stop (GstBaseSrc * basesrc);

static gboolean gst_raw_video_src_is_seekable (GstBaseSrc * src);
static GstFlowReturn gst_raw_video_src_fill (GstPushSrc * src,
    GstBuffer * buf);
static gboolean gst_raw_video_src_decide_allocation (GstBaseSrc * bsrc,
    GstQuery * query);
static gboolean gst_raw_video_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_raw_video_src_debug, "rawvideosrc", 0, "test rawvideosrc element");
#define gst_raw_video_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRawVideoSrc, gst_raw_video_src, GST_TYPE_PUSH_SRC,
    _do_init);

static void
gst_raw_video_src_class_init (GstRawVideoSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_raw_video_src_set_property;
  gobject_class->get_property = gst_raw_video_src_get_property;

  g_object_class_install_property (gobject_class, PROP_RAW_LOCATION,
      g_param_spec_string ("location", "YUV/RGB File Location",
          "Location of the raw video file to read from", NULL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  gobject_class->finalize = gst_raw_video_src_finalize;

  gst_element_class_set_static_metadata (gstelement_class,
      "Raw Video File Source",
      "Source/File",
      "Read from raw video file(NV12 only)",
      "Sreerenj Balachaandran <sreerenjb@gnome.org");

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_raw_video_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_raw_video_src_stop);
  gstbasesrc_class->is_seekable =
      GST_DEBUG_FUNCPTR (gst_raw_video_src_is_seekable);
  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_raw_video_src_fill);
  gstbasesrc_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_raw_video_src_decide_allocation);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_raw_video_src_setcaps);

}

static void
gst_raw_video_src_init (GstRawVideoSrc * src)
{
  src->raw_filename = NULL;
  src->raw_fd = 0;
  src->raw_uri = NULL;

}

static void
gst_raw_video_src_finalize (GObject * object)
{
  GstRawVideoSrc *src;

  src = GST_RAW_VIDEO_SRC (object);

  g_free (src->raw_filename);
  g_free (src->raw_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_raw_video_src_set_raw_location (GstRawVideoSrc * src,
    const gchar * location)
{
  GstState state;

  /* the element must be stopped in order to do this */
  GST_OBJECT_LOCK (src);
  state = GST_STATE (src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrong_state;
  GST_OBJECT_UNLOCK (src);

  g_free (src->raw_filename);
  g_free (src->raw_uri);

  /* clear the filename if we get a NULL */
  if (location == NULL) {
    src->raw_filename = NULL;
    src->raw_uri = NULL;
  } else {
    /* we store the filename as received by the application. On Windows this
     * should be UTF8 */
    src->raw_filename = g_strdup (location);
    src->raw_uri = gst_filename_to_uri (location, NULL);
    GST_INFO ("filename : %s", src->raw_filename);
    GST_INFO ("uri      : %s", src->raw_uri);
  }
  g_object_notify (G_OBJECT (src), "yuv_location");
  /* FIXME 2.0: notify "uri" property once there is one */

  return TRUE;

  /* ERROR */
wrong_state:
  {
    g_warning ("Changing the `location' property on filesrc when a file is "
        "open is not supported.");
    GST_OBJECT_UNLOCK (src);
    return FALSE;
  }
}

static void
gst_raw_video_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRawVideoSrc *src;

  g_return_if_fail (GST_IS_RAW_VIDEO_SRC (object));

  src = GST_RAW_VIDEO_SRC (object);

  switch (prop_id) {
    case PROP_RAW_LOCATION:
      gst_raw_video_src_set_raw_location (src, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_raw_video_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRawVideoSrc *src;

  g_return_if_fail (GST_IS_RAW_VIDEO_SRC (object));

  src = GST_RAW_VIDEO_SRC (object);

  switch (prop_id) {
    case PROP_RAW_LOCATION:
      g_value_set_string (value, src->raw_filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_raw_video_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *structure;
  GstRawVideoSrc *videotestsrc;
  GstVideoInfo info;

  videotestsrc = GST_RAW_VIDEO_SRC (bsrc);
  structure = gst_caps_get_structure (caps, 0);
  GST_DEBUG_OBJECT (videotestsrc, "Getting currently set caps%" GST_PTR_FORMAT,
      caps);
  if (gst_structure_has_name (structure, "video/x-raw")) {
    /* we can use the parsing code */
    if (!gst_video_info_from_caps (&info, caps))
      goto parse_failed;
  }
  videotestsrc->info = info;

  return TRUE;

/* ERRORS */
parse_failed:
  {
    GST_DEBUG_OBJECT (bsrc, "failed to parse caps");
    return FALSE;
  }
}

static gboolean
gst_raw_video_src_decide_allocation (GstBaseSrc * bsrc, GstQuery * query)
{
  GstRawVideoSrc *videotestsrc;
  GstBufferPool *pool;
  gboolean update;
  guint size, min, max;
  GstStructure *config;
  GstCaps *caps = NULL;

  videotestsrc = GST_RAW_VIDEO_SRC (bsrc);
  if (gst_query_get_n_allocation_pools (query) > 0) {
    GST_DEBUG ("gst_raw_video_src_decide_allocation > 0");
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    /* adjust size */
    size = MAX (size, videotestsrc->info.size);
    update = TRUE;
  } else {
    pool = NULL;
    size = videotestsrc->info.size;
    min = max = 0;
    update = FALSE;
  }

  /* no downstream pool, make our own */
  if (pool == NULL) {
    pool = gst_video_buffer_pool_new ();
  }
  config = gst_buffer_pool_get_config (pool);

  gst_query_parse_allocation (query, &caps, NULL);
  if (caps)
    gst_buffer_pool_config_set_params (config, caps, size, min, max);

  if (gst_query_find_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL)) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
  }
  gst_buffer_pool_set_config (pool, config);

  if (update)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  return GST_BASE_SRC_CLASS (parent_class)->decide_allocation (bsrc, query);
}

/***
 * read code below
 * that is to say, you shouldn't read the code below, but the code that reads
 * stuff is below.  Well, you shouldn't not read the code below, feel free
 * to read it of course.  It's just that "read code below" is a pretty crappy
 * documentation string because it sounds like we're expecting you to read
 * the code to understand what it does, which, while true, is really not
 * the sort of attitude we want to be advertising.  No sir.
 *
 */
static GstFlowReturn
gst_raw_video_src_fill (GstPushSrc * basesrc, GstBuffer * buf)
{
  GstRawVideoSrc *src;
  int ret;
  guint8 *data_y;
  guint8 *data_uv;
  guint y_width;
  guint uv_width;
  guint y_stride;
  guint uv_stride;
  GstVideoFrame frame;
  int i;
  src = GST_RAW_VIDEO_SRC_CAST (basesrc);

  if (!gst_video_frame_map (&frame, &src->info, buf, GST_MAP_WRITE))
    goto buffer_write_fail;

  // todo: only for NV12 input format
  y_width = GST_VIDEO_FRAME_WIDTH (&frame);
  uv_width = GST_VIDEO_FRAME_WIDTH (&frame);
  y_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);
  uv_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 1);
  data_y = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
  data_uv = GST_VIDEO_FRAME_PLANE_DATA (&frame, 1);

  if (GST_VIDEO_FRAME_FORMAT (&frame) == GST_VIDEO_FORMAT_NV12) {
    for (i = 0; i < GST_VIDEO_FRAME_HEIGHT (&frame); i++) {
      ret = read (src->raw_fd, data_y, y_width);
      if (ret < y_width)
        goto eos;
      data_y += y_stride;
    }
    for (i = 0; i < GST_VIDEO_FRAME_HEIGHT (&frame) / 2; i++) {
      ret = read (src->raw_fd, data_uv, uv_width);
      if (ret < uv_width)
        goto eos;
      data_uv += uv_stride;
    }
  } else {
    goto format_err;
  }
  gst_video_frame_unmap (&frame);

  return GST_FLOW_OK;

eos:
  {
    GST_DEBUG ("EOS");
    gst_video_frame_unmap (&frame);
    return GST_FLOW_EOS;
  }
buffer_write_fail:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL), ("Can't write to buffer"));
    return GST_FLOW_ERROR;
  }
format_err:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL), ("only support nv12 now"));
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_raw_video_src_is_seekable (GstBaseSrc * basesrc)
{
  return TRUE;
}

/* open the file, necessary to go to READY state */
static gboolean
gst_raw_video_src_start (GstBaseSrc * basesrc)
{
  GstRawVideoSrc *src = GST_RAW_VIDEO_SRC (basesrc);
  struct stat stat_results;

  if (src->raw_filename == NULL || src->raw_filename[0] == '\0')
    goto no_filename;

  GST_INFO_OBJECT (src, "opening yuv file %s", src->raw_filename);

  /* open the file */
  src->raw_fd = gst_open (src->raw_filename, O_RDONLY | O_BINARY, 0);

  if (src->raw_fd < 0)
    goto open_failed;

  /* check if it is a regular file, otherwise bail out */
  if (fstat (src->raw_fd, &stat_results) < 0)
    goto no_stat;

  if (S_ISDIR (stat_results.st_mode))
    goto was_directory;

  if (S_ISSOCK (stat_results.st_mode))
    goto was_socket;

  src->read_frame_idx = 0;

  return TRUE;

  /* ERROR */
no_filename:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
        ("No file name specified for reading."));
    goto error_exit;
  }
open_failed:
  {
    switch (errno) {
      case ENOENT:
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
            ("No such file \"%s\"", src->raw_filename));
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
            ("Could not open file \"%s\" for reading.", src->raw_filename),
            GST_ERROR_SYSTEM);
        break;
    }
    goto error_exit;
  }

no_stat:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Could not get info on \"%s\".", src->raw_filename), (NULL));
    goto error_close;
  }
was_directory:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("\"%s\" is a directory.", src->raw_filename), (NULL));
    goto error_close;
  }
was_socket:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("File \"%s\" is a socket.", src->raw_filename), (NULL));
    goto error_close;
  }
error_close:
  {
    if (src->raw_fd > 0)
      close (src->raw_fd);
  }
error_exit:
  return FALSE;
}

/* unmap and close the file */
static gboolean
gst_raw_video_src_stop (GstBaseSrc * basesrc)
{
  GstRawVideoSrc *src = GST_RAW_VIDEO_SRC (basesrc);

  /* close the file */
  if (src->raw_fd > 0)
    close (src->raw_fd);

  /* zero out a lot of our state */
  src->raw_fd = 0;
  return TRUE;
}
