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
        // 버퍼에서 1글자씩 읽기
        n = read(fd, &current, 1);

        if (n < 0 && errno == EINTR)
            continue;

        // 연결 종료 or 오류 or 타임아웃
        if (n <= 0 || buffer_idx >= size - 1)
            break;

        if (current == '\n')
        {
            if (buffer_idx > 0 && buf[buffer_idx - 1] == '\r')
                buffer_idx--;

            buf[buffer_idx] = '\0';
            return 1;
        }

        buf[buffer_idx++] = current;
    }

    buf[buffer_idx] = '\0';
    return 0;
}

void send_ok(int fd)
{
    send(fd, HANDSHAKE, strlen(HANDSHAKE), MSG_NOSIGNAL);
}