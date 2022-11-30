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
#include <gio/gio.h>
#include <gst/audio/audio-channels.h>

static void decode_bin_pad_added (GstElement *decode_bin, GstPad *pad, CustomData *data)
{
  gchar *pad_name = gst_pad_get_name(pad);
  gboolean success = gst_element_link(decode_bin, data->rtp_custom_data->video_data_pipe);
  if (!success) {
    gchar *message = g_strdup_printf("CustomRTPBackend is unable to link pad %s from decode_bin to video sink.", pad_name);
    set_ui_message(message, data);
    g_free(message);
  }
  g_free(pad_name);
}

static void rtp_bin_pad_added (GstElement *rtpBin, GstPad* pad, CustomData *data) {
  gchar *pad_name = gst_pad_get_name(pad);
  GST_DEBUG("rtp_bin_pad_added, pad name: %s", pad_name);
  if (strstr(pad_name, "send_rtp_src") != NULL) {
    GstPad *sink_pad = gst_element_get_static_pad(data->rtp_custom_data->out_audio_data_pipe, "sink");
    gst_pad_link(pad, sink_pad);
  } else if (strstr(pad_name, "recv_rtp_src") != NULL) {
    GstCaps *caps = gst_pad_get_current_caps(pad);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    int isAudioPad =
        (gst_structure_has_field_typed(structure, "media", G_TYPE_STRING) &&
         g_strcmp0(g_value_get_string(gst_structure_get_value(structure, "media")), "audio") == 0);
    GstPad *sink_pad = gst_element_get_static_pad(isAudioPad ? data->rtp_custom_data->audio_depay : data->rtp_custom_data->video_depay, "sink");
    gst_pad_link(pad, sink_pad);
  }
  g_free(pad_name);
}

static GstCaps *
request_srtp_key (GstElement *element, guint ssrc, GstBuffer *key_buffer)
{
  GstCaps *caps = gst_caps_new_simple(
      "application/x-srtp",
      "ssrc", G_TYPE_UINT, ssrc,
      "srtp-key", GST_TYPE_BUFFER, key_buffer,
      "mki", GST_TYPE_BUFFER, NULL,
      "srtp-cipher", G_TYPE_STRING, "aes-128-icm",
      "srtp-auth", G_TYPE_STRING, "hmac-sha1-80",
      "srtcp-cipher", G_TYPE_STRING, "aes-128-icm",
      "srtcp-auth", G_TYPE_STRING, "hmac-sha1-80",
      NULL
  );
  return caps;
}

static GstElement * get_srtp_decoder(
    GstBuffer *key_buffer,
    uint stream_id,
    GstElement *src_element,
    GstElement *sink_element,
    GstBin *pipeline,
    GstCaps *link_caps
) {
  GstElement *srtp_dec = gst_element_factory_make("srtpdec", NULL);
  if (srtp_dec) {
    gst_bin_add(pipeline, srtp_dec);
    g_signal_connect(srtp_dec,
                     "request-key",
                     G_CALLBACK(request_srtp_key),
                     key_buffer);
    GstCaps *srtp_caps = gst_caps_copy(link_caps);
    gst_structure_set_name(gst_caps_get_structure(srtp_caps, 0), "application/x-srtp");
    gst_caps_set_simple(srtp_caps,
                        "ssrc", G_TYPE_UINT, stream_id,
                        NULL);
    g_object_set(src_element, "caps", srtp_caps, NULL);
    gst_caps_unref(srtp_caps);
    gst_element_link(src_element, srtp_dec);
    if (!sink_element) {
      gst_element_link(srtp_dec, sink_element);
    }
  } else {
    GST_WARNING("Couldn't construct srtpdec.");
    if (!sink_element) {
      gst_element_link_filtered(src_element, sink_element, link_caps);
    }
  }
  return srtp_dec;
}

