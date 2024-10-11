#include "global.h"

std::atomic<bool> run;
std::map<std::string, nlohmann::json> clients;
nlohmann::json myself;
const char *mcastip = "224.0.0.167";
const int PORT = 53317;
