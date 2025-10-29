#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <thread>

#include <arpa/inet.h>
#include <execinfo.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

template <typename... Args>
void println(const Args &...args)
{
  bool end = true;
  ((std::cout << (end ? (end = false, "") : " ") << args), ...) << "\n";
}

//
// @Note(impcuong): Abbrev-list
//  + simp := simple
//  + sk   := socket
//  + rc   := return-code
//  + in   := Internet
//  + dfl  := default
//

void capture_signal_then_report(int sig_no, siginfo_t *info, void *_ctx)
{
  fprintf(stderr, "INFO: Emitted signal: %d\n", sig_no);
  fprintf(stderr, "INFO: Violated address (si_addr): %p\n", info->si_addr);

  const int _depth = 20;
  void *report[_depth];
  size_t size = backtrace(report, _depth);
  fprintf(stderr, "INFO: Stack-trace:\n");
  // Using the literal '2' for STDERR_FILENO since you noted the constant issue
  backtrace_symbols_fd(report, size, 2 /*fd=*/);

  signal(sig_no, SIG_DFL);
  raise(sig_no);
}

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

const int DEFAULT_BUF_SZ = 1024;

std::atomic<bool> running(true);

void recv_msg_cb(int sk_fd)
{
  char buf[DEFAULT_BUF_SZ];

  struct sockaddr_in sender_addr = {0};
  socklen_t sender_addr_sz = sizeof(struct sockaddr_in);

  while (running)
  {
    memset(buf, 0, DEFAULT_BUF_SZ);

    ssize_t recv_sz = recvfrom(sk_fd, buf, DEFAULT_BUF_SZ - 1, 0 /*flags=*/,
        (struct sockaddr *)&sender_addr, &sender_addr_sz);
    if (recv_sz > 0)
    {
      buf[recv_sz] = '\0';
      const char *ip = inet_ntoa(sender_addr.sin_addr);
      uint16_t port = ntohs(sender_addr.sin_port);

      std::string msg = std::format(">> {}:{} said: {}", ip, port, buf);
      println(msg);

      std::cout.flush();
    }
  }
}

int main(int argc, char **argv)
{
#define ENABLE_STACKTRACE
#ifdef ENABLE_STACKTRACE
  struct sigaction sig_reporter = {0};
  // union __sigaction_u
  // {
  //   void (*__sa_handler)(int);
  //   void (*__sa_sigaction)(int, siginfo_t *, void *);
  // };
  // #define sa_sigaction __sigaction_u.__sa_sigaction
  sig_reporter.sa_sigaction = capture_signal_then_report;
  sigemptyset(&sig_reporter.sa_mask);
  sig_reporter.sa_flags = SA_SIGINFO;

  if (sigaction(SIGSEGV, &sig_reporter, NULL) == -1)
    perror("ERROR: Segmentation fault has been found!");
#endif

  // -----

  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <my_port> <peer_port>" << std::endl;
    std::cout << "Example: " << argv[0] << " 8888 9999" << std::endl;
    return 1;
  }

  int port = atoi(argv[1]);
  int peer_port = atoi(argv[2]);

  // -----

  int simp_sk = socket(AF_INET /*address_family=*/, SOCK_DGRAM /*sock_type=*/, IPPROTO_UDP /*sock_proto=*/);
  if (simp_sk == -1)
  {
    println("ERROR: Cannot yield socket fd");
    return -1;
  }

  struct sockaddr_in addr;
  // memset() := writes len bytes of value `default` (converted to an unsigned char) to the string `b`.
  memset(&addr, 0 /*default=*/, sizeof(struct sockaddr_in) /*__len=*/);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY; // @Note: Defines the scope of assignable network interface.
  // htons() := converts network host value to network (short) byte order.
  addr.sin_port = htons(port);

  int opt = 1;
  //
  // @Docs:
  //  + https://stackoverflow.com/questions/21515946/what-is-sol-socket-used-for
  //  + https://pubs.opengroup.org/onlinepubs/7908799/xns/getsockopt.html
  //
  // setsockopt(simp_sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  int rc = bind(simp_sk, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
  if (rc != 0)
  {
    println("ERROR: Cannot bind socket to the given address");
    return -1;
  }

  // -----

  in_addr res_addr = {0};
  const char *host = inet_ntoa(addr.sin_addr);
  if (resolve_hostname(host, &res_addr))
  {
    println("INFO: Welcome to the hell-of-simp ~", host, port);
    println("INFO: Nw-number-part + local-nw-part ~", inet_netof(res_addr), inet_lnaof(res_addr));
  }

  // -----

  struct sockaddr_in peer_addr = {0};
  socklen_t peer_addr_sz = sizeof(struct sockaddr_in);

  memset(&peer_addr, 0 /*default=*/, peer_addr_sz /*__len=*/);
  peer_addr.sin_family = AF_INET;
  peer_addr.sin_port = htons(peer_port); // @Fixme: Accepts only one client.
  // inet_pton() := converts IPv4/v6 address from text to binary form.
  inet_pton(AF_INET /*address_family=*/, "127.0.0.1" /*src=*/, &(peer_addr.sin_addr) /*dst=*/);

  // -----

  std::thread recv_thread(&recv_msg_cb, simp_sk);

  std::string msg;
  while (running && std::getline(std::cin, msg))
  {
    if (msg == "/q" || msg == "quit")
    {
      running = false;
      return 0;
    }

    println(">> You:", msg);
    sendto(simp_sk, msg.c_str(), msg.length(), 0,
        (struct sockaddr *)&peer_addr, peer_addr_sz);
    std::cout.flush();
  }

  recv_thread.join();
  return 0;
}
