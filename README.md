# Simple test framework for poll() and select()

While troubleshooting a curious issue on MacOS X, that was causing trouble for `Qemu`
and its userland networking stack, `slirp`, I found the attached utilities helpful.

## `source`

A simple Go-implemented TCP server which accepts connects and writes a fixed amount
of data to any connecting clients before closing. The server can optionally wait before
closing.

Command line options:

- `-b` set the amount of data to write, defaulting to 1024 bytes
- `-w` wait before closing (in seconds)


## `polltest`

A simple TCP client which connects to a remote destination, polling a socket for readability
and reading data as it becomes available.

Command line options:

- `-b` set the read buffer size, defaulting to 1024 bytes
- `-w` express interest in file descriptor writability (not normally required)
- `-s` set the SO_OOBINLINE socket-level option
- `-t` customise the poll/select loop timeout


## Observations

Both apps work in an unsurprisingly ordinary way on Linux, FreeBSD, OpenBSD, NetBSD.
Example output from Linux shows the expected `POLLIN` flag being returned allowing the
app to read from the descriptor. The end of the stream is indicated by reading zero
bytes (logged here as EOF).

```
adamc@xps:~$ ./polltest -P
Resolving localhost: trying 127.0.0.1... OK
FD 3 ready: POLLIN 
FD 3: Read 1024 byte(s)
FD 3 ready: POLLIN 
FD 3: EOF
Read 1024 total byte(s)

```

Things are slightly different on the Mac (10.14.6 Mojave). We get POLLIN as usual telling us we can read,
and we successfully complete that. But the next `poll()` returns us both `POLLPRI` and
`POLLHUP`.  `POLLHUP` suggests the remote endpoint has closed his end of the connection
and according to the man page `POLLPRI` means we have high priority data to read. 

There is no high-priority data to read however. It may simply be that the peer disconnecting
is a high-priority event that the user app should be aware of.

> Useful to note that on BSD and Mac, unlike Linux, there is no suggestion in the documentation
> that `poll()` returning `POLLPRI` for a file descriptor indicates the presence of TCP
> urgent/out-of-band data.

```
Adams-MacBook-Pro:~$ ./polltest -P 
Resolving localhost: trying ::... OK
FD 5 ready: POLLIN 
FD 5: Read 1024 byte(s)
FD 5 ready: POLLIN POLLPRI POLLHUP 
FD 5: EOF
Read 1024 total byte(s)
```

