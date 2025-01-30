#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

#include "shared_ptr.hpp"
using namespace lockfree;
// using std::shared_ptr;

struct Base {
  Base() { std::cout << "Base::Base()\n"; }

  // Note: non-virtual destructor is OK here
  ~Base() { std::cout << "Base::~Base()\n"; }
};

struct Derived : public Base {
  Derived() { std::cout << "Derived::Derived()\n"; }

  ~Derived() { std::cout << "Derived::~Derived()\n"; }
};

void print(auto rem, shared_ptr<Base> const &sp) {
  std::cout << rem << "\n\tget() = " << sp.get()
            << ", use_count() = " << sp.use_count() << '\n';
}

void thr(shared_ptr<Base> p) {
  std::this_thread::sleep_for(987ms);
  shared_ptr<Base> lp = p; // thread-safe, even though the
                           // shared use_count is incremented
  {
    static std::mutex io_mutex;
    std::lock_guard<std::mutex> lk(io_mutex);
    print("Local pointer in a thread:", lp);
  }
}

int main() {
  shared_ptr<Base> p = shared_ptr<Derived>(new Derived);

  print("Created a shared Derived (as a pointer to Base)", p);

  std::thread t1{thr, p}, t2{thr, p}, t3{thr, p};
  p.reset(); // release ownership from main

  print("Shared ownership between 3 threads and released ownership from main:",
        p);

  t1.join();
  t2.join();
  t3.join();

  std::cout << "All threads completed, the last one deleted Derived.\n";
}