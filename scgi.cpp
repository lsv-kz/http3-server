#include "http3_server.h"

using namespace std;
//======================================================================
static int scgi_set_size_data(BytesArray *ba)
{
    ba->set_byte(':', 7);
    int i = 6;
    int size = ba->size() - 8;
    for ( ; i >= 0; --i)
    {
        ba->set_byte((size % 10) + '0', i);
        size /= 10;
        if (size == 0)
            break;
    }

    if (size != 0)
        return -1;

    ba->ncat(",", 1);
    ba->inc_offset(i);

    return 0;
}
//======================================================================
static int scgi_add_param(Stream *s, const char *name, const char *val, int len_val)
{
    if (name == NULL)
    {
        print_err("[%d/%d]<%s:%d> Error: name=NULL\n", s->num_conn, s->num_stream, __func__, __LINE__);
        return -1;
    }

    int len_name = strlen(name);

    if (val == NULL)
        len_val = 0;

    int len = len_name + len_val + 2;
    if ((len + s->data.size()) > 16000)
    {
        print_err("[%d/%d]<%s:%d> Error: name=NULL\n", s->num_conn, s->num_stream, __func__, __LINE__);
        return -1;
    }

    s->data.ncat(name, len_name);
    s->data.ncat("\0", 1);

    if (len_val > 0)
    {
        s->data.ncat(val, len_val);
    }

    s->data.ncat("\0", 1);

    return 0;
}
//======================================================================
static int scgi_create_params(Connect *c, Stream *s)
{
    int ret = 0;
    s->data.ncpy("\0\0\0\0\0\0\0\0", 8);
    s->data.reserve(768);

    if (s->httpMethod == M_POST)
    {
        if (s->content_length.size())
        {
            ret += scgi_add_param(s,
                    "CONTENT_LENGTH", 
                    s->content_length.c_str(), s->content_length.size());
        }
        else
            ret += scgi_add_param(s,
                    "CONTENT_LENGTH", 
                    "0", 1);

        ret += scgi_add_param(s,
                    "CONTENT_TYPE",
                    s->content_type.c_str(), s->content_type.size());
    }
    else
    {
        ret += scgi_add_param(s,
                    "CONTENT_LENGTH",
                    "0", 1);

        ret += scgi_add_param(s,
                    "CONTENT_TYPE",
                    NULL, 0);
    }

    ret += scgi_add_param(s,
                    "PATH",
                    "/bin:/usr/bin:/usr/local/bin", 28);

    ret += scgi_add_param(s,
                    "SERVER_SOFTWARE",
                    conf->ServerSoftware.c_str(), conf->ServerSoftware.size());

    ret += scgi_add_param(s,
                    "SCGI",
                    "1", 1);

    ret += scgi_add_param(s,
                    "DOCUMENT_ROOT",
                    conf->DocumentRoot.c_str(), conf->DocumentRoot.size());

    ret += scgi_add_param(s,
                    "DOCUMENT_URI",
                    s->decode_path.c_str(), s->decode_path.size());

    ret += scgi_add_param(s,
                    "REQUEST_URI",
                    s->raw_path.c_str(), s->raw_path.size());

    ret += scgi_add_param(s,
                    "REQUEST_METHOD",
                    get_str_method(s->httpMethod), strlen(get_str_method(s->httpMethod)));

    ret += scgi_add_param(s,
                    "SERVER_PROTOCOL",
                    "HTTP/3.0", 8);

    ret += scgi_add_param(s,
                    "SERVER_PORT",
                    conf->ServerPort.c_str(), conf->ServerPort.size());

    if (s->referer.size())
    {
        ret += scgi_add_param(s,
                    "HTTP_REFERER",
                    s->referer.c_str(), s->referer.size());
    }

    if (s->user_agent.size())
    {
        ret += scgi_add_param(s,
                    "HTTP_USER_AGENT",
                    s->user_agent.c_str(), s->user_agent.size());
    }

    ret += scgi_add_param(s,
                    "SCRIPT_NAME",
                    s->decode_path.c_str(), s->decode_path.size());

    if (s->query_string.size())
    {
        ret += scgi_add_param(s,
                    "QUERY_STRING",
                    s->query_string.c_str(), s->query_string.size());
    }
    else
    {
        ret += scgi_add_param(s,
                    "QUERY_STRING",
                    NULL, 0);
    }

    if (ret)
    {
        print_err(c, "[%d]<%s:%d> Error scgi_set_param()\n", s->num_stream, __func__, __LINE__);
        create_error_message(s, RS502, "502 Bad Gateway");
        return -1;
    }

    if (scgi_set_size_data(&s->data) < 0)
    {
        print_err(c, "<%s:%d> Error scgi_set_size_data()\n", __func__, __LINE__);
        create_error_message(s, RS502, "502 Bad Gateway");
        return -1;
    }

    s->status = SEND_PARAM;
    return 0;
}
//======================================================================
int scgi_send_param(Stream *s)
{
    int ret = write(s->cgi.fd, s->data.ptr_remain(), s->data.size_remain());
    if (ret < 0)
    {
        if (errno == EAGAIN)
            return 0;
        else
        {
            create_error_message(s, RS502, "502 Bad Gateway");
            return -1;
        }
    }

    s->cgi.timer = time(NULL);
    s->data.inc_offset(ret);
    if (s->data.size_remain() == 0)
    {
        s->data.init();
        if (s->httpMethod == M_POST)
        {
            if ((s->post_content_len <= 0) && (s->buf.size() == 0))
                s->status = SEND_HEADERS;
            else
                s->status = READ_DATA;
        }
        else
        {
            s->status = SEND_HEADERS;
        }
    }

    return 0;
}
//======================================================================
int scgi_create_connect(Connect *c, Stream *s)
{
    s->cgi.fd = create_cgi_socket(s->cgi.socket->c_str());
    if (s->cgi.fd < 0)
    {
        print_err(c, "<%s:%d> Error connect to scgi\n", __func__, __LINE__);
        return s->cgi.fd;
    }

    int ret = scgi_create_params(c, s);
    if (ret < 0)
        return ret;
    else
    {
        s->cgi.timer = 0;
        int opt = 1;
        ioctl(s->cgi.fd, FIONBIO, &opt);
    }

    s->status = SEND_PARAM;
    return 0;
}
