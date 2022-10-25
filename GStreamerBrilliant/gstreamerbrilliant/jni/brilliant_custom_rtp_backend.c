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

int build_custom_rtp_pipeline(CustomData *data)
{
  return 1;
}


void cleanup_custom_rtp_data(RTPCustomData *rtp_custom_data)
{
  if (rtp_custom_data == NULL) {
    return;
  }
  free(rtp_custom_data->incoming_video_server);
  free(rtp_custom_data->incoming_audio_server);
  free(rtp_custom_data->outgoing_audio_server);
  gst_buffer_unref(rtp_custom_data->incoming_video_key);
  gst_buffer_unref(rtp_custom_data->incoming_audio_key);
  gst_buffer_unref(rtp_custom_data->outgoing_audio_key);
  rtp_custom_data->incoming_video_server = NULL;
  rtp_custom_data->incoming_audio_server = NULL;
  rtp_custom_data->outgoing_audio_server = NULL;
  rtp_custom_data->incoming_video_key = NULL;
  rtp_custom_data->incoming_audio_key = NULL;
  rtp_custom_data->outgoing_audio_key = NULL;
}