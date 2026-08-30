#include "http3_server.h"
#include <algorithm>
#include <vector>
#include <dirent.h>

using namespace std;
//======================================================================
static bool isimage(const char *name)
{
    const char *p;

    if (!(p = strrchr(name, '.')))
        return false;

    if (!strlcmp_case(p, ".gif", 4))
        return true;
    else if (!strlcmp_case(p, ".png", 4))
        return true;
    else if (!strlcmp_case(p, ".svg", 4))
        return true;
    else if (!strlcmp_case(p, ".jpeg", 5) || !strlcmp_case(p, ".jpg", 4))
        return true;
    return false;
}
//======================================================================
static bool isaudio(const char *name)
{
    const char *p;

    if (!(p = strrchr(name, '.')))
        return false;

    if (!strlcmp_case(p, ".wav", 4))
        return true;
    else if (!strlcmp_case(p, ".mp3", 4))
        return true;
    else if (!strlcmp_case(p, ".ogg", 4))
        return true;
    return false;
}
//======================================================================
static bool isvideo(const char *name)
{
    const char *p;

    if (!(p = strrchr(name, '.')))
        return false;
    if (!strlcmp_case(p, ".mp4", 4))
        return true;
    else if (!strlcmp_case(p, ".webm", 4))
        return true;
    else if (!strlcmp_case(p, ".ogv", 4))
        return true;
    return false;
}
//======================================================================
static bool cmp(const string &a, const string &b)
{
    unsigned int n1, n2;
    bool i;

    if (((n1 = atoi(a.c_str())) > 0) && ((n2 = atoi(b.c_str())) > 0))
    {
        if (n1 < n2)
            i = 1;
        else if (n1 == n2)
            i = a < b;
        else
            i = 0;
    }
    else
        i = a < b;

    return i;
}
//======================================================================
static int create_index_html(Connect *c, vector<string>& list, int num_files, const char *dir_path, const char *uri, BytesArray *html)
{
    int dir_path_len = strlen(dir_path);
    char *file_path = (char*)malloc(dir_path_len + NAME_MAX + 1);
    if (file_path == NULL)
    {
        print_err(c, "<%s:%d>  Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
        return -RS500;
    }

    memcpy(file_path, dir_path, dir_path_len);
    //------------------------------------------------------------------
    html->strcpy("<!DOCTYPE HTML>\r\n"
            "<html>\r\n"
            " <head>\r\n"
            "  <meta charset=\"UTF-8\">\r\n"
            "  <title>Index of ");
    html->strcat(uri);
    html->strcat("</title>\r\n"
            "  <style>\r\n"
            "    body {\r\n"
            "      margin-left:100px;\r\n"
            "      margin-right:50px;\r\n"
            "      background-color: rgb(122, 122, 82);\r\n"
            "      color: rgb(200,240,120);\r\n"
            "    }\r\n"
            "    a {\r\n"
            "      text-decoration: none;\r\n"
            "      color: rgb(255, 209, 26);\r\n"
            "    }\r\n"
            "    h3 {\r\n"
            "      color: rgb(200,240,120);\r\n"
            "    }\r\n"
            "  </style>\r\n"
            "  <link href=\"styles.css\" type=\"text/css\" rel=\"stylesheet\">\r\n"
            " </head>\r\n"
            " <body id=\"top\">\r\n"
            "  <h3>Index of ");
    html->strcat(uri);
    html->strcat("</h3>\r\n"
            "  <table border=\"0\" width=\"100\%\">\r\n"
            "   <tr><td><h3>Directories</h3></td></tr>\r\n");
    //------------------------------------------------------------------
     if (!strcmp(uri, "/"))
        html->strcat("   <tr><td></td></tr>\r\n");
    else
        html->strcat("   <tr><td><a href=\"../\">Parent Directory/</a></td></tr>\r\n");
    //-------------------------- Directories ---------------------------
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        char buf[1024];
        struct stat st;
        memcpy(file_path + dir_path_len, (*it).c_str(), (*it).size() + 1);
        int n = lstat(file_path, &st);
        if ((n == -1) || !S_ISDIR (st.st_mode))
            continue;

        if (!encode((*it).c_str(), buf, sizeof(buf)))
        {
            print_err(c, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        html->strcat("   <tr><td><a href=\"");
        html->strcat(buf);
        html->strcat("/\">");
        html->strcat((*it).c_str());
        html->strcat("/</a></td></tr>\r\n");
    }
    //------------------------------------------------------------------
    html->strcat("  </table>\r\n   <hr>\r\n  <table border=\"0\" width=\"100\%\">\r\n"
                "   <tr><td><h3>Files</h3></td><td></td></tr>\r\n");
    //---------------------------- Files -------------------------------
    for (auto it = list.begin(); it != list.end(); ++it)
    {
        char buf[1024];
        struct stat st;
        memcpy(file_path + dir_path_len, (*it).c_str(), (*it).size() + 1);
        int n = lstat(file_path, &st);
        if ((n == -1) || !S_ISREG (st.st_mode))
            continue;
        else if ((*it) == "favicon.ico")
            continue;

        if (!encode((*it).c_str(), buf, sizeof(buf)))
        {
            print_err(c, "<%s:%d> Error: encode()\n", __func__, __LINE__);
            continue;
        }

        if (isimage((*it).c_str()) && conf->ShowMediaFiles)
        {
            html->strcat("   <tr><td><a href=\"");
            html->strcat(buf);
            html->strcat("\"><img src=\"");
            html->strcat(buf);
            html->strcat("\" width=\"100\"></a>");
            html->strcat((*it).c_str());
            html->strcat("</td><td align=\"right\">");
            html->cat_int(st.st_size);
            html->strcat(" bytes</td></tr>\r\n");
        }
        else if (isaudio((*it).c_str()) && conf->ShowMediaFiles)
        {
            html->strcat("   <tr><td><audio preload=\"none\" controls src=\"");
            html->strcat(buf);
            html->strcat("\"></audio><a href=\"");
            html->strcat(buf);
            html->strcat("\">");
            html->strcat((*it).c_str());
            html->strcat("</a></td><td align=\"right\">");
            html->cat_int(st.st_size);
            html->strcat(" bytes</td></tr>\r\n");
        }
        else if (isvideo((*it).c_str()) && conf->ShowMediaFiles)
        {
            html->strcat("   <tr><td><video width=\"320\" preload=\"none\" controls src=\"");
            //html->strcat("   <tr><td><video preload=\"none\" controls src=\"");
            html->strcat(buf);
            html->strcat("\"></video><a href=\"");
            html->strcat(buf);
            html->strcat("\">");
            html->strcat((*it).c_str());
            html->strcat("</a></td><td align=\"right\">");
            html->cat_int(st.st_size);
            html->strcat(" bytes</td></tr>\r\n");
        }
        else
        {
            html->strcat("   <tr><td><a href=\"");
            html->strcat(buf);
            html->strcat("\">");
            html->strcat((*it).c_str());
            html->strcat("</a></td><td align=\"right\">");
            html->cat_int(st.st_size);
            html->strcat(" bytes</td></tr>\r\n");
        }
    }
    //------------------------------------------------------------------
    html->strcat("  </table>\r\n"
              "  <hr>\r\n  ");
    html->timecat();
    html->strcat("\r\n"
              "  <a href=\"#top\" style=\"display:block;\r\n"
              "         position:fixed;\r\n"
              "         bottom:30px;\r\n"
              "         left:10px;\r\n"
              "         width:50px;\r\n"
              "         height:40px;\r\n"
              "         font-size:60px;\r\n"
              "         background:gray;\r\n"
              "         border-radius:10px;\r\n"
              "         color:black;\r\n"
              "         opacity: 0.7\">^</a>\r\n"
              " </body>\r\n"
              "</html>");
    free(file_path);
fprintf(stderr, "<%s:%d> -----------------------\n", __func__, __LINE__);
    return 0;
}
//======================================================================
int index_dir(Connect *c, const char *dir_path, const char *uri, BytesArray *html)
{
    DIR *dir;
    struct dirent *dirbuf;
    const int max_num_files = 1024;
    int num_files = 0;

    if (dir_path == NULL)
    {
        print_err(c, "<%s:%d>  Error dir_path = NULL\n", __func__, __LINE__);
        return -RS500;
    }

    vector<string> list;
    list.reserve(max_num_files);

    dir = opendir(dir_path);
    if (dir == NULL)
    {
        if (errno == EACCES)
            return -RS403;
        else
        {
            print_err(c, "<%s:%d>  Error opendir(\"%s\"): %s\n", __func__, __LINE__, dir_path, strerror(errno));
            return -RS500;
        }
    }

    while ((dirbuf = readdir(dir)))
    {
        if (dirbuf->d_name[0] == '.')
            continue;
        list.push_back(dirbuf->d_name);
        ++num_files;
    }

    closedir(dir);
    sort(list.begin(), list.end(), cmp);
    html->reserve(num_files * 100);
    return create_index_html(c, list, num_files, dir_path, uri, html);
}
