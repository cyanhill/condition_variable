// MIT License

// Copyright (c) 2022 CyanHill

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <pthread.h>

#include <cassert>
#include <chrono>
#include <mutex>

namespace cyan {

using namespace std::chrono;

template <typename _Tp, typename _Up>
constexpr _Tp __ceil_impl(const _Tp& __t, const _Up& __u) {
  return (__t < __u) ? (__t + _Tp{1}) : __t;
}

// C++11-friendly version of std::chrono::ceil<D> for internal use.
template <typename _ToDur, typename _Rep, typename _Period>
constexpr _ToDur ceil(const duration<_Rep, _Period>& __d) {
  return __ceil_impl(duration_cast<_ToDur>(__d), __d);
}

}  // namespace cyan

namespace cyan {

namespace chrono = std::chrono;

using std::mutex;
using std::unique_lock;

enum class cv_status { no_timeout, timeout };

class condition_variable {
  using steady_clock = chrono::steady_clock;
  using system_clock = chrono::system_clock;
  using __clock_t    = steady_clock;
  typedef pthread_cond_t __native_type;

  __native_type _M_cond;

 public:
  typedef __native_type* native_handle_type;

  condition_variable() noexcept {
    pthread_condattr_t attr;
    assert(!pthread_condattr_init(&attr));
    assert(!pthread_condattr_setclock(&attr, CLOCK_MONOTONIC));
    assert(!pthread_cond_init(&_M_cond, &attr));
    assert(!pthread_condattr_destroy(&attr));
  }
  ~condition_variable() noexcept { assert(!pthread_cond_destroy(&_M_cond)); }

  condition_variable(const condition_variable&) = delete;
  condition_variable& operator=(const condition_variable&) = delete;

  void notify_one() noexcept { assert(!pthread_cond_signal(&_M_cond)); }

  void notify_all() noexcept { assert(!pthread_cond_broadcast(&_M_cond)); }

  void wait(unique_lock<mutex>& __lock) noexcept {
    assert(!pthread_cond_wait(&_M_cond, __lock.mutex()->native_handle()));
  }

  template <typename _Predicate>
  void wait(unique_lock<mutex>& __lock, _Predicate __p) {
    while (!__p()) wait(__lock);
  }

  template <typename _Duration>
  cv_status wait_until(unique_lock<mutex>& __lock, const chrono::time_point<steady_clock, _Duration>& __atime) {
    return __wait_until_impl(__lock, __atime);
  }

  template <typename _Duration>
  cv_status wait_until(unique_lock<mutex>& __lock, const chrono::time_point<system_clock, _Duration>& __atime) {
    return wait_until<system_clock, _Duration>(__lock, __atime);
  }

  template <typename _Clock, typename _Duration>
  cv_status wait_until(unique_lock<mutex>& __lock, const chrono::time_point<_Clock, _Duration>& __atime) {
    using __s_dur                               = typename __clock_t::duration;
    const typename _Clock::time_point __c_entry = _Clock::now();
    const __clock_t::time_point __s_entry       = __clock_t::now();
    const auto __delta                          = __atime - __c_entry;
    const auto __s_atime                        = __s_entry + ceil<__s_dur>(__delta);

    if (__wait_until_impl(__lock, __s_atime) == cv_status::no_timeout) return cv_status::no_timeout;
    // We got a timeout when measured against __clock_t but
    // we need to check against the caller-supplied clock
    // to tell whether we should return a timeout.
    if (_Clock::now() < __atime) return cv_status::no_timeout;
    return cv_status::timeout;
  }

  template <typename _Clock, typename _Duration, typename _Predicate>
  bool wait_until(unique_lock<mutex>& __lock, const chrono::time_point<_Clock, _Duration>& __atime, _Predicate __p) {
    while (!__p())
      if (wait_until(__lock, __atime) == cv_status::timeout) return __p();
    return true;
  }

  template <typename _Rep, typename _Period>
  cv_status wait_for(unique_lock<mutex>& __lock, const chrono::duration<_Rep, _Period>& __rtime) {
    using __dur = typename steady_clock::duration;
    return wait_until(__lock, steady_clock::now() + ceil<__dur>(__rtime));
  }

  template <typename _Rep, typename _Period, typename _Predicate>
  bool wait_for(unique_lock<mutex>& __lock, const chrono::duration<_Rep, _Period>& __rtime, _Predicate __p) {
    using __dur = typename steady_clock::duration;
    return wait_until(__lock, steady_clock::now() + ceil<__dur>(__rtime), std::move(__p));
  }

  native_handle_type native_handle() { return &_M_cond; }

 private:
  template <typename _Dur>
  cv_status __wait_until_impl(unique_lock<mutex>& __lock, const chrono::time_point<steady_clock, _Dur>& __atime) {
    auto __s  = chrono::time_point_cast<chrono::seconds>(__atime);
    auto __ns = chrono::duration_cast<chrono::nanoseconds>(__atime - __s);

    struct timespec __ts = {static_cast<std::time_t>(__s.time_since_epoch().count()), static_cast<long>(__ns.count())};

    auto r = pthread_cond_timedwait(&_M_cond, __lock.mutex()->native_handle(), &__ts);
    assert(r == 0 || r == ETIMEDOUT);

    return (steady_clock::now() < __atime ? cv_status::no_timeout : cv_status::timeout);
  }
};

}  // namespace cyan
