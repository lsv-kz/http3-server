#ifndef SERVER_H_
#define SERVER_H_
#define _FILE_OFFSET_BITS 64

#include <iostream>
#include <cstdio>
#include <cstring>
#include <signal.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "bytes_array.h"
#include "huffman.h"

extern const char *static_tab[][2];

const int  ERR_TRY_AGAIN = -1000;

struct Config
{
    std::string ServerSoftware;

    std::string ServerAddr;
    std::string ServerPort;

    std::string DocumentRoot;
    std::string ScriptDir;
    std::string LogDir;

    std::string Certificate;
    std::string CertificateKey;

    std::string UsePHP;
    std::string PathPHP;

    int MaxAcceptConnections;
    int MaxStreams;

    bool ServerNameIndication = false;

    int TimeOut = 15;

    int MaxCgiProc = 5;
    int TimeoutCGI = 5;

    bool ShowMediaFiles = false;
};

extern const Config* const conf;
void kill_chld(pid_t pid);
std::string log_time();

void inc_all_cgi();
void dec_all_cgi();
int get_all_cgi();

void inc_work_cgi();
void dec_work_cgi();
int get_work_cgi();
//----------------------------------------------------------------------
enum HTTP_METHOD
{
    M_NULL, M_GET, M_HEAD, M_POST, M_OPTIONS, M_PUT,
    M_PATCH, M_DELETE, M_TRACE, M_CONNECT
};

enum
{
    RS101 = 101,
    RS200 = 200, RS204 = 204, RS206 = 206,
    RS301 = 301, RS302,
    RS400 = 400, RS401, RS402, RS403, RS404, RS405, RS406, RS407,
    RS408, RS411 = 411, RS413 = 413, RS414, RS415, RS416, RS417, RS418, RS429 = 429, RS431 = 431,
    RS500 = 500, RS501, RS502, RS503, RS504, RS505
};

enum SOURCE_DATA
{
    NO_SOURCE,
    DIRECTORY,
    FROM_FILE,
    FROM_DATA_BUFFER,
    DYN_PAGE,
};

enum STREAM_STATUS
{
    READ_HEADERS = 0x1,
    READ_DATA = 0x2,
    SEND_HEADERS = 0x4,
    SEND_DATA = 0x8,
    SEND_END = 0x10,
    STREAM_CLOSE = 0x20,
};

enum CONNECT_STATUS
{
    CONNECT_WAIT,
    CONNECT_OK,
    CONNECT_SHUTDOWN,
    CONNECT_CLOSE,
};

enum HTTP3_FRAME_TYPE
{
    DATA,
    HEADERS,

    CANCEL_PUSH =3,
    SETTINGS,
    PUSH_PROMISE,

    GOAWAY = 7,

    ORIGIN = 0x0C,
    MAX_PUSH_ID = 0x0D,
    METADATA = 0x4d,
};

enum CGI_TYPE { CGI, PHPCGI, PHPFPM, FASTCGI, SCGI, };
//----------------------------------------------------------------------
struct Cgi
{
    CGI_TYPE type;

    bool cgi = false;
    bool start = false;
    bool end = false;

    time_t timer;

    pid_t pid = 0;
    std::string path;
    int to_script;
    int from_script;

    long long send_post_data = 0;

    BytesArray headers;

    ~Cgi()
    {
        //fprintf(stderr, "<%s:%d> pid=%d, send_post_data=%lld\n", __func__, __LINE__, pid, send_post_data);
        if (pid)
        {
            kill_chld(pid);
        }

        if (start)
            dec_work_cgi();
        if (cgi)
            dec_all_cgi();
    }
};

struct Stream
{
    Stream *next = NULL;
    Stream *prev = NULL;

    SSL *ssl = NULL;
    int err = 0;

    STREAM_STATUS status;
    unsigned int num_conn;
    unsigned int num_stream = 0;
    long id;

    time_t stream_timer = 0;
    bool wait_write = false;

    HTTP3_FRAME_TYPE frame_type;
    int frame_size = 0;

    std::string method;
    std::string raw_path;
    std::string authority;
    std::string range;
    std::string user_agent;
    std::string referer;
    std::string content_type;
    std::string content_length;

    HTTP_METHOD httpMethod;
    long long post_content_len = 0;

