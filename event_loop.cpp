#include "http3_server.h"

using namespace std;

//======================================================================
void Server::add_to_list(Connect *c)
{
    c->next = NULL;
    c->prev = list_end;
    if (list_end)
        list_end->next = c;
    list_end = c;
    if (!list_start)
        list_start = c;
    ++num_accept_conn;
}
//======================================================================
void Server::close_connect(Connect *c)
{
    if (c->prev)
        c->prev->next = c->next;
    else
        list_start = c->next;

    if (c->next)
        c->next->prev = c->prev;
    else
        list_end = c->prev;
    unsigned int num_conn = c->num_conn;
    delete c;
    --num_accept_conn;
    string close_time = log_time();
    fprintf(stderr, "[%s] ********** close connect %u ***********\n", close_time.c_str(), num_conn);
    fprintf(stdout, "[%s] ********** close connect %u ***********\n", close_time.c_str(), num_conn);
}
//======================================================================
void Server::close_stream(Connect *c, Stream *s)
{
    if (s->cgi.pid)
    {
        --all_cgi;
        fprintf(stderr, "<%s:%u> all_cgi=%d\n", __func__, s->num_stream, all_cgi);
    }

    c->delete_stream(s);
}
//======================================================================
int get_poll_timeout(SSL *quic_listener)
{
    struct timeval tv;
    int is_infinite;

    if (!SSL_get_event_timeout(quic_listener, &tv, &is_infinite))
    {
        fprintf(stderr, "<%s:%d> Error SSL_get_event_timeout()\n", __func__, __LINE__);
        return -1;
    }

    if (is_infinite)
    {
        return -1;
    }

    int timeout_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    return timeout_ms < 0 ? 0 : timeout_ms;
}
//======================================================================
int Server::set_poll()
{
    int min_timeout = 100;
    work_cgi = 0;
    Connect *c = list_start, *c_next = NULL;
    for ( ; c; c = c_next)
    {
        c_next = c->next;
        
        int timeout = get_poll_timeout(c->ssl_conn);
        if (timeout >= 0)
        {
            if (timeout < min_timeout)
                min_timeout = timeout;
        }

        time_t now = time(NULL);
        Stream *s = c->stream_start, *s_next = NULL;
        for ( ; s; s = s_next)
        {
            s_next = s->next;
            if ((min_timeout > 0) && (s->wait_write == false))
            {
                if ((s->status == SEND_HEADERS) || (s->status == SEND_DATA))
                    min_timeout = 0;
                if (SSL_pending(s->ssl))
                    min_timeout = 0;
            }

            if ((s->source_data == DYN_PAGE) && (s->cgi.end == false) && (work_cgi < conf->MaxCgiProc))
            {
                if (s->cgi.start == false)
                {
                    int ret = cgi_create_proc(c, s);
                    if (ret < 0)
                    {
                        create_error_message(s, 500, "<h1>500 Internal Server Error, (Error create cgi)</h1>");
                        s->source_data = FROM_DATA_BUFFER;
                    }
                    else
                    {
                        s->cgi.start = true;
                        s->cgi.timer = now;
                    }
                }
                else
                {
                    if (s->status == READ_DATA)
                    {
                        poll_fd[1 + work_cgi].fd = s->cgi.to_script;
                        poll_fd[1 + work_cgi].events = POLLOUT;
                    }
                    else
                    {
                        poll_fd[1 + work_cgi].fd = s->cgi.from_script;
                        poll_fd[1 + work_cgi].events = POLLIN;
                    }
                    cgi_stream[work_cgi++] = s;
                }
            }

            if ((min_timeout == 0) && ((work_cgi >= conf->MaxCgiProc) || (all_cgi == work_cgi)))
                return min_timeout;
        }
    }

    return min_timeout;
}
//======================================================================
void Server::event_loop(SSL *quic_listener, int socket_fd)
{
    int poll_timeout = -1;
    int poll_num = 1;

    poll_fd = new(nothrow) struct pollfd [1 + conf->MaxCgiProc];
    if (!poll_fd)
    {
        print_err("<%s:%d> Error create pollfd array: %s\n", __func__, __LINE__, strerror(errno));
        printf("<%s:%d> Error create pollfd array: %s\n", __func__, __LINE__, strerror(errno));
        return;
    }

    cgi_stream = new(nothrow) Stream* [conf->MaxCgiProc];
    if (!cgi_stream)
    {
        print_err("<%s:%d> Error create Stream array: %s\n", __func__, __LINE__, strerror(errno));
        printf("<%s:%d> Error create Stream array: %s\n", __func__, __LINE__, strerror(errno));
        return;
    }

    poll_fd[0].fd = socket_fd;

    unsigned int num_conn = 0;

    bool run = true;
    for ( ; run; )
    {
        poll_fd[0].events = 0;
        //if (SSL_net_read_desired(quic_listener))
            poll_fd[0].events |= POLLIN;

        poll_num = 1;

        if (list_start)
        {
            poll_timeout = set_poll();
            poll_num += work_cgi;

            if ((poll_timeout < 0) || (poll_timeout >1000))
            {
                fprintf(stderr, "[%s]!!!!!!!!!!<%s:%d> 0x%02X, timeout=%d, poll_num=%d, work_cgi=%d\n", log_time().c_str(), __func__, __LINE__, poll_fd[0].events, poll_timeout, poll_num, work_cgi);
                poll_timeout = 1000;
            }
        }
        else
        {
            poll_timeout = get_poll_timeout(quic_listener);
        }

        int ret = poll(poll_fd, poll_num, poll_timeout);
        if (ret < 0)
        {
            fprintf(stderr, "<%s:%d> err poll(0x%02X, , ): %s\n", __func__, __LINE__, poll_fd[0].events, strerror(errno));
            if (errno == EINTR)
            {
                run = false;
                continue;
            }
            break;
        }

        if (work_cgi > 0)
        {
            time_t now = time(NULL);
            for (int i = 0; i < work_cgi; i++)
            {
                Stream *s = cgi_stream[i];
                if (poll_fd[1 + i].revents)
                {
                    if (poll_fd[1 + i].revents & POLLIN)
                    {
                        cgi_stdout(s);
                    }
                    else if (poll_fd[1 + i].revents & POLLOUT)
                    {
                        cgi_stdin(s);
                    }
                    else
                    {
                        fprintf(stderr, "[%u/%u]<%s:%d>**** cgi: revents=0x%02X\n", s->num_conn, s->num_stream, __func__, __LINE__, poll_fd[1 + i].revents);
                        s->cgi.end = true;
                    }
                }
                else
                {
                    if ((now - s->cgi.timer) > conf->TimeoutCGI)
                    {
                        fprintf(stderr, "[%u/%u]<%s:%d>**** cgi: timeout %d sec\n", s->num_conn, s->num_stream, __func__, __LINE__, (int)(now - s->cgi.timer));
                        create_error_message(s, RS504, "<h1>504 Gateway Time-out</h1>");
                        s->status = SEND_HEADERS;
                        s->source_data = FROM_DATA_BUFFER;
                    }
                }
            }
        }

        if (SSL_handle_events(quic_listener) <= 0)
        {
            print_err("<%s:%d> The connection was closed or an error occurred.\n", __func__, __LINE__);
            ERR_print_errors_fp(stderr);
            break;
        }

        if (num_accept_conn < conf->MaxAcceptConnections)
        {
            SSL *ssl_conn = SSL_accept_connection(quic_listener, 0);
            if (ssl_conn)
            {
                Connect *new_conn = new Connect;
                new_conn->ssl_conn = ssl_conn;
                new_conn->quic_listener = quic_listener;
                new_conn->num_conn = ++num_conn;
                print_err(new_conn, "========= Create new Connect =========\n");
                fprintf(stdout, "[%s] ========= Create new Connect [%u] =========\n", log_time().c_str(), num_conn);
                new_conn->conn_timer = time(NULL);
                add_to_list(new_conn);
            }
        }

        if (list_start)
        {
            connect_handler();
        }
    }
}
//======================================================================
void Server::connect_handler()
{
    time_t now = time(NULL);
    Connect *c = list_start, *next = NULL;
    for ( ; c; c = next)
    {
        next = c->next;

        if (c->ssl_conn == NULL)
        {
            print_err(c, "<%s:%d> Error ssl_conn=%p\n", __func__, __LINE__, c->ssl_conn);
            close_connect(c);
            continue;
        }
        
        c->wait_write = false;

        if ((now - c->conn_timer) >= conf->TimeOut)
        {
            print_err(c, "<%s:%d> ********* Timeout=%d *********\n", __func__, __LINE__, (int)(now - c->conn_timer));
            printf("[%u]<%s:%d> ********* Timeout=%d *********\n", c->num_conn, __func__, __LINE__, (int)(now - c->conn_timer));
            if (c->status != CONNECT_SHUTDOWN)
            {
                if (connect_shutdown(c, __func__, __LINE__))
                    continue;
                c->conn_timer = now;
            }
            else
            {
                close_connect(c);
                continue;
            }
        }

        if (c->status != CONNECT_SHUTDOWN)
        {
            SSL_CONN_CLOSE_INFO info;
            int ret = SSL_get_conn_close_info(c->ssl_conn, &info, sizeof(info));
            if (ret == 1)
            {
                if (connect_shutdown(c, __func__, __LINE__))
                    continue;
            }
        }
        else
        {
            int ret = SSL_shutdown(c->ssl_conn);
            if (ret == 1)
            {
                SSL_CONN_CLOSE_INFO info;
                ret = SSL_get_conn_close_info(c->ssl_conn, &info, sizeof(info));
                if (ret == 1)
                {
                    printf("[%u]<%s:%d> Code: %lu, Flags: %u, [%s]\n", c->num_conn, __func__, __LINE__, info.error_code, info.flags, info.reason ? info.reason : "-");
                    print_err(c, "<%s:%d> Code: %lu, Flags: %u, [%s]\n", __func__, __LINE__, info.error_code, info.flags, info.reason ? info.reason : "-");
                }

                close_connect(c);
                continue;
            }
            else if (ret < 0)
            {
                int err = SSL_get_error(c->ssl_conn, ret);
                printf("[%u]<%s:%d> Error SSL_shutdown(): %s\n", c->num_conn, __func__, __LINE__, ssl_strerror(err));
                //print_err(c, "<%s:%d> Error SSL_shutdown(): %s\n", __func__, __LINE__, ssl_strerror(err));
                if (err == SSL_ERROR_WANT_READ)
                    print_err(c, "<%s:%d> Error SSL_shutdown(): SSL_ERROR_WANT_READ\n", __func__, __LINE__);
                else if (err == SSL_ERROR_WANT_WRITE)
                    print_err(c, "<%s:%d> Error SSL_shutdown(): SSL_ERROR_WANT_WRITE\n", __func__, __LINE__);
                else
                {
                    close_connect(c);
                    continue;
                }
            }
        }

        if (c->status == CONNECT_SHUTDOWN)
        {
            continue;
        }

        if (c->status == CONNECT_WAIT)
        {
            if (SSL_get_state(c->ssl_conn) == TLS_ST_OK)
            {
                //print_err(c, "<%s:%d> SSL_get_state()=TLS_ST_OK\n", __func__, __LINE__);
                c->conn_timer = now;
                c->status = CONNECT_OK;
                // SSL_INCOMING_STREAM_POLICY_AUTO     create 1 stream
                // SSL_INCOMING_STREAM_POLICY_ACCEPT   create > 1 streams
                // SSL_INCOMING_STREAM_POLICY_REJECT   create 0 stream
                /*int err =*/ SSL_set_incoming_stream_policy(c->ssl_conn, SSL_INCOMING_STREAM_POLICY_ACCEPT, 123);
                //print_err(c, "<%s:%d> SSL_set_incoming_stream_policy()=%d\n", __func__, __LINE__, err);
            }
            else
            {
                if ((now - c->conn_timer) > 5)
                {
                    print_err(c, "<%s:%d> Error SSL_get_state(), timeout=%d\n", __func__, __LINE__, (int)(now - c->conn_timer));
                    connect_shutdown(c, __func__, __LINE__);
                }

                continue;
            }
        }

        if (c->status == CONNECT_OK)
        {
            if (c->num_work_stream < conf->MaxStreams)
            {
                long n = SSL_get_accept_stream_queue_len(c->ssl_conn);
                if ((n > 0) || c->tmp_stream)
                {
                    int ret = accept_stream(c, n);
                    if (ret < 0)
                    {
                        close_connect(c);
                        continue;
                    }
                }
            }
        }
        else
            continue;
        
        Stream *s = c->stream_start, *next = NULL;
        for ( ; s; s = next)
        {
            next = s->next;
            c->conn_timer = now;
            s->wait_write = false;
            if (stream_handler(c, s) < 0)
                break;
        }
    }
}
//======================================================================
int Server::stream_handler(Connect *c, Stream *s)
{
    if (s->status == STREAM_CLOSE)
    {
        close_stream(c, s);
        return 0;
    }

    int pend = SSL_pending(s->ssl);
    if (pend && (s->status & (READ_HEADERS | READ_DATA)))
    {
        if (s->frame_size == 0)
        {
            int ret = read_head_frame(s);
            if (ret < 0)
            {
                close_stream(c, s);
                return 0;
            }
            else if (ret == 0)
            {
                print_err(c, "[%u]<%s:%d> read_head_frame()=0\n", s->num_stream, __func__, __LINE__);
                return 0;
            }
        }

        if (s->frame_size)
        {
            char buf[1024];
            int nread = s->frame_size;
            if (nread > (int)sizeof(buf))
                nread = sizeof(buf);
            int ret = ssl_read(s->ssl, buf, nread, &s->err);
            if (ret > 0)
            {
                //fprintf(stderr, "[%u/%u]--- read from client %d/%d ---\n", s->num_conn, s->num_stream, ret, nread);
                if (s->frame_type == HEADERS)
                {
                //hex_print_stderr(__func__, __LINE__, buf, ret);
                    s->headers.ncat(buf, ret);
                    s->frame_size -= ret;
                    if (s->frame_size <= 0)
                    {
                        create_response(c, s);
                        return 0;
                    }
                }
                else if (s->frame_type == DATA)
                {
                    //if (s->cgi.send_post_data == 0)
                    //    hex_print_stderr(__func__, __LINE__, buf, 256);
                    s->buf.ncat(buf, ret);
                    s->frame_size -= ret;
                    s->post_content_len -= ret;
                }
                else
                {
                    fprintf(stderr, "[%u/%u] Error frame_type: %s\n", s->num_conn, s->num_stream, get_str_frame_type(s->frame_type));
                    hex_print_stderr(__func__, __LINE__, buf, ret);
                    return -1;
                }
            }
            else if (ret < 0)
            {
                fprintf(stderr, "[%u/%u]<%s:%d> Error read HEADERS\n", s->num_conn, s->num_stream, __func__, __LINE__);
                close_stream(c, s);
                return 0;
            }
        }
    }

    if (s->status == SEND_HEADERS)
    {
        if (s->headers.size() == 0)
        {
            if (s->source_data == DYN_PAGE)
            {
                int ret = cgi_parse_headers(c, s, true);
                if (ret < 0)
                {
                    create_error_message(s, 500, "<h1>500 Internal Server Error</h1>");
                    s->status = SEND_HEADERS;
                    s->source_data = FROM_DATA_BUFFER;
                }
                else if (ret == 0)
                {
                    return 0;
                }
                else
                {
                    headers_create(s, s->resp_status, 4);
                    header_add(s, 92, conf->ServerSoftware.c_str());
                    s->headers.ncat(s->cgi.headers.ptr(), s->cgi.headers.size());
                    frame_set_size(&s->headers);
                    
                    if (s->buf.size_remain() == 0)
                        s->buf.init();
                }
            }
        }

        int ret = ssl_write(s->ssl, s->headers.ptr(), s->headers.size(), &s->err);
        if (ret < 0)
        {
            if (ret == ERR_TRY_AGAIN)
            {
                s->wait_write = true;
                c->wait_write = true;
                return 0;
            }

            fprintf(stderr, "[%u/%u]<%s:%d> Error write HEADERS\n", s->num_conn, s->num_stream, __func__, __LINE__);
            close_stream(c, s);
            return 0;
        }
        else if (ret > 0)
        {
            //fprintf(stderr, "[%u/%u]<%s:%d> send HEADERS %d bytes\n", s->num_conn, s->num_stream, __func__, __LINE__, ret);
            parse_server_headers(&s->headers, 5);
            s->status = SEND_DATA;
        }
    }

    if (s->status == SEND_DATA)
    {
        char buf[16000];

        if (s->data.size() == 0)
        {
            if (s->source_data == FROM_FILE)
            {
                if (s->file_size > 0)
                {
                    int nread = sizeof(buf);
                    if (s->file_size < nread)
                        nread = s->file_size;
                    int ret = read(s->fd, buf, nread);
                    if (ret <= 0)
                    {
                        if (ret < 0)
                            fprintf(stderr, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
                        ret = SSL_stream_conclude(s->ssl, 0);
                        //print_err(c, "[%u]<%s:%d>[FROM_FILE] SSL_stream_conclude()=%d, send=%lld\n", s->num_stream, __func__, __LINE__, ret, s->data_send);
                        //printf("[%u/%u]<%s:%d>[FROM_FILE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                        s->status = STREAM_CLOSE;
                        s->stream_timer = time(NULL);
                        close(s->fd);
                        s->fd = -1;
                        return 0;
                    }

                    s->file_size -= ret;

                    s->data.ncpy("\x0\x80\x00\x00\x00", 5);
                    s->data.ncat(buf, ret);
                    frame_set_size(&s->data);
                }
                else
                {
                    /*int ret =*/ SSL_stream_conclude(s->ssl, 0);
                    //print_err("[%u/%u]<%s:%d>[FROM_FILE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                    //printf("[%u/%u]<%s:%d>[FROM_FILE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                    s->status = STREAM_CLOSE;
                    s->stream_timer = time(NULL);
                    if (s->fd > 0)
                    {
                        close(s->fd);
                        s->fd = -1;
                    }
                    return 0;
                }
            }
            else if (s->source_data == FROM_DATA_BUFFER)
            {
                if (s->buf.size_remain() > 0)
                {
                    int n = sizeof(buf);
                    if ((int)s->buf.size_remain() < n)
                        n = s->buf.size_remain();
                    s->data.ncpy("\x0\x80\x00\x00\x00", 5);
                    s->data.ncat(s->buf.ptr_remain(), n);
                    frame_set_size(&s->data);
                    s->buf.inc_offset(n);
                }
                else
                {
                    /*int ret =*/ SSL_stream_conclude(s->ssl, 0);
                    //print_err("[%u/%u]<%s:%d>[FROM_DATA_BUFFER] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                    //printf("[%u/%u]<%s:%d>[FROM_DATA_BUFFER] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                    s->status = STREAM_CLOSE;
                    s->stream_timer = time(NULL);
                    s->buf.init();
                    return 0;
                }
            }
            else if (s->source_data == DYN_PAGE)
            {
  //fprintf(stderr, "[%u/%u]<%s:%d> buf.size()=%d\n", s->num_conn, s->num_stream, __func__, __LINE__, c->stream.buf.size());
                if (s->buf.size_remain() > 0)
                {
                    int n = sizeof(buf);
                    if ((int)s->buf.size_remain() < n)
                        n = s->buf.size_remain();
                    s->data.ncpy("\x0\x80\x00\x00\x00", 5);
                    s->data.ncat(s->buf.ptr_remain(), n);
                    frame_set_size(&s->data);
                    
                    s->buf.inc_offset(n);
                    if (s->buf.size_remain() == 0)
                        s->buf.init();
                }
                else
                {
                    if (s->cgi.end)
                    {
                        /*int ret =*/ SSL_stream_conclude(s->ssl, 0);
                        //print_err("[%u/%u]<%s:%d>[DYN_PAGE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                        //printf("[%u/%u]<%s:%d>[DYN_PAGE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                        s->status = STREAM_CLOSE;
                        s->stream_timer = time(NULL);
                    }
                    return 0;
                }
            }
        }

        int ret = ssl_write(s->ssl, s->data.ptr(), s->data.size(), &s->err);
        if (ret < 0)
        {
            if (ret == ERR_TRY_AGAIN)
            {
                s->wait_write = true;
                c->wait_write = true;
                return 0;
            }

            fprintf(stderr, "[%u/%u]<%s:%d> Error write DATA\n", s->num_conn, s->num_stream, __func__, __LINE__);
            close_stream(c, s);
            return 0;
        }
        else if (ret > 0)
        {
    //fprintf(stderr, "[%u/%u]<%s:%d> send DATA %d bytes\n", s->num_conn, s->num_stream, __func__, __LINE__, ret);                
            s->data.init();

            s->all_data_send += ret;
            c->size_send_frame_data += ret;
            s->data_send += (ret - 5);
            c->size_send_data += (ret - 5);

            if (s->source_data == FROM_FILE)
            {
                if (s->file_size <= 0)
                {
                    ret = SSL_stream_conclude(s->ssl, 0);
                    //print_err("[%u/%u]<%s:%d>[FROM_FILE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                    //printf("[%u/%u]<%s:%d>[FROM_FILE] SSL_stream_conclude()=%d, send=%lld\n", s->num_conn, s->num_stream, __func__, __LINE__, ret, s->data_send);
                    s->status = STREAM_CLOSE;
                    s->stream_timer = time(NULL);
                    close(s->fd);
                    s->fd = -1;
                }
            }
        }
    }

    return 0;
}
//======================================================================
void signal_handler(int signo)
{
    if (signo == SIGINT)
    {
        fprintf(stderr, "<%s> ####### SIGINT #######\n", __func__);
    }
    else if (signo == SIGSEGV)
    {
        fprintf(stderr, "<%s> ####### SIGSEGV #######\n", __func__);
        abort();
    }
    else
        fprintf(stderr, "<%s> ? signo=%d (%s)\n", __func__, signo, strsignal(signo));
}
