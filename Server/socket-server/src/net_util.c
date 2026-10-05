#include "net_util.h"
#include "constants.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

int read_line(int fd, char *buf, size_t size)
{
    char current = '\0';
    int buffer_idx = 0;
    ssize_t n = 0;


    while (1)
    {
        n = read(fd, &current, 1);  // -1 or 0 or 1

        if (n == 0) return 0;
        else if (n < 0) return -1;


        if (current == '\n') break;

        buf[buffer_idx++] = current;
    }

    if (buf[buffer_idx - 1] == '\r')
    {
        buf[buffer_idx - 1] = '\0';
    }

    buf[buffer_idx] = '\0';
    return 1;
}

void send_ok(int fd)
{
    send(fd, HANDSHAKE, strlen(HANDSHAKE), MSG_NOSIGNAL);
}