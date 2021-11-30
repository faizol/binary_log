# binary_log

## Highlights

* Logs messages in a compact binary format
  * Average Latency:
      * ~7 ns for static data
      * ~20 ns for random integers, floats, and doubles 
      * ~130 ns for random 16-byte strings
* Extracts static information at compile-time
* Only logs the dynamic parts of the messages at runtime in the hot path
* An offline process can be used to deflate and format the log messages
* Synchronous logging - not thread safe
* Header-only library
* Requires C++20
* MIT License

```cpp
#include <binary_log/binary_log.hpp>

int main()
{
  binary_log::binary_log log("log.out");

  for (uint32_t i = 0; i < 1E6; ++i)
    BINARY_LOG(log, "Hello logger, msg number: {}", i);
}

/*
 * The above code:
 *   Runs in ~23 ms
 *   Average latency is ~18 ns
 *   File size: 5 MB (+ 32 bytes for an index file) 
*/
```

## Design Goals & Decisions

* Implement a single-threaded, synchronous logger - Do not provide thread safety
  - If the user wants multi-threaded behavior, the user can choose and implement their own queueing solution
  - There are numerous well-known lock-free queues available for this purpose ([moody::concurrentqueue](https://github.com/cameron314/concurrentqueue), [atomic_queue](https://github.com/max0x7ba/atomic_queue) etc.)
  - The latency of enqueuing into a lock-free queue is large enough to matter
    - Users who do not care about multi-threaded scenarios should NOT suffer the cost
    - Looking at the [atomic_queue benchmarks](https://max0x7ba.github.io/atomic_queue/html/benchmarks.html), the average latency across many state-of-the-art multi-producer, multi-consumer queues is around 150-250 ns
* Avoid writing information that is not requested by the user - log level, timestamp etc.
* Avoid writing static information (format string, and constants) more than once
  - Store the static information in an "index" file 
  - Store the dynamic information in the log file (refer to the index file where possible)
* Do as little work as possible in the runtime hot path
  - No formatting of any kind
  - All formatting will happen offline using an unpacker that deflates the binary logs
* Compress the logged information as much as possible
  - Never at the cost of latency
  - Use run-length encoding, if possible, to encode the number of consecutive log calls made

## Benchmarks

```console
foo@bar:~/dev/binary_log$ ./build/benchmark/binary_log_benchmark
2021-11-30T07:58:07-06:00
Running ./build/benchmark/binary_log_benchmark
Run on (16 X 3504 MHz CPU s)
Load Average: 0.52, 0.58, 0.59
---------------------------------------------------------------------------------------------
Benchmark                                                   Time             CPU   Iterations
---------------------------------------------------------------------------------------------
BM_binary_log_latency_static_string                      7.53 ns         7.50 ns     89600000
BM_binary_log_latency_one_static_integer                 7.52 ns         7.67 ns     89600000
BM_binary_log_latency_two_static_integers                7.89 ns         7.85 ns     89600000
BM_binary_log_latency_one_static_float                   7.63 ns         7.53 ns    112000000
BM_binary_log_latency_one_static_double                  7.79 ns         7.67 ns     89600000
BM_binary_log_latency_static_ints_and_doubles            7.78 ns         7.67 ns     89600000
BM_binary_log_latency_static_cstring                     7.70 ns         7.67 ns     89600000
BM_binary_log_latency_static_std_string                  8.64 ns         8.54 ns     89600000
BM_binary_log_latency_static_std_string_view             7.52 ns         7.50 ns     89600000
BM_binary_log_latency_random_integer                     24.3 ns         24.6 ns     28000000
BM_binary_log_latency_random_float                       18.7 ns         18.8 ns     37333333
BM_binary_log_latency_random_double                      26.4 ns         26.8 ns     28000000
BM_binary_log_latency_random_std_string_of_size/4        50.7 ns         51.6 ns     10000000
BM_binary_log_latency_random_std_string_of_size/8        74.6 ns         75.3 ns     11200000
BM_binary_log_latency_random_std_string_of_size/16        131 ns          132 ns      4977778
BM_binary_log_latency_random_std_string_of_size/32        235 ns          235 ns      2986667
BM_binary_log_latency_random_std_string_of_size/64        421 ns          420 ns      1600000
```

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# Licensing

<!--
Please go to https://choosealicense.com/ and choose a license that fits your
needs. GNU GPLv3 is a pretty nice option ;-)
-->