static void add_srtp_encoder(
    GstBuffer *key_buffer,
    uint stream_id,
    GstElement *src_element,
    GstElement *sink_element,
    GstBin *pipeline,
    GstCaps *link_caps
) {
  GstElement *srtp_enc = gst_element_factory_make("srtpenc", NULL);
  if (srtp_enc) {
    gst_bin_add(pipeline, srtp_enc);
    g_object_set(srtp_enc, "key", key_buffer, NULL);
    GstCaps *srtpCaps = gst_caps_copy(link_caps);
    gst_caps_set_simple(srtpCaps, "ssrc", G_TYPE_UINT, stream_id, NULL);
    gst_element_link_filtered(src_element, srtp_enc, srtpCaps);
    gst_caps_unref(srtpCaps);
    gst_element_link(srtp_enc, sink_element);
  } else {
    GST_WARNING("Couldn't construct srtp_enc: sending audio might not work!");
    gst_element_link_filtered(src_element, sink_element, link_caps);
  }
}


/*
 *  Video Pipeline Diagram:
 *
 *   [rtp udpsrc]  [rtcp udpsrc]
 *        V             V
 *        |          #2 |   -------  #3
 *        |             +->|      |----->[rtcp udpsink]
 *        +-->[srtpdec]--->|rtpbin|
 *                     #1  |      |
 *                         -------
 *                          |
 *                          V                              **
 *                   [rtph264depay]-->[queue]-->[decodebin]-->[identity]-->[autovideoconvert]-->[autovideosink]
 *
 *  (**) denotes a link added in response to the pad-added signal being emitted.
 *  (#*) denotes a manual pad link
 *
 *  We separate this setup into two functions, set_up_video_sink handles [identity] onwards
 *  so that we can expose a handle to the video sink as soon as possible.
 *  set_up_receive_video_pipeline handles the set up of everything else.
 */
static int set_up_video_sink(CustomData *data) {
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when constructing video sink.");
    return FALSE;
  }
  GST_DEBUG("Starting to set up video sink");
  rtp_custom_data->video_data_pipe = gst_element_factory_make("identity", NULL);
  // Use autovideoconvert ! autovideosink
  GstElement *auto_video_convert = gst_element_factory_make("autovideoconvert", NULL);
  GstElement *auto_video_sink = gst_element_factory_make("autovideosink", NULL);
  if (!rtp_custom_data->video_data_pipe) {
  }

  gst_bin_add_many(GST_BIN(data->pipeline),
                   rtp_custom_data->video_data_pipe,
                   auto_video_convert,
                   auto_video_sink,
                   NULL);
  gboolean linking_result = gst_element_link_many(
      rtp_custom_data->video_data_pipe,
      auto_video_convert,
      auto_video_sink,
      NULL
  );
  if (!linking_result) {
    GST_ERROR("Failed to link video sink elements.");
    return FALSE;
  }
  return TRUE;
}

