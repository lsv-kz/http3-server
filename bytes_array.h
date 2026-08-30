#ifndef BYTESARRAY_H_
#define BYTESARRAY_H_

#include <iostream>
#include <cstring>
#include <sys/time.h>

extern const unsigned int array_reserve;
//======================================================================
class BytesArray
{
    char int_buf[21];
    char *buf;
    unsigned int buf_size;
    unsigned int buf_len;
    int offset;
    int err;
    //------------------------------------------------------------------
    char *ll_to_string(long long ll)
    {
        if (err)
            return NULL;
        int cnt = 20, minus = (ll < 0) ? 1 : 0;
        const char *get_char = "9876543210123456789";
    
        int_buf[cnt] = 0;
        while (cnt > 0)
        {
            int_buf[--cnt] = get_char[9 + ll % 10];
            ll /= 10;
            if (ll == 0)
                break;
        }
    
        if ((minus) && (cnt > 0))
            int_buf[--cnt] = '-';
    
        return int_buf + cnt;
    }
    //------------------------------------------------------------------
    BytesArray(const BytesArray&);
    BytesArray& operator=(const BytesArray&);

public:

    BytesArray()
    {
        buf = NULL;
        buf_size = buf_len = 0;
        offset = 0;
        err = 0;
    }
    //------------------------------------------------------------------
    ~BytesArray()
    {
        if (buf)
        {
            delete [] buf;
            buf = NULL;
        }
    }
    //------------------------------------------------------------------
    void init()
    {
        buf_len = 0;
        offset = 0;
        err = 0;
    }
    //------------------------------------------------------------------
    int reserve(unsigned int new_size)
    {
        if (err)
            return -1;
        if ((new_size <= buf_size) || (new_size == 0))
            return buf_size;
        new_size += array_reserve;
        char *new_ptr = new(std::nothrow) char [new_size + 1];
        if (new_ptr == NULL)
        {
            fprintf(stderr, "<%s:%d> Error new char [%d]\n", __func__, __LINE__, new_size);
            err = -1;
            return -1;
        }

        if (buf)
        {
            if (buf_len > 0)
                memcpy(new_ptr, buf, buf_len + 1);
            delete [] buf;
        }

        buf = new_ptr;
        buf_size = new_size;
        return 0;
    }
    //------------------------------------------------------------------
    int ncpy(const char *b, unsigned int len)
    {
        if (err)
        {
            fprintf(stderr, "<%s:%d> error=%d\n", __func__, __LINE__, err);
            return -1;
        }

        buf_len = offset = 0;
        if ((b == NULL) || (len == 0))
            return 0;

        if (len > buf_size)
        {
            if (reserve(len))
                return -1;
        }

        memcpy(buf, b, len);
        buf_len = len;
        return 0;
    }
    //------------------------------------------------------------------
    int ncat(const char *b, unsigned int len)
    {
        if (err)
        {
            fprintf(stderr, "<%s:%d> error=%d\n", __func__, __LINE__, err);
            return -1;
        }

        if ((b == NULL) || (len == 0))
            return 0;

        if ((buf_len + len) > buf_size)
        {
            if (reserve(buf_len + len))
                return -1;
        }

        memcpy(buf + buf_len, b, len);
        buf_len += len;
        return 0;
    }
    //------------------------------------------------------------------
    int bytecat(const char ch)
    {
        if (err)
            return -1;
        if ((buf_len + 1) > buf_size)
        {
            if (reserve(buf_len + 1))
                return -1;
        }

        buf[buf_len] = ch;
        buf_len += 1;
        return 0;
    }
    //------------------------------------------------------------------
    int cpy_int(long long ll) { return strcpy(ll_to_string(ll)); }
    //------------------------------------------------------------------
    int cat_int(long long ll) { return strcat(ll_to_string(ll)); }
    //------------------------------------------------------------------
    int strcpy(const char *s)
    {
        if (err)
            return -1;
        buf_len = offset = 0;
        if (s == NULL)
            return 0;
        unsigned int len = strlen(s);
        if (len)
            return ncpy(s, len);
        return 0;
    }
    //------------------------------------------------------------------
    int strcat(const char *s)
    {
        if (err)
            return -1;
        if (s == NULL)
            return 0;
        unsigned int len = strlen(s);
        if (len)
            return ncat(s, len);
        else
            return 0;
    }
    //------------------------------------------------------------------
    void timecat()
    {
        if (err)
            return;
        struct tm t;
        char s[40];
        time_t now = time(NULL);
    
        gmtime_r(&now, &t);
        unsigned int len = strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S %Z", &t);
        ncat(s, len);
    }
    //------------------------------------------------------------------
    void logtimecat()
    {
        if (err)
            return;
        struct tm t;
        char s[40];
        time_t now = time(NULL);
    
        localtime_r(&now, &t);
        unsigned int len = strftime(s, sizeof(s), "%d/%b/%Y:%H:%M:%S %Z", &t);
        ncat(s, len);
    }
    //------------------------------------------------------------------
    const char *ptr()
    {
        if (buf)
        {
            buf[buf_len] = 0;
            return buf;
        }
        else
            return "";
    }
    //------------------------------------------------------------------
    const char *ptr_remain()
    {
        if (buf)
        {
            buf[buf_len] = 0;
            return buf + offset;
        }
        else
            return "";
    }
    //------------------------------------------------------------------
    int truncation(unsigned int n)
    {
        if (n >= buf_len)
            return buf_len;
        buf_len = n;
        offset = 0;
        return buf_len;
    }
    //------------------------------------------------------------------
    int get_byte(unsigned int i)
    {
        if ((i >= buf_len) || err)
            return -1;
        return (unsigned char)buf[i];
    }
    //------------------------------------------------------------------
    int set_byte(char ch, unsigned int i)
    {
        if ((i >= buf_len) || (buf_size == 0) || err)
            return -1;
        else
        {
            buf[i] = ch;
            return 0;
        }
    }
    //------------------------------------------------------------------
    unsigned int size() { return buf_len; }
    unsigned int capacity() { return buf_size; }
    unsigned int size_remain() { return buf_len - offset; }
    unsigned int get_offset() { return offset; }
    int error() { return err; }

    int inc_offset(unsigned int n)
    {
        if ((offset + n) > buf_len)
        {
            fprintf(stderr, "<%s:%d> Error new offset=%u, buf_len=%u\n", __func__, __LINE__, offset + n, buf_len);
            return -1;
        }
        else if (n == 0)
        {
            fprintf(stderr, "<%s:%d> n=%u\n", __func__, __LINE__, n);
            return offset;
        }

        offset += n;
        return offset;
    }
};

#endif
