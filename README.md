# Binary Compatible Critical Section Delegation

In high-performance applications, critical sections often be come performance bottlenecks due to contention among mul tiple cores. Critical section delegation mitigates this over head by consistently executing critical sections on the same core, thereby reducing contention. Traditional delegation tech niques, however, typically require manual code refactoring, which limits their practicality and adoption. More recent schemes that integrate with existing lock APIs have lowered this barrier, but delegating unsupported critical sections can still cause undefined behavior, including crashes.

To make delegation broadly accessible—even for end-users without access to or knowledge of source code, we introduce BCD, a binary-compatible delegation mechanism. BCD lever ages kernel support to delegate userspace critical sections at the point of futex_wake. It employs an in-kernel virtual machine to supervise the execution of delegated userspace instructions, eliminating the need for binary modification. Crucially, BCD can detect and safely exclude unsupported critical sections, preserving correctness. This enables exist ing applications to transparently benefit from critical section delegation, without requiring developer intervention.

author: Junyao Zhang, Zhuo Wang, Zhe Zhou

The following is the artifact evaluation guide

## Configuration

1. Software configuration
   - Ubuntu [20.04.2](https://old-releases.ubuntu.com/releases/20.04.2/ubuntu-20.04.2-desktop-amd64.iso) with Kernel version 6.4.1
   - python 3.8.10 & module: [miasm v0.1.3](https://github.com/cea-sec/miasm/releases/tag/v0.1.3)
   - gcc 9.4.0
   - leveldb 1.23
   - memcached 1.6.32
   - memtier_benchmark 2.1.1
   - parsec-benchmark 3.0
2. Hardware configuration
   - cpu: Intel(R) Xeon(R) Gold 6438N * 2, each has 32 physical cores.
   - memory: SK Hynix 16GB DDR5-5600 * 2
   - hyper-threading disabled

## Building

1. `sudo apt-get update`
2. `sudo apt-get install build-essential libncurses-dev bison flex libssl-dev libelf-dev coreutils numactl libpapi-dev dwarves zstd` 
3. Enter the **linux-6.4.1** directory ， compile and install this kernel:
   - `make menuconfig`
   - `make -j $(nproc)`
   - `sudo make modules_install -j $(nproc)`
   - `sudo make install`
   - Reboot and switch to the new kernel.
4. Enter the **kernel_module** directory , compile it using the `make` command and execute `env.sh` .
5. Enter the **daemon** directory , compile it using the `make` command.
6. Enter the `TCLocks/src/userspace/litl` directory , compile it using the `make` command.
7. `sudo apt-get install python3-distutils python3-dev python3-pip msttcorefonts`
8. `sudo pip install future pyparsing==2.4.7`
9. `pip install numpy matplotlib`
10. install python module [miasm v0.1.3](https://github.com/cea-sec/miasm/releases/tag/v0.1.3). Please install the following libraries and modules before installation:

## Testing

### Micro-benchmark (Paper Section 4.2)

1. Enter the **benchmark/micro_benchmark** directory.
2. Executing the script `test_thread_1.sh` yields Figure 5a.
3. Executing the script `test_thread_2.sh` yields Figure 5b.
4. Executing the script `test_thread_4.sh` yields Figure 5c.
5. Executing the script `test_latency_90.sh` yields Figure 5d.
6. Executing the script `test_latency_99.sh` yields Figure 5e.
7. Executing the script `test_ncs.sh` yields Figure 5f.
8. Executing the script `test_cache_array.sh` yields Figure 5g.
9. Executing the script `test_cache_list.sh` yields Figure 5h.
10. Executing the script `test_rwlock.sh` yields Figure 5i.

### Effect of Optimization (Paper Section 4.3)

1. Enter the **benchmark/micro_benchmark** directory.
2. Executing the script `test_opt.sh` yields Figure 5j.

### Apps Test (Paper Section 4.4)

#### Splash2x benchmark

1. Enter the **benchmark/parsec-benchmark** directory.
2. `./bin/parsecmgmt -a build -p splash2x`
3. Execute the scripts: `test_raytrace_teapot.sh` ,`test_raytrace_balls4.sh` and ` test_raytrace_car.sh` , to test the Raytrace app.
4. Execute the script `test_radiosity.sh` to test the Radiosity app.
5. Execute the script `test_volrend.sh` to test the Volrend app.
6. Execute the script `test_ocean_cp.sh` to test the Ocean_cp app.

#### Memcached Test (Using Memtier_benchmark)

1. Enter the **benchmark/memcached-1.6.32** directory.
2. `sudo apt-get install autotools-dev automake libevent-dev`
3. `./configure`
4. `make`
5. Enter the **benchmark/memtier_benchmark-2.1.1** directory.
6. `sudo apt-get install build-essential autoconf automake libevent-dev pkg-config zlib1g-dev libssl-dev libpcre3-dev`
7. `autoreconf -ivf`
8. `./configure`
9. `make`
10. Executing the script `test_memcached.sh` yields Figure 5k.

#### LevelDB Test (Using Readrandom Benchmark)

1. Enter the **benchmark/leveldb-1.23** directory.
2. `sudo apt-get install cmake`
3. `mkdir build && cd build`
4. `cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . -j $(nproc)`
5. `cp ../draw.py ../get_ops.py ../test_leveldb.sh .`
6. Executing the script `test_leveldb.sh` yields Figure 5l.