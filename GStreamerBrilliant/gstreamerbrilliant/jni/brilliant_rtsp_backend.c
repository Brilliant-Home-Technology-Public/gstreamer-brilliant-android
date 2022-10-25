/*****************************************************************************
 * GStreamerBrilliant: Android Library built with system's GStreamer Implementation. Intended for use in Brilliant Mobile App.
 *****************************************************************************
 * Copyright (C) 2022 Brilliant Home Technologies
 *
 * Authors: Brilliant iOS Team <android_developer # brilliant.tech>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "gstreamer_brilliant_android.h"
#include <gst/gst.h>

int build_rtsp_pipeline(CustomData *data)
{
  RTSPData *rtsp_data = data->rtsp_data;
  if (rtsp_data == NULL) {
    GST_ERROR("RTSPData struct missing when setting up pipeline, aborting.");
    return -1;
  }
  GError *error = NULL;
  /* Build pipeline */
  char *parseLaunchString = "rtspsrc debug=true name=rtspsrc rtspsrc. ! "
                            "rtph264depay ! h264parse ! decodebin ! "
                            "autovideoconvert ! autovideosink "
                            "rtspsrc. ! decodebin ! audioconvert ! "
                            "volume name=vol ! autoaudiosink";

  data->pipeline = gst_parse_launch (parseLaunchString, &error);
  if (error) {
      gchar *message =
              g_strdup_printf ("Unable to build pipeline: %s", error->message);
      g_clear_error (&error);
      set_ui_message (message, data);
      g_free (message);
      return -1;
  }

  rtsp_data->rtsp_src = gst_bin_get_by_name(GST_BIN (data->pipeline), "rtspsrc");
  if (rtsp_data->rtsp_src == NULL) {
      GST_ERROR("Could not retrieve rtsp_src");
  }
  data->volume = gst_bin_get_by_name(GST_BIN (data->pipeline), "vol");
  if (data->volume == NULL) {
      GST_ERROR("Could not retrieve volume");
  } else {
      g_object_set(data->volume, "mute", FALSE, NULL);
  }

  g_object_set(rtsp_data->rtsp_src, "protocols", 0x4, NULL);
  g_object_set(rtsp_data->rtsp_src, "tcp-timeout",(guint64)1000000*15, NULL); // In microseconds
  return 1;
}