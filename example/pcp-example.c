#ifndef WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <pcpnatpmp.h>
#include <stdio.h>
#include <string.h>

int main(void) {
#ifdef WIN32
    fprintf(stderr, "This example is intended for POSIX-like systems.\n");
    return 1;
#else
    struct sockaddr_storage src;
    struct sockaddr_storage dst;
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    struct addrinfo *ai;
    int s = -1;
    socklen_t src_len = sizeof(src);
    pcp_flow_t *f;
    pcp_ctx_t *ctx;
    char buff[1024] = {0};
    const char req[] = "GET / HTTP/1.0\r\nHost: www.example.com\r\n\r\n";

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    if (getaddrinfo("www.example.com", "80", &hints, &results) != 0) {
        perror("getaddrinfo");
        return 1;
    }
    for (ai = results; ai != NULL; ai = ai->ai_next) {
        s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s < 0) {
            continue;
        }
        if (connect(s, ai->ai_addr, ai->ai_addrlen) == 0) {
            memcpy(&dst, ai->ai_addr, ai->ai_addrlen);
            break;
        }
        close(s);
        s = -1;
    }
    freeaddrinfo(results);
    if (s < 0) {
        perror("connect");
        return 1;
    }
    if (getsockname(s, (struct sockaddr *)&src, &src_len) != 0) {
        perror("getsockname");
        close(s);
        return 1;
    }

    ctx = pcp_init(ENABLE_AUTODISCOVERY, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "pcp_init failed\n");
        close(s);
        return 1;
    }

    f = pcp_new_flow(ctx, (struct sockaddr *)&src, (struct sockaddr *)&dst,
                     NULL, IPPROTO_TCP, 60, NULL);
    if (f == NULL) {
        fprintf(stderr, "pcp_new_flow failed\n");
        pcp_terminate(ctx, 0);
        close(s);
        return 1;
    }
    pcp_wait(f, 500, 0);

    if (send(s, req, sizeof(req) - 1, 0) < 0) {
        perror("send");
    }
    if (recv(s, buff, sizeof(buff) - 1, 0) < 0) {
        perror("recv");
    }
    printf("RESULT: %s\n", buff);

    close(s);
    pcp_close_flow(f);
    pcp_delete_flow(f);

    pcp_terminate(ctx, 0);
    return 0;
#endif
}
