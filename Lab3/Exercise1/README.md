>Exercise 1. Modify mem_init() in kern/pmap.c to allocate and map the envs array. This array consists of exactly NENV instances of the Env structure allocated much like how you allocated the pages array. Also like the pages array, the memory backing envs should also be mapped user read-only at UENVS (defined in inc/memlayout.h) so user processes can read from this array.

>You should run your code and make sure check_kern_pgdir() succeeds.

要求：在 mem_init() 中初始化一个类似于 pages[] 的环境数组 envs[]，并在页表中将其映射在 UENVS 部分。

# 代码

