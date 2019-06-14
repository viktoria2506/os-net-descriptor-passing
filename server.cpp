#include "utils.cpp"

std::map<int, int> client_to_fd;

int main() {
    unlink(name);
    int server_sockd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un server;
    server.sun_family = AF_UNIX;
    memcpy(server.sun_path, name, sizeof(name));
    int yes = 1;
    if (setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        perror("setsockopt error");
        close(server_sockd);
        return 1;
    }
    if (bind(server_sockd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("bind error");
        return 1;
    }
    listen(server_sockd, 4);

    int epfd = epoll_create(EPOLL_SIZE);
    if (epfd < 0) {
        perror("epoll_create error");
        close(server_sockd);
        return 1;
    }
    static struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_sockd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_sockd, &event) < 0) {
        perror("epoll_ctl error");
        close(epfd);
        close(server_sockd);
        return 1;
    }
    bool f = true;
    while (f) {
        static struct epoll_event events[EPOLL_SIZE];
        int res = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        if (res < 0) {
            close(epfd);
            close(server_sockd);
            f = false;
        }
        for (int i = 0; i < res; i++) {
            if (events[i].data.fd == server_sockd) {
                int client_addr_length = sizeof(struct sockaddr);
                static struct sockaddr_in client;
                int client_sockd = accept(server_sockd, (struct sockaddr *) &client,
                                          (socklen_t * ) & client_addr_length);
                if (client_sockd == -1) {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        continue;
                    } else {
                        perror("accept error");
                    }
                } else {
                    static struct epoll_event client_event;
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    client_event.data.fd = client_sockd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_sockd, &client_event) < 0) {
                        perror("epoll_ctl error");
                    }
                }
            } else if (events[i].events == EPOLLIN) {
                char buf[STR_SIZE];
                int buf_size = get_message(events[i].data.fd, buf);
                if (buf_size < 0) {
                    printf("get_message error: can't get message\n");
                    close(events[i].data.fd);
                    continue;
                }
                printf("client id = %d want to open file = %s\n", events[i].data.fd, buf);
                int fd = open(buf, O_CREAT | O_WRONLY | O_RDONLY, S_IRWXU | S_IRWXG | S_IRWXO);
                if (fd < 0) {
                    perror("open");
                    char msg[] = "can't open file";
                    if (send_message(events[i].data.fd, msg) < 0) {
                        perror("send_message error");
                        continue;
                    }
                    continue;
                }
                char msg[] = "OK";
                if (send_message(events[i].data.fd, msg) < 0) {
                    perror("send_message error");
                    continue;
                }
                if (send_fd(events[i].data.fd, fd) < 0) {
                    printf("can't send fd to client = %d\n", events[i].data.fd);
                    continue;
                }
                client_to_fd[events[i].data.fd] = fd;
            } else {
                if (client_to_fd.count(events[i].data.fd) > 0) {
                    close(client_to_fd[events[i].data.fd]);
                }
                close(events[i].data.fd);
            }
        }
    }
    unlink(name);
    return 0;
}