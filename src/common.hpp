#include <cstdint>
#include <fmt/core.h>
#include <functional>

#define NONCOPYABLE(CLASS)                                                     \
  CLASS(const CLASS &) = delete;                                               \
  CLASS &operator=(const CLASS &) = delete;

#define NONMOVEABLE(CLASS) CLASS(CLASS &&) = delete;

#define OS_ERR(F)                                                              \
  {                                                                            \
    int err_code_ = F;                                                         \
    if (err_code_ < 0) {                                                       \
      fmt::print(                                                              \
          "Error; message={}",                                                 \
          std::error_code(err_code_, std::system_category()).message());       \
    }                                                                          \
  }

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;

class scope_guard {
  std::function<void()> action_;

public:
  explicit scope_guard(std::function<void()> action);
  ~scope_guard();
};
