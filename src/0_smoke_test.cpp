#include "common.hpp"

#include <arpa/inet.h>
#include <fmt/core.h>
#include <iostream>
#include <liburing.h>
#include <memory>
#include <netinet/in.h>
#include <ranges>
#include <system_error>

const u32 ENTRIES = 32;
const usize BUF_ENTRIES = 32;
const usize BUF_LEN = 2048;

enum EventType : u32 {
  ACCEPT,
  RECV,
  WRITE,
  CLOSE,
};

typedef u32 ClientId;

struct alignas(u64) ClientEvent {
  EventType event;
  ClientId client_id;

  explicit ClientEvent(u64 data) { memcpy(this, &data, sizeof(u64)); }
  explicit ClientEvent(EventType event, ClientId client_id)
      : event{event}, client_id{client_id} {}

  operator u64() { return *static_cast<u64 *>(static_cast<void *>(this)); }
};
static_assert(sizeof(ClientEvent) == sizeof(u64));

struct Client {
  // Because we submit raw addresses from this struct to the kernel,
  // preventing all moves is a simple way to guarantee their lifetime
  // for the duration of the completion request.
  // A more defensive variation would also include a flag that is set
  // on submission, cleared on completion, and the destructor warns about.
  NONCOPYABLE(Client)
  NONMOVEABLE(Client)

  int descriptor;
  sockaddr_storage addr;
  socklen_t addr_len;
  std::array<u8, 2048> buffer;
  std::size_t buffer_len;

  Client() {}
};

typedef std::unordered_map<ClientId, std::unique_ptr<Client>> ClientMap;

void queue_accept(io_uring &ring, int server_fd, ClientId client_id,
                  std::unique_ptr<Client> &client) {
  auto *sqe = io_uring_get_sqe(&ring);
  io_uring_prep_accept(sqe, server_fd, (sockaddr *)&client->addr,
                       &client->addr_len, 0);
  io_uring_sqe_set_data64(sqe, ClientEvent{EventType::ACCEPT, client_id});
}

void queue_recv(io_uring &ring, ClientId client_id,
                std::unique_ptr<Client> &client) {
  auto *sqe = io_uring_get_sqe(&ring);
  io_uring_prep_recv(sqe, client->descriptor, client->buffer.data(),
                     client->buffer.size(), 0);
  io_uring_sqe_set_data64(sqe, ClientEvent{EventType::RECV, client_id});
}

void queue_write(io_uring &ring, ClientId client_id,
                 std::unique_ptr<Client> &client) {
  auto *sqe = io_uring_get_sqe(&ring);
  io_uring_prep_write(sqe, client->descriptor, client->buffer.data(),
                      client->buffer_len, 0);
  io_uring_sqe_set_data64(sqe, ClientEvent{EventType::WRITE, client_id});
}

void queue_close(io_uring &ring, ClientId client_id,
                 std::unique_ptr<Client> &client) {
  auto *sqe = io_uring_get_sqe(&ring);
  io_uring_prep_close(sqe, client->descriptor);
  io_uring_sqe_set_data64(sqe, ClientEvent{EventType::CLOSE, client_id});
}

int main() {
  // Set up the initial IO ring
  io_uring ring;
  OS_ERR(io_uring_queue_init(ENTRIES, &ring, 0));

  // Initialize a socket
  // Can't seem to use `io_uring_prep_socket` right now?
  // https://github.com/axboe/liburing/issues/234?
  auto socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  auto socket_addr = sockaddr_in{.sin_family = AF_INET,
                                 .sin_port = htons(4135),
                                 .sin_addr = {.s_addr = INADDR_ANY}};
  bind(socket_fd, (sockaddr *)&socket_addr, sizeof(socket_addr));
  listen(socket_fd, 10);

  ClientMap clients{};

  ClientId pending_id{};
  auto pending_client = std::unique_ptr<Client>(new Client());

  queue_accept(ring, socket_fd, pending_id, pending_client);
  io_uring_submit(&ring);

  while (true) {
    io_uring_cqe *cqe;
    io_uring_wait_cqe(&ring, &cqe);

    ClientEvent client_event{io_uring_cqe_get_data64(cqe)};
    switch (client_event.event) {
    case EventType::ACCEPT: {
      pending_client->descriptor = cqe->res;
      queue_recv(ring, pending_id, pending_client);

      clients[pending_id] = std::move(pending_client);

      pending_id += 1;
      pending_client = std::unique_ptr<Client>(new Client());
      queue_accept(ring, socket_fd, pending_id, pending_client);
      break;
    }

    case EventType::RECV: {
      auto bytes = cqe->res;
      auto &client = clients.at(client_event.client_id);
      if (bytes == 0) {
        queue_close(ring, client_event.client_id, client);
      } else {
        client->buffer_len = cqe->res;
        queue_write(ring, client_event.client_id, client);
      }
      break;
    }

    case EventType::WRITE: {
      queue_recv(ring, client_event.client_id,
                 clients.at(client_event.client_id));
      break;
    }

    case EventType::CLOSE: {
      clients.erase(client_event.client_id);
      break;
    }
    }

    io_uring_cqe_seen(&ring, cqe);
    io_uring_submit(&ring);
  }

  io_uring_queue_exit(&ring);
}