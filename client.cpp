#include "utils.cpp"

int main() {
    int server_sockd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sockd < 0) {
        perror("socket error");
        return 1;
    }
    printf("Socked created\n");
    int epfd = epoll_create(EPOLL_SIZE);
    if (epfd < 0) {
        perror("epoll error");
        close(server_sockd);
        return 1;
    }
    static struct sockaddr_un server;
    server.sun_family = AF_UNIX;
    memcpy(server.sun_path, name, sizeof(name));
    bool conected = true;
    if (connect(server_sockd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        if (errno == EINPROGRESS) {
            conected = false;
        } else {
            perror("connect error");
            close(server_sockd);
            close(epfd);
            return 1;
        }
    }

    static struct epoll_event input_event;
    input_event.events = EPOLLIN;
    input_event.data.fd = STDIN_FILENO;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &input_event) < 0) {
        perror("epoll_ctl error");
        close(epfd);
        close(server_sockd);
        return 1;
    }
    static struct epoll_event server_event;
    if (!conected) {
        server_event.events = EPOLLIN | EPOLLOUT;
    } else {
        printf("connected to server\n");
        print_help();
        server_event.events = EPOLLIN;
    }
    server_event.data.fd = server_sockd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_sockd, &server_event) < 0) {
        perror("epoll_ctl error");
        close(server_sockd);
        close(epfd);
        return 1;
    }
    bool f = true;
    char to_server[STR_SIZE], from_server[STR_SIZE];
    int fd = -1;
    int step = 0;
    while (f) {
        static struct epoll_event events[EPOLL_SIZE];
        int res = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        for (int i = 0; i < res; i++) {
            if (events[i].data.fd == server_sockd && events[i].events & EPOLLOUT) {
                if (!conected) {
                    int len = sizeof(int);
                    int err = 0;
                    int q = getsockopt(server_sockd, SOL_SOCKET, SO_ERROR, &err, (socklen_t * )(&len));
                    if (q != 1) {
                        if (err == 0) {
                            printf("connected to server\n");
                            print_help();
                            static struct epoll_event event;
                            event.events = EPOLLIN;
                            event.data.fd = server_sockd;
                            if (epoll_ctl(epfd, EPOLL_CTL_MOD, server_sockd, &event) < 0) {
                                perror("epoll_ctl error");
                                f = false;
                                break;
                            }
                        } else {
                            perror("");
                            static struct epoll_event event;
                            event.events = 0;
                            event.data.fd = server_sockd;
                            epoll_ctl(epfd, EPOLL_CTL_DEL, server_sockd, &event);
                            close(server_sockd);
                            f = false;
                        }
                    }
                }
            } else if (events[i].data.fd == STDIN_FILENO) {
                ssize_t size1 = read(STDIN_FILENO, to_server, 1024);
                to_server[size1 - 1] = '\0';
                if (strcmp(to_server, "<close>") == 0) {
                    f = false;
                    break;
                }
                if (step == 0) {
                    int code = send_message(server_sockd, to_server);
                    if (code < 0) {
                        perror("send_message error");
                        f = false;
                        close(fd);
                        break;
                    }
                    step = 1;
                } else if (step == 3) {
                    if (write(fd, to_server, strlen(to_server)) < 0) {
                        perror("write");
                        close(fd);
                    }
                }
            } else if (events[i].data.fd == server_sockd) {
                if (step == 1) {
                    int code = get_message(server_sockd, from_server);
                    if (code < 0) {
                        perror("get_message error");
                        f = false;
                        break;
                    }
                    if (strcmp(from_server, "OK") != 0) {
                        printf("can't open file = %s\n", to_server);
                        f = false;
                        break;
                    } else {
                        printf("file open");
                    }
                    if (receive_fd(server_sockd, &fd) < 0) {
                        printf("can't get descriptor\n");
                        f = false;
                        break;
                    } else {
                        printf("get descriptor\n");
                    }
                    step = 3;
                }
            } else {
                close(events[i].data.fd);
                f = false;
            }
        }
    }
    close(server_sockd);
    close(epfd);
    return 0;
}