#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
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
//  + in := Internet
//

bool resolve_hostname(const char *from_hostname, struct in_addr *to_addr)
{
  struct addrinfo *info;
  int rc = getaddrinfo(from_hostname /*hostname=*/, NULL, NULL, &info /*res_addrinfo=*/);
  if (rc != 0)
    return false;

  auto *sock_addr = (struct sockaddr_in *)info->ai_addr;
  memcpy(to_addr, &sock_addr->sin_addr /*__src=*/, sizeof(struct in_addr) /*__n=*/);
  freeaddrinfo(info);

  return true;
}

int main()
{
  // -----

  int simp_sk = socket(AF_INET /*address_family=*/, SOCK_DGRAM /*sock_type=*/, IPPROTO_UDP /*sock_proto=*/);
  if (simp_sk == -1)
  {
    println("ERROR: Cannot yield socket fd");
    return -1;
  }

  struct sockaddr_in addr;
  // memset() := writes len bytes of value `default` (converted to an unsigned char) to the string `b`.
  memset(&addr, 0 /*default=*/, sizeof(addr) /*__len=*/);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY; // @Note: Defines the scope of assignable network interface.
  // htons() := converts network host value to network (short) byte order.
  addr.sin_port = htons(DEFAULT_PORT);

  int bind_rc = bind(simp_sk, (struct sockaddr *)&addr, sizeof(addr));
  if (bind_rc == -1)
  {
    println("ERROR: Cannot bind socket to the given address");
    return -1;
  }

  // -----

  struct sockaddr_in sender_addr;
  socklen_t sender_addr_sz = sizeof(struct sockaddr_in);
  memset(&sender_addr, 0 /*default=*/, sender_addr_sz /*__len=*/);
  sender_addr.sin_family = AF_INET;
  sender_addr.sin_port = htons(DEFAULT_SENDER_PORT);
  // inet_pton() := converts IPv4/v6 address from text to binary form.
  inet_pton(AF_INET /*address_family=*/, "127.0.0.1" /*src=*/, &sender_addr.sin_addr /*dst=*/);

  std::string sender_msg = "YOoO, mate, ltns ^^";
  for (int _ = 0; _ < 10; _++)
    sendto(simp_sk, sender_msg.c_str(), sender_msg.size(), 0 /*flags=*/,
        (struct sockaddr *)&addr, sender_addr_sz);

  // -----

  std::atomic<bool> running = true;
  while (running)
  {
    char buf[DEFAULT_BUF_SZ];
    memset(buf, 0, DEFAULT_BUF_SZ);

    ssize_t recv_sz = recvfrom(simp_sk, buf, DEFAULT_BUF_SZ - 1, 0 /*flags=*/,
        (struct sockaddr *)&sender_addr, &sender_addr_sz);
    if (recv_sz)
    {
      in_addr res_addr = {0};
      const char *host = inet_ntoa(sender_addr.sin_addr);
      if (resolve_hostname(host, &res_addr))
      {
        println("INFO: Welcome to the hell-of-simp ~", host);
        println("INFO: Nw-number-part + local-nw-part ~", inet_netof(res_addr), inet_lnaof(res_addr));
      }
    }
  }
}
