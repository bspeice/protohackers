#include "common.hpp"

scope_guard::scope_guard(std::function<void()> action) : action_{action} {}
scope_guard::~scope_guard() { action_(); }
