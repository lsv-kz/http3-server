#include "http3_server.h"

using namespace std;
//======================================================================
void fcgi_set_header(BytesArray* ba, unsigned char type)
{
    int dataLen = ba->size() - 8;

    ba->set_byte(FCGI_VERSION_1, 0);
    ba->set_byte((unsigned char)type, 1);
    ba->set_byte((unsigned char) ((1 >> 8) & 0xff), 2);
    ba->set_byte((unsigned char) ((1) & 0xff), 3);
    ba->set_byte((unsigned char) ((dataLen >> 8) & 0xff), 4);
    ba->set_byte((unsigned char) ((dataLen) & 0xff), 5);
    ba->set_byte(0, 6);
    ba->set_byte(0, 7);
}
//======================================================================
void fcgi_set_header(BytesArray* ba, int offset, unsigned char type)
{
    int dataLen = ba->size() - 8 - offset;
    ba->set_byte(FCGI_VERSION_1, 0 + offset);
    ba->set_byte((unsigned char)type, 1 + offset);
    ba->set_byte((unsigned char) ((1 >> 8) & 0xff), 2 + offset);
    ba->set_byte((unsigned char) ((1) & 0xff), 3 + offset);
    ba->set_byte((unsigned char) ((dataLen >> 8) & 0xff), 4 + offset);
    ba->set_byte((unsigned char) ((dataLen) & 0xff), 5 + offset);
    ba->set_byte(0, 6 + offset);
    ba->set_byte(0, 7 + offset);
}
//======================================================================
void fcgi_set_header(char *buf, unsigned char type, int dataLen)
{
    buf[0] = (unsigned char)FCGI_VERSION_1;
    buf[1] = (unsigned char)type;
    buf[2] = (unsigned char)((1 >> 8) & 0xff);
    buf[3] = (unsigned char)((1) & 0xff);
    buf[4] = (unsigned char)((dataLen >> 8) & 0xff);
    buf[5] = (unsigned char)((dataLen) & 0xff);
    buf[6] = 0;
    buf[7] = 0;
}
//======================================================================
int fcgi_add_param(Stream *s, const char *name, const char *val, int len_val)
{
    if (name == NULL)
    {
        print_err("<%s:%d> Error: name=NULL\n", __func__, __LINE__);
        return -1;
    }

    int len_name = strlen(name);
    if (val == NULL)
        len_val = 0;
    char buf[8], *p = buf;
    int i = 0;

    if (len_name < 0x80)
    {
        *(p++) = (unsigned char)len_name;
        ++i;
    }
    else
    {
        *(p++) = (unsigned char)((len_name >> 24) | 0x80);
        *(p++) = (unsigned char)(len_name >> 16);
        *(p++) = (unsigned char)(len_name >> 8);
        *(p++) = (unsigned char)len_name;
        i += 4;
    }

    if (len_val < 0x80)
    {
        *(p++) = (unsigned char)len_val;
        ++i;
    }
    else
    {
        *(p++) = (unsigned char)((len_val >> 24) | 0x80);
        *(p++) = (unsigned char)(len_val >> 16);
        *(p++) = (unsigned char)(len_val >> 8);
        *(p++) = (unsigned char)len_val;
        i += 4;
    }

    s->cgi.params.ncat(buf, i);
    s->cgi.params.ncat(name, len_name);
    if (len_val > 0)
    {
        s->cgi.params.ncat(val, len_val);
    }

    return 0;
}
//======================================================================
int fcgi_create_params(Connect *c, Stream *s)
{
    int ret = 0;
    s->cgi.params.ncat("\0\0\0\0\0\0\0\0", 8);

    if (s->cgi.type == PHPFPM)
    {
        ret += fcgi_add_param(s, "REDIRECT_STATUS", "true", 4);
    }

    ret += fcgi_add_param(s, "PATH", "/bin:/usr/bin:/usr/local/bin", 28);

    ret += fcgi_add_param(s, 
            "SERVER_SOFTWARE",
            conf->ServerSoftware.c_str(), conf->ServerSoftware.size());

    ret += fcgi_add_param(s, 
            "GATEWAY_INTERFACE",
            "CGI/1.1", 7);

    ret += fcgi_add_param(s, 
            "DOCUMENT_ROOT",
            conf->DocumentRoot.c_str(), conf->DocumentRoot.size());

    ret += fcgi_add_param(s,
                    "DOCUMENT_URI",
                    s->decode_path.c_str(), s->decode_path.size());

    ret += fcgi_add_param(s,
                    "REQUEST_URI",
                    s->raw_path.c_str(), s->raw_path.size());

    ret += fcgi_add_param(s,
                    "REQUEST_METHOD",
                    get_str_method(s->httpMethod), strlen(get_str_method(s->httpMethod)));

    ret += fcgi_add_param(s,
                    "SERVER_PROTOCOL",
                    "HTTP/3.0", 8);

    ret += fcgi_add_param(s,
                    "SERVER_PORT",
                    conf->ServerPort.c_str(), conf->ServerPort.size());

    if (s->referer.size())
    {
        ret += fcgi_add_param(s,
                    "HTTP_REFERER",
                    s->referer.c_str(), s->referer.size());
    }

    if (s->user_agent.size())
    {
        ret += fcgi_add_param(s,
                    "HTTP_USER_AGENT",
                    s->user_agent.c_str(), s->user_agent.size());
    }

    ret += fcgi_add_param(s,
                    "SCRIPT_NAME",
                    s->decode_path.c_str(), s->decode_path.size());

    if (s->cgi.type == PHPFPM)
    {
        s->cgi.path = conf->DocumentRoot;
        s->cgi.path += s->decode_path.c_str();
        ret += fcgi_add_param(s,
                    "SCRIPT_FILENAME",
                    s->cgi.path.c_str(), s->cgi.path.size());
    }

    if (s->httpMethod == M_POST)
    {
        if (s->content_type.size())
        {
            ret += fcgi_add_param(s,
                    "CONTENT_TYPE",
                    s->content_type.c_str(), s->content_type.size());
        }

        if (s->content_length.size())
        {
            ret += fcgi_add_param(s,
                        "CONTENT_LENGTH", 
                        s->content_length.c_str(), s->content_length.size());
        }
    }

    ret += fcgi_add_param(s,
                    "QUERY_STRING",
                    s->query_string.c_str(), s->query_string.size());

    if (ret)
    {
        print_err("<%s:%d> Error: create fcgi param\n", __func__, __LINE__);
        return -1;
    }

    fcgi_set_header(&s->cgi.params, 16, FCGI_PARAMS);
    s->cgi.params.ncat("\x01\x04\x00\x01\x00\x00\x00\x00", 8);
    return 0;
}
//======================================================================
int fcgi_create_connect(Connect *c, Stream *s)
{
    if ((s->cgi.type != PHPFPM) && (s->cgi.type != FASTCGI))
    {
        print_err("<%s:%d> ? req->scriptType=%d \n", __func__, __LINE__, s->cgi.type);
        return -1;
    }

    if (s->cgi.type == PHPFPM)
        s->cgi.socket = &conf->PathPHP;

    s->cgi.fd = create_cgi_socket(s->cgi.socket->c_str());
    if (s->cgi.fd < 0)
    {
        print_err("<%s:%d> Error connect to fcgi\n", __func__, __LINE__);
        return -1;
    }

    char buf[16];
    buf[0] = FCGI_VERSION_1;
    buf[1] = FCGI_BEGIN_REQUEST;
    buf[2] = (unsigned char) ((1 >> 8) & 0xff);
    buf[3] = (unsigned char) ((1) & 0xff);
    buf[4] = (unsigned char) ((8 >> 8) & 0xff);
    buf[5] = (unsigned char) ((8) & 0xff);
    buf[6] = 0;
    buf[7] = 0;

    buf[8] = (unsigned char) ((FCGI_RESPONDER >> 8) & 0xff);
    buf[9] = (unsigned char) (FCGI_RESPONDER        & 0xff);
    buf[10] = (unsigned char) 0;
    memset(buf + 11, 0, 5);
    s->cgi.params.reserve(4096);
    s->cgi.params.ncpy(buf, 16);
    set_stream_status(s, SEND_PARAM);
    return fcgi_create_params(c, s);
}
//======================================================================
int fcgi_stdin(Stream *s)
{
    if (s->data.size_remain() == 0)
    {
        if (s->buf.size_remain())
        {
            s->data.ncpy("00000000", 8);
            s->data.ncat(s->buf.ptr_remain(), s->buf.size_remain());
            s->buf.init();
            fcgi_set_header(&s->data, FCGI_STDIN);
        }

        if (s->req_content_len <= 0)
            s->data.ncat("\x01\x05\x00\x01\x00\x00\x00\x00", 8);
    }
    
    int ret = write(s->cgi.fd, s->data.ptr_remain(), s->data.size_remain());
    if (ret < 0)
    {
        if (errno == EAGAIN)
            return 0;
        else
        {
            create_error_message(s, RS502, "<h2>502 Bad Gateway</h2>");
            return -1;
        }
    }

    s->cgi.timer = time(NULL);
    s->data.inc_offset(ret);
    if (s->data.size_remain() == 0)
    {
        s->data.init();
        if ((s->buf.size() == 0) && (s->req_content_len <= 0))
            set_stream_status(s, SEND_HEADERS);
    }

    return 0;
}
//======================================================================
int fcgi_stdout(Stream *s, int fd)
{
    if (s->cgi.fcgiContentLen == 0)
    {
        if (s->cgi.fcgiPaddingLen > 0)
        {
            char buf[256];
            int ret = read(fd, buf, s->cgi.fcgiPaddingLen);
            if (ret <= 0)
            {
                return -1;
            }

            s->cgi.timer = time(NULL);
            s->cgi.fcgiPaddingLen -= ret;
            if (s->cgi.fcgiPaddingLen > 0)
                return 0;
        }
        
        char buf[8];
        int ret = read(fd, buf, 8);
        if (ret != 8)
        {
            if ((ret == -1) && (errno == EAGAIN))
                return 0;
            return -1;
        }
        
        s->cgi.fcgi_type = buf[1];
        s->cgi.fcgiContentLen = ((unsigned char)buf[4]<<8) | (unsigned char)buf[5];
        s->cgi.fcgiPaddingLen = (unsigned char)buf[6];
        if (s->cgi.fcgiContentLen == 0)
            return 0;
        switch (s->cgi.fcgi_type)
        {
            case FCGI_STDOUT:
                break;
            case FCGI_STDERR:
                break;
            case FCGI_END_REQUEST:
                break;
            default:
                print_err("<%s:%d> Error fcgi type: %d\n", __func__, __LINE__, s->cgi.fcgi_type);
                return -1;
        }
    }
    
    if (s->cgi.fcgi_type == FCGI_STDOUT)
    {
        char buf[16000];
        int num_read = s->cgi.fcgiContentLen;
        if (num_read > (int)sizeof(buf))
            num_read = sizeof(buf);
        int ret = read(fd, buf, num_read);
        if (ret > 0)
        {
            s->cgi.fcgiContentLen -= ret;
            s->buf.ncat(buf, ret);
            s->cgi.timer = time(NULL);
            s->cgi.read_from_cgi += ret;
        }
        else
            return -1;
    }
    else if (s->cgi.fcgi_type == FCGI_STDERR)
    {
        char buf[16000];
        int num_read = s->cgi.fcgiContentLen;
        if (num_read > (int)sizeof(buf))
            num_read = sizeof(buf);
        int ret = read(fd, buf, num_read);
        if (ret > 0)
        {
            s->cgi.fcgiContentLen -= ret;
            fwrite(buf, 1, ret, stderr);
            fprintf(stderr, "\n");
            s->cgi.timer = time(NULL);
            s->cgi.read_from_cgi += ret;
        }
        else
            return -1;
    }
    else if (s->cgi.fcgi_type == FCGI_END_REQUEST)
    {
        char buf[16];
        int num_read = s->cgi.fcgiContentLen;
        if (num_read > (int)sizeof(buf))
            num_read = sizeof(buf);
        int ret = read(fd, buf, num_read);
        if (ret > 0)
        {
            s->cgi.fcgiContentLen -= ret;
            s->cgi.end = true;
            s->cgi.timer = time(NULL);
            s->cgi.read_from_cgi += ret;
        }
        else
            return -1;
    }

    return 0;
}
