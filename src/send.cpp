#include "send.h"

#include <curl/curl.h>
#include <magic.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "global.h"
#include "sha.h"
#include "udp.h"

static int progress_callback(void *clientp, curl_off_t total_to_download,
                             curl_off_t now_download,
                             curl_off_t total_to_upload,
                             curl_off_t now_upload) {
  double progress = static_cast<double>(now_upload) / total_to_upload * 100.0;
  std::cout << "上传进度: " << now_upload << " / " << total_to_upload << "("
            << progress << "%)\r";
  std::cout.flush();
  return 0;
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *userp) {
  size_t total_size = size * nmemb;
  userp->append(static_cast<char *>(contents), total_size);
  return total_size;
}

void sendfiles(nlohmann::json &client, nlohmann::json &fileinfos) {
  CURL *curl;
  CURLcode res;

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); //
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

  curl_easy_setopt(curl, CURLOPT_POST, 1L);

  if (curl) {
    auto ip = client["ip"].get<std::string>();
    auto schema = client["protocol"].get<std::string>();
    auto baseurl = schema + "://" + ip + ":" + std::to_string(PORT) +
                   "/api/localsend/v2/"
                   "upload?";
    baseurl.append("sessionId=")
        .append(fileinfos["upload"]["sessionId"].get<std::string>());
    for (auto &f : fileinfos["upload"]["files"].items()) {
      std::string url = baseurl;
      url.append("&fileId=").append(f.key());
      url.append("&token=").append(f.value());
#ifdef DEBUG
      std::cout << url << std::endl;
#endif
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

      auto realpath = fileinfos["realpath"][f.key()].get<std::string>();

      std::ifstream file(realpath, std::ios::binary);
      if (!file) {
        std::cerr << "Could not open file for reading." << std::endl;
        return;
      }

      std::string file_content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
      file.close();

      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, file_content.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, file_content.size());
      curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
      curl_easy_setopt(curl, CURLOPT_XFERINFODATA, nullptr);
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

      std::cout << "开始传输: "
                << fileinfos["files"][f.key()]["fileName"].get<std::string>()
                << std::endl;
      res = curl_easy_perform(curl);

      if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() 失败: " << curl_easy_strerror(res)
                  << std::endl;
      }
    }
    curl_easy_cleanup(curl);
  }

  curl_global_cleanup();
  return;
}
nlohmann::json prepare_upload(nlohmann::json &client,
                              nlohmann::json &fileinfos) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    std::cerr << "Failed to initialize curl" << std::endl;
    return false;
  }

  auto ip = client["ip"].get<std::string>();
  auto schema = client["protocol"].get<std::string>();
  std::string url =
      schema + "://" + ip + ":53317/api/localsend/v2/prepare-upload";

  nlohmann::json data;
  data["info"] = myself;
  data["files"] = fileinfos["files"];
  std::string json_data = data.dump();
  std::string response_string;
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);
  nlohmann::json response;
  long response_code;
  if (res != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
              << std::endl;
  }else{
	   curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
#ifdef DEBUG
            printf("HTTP Response Code: %ld\n", response_code);
#endif
	    if(response_code==200)
  response = nlohmann::json::parse(response_string);
	    else response["error"]=response_code;

  }

  curl_easy_cleanup(curl);
  return response;
}
bool checkfiles(const std::vector<std::string> &files,
                nlohmann::json &fileinfos) {
  //
  std::cout << "检查文件 ..." << std::endl;
  magic_t magic = magic_open(MAGIC_MIME_TYPE);
  if (magic == nullptr) {
    std::cerr << "初始化 libmagic 失败!" << std::endl;
    return 1;
  }

  if (magic_load(magic, nullptr) != 0) {
    std::cerr << "载入 libmagic 数据失败: " << magic_error(magic) << std::endl;
    magic_close(magic);
    return 1;
  }

  bool status = true;
  for (auto f : files) {
    if (std::filesystem::exists(f)) {
      auto type = std::string(magic_file(magic, f.c_str()));
      auto hash = file_sha256(f);
      fileinfos["files"][hash]["id"] = hash;
      fileinfos["files"][hash]["fileName"] =
          std::filesystem::path(f).filename().string();
      fileinfos["files"][hash]["size"] = std::filesystem::file_size(f);
      fileinfos["files"][hash]["fileType"] = type;
      fileinfos["files"][hash]["sha256"] = hash;
      fileinfos["files"][hash]["preview"] = "";
      fileinfos["realpath"][hash] = f;
    } else {
      std::cerr << "文件不存在: " << f << std::endl;
      status = false;
      break;
    }
  }
  magic_close(magic);
  return status;
}
void send(const std::vector<std::string> &files) {
  nlohmann::json fileinfos;
  if (!checkfiles(files, fileinfos))
    exit(1);
  run.store(true);
  std::thread listener(start_listener);
  std::string input;

  std::cout << "开始搜索客户端 ..." << std::endl;
  std::cout << highlight<<"按 Enter 键停止搜索" <<highlight_end<< std::endl;

  while (true) {
    std::getline(std::cin, input);
    if (input.empty())
      break;
  }
  run.store(false);
  listener.join();
  if (!clients.size()) {
    std::cout << "没有发现客户端, 退出！" << std::endl;
    exit(0);
  }
  std::cout << "请选择客户端 ..." << std::endl;
  int index = 1;
  for (auto c : clients) {
    std::cout << index << ": " << c.second["alias"] << std::endl;
  }

  while (true) {
    std::cout << "请输入编号或回车选择"<<highlight<<" 1(1 - " << clients.size() << "): "<<highlight_end;

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
      index = 1;
      break;
    }

    try {
      index = std::stoi(input);
    } catch (const std::invalid_argument &) {
      std::cin.clear();
      std::cout << "无效输入" << std::endl;
      continue;
    } catch (const std::out_of_range &) {
      std::cin.clear();
      std::cout << "无效输入" << std::endl;
      continue;
    }

    if (index > 0 && index <= clients.size())
      break;
    std::cout << "无效输入" << std::endl;
  }
  auto it = clients.begin();
  std::advance(it, index - 1);
  auto response = prepare_upload(it->second, fileinfos);

std::map<long,std::string> prepareerrors;
prepareerrors.emplace(204,"");
  if(response.contains("error")){
	  auto code=response["error"].get<long>();
	  if(prepareerrors.contains(code))
	  std::cout<<prepareerrors[code]<<std::endl;
	  else std::cout<<"接收端拒收"<<std::endl;
	  return;}
  fileinfos["upload"] = response;
  sendfiles(it->second, fileinfos);
}
