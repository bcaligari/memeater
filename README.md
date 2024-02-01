# memeater

MemEater 2.0 - (c) 2024 "Brendon Caligari" \<caligari.m0ptp@gmail.com\>

License: GPLv3

This is a tool written in my free time for my own use and enjoyment watching
systems break as memory is eaten up.

If someone came across this little project and finds a use for it I would
really appreciate an email.  It'll make my day.

## Usage

```
Usage: memeater [-i secs] [-b bytes] [-c count] [-f forks]
    -i secs       : interval sleep between malloc()s
    -b bytes      : bytes per malloc()
    -c count      : number of malloc()s, 0 for infinity
    -f forks      : number of program forks
    -w chillout   : seconds to sleep after last iteration
    -l            : mark faulted in pages as locked
```

Memeater will `malloc()`, then `memset()`, **-b** bytes every **-i** seconds up
to **-c** times. 

*Up to* because the system may kill `memeater` or crash before the final
iteration.

If **-f** is used then one or more memory eating forks are spawned while the
the parent process will just `wait()` on the forked processes.  Depending on the
use case even a single fork may be of use, or curiosity.  Forks have the process
renamed which may help when going through logs and dumps.

With **-l** faulted in pages are marked as locked.

### References

* `/proc/meminfo`
* `/proc/PID/smaps`

## Examples

### ulimited environment

* `$ ulimit -v $((256*1024))` - ulimit vm value is in kb

```
$ ./memeater -b $((1 << 26)) -c 5 -i 0
memeater: forks: 0; block size: 64 MiB; count: 5; sleep: 0
memeater: malloc() iteration 0
memeater: malloc() iteration 1
memeater: malloc() iteration 2
memeater: malloc() iteration 3
memeater: malloc() failed with 12 (Cannot allocate memory)
memeater: malloc() iteration 4
memeater: malloc() failed with 12 (Cannot allocate memory)
```

### kernel panic 

* `# sysctl -w vm.panic_on_oom=1`
* `$ ./memeater -c 0 -b $((1<<30))`

```
# find /var/crash
/var/crash
/var/crash/2024-01-07-20:01
/var/crash/2024-01-07-20:01/dmesg.txt
/var/crash/2024-01-07-20:01/vmcore
/var/crash/2024-01-07-20:01/README.txt
/var/crash/2024-01-07-20:01/System.map-5.14.21-150400.24.97-default
/var/crash/2024-01-07-20:01/vmlinux-5.14.21-150400.24.97-default.gz
```

```
[  185.133252] Kernel panic - not syncing: Out of memory: system-wide panic_on_oom is enabled 
[  185.138060] CPU: 1 PID: 2272 Comm: memeater Kdump: loaded Tainted: G               X    5.14.21-150400.24.97-default #1 SLE15-SP4 eb7f17f5a9cf6ac3de6aa7e2b8bbe83927b5cfd3
```

### oom_killed processes

* `# sysctl -w vm.panic_on_oom=0`
* `$ ./memeater -f 2 -c 0 -b $((1<<30))`

```
$ ./memeater -f 2 -c 0 -b $((1<<30))
memeater: forks: 2; block size: 1 GiB; count: 0; sleep: 5
memeater: forked 10521 ...
memeater: forked 10522 ...
memeater-10521: malloc() iteration 0
memeater-10522: malloc() iteration 0
memeater-10521: malloc() iteration 1
memeater-10522: malloc() iteration 1
memeater: reaped 10521 terminated by a signal 9 (Killed)
memeater-10522: malloc() iteration 2
memeater-10522: malloc() iteration 3
memeater: reaped 10522 terminated by a signal 9 (Killed)
memeater: no more children to wait()
```

```
...
2024-01-07T17:53:49.053424+00:00 testbox kernel: [ 9421.982946][T10521] memeater-10521 invoked oom-killer: gfp_mask=0x1100dca(GFP_HIGHUSER_MOVABLE|__GFP_ZERO), order=0, oom_score_adj=0
...
2024-01-07T17:53:49.053661+00:00 testbox kernel: [ 9422.457061][T10521] oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),cpuset=/,mems_allowed=0,global_oom,task_memcg=/,task=memeater-10521,pid=10521,uid=1000
2024-01-07T17:53:49.053661+00:00 testbox kernel: [ 9422.464959][T10521] Out of memory: Killed process 10521 (memeater-10521) total-vm:2099680kB, anon-rss:1646812kB, file-rss:0kB, shmem-rss:0kB, UID:1000 pgtables:3268kB oom_score_adj:0
2024-01-07T17:53:49.108848+00:00 testbox kernel: [ 9422.529890][   T32] oom_reaper: reaped process 10521 (memeater-10521), now anon-rss:0kB, file-rss:0kB, shmem-rss:0kB
...
...
2024-01-07T17:54:00.759884+00:00 testbox kernel: [ 9433.667228][T10522] memeater-10522 invoked oom-killer: gfp_mask=0x1100cca(GFP_HIGHUSER_MOVABLE), order=0, oom_score_adj=0
...
2024-01-07T17:54:00.760128+00:00 testbox kernel: [ 9434.163003][T10522] oom-kill:constraint=CONSTRAINT_NONE,nodemask=(null),cpuset=/,mems_allowed=0,global_oom,task_memcg=/,task=memeater-10522,pid=10522,uid=1000
2024-01-07T17:54:00.760129+00:00 testbox kernel: [ 9434.170950][T10522] Out of memory: Killed process 10522 (memeater-10522) total-vm:4196840kB, anon-rss:3266664kB, file-rss:0kB, shmem-rss:0kB, UID:1000 pgtables:6440kB oom_score_adj:0
2024-01-07T17:54:00.848837+00:00 testbox kernel: [ 9434.267015][   T32] oom_reaper: reaped process 10522 (memeater-10522), now anon-rss:0kB, file-rss:0kB, shmem-rss:0kB
```

## TODO

* Add Makefile
* Add Dockerfile
* Add sample Kubernetes manifest
* Add cumulative allocation
    * Collate forks' allocations total in parent process