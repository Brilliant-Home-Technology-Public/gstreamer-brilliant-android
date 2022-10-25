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

#ifndef GSTREAMERBRILLIANT_BRILLIANT_CUSTOM_RTP_BACKEND_H
#define GSTREAMERBRILLIANT_BRILLIANT_CUSTOM_RTP_BACKEND_H
#include "gstreamer_brilliant_android.h"

int build_custom_rtp_pipeline(CustomData *data);
void cleanup_custom_rtp_data(RTPCustomData *rtp_custom_data);

#endif //GSTREAMERBRILLIANT_BRILLIANT_CUSTOM_RTP_BACKEND_H
