#include "http3_server.h"

using namespace std;

extern int all_cgi, work_cgi;
//======================================================================
int pow_(int x, int y)
{
    if (y < 0)
        return -1;
    int m = 1;
    for (int i = 0; i < y; ++i)
        m = m * x;
    return m;
}
//======================================================================
int bytes_to_int(unsigned char prefix, int pref_len, const char *s, int size, int *len)
{
    int data = pow_(2, pref_len) - 1;
    if (prefix < data)
        data = prefix;
    else
    {
        unsigned char ch;
        for (int i = 0; (*len) < size; ++i)
        {
            ch = s[(*len)++];
            data = data + ((ch & 0x7f)<<(i*7));
            if (!(ch & 0x80))
                break;
        }
    }

    return data;
}
//======================================================================
int get_str(BytesArray *ba, int val_len, bool huffman, std::string& str, int *offset)
{
    if ((val_len + *offset) > (int)ba->size())
    {
        fprintf(stderr, "<%s:%d> Error out of range [%d > %d] off=%d\n", __func__, __LINE__, val_len + *offset, ba->size(), *offset);
        hex_print_stderr(__func__, __LINE__, ba->ptr()+(*offset), ba->size()-(*offset));
        return -1;
    }

    if (huffman)
    {
        HuffmanCode huff;
        huff.decode(ba->ptr() + *offset, val_len, str);
    }
    else
        str.assign(ba->ptr() + *offset, val_len);
    (*offset) += val_len;
    return 0;
}
//======================================================================
int get_str(BytesArray *ba, std::string& str, int *offset)
{
    int ch;
    if (offset == NULL)
    {
        fprintf(stderr, "<%s:%d> Error: offset=NULL\n", __func__, __LINE__);
        return -1;
    }

    if ((ch = ba->get_byte((*offset)++)) < 0)
    {
        fprintf(stderr, "<%s:%d> Error get_byte()=%d\n", __func__, __LINE__, ch);
        return -1;
    }

    bool huffman = ch & 0x80;
    int val_len = ch & 0x7f;
    if (val_len == 0x7f)
    {
        val_len = bytes_to_int(ch & 0x7f, 7, ba->ptr(), ba->size(), offset);
        if (val_len <= 0)
        {
            fprintf(stderr, "<%s:%d> Error bytes_to_int()=%d\n", __func__, __LINE__, val_len);
            return -1;
        }
    }

    if ((val_len + *offset) > (int)ba->size())
    {
        fprintf(stderr, "<%s:%d> Error out of range [%d > %d]\n", __func__, __LINE__, val_len + *offset, ba->size());
        return -1;
    }

    if (huffman)
    {
        HuffmanCode huff;
        huff.decode(ba->ptr() + *offset, val_len, str);
    }
    else
        str.assign(ba->ptr() + *offset, val_len);
    (*offset) += val_len;
    return 0;
}
//======================================================================
int parse_headers(Stream *s)
{
    fprintf(stderr, "[%u/%u]<%s:%d> -------- HEADERS recv from client ----------\n", s->num_conn, s->num_stream, __func__, __LINE__);
    //hex_print_stderr(__func__, __LINE__, s->headers.ptr(), s->headers.size());
    int offset = 0;
    int ch;
    std::string name;
    std::string val;

    offset += 2;

    for ( int i = 0; (offset < (int)s->headers.size()) && (i < 2); )
    {
        if ((ch = s->headers.get_byte(offset++)) < 0)
        {
            fprintf(stderr, "<%s:%d> Error ch=%d, 0x%X\n", __func__, __LINE__, ch, ch);
            return -1;
        }
        //fprintf(stderr, "[0x%X] [%08b]\n", ch, ch);
        if (ch >= 0x80)                     // 4.5.2. Indexed Field Line
        {
            if (!(ch & 0x40))
            {
                fprintf(stderr, "<%s:%d> [0x%02X] Dynamic Table is not created\n", __func__, __LINE__, ch);
                return -1;
            }

            int ind = bytes_to_int(ch & 0x3f, 6, s->headers.ptr(), s->headers.size(), &offset);
            if ((ind >= 0) && (ind <= 98))
            {
                name = static_tab[ind][0];
                val = static_tab[ind][1];
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error index=%d\n", __func__, __LINE__, ind);
                return -1;
            }
        }
        else if ((ch >= 0x20) && (ch <= 0x3f)) // 4.5.6. Literal Field Line with Literal Name
        {
            int name_len = bytes_to_int(ch & 0x07, 3, s->headers.ptr(), s->headers.size(), &offset);
            if (get_str(&s->headers, name_len, ch & 0x08, name, &offset) < 0)
                return -1;
            if (get_str(&s->headers, val, &offset) < 0)
                    return -1;
        }
        else if ((ch >= 0x40) && (ch <= 0x7f)) // 4.5.4. Literal Field Line with Name Reference
        {
            if (!(ch & 0x10))
            {
                fprintf(stderr, "<%s:%d> [0x%02X] Dynamic Table is not created\n", __func__, __LINE__, ch);
                return -1;
            }

            int ind = bytes_to_int(ch & 0x0f, 4, s->headers.ptr(), s->headers.size(), &offset);
            if ((ind >= 0) && (ind < 98))
            {
                name = static_tab[ind][0];
                if (get_str(&s->headers, val, &offset) < 0)
                    return -1;
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error ind=%d\n", __func__, __LINE__, ind);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error [0x%02X]\n", __func__, __LINE__, ch);
            return -1;
        }

        fprintf(stderr, "[0x%02X] [%s: %s]\n", ch, name.c_str(), val.c_str());
        //fprintf(stderr, "[0x%02X] [%08b] [%s: %s]\n", ch, ch, name.c_str(), val.c_str());
        if (name == ":method")
        {
            s->method = val;
            s->httpMethod = get_int_method(val.c_str());
        }
        else if (name == ":authority")
        {
            s->authority = val;
        }
        else if (name == "content-type")
        {
            s->content_type = val;
        }
        else if (name == "content-length")
        {
            s->content_length = val;
            try
            {
                s->post_content_len = stoll(val, NULL, 10);
            }
            catch (...)
            {
                print_err("<%s:%d> Error stoll(\"%s\")\n", __func__, __LINE__, val.c_str());
                return -1;
            }
        }
        else if (name == "range")
        {
            print_err("<%s:%d> [%s: %s]\n", __func__, __LINE__, name.c_str(), val.c_str());
            s->range = val;
        }
        else if (name == ":path")
            s->raw_path = val;
        else if (name == "user-agent")
        {
            s->user_agent = val;
        }
        else if (name == "referer")
        {
            s->referer = val;
        }
    }

    return 0;
}
//======================================================================
int Server::create_response(Connect *c, Stream *s)
{
    int ret = parse_headers(s);
    if (ret < 0)
    {
        create_error_message(s, RS500, "500 Internal Server Error");
        s->status = SEND_HEADERS;
        s->source_data = FROM_DATA_BUFFER;
        return -1;
    }

    s->headers.init();

    int path_len = 0;
    s->decode_query_string = "";
    const char *p = strchr(s->raw_path.c_str(), '?');
    if (p)
    {
        s->query_string = p + 1;
        path_len = p - s->raw_path.c_str();
    }
    else
    {
        s->query_string = "";
        path_len = s->raw_path.size();
    }
    decode(s->raw_path.c_str(), path_len, s->decode_path);

    if (clean_path(s->decode_path) < 0)
    {
        print_err("<%s:%d> Error clean_path(%s)\n", __func__, __LINE__, s->decode_path.c_str());
        create_error_message(s, RS400, "400 Bad Request");
        s->status = SEND_HEADERS;
        s->source_data = FROM_DATA_BUFFER;
        return 0;
    }

    if (s->query_string.size())
    {
        decode(s->query_string.c_str(), s->query_string.size(), s->decode_query_string);
    }

    if (!strncmp(s->decode_path.c_str(), "/cgi-bin/", 9) || !strncmp(s->decode_path.c_str(), "/cgi/", 5))
    {/*
        fprintf(stderr, "<%s:%d> Error: CGI is not supported\n", __func__, __LINE__);
        create_error_message(s, 404, "404 Not Found (CGI is not supported)");
        s->status = SEND_HEADERS;
        s->source_data = FROM_DATA_BUFFER;
        */
        all_cgi++;
        s->source_data = DYN_PAGE;
        s->cgi.cgi_type = CGI;
        s->resp_status = RS200;
        if ((s->httpMethod == M_POST) && (s->post_content_len > 0))
            s->status = READ_DATA;
        else
            s->status = SEND_HEADERS;
fprintf(stderr, "<%s:%d> --------------- CGI %d, all_cgi=%d -------------\n", __func__, __LINE__, s->status, all_cgi);
        return 0;
    }
    else if (strstr(s->decode_path.c_str(), ".php"))
    {
        all_cgi++;
        s->source_data = DYN_PAGE;
        s->buf.init();
        if ((s->httpMethod == M_POST) && (s->post_content_len > 0))
            s->status = READ_DATA;
        else
            s->status = SEND_HEADERS;
fprintf(stderr, "<%s:%d> --------------- PHP %d, all_cgi=%d -------------\n", __func__, __LINE__, s->status, all_cgi);
        if (conf->UsePHP == "php-cgi")
        {
            s->cgi.cgi_type = PHPCGI;
        }
        /*else if (conf->UsePHP == "php-fpm")
        {
            s->cgi.cgi_type = PHPFPM;
        }*/
        else
        {
            create_error_message(s, RS404, "404 Not Found");
            s->status = SEND_HEADERS;
            s->source_data = FROM_DATA_BUFFER;
        }
        return 0;
    }

    s->full_path = conf->DocumentRoot + s->decode_path;
    s->source_data = get_source_data(s->full_path.c_str());
    if (s->source_data == FROM_FILE)
    {
        s->file_size = file_size(s->full_path.c_str());
        if (s->file_size < 0)
        {
            print_err("<%s:%d> Error file_size(%s)\n", __func__, __LINE__, s->full_path.c_str());
            create_error_message(s, RS500, "Internal Server Error");
            s->status = SEND_HEADERS;
            s->source_data = FROM_DATA_BUFFER;
            return 0;
        }

        if (s->range.size())
        {
            int ret = parse_range(s->range.c_str(), s->file_size, &s->offset, &s->resp_content_len);
            if (ret < 0)
            {
                print_err("<%s:%d> Error parse_range(%s)\n", __func__, __LINE__, s->range.c_str());
                create_error_message(s, RS400, "Bad Request");
                s->status = SEND_HEADERS;
                s->source_data = FROM_DATA_BUFFER;
                return 0;
            }
            else if (ret == 0)
            {
                s->resp_content_len = s->file_size;
                s->resp_status = RS200;
            }
            else
                s->resp_status = RS206;
        }
        else
        {
            s->resp_content_len = s->file_size;
            s->resp_status = RS200;
        }

        if (s->resp_status == RS206)
        {
            headers_create(s, RS206, 4);
        }
        else
        {
            headers_create(s, RS200, 4);
        }

        header_add(s, 92, conf->ServerSoftware.c_str());

        const char *resp_content_type = get_content_type(s->decode_path.c_str());
        if (resp_content_type)
            header_add(s, 44, resp_content_type); // 44 "content-type"
        header_add(s, 4, s->resp_content_len);    // 4 "content-length"
        header_add(s, 32);                        // 32  "accept-ranges" : "bytes"
        if (s->resp_status == RS206)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "bytes %lld-%lld/%lld", s->offset, s->offset + s->resp_content_len - 1, s->file_size);
            header_add(s, "content-range", buf);
        }

        frame_set_size(&s->headers);
        s->status = SEND_HEADERS;
        s->file_size = s->resp_content_len;

        s->fd = open(s->full_path.c_str(), O_RDONLY | O_CLOEXEC);
        if (s->fd < 0)
        {
            fprintf(stderr, "<%s:%d> Error open(%s): %s\n", __func__, __LINE__, s->full_path.c_str(), strerror(errno));
            create_error_message(s, RS404, "404 Not Found");
            s->status = SEND_HEADERS;
            s->source_data = FROM_DATA_BUFFER;
        }

        if (s->offset > 0)
            lseek(s->fd, s->offset, SEEK_SET);
    }
    else if (s->source_data == DIRECTORY)
    {
        if (s->decode_path[s->decode_path.size() - 1] != '/')
        {
            s->raw_path += '/';
            string loc = "<a href=\"" + s->raw_path + "\">Moved to: " + s->decode_path + "/</a>";
            create_html(&s->buf, loc.c_str(), "Moved");
            s->status = SEND_HEADERS;
            s->source_data = FROM_DATA_BUFFER;

            headers_create(s, 301, 4);
            header_add(s, 92, conf->ServerSoftware.c_str());
            header_add(s, 12, s->raw_path.c_str());   // 12 "location"
            header_add(s, 44, "text/html");           // 44 "content-type"
            header_add(s, 4, s->buf.size());          // 4  "content-length"
            frame_set_size(&s->headers);
        }
        else
        {
            int err = index_dir(c, s->full_path.c_str(), s->decode_path.c_str(), &s->buf);
            if (err)
            {
                create_error_message(s, RS500, "500 Internal Server Error");
                s->status = SEND_HEADERS;
                s->source_data = FROM_DATA_BUFFER;
                return -1;
            }
            else
            {
                s->status = SEND_HEADERS;
                s->source_data = FROM_DATA_BUFFER;
                headers_create(s, RS200, 4);
                header_add(s, 92, conf->ServerSoftware.c_str());
                header_add(s, 4, s->buf.size());    // 4 "content-length"
                header_add(s, 44, "text/html");
                frame_set_size(&s->headers);
            }
        }
    }
    else
    {
        print_err("[%u/%u]<%s:%d> Error: 404 Not Found\n", s->num_conn, s->num_stream, __func__, __LINE__);
        create_error_message(s, RS404, "404 Not Found");
        s->status = SEND_HEADERS;
        s->source_data = FROM_DATA_BUFFER;
    }

    return 0;
}
//======================================================================
int read_head_frame(Stream *s)
{
    char buf[16];
    int type;

    int ret = ssl_peek(s->ssl, buf, 9, &s->err);
    if (ret < 0)
    {
        fprintf(stderr, "<%s:%d> Error ssl_peek()=%d\n", __func__, __LINE__, ret);
        return ret;
    }
    else if (ret == 0)
        return 0;
//hex_print_stderr(__func__, __LINE__, buf, ret);
    type = (unsigned char)buf[0];
    if (type > 4)
    {
        fprintf(stderr, "*<%s:%d> frame type=%d/0x%X\n", __func__, __LINE__, type, type);
        return -1;
    }

    if (ret < 2)
    {
        fprintf(stderr, "<%s:%d> (ret < 2)\n", __func__, __LINE__);
        return 0;
    }

    int len_bytes = 0;
    switch (buf[1] & 0xc0)
    {
        case 0:
            len_bytes = 1;
            break;
        case 0x40:
            len_bytes = 2;
            break;
        case 0x80:
            len_bytes = 4;
            break;
        case 0xc0:
            len_bytes = 8;
            break;
    }

    if (ret < (len_bytes + 1))
    {
        fprintf(stderr, "<%s:%d> read < %d\n", __func__, __LINE__, len_bytes + 1);
        return 0;
    }

    buf[1] &= 0x3f;
    int shift = len_bytes;
    for ( int i = 1; shift; )
    {
        shift--;
        s->frame_size |= (unsigned char)buf[i++];
        if (shift > 0)
        {
            s->frame_size <<= 8;
        }
    }
//fprintf(stderr, "*<%s:%d> type=%d, %s, size=%d\n", __func__, __LINE__, type, get_str_frame_type((HTTP3_FRAME_TYPE)type), s->frame_size);
    ret = ssl_read(s->ssl, buf, len_bytes + 1, &s->err);
    if (ret <= 0)
    {
        fprintf(stderr, "<%s:%d> Error ssl_read()=%d\n", __func__, __LINE__, ret);
        return ret;
    }

    s->frame_type = (HTTP3_FRAME_TYPE)type;
    return 1;
}
//======================================================================
int Server::accept_stream(Connect *c, int stream_num)
{
    int ret = 0;
    for ( ; stream_num || c->tmp_stream; )
    {
        if (c->tmp_stream == NULL)
        {
            c->tmp_stream = SSL_accept_stream(c->ssl_conn, 0);
        }

        if (c->tmp_stream)
        {
            char buf[256];
            int err = 0;
            int ret = ssl_peek(c->tmp_stream, buf, 8, &err);
            if (ret > 0)
            {
    //hex_print_stderr(__func__, __LINE__, buf, ret);
                if (buf[0] == 0)
                {
                    if (!c->ctrl_ssl)
                    {
                        c->ctrl_ssl = c->tmp_stream;
                        c->tmp_stream = NULL;
                        int pend = SSL_pending(c->ctrl_ssl);
                        fprintf(stderr, "[%u]<%s:%d> Accept Control Stream, pend=%d\n", c->num_conn, __func__, __LINE__, pend);
                        if (pend)
                        {
                            if (pend > (int)sizeof(buf))
                                pend = sizeof(buf);
                            int err = 0;
                            ret = ssl_read(c->ctrl_ssl, buf, pend, &err);
                            if (ret > 0)
                                hex_print_stderr(__func__, __LINE__, buf, ret);
                        }
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if (buf[0] == 2)
                {
                    if (!c->enc_ssl)
                    {
                        fprintf(stderr, "[%u]<%s:%d> Accept Encoder Stream\n", c->num_conn, __func__, __LINE__);
                        c->enc_ssl = c->tmp_stream;
                        c->tmp_stream = NULL;
                        int err = 0;
                        ssl_read(c->enc_ssl, buf, sizeof(buf), &err);
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if (buf[0] == 3)
                {
                    if (!c->dec_ssl)
                    {
                        fprintf(stderr, "[%u]<%s:%d> Accept Decoder Stream\n", c->num_conn, __func__, __LINE__);
                        c->dec_ssl = c->tmp_stream;
                        c->tmp_stream = NULL;
                        int err = 0;
                        ssl_read(c->dec_ssl, buf, sizeof(buf), &err);
                    }
                    else
                    {
                        return -1;
                    }
                }
                else if (buf[0] == 1)
                {
                    print_err(c, "<%s:%d> Accept Stream\n", __func__, __LINE__);
                    Stream *s = c->create_stream(c->tmp_stream);
                    if (s)
                    {
                        printf("[%u/%u]<%s:%d> Accept Stream\n", s->num_conn, s->num_stream, __func__, __LINE__);
                        c->tmp_stream = NULL;
                        ret = 1;
                    }
                    else
                    {
                        fprintf(stderr, "[%u]<%s:%d> Error Create Stream\n", c->num_conn, __func__, __LINE__);
                        c->tmp_stream = NULL;
                        return -1;
                    }
                }
                else
                {
                    fprintf(stderr, "[%u]<%s:%d> Error Stream %u\n", c->num_conn, __func__, __LINE__, (unsigned char)buf[0]);
                    int err = 0;
                    int ret = ssl_read(c->tmp_stream, buf, sizeof(buf), &err);
                    hex_print_stderr(__func__, __LINE__, buf, ret);
                    SSL_free(c->tmp_stream);
                    c->tmp_stream = NULL;
                    return -1;
                }
            }
            else
            {
                if ((err != SSL_ERROR_WANT_READ) && (err != SSL_ERROR_WANT_WRITE))
                {
                    SSL_free(c->tmp_stream);
                    c->tmp_stream = NULL;
                    return -1;
                }
            }
        }
        
        stream_num = SSL_get_accept_stream_queue_len(c->ssl_conn);
    }
    
    return ret;
}
//======================================================================
int Server::connect_shutdown(Connect *c, const char *func, int line)
{
    c->conn_timer = time(NULL);
    c->status = CONNECT_SHUTDOWN;

    SSL_SHUTDOWN_EX_ARGS args;
    memset(&args, 0, sizeof(SSL_SHUTDOWN_EX_ARGS));
    args.quic_error_code = 1234;
    args.quic_reason = "server close connect";
    int ret = SSL_shutdown_ex(c->ssl_conn, 0, &args, sizeof(SSL_SHUTDOWN_EX_ARGS));
    print_err(c, "<%s:%d> SSL_shutdown_ex()=%d\n", func, line, ret);
    if (ret == -1)
    {
        int err = SSL_get_error(c->quic_listener, ret);
        printf("<%s:%d> Error SSL_shutdown(): %s\n", __func__, __LINE__, ssl_strerror(err));
        if (err == SSL_ERROR_ZERO_RETURN)
        {
            close_connect(c);
            return 1;
        }
        else if (err == SSL_ERROR_WANT_READ)
        {
            //return 1;
        }
        else if (err == SSL_ERROR_WANT_WRITE)
        {
            //return 1;
        }
        else
        {
            close_connect(c);
            return 1;
        }
    }
    else if (ret == 1)
    {
        print_err(c, "<%s:%d> SSL_shutdown()=%d\n", __func__, __LINE__, ret);
        printf("[%u]<%s:%d> SSL_shutdown()=%d\n", c->num_conn, __func__, __LINE__, ret);
        close_connect(c);
        return 1;
    }

    return 0;
}
//======================================================================
int int_to_bytes(BytesArray& buf, int data, int pref_len, int huff_coding_mask)
{
    int ret = 0;

    if (data < (pow_(2, pref_len) - 1))
    {
        buf.bytecat((data | huff_coding_mask));
        ++ret;
    }
    else
    {
        buf.bytecat((pow_(2, pref_len) - 1) | huff_coding_mask);
        ++ret;
        data = data - (pow_(2, pref_len) - 1);
        while (data > 128)
        {
            buf.bytecat(data % 128 + 128);
            ++ret;
            data = data / 128;
        }

        buf.bytecat((char)data);
        ++ret;
    }

    return ret;
}
//======================================================================
int headers_create(Stream *s, int status, int size_bytes_num)
{
    const char *ptr;
    int len_bytes = 0;
    switch (size_bytes_num)
    {
        case 1:
            len_bytes = 2;
            ptr = "\x01\x00";
            break;
        case 2:
            len_bytes = 3;
            ptr = "\x01\x40\x00";
            break;
        case 4:
            len_bytes = 5;
            ptr = "\x01\x80\x00\x00\x00";
            break;
        //case 8:
        //    len_bytes = 9;
        //    ptr = "\x01\xc0\x00\x00\x00\x00\x00\x00\x00";
        //    break;
        default:
            fprintf(stderr, "<%s:%d> Error size_bytes_num=%d\n", __func__, __LINE__, size_bytes_num);
            return -1;
    }

    s->headers.ncpy(ptr, len_bytes);       // type, len(4)
    s->headers.ncat("\x0\x0", 2);          // prefix qpack
    
    s->resp_status = status;
    int ind = status_to_index(s, status);
    if (ind)
        int_to_bytes(s->headers, ind, 6, 0xc0); // ":status"

    s->headers.bytecat(0xd7);              // 23 ":scheme" "https"

    s->headers.bytecat(0x56);              // 6  "date"  ""
    string date = get_time();
    int_to_bytes(s->headers, date.size(), 7, 0);
    s->headers.strcat(date.c_str());

    header_add(s, 36, "no-cache, no-store, must-revalidate");// 36  "cache-control" ""

    return 0;
}
//======================================================================
void header_add(Stream *s, int ind)
{
    int_to_bytes(s->headers, ind, 6, 0xc0);
}
//======================================================================
void header_add(Stream *s, int ind, const char *val)
{
    if (val == NULL)
        return;
    int_to_bytes(s->headers, ind, 4, 0x50);
    int_to_bytes(s->headers, strlen(val), 7, 0);
    s->headers.strcat(val);
}
//======================================================================
void header_add(Stream *s, int ind, long long val)
{
    int_to_bytes(s->headers, ind, 4, 0x50);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", val);
    int_to_bytes(s->headers, strlen(buf), 7, 0);
    s->headers.strcat(buf);
}
//======================================================================
void header_add(Stream *s, const char *name, long long ll)
{
    if (name == NULL)
        return;
    int_to_bytes(s->headers, strlen(name), 3, 0x20);
    s->headers.strcat(name);
    char val[32];
    snprintf(val, sizeof(val),"%lld", ll);
    int_to_bytes(s->headers, strlen(val), 7, 0);
    s->headers.strcat(val);
}
//======================================================================
void header_add(Stream *s, const char *name, const char *val)
{
    if ((name == NULL) || (val == NULL))
        return;
    int_to_bytes(s->headers, strlen(name), 3, 0x20);
    s->headers.strcat(name);
    int_to_bytes(s->headers, strlen(val), 7, 0);
    s->headers.strcat(val);
}
//======================================================================
void header_add(BytesArray *ba, const char *name, const char *val)
{
    if ((name == NULL) || (val == NULL))
        return;
    int_to_bytes(*ba, strlen(name), 3, 0x20);
    ba->strcat(name);
    int_to_bytes(*ba, strlen(val), 7, 0);
    ba->strcat(val);
}
//======================================================================
void frame_set_size(BytesArray *ba)
{
    int bytes_num = 0;
    int mask = ba->get_byte(1);
    switch (mask)
    {
        case 0:
            bytes_num = 1;
            break;
        case 0x40:
            bytes_num = 2;
            break;
        case 0x80:
            bytes_num = 4;
            break;
        case 0xc0:
            bytes_num = 8;
            break;
    }

    int size = ba->size() - (bytes_num + 1);
    
    for ( int i = bytes_num; i > 1; --i)
    {
        ba->set_byte(size, i);
        size>>=8;
    }

    ba->set_byte(size | mask, 1);
    //hex_print_stderr(__func__, __LINE__, ba->ptr(), ba->size());
}
//======================================================================
void create_html(BytesArray *ba, const char *msg, const char *title)
{
    ba->strcpy("<!DOCTYPE html>\n"
                 "<html>\n"
                 " <head>\n"
                 "   <meta charset=\"utf-8\">\n"
                 "   <title>");
    ba->strcat(title);
    ba->strcat("</title>\n"
                 " </head>\n"
                 " <body>\n"
                 "  <p>");
    ba->strcat(msg);
    ba->strcat("</p>\n"
                 " </body>\n"
                 "</html>\n");
}
//======================================================================
void create_error_message(Stream *s, int status, const char *msg)
{
    create_html(&s->buf, msg, "Error");

    headers_create(s, status, 4);
    header_add(s, 92, conf->ServerSoftware.c_str());
    header_add(s, 44, "text/html");
    header_add(s, 4, s->buf.size()); // 4 "content-length"
    header_add(s, "qwerty", "ggg");
    frame_set_size(&s->headers);
}
//======================================================================
int cgi_parse_headers(Connect* c, Stream *resp, bool lower_case)
{
    const int MAX_HEADER_LEN = 512;
    const char *p = resp->buf.ptr_remain();
    unsigned int size = resp->buf.size_remain();

    char name[512];
    char val[512];
    name[0] = 0;
    val[0] = 0;

    int name_len = 0;
    int val_len = 0;

    for (unsigned int i = 0; i < size; )
    {
        for ( name_len = 0; i < size; ) // name
        {
            if (i > MAX_HEADER_LEN)
            {
                print_err(c, "<%s:%d> Error: size of header > %d bytes\n", __func__, __LINE__, i);
                return -1;
            }

            char ch = *(p++);
            ++i;
            if (ch == ':')
                break;
            else if (ch == '\r')
            {
                if (name_len)
                {
                    print_err(c, "<%s:%d> Error\n", __func__, __LINE__);
                    return -1;
                }

                if (i < size)
                {
                    ch = *p++;
                    ++i;
                    if (ch == '\n') // empty line
                    {
                        name[name_len] = 0;
                        resp->buf.inc_offset(i);
                        if (resp->buf.size_remain() == 0)
                            resp->buf.init();
                        return 1;
                    }
                    else
                    {
                        print_err(c, "<%s:%d> Error: \\n not found\n", __func__, __LINE__);
                        return -1;
                    }
                }

                return 0;
            }
            else if (ch == '\n') // empty line
            {
                if (name_len)
                {
                    print_err(c, "<%s:%d> Error\n", __func__, __LINE__);
                    return -1;
                }

                resp->buf.inc_offset(i);
                if (resp->buf.size_remain() == 0)
                    resp->buf.init();
                return 1;
            }
            else if (ch == 0)
            {
                print_err(c, "<%s:%d> Error: character = 0\n", __func__, __LINE__);
                return -1;
            }
            else
            {
                name[name_len++] = ch;
                if ((int)sizeof(name) <= name_len)
                {
                    print_err(c, "<%s:%d> Error: names size >= %d\n", __func__, __LINE__, name_len);
                    return -1;
                }
            }
        }

        name[name_len] = 0;

        if (i == size)
            return 0;
        if (lower_case)
        {
            for (int n = 0; n < name_len; ++n)
            {
                name[n] = tolower(name[n]);
            }
        }

        for ( val_len = 0; i < size; ) // value
        {
            if (i > MAX_HEADER_LEN)
            {
                print_err(c, "<%s:%d> Error: size of header > %d bytes\n", __func__, __LINE__, i);
                return -1;
            }

            char ch = *(p++);
            ++i;

            if (ch == '\r')
                continue;
            else if (ch == '\n')
            {
                val[val_len] = 0;
                if (!strcmp_case(name, "status"))
                {
                    sscanf(val, "%d", &resp->resp_status);
                }
                else
                {
                    // add header
                    header_add(&resp->cgi.headers, name, val);
                }
                
                resp->buf.inc_offset(i);
                size = resp->buf.size_remain();
                if (resp->buf.size_remain() == 0)
                    resp->buf.init();
                i = 0;
                break;
            }
            else if (ch == 0)
            {
                print_err(c, "<%s:%d> Error: character = 0\n", __func__, __LINE__);
                return -1;
            }
            else if (ch == ' ')
            {
                if (val_len)
                    val[val_len++] = ch;
            }
            else
                val[val_len++] = ch;
        }

        if (i == size)
            return 0;
    }

    return 0;
}
//======================================================================
int status_to_index(Stream *s, int status)
{
    switch (status)
    {
        case 200:
            return 25;
        case 206:
            return 65;
        case 204:
            return 64;
        case 302:
            return 66;
        case 400:
            return 67;
        case 403:
            return 68;
        case 404:
            return 27;
        case 500:
            return 71;
        case 503:
            return 28;
    }

    //header_add(s, ":status", status);
    header_add(s, 24, status);
    return 0;
}
//======================================================================
int parse_server_headers(BytesArray *ba, int n)
{
    fprintf(stderr, "<%s:%d> -------- HEADERS send to client ----------\n", __func__, __LINE__);
    //hex_print_stderr(__func__, __LINE__, ba->ptr(), ba->size());
    int offset = 0;
    int ch;
    std::string name;
    std::string val;

    offset += n;
    offset += 2;
    
    for ( int i = 0; (offset < (int)ba->size()) && (i < 2); )
    {
        if ((ch = ba->get_byte(offset++)) < 0)
        {
            fprintf(stderr, "<%s:%d> Error ch=%d, 0x%X\n", __func__, __LINE__, ch, ch);
            return -1;
        }
        //fprintf(stderr, "[0x%X] [%08b]\n", ch, ch);
        if (ch >= 0x80)                     // 4.5.2. Indexed Field Line
        {
            if (!(ch & 0x40))
            {
                fprintf(stderr, "<%s:%d> [0x%02X] Dynamic Table is not created\n", __func__, __LINE__, ch);
                return -1;
            }

            int ind = bytes_to_int(ch & 0x3f, 6, ba->ptr(), ba->size(), &offset);
            if ((ind >= 0) && (ind <= 98))
            {
                name = static_tab[ind][0];
                val = static_tab[ind][1];
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error index=%d\n", __func__, __LINE__, ind);
                return -1;
            }
        }
        else if ((ch >= 0x20) && (ch <= 0x3f)) // 4.5.6. Literal Field Line with Literal Name
        {
            int name_len = bytes_to_int(ch & 0x07, 3, ba->ptr(), ba->size(), &offset);
            if (get_str(ba, name_len, ch & 0x08, name, &offset) < 0)
                return -1;
            if (get_str(ba, val, &offset) < 0)
                    return -1;
        }
        else if ((ch >= 0x40) && (ch <= 0x7f)) // 4.5.4. Literal Field Line with Name Reference
        {
            if (!(ch & 0x10))
            {
                fprintf(stderr, "<%s:%d> [0x%02X] Dynamic Table is not created\n", __func__, __LINE__, ch);
                return -1;
            }

            int ind = bytes_to_int(ch & 0x0f, 4, ba->ptr(), ba->size(), &offset);
            if ((ind >= 0) && (ind < 98))
            {
                name = static_tab[ind][0];
                if (get_str(ba, val, &offset) < 0)
                    return -1;
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error ind=%d\n", __func__, __LINE__, ind);
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error [0x%02X]\n", __func__, __LINE__, ch);
            return -1;
        }
        
        fprintf(stderr, "[0x%02X] [%s: %s]\n", ch, name.c_str(), val.c_str());
        //fprintf(stderr, "[0x%02X] [%08b] [%s: %s]\n", ch, ch, name.c_str(), val.c_str());
    }

    return 0;
}
//======================================================================
const char *static_tab[][2] = {
    {":authority", ""},
    {":path", "/"},
    {"age", "0"},
    {"content-disposition", ""},
    {"content-length", "0"},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"referer", ""},
    {"set-cookie", ""},
    {":method", "CONNECT"},
    {":method", "DELETE"},
    {":method", "GET"},
    {":method", "HEAD"},
    {":method", "OPTIONS"},
    {":method", "POST"},
    {":method", "PUT"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "103"},
    {":status", "200"},
    {":status", "304"},
    {":status", "404"},
    {":status", "503"},
    {"accept", "*/*"},
    {"accept", "application/dns-message"},
    {"accept-encoding", "gzip, deflate, br"},
    {"accept-ranges", "bytes"},
    {"access-control-allow-headers", "cache-control"},
    {"access-control-allow-headers", "content-type"},
    {"access-control-allow-origin", "*"},
    {"cache-control", "max-age=0"},
    {"cache-control", "max-age=2592000"},
    {"cache-control", "max-age=604800"},
    {"cache-control", "no-cache"},
    {"cache-control", "no-store"},
    {"cache-control", "public, max-age=31536000"},
    {"content-encoding", "br"},
    {"content-encoding", "gzip"},
    {"content-type", "application/dns-message"},
    {"content-type", "application/javascript"},
    {"content-type", "application/json"},
    {"content-type", "application/x-www-form-urlencoded"},
    {"content-type", "image/gif"},
    {"content-type", "image/jpeg"},
    {"content-type", "image/png"},
    {"content-type", "text/css"},
    {"content-type", "text/html;charset=utf-8"},
    {"content-type", "text/plain"},
    {"content-type", "text/plain;charset=utf-8"},
    {"range", "bytes=0-"},
    {"strict-transport-security", "max-age=31536000"},
    {"strict-transport-security", "max-age=31536000; includesubdomains"},
    {"strict-transport-security", "max-age=31536000; includesubdomains; preload"},
    {"vary", "accept-encoding"},
    {"vary", "origin"},
    {"x-content-type-options", "nosniff"},
    {"x-xss-protection", "1; mode=block"},
    {":status", "100"},
    {":status", "204"},
    {":status", "206"},
    {":status", "302"},
    {":status", "400"},
    {":status", "403"},
    {":status", "421"},
    {":status", "425"},
    {":status", "500"},
    {"accept-language", ""},
    {"access-control-allow-credentials", "FALSE"},
    {"access-control-allow-credentials", "TRUE"},
    {"access-control-allow-headers", "*"},
    {"access-control-allow-methods", "get"},
    {"access-control-allow-methods", "get, post, options"},
    {"access-control-allow-methods", "options"},
    {"access-control-expose-headers", "content-length"},
    {"access-control-request-headers", "content-type"},
    {"access-control-request-method", "get"},
    {"access-control-request-method", "post"},
    {"alt-svc", "clear"},
    {"authorization", ""},
    {"content-security-policy", "script-src 'none'; object-src 'none'; base-uri 'none'"},
    {"early-data", "1"},
    {"expect-ct", ""},
    {"forwarded", ""},
    {"if-range", ""},
    {"origin", ""},
    {"purpose", "prefetch"},
    {"server", ""},
    {"timing-allow-origin", "*"},
    {"upgrade-insecure-requests", "1"},
    {"user-agent", ""},
    {"x-forwarded-for", ""},
    {"x-frame-options", "deny"},
    {"x-frame-options", "sameorigin"},
    {NULL, NULL}};
