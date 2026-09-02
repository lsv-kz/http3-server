#include "http3_server.h"

const unsigned int array_reserve = 32;
//======================================================================
int main()
{
    if (read_conf_file("server.conf"))
        return 1;
    std::cout <<  " ServerAddr: " << conf->ServerAddr
        << "\n ServerPort: " << conf->ServerPort
        << "\n ServerSoftware: " << conf->ServerSoftware
        << "\n DocumentRoot: " << conf->DocumentRoot
        << "\n ScriptDir: " << conf->ScriptDir
        << "\n UsePHP: " << conf->UsePHP
        << "\n PathPHP: " << conf->PathPHP
        << "\n LogDir: " << conf->LogDir
        << "\n ServerNameIndication: " << conf->ServerNameIndication
        << "\n MaxAcceptConnections: " << conf->MaxAcceptConnections
        << "\n MaxStreams: " << conf->MaxStreams
        << "\n TimeOut: " << conf->TimeOut
        << "\n MaxCgiProc: " << conf->MaxCgiProc
        << "\n TimeoutCGI: " << conf->TimeoutCGI
        << "\n ShowMediaFiles: " << conf->ShowMediaFiles << "\n";

    for ( int i = 0; environ[i]; )
    {
        char *p, buf[512];
        if ((p = (char*)memccpy(buf, environ[i], '=', strlen(environ[i]))))
        {
            if (strstr(environ[i], "DISPLAY") ||
                strstr(environ[i], "XDG_RUNTIME_DIR")// ||
                //strstr(environ[i], "HOME") ||
                //strstr(environ[i], "SESSION_MANAGER") ||
                //strstr(environ[i], "PATH")
             )
            {
                i++;
                continue;
            }

            *(p - 1) = 0;
            unsetenv(buf);
        }
    }

    BIO *bio = NULL;
    SSL *quic_listener = NULL;

    SSL_CTX *ctx = InitCTX();
    if (ctx == NULL)
    {
        return 1;
    }

    int server_fd = create_server_socket(conf->ServerPort.c_str());
    if (server_fd < 0)
    {
        return 1;
    }

    create_logfiles(conf->LogDir);

    bio = BIO_new_dgram(server_fd, BIO_CLOSE);
    if (!bio)
    {
        fprintf(stderr, "<%s:%d> Error BIO_new_dgram()\n", __func__, __LINE__);
        return 1;
    }

    quic_listener = SSL_new_listener(ctx, 0);
    if (!quic_listener)
    {
        fprintf(stderr, "<%s:%d> Error SSL_new_listener()\n", __func__, __LINE__);
        return 1;
    }

    SSL_set_bio(quic_listener, bio, bio);

    if (!SSL_set_blocking_mode(quic_listener, 0))
    {
        fprintf(stderr, "<%s:%d> Error SSL_set_blocking_mode()\n", __func__, __LINE__);
        return 1;
    }

    if (SSL_listen(quic_listener) <= 0)
    {
        fprintf(stderr, "<%s:%d> Error SSL_listen()\n", __func__, __LINE__);
        ERR_print_errors_fp(stderr);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    printf("HTTP/3 server run port: %s. Waiting connect from clients ...\n", conf->ServerPort.c_str());

    Server server;
    server.event_loop(quic_listener, server_fd);
    server.close_connections();

    SSL_free(quic_listener);
    SSL_CTX_free(ctx);
    close(server_fd);

    return 0;
}