static int set_up_receive_video_pipeline(CustomData *data) {
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when constructing receive video pipeline.");
    return FALSE;
  }
  if (rtp_custom_data->video_depay) {
    GST_DEBUG("Video pipeline already set up.");
    return TRUE;
  }
  GST_DEBUG("Starting to set up receive video pipeline");
  GstElement *rtp_video_udp_src = gst_element_factory_make("udpsrc", "rtp_video_udp_src");
  g_object_set(rtp_video_udp_src, "port", rtp_custom_data->local_rtp_video_udp_port, NULL);
  GstElement *rtcp_video_udp_src = gst_element_factory_make("udpsrc", "rtcp_video_udp_src");
  g_object_set(rtcp_video_udp_src, "port", rtp_custom_data->local_rtcp_video_udp_port, NULL);
  GstElement *rtcp_video_udp_sink = gst_element_factory_make("udpsink", "rtcp_video_udp_sink");
  g_object_set(rtcp_video_udp_sink,
               "host", rtp_custom_data->incoming_video_server,
               "port", rtp_custom_data->incoming_video_port + 1,
               "bind-port", rtp_custom_data->local_rtcp_video_udp_port,
               "sync", FALSE,
               "async", FALSE,
               NULL);
  rtp_custom_data->video_depay = gst_element_factory_make("rtph264depay", "video_depay");
  GstElement *queue = gst_element_factory_make("queue", "video_queue");
  GstElement *h264Parse = gst_element_factory_make("h264parse", "parser");
  g_object_set(queue,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);
  GstElement *decode_bin = gst_element_factory_make("decodebin", NULL);
  g_signal_connect(G_OBJECT(decode_bin),
                   "pad-added",
                   G_CALLBACK(decode_bin_pad_added),
                   data);
  gst_bin_add_many(GST_BIN(data->pipeline),
                   rtp_video_udp_src,
                   rtcp_video_udp_src,
                   rtcp_video_udp_sink,
                   rtp_custom_data->video_depay,
                   queue,
                   h264Parse,
                   decode_bin,
                   NULL);
  gst_element_link_many(rtp_custom_data->video_depay, queue, h264Parse, decode_bin, NULL);
  GstCaps *video_caps = gst_caps_new_simple("application/x-srtp",
                                             "clock-rate", G_TYPE_INT, rtp_custom_data->incoming_video_sample_rate,
                                             "encoding-name", G_TYPE_STRING, "H264",
                                             "payload", G_TYPE_INT, rtp_custom_data->incoming_video_payload_type,
                                             "media", G_TYPE_STRING, "video",
                                             NULL);
  // incomingVideoSsrc expected to be in [0, 4,294,967,295] as values can be up to 2^31
  GstElement *srtp_dec = get_srtp_decoder(
      rtp_custom_data->incoming_video_key,
      rtp_custom_data->incoming_video_ssrc,
      rtp_video_udp_src,
      NULL,
      GST_BIN(data->pipeline),
      video_caps
  );
  if (srtp_dec == NULL) {
    GST_WARNING("Failed to set up srtpdec element in RTP Custom video pipeline.");
    return FALSE;
  }
  // #1 Manually link srtpdec:rtp_src to rtpbin:recv_rtp_sink_0
  GstPad *srtp_dec_rtp_src = gst_element_get_static_pad(srtp_dec, "rtp_src");
  GstPad *rtp_bin_recv_rtp_sink = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "recv_rtp_sink_%u");
  gst_pad_link(srtp_dec_rtp_src, rtp_bin_recv_rtp_sink);

  // #2 Manually link rtcp udpsrc:src to rtpbin:recv_rtcp_sink_0
  GstPad *rtcp_udp_src = gst_element_get_static_pad(rtcp_video_udp_src, "src");
  GstPad *rtp_bin_recv_rtcp_sink = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "recv_rtcp_sink_%u");
  gst_pad_link(rtcp_udp_src, rtp_bin_recv_rtcp_sink);

  // #3 Manually link rtpbin:send_rtcp_src_0 to rtcp udpsink:sink
  GstPad *rtp_bin_send_rtcp_src = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "send_rtcp_src_%u");
  GstPad *rtcp_udp_sink = gst_element_get_static_pad(rtcp_video_udp_sink, "sink");
  gst_pad_link(rtp_bin_send_rtcp_src, rtcp_udp_sink);
  GST_DEBUG("Completed setup of receive video pipeline");
  return TRUE;
}

static GSocket * create_socket_on_port(int port) {
  GError *error;
  GSocket *socket = g_socket_new(G_SOCKET_FAMILY_IPV4,
                                 G_SOCKET_TYPE_DATAGRAM,
                                 G_SOCKET_PROTOCOL_UDP,
                                 &error);
  if (!socket) {
    GST_ERROR("Failed to create GSocket! Error: %s", error->message);
    return NULL;
  }
  GInetAddress *host_address = g_inet_address_new_from_string("0.0.0.0");
  GSocketAddress *bind_address = g_inet_socket_address_new(host_address, port);
  if (!g_socket_bind(socket, bind_address, TRUE, &error)) {
    GST_ERROR("Failed to bind port %d. Error: %s", port, error ? error->message : "<unknown error>");
    g_socket_close(socket, NULL);
    g_object_unref(socket);
    g_object_unref(host_address);
    g_object_unref(bind_address);
    return NULL;
  }
  return socket;
}

