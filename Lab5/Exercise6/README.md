>Exercise 6. Implement serve_write in fs/serv.c and devfile_write in lib/file.c.

Use make grade to test your code. Your code should pass "file_write", "file_read after file_write", "open", and "large file" for a score of 90/150.

# 代码

```
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
    // LAB 5: Your code here.
    struct OpenFile *o;
    int r;
    if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0) {
        return r;
    }
    int total = 0;
    while (1) {
        r = file_write(o->o_file, req->req_buf, req->req_n, o->o_fd->fd_offset);
        if (r < 0) return r;
        total += r;
        o->o_fd->fd_offset += r;
        if (req->req_n <= total)
            break;
    }
    return total;
}
```

函数 devfile_write 将参数填入结构 union Fsipc 中以方便 IPC 请求、调用fsipc并返回结果
```
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
    // Make an FSREQ_WRITE request to the file system server.  Be
    // careful: fsipcbuf.write.req_buf is only so large, but
    // remember that write is always allowed to write *fewer*
    // bytes than requested.
    // LAB 5: Your code here
    fsipcbuf.write.req_fileid = fd->fd_file.id;
    fsipcbuf.write.req_n = n;
    memmove(fsipcbuf.write.req_buf, buf, n);
    return fsipc(FSREQ_WRITE, NULL);
}
```
