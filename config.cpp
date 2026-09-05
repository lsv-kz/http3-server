#include "http3_server.h"

using namespace std;

static Config c;
const Config* const conf = &c;
//======================================================================
enum { CURRENT_DIR, PARENT_DIR, ADD_FOLDER };
//======================================================================
static int parse_folder(const char *s, int i)// "../", "/../", "/.."
{
    if (!strncmp(s, "../", i) )
        return PARENT_DIR;
    else if (!strncmp(s, "/../", i))
        return PARENT_DIR;
    else if (!strncmp(s, "/..", i))
        return PARENT_DIR;
    else if (!strncmp(s, "./", i))
        return CURRENT_DIR;
    else if (!strncmp(s, "/.", i))
        return CURRENT_DIR;
    else if (!strncmp(s, "/./", i))
        return CURRENT_DIR;
    return ADD_FOLDER;
}
//======================================================================
static void *memrchr_(const void *s, char c, int n)
{
    if (n > 0)
    {
        const char *p = (const char *)s + n;
        while (--n > 0)
        {
            if (*(--p) == c)
                return (void *)p;
        }
    }
    return NULL;
}
//======================================================================
static int absolute_path(string& path)
{
    struct stat st;
    int ret = stat(path.c_str(), &st);
    if (ret == -1)
    {
        fprintf(stderr, "<%s:%d> Error stat(%s): %s\n", __func__, __LINE__, path.c_str(), strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "<%s:%d> [%s] is not directory\n", __func__, __LINE__, path.c_str());
        return -1;
    }

    if (path[0] == '/')
    {
        if (path[path.size() - 1] == '/')
            path.resize(path.size() - 1);

        return 0;
    }

    char cwd[8192];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        fprintf(stderr, "<%s:%d> Error getcwd(): %s\n", __func__, __LINE__, strerror(errno));
        return -1;
    }

    int cwd_len = strlen(cwd);
    if (cwd[cwd_len - 1] == '/')
    {
        cwd[cwd_len - 1] = 0;
        cwd_len--;
    }

    int path_len = path.size();
    int i_path = 0;
    int anchor[2] = {0};
    char ch;
    char prev_ch = '\0';

    while (path_len > 0)
    {
        --path_len;
        anchor[1] = i_path;
        ch = path[i_path++];

        if (ch == '/')
        {
            if (prev_ch != '/')
            {
                int ret = parse_folder(path.c_str() + anchor[0], anchor[1] - anchor[0] + 1);
                if (ret == PARENT_DIR)// [/../], [../]
                {
                    char *p = (char*)memrchr_(cwd, '/', cwd_len);
                    if (p)
                    {
                        if (p == cwd)
                        {
                            cwd_len = 1;
                            cwd[cwd_len] = 0;
                        }
                        else
                        {
                            *p = 0;
                            cwd_len = p - cwd;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "<%s:%d> Error: '/' not found\n", __func__, __LINE__);
                        return -1;
                    }
                }
                else if (ret == ADD_FOLDER)
                {
                    int len = anchor[1] - anchor[0];
                    if (path[anchor[0]] != '/')
                    {
                        if ((cwd_len + len + 1) >= (int)sizeof(cwd))
                        {
                            fprintf(stderr, "<%s:%d> Error: danger of overflow (%d => %d)\n", __func__, __LINE__, cwd_len + len + 1, (int)sizeof(cwd));
                            return -1;
                        }
                        *(cwd + cwd_len) = '/';
                        ++cwd_len;
                        cwd[cwd_len] = 0;
                    }
                    else
                    {
                        if ((cwd_len + len) >= (int)sizeof(cwd))
                        {
                            fprintf(stderr, "<%s:%d> Error: danger of overflow (%d => %d)\n", __func__, __LINE__, cwd_len + len, (int)sizeof(cwd));
                            return -1;
                        }
                    }

                    memcpy(cwd + cwd_len, path.c_str() + anchor[0], len);
                    cwd_len += len;
                    *(cwd + cwd_len) = 0;
                }
            }

            anchor[0] = anchor[1];
        }
        else
        {
            if (path_len == 0)
            {
                int ret = parse_folder(path.c_str() + anchor[0], anchor[1] - anchor[0] + 1);
                if (ret == PARENT_DIR)// [/../], [../]
                {
                    char *p = (char*)memrchr_(cwd, '/', cwd_len);
                    if (p)
                    {
                        if (p == cwd)
                        {
                            cwd_len = 1;
                            cwd[cwd_len] = 0;
                        }
                        else
                        {
                            *p = 0;
                            cwd_len = p - cwd;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "<%s:%d> Error: '/' not found\n", __func__, __LINE__);
                        return -1;
                    }
                }
                else if (ret == ADD_FOLDER)
                {
                    int len = anchor[1] - anchor[0] + 1;
                    if (path[anchor[0]] != '/')
                    {
                        if ((cwd_len + len + 1) >= (int)sizeof(cwd))
                        {
                            fprintf(stderr, "<%s:%d> Error: danger of overflow (%d => %d)\n", __func__, __LINE__, cwd_len + len + 1, (int)sizeof(cwd));
                            return -1;
                        }
                        *(cwd + cwd_len) = '/';
                        ++cwd_len;
                        cwd[cwd_len] = 0;
                    }
                    else
                    {
                        if ((cwd_len + len) >= (int)sizeof(cwd))
                        {
                            fprintf(stderr, "<%s:%d> Error: danger of overflow (%d => %d)\n", __func__, __LINE__, cwd_len + len, (int)sizeof(cwd));
                            return -1;
                        }
                    }

                    memcpy(cwd + cwd_len, path.c_str() + anchor[0], len);
                    cwd_len += len;
                    *(cwd + cwd_len) = 0;
                }
            }
        }

        prev_ch = ch;
    }

    path = cwd;
    if (stat(path.c_str(), &st) == -1)
    {
        fprintf(stderr, "<%s:%d> Error stat(%s): %s\n", __func__, __LINE__, path.c_str(), strerror(errno));
        return -1;
    }
    return 0;
}
//======================================================================
static int line_ = 1, line_inc = 0;
static char bracket = 0;
//----------------------------------------------------------------------
static int getLine(FILE *f, char *str, int size)
{
    if ((str == NULL) || (size == 0))
    {
        return -1;
    }

    int offset = 0;
    str[offset] = 0;

    if (bracket)
    {
        str[offset++] = bracket;
        str[offset] = 0;
        bracket = 0;
        return 1;
    }

    int ch, len = 0, numWords = 0, wr = 1, wrSpace = 0;

    if (line_inc)
    {
        ++line_;
        line_inc = 0;
    }

    while (((ch = getc(f)) != EOF))
    {
        if (ch == '\n')
        {
            if (len)
            {
                line_inc = 1;
                return ++numWords;
            }
            else
            {
                ++line_;
                wr = 1;
                offset = 0;
                str[offset] = 0;
                wrSpace = 0;
                continue;
            }
        }
        else if (wr == 0)
            continue;
        else if ((ch == ' ') || (ch == '\t'))
        {
            if (len)
                wrSpace = 1;
        }
        else if (ch == '#')
            wr = 0;
        else if ((ch == '{') || (ch == '}'))
        {
            if (len)
                bracket = (char)ch;
            else
            {
                str[offset++] = (char)ch;
                str[offset] = 0;
                ++len;
            }
            return ++numWords;
        }
        else if (ch != '\r')
        {
            if (wrSpace)
            {
                str[offset++] = ' ';
                str[offset] = 0;
                ++len;
                ++numWords;
                wrSpace = 0;
            }

            str[offset++] = (char)ch;
            str[offset] = 0;
            ++len;
        }
    }

    if (len)
        return ++numWords;
    return -1;
}
//======================================================================
static int is_number(const char *s)
{
    if (!s)
        return 0;
    int n = isdigit((int)*(s++));
    while (*s && n)
        n = isdigit((int)*(s++));
    return n;
}
//======================================================================
static int find_bracket(FILE *f, char c)
{
    if (bracket)
    {
        if (c != bracket)
        {
            bracket = 0;
            return 0;
        }

        bracket = 0;
        return 1;
    }

    int ch, grid = 0;
    if (line_inc)
    {
        ++line_;
        line_inc = 0;
    }

    while (((ch = getc(f)) != EOF))
    {
        if (ch == '#')
        {
            grid = 1;
        }
        else if (ch == '\n')
        {
            grid = 0;
            ++line_;
        }
        else if ((ch == '{') && (grid == 0))
            return 1;
        else if ((ch != ' ') && (ch != '\t') && (grid == 0))
            return 0;
    }

    return 0;
}
//======================================================================
static void create_fcgi_list(fcgi_list_addr **l, const char *s1, const char *s2, CGI_TYPE type)
{
    if (l == NULL)
    {
        fprintf(stderr, "<%s:%d> Error pointer = NULL\n", __func__, __LINE__);
        exit(errno);
    }

    fcgi_list_addr *t;
    try
    {
        t = new fcgi_list_addr;
    }
    catch (...)
    {
        fprintf(stderr, "<%s:%d> Error new(): %s\n", __func__, __LINE__, strerror(errno));
        exit(errno);
    }

    t->script_name = s1;
    t->addr = s2;
    t->type = type;
    t->next = *l;
    *l = t;
}
//======================================================================
static int read_conf_file(FILE *fconf)
{
    char str[1024];

    int n;
    while ((n = getLine(fconf, str, sizeof(str) - 1)) > 0)
    {
        if (n == 2)
        {
            char s1[512], s2[512];
            if (sscanf(str, "%s %s", s1, s2) != 2)
            {
                fprintf(stderr, "<%s:%d> Error sscanf(%s) != 2\n", __func__, __LINE__, str);
                return -1;
            }

            if (!strcmp(s1, "ServerSoftware"))
                c.ServerSoftware = s2;
            else if (!strcmp(s1, "ServerAddr"))
                c.ServerAddr = s2;
            else if (!strcmp(s1, "ServerPort"))
                c.ServerPort = s2;
            else if (!strcmp(s1, "DocumentRoot"))
                c.DocumentRoot = s2;
            else if (!strcmp(s1, "ScriptDir"))
                c.ScriptDir = s2;
            else if (!strcmp(s1, "LogDir"))
                c.LogDir = s2;
            else if (!strcmp(s1, "Certificate"))
                c.Certificate = s2;
            else if (!strcmp(s1, "CertificateKey"))
                c.CertificateKey = s2;
            else if (!strcmp(s1, "UsePHP"))
            {
                if ((!strcmp(s2, "off")) || (!strcmp(s2, "php-fpm")) || (!strcmp(s2, "php-cgi")))
                    c.UsePHP = s2;
                else
                {
                    fprintf(stderr, "<%s:%d> Error read config file: [%s], line <%d>\n", __func__, __LINE__, str, line_);
                    return -1;
                }
            }
            else if (!strcmp(s1, "PathPHP"))
                c.PathPHP = s2;
            else if ((!strcmp(s1, "MaxAcceptConnections")) && is_number(s2))
                c.MaxAcceptConnections = atoi(s2);
            else if ((!strcmp(s1, "MaxStreams")) && is_number(s2))
                c.MaxStreams = atoi(s2);
            else if (!strcmp(s1, "ServerNameIndication"))
            {
                if (!strcmp_case(s2, "on"))
                    c.ServerNameIndication = true;
                else if (!strcmp_case(s2, "off"))
                    c.ServerNameIndication = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n",
                            __func__, __LINE__, line_, str);
                    return -1;
                }
            }
            else if ((!strcmp(s1, "TimeOut")) && is_number(s2))
                c.TimeOut = atoi(s2);
            else if ((!strcmp(s1, "TimeoutCGI")) && is_number(s2))
                c.TimeoutCGI = atoi(s2);
            else if ((!strcmp(s1, "MaxCgiProc")) && is_number(s2))
                c.MaxCgiProc = atoi(s2);
            else if (!strcmp(s1, "ShowMediaFiles"))
            {
                if (!strcmp_case(s2, "on"))
                    c.ShowMediaFiles = true;
                else if (!strcmp_case(s2, "off"))
                    c.ShowMediaFiles = false;
                else
                {
                    fprintf(stderr, "<%s:%d> Error config file line <%d> \"%s\": [on | off]\n",
                            __func__, __LINE__, line_, str);
                    return -1;
                }
            }
            else
            {
                fprintf(stderr, "<%s:%d> Error read config file: [%s], line <%d>\n", __func__, __LINE__, str, line_);
                return -1;
            }
        }
        else if (n == 1)
        {
            if (!strcmp(str, "ServerSoftware"))
                c.ServerSoftware = "";
            else if (!strcmp(str, "scgi"))
            {
                if (find_bracket(fconf, '{') == 0)
                {
                    fprintf(stderr, "<%s:%d> Error not found \"{\", line <%d>\n", __func__, __LINE__, line_);
                    return -1;
                }

                while (getLine(fconf, str, sizeof(str) - 1) == 2)
                {
                    char s1[512], s2[512];
                    if (sscanf(str, "%s %s", s1, s2) != 2)
                    {
                        fprintf(stderr, "<%s:%d> Error sscanf(%s) != 2\n", __func__, __LINE__, str);
                        return -1;
                    }
                    create_fcgi_list(&c.fcgi_list, s1, s2, SCGI);
                }

                if (strcmp(str, "}"))
                {
                    fprintf(stderr, "<%s:%d> Error not found \"}\", line <%d>\n", __func__, __LINE__, line_);
                    return -1;
                }
            }
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error read config file: [%s], line <%d>\n", __func__, __LINE__, str, line_);
            return -1;
        }
    }

    if (!feof(fconf))
    {
        fprintf(stderr, "<%s:%d> Error read config file\n", __func__, __LINE__);
        return -1;
    }

    fclose(fconf);
    //------------------------------------------------------------------
    if (absolute_path(c.DocumentRoot) == -1)
    {
        fprintf(stderr, "<%s:%d> !!! Error DocumentRoot [%s]\n", __func__, __LINE__, conf->DocumentRoot.c_str());
        return -1;
    }
    //------------------------------------------------------------------
    if (absolute_path(c.LogDir) == -1)
    {
        fprintf(stderr, "<%s:%d> !!! Error LogDir [%s]\n", __func__, __LINE__, conf->LogDir.c_str());
        return -1;
    }
    //------------------------------------------------------------------
    if (absolute_path(c.ScriptDir) == -1)
    {
        fprintf(stderr, "<%s:%d> !!! Error ScriptDir [%s]\n", __func__, __LINE__, conf->ScriptDir.c_str());
        return -1;
    }

    return 0;
}
//======================================================================
int read_conf_file(const char *path_conf)
{
    FILE *fconf = fopen(path_conf, "r");
    if (!fconf)
    {
        if (errno == ENOENT)
            fprintf(stderr, " Error: config file %s not found\n", path_conf);
        else
            fprintf(stderr, "<%s:%d> Error fopen(%s): %s\n", __func__, __LINE__, path_conf, strerror(errno));
        return -1;
    }

    return read_conf_file(fconf);
}
