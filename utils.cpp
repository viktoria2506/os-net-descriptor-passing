#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <zconf.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <map>

const int STR_SIZE = 1024, EPOLL_SIZE = 1024;
const char name[] = "socket.soc";

int send_message(int sockd, char *message) {
    int message_len = strlen(message);
    unsigned char str_message_len[2];
    str_message_len[1] = message_len % 256;
    message_len /= 256;
    str_message_len[0] = message_len % 256;
    message_len = strlen(message);
    int sended_bytes = 0;
    while (true) {
        int first_byte_status = send(sockd, (char *) &str_message_len[0], 1, 0);
        if (first_byte_status < 0) {
            perror("send error");
            return -1;
        } else if (first_byte_status == 1) {
            break;
        }
    }
    while (true) {
        int second_byte_status = send(sockd, (char *) &str_message_len[1], 1, 0);
        if (second_byte_status < 0) {
            perror("send error");
            return -1;
        } else if (second_byte_status == 1) {
            break;
        }
    }
    while (sended_bytes < message_len) {
        int bytes = send(sockd, message + sended_bytes, message_len - sended_bytes, 0);
        if (bytes < 0) {
            perror("send error");
            return -1;
        } else {
            sended_bytes += bytes;
        }
    }
    return 1;
}

int get_message(int sockd, char *message) {
    int message_len = 0;
    unsigned char first_byte[1], second_byte[1];
    while (true) {
        int bytes = recv(sockd, first_byte, 1, 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            perror("recv error");
            return -1;
        } else if (bytes == 1) {
            break;
        }
    }
    while (true) {
        int bytes = recv(sockd, second_byte, 1, 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            perror("recv error");
            return -1;
        } else if (bytes == 1) {
            break;
        }
    }
    message_len = (int) first_byte[0] * 256 + (int) second_byte[0];
    int geted_bytes = 0;
    while (geted_bytes < message_len) {
        int bytes = recv(sockd, message + geted_bytes, message_len - geted_bytes, 0);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            perror("recv error");
            return -1;
        } else {
            geted_bytes += bytes;
        }
    }
    message[message_len] = '\0';
    return message_len;
}

ssize_t send_fd(int client, int fd) {
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    int *fdptr;
    char iobuf[1];
    struct iovec io;
    io.iov_base = iobuf;
    io.iov_len = sizeof(iobuf);
    union {
        char buf[CMSG_SPACE(sizeof(fd))];
        struct cmsghdr align;
    } u;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    fdptr = (int *) CMSG_DATA(cmsg);
    memcpy(fdptr, &fd, sizeof(fd));
    return sendmsg(client, &msg, 0);
}

int receive_fd(int server, int *fd) {
    static struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    char buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    if (recvmsg(server, &msg, 0) < 0) {
        perror("recvmsg error");
        return -1;
    }
    struct cmsghdr *cmsghdr1 = CMSG_FIRSTHDR(&msg);
    if (cmsghdr1 == nullptr || cmsghdr1->cmsg_type != SCM_RIGHTS) {
        printf("The first control structure contains no file descriptor\n");
        return -1;
    }
    memcpy(fd, CMSG_DATA(cmsghdr1), sizeof(int));
    return 0;
}

void print_help() {
    printf("1) enter file path to file when you want to open\n"
                   "2) enter text to file\n"
                   "3) if you want to close file write '<close>'\n");
}