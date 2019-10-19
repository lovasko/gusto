# gusto 
The `gusto` utility implements a datagram UNIX domain socket client. It allows
for request-response communication with an another process that exposes a
server UNIX domain socket.

## Build
The `gusto` utility can be built using the standard `Makefile` system as
follows:
```
$ make
$ ls -lah bin/gusto
```

## Example Usage
In order to display the help message, use the `-h` option:
```
$ bin/gusto -h
```

In order to initiate request-response communication with a server that exposes
a UNIX domain socket, use the positional argument:
```
$ bin/gusto /path/to/socket
```

## Alternatives
The two main alternatives are `socat` with the `UNIX-CLIENT` mode, and the
OpenBSD-extended `nc` with options `-u` and `-U`. The reasons for writing
`gusto` were threefold: education, minimalistic implementation and full control
over all aspects of the communication (i.e. all flags passes to system calls,
memory allocation).

## Standards
The `gusto` utility is written in the C99 language without any extensions. It
uses the POSIX.1-2008 interface to communicate with the kernel. Any environment
that supports there two standards should be able to compile and run the
utility. If this is not the case, feel free to contact the author.

## License
The `gusto` project is licensed under the terms of the
[2-cause BSD license](LICENSE).

## Author
Daniel Lovasko <daniel.lovasko@gmail.com>
