# YADL
Mutual Exclusion algorithm Yang-Anderson Doorway Lock (also called YAMCS) implemented in pthreads and MPI

Repository YAL contains the base Yang-Anderson Lock (YAL) algorithm's implementation.

This is a pthread- and MPI- based implementation of a variant of the original YAL for Mutual Exclusion (https://www.cs.unc.edu/~anderson/papers/dc95.pdf).

This algorithm is called Yang-Anderson Doorway Lock (YADL) or YAMCS in the initial proposal (MS Abdulla, "MCS-like Algorithms for Efficient Mutual Exclusion in Cloud and Multi-Core settings", Advances in Cloud Computing, Bangalore, July 2012. Slides provided in repository)

The key step in YADL/YAMCS is to make the process which wins contention in the YAL-tree, also the creator of a linked list of processes which are contending for various locks (at various levels) in the rest of the YAL-tree.

This linked-list of processes spinning on a local variable called Freed_YA, each being woken up when their predecessor finishes, is like Mellor-Crummey-Scott (MCS) mutual exclusion algorithm.

However, unlike MCS, complex atomic instructions like Fetch-and-Store or Compare-and-Swap are avoided.

Analysis shows that for high contention, i.e. when all N processes are contending at all time, the remote-accesses required per process is O(1).

Preparation of cluster and compilation are as in the YAL repository.

Sample output:

root@master:/home/ubuntu# /usr/bin/mpicc -lpthread YADL*.c -o YADL -lm

root@master:/home/ubuntu# for i in {2..30..2}; do /usr/bin/mpirun -host master,node001,node002,node003,node004,node005,node006,node007 ./YADL $i; done

In 2s, each node enters C-S average 1.88x, 1.51s delay (max 2.07s) RAs: 58.4, MCS-length:1.88, MCS-Qs: 3.0, Scanned locks: 2.25

In 4s, each node enters C-S average 2.50x, 2.12s delay (max 4.24s) RAs: 75.9, MCS-length:2.50, MCS-Qs: 4.0, Scanned locks: 3.00

In 6s, each node enters C-S average 3.25x, 2.54s delay (max 4.56s) RAs: 100.6, MCS-length:3.25, MCS-Qs: 5.0, Scanned locks: 3.75

In 8s, each node enters C-S average 4.00x, 2.28s delay (max 5.02s) RAs: 130.1, MCS-length:4.00, MCS-Qs: 6.0, Scanned locks: 4.62

In 10s, each node enters C-S average 5.38x, 2.04s delay (max 4.52s) RAs: 164.6, MCS-length:5.38, MCS-Qs: 7.0, Scanned locks: 5.75

In 12s, each node enters C-S average 5.75x, 2.25s delay (max 4.85s) RAs: 182.8, MCS-length:4.50, MCS-Qs: 8.0, Scanned locks: 6.25

In 14s, each node enters C-S average 6.50x, 2.28s delay (max 5.53s) RAs: 201.2, MCS-length:5.56, MCS-Qs: 8.0, Scanned locks: 6.62

In 16s, each node enters C-S average 7.38x, 2.37s delay (max 5.66s) RAs: 229.0, MCS-length:5.88, MCS-Qs: 9.0, Scanned locks: 7.50

In 18s, each node enters C-S average 7.88x, 2.48s delay (max 5.57s) RAs: 241.6, MCS-length:6.94, MCS-Qs: 9.0, Scanned locks: 7.62

In 20s, each node enters C-S average 9.25x, 2.28s delay (max 5.80s) RAs: 291.6, MCS-length:6.06, MCS-Qs: 12.0, Scanned locks: 9.75

In 22s, each node enters C-S average 10.00x, 2.37s delay (max 5.42s) RAs: 310.4, MCS-length:6.25, MCS-Qs: 13.0, Scanned locks: 10.62

In 24s, each node enters C-S average 9.50x, 2.66s delay (max 6.33s) RAs: 300.6, MCS-length:5.88, MCS-Qs: 13.0, Scanned locks: 10.38

In 26s, each node enters C-S average 11.00x, 2.52s delay (max 6.08s) RAs: 341.4, MCS-length:7.38, MCS-Qs: 12.0, Scanned locks: 10.25

In 28s, each node enters C-S average 12.25x, 2.39s delay (max 5.84s) RAs: 379.1, MCS-length:6.12, MCS-Qs: 16.0, Scanned locks: 12.88

In 30s, each node enters C-S average 14.50x, 2.13s delay (max 5.29s) RAs: 456.2, MCS-length:6.06, MCS-Qs: 19.0, Scanned locks: 15.25

