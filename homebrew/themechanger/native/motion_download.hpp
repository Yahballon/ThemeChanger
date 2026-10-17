// SPDX-License-Identifier: GPL-3.0-or-later
// Verified HTTPS downloader adapted from PSTUBE transfer.cpp by YAHBALLON.
// Whitelisted to official motionbgs.com domain (plus subdomains).
#pragma once
#include <atomic>
#include <string>
namespace motion {
bool fetch_file(const std::string& url,const std::string& path,
                std::atomic<int>& progress,std::string& error);
}
