// Copyright (c) 2019 Daniel Lovasko
// All Rights Reserved
//
// Distributed under the terms of the 2-clause BSD License. The full
// license is in the file LICENSE, distributed as part of this software.

#include <sys/un.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>


// Convert the value of a #define macro into a C string.
#define _STRINGIFY(str) #str
#define STRINGIFY(str) _STRINGIFY(str)

// Size of the message buffer.
#define BUFFER_SIZE 768

/// Handle outgoing data from the standard input.
/// @return success/failure indication
///
/// @param[in] sock UNIX socket
/// @param[in] addr target address
static bool
handle_outgoing(int sock, struct sockaddr_un* addr)
{
  ssize_t retss;
  char data[BUFFER_SIZE];
  size_t len;
  char* rets;
  int reti;

  // Read the message from the standard input stream.
  (void)memset(data, 0, sizeof(data));
  rets = fgets(data, sizeof(data), stdin);
  if (rets == NULL) {
    reti = feof(stdin);
    if (reti != 0) {
      return false;
    }

    perror("fgets");
    return true;
  }

  // Send the message to the socket.
  len = strlen(data);
  retss = sendto(sock, data, len, 0, (struct sockaddr*)addr, sizeof(*addr));

  // Check for errors.
  if (retss == -1) {
    perror("sendto");
    return false;
  }

  // Check whether all data was sent.
  if (retss != (ssize_t)len) {
    (void)fprintf(stderr, "sendmsg: Did not send all data\n");
    return false;
  }

  return true;
}

/// Handle incoming data on the socket.
/// @return success/failure indication
///
/// @param[in] sock UNIX datagram socket
static bool
handle_incoming(int sock)
{
  ssize_t retss;
  char data[BUFFER_SIZE];

  // Receive the message from the socket.
  (void)memset(&data, 0, sizeof(data));
  retss = recvfrom(sock, data, sizeof(data) - 1, 0, NULL, NULL);
  
  // Check for potential errors.
  if (retss == -1) {
    perror("recvfrom");
    return false;
  }

  // Display the message on the standard output stream.
  (void)write(STDOUT_FILENO, data, (size_t)retss);
  (void)write(STDOUT_FILENO, "\n", 1);

  return true;
}

/// Continuously await and relay messages from the UNIX socket.
/// @return success/failure indication
///
/// @param[in] sock UNIX socket
/// @param[in] path path to target socket
static bool
event_loop(int sock, char* path)
{
  fd_set rfds;
  int reti;
  struct sockaddr_un addr;
  bool retb;

  // Prepare the target address.
  (void)memset(&addr, 0, sizeof(addr));
  (void)strncpy(addr.sun_path, path, sizeof(addr.sun_path));
  addr.sun_family = AF_UNIX;

  while (true) {
    // Prepare the event set.
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(sock, &rfds);

    // Wait for events.
    reti = select(sock + 1, &rfds, NULL, NULL, NULL);
    if (reti == -1) {
      perror("select");
      return false;
    }

    // Check whether there is incoming data on the socket.
    reti = FD_ISSET(sock, &rfds);
    if (reti > 0) {
      retb = handle_incoming(sock);
      if (retb == false) {
        break;
      }
    }

    // Check whether there is incoming data in the standard input stream.
    reti = FD_ISSET(STDIN_FILENO, &rfds);
    if (reti > 0) {
      retb = handle_outgoing(sock, &addr);
      if (retb == false) {
        break;
      }
    }
  }

  return true;
}

/// Initialize the UNIX domain socket.
/// @return success/failure indication
///
/// @param[out] sock UNIX socket
/// @param[out] tmp  temporary directory path
static bool
create_socket(int* sock, char* tmp)
{
  char path[64];
  struct sockaddr_un addr;
  int reti;
  size_t len;
  char* rets;

	// Get the current working directory.
	rets = getcwd(tmp, 512);
	if (rets == NULL) {
	  perror("getcwd");
		return false;
	}

	// Append the name template.
	len = strlen(tmp);
	(void)strncat(tmp, "/gusto.XXXXXX", len - 13);

  // Create a temporary directory.
  rets = mkdtemp(tmp);
  if (rets == NULL) {
    perror("mkdtemp");
    return false;
  }

  // Create a path to the UNIX domain socket in that directory.
  len = strlen(tmp); 
  (void)memset(path, 0, sizeof(path));
  (void)strncpy(path, tmp, len);
  (void)strncat(path, "/socket", sizeof(path) - len - 1);

  // Create a UNIX domain datagram socket.
  *sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (*sock == -1) {
    perror("socket");
    return false;
  }

  // Prepare the socket address.
  (void)memset(&addr, 0, sizeof(addr));
  (void)strncpy(addr.sun_path, path, sizeof(addr.sun_path));
  addr.sun_family = AF_UNIX;

  // Bind the socket to the address.
  reti = bind(*sock, (struct sockaddr*)&addr, sizeof(addr));
  if (reti == -1) {
    perror("bind");
    return false;
  }

  return true;
}

