#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

template <typename... Args>
void println(const Args &...args)
{
  bool end = true;
  ((std::cout << (end ? (end = false, "") : " ") << args), ...) << "\n";
}

const int DEFAULT_PORT = 8888;
const int DEFAULT_SENDER_PORT = 9999;
const int DEFAULT_BUF_SZ = 1024;

//
// @Note(impcuong): Abbrev-list
//  + simp := simple
//  + sk := socket
//  + rc := return-code
//

int main()
{
  // -----

  int simp_sk = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (simp_sk == -1)
  {
    println("ERROR: Cannot yield socket fd");
    return -1;
  }

  sockaddr_in addr;
  // memset() := writes len bytes of value `default` (converted to an unsigned char) to the string `b`.
  memset(&addr, 0 /*default=*/, sizeof(addr) /*__len=*/);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY; // @Note: Defines the scope of assignable network interface.
  // htons() := converts network host value to network (short) byte order.
  addr.sin_port = htons(DEFAULT_PORT);

  int bind_rc = bind(simp_sk, (sockaddr *)&addr, sizeof(addr));
  if (bind_rc == -1)
  {
    println("ERROR: Cannot bind socket with given address");
    return -1;
  }

  sockaddr_in sender_addr;
  memset(&sender_addr, 0 /*default=*/, sizeof(sender_addr) /*__len=*/);
  sender_addr.sin_family = AF_INET;
  sender_addr.sin_port = htons(DEFAULT_SENDER_PORT);
  socklen_t sender_packet_sz = sizeof(sender_addr);
  inet_pton(AF_INET, "127.0.0.1", &sender_addr.sin_addr);
  std::string sender_msg = "YOoo, mate, ltns ^^";
  for (int _ = 0; _ < 10; _++)
    sendto(simp_sk, sender_msg.c_str(), sender_msg.size(), 0 /*flags=*/,
        (sockaddr *)&addr, sizeof(sender_addr));

  std::atomic<bool> running = true;
  while (running)
  {
    char buf[DEFAULT_BUF_SZ];
    memset(buf, 0, DEFAULT_BUF_SZ);

    ssize_t recv_sz = recvfrom(simp_sk, buf, DEFAULT_BUF_SZ - 1, 0 /*flags=*/,
        (sockaddr *)&sender_addr, &sender_packet_sz);
    if (recv_sz)
      println("INFO: Welcome to the hell-of-simp ~", inet_ntoa(sender_addr.sin_addr));
  }
}
