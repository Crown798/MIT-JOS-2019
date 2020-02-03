>Exercise 1. i386_init identifies the file system environment by passing the type ENV_TYPE_FS to your environment creation function, env_create. Modify env_create in env.c, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.

Make sure you can start the file environment without causing a General Protection fault. You should pass the "fs i/o" test in make grade.

# 代码

```
    if (type == ENV_TYPE_FS) {
        e->env_tf.tf_eflags |= FL_IOPL_MASK;
    }
```