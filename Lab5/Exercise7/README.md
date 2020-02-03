>Exercise 7. spawn relies on the new syscall sys_env_set_trapframe to initialize the state of the newly created environment. Implement sys_env_set_trapframe in kern/syscall.c (don't forget to dispatch the new system call in syscall()).

Test your code by running the user/spawnhello program from kern/init.c, which will attempt to spawn /hello from the file system.

Use make grade to test your code.

# 代码

```
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
    // LAB 5: Your code here.
    // Remember to check whether the user has supplied us with a good
    // address!
    int r;
    struct Env *e;
    if ((r = envid2env(envid, &e, 1)) < 0) {
        return r;
    }
    tf->tf_eflags = FL_IF;
    tf->tf_eflags &= ~FL_IOPL_MASK;         //普通进程不能有IO权限
    tf->tf_cs = GD_UT | 3;
    e->env_tf = *tf;
    return 0;
}
```