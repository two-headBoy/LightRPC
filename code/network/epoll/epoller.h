#pragma once

#include <assert.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rpc {

class Channel;

class Epoller {
public:
    explicit Epoller(int maxEventCount = 1024);
    ~Epoller();

    bool AddFd(int fd, uint32_t events, Channel *channel);
    bool ModFd(int fd, uint32_t events, Channel *channel);
    bool DelFd(int fd);

    int Wait(int timeoutMs = -1);

    int GetEventFd(size_t i) const;
    uint32_t GetEvents(size_t i) const;
    Channel *GetChannel(size_t i) const;

    static bool SetNonBlocking(int fd);

private:
    int epollFd_;
    std::vector<struct epoll_event> events_;
};

}  // namespace rpc