/*
 *  Audio Pipeline Diagram:
 *
 *  Shares the same rtpbin with the video elements in the receive pipeline.
 *
 *   [rtp udpsrc]  [rtcp udpsrc]
 *        V             V
 *        |          #2 |   -------  #3
 *        |             +->|      |----->[rtcp udpsink]
 *        +-->[srtpdec]--->|rtpbin|
 *                     #1  |      |
 *                         -------
 *                            |
 *                    +-------+
 *                    V
 *             [rtpL16depay]-->[queue]-->[audioconvert]-->[volume]-->[autoaudiosink]
 *
 *  (*) denotes an optional element in the pipeline
 *  (#*) denotes a manual pad link
 */
static int set_up_receive_audio_pipeline(CustomData *data, GSocket *socket) {
  GST_DEBUG("Starting to set up receive audio pipeline");
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when constructing receive audio pipeline.");
    return FALSE;
  }
  GstElement *rtp_audio_udp_src = gst_element_factory_make("udpsrc", "rtp_audio_udp_src");
  g_object_set(rtp_audio_udp_src,
               "timeout", (guint64) 1000000000,
               "socket", socket,
               "close-socket", FALSE,
               NULL);
  GstElement *rtcp_audio_udp_src = gst_element_factory_make("udpsrc", "rtcp_audio_udp_src");
  g_object_set(rtcp_audio_udp_src, "port", rtp_custom_data->local_rtcp_audio_udp_port, NULL);
  GstElement *rtcp_audio_udp_sink = gst_element_factory_make("udpsink", "rtcp_audio_udp_sink");
  g_object_set(rtcp_audio_udp_sink,
               "host", rtp_custom_data->incoming_audio_server,
               "port", rtp_custom_data->incoming_audio_port + 1,
               "sync", FALSE,
               "async", FALSE,
               NULL);
  rtp_custom_data->audio_depay = gst_element_factory_make("rtpL16depay", "audio_depay");
  GstElement *queue = gst_element_factory_make("queue", "audio_queue");
  g_object_set(queue,
               "max-size-buffers", 0,
               "max-size-bytes", 0,
               "max-size-time", 0,
               NULL);

  GstElement *audio_convert = gst_element_factory_make("audioconvert", NULL);
  data->volume = gst_element_factory_make("volume", NULL);
  g_object_set(data->volume, "mute", TRUE, NULL);
  GstElement *auto_audio_sink = gst_element_factory_make("autoaudiosink", NULL);

  gst_bin_add_many(GST_BIN(data->pipeline),
                   rtp_audio_udp_src,
                   rtcp_audio_udp_src,
                   rtcp_audio_udp_sink,
                   rtp_custom_data->audio_depay,
                   queue,
                   audio_convert,
                   data->volume,
                   auto_audio_sink,
                   NULL);
  if (!gst_element_link(rtp_custom_data->audio_depay, queue)) {
    GST_WARNING("Failed to link audio_depay to audio_queue");
    return FALSE;
  }
  if (!gst_element_link_many(queue, audio_convert, data->volume, auto_audio_sink, NULL)) {
    GST_WARNING("Failed to link audio_queue to rest of audio pipeline");
    return FALSE;
  }
  GstCaps *audio_caps = gst_caps_new_simple("application/x-srtp",
                                            "clock-rate", G_TYPE_INT, rtp_custom_data->incoming_audio_sample_rate,
                                            "encoding-name", G_TYPE_STRING, "L16",
                                            "payload", G_TYPE_INT, rtp_custom_data->incoming_audio_payload_type,
                                            "media", G_TYPE_STRING, "audio",
                                            "channels", G_TYPE_INT, rtp_custom_data->incoming_audio_channels,
                                            "channel-mask", GST_TYPE_BITMASK, 0x3,
                                            "format", G_TYPE_STRING, "S16LE",
                                            NULL);
  GstElement *srtp_dec = get_srtp_decoder(
      rtp_custom_data->incoming_audio_key,
      rtp_custom_data->incoming_audio_ssrc,
      rtp_audio_udp_src,
      NULL,
      GST_BIN(data->pipeline),
      audio_caps
  );
  //gst_object_unref(audio_caps);
  if (srtp_dec == NULL) {
    GST_WARNING("Failed to set up srtpdec element in CustomRTP audio pipeline");
    return FALSE;
  }

  // #1 Manually link srtpdec:rtp_src to rtpbin:recv_rtp_sink_1
  GstPad *srtp_dec_rtp_src = gst_element_get_static_pad(srtp_dec, "rtp_src");
  GstPad *rtp_bin_recv_rtp_sink = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "recv_rtp_sink_%u");
  gst_pad_link(srtp_dec_rtp_src, rtp_bin_recv_rtp_sink);

  // #2 Manually link rtcp udpsrc:src to rtpbin:recv_rtcp_sink_1
  GstPad *rtcp_udp_src = gst_element_get_static_pad(rtcp_audio_udp_src, "src");
  GstPad *rtp_bin_recv_rtcp_sink = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "recv_rtcp_sink_%u");
  gst_pad_link(rtcp_udp_src, rtp_bin_recv_rtcp_sink);

  // #3 Manually link rtpbin:send_rtcp_src_1 to rtcp udpsink:sink
  GstPad *rtp_bin_send_rtcp_src = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "send_rtcp_src_%u");
  GstPad *rtcp_udp_sink = gst_element_get_static_pad(rtcp_audio_udp_sink, "sink");
  gst_pad_link(rtp_bin_send_rtcp_src, rtcp_udp_sink);

  GST_DEBUG("Completed setup of receive audio pipeline");
  return TRUE;
}

