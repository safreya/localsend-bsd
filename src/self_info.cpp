
#include "self_info.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "sha.h"
#include "global.h"

void set_myself() {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::string current_time = std::ctime(&time_t_now);
  current_time.erase(current_time.length() - 1);
  std::string hash = str_sha256(current_time);
  auto name = std::string(std::getenv("USER")) + "_" + hash.substr(0, 4);
  myself["alias"] = name;
  myself["version"] = "2.1";
  myself["deviceModel"] = "FreeBSD";
  myself["deviceType"] = "desktop";
  myself["fingerprint"] = hash;
  myself["port"] = 53317;
  myself["protocal"] = "https";
  myself["download"] = false;
}
