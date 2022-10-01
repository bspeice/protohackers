#include "common.hpp"

#include <arpa/inet.h>
#include <fmt/core.h>
#include <iostream>
#include <liburing.h>
#include <netinet/in.h>
#include <ranges>
#include <system_error>

const u32 ENTRIES = 32;
const usize BUF_ENTRIES = 32;
const usize BUF_LEN = 2048;

int main() {
  // Set up the initial IO ring
  io_uring ring;
  OS_ERR(io_uring_queue_init(ENTRIES, &ring, 0));

  /*
  // Set up the IO ring buffers
  std::array<std::array<u8, BUF_LEN>, BUF_ENTRIES> buffers{};
  io_uring_buf_ring buf_ring;
  io_uring_buf_ring_init(&buf_ring);

  auto buf_ring_mask = io_uring_buf_ring_mask(BUF_ENTRIES);
  for (auto i = 0; i < buffers.size(); i++) {
      auto& buffer = buffers[i];
      io_uring_buf_ring_add(&buf_ring, &buffer, buffer.size(), i, buf_ring_mask,
  i);
  }
  io_uring_buf_ring_advance(&buf_ring, BUF_ENTRIES);
  */

  // Initialize a socket
  // Can't seem to use `io_uring_prep_socket` right now?
  // https://github.com/axboe/liburing/issues/234?
  auto socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  auto socket_addr = sockaddr_in{.sin_family = AF_INET,
                                 .sin_port = htons(4135),
                                 .sin_addr = {.s_addr = INADDR_ANY}};
  bind(socket_fd, (sockaddr *)&socket_addr, sizeof(socket_addr));
  listen(socket_fd, 10);

  // Wait for connection accept
  auto *accept_sqe = io_uring_get_sqe(&ring);
  if (!accept_sqe) {
    return -1;
  }

  sockaddr client_addr{};
  socklen_t client_addr_len{};
  io_uring_prep_accept(accept_sqe, socket_fd, &client_addr, &client_addr_len,
                       0);
  io_uring_submit(&ring);

  // Wait for the connection
  io_uring_cqe *accept_cqe;
  OS_ERR(io_uring_wait_cqe(&ring, &accept_cqe));
  if (accept_cqe->res < 0) {
    return -accept_cqe->res;
  }
  auto client_socket_fd = accept_cqe->res;
  io_uring_cqe_seen(&ring, accept_cqe);

  // Read from the socket
  while (true) {
    std::array<char, 1024> recv_buffer{};

    auto *recv_sqe = io_uring_get_sqe(&ring);
    if (!recv_sqe) {
      return -1;
    }

    io_uring_prep_recv(recv_sqe, client_socket_fd, recv_buffer.data(),
                       recv_buffer.size(), 0);
    io_uring_submit(&ring);

    io_uring_cqe *recv_cqe;
    OS_ERR(io_uring_wait_cqe(&ring, &recv_cqe));
    if (!recv_cqe || recv_cqe->res < 0) {
      return !recv_cqe || -recv_cqe->res;
    }

    auto recv_bytes = recv_cqe->res;
    io_uring_cqe_seen(&ring, recv_cqe);

    if (recv_bytes == 0) {
      auto *close_sqe = io_uring_get_sqe(&ring);
      if (!close_sqe) {
        return -1;
      }

      io_uring_prep_close(close_sqe, client_socket_fd);
      io_uring_submit(&ring);
      io_uring_cqe *close_cqe;
      OS_ERR(io_uring_wait_cqe(&ring, &close_cqe));
      break;
    }

    std::string_view recv_data{recv_buffer.data(), (std::size_t)recv_bytes};
    fmt::print("Received: {}\n", recv_data);

    auto *write_sqe = io_uring_get_sqe(&ring);
    if (!write_sqe) {
      return -1;
    }

    io_uring_prep_write(write_sqe, client_socket_fd, recv_buffer.data(),
                        recv_bytes, 0);
    io_uring_submit(&ring);

    io_uring_cqe *write_cqe;
    OS_ERR(io_uring_wait_cqe(&ring, &write_cqe));
    io_uring_cqe_seen(&ring, write_cqe);

    /*
    auto recv_bytes = recv(client_socket_fd, recv_buffer.data(),
    recv_buffer.size(), 0);

    if (recv_bytes == 0) {
        close(client_socket_fd);
        break;
    }
    */
  }

  io_uring_queue_exit(&ring);
}