/*
 *  Outgoing Audio Pipeline Diagram:
 *
 *  Shares the same rtpbin with the elements in the receive pipeline.
 *
 *  [autioaudiosrc]
 *      V
 *  [audioconvert]       [rtcp udpsrc]
 *      V                      V
 *  [capsfilter]               |      (#1)     -------
 *      V                      +------------->|      | (#3)
 *  [volume]-->[rtpL16pay]------------------->|rtpbin|-->[rtcp udpsink]
 *                                        (#2)|      |
 *                                             -------
 *                                               V
 *                                           [identity]
 *                                               V
 *                                           [srtpenc]
 *                                               V
 *                                         [rtp udpsink]
 *
 *  (*) denotes an optional element in the pipeline
 *  (#*) denotes a manual pad link
 */
static int set_up_send_audio_pipeline(CustomData *data, GSocket *socket) {
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when constructing send audio pipeline.");
    return FALSE;
  }

  GstElement *auto_audio_src = gst_element_factory_make("autoaudiosrc", NULL);
  GstElement *audio_convert = gst_element_factory_make("audioconvert", NULL);
  GstElement *audio_resample = gst_element_factory_make("audioresample", NULL);
  GstCaps *src_caps = gst_caps_new_simple("audio/x-raw",
                                          "rate", G_TYPE_INT, rtp_custom_data->outgoing_audio_sample_rate,
                                          "channels", G_TYPE_INT, rtp_custom_data->audio_channels,
                                          "format", G_TYPE_STRING, "S16LE",
                                          "channel-mask", GST_TYPE_BITMASK, 0x3,
                                          NULL);
  GstElement *outgoing_audio_caps = gst_element_factory_make("capsfilter", "outgoing_audio_rtp_caps");
  g_object_set(outgoing_audio_caps, "caps", src_caps, NULL);
  // gst_object_unref(src_caps);

  GstElement *rtp_l16_pay = gst_element_factory_make("rtpL16pay", NULL);
  g_object_set(rtp_l16_pay, "mtu", 332, "min_ptime", 20000000, NULL);
  gst_bin_add_many(GST_BIN(data->pipeline),
                   auto_audio_src,
                   audio_convert,
                   audio_resample,
                   outgoing_audio_caps,
                   rtp_l16_pay,
                   NULL);
  if (!gst_element_sync_state_with_parent(auto_audio_src) ||
      !gst_element_sync_state_with_parent(rtp_custom_data->mic_volume)) {
    GST_WARNING("Failed to sync state while setting up audio source");
  }
  if (!gst_element_link(auto_audio_src, audio_convert)) {
    GST_ERROR("Failed to link auto_audio_src and audio_convert");
    return FALSE;
  }
  if (!gst_element_link(audio_convert, audio_resample)) {
    GST_ERROR("Failed to link audio_convert and audio_resample");
    return FALSE;
  }
  if (!gst_element_link(audio_resample, outgoing_audio_caps)) {
    GST_ERROR("Failed to link audio_resample and outgoing_audio_caps");
    return FALSE;
  }
  if (!gst_element_link(outgoing_audio_caps, rtp_custom_data->mic_volume)) {
    GST_ERROR("Failed to link outgoing_audio_caps to micVolume");
    return FALSE;
  }
  if (!gst_element_link(rtp_custom_data->mic_volume, rtp_l16_pay)) {
    GST_ERROR("Failed to link micVolume and rtpL16pay");
    return FALSE;
  }

  GstElement *rtp_audio_udp_sink = gst_element_factory_make("udpsink", "rtp_outgoing_audio_udp_sink");
  g_object_set(rtp_audio_udp_sink,
               "host", rtp_custom_data->outgoing_audio_server,
               "port", rtp_custom_data->outgoing_audio_port,
               "socket", socket,
               "close-socket", FALSE,
               "sync", FALSE,
               "async", FALSE,
               NULL);

  GstElement *rtcp_audio_udp_sink = gst_element_factory_make("udpsink", "rtcp_outgoing_audio_udp_sink");
  g_object_set(rtcp_audio_udp_sink,
               "host", rtp_custom_data->outgoing_audio_server,
               "port", rtp_custom_data->outgoing_audio_port + 1,
               "bind-port", rtp_custom_data->local_rtcp_audio_udp_port,
               "sync", FALSE,
               "async", FALSE,
               NULL);
  GstElement *rtcp_audio_udp_src = gst_element_factory_make("udpsrc", "rtcp_outgoing_audio_udp_src");
  g_object_set(rtcp_audio_udp_src, "port", rtp_custom_data->local_rtcp_audio_udp_port, NULL);
  rtp_custom_data->out_audio_data_pipe = gst_element_factory_make("identity", NULL);
  GstCaps *audio_caps = gst_caps_new_simple("application/x-rtp",
                                            "clock-rate", G_TYPE_INT, rtp_custom_data->outgoing_audio_sample_rate,
                                            "encoding-name", G_TYPE_STRING, "L16",
                                            "payload", G_TYPE_INT, rtp_custom_data->outgoing_audio_payload_type,
                                            "media", G_TYPE_STRING, "audio",
                                            "channels", G_TYPE_INT, rtp_custom_data->audio_channels,
                                            "channel-mask", GST_TYPE_BITMASK, 0x3,
                                            "format", G_TYPE_STRING, "S16LE",
                                            "ssrc", G_TYPE_UINT, rtp_custom_data->outgoing_audio_ssrc,
                                            "srtp-cipher", G_TYPE_STRING, "aes-128-icm",
                                            "srtp-auth", G_TYPE_STRING, "hmac-sha1-80",
                                            NULL);
  GstElement *rtp_caps_filter = gst_element_factory_make("capsfilter", "audio_rtp_caps");
  g_object_set(rtp_caps_filter, "caps", audio_caps, NULL);
  gst_object_unref(audio_caps);

  gst_bin_add_many(GST_BIN(data->pipeline),
                   rtp_caps_filter,
                   rtp_audio_udp_sink,
                   rtcp_audio_udp_sink,
                   rtcp_audio_udp_src,
                   rtp_custom_data->out_audio_data_pipe,
                   NULL);
  add_srtp_encoder(
      rtp_custom_data->outgoing_audio_key,
      rtp_custom_data->outgoing_audio_ssrc,
      rtp_custom_data->out_audio_data_pipe,
      rtp_audio_udp_sink,
      GST_BIN(data->pipeline),
      audio_caps
  );

  // (#1) Manually link rtcp udpsrc:src to rtpbin:recv_rtcp_sink_2
  GstPad *rtcp_udp_src = gst_element_get_static_pad(rtcp_audio_udp_src, "src");
  GstPad *rtp_bin_recv_rtcp_sink = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "recv_rtcp_sink_%u");
  gst_pad_link(rtcp_udp_src, rtp_bin_recv_rtcp_sink);

  // (#2) Manually link rtpL16pay[capsfilter]:src to rtpbin:send_rtp_sink_0, automatically creates
  // send_rtp_src_0 pad on rtpbin
  gst_element_link(rtp_l16_pay, rtp_caps_filter);
  GstPad *rtp_pay_src = gst_element_get_static_pad(rtp_caps_filter, "src");
  GstPad *rtp_bin_send_rtp_sink = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "send_rtp_sink_%u");
  gst_pad_link(rtp_pay_src, rtp_bin_send_rtp_sink);

  // (#3) Manually link rtpbin:send_rtcp_src_2 to rtcp udpsink:sink
  GstPad *rtp_bin_send_rtcp_src = gst_element_request_pad_simple(rtp_custom_data->rtp_bin, "send_rtcp_src_%u");
  GstPad *rtcp_udp_sink = gst_element_get_static_pad(rtcp_audio_udp_sink, "sink");
  gst_pad_link(rtp_bin_send_rtcp_src, rtcp_udp_sink);

  GST_DEBUG("Completed setup of send audio pipeline");
  return TRUE;
}