/// Finalize the socket structure.
/// @return success/failure indication
///
/// @param[in] sock UNIX socket
/// @param[in] tmp  temporary directory path
static bool
delete_socket(int sock, char* tmp)
{
  int reti;
  size_t len;
  char path[64];

  // Close the socket.
  reti = close(sock);
  if (reti == -1) {
    perror("close");
    return false;
  }

  // Create a path to the UNIX domain socket in that directory.
  len = strlen(tmp); 
  (void)memset(path, 0, sizeof(path));
  (void)strncpy(path, tmp, len);
  (void)strncat(path, "/socket", sizeof(path) - len - 1);

  // Remove the socket file.
  reti = unlink(path);
  if (reti == -1) {
    perror("unlink");
    return false;
  }

  // Remove the temporary directory.
  reti = rmdir(tmp);
  if (reti == -1) {
    perror("rmdir");
    return false;
  }

  return true;
}

/// Retrieve the program configuration from the arguments.
/// @return success/failure indication
///
/// @param[out] path socket path
/// @param[in]  argc argument count
/// @param[in]  argv argument vector
static bool
parse_arguments(char** path, int argc, char* argv[])
{
  int reti;

  // Ensure that we receive exactly one argument: the target socket.
  if (argc != 2) {
    (void)fprintf(stderr, "Expecting exactly one argument: socket path\n"
		          "Use option `-h` to display help message.\n");
    return false;
  }

  // Check for a help flag.
  reti = strncmp(argv[1], "-h", 3);
  if (reti == 0) {
    (void)fprintf(stderr,
       "gusto - generic datagram UNIX domain socket client\n"
       "\nUsage:\n"
       "  gusto [-h] sock\n"
       "\nOptions:\n"
       "  -h     Display help message.\n"
       "\nArguments:\n"
       "  sock   Path to a datagram UNIX domain socket.\n"
       "\nDetails:\n"
       "  Compiler: "             STRINGIFY(GUSTO_C99) "\n"
       "  C Standard: "           STRINGIFY(GUSTO_STD) "\n"
       "  Feature Test Macros: "  STRINGIFY(GUSTO_FTM) "\n"
       "  Optimizations: "        STRINGIFY(GUSTO_OPT) "\n"
       "  Linking: "              STRINGIFY(GUSTO_LNK) "\n"
       "  Build Architecture: "   STRINGIFY(GUSTO_HWT) "\n"
       "  Build Author: "         STRINGIFY(GUSTO_USR) "\n"
       "  Source Code Length: "   STRINGIFY(GUSTO_LEN) " bytes\n"
       "  Source Code Checksum: " STRINGIFY(GUSTO_CRC) "\n");

       return false;
  }

  *path = argv[1];
  return true;
}

/// Block all incoming signals.
/// @return success/failure indication
static bool
block_all_signals(void)
{
  sigset_t set;
  int reti;

  // Ensure all signals are in the set.
  (void)sigfillset(&set);

  // Block all signals.
  reti = sigprocmask(SIG_BLOCK, &set, NULL);
  if (reti == -1) {
    perror("sigprocmask");
    return false;
  }

  return true;
}

/// Generic UNIX Socket Transmitter for Operators.
int
main(int argc, char* argv[])
{
  int sock;                   // Local socket.
  int retb;                   // Boolean return value.
  char* srv;                  // Service socket path.
  char tmp[1024]; // Temporary directory path.

  retb = parse_arguments(&srv, argc, argv);
  if (retb == false) {
    return EXIT_FAILURE;
  }

  retb = block_all_signals();
  if (retb == false) {
    return EXIT_FAILURE;
  }

  retb = create_socket(&sock, tmp);
  if (retb == false) {
    return EXIT_FAILURE;
  }

  retb = event_loop(sock, srv);
  if (retb == false) {
    return EXIT_FAILURE;
  }

  retb = delete_socket(sock, tmp);
  if (retb == false) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
