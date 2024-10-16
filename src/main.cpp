#include "global.h"
#include "recv.h"
#include "self_info.h"
#include "send.h"
#include <cxxopts.hpp>
#include <iostream>

bool supportsColor() {
    const char* term = std::getenv("TERM");
    if (!term) {
        return false;
    }
    std::string termStr(term);
    if (termStr == "xterm" || termStr == "xterm-color" || termStr == "xterm-256color" ||
        termStr == "screen" || termStr == "screen-256color" || termStr == "tmux" ||
        termStr == "tmux-256color" || termStr == "rxvt" || termStr == "rxvt-unicode") {
        return true;
    }
    if (termStr.find("256color") != std::string::npos) {
        return true;
    }
    return false;
}
int main(int argc, char **argv) {
  cxxopts::Options opt("localsend", "一个简单的 localsend 实现");
  opt.add_options()("h", "display help");
  opt.add_options()("subcmd", "subcmd", cxxopts::value<std::string>())(
      "filenames", "filename", cxxopts::value<std::vector<std::string>>());
  opt.parse_positional({"subcmd", "filenames"});
  opt.positional_help("send|recv file1|dir [file2 [file3 [ ...] ] ]");
  auto result = opt.parse(argc, argv);
  if (result.count("h")) {
    std::cout << opt.help() << std::endl;
    exit(0);
  }
  if(supportsColor()){
	  highlight="\033[32m";
	  highlight_end="\033[0m";
  }else{
	  highlight=" ** ";
	  highlight_end=" ** ";
  }
  int timeout;
  set_myself();
  if (result.count("subcmd") &&
      result["subcmd"].as<std::string>().compare("send") == 0) {
    if (!result.count("filenames")) {
      std::cout << "没有指定要发送的文件" << std::endl;
      exit(1);
    }
    send(result["filenames"].as<std::vector<std::string>>());
    return 0;
  }
  if (result.count("subcmd") &&
      result["subcmd"].as<std::string>().compare("recv") == 0) {
    std::string savedir(".");
    if (!result.count("filenames")) {
      std::cout << "没有指定保存目录, 使用"<<highlight<<"当前目录"<<highlight_end << std::endl;
    } else {
      savedir = result["filenames"].as<std::vector<std::string>>().at(0);
    }

    start_recv(savedir);
  }
}
