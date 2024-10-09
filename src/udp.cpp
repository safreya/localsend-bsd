#include "udp.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include "global.h"


void udpresponse(std::string ip) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    std::cerr << "创建 socket 失败!" << std::endl;
    return;
  }

  sockaddr_in target_addr;
  target_addr.sin_family = AF_INET;
  target_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, ip.c_str(), &target_addr.sin_addr);

  auto msg = myself.dump();
  ssize_t send_length =
      sendto(sock, msg.c_str(), msg.size(), 0, (struct sockaddr *)&target_addr,
             sizeof(target_addr));

  if (send_length < 0) {
    std::cerr << "发送回应消息失败!" << std::endl;
    close(sock);
  }
#ifdef DEBUG
  std::cout << "Message sent: " << msg << std::endl;
#endif

  close(sock);
  return;
}

void procAnnounce(const char *buffer, const std::string &ip) {
  auto recvjson = nlohmann::json::parse(buffer, nullptr, false);
  if (!recvjson.is_discarded()) {
    auto fingerprint = recvjson["fingerprint"].get<std::string>();

    if (fingerprint.compare(myself["fingerprint"].get<std::string>()) == 0)
      return;
    if (clients.count(fingerprint))
      return;
    recvjson["ip"] = ip;
    clients[fingerprint] = recvjson;
    std::cout << "找到: " << recvjson["alias"].get<std::string>() << std::endl;
    if (recvjson["announce"].get<bool>())
      udpresponse(ip);
  }
}

void setupSocket(int &sockfd, const char *ipAddress, int port,
                 bool isMulticast) {
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket creation error");
    exit(EXIT_FAILURE);
  }

  int reuse = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = isMulticast ? inet_addr(ipAddress) : INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind error");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  if (isMulticast) {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ipAddress);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <
        0) {
      perror("setsockopt for multicast error");
      close(sockfd);
      exit(EXIT_FAILURE);
    }
  }

  // 设置为非阻塞模式
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int receiveData(int sockfd, char *buffer, size_t bufferSize,
                sockaddr_in &addr) {
  socklen_t addr_len = sizeof(addr);
  int nbytes = recvfrom(sockfd, buffer, bufferSize, 0, (struct sockaddr *)&addr,
                        &addr_len);
  if (nbytes >= 0) {
    buffer[nbytes] = '\0'; // 确保以 null 结尾
    return nbytes;
  } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
    perror("recvfrom error");
  }
  return -1;
}

void start_listener() {
  int multicast_sockfd, udp_sockfd;

  setupSocket(multicast_sockfd, mcastip, PORT, true);
  setupSocket(udp_sockfd, nullptr, PORT, false);

  char buffer[1024];
  sockaddr_in addr;

  while (run.load()) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(multicast_sockfd, &readfds);
    FD_SET(udp_sockfd, &readfds);

    // 等待活动的文件描述符，设置超时时间为1秒
    struct timeval timeout;
    timeout.tv_sec = 1;  // 1秒
    timeout.tv_usec = 0; // 0微秒

    int maxfd = std::max(multicast_sockfd, udp_sockfd) + 1;
    int activity = select(maxfd, &readfds, nullptr, nullptr, &timeout);

    if (activity < 0) {
      perror("select error");
      break;
    } else if (activity == 0) {
      continue; // 超时，不做任何事情
    }

    // 处理多播消息
    if (FD_ISSET(multicast_sockfd, &readfds)) {
      if (receiveData(multicast_sockfd, buffer, sizeof(buffer), addr) > 0) {
        procAnnounce(buffer,
                     inet_ntoa(addr.sin_addr)); // 处理多播消息
      }
    }

    // 处理普通 UDP 消息
    if (FD_ISSET(udp_sockfd, &readfds)) {
      if (receiveData(udp_sockfd, buffer, sizeof(buffer), addr) > 0) {
        procAnnounce(buffer, inet_ntoa(addr.sin_addr));
      }
    }
  }

  close(udp_sockfd);
  close(multicast_sockfd);
}
