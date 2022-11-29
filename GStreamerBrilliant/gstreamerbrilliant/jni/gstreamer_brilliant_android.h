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

#ifndef GSTREAMERBRILLIANT_GSTREAMER_BRILLIANT_ANDROID_H
#define GSTREAMERBRILLIANT_GSTREAMER_BRILLIANT_ANDROID_H
#include <jni.h>
#include <string.h>
#include <stdint.h>
#include <gst/gst.h>
#include <android/native_window.h>
#include <gio/gio.h>

/* Structure to contain all our Custom RTP Backend information,
 * when applicable.
 * We will also store and additional element and pipeline handles specific to this
 * pipeline type here.
 * */
typedef struct _RTPCustomData
{
  GstElement *out_audio_data_pipe;    /* The running outgoing pipeline */
  GstElement *audio_depay;            /* The audio depay element we will link the rtp src pad to */
  GstElement *video_depay;            /* The video depay element we will link the rtp src pad to */
  GstElement *mic_volume;             /* Volume element for muting and adjusting stream volume */
  GstElement *rtp_bin;                /* RTP Bin element */
  GstElement *video_data_pipe;        /* Incoming video data pipe */
  GSocket *audio_rtp_socket;          /* Shared audio RTP Socket */

  int local_rtp_video_udp_port;
  int local_rtcp_video_udp_port;
  int local_rtp_audio_udp_port;
  int local_rtcp_audio_udp_port;
  gchar *incoming_video_server;
  gchar *incoming_audio_server;
  gchar *outgoing_audio_server;
  int incoming_video_sample_rate;
  int incoming_audio_sample_rate;
  int outgoing_audio_sample_rate;
  int incoming_video_payload_type;
  int incoming_audio_payload_type;
  int outgoing_audio_payload_type;
  GstBuffer *incoming_video_key;
  GstBuffer *incoming_audio_key;
  GstBuffer *outgoing_audio_key;
  uint32_t incoming_video_ssrc;
  uint32_t incoming_audio_ssrc;
  uint32_t outgoing_audio_ssrc;
  int incoming_video_port;
  int incoming_audio_port;
  int outgoing_audio_port;
  int incoming_audio_channels;
  int audio_channels;
} RTPCustomData;

/* Structure to contain all our RTSP Backend information,
 * when applicable.
 * We will also store and additional element and pipeline handles specific to this
 * pipeline type here.
 * */
typedef struct _RTSPData {
  GstElement *rtsp_src;           /* The rtspsrc element */
} RTSPData;

/* Structure to contain all our information common to all backend types,
 * so we can pass it to callbacks
 * */
typedef struct _CustomData
{
    jobject app;                    /* Application instance, used to call its methods. A global reference is kept. */
    gchar *backend_type;            /* String constant identifying the backend pipeline */
    GstElement *pipeline;           /* The running pipeline */
    GstElement *video_sink;         /* The video sink element which receives XOverlay commands */
    GstElement *volume;             /* The volume element */
    GMainContext *context;          /* GLib context used to run the main loop */
    GMainLoop *main_loop;           /* GLib main loop */
    RTPCustomData *rtp_custom_data; /* Data used by Custom RTP pipeline */
    RTSPData *rtsp_data;            /* Data used by RTSP pipeline */
    gboolean initialized;           /* To avoid informing the UI multiple times about the initialization */
    ANativeWindow *native_window;   /* The Android native window where video will be rendered */
    GstState state;                 /* Current pipeline state */
    GstState target_state;          /* Desired pipeline state, to be set once buffering is complete */
    gint64 duration;                /* Cached clip duration */
    gint64 desired_position;        /* Position to seek to, once the pipeline is running */
    GstClockTime last_seek_time;    /* For seeking overflow prevention (throttling) */
    gboolean is_live;               /* Live streams do not use buffering */
} CustomData;
void set_ui_message (const gchar * message, CustomData * data);
#endif //GSTREAMERBRILLIANT_GSTREAMER_BRILLIANT_ANDROID_H
