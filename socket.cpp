#include "http3_server.h"

//======================================================================
int create_server_socket(const char *port)
{
    struct sockaddr_in addr;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(port));

    int sock_opt = 1;
    ioctl(sock, FIONBIO, &sock_opt);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        fprintf(stderr, "<%s:%d> Error bind(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    return sock;
}
//======================================================================
int create_cgi_socket(const char *script_path)
{
    int sockfd, n;
    char addr[256];
    char port[16];

    n = sscanf(script_path, "%[^:]:%s", addr, port);
    if (n == 2) //==== AF_INET ====
    {
        struct sockaddr_in sock_addr;
        memset(&sock_addr, 0, sizeof(sock_addr));

        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == -1)
        {
            print_err("<%s:%d> Error socket(): %s\n", __func__, __LINE__, strerror(errno));
            return -1;
        }

        const int sock_opt = 1;
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *)&sock_opt, sizeof(sock_opt)))
        {
            print_err("<%s:%d> Error setsockopt(TCP_NODELAY): %s\n", __func__, __LINE__, strerror(errno));
            close(sockfd);
            return -1;
        }

        sock_addr.sin_port = htons(atoi(port));
        sock_addr.sin_family = AF_INET;
        if (inet_aton(addr, &(sock_addr.sin_addr)) == 0)
//      if (inet_pton(AF_INET, addr, &(sock_addr.sin_addr)) < 1)
        {
            print_err("<%s:%d> Error inet_pton(%s): %s\n", __func__, __LINE__, addr, strerror(errno));
            close(sockfd);
            return -1;
        }

        int flags = 1;
        if (ioctl(sockfd, FIONBIO, &flags) == -1)
        {
            print_err("<%s:%d> Error ioctl(FIONBIO, 1): %s\n", __func__, __LINE__, strerror(errno));
            return -1;
        }

        if (connect(sockfd, (struct sockaddr *)(&sock_addr), sizeof(sock_addr)) != 0)
        {
            if (errno != EINPROGRESS)
            {
                print_err("<%s:%d> Error connect(%s): %s\n", __func__, __LINE__, script_path, strerror(errno));
                close(sockfd);
                return -1;
            }
            else
                errno = 0;
        }
    }
    else //==== PF_UNIX ====
    {
        struct sockaddr_un sock_addr;
        sockfd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1)
        {
            print_err("<%s:%d> Error socket(): %s\n", __func__, __LINE__, strerror(errno));
            return -1;
        }

        sock_addr.sun_family = AF_UNIX;
        snprintf(sock_addr.sun_path, sizeof(sock_addr.sun_path), "%s", script_path); // resp->cgi.socket->c_str()

        int flags = fcntl(sockfd, F_GETFL);
        if (flags == -1)
        {
            print_err("<%s:%d> Error fcntl(, F_GETFL, ): %s\n", __func__, __LINE__, strerror(errno));
            return -1;
        }
        else
        {
            flags |= O_NONBLOCK;
            if (fcntl(sockfd, F_SETFL, flags) == -1)
            {
                print_err("<%s:%d> Error fcntl(, F_SETFL, ): %s\n", __func__, __LINE__, strerror(errno));
                return -1;
            }
        }

        if (connect(sockfd, (struct sockaddr *) &sock_addr, SUN_LEN(&sock_addr)) == -1)
        {
            if (errno != EINPROGRESS)
            {
                print_err("<%s:%d> Error connect(%s): %s\n", __func__, __LINE__, script_path, strerror(errno));
                close(sockfd);
                return -1;
            }
            else
                errno = 0;
        }
    }

    int flags = fcntl(sockfd, F_GETFD);
    if (flags == -1)
    {
        print_err("<%s:%d> Error fcntl(F_GETFD): %s\n", __func__, __LINE__, strerror(errno));
        close(sockfd);
        return -1;
    }

    flags |= FD_CLOEXEC;
    if (fcntl(sockfd, F_SETFD, flags) == -1)
    {
        print_err("<%s:%d> Error fcntl(F_SETFD, FD_CLOEXEC): %s\n", __func__, __LINE__, strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}
