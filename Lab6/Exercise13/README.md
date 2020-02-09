>Exercise 13. The web server is missing the code that deals with sending the contents of a file back to the client. Finish the web server by implementing send_file and send_data.

# 代码

```
static int
send_data(struct http_request *req, int fd)
{
	// LAB 6: Your code here.
	struct Stat stat;
	fstat(fd, &stat);
	void *buf = malloc(stat.st_size);
	//read from file
	if (readn(fd, buf, stat.st_size) != stat.st_size) {
		panic("Failed to read requested file");
	}

	//write to socket
  	if (write(req->sock, buf, stat.st_size) != stat.st_size) {
		panic("Failed to send bytes to client");
	}
	free(buf);
	return 0;
}
```

```
static int
send_file(struct http_request *req)
{
	int r;
	off_t file_size = -1;
	int fd;

	// open the requested url for reading
	// if the file does not exist, send a 404 error using send_error
	// if the file is a directory, send a 404 error using send_error
	// set file_size to the size of the file

	// LAB 6: Your code here.
	if ((fd = open(req->url, O_RDONLY)) < 0) {
		send_error(req, 404);
		goto end;
	}

	struct Stat stat;
	fstat(fd, &stat);
	if (stat.st_isdir) {
		send_error(req, 404);
		goto end;
	}

	if ((r = send_header(req, 200)) < 0)
		goto end;

	if ((r = send_size(req, file_size)) < 0)
		goto end;

	if ((r = send_content_type(req)) < 0)
		goto end;

	if ((r = send_header_fin(req)) < 0)
		goto end;

	r = send_data(req, fd);

end:
	close(fd);
	return r;
}
```