# gstreamer-brilliant-android
This is the Brilliant Android GStreamer Backend.

Based on the example code from the GStreamer Library, this repository can generate an AAR that will allow a client to generate an RTSP GStreamer pipeline and run it.

## Requirements
A valid GStreamer installation located at `~/Library/Developer/GStreamerAndroid`

## Updating Version
1) Increment the `version` in the `gradle.properties` file.

## Building
Each of the following commands builds will run all previous commands (e.g. `make tar` builds both debug and release).
All products will be located in `<repo root>/build`

### To build a framework for only debug or release, run one of:
* `make debug_aar`
* `make release_aar`

### To build both debug and release, run:
* `make combined_aars`

### To make a tar of the combined aars, run:
* `make tar`

## Cleaning
Run `make clean` to clean contents of build folder (as well as the project build folder).
