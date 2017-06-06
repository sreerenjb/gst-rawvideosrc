/* GStreamer
 * Copyright (C) 2017 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
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

#include <gst/gst.h>
#include "gstrawvideosrc.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "rawvideosrc",
      GST_RANK_NONE, GST_TYPE_RAW_VIDEO_SRC);
}

GstPluginDesc gst_plugin_desc = {
  .major_version = GST_VERSION_MAJOR,
  .minor_version = GST_VERSION_MINOR,
  .name = "libgstrawvideosrc",
  .description =  "Src element to read raw video frames",
  .plugin_init = plugin_init,
  .version = "0.1",
  .license = "LGPL",
  .source = "gst-rawvideosrc",
  .package = "gst-rawvideosrc",
  .origin = "https://github.com/sreerenjb/gst-rawvideosrc",
};
