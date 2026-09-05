#include "http3_server.h"

using namespace std;
//======================================================================
unsigned char proto_alpn[] = { 2, 'h', '3' };
const char *server_name = "localhost";
//======================================================================
SSL_CTX* InitCTX()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    
    SSL_CTX *ctx = SSL_CTX_new(OSSL_QUIC_server_method());
    if (!ctx)
    {
        fprintf(stderr, "<%s:%d> Error SSL_CTX_new()\n", __func__, __LINE__);
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    if (configure_context(ctx))
    {
        SSL_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}
//======================================================================
static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out, unsigned char *outlen,
                                const unsigned char *in, unsigned int inlen, void *arg)
{
    hex_print_stderr("client", __LINE__, in, inlen);
    unsigned int proto_alpn_len = sizeof(proto_alpn);
    //hex_print_stderr("server", __LINE__, proto_alpn, proto_alpn_len);
    for ( unsigned int i = 0; i < proto_alpn_len; i += (unsigned int)(proto_alpn[i] + 1))
    {
        for (unsigned int j = 0; j < inlen; j += (unsigned int)(in[j] + 1))
        {
            if (in[j] != proto_alpn[i])
                continue;
            if (memcmp(&in[j], &proto_alpn[i], in[j] + 1) == 0)
            {
                *out = (unsigned char *)&in[j + 1];
                *outlen = in[j];
                return SSL_TLSEXT_ERR_OK;
            }
        }
    }

    return SSL_TLSEXT_ERR_NOACK;
}
//======================================================================
int sni_callback(SSL *ssl, int *al, void *arg)
{
    //fprintf(stderr, "<%s:%d> --------------------\n", __func__, __LINE__);
    const char *servname = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    if (servname)
        fprintf(stderr, "<%s:%d> servername from client: [%s]\n", __func__, __LINE__, servname);
    else
    {
        fprintf(stderr, "<%s:%d> servername from client: NULL\n", __func__, __LINE__);
        //return SSL_TLSEXT_ERR_ALERT_FATAL;
        return SSL_TLSEXT_ERR_OK;
    }

    if (arg == NULL)
        return SSL_TLSEXT_ERR_OK;
    else
        fprintf(stderr, "<%s:%d> servername from server: [%s]\n", __func__, __LINE__, (char*)arg);

    char *s = (char*)arg;
    if (strcmp(servname, s) == 0)
        return SSL_TLSEXT_ERR_OK;

    return SSL_TLSEXT_ERR_ALERT_FATAL; //  SSL_TLSEXT_ERR_ALERT_FATAL (SSL_ERROR_UNRECOGNIZED_NAME_ALERT)
}
//======================================================================
int configure_context(SSL_CTX *ctx)
{
    if (SSL_CTX_use_certificate_file(ctx, conf->Certificate.c_str(), SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, conf->CertificateKey.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        fprintf(stderr, "<%s:%d> Error loading certificates\n", __func__, __LINE__);
        return 1;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, NULL);

    if (conf->ServerNameIndication)
    {
        SSL_CTX_set_tlsext_servername_callback(ctx, sni_callback);
        SSL_CTX_set_tlsext_servername_arg(ctx, (void*)server_name);
    }

    return 0;
}
//======================================================================
const char *ssl_strerror(int err)
{
    switch (err)
    {
        case SSL_ERROR_NONE: // 0
            return "SSL_ERROR_NONE";
        case SSL_ERROR_SSL:  // 1
            return "SSL_ERROR_SSL";
        case SSL_ERROR_WANT_READ:  // 2
            return "SSL_ERROR_WANT_READ";
        case SSL_ERROR_WANT_WRITE:  // 3
            return "SSL_ERROR_WANT_WRITE";
        case SSL_ERROR_WANT_X509_LOOKUP:  // 4
            return "SSL_ERROR_WANT_X509_LOOKUP";
        case SSL_ERROR_SYSCALL:  // 5
            return "SSL_ERROR_SYSCALL";
        case SSL_ERROR_ZERO_RETURN:  // 6
            return "SSL_ERROR_ZERO_RETURN";
        case SSL_ERROR_WANT_CONNECT:  // 7
            return "SSL_ERROR_WANT_CONNECT";
        case SSL_ERROR_WANT_ACCEPT:  // 8
            return "SSL_ERROR_WANT_ACCEPT";
    }
    
    return "?";
}
//======================================================================
int ssl_read(SSL *ssl, char *buf, int buf_size, int *err)
{
    if ((ssl == NULL) || (buf == NULL) || (buf_size <= 0))
    {
        fprintf(stderr, "<%s:%d> Error conn=%p, buf=%p, len=%d\n", __func__, __LINE__, ssl, buf, buf_size);
        return -1;
    }

    int ret = SSL_read(ssl, buf, buf_size);
    if (ret > 0)
    {
        return ret;
    }
    else
    {
        *err = SSL_get_error(ssl, ret);
        if (*err == SSL_ERROR_ZERO_RETURN)
        {
            fprintf(stderr, "<%s:%d> Error SSL_read(): SSL_ERROR_ZERO_RETURN\n", __func__, __LINE__);
            return -1;
        }
        else if (*err == SSL_ERROR_WANT_READ)
        {
            //fprintf(stderr, "<%s:%d> Error SSL_read(): SSL_ERROR_WANT_READ\n", __func__, __LINE__);
            return 0;
        }
        else if (*err == SSL_ERROR_WANT_WRITE)
        {
            fprintf(stderr, "<%s:%d> Error SSL_read(): SSL_ERROR_WANT_WRITE\n", __func__, __LINE__);
            return 0;
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error SSL_read(, , %d)=%d: %s\n", __func__, __LINE__, buf_size, ret, ssl_strerror(*err));
            return -1;
        }
    }
}
//======================================================================
int ssl_write(SSL *ssl, const char *buf, int buf_size, int *err)
{
    ERR_clear_error();
    if ((ssl == NULL) || (buf == NULL) || (buf_size <= 0))
    {
        fprintf(stderr, "<%s:%d> Error conn=%p, buf=%p, len=%d\n", __func__, __LINE__, ssl, buf, buf_size);
        return -1;
    }

    int ret = SSL_write(ssl, buf, buf_size);
    if (ret <= 0)
    {
        *err = SSL_get_error(ssl, ret);
        if (*err == SSL_ERROR_WANT_WRITE)
        {
            //fprintf(stderr, "<%s:%d> Error SSL_write(): SSL_ERROR_WANT_WRITE\n", __func__, __LINE__);
            return ERR_TRY_AGAIN;
        }
        else if (*err == SSL_ERROR_WANT_READ)
        {
            fprintf(stderr, "<%s:%d> Error SSL_write(): SSL_ERROR_WANT_READ\n", __func__, __LINE__);
            return ERR_TRY_AGAIN;
        }
        fprintf(stderr, "<%s:%d> Error SSL_write()=%d: %s\n", __func__, __LINE__, ret, ssl_strerror(*err));
        return -1;
    }

    return ret;
}
//======================================================================
int ssl_peek(SSL *ssl, char *buf, int buf_size, int *err)
{
    ERR_clear_error();
    if ((ssl == NULL) || (buf == NULL) || (buf_size <= 0))
    {
        fprintf(stderr, "<%s:%d> Error conn=%p, buf=%p, len=%d\n", __func__, __LINE__, ssl, buf, buf_size);
        return -1;
    }

    int ret = SSL_peek(ssl, buf, buf_size);
    if (ret > 0)
    {
        return ret;
    }
    else
    {
        *err = SSL_get_error(ssl, ret);
        if (*err == SSL_ERROR_ZERO_RETURN)
        {
            fprintf(stderr, "<%s:%d> Error SSL_peek(): SSL_ERROR_ZERO_RETURN\n", __func__, __LINE__);
            return -1;
        }
        else if (*err == SSL_ERROR_WANT_READ)
        {
            //fprintf(stderr, "<%s:%d> Error SSL_peek(): SSL_ERROR_WANT_READ\n", __func__, __LINE__);
            return 0;
        }
        else if (*err == SSL_ERROR_WANT_WRITE)
        {
            fprintf(stderr, "<%s:%d> Error SSL_peek(): SSL_ERROR_WANT_WRITE\n", __func__, __LINE__);
            return 0;
        }
        else
        {
            fprintf(stderr, "<%s:%d> Error SSL_peek(, , %d)=%d: %s\n", __func__, __LINE__, buf_size, ret, ssl_strerror(*err));
            return -1;
        }
    }
}
