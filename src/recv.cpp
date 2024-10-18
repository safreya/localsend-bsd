#include "recv.h"

#include <filesystem>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <thread>

#include "global.h"
#include "selfcert.h"
#include "sha.h"
#include "udp.h"
nlohmann::json uploadsessions;

std::string genRndStr(size_t length) {
	const std::string characters =
	    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::string randomString;
	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<> distribution(0, characters.size() - 1);
	for (size_t i = 0; i < length; ++i) {
		randomString += characters[distribution(generator)];
	}

	return randomString;
}
std::string get_new_filename(const std::string &filename) {
	auto dot_position = filename.find_last_of('.');
	std::string base_name = filename.substr(0, dot_position);
	std::string extension = filename.substr(dot_position);

	std::string random_suffix = genRndStr(4);
	return base_name + "_" + random_suffix + extension;
}
void handleupload(const httplib::Request &req, httplib::Response &res,
		  const httplib::ContentReader &content_reader,
		  const std::string &savedir) {
	auto sessionId = req.get_param_value("sessionId");
	if (!uploadsessions.contains(sessionId)) {
		res.status = 400;
		return;
	}
	auto fileId = req.get_param_value("fileId");
	if (!uploadsessions[sessionId].contains(fileId)) {
		res.status = 400;
		return;
	}
	auto token = req.get_param_value("token");
	if (uploadsessions[sessionId][fileId]["token"]
		.get<std::string>()
		.compare(token) != 0) {
		res.status = 400;
		return;
	}

	std::string file_content;
	size_t recvsize = 0;
	size_t totalsize =
	    uploadsessions[sessionId][fileId]["info"]["size"].get<size_t>();
	content_reader(
	    [&](const char *data, size_t data_length) {
		    file_content.append(data, data_length);
		    recvsize += data_length;
		    std::cout
			<< "\r\033[K"
			<< uploadsessions[sessionId][fileId]["info"]["fileName"]
			       .get<std::string>()
			<< "(" << sessionId << "): " << recvsize << " / "
			<< totalsize << "("
			<< (double)recvsize / totalsize * 100 << "%)";
		    std::cout.flush();
		    return true;
	    });
	std::cout << std::endl;

	if (uploadsessions[sessionId][fileId]["info"].contains("token") &&
	    !uploadsessions[sessionId][fileId]["info"]["token"].is_null() &&
	    uploadsessions[sessionId][fileId]["info"]["token"]
		    .get<std::string>()
		    .size() > 0) {
		auto metasha256 = str_sha256(file_content);
		if (uploadsessions[sessionId][fileId]["info"]["token"]
			.get<std::string>()
			.compare(metasha256) != 0) {
			res.status = 400;
			return;
		}
	}

	std::string filename =
	    uploadsessions[sessionId][fileId]["info"]["fileName"]
		.get<std::string>();
	while (std::filesystem::exists(savedir + "/" + filename)) {
		filename = get_new_filename(filename);
	}

	std::ofstream outfile(savedir + "/" + filename, std::ios::binary);
	if (!outfile) {
		std::cerr << "打开文件出错!" << std::endl;
		res.status = 500;
		//	res.set_content("打开文件出错!",
		//"text/plain");//无须返回
		return;
	}

	outfile.write(file_content.data(), file_content.size());
	if (outfile.fail()) {
		std::cerr << "写入文件出错!" << std::endl;
		res.status = 500;
		//	res.set_content("写入文件出错!",
		//"text/plain");//无须返回
		return;
	}

	outfile.close();
	if (outfile.fail()) {
		std::cerr << "Error closing file!" << std::endl;
		res.status = 500;
		res.set_content("Error closing file!", "text/plain");
		return;
	}

	std::cout << "文件已保存为: " << filename << "(" << sessionId << ")"
		  << std::endl;
	res.status = 200;
	uploadsessions[sessionId].erase(fileId);
	if (uploadsessions[sessionId].size() == 0) {
		uploadsessions.erase(sessionId);
		std::cout << "会话 " << sessionId << " : 已完成!" << std::endl;
	}
	return;
}

void handleprepareupload(const httplib::Request &req, httplib::Response &res) {
	auto reqjson = nlohmann::json::parse(req.body);
	std::cout << reqjson["info"]["alias"].get<std::string>() << "( "
		  << req.remote_addr << " ) " << highlight << "请求发送文件"
		  << highlight_end << std::endl;
	int i = 1;
	for (auto &file : reqjson["files"].items()) {
		std::cout << i << ": "
			  << file.value()["fileName"].get<std::string>() << "( "
			  << file.value()["fileType"].get<std::string>()
			  << ", size: " << file.value()["size"].get<size_t>()
			  << " )" << std::endl;
		i++;
	}
	std::string input;
	if (autoyes)
		input = "yes";
	else {
		std::cout << "输入 " << highlight << "'yes'" << highlight_end
			  << " 继续: ";
		std::cin >> input;
	}

	if (input != "yes") {
		std::cout << "拒绝!" << std::endl;
		res.status = 403;
		return;
	}
	nlohmann::json resjson;
	auto sid = genRndStr(15);
	resjson["sessionId"] = sid;
	for (auto &file : reqjson["files"].items()) {
		auto fid = file.value()["id"].get<std::string>();
		auto ftk = genRndStr(18);
		resjson["files"][fid] = ftk;

		uploadsessions[sid][fid]["token"] = ftk;
		uploadsessions[sid][fid]["info"] = file.value();
	}
#ifdef DEBUG
	std::cout << uploadsessions.dump(4) << std::endl;
	std::cout << resjson.dump(4) << std::endl;
#endif
	res.set_content(resjson.dump(), "application/json");
	std::cout << highlight << (autoyes ? "已自动" : "") << "允许"
		  << highlight_end << "传输,会话为: " << sid << std::endl;
}
void start_recv(const std::string &savedir) {
	if (!std::filesystem::exists(savedir) ||
	    !std::filesystem::is_directory(savedir)) {
		std::cerr << savedir << ": 不存在或不是目录" << std::endl;
		exit(1);
	}
	std::string cfgdir(getenv("HOME"));
	cfgdir.append("/.config/localsend");
	if (!std::filesystem::exists(cfgdir)) {
		std::filesystem::create_directories(cfgdir);
	}
	auto certfile = cfgdir + "/server.cert";
	auto keyfile = cfgdir + "/server.pem";
	if (!std::filesystem::exists(certfile) ||
	    !std::filesystem::exists(keyfile)) {
		selfcert(cfgdir);
	}
	run.store(true);
	std::thread t(start_listener);
	httplib::SSLServer svr(certfile.c_str(), keyfile.c_str());
	svr.Post("/api/localsend/v2/prepare-upload", handleprepareupload);
	svr.Post("/api/localsend/v2/upload",
		 [savedir](const httplib::Request &req, httplib::Response &res,
			   const httplib::ContentReader &contentReader) {
			 handleupload(req, res, contentReader, savedir);
		 });
	std::cout << "接收端已准备, 等待连接..." << std::endl;
	svr.listen("0.0.0.0", 53317);
	run.store(false);
	return;
}
