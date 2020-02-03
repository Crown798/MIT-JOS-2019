>Exercise 10. The shell doesn't support I/O redirection. It would be nice to run sh <script instead of having to type in all the commands in the script by hand, as you did above. Add I/O redirection for < to user/sh.c.

Test your implementation by typing sh <script into your shell

Run make run-testshell to test your shell. testshell simply feeds the above commands (also found in fs/testshell.sh) into the shell and then checks that the output matches fs/testshell.key.

# 代码

```
runcmd(char* s) {
            ...
            // Open 't' for reading as file descriptor 0
			// (which environments use as standard input).
			// We can't open a file onto a particular descriptor,
			// so open the file as 'fd',
			// then check whether 'fd' is 0.
			// If not, dup 'fd' onto file descriptor 0,
			// then close the original 'fd'.
            
            if ((fd = open(t, O_RDONLY)) < 0) {          // 打开文件
                cprintf("open %s for write: %e", t, fd);
                exit();
            }
            if (fd != 0) {               
                dup(fd, 0);				// 复制文件描述符到标准输入
                close(fd);
            }
            ...
}
```