    std::string decode_path;
    std::string full_path;
    std::string query_string;
    std::string decode_query_string;

    SOURCE_DATA source_data = NO_SOURCE;
    int resp_status = RS200;

    BytesArray buf;
    BytesArray headers;
    BytesArray data;

    int fd = -1;
    long long file_size = 0;
    long long offset = 0;
    long long resp_content_len = 0;

    long long all_data_send = 0;
    long long data_send = 0;

    Cgi cgi;

    void set_cgi()
    {
        cgi.cgi = true;
        inc_all_cgi();
        buf.init();
        resp_status = RS200;
        cgi.timer = time(NULL);
    }

    Stream()
    {
        status = READ_HEADERS;
        stream_timer = time(NULL);
    }

    ~Stream()
    {
        //fprintf(stdout, "[%u/%u~]<%s:%d> data=%lld, all_data=%lld, cgi.end=%d\n", num_conn, num_stream, __func__, __LINE__, data_send, all_data_send, cgi.end);
        //fprintf(stderr, "[%u/%u~]<%s:%d> data=%lld, all_data=%lld, cgi.end=%d\n", num_conn, num_stream, __func__, __LINE__, data_send, all_data_send, cgi.end);
        if (ssl)
        {
            SSL_free(ssl);
            ssl = NULL;
        }
    }
};

void print_log(Stream *s);

struct Connect
{
    Connect *next = NULL;
    Connect *prev = NULL;

    Stream *stream_start = NULL;
    Stream *stream_end = NULL;

    unsigned int num_conn = 0;
    unsigned int num_stream = 0;

    int num_work_stream = 0;

    time_t conn_timer = 0;
    bool wait_write = false;

    CONNECT_STATUS status;

    SSL *quic_listener = NULL;
    SSL *ssl_conn = NULL;

    SSL *tmp_stream = NULL;

    SSL *ctrl_ssl = NULL;
    SSL *enc_ssl = NULL;
    SSL *dec_ssl = NULL;

    long long size_send_data = 0;
    long long size_send_frame_data = 0;

    Stream *create_stream(SSL *ssl)
    {
        Stream *s = new(std::nothrow) Stream;
        if (!s)
            return NULL;
        s->ssl = ssl;
        s->num_conn = num_conn;
        s->num_stream = ++num_stream;
        s->id = SSL_get_stream_id(s->ssl);
        s->next = NULL;
        s->prev = stream_end;
        if (stream_end)
            stream_end->next = s;
        stream_end = s;
        if (!stream_start)
            stream_start = s;
        ++num_work_stream;
//fprintf(stderr, "[%u/%d]<%s:%d> Create Stream, work_stream=%d\n", num_conn, num_stream, __func__, __LINE__, num_work_stream);
//fprintf(stdout, "[%u/%d]<%s:%d> Create Stream, work_stream=%d\n", num_conn, num_stream, __func__, __LINE__, num_work_stream);
        return s;
    }

    void delete_stream(Stream *s)
    {
        if (s->status >= SEND_HEADERS)
            print_log(s);
        if (s->prev)
        s->prev->next = s->next;
        else
            stream_start = s->next;

        if (s->next)
            s->next->prev = s->prev;
        else
            stream_end = s->prev;
        //fprintf(stderr, "<%s:%u> cgi.pid=%u, %d/%d\n", __func__, s->num_stream, s->cgi.pid, s->cgi.start, s->cgi.end);
        delete s;
        --num_work_stream;
    }

    Connect()
    {
        status = CONNECT_WAIT;
    }

    ~Connect()
    {
        fprintf(stderr, "[%s]-[%u~]<%s:%d> send_data=%lld, size_frames_data=%lld, work_stream=%d\n", log_time().c_str(), num_conn, __func__, __LINE__, size_send_data, size_send_frame_data, num_work_stream);
        fprintf(stdout, "[%s]-[%u~]<%s:%d> send_data=%lld, size_frames_data=%lld, work_stream=%d\n", log_time().c_str(), num_conn, __func__, __LINE__, size_send_data, size_send_frame_data, num_work_stream);
        if (ctrl_ssl)
            SSL_free(ctrl_ssl);
        if (enc_ssl)
            SSL_free(enc_ssl);
        if (dec_ssl)
            SSL_free(dec_ssl);

        if (stream_start)
        {
            Stream *s = stream_start, *next = NULL;
            for ( ; s; s = next)
            {
                next = s->next;
                delete_stream(s);
            }
        }

        if (ssl_conn)
            SSL_free(ssl_conn);
    }
};

