#include "bytes_array.h"
//======================================================================
const unsigned char huffman_decode_table[256] = {
     48,  49,  50,  97,  99, 101, 105, 111, 115, 116,
     32,  37,  45,  46,  47,  51,  52,  53,  54,  55,
     56,  57,  61,  65,  95,  98, 100, 102, 103, 104,
    108, 109, 110, 112, 114, 117,  58,  66,  67,  68,
     69,  70,  71,  72,  73,  74,  75,  76,  77,  78,
     79,  80,  81,  82,  83,  84,  85,  86,  87,  89,
    106, 107, 113, 118, 119, 120, 121, 122,  38,  42,
     44,  59,  88,  90,  33,  34,  40,  41,  63,  39,
     43, 124,  35,  62,   0,  36,  64,  91,  93, 126,
     94, 125,  60,  96, 123,  92, 195, 208, 128, 130,
    131, 162, 184, 194, 224, 226, 153, 161, 167, 172,
    176, 177, 179, 209, 216, 217, 227, 229, 230, 129,
    132, 133, 134, 136, 146, 154, 156, 160, 163, 164,
    169, 170, 173, 178, 181, 185, 186, 187, 189, 190,
    196, 198, 228, 232, 233,   1, 135, 137, 138, 139,
    140, 141, 143, 147, 149, 150, 151, 152, 155, 157,
    158, 165, 166, 168, 174, 175, 180, 182, 183, 188,
    191, 197, 231, 239,   9, 142, 144, 145, 148, 159,
    171, 206, 215, 225, 236, 237, 199, 207, 234, 235,
    192, 193, 200, 201, 202, 205, 210, 213, 218, 219,
    238, 240, 242, 243, 255, 203, 204, 211, 212, 214,
    221, 222, 223, 241, 244, 245, 246, 247, 248, 250,
    251, 252, 253, 254,   2,   3,   4,   5,   6,   7,
      8,  11,  12,  14,  15,  16,  17,  18,  19,  20,
     21,  23,  24,  25,  26,  27,  28,  29,  30,  31,
    127, 220, 249,  10,  13,  22,
};
//======================================================================
const unsigned int huffman_encode_table[256][2] = {
    { 0x00001ff8, 13}, { 0x007fffd8, 23}, { 0x0fffffe2, 28}, { 0x0fffffe3, 28},
    { 0x0fffffe4, 28}, { 0x0fffffe5, 28}, { 0x0fffffe6, 28}, { 0x0fffffe7, 28},
    { 0x0fffffe8, 28}, { 0x00ffffea, 24}, { 0x3ffffffc, 30}, { 0x0fffffe9, 28},
    { 0x0fffffea, 28}, { 0x3ffffffd, 30}, { 0x0fffffeb, 28}, { 0x0fffffec, 28},
    { 0x0fffffed, 28}, { 0x0fffffee, 28}, { 0x0fffffef, 28}, { 0x0ffffff0, 28},
    { 0x0ffffff1, 28}, { 0x0ffffff2, 28}, { 0x3ffffffe, 30}, { 0x0ffffff3, 28},
    { 0x0ffffff4, 28}, { 0x0ffffff5, 28}, { 0x0ffffff6, 28}, { 0x0ffffff7, 28},
    { 0x0ffffff8, 28}, { 0x0ffffff9, 28}, { 0x0ffffffa, 28}, { 0x0ffffffb, 28},
    { 0x00000014,  6}, { 0x000003f8, 10}, { 0x000003f9, 10}, { 0x00000ffa, 12},
    { 0x00001ff9, 13}, { 0x00000015,  6}, { 0x000000f8,  8}, { 0x000007fa, 11},
    { 0x000003fa, 10}, { 0x000003fb, 10}, { 0x000000f9,  8}, { 0x000007fb, 11},
    { 0x000000fa,  8}, { 0x00000016,  6}, { 0x00000017,  6}, { 0x00000018,  6},
    { 0x00000000,  5}, { 0x00000001,  5}, { 0x00000002,  5}, { 0x00000019,  6},
    { 0x0000001a,  6}, { 0x0000001b,  6}, { 0x0000001c,  6}, { 0x0000001d,  6},
    { 0x0000001e,  6}, { 0x0000001f,  6}, { 0x0000005c,  7}, { 0x000000fb,  8},
    { 0x00007ffc, 15}, { 0x00000020,  6}, { 0x00000ffb, 12}, { 0x000003fc, 10},
    { 0x00001ffa, 13}, { 0x00000021,  6}, { 0x0000005d,  7}, { 0x0000005e,  7},
    { 0x0000005f,  7}, { 0x00000060,  7}, { 0x00000061,  7}, { 0x00000062,  7},
    { 0x00000063,  7}, { 0x00000064,  7}, { 0x00000065,  7}, { 0x00000066,  7},
    { 0x00000067,  7}, { 0x00000068,  7}, { 0x00000069,  7}, { 0x0000006a,  7},
    { 0x0000006b,  7}, { 0x0000006c,  7}, { 0x0000006d,  7}, { 0x0000006e,  7},
    { 0x0000006f,  7}, { 0x00000070,  7}, { 0x00000071,  7}, { 0x00000072,  7},
    { 0x000000fc,  8}, { 0x00000073,  7}, { 0x000000fd,  8}, { 0x00001ffb, 13},
    { 0x0007fff0, 19}, { 0x00001ffc, 13}, { 0x00003ffc, 14}, { 0x00000022,  6},
    { 0x00007ffd, 15}, { 0x00000003,  5}, { 0x00000023,  6}, { 0x00000004,  5},
    { 0x00000024,  6}, { 0x00000005,  5}, { 0x00000025,  6}, { 0x00000026,  6},
    { 0x00000027,  6}, { 0x00000006,  5}, { 0x00000074,  7}, { 0x00000075,  7},
    { 0x00000028,  6}, { 0x00000029,  6}, { 0x0000002a,  6}, { 0x00000007,  5},
    { 0x0000002b,  6}, { 0x00000076,  7}, { 0x0000002c,  6}, { 0x00000008,  5},
    { 0x00000009,  5}, { 0x0000002d,  6}, { 0x00000077,  7}, { 0x00000078,  7},
    { 0x00000079,  7}, { 0x0000007a,  7}, { 0x0000007b,  7}, { 0x00007ffe, 15},
    { 0x000007fc, 11}, { 0x00003ffd, 14}, { 0x00001ffd, 13}, { 0x0ffffffc, 28},
    { 0x000fffe6, 20}, { 0x003fffd2, 22}, { 0x000fffe7, 20}, { 0x000fffe8, 20},
    { 0x003fffd3, 22}, { 0x003fffd4, 22}, { 0x003fffd5, 22}, { 0x007fffd9, 23},
    { 0x003fffd6, 22}, { 0x007fffda, 23}, { 0x007fffdb, 23}, { 0x007fffdc, 23},
    { 0x007fffdd, 23}, { 0x007fffde, 23}, { 0x00ffffeb, 24}, { 0x007fffdf, 23},
    { 0x00ffffec, 24}, { 0x00ffffed, 24}, { 0x003fffd7, 22}, { 0x007fffe0, 23},
    { 0x00ffffee, 24}, { 0x007fffe1, 23}, { 0x007fffe2, 23}, { 0x007fffe3, 23},
    { 0x007fffe4, 23}, { 0x001fffdc, 21}, { 0x003fffd8, 22}, { 0x007fffe5, 23},
    { 0x003fffd9, 22}, { 0x007fffe6, 23}, { 0x007fffe7, 23}, { 0x00ffffef, 24},
    { 0x003fffda, 22}, { 0x001fffdd, 21}, { 0x000fffe9, 20}, { 0x003fffdb, 22},
    { 0x003fffdc, 22}, { 0x007fffe8, 23}, { 0x007fffe9, 23}, { 0x001fffde, 21},
    { 0x007fffea, 23}, { 0x003fffdd, 22}, { 0x003fffde, 22}, { 0x00fffff0, 24},
    { 0x001fffdf, 21}, { 0x003fffdf, 22}, { 0x007fffeb, 23}, { 0x007fffec, 23},
    { 0x001fffe0, 21}, { 0x001fffe1, 21}, { 0x003fffe0, 22}, { 0x001fffe2, 21},
    { 0x007fffed, 23}, { 0x003fffe1, 22}, { 0x007fffee, 23}, { 0x007fffef, 23},
    { 0x000fffea, 20}, { 0x003fffe2, 22}, { 0x003fffe3, 22}, { 0x003fffe4, 22},
    { 0x007ffff0, 23}, { 0x003fffe5, 22}, { 0x003fffe6, 22}, { 0x007ffff1, 23},
    { 0x03ffffe0, 26}, { 0x03ffffe1, 26}, { 0x000fffeb, 20}, { 0x0007fff1, 19},
    { 0x003fffe7, 22}, { 0x007ffff2, 23}, { 0x003fffe8, 22}, { 0x01ffffec, 25},
    { 0x03ffffe2, 26}, { 0x03ffffe3, 26}, { 0x03ffffe4, 26}, { 0x07ffffde, 27},
    { 0x07ffffdf, 27}, { 0x03ffffe5, 26}, { 0x00fffff1, 24}, { 0x01ffffed, 25},
    { 0x0007fff2, 19}, { 0x001fffe3, 21}, { 0x03ffffe6, 26}, { 0x07ffffe0, 27},
    { 0x07ffffe1, 27}, { 0x03ffffe7, 26}, { 0x07ffffe2, 27}, { 0x00fffff2, 24},
    { 0x001fffe4, 21}, { 0x001fffe5, 21}, { 0x03ffffe8, 26}, { 0x03ffffe9, 26},
    { 0x0ffffffd, 28}, { 0x07ffffe3, 27}, { 0x07ffffe4, 27}, { 0x07ffffe5, 27},
    { 0x000fffec, 20}, { 0x00fffff3, 24}, { 0x000fffed, 20}, { 0x001fffe6, 21},
    { 0x003fffe9, 22}, { 0x001fffe7, 21}, { 0x001fffe8, 21}, { 0x007ffff3, 23},
    { 0x003fffea, 22}, { 0x003fffeb, 22}, { 0x01ffffee, 25}, { 0x01ffffef, 25},
    { 0x00fffff4, 24}, { 0x00fffff5, 24}, { 0x03ffffea, 26}, { 0x007ffff4, 23},
    { 0x03ffffeb, 26}, { 0x07ffffe6, 27}, { 0x03ffffec, 26}, { 0x03ffffed, 26},
    { 0x07ffffe7, 27}, { 0x07ffffe8, 27}, { 0x07ffffe9, 27}, { 0x07ffffea, 27},
    { 0x07ffffeb, 27}, { 0x0ffffffe, 28}, { 0x07ffffec, 27}, { 0x07ffffed, 27},
    { 0x07ffffee, 27}, { 0x07ffffef, 27}, { 0x07fffff0, 27}, { 0x03ffffee, 26},
};