static int set_up_two_way_audio_pipeline(CustomData *data) {
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when constructing audio pipeline.");
    return FALSE;
  }
  if (rtp_custom_data->audio_depay) {
    // Assume audio pipeline has already been setup.
    return TRUE;
  }
  // The socket is shared for sending/receiving audio RTP packets because the server will
  // deliver audio to the port it sees packets coming from
  GSocket *audio_rtp_socket = create_socket_on_port(rtp_custom_data->local_rtp_audio_udp_port);
  if (!audio_rtp_socket) {
    GST_WARNING("Failed to create audio RTP socket.");
    return FALSE;
  }
  rtp_custom_data->audio_rtp_socket = audio_rtp_socket;
  int set_up_receive_audio_result = set_up_receive_audio_pipeline(data, audio_rtp_socket);
  if (!set_up_receive_audio_result) {
    GST_WARNING("Failed to set up incoming audio pipeline.");
    return FALSE;
  }
  int set_up_send_audio_result = set_up_send_audio_pipeline(data, audio_rtp_socket);
  if (!set_up_send_audio_result) {
    GST_WARNING("Failed to set up outgoing audio pipeline.");
    return FALSE;
  }
  return TRUE;
}

static int notify_custom_rtp_start_sending(CustomData *data, gchar *server, int port, int local_port) {
  GSocket *socket = create_socket_on_port(local_port);
  GError *error = NULL;
  GInetAddress *host_address = g_inet_address_new_from_string(server);
  GSocketAddress *dest_address = g_inet_socket_address_new(host_address, port);
  g_socket_send_to(socket, dest_address, "Start Data", 10, NULL, &error);
  if (error != NULL) {
    GST_ERROR ("Failed to notify server %s:%d to start. Error: %s", server, port, error->message);
    g_error_free(error);
    g_socket_close(socket, NULL);
    g_object_unref(socket);
    return FALSE;
  }
  g_object_unref(host_address);
  g_object_unref(dest_address);
  g_socket_close(socket, NULL);
  g_object_unref(socket);
  return TRUE;
}

