#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>

std::chrono::nanoseconds get_nano(double n, double f) noexcept {
  double dval = n*f + 0.3e0;
  long ival = std::round(dval);
  printf("Double Nanoseconds are: %10.3f\n", dval);
  return std::chrono::nanoseconds(ival);
}

int main() {
    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::nanoseconds ns;
    typedef std::chrono::duration<float> fnsec;
    typedef std::chrono::duration<float, std::nano> duration;

    auto t0 = Time::now();
    std::this_thread::sleep_for(ns(2000));
    auto t1 = Time::now();

    fnsec diff_ns = t1 - t0; // 1
    ns dur = std::chrono::duration_cast<ns>(diff_ns);// 2
    duration fns = t1 - t0; //3

    std::cout<<"\n--> diff count: "<< diff_ns.count();
    std::cout<<"\n--> duration(1): "<< dur.count();
    std::cout<<"\n--> duration(2): "<< fns.count();
    std::cout<<"\n";

    auto ins = get_nano(178263781.123e0, 78.3e0);
    printf("Integer nanoseconds are: %ld\n", ins.count());
    t1 -= ins;

    return 0;
}
