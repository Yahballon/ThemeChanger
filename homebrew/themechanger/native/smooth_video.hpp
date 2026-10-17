// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <string>
namespace smooth_video {
// Returns true for H264 streams with too many reference frames for the PS5
// ShellUI FullHD decoder. Reads the actual SPS, not merely level_idc.
bool needs_conversion(const std::string& path);
// FFmpeg / x264 software conversion, preserving the original until complete.
// This runs on an isolated download worker, never on the render/UI thread.
bool convert(const std::string& path,std::atomic<int>& progress,std::string& error);
}