int complete_custom_rtp_track_pipeline_setup(CustomData *data) {
  GST_DEBUG("Completing setup for custom rtp backend...");
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when completing custom rtp backend pipeline.");
    return FALSE;
  }
  int track_info_present = rtp_custom_data->incoming_video_server &&
      rtp_custom_data->incoming_video_key &&
      rtp_custom_data->incoming_audio_server &&
      rtp_custom_data->incoming_audio_key &&
      rtp_custom_data->outgoing_audio_server &&
      rtp_custom_data->outgoing_audio_key;
  if (!track_info_present) {
    GST_ERROR("Missing Track Info when completing custom rtp backend pipeline.");
    return FALSE;
  }
  int video_setup_result = set_up_receive_video_pipeline(data);
  if (!video_setup_result) {
    GST_WARNING("Failed to set up video pipeline.");
    return FALSE;
  }
  int notify_video_result = notify_custom_rtp_start_sending(
      data,
      rtp_custom_data->incoming_video_server,
      rtp_custom_data->incoming_video_port,
      rtp_custom_data->local_rtp_video_udp_port
  );
  if (notify_video_result != 1) {
    GST_WARNING("Failed to notify Target to start video.");
    return FALSE;
  }
  int audio_setup_result = set_up_two_way_audio_pipeline(data);
  if (!audio_setup_result) {
    GST_WARNING("Failed to set up audio pipeline.");
    return FALSE;
  }
  int notify_audio_result = notify_custom_rtp_start_sending(
      data,
      rtp_custom_data->incoming_audio_server,
      rtp_custom_data->incoming_audio_port,
      rtp_custom_data->local_rtp_audio_udp_port
  );
  if (!notify_audio_result) {
    GST_WARNING("Failed to notify Target to start audio.");
    return FALSE;
  }
  GST_DEBUG("Completed setup for custom rtp backend.");
  return TRUE;
}

