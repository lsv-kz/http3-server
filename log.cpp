#include "http3_server.h"

using namespace std;

static int flog = STDOUT_FILENO, flog_err = STDERR_FILENO;
static mutex mtxLog;
static unsigned int num_log_records = 0, num_logerr_records = 0;

static time_t create_time;
//======================================================================
void create_logfile(const string& log_dir)
{
    char buf[256];
    struct tm tm1;
    time_t t1;

    time(&t1);
    create_time = t1;
    tm1 = *localtime(&t1);
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm1);

    BytesArray file_name;
    file_name.ncpy(log_dir.c_str(), log_dir.size());
    file_name.ncat("/", 1);
    file_name.strcat(buf);
    file_name.ncat("-", 1);
    file_name.ncat(conf->ServerSoftware.c_str(), conf->ServerSoftware.size());
    file_name.strcat(".log");

    flog = open(file_name.ptr(), O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (flog == -1)
    {
        cerr << " Error create log: " << file_name.ptr() << "\n";
        exit(1);
    }
}
//======================================================================
void create_error_logfile(const string& log_dir)
{
    char buf[256];
    struct tm tm1;
    time_t t1;

    time(&t1);
    tm1 = *localtime(&t1);
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm1);

    BytesArray file_name;
    file_name.ncpy(log_dir.c_str(), log_dir.size());
    file_name.strcat("/error_");
    file_name.strcat(buf);
    file_name.ncat("_", 1);
    file_name.ncat(conf->ServerSoftware.c_str(), conf->ServerSoftware.size());
    file_name.strcat(".log");

    flog_err = open(file_name.ptr(), O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (flog_err == -1)
    {
        cerr << "  Error create log_err: " << file_name.ptr() << "\n";
        exit(1);
    }

    dup2(flog_err, STDERR_FILENO);
}

//======================================================================
void close_logs()
{
    close(flog);
    close(flog_err);
}
//======================================================================
void print_err(const char *format, ...)
{
    char buf[300];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    BytesArray str;
    str.reserve(330);
    if (str.error())
    {
        fprintf(stderr, "<%s:%d> Error BytesArray.reserve()\n", __func__, __LINE__);
        return;
    }

    str.ncpy("[", 1);
    str.logtimecat();
    str.ncat("] ", 2);
    str.strcat(buf);
    if (n >= (int)sizeof(buf))
        str.strcat("--- overflow ---\n");

mtxLog.lock();
    write(flog_err, str.ptr(), str.size());
    num_logerr_records++;
    if (num_logerr_records > 500000)
    {
        time_t t = time(NULL);
        if ((t - create_time) <  300)
        {
            close(flog_err);
            flog_err = STDERR_FILENO;
            exit(1);
        }
        else
            create_time = time(NULL);
        close(flog_err);
        create_error_logfile(conf->LogDir);
        num_logerr_records = 0;
    }
mtxLog.unlock();
}
//======================================================================
void print_err(Connect *con, const char *format, ...)
{
    if (!con)
        return;
    char buf[768];
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    BytesArray str;
    str.reserve(1024);
    if (str.error())
    {
        fprintf(stderr, "<%s:%d> Error BytesArray.reserve()\n", __func__, __LINE__);
        return;
    }

    str.ncpy("[", 1);
    str.logtimecat();
    str.ncat("] - [", 5);
    str.cat_int(con->num_conn);
    str.ncat("] ", 2);
    str.strcat(buf);
    if (n >= (int)sizeof(buf))
        str.strcat("--- overflow ---\n");

mtxLog.lock();
    write(flog_err, str.ptr(), str.size());
    num_logerr_records++;
    if (num_logerr_records > 500000)
    {
        time_t t = time(NULL);
        if ((t - create_time) <  300)
        {
            close(flog_err);
            flog_err = STDERR_FILENO;
            exit(1);
        }
        else
            create_time = time(NULL);
        close(flog_err);
        create_error_logfile(conf->LogDir);
        num_logerr_records = 0;
    }
mtxLog.unlock();
}
//======================================================================
void print_log(Connect *c, Stream *s)
{
    if (!c || !s)
        return;
    BytesArray str;
    str.reserve(1024);
    if (str.error())
    {
        fprintf(stderr, "<%s:%d> Error BytesArray.reserve()\n", __func__, __LINE__);
        return;
    }

    str.cpy_int(s->num_conn);
    str.ncat("/", 1);
    str.cat_int(s->num_stream);
    str.ncat(" - [", 4);
    str.logtimecat();
    str.ncat("] \"", 3);
    if (s->httpMethod != M_NULL)
    {
        str.strcat(get_str_method(s->httpMethod));
        str.ncat(" ", 1);
    }

    if (s->decode_path.c_str())
    {
        str.strcat(s->decode_path.c_str());
        if (s->decode_query_string.size())
        {
            str.ncat("?", 1);
            str.ncat(s->decode_query_string.c_str(), s->decode_query_string.size());
        }
    }

    str.strcat(" HTTP/3\" ");
    if (s->resp_status)
    {
        str.cat_int(s->resp_status);
        str.ncat(" ", 1);
        str.cat_int(s->data_send);
    }

    str.ncat(" \"", 2);
    if (s->referer.size())
        str.ncat(s->referer.c_str(), s->referer.size());
    else
        str.ncat("-", 1);
    str.ncat("\" \"", 3);
    if (s->user_agent.size())
        str.ncat(s->user_agent.c_str(), s->user_agent.size());
    else
        str.ncat("-", 1);
    str.ncat("\" - id=", 7);
    str.cat_int(s->id);
    str.ncat(" \n", 2);

mtxLog.lock();
    write(flog, str.ptr(), str.size());
    num_log_records++;
    if (num_log_records > 500000)
    {
        close(flog);
        create_logfile(conf->LogDir);
        num_log_records = 0;
    }
mtxLog.unlock();
}
//======================================================================
void create_logfiles(const string& log_dir)
{
    create_logfile(log_dir);
    create_error_logfile(log_dir);
}
