# condition_variable

- condition_variable implementation to fix monotonic_clock. This is header-only, dependency C++11 and POSIX library.
- There is a problem with `std::condition_variable` in lower versions of gcc, which actually uses `std::chrono::system_clock` instead of `std::chrono::steady_clock`. [Bug 41861 (DR887) - [DR 887][C++0x] <condition_variable> does not use monotonic_clock](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=41861)
- Since new versions of gcc cannot be used, `cyan::condition_variable` can only be manually implemented instead of C++11's `std::condition_variable`.
- This problem is mainly solved by using the POSIX function `pthread_condattr_setclock` to set the attribute of the condition variable to `CLOCK_MONOTONIC`.

## Example

```cpp
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "condition_variable.h"

cyan::condition_variable cv;
std::mutex cv_m;
int i;

void waits(int idx) {
  std::unique_lock<std::mutex> lk(cv_m);
  if (cv.wait_for(lk, idx * std::chrono::milliseconds(100), [] { return i == 1; }))
    std::cerr << "Thread " << idx << " finished waiting. i == " << i << '\n';
  else
    std::cerr << "Thread " << idx << " timed out. i == " << i << '\n';
}

void signals() {
  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  std::cerr << "Notifying...\n";
  cv.notify_all();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  {
    std::lock_guard<std::mutex> lk(cv_m);
    i = 1;
  }
  std::cerr << "Notifying again...\n";
  cv.notify_all();
}

int main() {
  std::thread t1(waits, 1), t2(waits, 2), t3(waits, 3), t4(signals);
  t1.join();
  t2.join();
  t3.join();
  t4.join();
}
```

## output

```console
Thread 1 timed out. i == 0
Notifying...
Thread 2 timed out. i == 0
Notifying again...
Thread 3 finished waiting. i == 1
```