int build_custom_rtp_pipeline(CustomData *data)
{
  RTPCustomData *rtp_custom_data = data->rtp_custom_data;
  if (!rtp_custom_data) {
    GST_ERROR("Missing RTPCustomData struct when constructing custom rtp backend pipeline.");
    return FALSE;
  }
  data->pipeline = gst_pipeline_new("incoming_video_pipeline");
  rtp_custom_data->rtp_bin = gst_element_factory_make("rtpbin", "incoming_video_manager");
  g_object_set(
      rtp_custom_data->rtp_bin,
      "latency", 500,
      "autoremove", TRUE,
      "buffer-mode", 1, // RTP_JITTER_BUFFER_MODE_SLAVE
      "rtcp-sync", 2, // GST_RTP_BIN_RTCP_SYNC_REP,
      NULL
  );
  g_signal_connect (G_OBJECT (rtp_custom_data->rtp_bin), "pad-added", (GCallback) rtp_bin_pad_added,
                    data);
  gst_bin_add(GST_BIN(data->pipeline), rtp_custom_data->rtp_bin);
  rtp_custom_data->mic_volume = gst_element_factory_make("volume", "mic_volume");
  g_object_set(rtp_custom_data->mic_volume, "mute", TRUE, NULL);
  gst_bin_add(GST_BIN(data->pipeline), rtp_custom_data->mic_volume);

  if (!set_up_video_sink(data)) {
    GST_ERROR("Video sink set up failed, pipeline will not work correctly.");
    return FALSE;
  }

  if (!complete_custom_rtp_track_pipeline_setup(data)) {
    GST_WARNING("Missing track info during initialization waiting until play to complete pipeline setup.");
  }
  data->volume = gst_bin_get_by_name(GST_BIN (data->pipeline), "vol");
  return TRUE;
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