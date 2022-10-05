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


/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
    jobject app;                  /* Application instance, used to call its methods. A global reference is kept. */
    GstElement *pipeline;         /* The running pipeline */
    GstElement *rtsp_src;         /* The rtspsrc element */
    GstElement *video_sink;       /* The video sink element which receives XOverlay commands */
    GstElement *volume;           /* The volume element */
    GMainContext *context;        /* GLib context used to run the main loop */
    GMainLoop *main_loop;         /* GLib main loop */
    gboolean initialized;         /* To avoid informing the UI multiple times about the initialization */
    ANativeWindow *native_window; /* The Android native window where video will be rendered */
    GstState state;               /* Current pipeline state */
    GstState target_state;        /* Desired pipeline state, to be set once buffering is complete */
    gint64 duration;              /* Cached clip duration */
    gint64 desired_position;      /* Position to seek to, once the pipeline is running */
    GstClockTime last_seek_time;  /* For seeking overflow prevention (throttling) */
    gboolean is_live;             /* Live streams do not use buffering */
} CustomData;
void set_ui_message (const gchar * message, CustomData * data);
#endif //GSTREAMERBRILLIANT_GSTREAMER_BRILLIANT_ANDROID_H