const unsigned char mask[8] = {0, 1, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f};
void print_err(const char *format, ...);
//======================================================================
static int find_char(const unsigned long long in, char *ch)
{
    int len = 0;
    int decode_table_index = 0;
    if (in < 0x5000000000000000)
    {
        decode_table_index = in >> (27+32);
        len = 5;
    }
    else if (in < 0xb800000000000000)
    {
        decode_table_index = (in >> (26+32)) - 10;
        len = 6;
    }
    else if (in < 0xf800000000000000)
    {
        decode_table_index = ((in >> (25+32)) & 0x3f) + 8;
        len = 7;
    }
    else if (in < 0xfe00000000000000)
    {
        decode_table_index = ((in >> (24+32)) & 0x07) + 68;
        len = 8;
    }
    else if (in < 0xff40000000000000)
    {
        decode_table_index = ((in >> (22+32)) & 0x07) + 74;
        len = 10;
    }
    else if (in < 0xffa0000000000000)
    {
        decode_table_index = ((in >> (21+32)) & 0x07) + 77;
        len = 11;
    }
    else if (in < 0xffc0000000000000)
    {
        decode_table_index = ((in & 0x10000000000000) ? 1 : 0) + 82;
        len = 12;
    }
    else if (in < 0xfff0000000000000)
    {
        decode_table_index = ((in >> (19+32)) & 0x07) + 84;
        len = 13;
    }
    else if (in < 0xfff8000000000000)
    {
        decode_table_index = ((in & 0x4000000000000) ? 1 : 0) + 90;
        len = 14;
    }
    else if (in < 0xfffe000000000000)
    {
        decode_table_index = ((in >> (17+32)) & 0x03) + 92;
        len = 15;
    }
    else if (in < 0xfffe600000000000)
    {
        decode_table_index = ((in >> (13+32)) & 0x03) + 95;
        len = 19;
    }
    else if (in < 0xfffee00000000000)
    {
        decode_table_index = ((in >> (12+32)) & 0x0f) + 92;
        len = 20;
    }
    else if (in < 0xffff480000000000)
    {
        decode_table_index = ((in >> (11+32)) & 0x3f) + 78;
        len = 21;
    }
    else if (in < 0xffffb00000000000)
    {
        decode_table_index = ((in >> (10+32)) & 0x3f) + 101;
        len = 22;
    }
    else if (in < 0xffffea0000000000)
    {
        decode_table_index = ((in >> (9+32)) & 0x3f) + 121;
        len = 23;
    }
    else if (in < 0xfffff60000000000)
    {
        decode_table_index = ((in >> (8+32)) & 0x1f) + 164;
        len = 24;
    }
    else if (in < 0xfffff80000000000)
    {
        decode_table_index = ((in >> (7+32)) & 0x03) + 186;
        len = 25;
    }
    else if (in < 0xfffffbc000000000)
    {
        decode_table_index = ((in >> (6+32)) & 0x0f) + 190;
        len = 26;
    }
    else if (in < 0xfffffe2000000000)
    {
        decode_table_index = ((in >> (5+32)) & 0x3f) + 175;
        len = 27;
    }
    else if (in < 0xfffffff000000000)
    {
        decode_table_index = ((in >> (4+32)) & 0x1f) + 222;
        len = 28;
    }
    else if (in < 0xfffffffc00000000)
    {
        decode_table_index = ((in >> (2+32)) & 0x03) + 253;
        len = 30;
    }
    else // in >= 0xfffffffc00000000
    {
        print_err("<%s:%d> Error {in = 0x%llx} >= 0xfffffffc00000000\n", __func__, __LINE__, in);
        return 0;
    }

    if ((decode_table_index >= 0) && (decode_table_index < 256))
        *ch = huffman_decode_table[decode_table_index];
    else
    {
        print_err("<%s:%d> Error decode_table_index=%d, in=%llu\n", __func__, __LINE__, decode_table_index, in);
        len = 0;
    }

    return len;
}
//======================================================================
int huffman_decode(const char *s, int len, std::string& out)
{
    out = "";
    if ((s == NULL) || (len == 0))
        return 0;
    const int max_bits = 64;

    unsigned long long huff_buf = 0;
    int num_free_bits = max_bits;

    unsigned long long buf = 0;
    int out_len = 0;

    for ( ; (len > 0) || num_free_bits; )
    {
        for ( ; (num_free_bits > 32) && (len > 0); )
        {
            buf = *((unsigned char*)s++);
            len--;
            huff_buf |= buf<<(num_free_bits - 8);
            num_free_bits -= 8;
        }

        int size = max_bits - num_free_bits;
        if (size < 8)
        {
            if (size < 5)
            {
                if (size == 0)
                    return out_len;
                else if ((size == 1) && (huff_buf == 0x8000000000000000))
                    return out_len;
                else if ((size == 2) && (huff_buf == 0xc000000000000000))
                    return out_len;
                else if ((size == 3) && (huff_buf == 0xe000000000000000))
                    return out_len;
                else if ((size == 4) && (huff_buf == 0xf000000000000000))
                    return out_len;
                else
                {
                    print_err("<%s:%d>Error size=%d, %llb\n", __func__, __LINE__, size, huff_buf);
                    return 0;
                }
            }
            else if ((size == 5) && (huff_buf == 0xf800000000000000))
                return out_len;
            else if ((size == 6) && (huff_buf == 0xfc00000000000000))
                return out_len;
            else if ((size == 7) && (huff_buf == 0xfe00000000000000))
                return out_len;
        }

        char ch;
        int code_len = find_char(huff_buf & 0xffffffffffffffff, &ch);
        if ((code_len > 0) && (code_len <= size))
        {
            out += ch;
            out_len++;
            huff_buf = huff_buf<<code_len;
            num_free_bits += code_len;
        }
        else
            return 0;
    }

    return 0;
}
//======================================================================
int huffman_encode(const char *in, BytesArray& out)
{
    out.init();
    unsigned int index = 0;
    long long huff_buf = 0;
    int huff_buf_len = 0;
    int out_len = 0;

    while (*in)
    {
        index = (unsigned char)*(in++);
        int bits_len = huffman_encode_table[index][1];
        huff_buf = huff_buf<<bits_len | huffman_encode_table[index][0];
        huff_buf_len += bits_len;

        while (huff_buf_len >= 8)
        {
            out.bytecat((unsigned char)(huff_buf>>(huff_buf_len - 8)));
            out_len++;
            huff_buf_len -= 8;
        }
    }

    if (huff_buf_len > 0)
    {
        unsigned char uch = 0xff>>huff_buf_len;
        out.bytecat(uch | (huff_buf<<(8 - huff_buf_len)));
        out_len++;
    }

    return out_len;
}
//======================================================================
int huffman_encode(const char *in, int len, BytesArray& out)
{
    out.init();
    unsigned int index = 0;
    long long huff_buf = 0;
    int huff_buf_len = 0;
    int out_len = 0;

    while (len > 0)
    {
        index = (unsigned char)*(in++);
        len--;
        int bits_len = huffman_encode_table[index][1];
        huff_buf = huff_buf<<bits_len | huffman_encode_table[index][0];
        huff_buf_len += bits_len;

        while (huff_buf_len >= 8)
        {
            out.bytecat((unsigned char)(huff_buf>>(huff_buf_len - 8)));
            out_len++;
            huff_buf_len -= 8;
        }
    }

    if (huff_buf_len > 0)
    {
        unsigned char uch = 0xff>>huff_buf_len;
        out.bytecat(uch | (huff_buf<<(8 - huff_buf_len)));
        out_len++;
    }

    return out_len;
}
