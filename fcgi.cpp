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
void fcgi_set_header(char *s, unsigned char type, int dataLen)
{
    s[0] = (unsigned char)FCGI_VERSION_1;
    s[1] = (unsigned char)type;
    s[2] = (unsigned char)((1 >> 8) & 0xff);
    s[3] = (unsigned char)((1) & 0xff);
    s[4] = (unsigned char)((dataLen >> 8) & 0xff);
    s[5] = (unsigned char)((dataLen) & 0xff);
    s[6] = 0;
    s[7] = 0;
}
//======================================================================
int fcgi_add_param(Stream *str, const char *name, const char *val, int len_val)
{
    if (name == NULL)
    {
        print_err("<%s:%d> Error: name=NULL\n", __func__, __LINE__);
        return -1;
    }

    int len_name = strlen(name);
    if (val == NULL)
        len_val = 0;
    char s[8], *p = s;
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

    str->params.ncat(s, i);
    str->params.ncat(name, len_name);
    if (len_val > 0)
    {
        str->params.ncat(val, len_val);
    }

    return 0;
}
//======================================================================
int fcgi_create_params(Connect *c, Stream *str)
{
    int ret = 0;
    str->params.ncat("\0\0\0\0\0\0\0\0", 8);

    if (str->cgi.type == PHPFPM)
    {
        ret += fcgi_add_param(str, "REDIRECT_STATUS", "true", 4);
    }

    ret += fcgi_add_param(str, "PATH", "/bin:/usr/bin:/usr/local/bin", 28);

    ret += fcgi_add_param(str, 
            "SERVER_SOFTWARE",
            conf->ServerSoftware.c_str(), conf->ServerSoftware.size());

    ret += fcgi_add_param(str, 
            "GATEWAY_INTERFACE",
            "CGI/1.1", 7);

    ret += fcgi_add_param(str, 
            "DOCUMENT_ROOT",
            conf->DocumentRoot.c_str(), conf->DocumentRoot.size());

    ret += fcgi_add_param(str,
                    "DOCUMENT_URI",
                    str->decode_path.c_str(), str->decode_path.size());

    ret += fcgi_add_param(str,
                    "REQUEST_URI",
                    str->raw_path.c_str(), str->raw_path.size());

    ret += fcgi_add_param(str,
                    "REQUEST_METHOD",
                    get_str_method(str->httpMethod), strlen(get_str_method(str->httpMethod)));

    ret += fcgi_add_param(str,
                    "SERVER_PROTOCOL",
                    "HTTP/3.0", 8);

    ret += fcgi_add_param(str,
                    "SERVER_PORT",
                    conf->ServerPort.c_str(), conf->ServerPort.size());

    if (str->referer.size())
    {
        ret += fcgi_add_param(str,
                    "HTTP_REFERER",
                    str->referer.c_str(), str->referer.size());
    }

    if (str->user_agent.size())
    {
        ret += fcgi_add_param(str,
                    "HTTP_USER_AGENT",
                    str->user_agent.c_str(), str->user_agent.size());
    }

    ret += fcgi_add_param(str,
                    "SCRIPT_NAME",
                    str->decode_path.c_str(), str->decode_path.size());

    if (str->cgi.type == PHPFPM)
    {
        str->cgi.path = conf->DocumentRoot;
        str->cgi.path += str->decode_path.c_str();
        ret += fcgi_add_param(str,
                    "SCRIPT_FILENAME",
                    str->cgi.path.c_str(), str->cgi.path.size());
    }

    if (str->httpMethod == M_POST)
    {
        if (str->content_type.size())
        {
            ret += fcgi_add_param(str,
                    "CONTENT_TYPE",
                    str->content_type.c_str(), str->content_type.size());
        }

        if (str->content_length.size())
        {
            ret += fcgi_add_param(str,
                        "CONTENT_LENGTH", 
                        str->content_length.c_str(), str->content_length.size());
        }
    }

    ret += fcgi_add_param(str,
                    "QUERY_STRING",
                    str->query_string.c_str(), str->query_string.size());

    if (ret)
    {
        print_err("<%s:%d> Error: create fcgi param\n", __func__, __LINE__);
        return -1;
    }

    fcgi_set_header(&str->params, 16, FCGI_PARAMS);
    str->params.ncat("\x01\x04\x00\x01\x00\x00\x00\x00", 8);
    return 0;
}
//======================================================================
int fcgi_create_connect(Connect *c, Stream *str)
{
    if ((str->cgi.type != PHPFPM) && (str->cgi.type != FASTCGI))
    {
        print_err("<%s:%d> ? req->scriptType=%d \n", __func__, __LINE__, str->cgi.type);
        return -1;
    }

    if (str->cgi.type == PHPFPM)
        str->cgi.socket = &conf->PathPHP;

    str->cgi.fd = create_cgi_socket(str->cgi.socket->c_str());
    if (str->cgi.fd < 0)
    {
        print_err("<%s:%d> Error connect to fcgi\n", __func__, __LINE__);
        return -1;
    }

    char s[16];
    s[0] = FCGI_VERSION_1;
    s[1] = FCGI_BEGIN_REQUEST;
    s[2] = (unsigned char) ((1 >> 8) & 0xff);
    s[3] = (unsigned char) ((1) & 0xff);
    s[4] = (unsigned char) ((8 >> 8) & 0xff);
    s[5] = (unsigned char) ((8) & 0xff);
    s[6] = 0;
    s[7] = 0;

    s[8] = (unsigned char) ((FCGI_RESPONDER >> 8) & 0xff);
    s[9] = (unsigned char) (FCGI_RESPONDER        & 0xff);
    s[10] = (unsigned char) 0;
    memset(s + 11, 0, 5);
    str->params.reserve(4096);
    str->params.ncpy(s, 16);
    str->status = SEND_PARAM;
    return fcgi_create_params(c, str);
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
        {
            s->status = SEND_HEADERS;
        }
    }

    return 0;
}
//======================================================================
int fcgi_stdout(Stream *str, int fd)
{
    if (str->cgi.fcgiContentLen == 0)
    {
        if (str->cgi.fcgiPaddingLen > 0)
        {
            char s[256];
            int ret = read(fd, s, str->cgi.fcgiPaddingLen);
            if (ret <= 0)
            {
                return -1;
            }

            str->cgi.timer = time(NULL);
            str->cgi.fcgiPaddingLen -= ret;
            if (str->cgi.fcgiPaddingLen > 0)
                return 0;
        }
        
        char s[8];
        int ret = read(fd, s, 8);
        if (ret != 8)
        {
            if ((ret == -1) && (errno == EAGAIN))
                return 0;
            return -1;
        }
        
        str->cgi.fcgi_type = s[1];
        str->cgi.fcgiContentLen = ((unsigned char)s[4]<<8) | (unsigned char)s[5];
        str->cgi.fcgiPaddingLen = (unsigned char)s[6];
        if (str->cgi.fcgiContentLen == 0)
            return 0;
        switch (str->cgi.fcgi_type)
        {
            case FCGI_STDOUT:
                break;
            case FCGI_STDERR:
                break;
            case FCGI_END_REQUEST:
                break;
            default:
                print_err("<%s:%d> Error fcgi type: %d\n", __func__, __LINE__, str->cgi.fcgi_type);
                return -1;
        }
    }
    
    if (str->cgi.fcgi_type == FCGI_STDOUT)
    {
        char buf[16000];
        int num_read = str->cgi.fcgiContentLen;
        if (num_read > (int)sizeof(buf))
            num_read = sizeof(buf);
        int ret = read(fd, buf, num_read);
        if (ret > 0)
        {
            str->cgi.fcgiContentLen -= ret;
            str->buf.ncat(buf, ret);
            str->cgi.timer = time(NULL);
            str->cgi.read_from_cgi += ret;
        }
        else
            return -1;
    }
    else if (str->cgi.fcgi_type == FCGI_STDERR)
    {
        char buf[16000];
        int num_read = str->cgi.fcgiContentLen;
        if (num_read > (int)sizeof(buf))
            num_read = sizeof(buf);
        int ret = read(fd, buf, num_read);
        if (ret > 0)
        {
            str->cgi.fcgiContentLen -= ret;
            fwrite(buf, 1, ret, stderr);
            fprintf(stderr, "\n");
            str->cgi.timer = time(NULL);
            str->cgi.read_from_cgi += ret;
        }
        else
            return -1;
    }
    else if (str->cgi.fcgi_type == FCGI_END_REQUEST)
    {
        char buf[16];
        int num_read = str->cgi.fcgiContentLen;
        if (num_read > (int)sizeof(buf))
            num_read = sizeof(buf);
        int ret = read(fd, buf, num_read);
        if (ret > 0)
        {
            str->cgi.fcgiContentLen -= ret;
            str->cgi.end = true;
            str->cgi.timer = time(NULL);
            str->cgi.read_from_cgi += ret;
        }
        else
            return -1;
    }

    return 0;
}
