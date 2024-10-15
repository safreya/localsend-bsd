#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <map>
#include <nlohmann/json.hpp>
#include <string>
extern std::atomic<bool> run;
extern std::map<std::string, nlohmann::json> clients;
extern const int PORT;
extern nlohmann::json myself;
extern const char *mcastip;
extern std::string highlight;
extern std::string highlight_end;
#endif
