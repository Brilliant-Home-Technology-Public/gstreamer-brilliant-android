PWD = $(shell pwd)
REPO_ROOT = $(abspath $(PWD))
PYTHON = python3
VERSION = $(shell grep "version=" GStreamerBrilliant/gradle.properties | cut -d "=" -f2)
LIBRARY_NAME = $(shell echo "gstreamer-brilliant-android-$(VERSION)")
TAR_OUTPUTNAME = $(shell echo "build/$(LIBRARY_NAME).tar.gz")
GRADLEW ?= ./gradlew

.DEFAULT_GOAL := tar

.PHONY = debug_aar release_aar combined_aars tar clean

debug_aar:
	cd GStreamerBrilliant && $(GRADLEW) gstreamerbrilliant:bundleDebugAar
	mkdir -p build/$(LIBRARY_NAME)
	find GStreamerBrilliant/gstreamerbrilliant/build/outputs -type f -name '*debug*.aar' -exec cp '{}' build/$(LIBRARY_NAME)/$(LIBRARY_NAME)-debug.aar ';'

release_aar:
	cd GStreamerBrilliant && $(GRADLEW) gstreamerbrilliant:bundleReleaseAar
	mkdir -p build/$(LIBRARY_NAME)
	find GStreamerBrilliant/gstreamerbrilliant/build/outputs -type f -name '*release*.aar' -exec cp '{}' build/$(LIBRARY_NAME)/$(LIBRARY_NAME)-release.aar ';'

tar: debug_aar release_aar
	tar -C build -zcvf $(TAR_OUTPUTNAME) $(LIBRARY_NAME)

clean:
	rm -rf build/*
	rm -rf GStreamerBrilliant/gstreamerbrilliant/build/*
