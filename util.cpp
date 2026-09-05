#include "http3_server.h"

using namespace std;
//======================================================================
string get_time()
{
    struct tm t;
    char s[40];
    time_t now = time(NULL);

    gmtime_r(&now, &t);
    strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S %Z", &t);
    return s;
}
//======================================================================
string log_time()
{
    struct tm t;
    char s[40];
    time_t now = time(NULL);

    localtime_r(&now, &t);
    strftime(s, sizeof(s), "%d/%b/%Y:%H:%M:%S %Z", &t);
    return s;
}
//======================================================================
long long file_size(const char *s)
{
    struct stat st;

    if (!stat(s, &st))
        return st.st_size;
    else
        return -1;
}
//======================================================================
SOURCE_DATA get_source_data(const char *path)
{
    struct stat st;
    int ret = lstat(path, &st);
    if (ret == -1)
    {
        return NO_SOURCE;
    }

    if (S_ISDIR(st.st_mode))
        return DIRECTORY;
    else if (S_ISREG(st.st_mode))
        return FROM_FILE;
    else
        return NO_SOURCE;
}
//======================================================================
int strlcmp_case(const char *s1, const char *s2, int len)
{
    char c1, c2;

    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    int diff = ('a' - 'A');

    for (; len > 0; --len, ++s1, ++s2)
    {
        c1 = *s1;
        c2 = *s2;
        if (!c1 && !c2) return 0;

        c1 += (c1 >= 'A') && (c1 <= 'Z') ? diff : 0;
        c2 += (c2 >= 'A') && (c2 <= 'Z') ? diff : 0;

        if (c1 != c2) return (c1 - c2);
    }

    return 0;
}
//======================================================================
int strcmp_case(const char *s1, const char *s2)
{
    char c1, c2;

    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    int diff = ('a' - 'A');

    for (; (*s1) && (*s2); ++s1, ++s2)
    {
        c1 = *s1;
        c2 = *s2;
        if (!c1 && !c2) return 0;

        c1 += (c1 >= 'A') && (c1 <= 'Z') ? diff : 0;
        c2 += (c2 >= 'A') && (c2 <= 'Z') ? diff : 0;

        if (c1 != c2) return (c1 - c2);
    }

    return (*s1 - *s2);
}
//======================================================================
HTTP_METHOD get_int_method(const char *s)
{
    if (!strlcmp_case(s, "GET", 3))
        return M_GET;
    else if (!strlcmp_case(s, "POST", 4))
        return M_POST;
    else if (!strlcmp_case(s, "HEAD", 4))
        return M_HEAD;
    else
        return M_NULL;
}
//======================================================================
const char *get_str_method(int i)
{
    switch (i)
    {
        case M_GET:
            return "GET";
        case M_POST:
            return "POST";
        case M_HEAD:
            return "HEAD";
    }

    return "";
}
//======================================================================
const char *get_str_frame_type(HTTP3_FRAME_TYPE t)
{
    switch (t)
    {
        case DATA: // 0
            return "DATA";
        case HEADERS: // 1
            return "HEADERS";
        case CANCEL_PUSH: // 3
            return "CANCEL_PUSH";
        case SETTINGS: // 4
            return "SETTINGS";
        case PUSH_PROMISE: // 5
            return "PUSH_PROMISE";
        case GOAWAY: // 7
            return "GOAWAY";
        case ORIGIN: // 12 (0xC)
            return "ORIGIN";
        case MAX_PUSH_ID: // 13 0x0D,
            return "MAX_PUSH_ID";
        case METADATA: //  (0x4d)
            return "METADATA";
    }

    return "?";
}
//======================================================================
const char *get_cgi_type(CGI_TYPE t)
{
    switch (t)
    {
        case CGI:
            return "CGI";
        case PHPCGI:
            return "PHPCGI";
        case PHPFPM:
            return "PHPFPM";
        case FASTCGI:
            return "FASTCGI";
        case SCGI:
            return "SCGI";
    }

    return "?";
}
//======================================================================
const char *get_content_type(const char *s)
{
    const char *p = strrchr(s, '.');

    if (!p)
        goto end;

    //       video
    if (!strlcmp_case(p, ".ogv", 4))
        return "video/ogg";
    else if (!strlcmp_case(p, ".mp4", 4))
        return "video/mp4";
    else if (!strlcmp_case(p, ".avi", 4))
        return "video/x-msvideo";
    else if (!strlcmp_case(p, ".mov", 4))
        return "video/quicktime";
    else if (!strlcmp_case(p, ".mkv", 4))
        return "video/x-matroska";
    else if (!strlcmp_case(p, ".flv", 4))
        return "video/x-flv";
    else if (!strlcmp_case(p, ".mpeg", 5) || !strlcmp_case(p, ".mpg", 4))
        return "video/mpeg";
    else if (!strlcmp_case(p, ".asf", 4))
        return "video/x-ms-asf";
    else if (!strlcmp_case(p, ".wmv", 4))
        return "video/x-ms-wmv";
    else if (!strlcmp_case(p, ".swf", 4))
        return "application/x-shockwave-flash";
    else if (!strlcmp_case(p, ".3gp", 4))
        return "video/video/3gpp";

    //       sound
    else if (!strlcmp_case(p, ".mp3", 4))
        return "audio/mpeg";
    else if (!strlcmp_case(p, ".wav", 4))
        return "audio/x-wav";
    else if (!strlcmp_case(p, ".ogg", 4))
        return "audio/ogg";
    else if (!strlcmp_case(p, ".pls", 4))
        return "audio/x-scpls";
    else if (!strlcmp_case(p, ".aac", 4))
        return "audio/aac";
    else if (!strlcmp_case(p, ".aif", 4))
        return "audio/x-aiff";
    else if (!strlcmp_case(p, ".ac3", 4))
        return "audio/ac3";
    else if (!strlcmp_case(p, ".voc", 4))
        return "audio/x-voc";
    else if (!strlcmp_case(p, ".flac", 5))
        return "audio/flac";
    else if (!strlcmp_case(p, ".amr", 4))
        return "audio/amr";
    else if (!strlcmp_case(p, ".au", 3))
        return "audio/basic";

    //       image
    else if (!strlcmp_case(p, ".gif", 4))
        return "image/gif";
    else if (!strlcmp_case(p, ".svg", 4) || !strlcmp_case(p, ".svgz", 5))
        return "image/svg+xml";
    else if (!strlcmp_case(p, ".png", 4))
        return "image/png";
    else if (!strlcmp_case(p, ".ico", 4))
        return "image/vnd.microsoft.icon";
    else if (!strlcmp_case(p, ".jpeg", 5) || !strlcmp_case(p, ".jpg", 4))
        return "image/jpeg";
    else if (!strlcmp_case(p, ".djvu", 5) || !strlcmp_case(p, ".djv", 4))
        return "image/vnd.djvu";
    else if (!strlcmp_case(p, ".tiff", 5))
        return "image/tiff";
    //       text
    else if (!strlcmp_case(p, ".txt", 4))
        return "text/plain; charset=UTF-8";
    else if (!strlcmp_case(p, ".html", 5) || !strlcmp_case(p, ".htm", 4) || !strlcmp_case(p, ".shtml", 6))
        return "text/html; charset=UTF-8";
    else if (!strlcmp_case(p, ".css", 4))
        return "text/css";

    //       application
    else if (!strlcmp_case(p, ".pdf", 4))
        return "application/pdf";
    else if (!strlcmp_case(p, ".gz", 3))
        return "application/gzip";
end:
    return NULL;
}
//======================================================================
int clean_path(string& path)
{
    unsigned int num_subfolder = 0;
    const unsigned int max_subfolder = 20;
    int arr[max_subfolder];
    int len = path.size(), i = 0, j = 0;

    for ( ; j < len; )
    {
        if ((path.substr(j, 3) == "/..") && ((len - j) == 3))
        {
            if (num_subfolder)
            {
                i = arr[--num_subfolder];
                i++;
                break;
            }
            else
                return -1;
        }
        if ((path.substr(j, 2) == "/.") && ((len - j) == 2))
        {
            path[i++] = '/';
            break;
        }
        else if (path.substr(j, 4) == "/../")
        {
            if (num_subfolder)
                i = arr[--num_subfolder];
            else
                return -1;
            j += 3;
        }
        else if (path.substr(j, 2) == "//")
            j++;
        else if (path.substr(j, 3) == "/./")
            j += 2;
        else if (path.substr(j, 2) == "/.")
        {
            if (path[j + 2] == '.')
            {
                if (num_subfolder < max_subfolder)
                    arr[num_subfolder++] = i;
                else
                    return -1;
                path[i++] = path[j++];
                path[i++] = path[j++];
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error: Hidden File\n", __func__, __LINE__);
                return -1;
            }
        }
        else
        {
            if (path[j] == '/')
            {
                if (num_subfolder < max_subfolder)
                    arr[num_subfolder++] = i;
                else
                    return -1;
            }

            path[i++] = path[j++];
        }
    }

    path.resize(i);
    return i;
}
//======================================================================
int parse_range(const char *s, long long file_size, long long *offset, long long *content_length)
{
    const char *p = strchr(s, '=');
    if (p == NULL)
        return -1;
    if (*(p + 1) == '-')
    {
        p += 2;
        char buf[32];
        buf[0] = 0;
        int i = 0;
        for ( ; *p; )
        {
            char ch = *p;
            if (isdigit(ch))
            {
                buf[i++] = ch;
                ++p;
            }
            else
                break;
        }
        buf[i] = 0;

        if (*p != 0)
            return 0;
        sscanf(buf, "%lld", content_length);
        if (*content_length > file_size)
            *content_length = file_size;
        *offset = file_size - *content_length;
    }
    else
    {
        ++p;
        char buf[32];
        buf[0] = 0;
        int i = 0;
        for ( ; *p; )
        {
            char ch = *p;
            if (isdigit(ch))
            {
                buf[i++] = ch;
                ++p;
            }
            else
                break;
        }
        buf[i] = 0;

        if (*p != '-')
            return 0;
        sscanf(buf, "%lld", offset);
        if (*offset >= file_size)
            return -RS416;
        ++p;
        buf[0] = 0;
        i = 0;
        for ( ; *p; )
        {
            char ch = *p;
            if (isdigit(ch))
            {
                buf[i++] = ch;
                ++p;
            }
            else
                break;
        }
        buf[i] = 0;

        if (*p != 0)
            return 0;

        if (strlen(buf) == 0)
        {
            if (*offset > (file_size - 1))
                return -RS416;
            *content_length = file_size - *offset;
        }
        else
        {
            long long end = 0;
            sscanf(buf, "%lld", &end);
            if (end > (file_size - 1))
                end = file_size - 1;
            *content_length = end + 1 - *offset;
        }
    }

    return 1;
}
//======================================================================
void hex_print_stderr(const char *s, int line, const void *p, int n)
{
    int count, addr = 0, col;
    unsigned char *buf = (unsigned char*)p;
    char str[18];
    fprintf(stderr, "<%s:%d>--------------------------------\n", s, line);
    for(count = 0; count < n;)
    {
        fprintf(stderr, "%08X  ", addr);
        for(col = 0, addr = addr + 0x10; (count < n) && (col < 16); count++, col++)
        {
            if (col == 8) fprintf(stderr, " ");
            fprintf(stderr, "%02X ", *(buf+count));
            str[col] = (*(buf + count) >= 32 && *(buf + count) < 127) ? *(buf + count) : '.';
        }
        str[col] = 0;
        if (col <= 8) fprintf(stderr, " ");
        fprintf(stderr, "%*s  %s\n",(16 - (col)) * 3, "", str);
    }

    //fprintf(stderr, "\n");
}
