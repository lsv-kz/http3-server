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

    if (s->cgi.cgi_type == CGI)
    {
        s->cgi.path = conf->ScriptDir;
        s->cgi.path += get_script_name(s->decode_path.c_str());
    }
    else if (s->cgi.cgi_type == PHPCGI)
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

        if (s->cgi.cgi_type == PHPCGI)
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

        if (s->cgi.cgi_type == CGI)
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
        else if (s->cgi.cgi_type == PHPCGI)
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
int cgi_stdin(Stream *s)
{
    if (s->buf.size())
    {
        int ret = write(s->cgi.to_script, s->buf.ptr_remain(), s->buf.size_remain());
//fprintf(stderr, "[%s]<%s:%d> write to script: %d\n", log_time().c_str(), __func__, __LINE__, ret);
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
            fprintf(stderr, "[%u/%u]<%s:%d>**** cgi\n", s->num_conn, s->num_stream, __func__, __LINE__);
            s->cgi.end = true;
            create_error_message(s, RS502, "502 Bad Gateway");
            s->status = SEND_HEADERS;
            s->source_data = FROM_DATA_BUFFER;
            return 1;
        }
    }

    return 0;
}
//======================================================================
int cgi_stdout(Stream *s)
{
    char buf[16000];
    int ret = read(s->cgi.from_script, buf, sizeof(buf) - 1);
//fprintf(stderr, "[%s]<%s:%d> read from script: %d\n", log_time().c_str(), __func__, __LINE__, ret);
    if (ret > 0)
    {
        buf[ret] = 0;
        //fprintf(stderr, "<%s:%d> --------- read from cgi --------\n%s\n", __func__, __LINE__, buf);
        s->buf.ncat(buf, ret);
        s->cgi.timer = time(NULL);
    }
    else
    {
        fprintf(stderr, "[%u/%u]<%s:%d>**** read from cgi %d\n", s->num_conn, s->num_stream, __func__, __LINE__, ret);
        s->cgi.end = true;
        return 1;
    }

    return 0;
}