struct Server
{
    Connect *list_start = NULL;
    Connect *list_end = NULL;

    Stream **cgi_stream;
    int cgi_stream_size = 0;

    struct pollfd *poll_fd = NULL;

    int num_accept_conn = 0;

    void add_to_list(Connect *c);
    void close_connect(Connect *c);
    int set_poll();
    void event_loop(SSL *quic_listener, int socket_fd);
    void connect_handler();
    int stream_handler(Connect *c, Stream *s);
    int accept_stream(Connect *c, int stream_num);
    void close_stream(Connect *c, Stream *s);
    int connect_shutdown(Connect *c, const char *func, int line);
    int create_response(Connect *c, Stream *s);

    void close_connections()
    {
        Connect *c = list_start, *next = NULL;
        for ( ; c; c = next)
        {
            next = c->next;
            if (c->status != CONNECT_SHUTDOWN)
            {
                if (connect_shutdown(c, __func__, __LINE__))
                    continue;
                close_connect(c);
            }
        }
    }

    ~Server()
    {
        fprintf(stderr, "~<%s:%d> all_cgi=%d\n", __func__, __LINE__, get_all_cgi());
        if (list_start)
            close_connections();

        if (poll_fd)
        {
            delete [] poll_fd;
            poll_fd = NULL;
        }

        if (cgi_stream)
        {
            delete [] cgi_stream;
            cgi_stream = NULL;
        }
    }
};
//======================================================================
int encode(const char *s_in, char *s_out, int len_out);
int decode(const char *s_in, int len_in, std::string& s_out);
int index_dir(Connect *c, const char *dir_path, const char *uri, BytesArray *html);
//======================== event_loop.cpp ==============================
void signal_handler(int signo);
//============================ ssl.cpp =================================
int create_server_socket(const char *port);
SSL_CTX* InitCTX();
int configure_context(SSL_CTX *ctx);
const char *ssl_strerror(int err);
int ssl_read(SSL *ssl, char *buf, int buf_size, int *err);
int ssl_write(SSL *ssl, const char *buf, int buf_size, int *err);
int ssl_peek(SSL *ssl, char *buf, int buf_size, int *err);
//=========================== http3.cpp ================================
int parse_headers(Stream *s);
int read_head_frame(Stream *s);

int headers_create(Stream *s, int status, int n);
void header_add(Stream *s, int ind);
void header_add(Stream *s, int ind, const char *val);
void header_add(Stream *s, int ind, long long val);
void header_add(Stream *s, const char *name, long long ll);
void header_add(Stream *s, const char *name, const char *val);
void header_add(BytesArray *ba, const char *name, const char *val);
void frame_set_size(BytesArray *ba);
void create_html(BytesArray *ba, const char *msg, const char *title);
void create_error_message(Stream *s, int status, const char *msg);
int cgi_parse_headers(Connect* c, Stream *resp, bool lower_case);
int status_to_index(Stream *s, int status);
int parse_server_headers(BytesArray *ba, int n);
//============================ util.cpp ================================
std::string get_time();
long long file_size(const char *s);
SOURCE_DATA get_source_data(const char *path);
int strlcmp_case(const char *s1, const char *s2, int len);
int strcmp_case(const char *s1, const char *s2);
HTTP_METHOD get_int_method(const char *s);
const char *get_str_method(int i);
const char *get_str_frame_type(HTTP3_FRAME_TYPE t);
const char *get_content_type(const char *s);
int clean_path(std::string& path);
int parse_range(const char *s, long long file_size, long long *offset, long long *content_length);
void hex_print_stderr(const char *s, int line, const void *p, int n);
//============================ config.cp ===============================
int read_conf_file(const char *path_conf);
//============================= log.cpp ================================
void create_logfiles(const std::string& log_dir);
void close_logs();
void print_err(const char *format, ...);
void print_err(Connect *con, const char *format, ...);
//============================= cgi.cpp ================================
int cgi_create_proc(Connect *c, Stream *resp);
int cgi_stdin(Stream *s, int fd);
int cgi_stdout(Stream *s, int fd);
//======================================================================

#endif
