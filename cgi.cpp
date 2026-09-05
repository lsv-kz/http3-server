#include "http3_server.h"

using namespace std;
//======================================================================
void kill_chld(pid_t pid)
{
    if (pid > 0)
    {
        if (waitpid(pid, NULL, WNOHANG) == 0)
        {
            if (kill(pid, SIGKILL) == 0)
                waitpid(pid, NULL, 0);
            else
                print_err("<%s:%d> Error kill(): %s\n", __func__, __LINE__, strerror(errno));
        }
    }
}
//======================================================================
const char *get_script_name(const char *name)
{
    const char *p;
    if (!name)
        return "";

    if ((p = strchr(name + 1, '/')))
        return p;

    return "";
}
//======================================================================
const char *base_name(const char *path)
{
    const char *p;

    if (!path)
        return NULL;

    p = strrchr(path, '/');
    if (p)
        return p + 1;

    return path;
}
//======================================================================
int cgi_fork(Connect *c, Stream *s, int* serv_cgi, int* cgi_serv)
{
    struct stat st;

    if (s->cgi.type == CGI)
    {
        s->cgi.path = conf->ScriptDir;
        s->cgi.path += get_script_name(s->decode_path.c_str());
    }
    else if (s->cgi.type == PHPCGI)
    {
        s->cgi.path = conf->DocumentRoot;
        s->cgi.path += s->decode_path;
    }

    if (stat(s->cgi.path.c_str(), &st) == -1)
    {
        print_err("<%s:%d> script (%s) not found\n", __func__, __LINE__, s->cgi.path.c_str());
        return -RS404;
    }
    //--------------------------- fork ---------------------------------
    pid_t pid = fork();
    if (pid < 0)
    {
        s->cgi.pid = pid;
        print_err("<%s:%d> Error fork(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }
    else if (pid == 0)
    {
        //----------------------- child --------------------------------
        close(cgi_serv[0]);
        cgi_serv[0] = -1;

        int fd = open("/dev/null", O_RDONLY);
        if (fd > 0)
        {
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        if (s->httpMethod == M_POST)
        {
            close(serv_cgi[1]);
            if (serv_cgi[0] != STDIN_FILENO)
            {
                if (dup2(serv_cgi[0], STDIN_FILENO) < 0)
                {
                    print_err("<%s:%d> Error dup2(): %s\n", __func__, __LINE__, strerror(errno));
                    exit(1);
                }
                close(serv_cgi[0]);
                serv_cgi[0] = -1;
            }
        }

        if (cgi_serv[1] != STDOUT_FILENO)
        {
            if (dup2(cgi_serv[1], STDOUT_FILENO) < 0)
            {
                print_err("<%s:%d> Error dup2(): %s\n", __func__, __LINE__, strerror(errno));
                goto to_pipe;
            }
            close(cgi_serv[1]);
            cgi_serv[1] = -1;
        }

        if (s->cgi.type == PHPCGI)
            setenv("REDIRECT_STATUS", "true", 1);
        setenv("SERVER_SOFTWARE", conf->ServerSoftware.c_str(), 1);
        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("REQUEST_METHOD", get_str_method(s->httpMethod), 1);
        setenv("SERVER_PROTOCOL", "HTTP/3", 1);
        setenv("DOCUMENT_ROOT", conf->DocumentRoot.c_str(), 1);
        setenv("DOCUMENT_URI", s->decode_path.c_str(), 1);
        setenv("REQUEST_URI", s->raw_path.c_str(), 1);
        setenv("SCRIPT_NAME", s->decode_path.c_str(), 1);
        setenv("SCRIPT_FILENAME", s->cgi.path.c_str(), 1);

        if (s->authority.size())
            setenv("HTTP_HOST", s->authority.c_str(), 1);
        if (s->referer.size())
            setenv("HTTP_REFERER", s->referer.c_str(), 1);
        if (s->user_agent.size())
            setenv("HTTP_USER_AGENT", s->user_agent.c_str(), 1);

        if (s->httpMethod == M_POST)
        {
            if (s->content_type.size())
                setenv("CONTENT_TYPE", s->content_type.c_str(), 1);

            if (s->content_length.size())
                setenv("CONTENT_LENGTH", s->content_length.c_str(), 1);
        }

        if (s->query_string.size())
            setenv("QUERY_STRING", s->query_string.c_str(), 1);

        if (s->cgi.type == CGI)
        {
            if (chdir(conf->ScriptDir.c_str()))
            {
                fprintf(stderr, "<%s:%d> Error chdir(%s): %s\n", __func__, __LINE__, conf->ScriptDir.c_str(), strerror(errno));
                goto to_pipe;
            }

            execl(get_script_name(s->decode_path.c_str()) + 1, base_name(s->decode_path.c_str()), NULL);
            print_err("<%s:%d> Error execl(%s, %s): %s\n", __func__, __LINE__,
                        get_script_name(s->decode_path.c_str()) + 1, base_name(s->decode_path.c_str()), strerror(errno));
        }
        else if (s->cgi.type == PHPCGI)
        {
            execl(conf->PathPHP.c_str(), base_name(conf->PathPHP.c_str()), NULL);
            print_err("<%s:%d> Error execl(%s, %s): %s\n", __func__, __LINE__,
                        conf->PathPHP.c_str(), base_name(conf->PathPHP.c_str()), strerror(errno));
        }

    to_pipe:
        {
            char err_msg[] = "Status: 500 Internal Server Error\r\n"
                "Content-type: text/html; charset=UTF-8\r\n"
                "\r\n"
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "<title>500 Internal Server Error</title>\n"
                "<meta http-equiv=\"content-type\" content=\"text/html\">\n"
                "</head>\n"
                "<body>\n"
                "<p> 500 Internal Server Error</p>\n"
                "</body>\n"
                "</html>";
            write(STDOUT_FILENO, err_msg, strlen(err_msg));
        }
        close(STDOUT_FILENO);
        exit(EXIT_FAILURE);
    }
    else
    {
        s->cgi.pid = pid;
        s->cgi.timer = 0;

        close(cgi_serv[1]);
        cgi_serv[1] = -1;

        int opt = 1;
        ioctl(cgi_serv[0], FIONBIO, &opt);

        if (s->httpMethod == M_POST)
        {
            ioctl(serv_cgi[1], FIONBIO, &opt);
            if (serv_cgi[0] > 0)
            {
                close(serv_cgi[0]);
                serv_cgi[0] = -1;
            }

            if (s->post_content_len <= 0)
            {
                close(serv_cgi[1]);
                serv_cgi[1] = -1;
            }
        }

        s->cgi.from_script = cgi_serv[0];
        s->cgi.to_script = serv_cgi[1];

        return 0;
    }
}
//======================================================================
int cgi_create_proc(Connect *c, Stream *s)
{
    int serv_cgi[2], cgi_serv[2];
    int n = pipe(cgi_serv);
    if (n == -1)
    {
        print_err("<%s:%d> Error pipe()=%d, id=%d \n", __func__, __LINE__, n, s->id);
        return -1;
    }

    if (s->httpMethod == M_POST)
    {
        n = pipe(serv_cgi);
        if (n == -1)
        {
            print_err("<%s:%d> Error pipe()=%d, id=%d \n", __func__, __LINE__, n, s->id);
            close(cgi_serv[0]);
            cgi_serv[0] = -1;

            close(cgi_serv[1]);
            cgi_serv[1] = -1;
            return -1;
        }
    }
    else
    {
        serv_cgi[0] = -1;
        serv_cgi[1] = -1;
    }

    n = cgi_fork(c, s, serv_cgi, cgi_serv);
    if (n < 0)
    {
        if (s->httpMethod == M_POST)
        {
            close(serv_cgi[0]);
            serv_cgi[0] = -1;

            close(serv_cgi[1]);
            serv_cgi[1] = -1;
            s->cgi.to_script = -1;
        }

        close(cgi_serv[0]);
        cgi_serv[0] = -1;
        s->cgi.from_script = -1;

        close(cgi_serv[1]);
        cgi_serv[1] = -1;
        return n;
    }

    return 0;
}
//======================================================================
int cgi_stdin(Stream *s, int fd)
{
    if (s->buf.size())
    {
        int ret = write(fd, s->buf.ptr_remain(), s->buf.size_remain());
        if (ret > 0)
        {
            s->cgi.timer = time(NULL);
            s->buf.inc_offset(ret);
            s->cgi.send_post_data += ret;
            if (s->buf.size_remain() == 0)
                s->buf.init();

            if ((s->post_content_len <= 0) && (s->buf.size() == 0))
            {
                s->status = SEND_HEADERS;
                close(s->cgi.to_script);
                s->cgi.to_script = -1;
            }
        }
        else
        {
            fprintf(stderr, "[%u/%u]<%s:%d> Error cgi_stdin()=%d\n", s->num_conn, s->num_stream, __func__, __LINE__, ret);
            s->cgi.end = true;
            create_error_message(s, RS502, "502 Bad Gateway");
            return -1;
        }
    }

    return 0;
}
//======================================================================
int cgi_stdout(Stream *s, int fd)
{
    char buf[16000];
    int ret = read(fd, buf, sizeof(buf) - 1);
    if (ret > 0)
    {
        buf[ret] = 0;
        s->buf.ncat(buf, ret);
        s->cgi.timer = time(NULL);
        s->cgi.read_from_cgi += ret;
    }
    else if (ret == 0)
    {
        s->cgi.end = true;
    }
    else
    {
        fprintf(stderr, "[%u/%u]<%s:%d> Error read from cgi: %s\n", s->num_conn, s->num_stream, __func__, __LINE__, strerror(errno));
    }

    return ret;
}
//======================================================================
int Server::cgi_handler()
{
    time_t now = time(NULL);
    for (int i = 0; i < cgi_stream_size; i++)
    {
        Stream *s = cgi_stream[i];
        if (poll_fd[1 + i].revents)
        {
            if ((s->cgi.type == CGI) || (s->cgi.type == PHPCGI))
            {
                if (poll_fd[1 + i].revents == POLLOUT)
                {
                    cgi_stdin(s, s->cgi.to_script);
                }
                else if (poll_fd[1 + i].revents & POLLIN)
                {
                    cgi_stdout(s, s->cgi.from_script);
                    if (poll_fd[1 + i].revents & POLLHUP)
                        s->cgi.end = true;
                }
                else
                {
                    //fprintf(stderr, "[%u/%u]<%s:%d> CGI: %d/%d/%d, pid=%d, fd=%d, revents=0x%02X; %lld\n", s->num_conn, s->num_stream, 
                    //        __func__, __LINE__, s->cgi.cgi, s->cgi.start, s->cgi.end, s->cgi.pid, poll_fd[1 + i].fd, poll_fd[1 + i].revents, s->cgi.read_from_cgi);
                    s->cgi.end = true;
                }
            }
            else if (s->cgi.type == SCGI)
            {
                if (poll_fd[1 + i].revents == POLLOUT)
                {
                    if (s->status == SEND_PARAM)
                        scgi_send_param(s);
                    else if (s->status == READ_DATA)
                        cgi_stdin(s, s->cgi.fd);
                }
                else if (poll_fd[1 + i].revents & POLLIN)
                {
                    cgi_stdout(s, s->cgi.fd);
                    if (poll_fd[1 + i].revents & POLLHUP)
                        s->cgi.end = true;
                }
                else
                {
                    s->cgi.end = true;
                }
            }
        }
        else
        {
            if ((now - s->cgi.timer) >= conf->TimeoutCGI)
            {
                fprintf(stderr, "[%u/%u]<%s:%d> CGI timeout %d sec\n", s->num_conn, s->num_stream, __func__, __LINE__, (int)(now - s->cgi.timer));
                create_error_message(s, RS504, "<h1>504 Gateway Time-out</h1>");
            }
        }
    }

    return 0;
}
//======================================================================
bool is_cgi(Stream *s)
{
    const char *p = strrchr(s->decode_path.c_str(), '/');
    if (!p)
        return false;
    fcgi_list_addr *i = conf->fcgi_list;
    for (; i; i = i->next)
    {
        if (i->script_name[0] == '~')
        {
            if (!strcmp(p, i->script_name.c_str() + 1))
                break;
        }
        else
        {
            if (s->decode_path == i->script_name)
                break;
        }
    }

    if (!i)
        return false;

    s->cgi.socket = &i->addr;
    if (i->type == FASTCGI)
        s->cgi.type = FASTCGI;
    else if (i->type == SCGI)
        s->cgi.type = SCGI;
    else
    {
        s->source_data = NO_SOURCE;
        return false;
    }

    s->source_data = DYN_PAGE;
    s->resp_status = RS200;

    return true;
}
