/* GStreamer
 * Copyright (C) 2017 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 *
 * gstrawvideosrc.h:
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

#ifndef __GST_RAW_VIDEO_SRC_H__
#define __GST_RAW_VIDEO_SRC_H__

#include <sys/types.h>

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gstpushsrc.h>

#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>
G_BEGIN_DECLS

#define GST_TYPE_RAW_VIDEO_SRC \
  (gst_raw_video_src_get_type())
#define GST_RAW_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RAW_VIDEO_SRC,GstRawVideoSrc))
#define GST_RAW_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RAW_VIDEO_SRC,GstRawVideoSrcClass))
#define GST_IS_RAW_VIDEO_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RAW_VIDEO_SRC))
#define GST_IS_RAW_VIDEO_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RAW_VIDEO_SRC))
#define GST_RAW_VIDEO_SRC_CAST(obj) ((GstRawVideoSrc*) obj)

typedef struct _GstRawVideoSrc GstRawVideoSrc;
typedef struct _GstRawVideoSrcClass GstRawVideoSrcClass;

/**
 * GstFileSrc:
 *
 * Opaque #GstFileSrc structure.
 */
struct _GstRawVideoSrc {
  GstPushSrc element;

  /*< private >*/
  GstVideoInfo info;
  gchar *raw_filename;			/* filename */
  gchar *raw_uri;				/* caching the URI */
  gint raw_fd;				/* open file descriptor */
  guint width;				/* frame width */
  guint height;				/* frame height */
  gint read_frame_idx;		       /* read frame index of fd */
};

struct _GstRawVideoSrcClass {
  GstPushSrcClass parent_class;
};

GType 
gst_raw_video_src_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GST_FILE_SRC_H__ */
