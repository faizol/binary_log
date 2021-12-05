<p align="center">
  <img height="60" src="images/logo.png"/>  
</p>

# Highlights

* Logs messages in a compact binary format
* Fast
  * ***Hundreds of millions*** of logs per second
  * 2-6 ns average latency on basic data types
* Extracts static information at compile-time
* Only logs the dynamic parts of the messages at runtime in the hot path
* Provides an [unpacker](https://github.com/p-ranav/binary_log/tree/master/tools/unpacker) to deflate the log messages
* Uses [fmtlib](https://github.com/fmtlib/fmt) to format the logs
* Synchronous logging - not thread safe
* Header-only library
* Requires C++20
* MIT License

# Usage and Performance

The following code logs 1 billion integers to file.

```cpp
#include <binary_log/binary_log.hpp>

int main()
{
  binary_log::binary_log log("log.out");

  for (int i = 0; i < 1E9; ++i)
    BINARY_LOG(log, "Hello logger, msg number: {}", i);
}
```

On a [modern workstation desktop](#hardware-details), the above code executes in `~4.1s`.

| Type            | Value                                       |
| --------------- | ------------------------------------------- |
| Time Taken      | 4.1 s                                       | 
| Throughput      | 1.465 Gb/s                                  |
| Performance     | 244 million logs/s                          |
| Average Latency | 4.1 ns                                      |
| File Size       | ~6 GB (log file) + 32 bytes (index file)    |

```console
foo@bar:~/dev/binary_log$ time ./build/example/example

real    0m4.093s
user    0m2.672s
sys     0m1.422s

foo@bar:~/dev/binary_log$ ls -lart log.out*
-rw-r--r-- 1 pranav pranav         32 Dec  3 13:33 log.out.index
-rw-r--r-- 1 pranav pranav 5999868672 Dec  3 13:33 log.out
```

## Deflate the logs

These binary log files can be deflated using the provided [unpacker](https://github.com/p-ranav/binary_log/tree/master/tools/unpacker) app:

```console
foo@bar:~/dev/binary_log$ ./build/tools/unpacker/unpacker log.out > log.deflated

foo@bar:~/dev/binary_log$ wc -l log.deflated
1000000000 log.deflated

foo@bar:~/dev/binary_log$ $ head log.deflated
Hello logger, msg number: 0
Hello logger, msg number: 1
Hello logger, msg number: 2
Hello logger, msg number: 3
Hello logger, msg number: 4
Hello logger, msg number: 5
Hello logger, msg number: 6
Hello logger, msg number: 7
Hello logger, msg number: 8
Hello logger, msg number: 9

foo@bar:~/dev/binary_log$ tail log.deflated
Hello logger, msg number: 999999990
Hello logger, msg number: 999999991
Hello logger, msg number: 999999992
Hello logger, msg number: 999999993
Hello logger, msg number: 999999994
Hello logger, msg number: 999999995
Hello logger, msg number: 999999996
Hello logger, msg number: 999999997
Hello logger, msg number: 999999998
Hello logger, msg number: 999999999

foo@bar:~/dev/binary_log$ ls -lart log.deflated
-rw-r--r-- 1 pranav pranav 35888888890 Dec  3 18:09 log.deflated
```

| Type                | Value     |
| ------------------- | --------- |
| Time Taken          | 2m 33s    | 
| Throughput          | 234 MB/s  |
| Deflated File Size  | ~35 GB    |

See [benchmarks](https://github.com/p-ranav/binary_log/blob/master/README.md#benchmarks) section for more performance metrics.

# Design Goals & Decisions

* Implement a single-threaded, synchronous logger - Do not provide thread safety
  - If the user wants multi-threaded behavior, the user can choose and implement their own queueing solution
  - There are numerous well-known lock-free queues available for this purpose ([moody::concurrentqueue](https://github.com/cameron314/concurrentqueue), [atomic_queue](https://github.com/max0x7ba/atomic_queue) etc.) - let the user choose the technology they want to use.
  - The latency of enqueuing into a lock-free queue is large enough to matter
    - Users who do not care about multi-threaded scenarios should NOT suffer the cost
    - Looking at the [atomic_queue benchmarks](https://max0x7ba.github.io/atomic_queue/html/benchmarks.html), the average latency across many state-of-the-art multi-producer, multi-consumer queues is around 150-250 ns
* Avoid writing static information (format string, and constants) more than once
  - Store the static information in an "index" file 
  - Store the dynamic information in the log file (refer to the index file where possible)
* Do as little work as possible in the runtime hot path
  - No formatting of any kind
  - All formatting will happen offline using an unpacker that deflates the binary logs

# How it Works

`binary_log` splits the logging into three files:

<p>
  <img height="600" src="images/how_it_works.png"/>  
</p>

1. ***Index file*** contains all the static information from the logs, e.g., format string, number of args, type of each arg etc.
   - If a format argument is marked as constant using `binary_log::constant`, the value of the arg is also stored in the index file
2. ***Log file*** contains two pieces of information per log call:
   1. An index into the index table (in the index file) to know which format string was used
      - If runlength encoding is working, this index might not be written, instead the final runlength will be written to the runlengths file
   3. The value of each argument
3. ***Runlength file*** contains runlengths - If a log call is made 5 times, this information is stored here (instead of storing the index 5 times in the log file)
   - NOTE: Runlengths are only stored if the runlength > 1 (to avoid the inflation case with RLE)

## Packing Integers

`binary_log` packs integers based on value. An argument of type `uint64_t` with a value of `19` will be packed as a `uint8_t`, saving 7 bytes of space (actually 6, one byte is required to store the number of bytes consumed by the integer)

Consider the example:

```cpp
  uint64_t value = 19;
  BINARY_LOG(log, "{}", value);
```

Here is the index file:

```console
foo@bar:~/dev/binary_log$ hexdump -C log.out.index
00000000  02 7b 7d 01 05 00                                 |.{}...|
00000006
```

This index file has 6 bytes of information:
* `0x02` - Length of the format string
* `0x7b 0x7d` - This is the format string `"{}"`
* `0x01` - This is the number of arguments required
* `0x05` - This is the type of the argument `uint64_t` (according to [this](https://github.com/p-ranav/binary_log/blob/master/include/binary_log/detail/args.hpp#L17) enum class)
* `0x00` - To indicate that this value is not a "constant"

Here is the log file

```console
foo@bar:~/dev/binary_log$ hexdump -C log.out
00000000  00 01 13                                          |...|
00000003
```

The log file has 3 bytes: 
* `0x00` indicates that the first format string in the table (in the index file) is being used
* `0x01` indicates that the integer argument takes up 1 byte
* `0x13` is the value - the decimal `19`

## Constants

One can specify a log format argument as a constant by wrapping the value with `binary_log::constant(...)`. When this is detected, the value is stored in the index file instead of the log file as it is now considered "static information" and does not change between calls. 

```cpp
for (auto i = 0; i < 1E9; ++i) {
  BINARY_LOG(log, "Joystick {}: x_min={}, x_max={}, y_min={}, y_max={}",
             binary_log::constant("Nintendo Joycon"),
             binary_log::constant(-0.6),
             binary_log::constant(+0.65),
             binary_log::constant(-0.54),
             binary_log::constant(+0.71));
}
```

The above loop runs in under `500 ms`. The final output is compact at just `118 bytes` and contains all the information needed to deflate the log (if needed). 

| File               | Size      |
| ------------------ | --------- |
| log.out            | 1 byte    | 
| log.out.index      | 6 bytes   |
| log.out.runlength  | 111 bytes |

```console
foo@bar:~/dev/binary_log$ ls -lart log.out*
-rw-r--r-- 1 pranav pranav   6 Dec  5 08:41 log.out.runlength
-rw-r--r-- 1 pranav pranav 111 Dec  5 08:41 log.out.index
-rw-r--r-- 1 pranav pranav   1 Dec  5 08:41 log.out

foo@bar:~/dev/binary_log$ hexdump -C log.out.index
00000000  33 4a 6f 79 73 74 69 63  6b 20 7b 7d 3a 20 78 5f  |3Joystick {}: x_|
00000010  6d 69 6e 3d 7b 7d 2c 20  78 5f 6d 61 78 3d 7b 7d  |min={}, x_max={}|
00000020  2c 20 79 5f 6d 69 6e 3d  7b 7d 2c 20 79 5f 6d 61  |, y_min={}, y_ma|
00000030  78 3d 7b 7d 05 0c 0b 0b  0b 0b 01 0f 4e 69 6e 74  |x={}........Nint|
00000040  65 6e 64 6f 20 4a 6f 79  63 6f 6e 01 33 33 33 33  |endo Joycon.3333|
00000050  33 33 e3 bf 01 cd cc cc  cc cc cc e4 3f 01 48 e1  |33..........?.H.|
00000060  7a 14 ae 47 e1 bf 01 b8  1e 85 eb 51 b8 e6 3f     |z..G.......Q..?|
0000006f
```

# Benchmarks

### Hardware Details

| Type            | Value                                                                                                     |
| --------------- | --------------------------------------------------------------------------------------------------------- |
| Processor       | 11th Gen Intel(R) Core(TM) i9-11900KF @ 3.50GHz   3.50 GHz                                                |
| Installed RAM   | 32.0 GB (31.9 GB usable)                                                                                  |
| SSD             | [ADATA SX8200PNP](https://www.adata.com/upload/downloadfile/Datasheet_XPG%20SX8200%20Pro_EN_20181017.pdf) |
| OS              | Ubuntu 20.04 LTS running on WSL in Windows 11                                                             |

```console
foo@bar:~/dev/binary_log$  ./build/benchmark/binary_log_benchmark --benchmark_counters_tabular=true
2021-12-03T13:42:15-06:00
Running ./build/benchmark/binary_log_benchmark
Run on (16 X 3504 MHz CPU s)
Load Average: 0.52, 0.58, 0.59
-------------------------------------------------------------------------------------------------------
Benchmark                            Time             CPU   Iterations    Bytes/s    Latency     Logs/s
-------------------------------------------------------------------------------------------------------
BM_binary_log_static_string      0.678 ns        0.672 ns   1000000000 1.48837G/s  671.875ps 1.48837G/s
BM_binary_log_constants          0.676 ns        0.680 ns    896000000 1.47036G/s  680.106ps 1.47036G/s
BM_binary_log_integer             2.05 ns         2.04 ns    344615385  2.4506G/s  2.04032ns  490.12M/s
BM_binary_log_double              6.14 ns         6.14 ns    112000000 1.46618G/s  6.13839ns 162.909M/s
BM_binary_log_string              12.2 ns         12.2 ns     64000000 1.39264G/s   12.207ns   81.92M/s
```

# Implementation Notes

## Limitation 1: Cannot change the type of format args - Do not use in function template

`binary_log` save meta information for each unique line of code that uses `BINARY_LOG(...)` to the index file. That means, that each line of code that uses this macro is marked as having a particular format string, requiring a particular number of args, and requiring a particular type for each arg. This information is stored in the index file the very first time that line of code is executed.

Due to this, the type of format arguments should not change between invocations for that line of code.

Here's an example to illustrate this:

```cpp
1 int foo = 5;
2 BINARY_LOG(log, "{}", foo);
```

For the above line of code, `binary_log` updates the index table with static information: The format string is `"{}"` and the line of log requires 1 argument and it is expected to be an integer. The index file is updated once, the first time line #2 executes. Later, the unpacker will use this type information to correctly parse and deflate the log file (which is expected to have one argument of type `int32_t`)

Now, consider this second example that uses `BINARY_LOG` in a function template:

```cpp
1 template <typename T>
2 void save_data(binary_log::binary_log& log, T&& value) {
3   BINARY_LOG(log, "{}", std::forward<T>(value));
4 }
.
.
.
10 save_data(5);
11 save_data(3.14);
12 save_data(true);
```

When the above code runs, `binary_log` will update the index file once, the first time the line of code (line #3) is executed. This first time, the index file is updated with the information: format string `"{}"`, requires 1 argument, that argument is an `int32_t`. 

Unfortunately, this does not hold true here. The second time the log call is made, the argument is actually a `double`, which takes up 8 bytes in the log file (instead of the expected 4). 

Thus, in this case, the `unpacker` will fail to correctly parse the log file.

This design choice was made intentionally in order to keep the output of the log file as compact as possible. To fix this problem, the logger would have to store the type of the argument in the log file for each invocation, instead of once in the index file at the first invocation. Such a change would result in a large amount of redundant information and I have chosen to avoid it. Instead, this limitation is simply documented here.

# Building and installing

See the [BUILDING](BUILDING.md) document.

# Contributing

See the [CONTRIBUTING](CONTRIBUTING.md) document.

# License

The project is available under the [MIT](https://opensource.org/licenses/MIT) license.
