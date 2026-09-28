#include "../Include/image.h"
#include "../Include/stdint.h"
#include "../Include/gfx.h"
#include "../Include/fat32.h"
#include "../Include/memory.h"
#include "../Include/string.h"

#define IMAGE_MAX_BYTES (64u * 1024u * 1024u)
#define HUFFMAN_TABLE_BITS 15
#define HUFFMAN_TABLE_SIZE (1u << HUFFMAN_TABLE_BITS)

static image_error_t image_last_error = IMG_OK;

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static uint32_t bit_reverse(uint32_t v,int bits)
{
    uint32_t r = 0;
    int i;

    for(i = 0;i < bits;i++)
    {
        r = (r << 1) | (v & 1u);
        v >>= 1;
    }

    return r;
}

typedef struct
{
    const uint8_t *data;
    uint32_t size;
    uint32_t pos;
    uint32_t bits;
    int count;
} BIT_READER;

static uint32_t br_peek(BIT_READER *br,int n)
{
    while(br->count < n)
    {
        uint32_t b = 0;

        if(br->pos < br->size)
            b = br->data[br->pos++];

        br->bits |= b << br->count;
        br->count += 8;
    }

    return br->bits & ((1u << n) - 1u);
}

static uint32_t br_read(BIT_READER *br,int n)
{
    uint32_t v = br_peek(br,n);

    br->bits >>= n;
    br->count -= n;

    return v;
}

static void br_align(BIT_READER *br)
{
    int n = br->count & 7;

    if(n)
    {
        br->bits >>= n;
        br->count -= n;
    }
}

typedef struct
{
    uint16_t *table;
} HUFFMAN;

static int huffman_build(HUFFMAN *h,const uint8_t *lengths,int count)
{
    uint16_t *table;
    uint16_t bl_count[16];
    uint16_t next_code[16];
    uint32_t codes[288];
    int i;
    int bits;

    table = (uint16_t *)malloc(
        HUFFMAN_TABLE_SIZE * sizeof(uint16_t)
    );

    if(!table)
        return -1;

    memset(
        table,
        0,
        HUFFMAN_TABLE_SIZE * sizeof(uint16_t)
    );

    memset(bl_count,0,sizeof(bl_count));
    memset(next_code,0,sizeof(next_code));
    memset(codes,0,sizeof(codes));

    for(i = 0;i < count;i++)
    {
        if(lengths[i] > 15)
        {
            free(table);
            return -1;
        }

        if(lengths[i])
            bl_count[lengths[i]]++;
    }

    {
        uint32_t code = 0;

        for(bits = 1;bits <= 15;bits++)
        {
            code = (code + bl_count[bits - 1]) << 1;
            next_code[bits] = (uint16_t)code;
        }
    }

    for(i = 0;i < count;i++)
    {
        int len = lengths[i];

        if(len)
        {
            codes[i] = bit_reverse(next_code[len],len);
            next_code[len]++;
        }
    }

    for(i = 0;i < count;i++)
    {
        int len = lengths[i];
        uint32_t code;
        uint32_t step;
        uint32_t index;

        if(!len)
            continue;

        code = codes[i];
        step = 1u << len;

        for(index = code;index < HUFFMAN_TABLE_SIZE;index += step)
        {
            uint32_t n;
            uint16_t entry;

            n = index;

            // File: Kernel/SourceI/image.c (huffman_build: BUG FIX #2 -
            // khi nhan ban entry ra nhieu o trong bang (vi code ngan hon
            // HUFFMAN_TABLE_BITS), phai nhay CACH NHAU (1<<len) o
            // (table[n + (j<<len)]), KHONG PHAI lien ke (table[n+j]).
            // Chi co cac bit CAO (ngoai len bit thap = ma Huffman) duoc
            // phep thay doi; ghi lien ke lam sai gan het bang tra, khien
            // hau het PNG nen bang Huffman (gan nhu moi PNG thuc te) bi
            // giai ma sai/that bai.
            if(len <= HUFFMAN_TABLE_BITS)
            {
                uint32_t repeat = 1u << (HUFFMAN_TABLE_BITS - len);
                uint32_t j;

                entry = (uint16_t)(((uint16_t)len << 9) | (uint16_t)i);

                for(j = 0;j < repeat;j++)
                {
                    uint32_t idx = n + (j << len);

                    if(idx < HUFFMAN_TABLE_SIZE)
                        table[idx] = entry;
                }

                break;
            }
        }
    }

    h->table = table;
    return 0;
}

static void huffman_free(HUFFMAN *h)
{
    if(h && h->table)
    {
        free(h->table);
        h->table = 0;
    }
}

static int huffman_decode(BIT_READER *br,HUFFMAN *h)
{
    uint32_t index;
    uint16_t entry;
    int len;

    if(!h || !h->table)
        return -1;

    index = br_peek(br,HUFFMAN_TABLE_BITS);
    entry = h->table[index];

    if(!entry)
        return -1;

    len = entry >> 9;

    br_read(br,len);

    return entry & 0x1FF;
}

static const uint16_t length_base[29] =
{
    3,4,5,6,7,8,9,10,
    11,13,15,17,
    19,23,27,31,
    35,43,51,59,
    67,83,99,115,
    131,163,195,227,
    258
};

static const uint8_t length_extra[29] =
{
    0,0,0,0,0,0,0,0,
    1,1,1,1,
    2,2,2,2,
    3,3,3,3,
    4,4,4,4,
    5,5,5,5,
    0
};

static const uint16_t dist_base[30] =
{
    1,2,3,4,5,7,9,13,
    17,25,33,49,65,97,
    129,193,257,385,
    513,769,1025,1537,
    2049,3073,4097,6145,
    8193,12289,16385,24577
};

static const uint8_t dist_extra[30] =
{
    0,0,0,0,1,1,2,2,
    3,3,4,4,5,5,
    6,6,7,7,
    8,8,9,9,
    10,10,11,11,
    12,12,13,13
};

static int build_fixed(HUFFMAN *litlen,HUFFMAN *dist)
{
    uint8_t *ll;
    uint8_t *dd;
    int i;
    int result;

    ll = (uint8_t *)malloc(288);
    dd = (uint8_t *)malloc(32);

    if(!ll || !dd)
    {
        if(ll)
            free(ll);

        if(dd)
            free(dd);

        return -1;
    }

    for(i = 0;i <= 143;i++)
        ll[i] = 8;

    for(i = 144;i <= 255;i++)
        ll[i] = 9;

    for(i = 256;i <= 279;i++)
        ll[i] = 7;

    for(i = 280;i < 288;i++)
        ll[i] = 8;

    for(i = 0;i < 32;i++)
        dd[i] = 5;

    result = huffman_build(litlen,ll,288);

    if(result == 0)
        result = huffman_build(dist,dd,32);

    free(ll);
    free(dd);

    if(result != 0)
    {
        huffman_free(litlen);
        huffman_free(dist);
        return -1;
    }

    return 0;
}

static uint8_t paeth(uint8_t a,uint8_t b,uint8_t c)
{
    int p;
    int pa;
    int pb;
    int pc;

    p = (int)a + (int)b - (int)c;
    pa = p - (int)a;
    pb = p - (int)b;
    pc = p - (int)c;

    if(pa < 0)
        pa = -pa;

    if(pb < 0)
        pb = -pb;

    if(pc < 0)
        pc = -pc;

    if(pa <= pb && pa <= pc)
        return a;

    if(pb <= pc)
        return b;

    return c;
}

static int inflate_data(
    const uint8_t *data,
    uint32_t size,
    uint8_t *out,
    uint32_t out_size
)
{
    BIT_READER br;
    uint32_t outpos = 0;
    int final = 0;

    if(size < 6)
        return -1;

    if((data[0] & 0x0F) != 8)
        return -1;

    if((((uint32_t)data[0] << 8) | data[1]) % 31 != 0)
        return -1;

    if(data[1] & 0x20)
        return -1;

    br.data = data + 2;
    br.size = size - 2;
    br.pos = 0;
    br.bits = 0;
    br.count = 0;

    while(!final)
    {
        uint32_t block_type;

        final = (int)br_read(&br,1);
        block_type = br_read(&br,2);

        if(block_type == 0)
        {
            uint32_t len;
            uint32_t nlen;
            uint32_t i;

            br_align(&br);

            len = br_read(&br,16);
            nlen = br_read(&br,16);

            if(((len ^ 0xFFFFu) & 0xFFFFu) != nlen)
                return -1;

            if(outpos + len > out_size)
                return -1;

            for(i = 0;i < len;i++)
                out[outpos++] = (uint8_t)br_read(&br,8);
        }
        else if(block_type == 1 || block_type == 2)
        {
            HUFFMAN litlen;
            HUFFMAN dist;
            int result;

            litlen.table = 0;
            dist.table = 0;

            if(block_type == 1)
            {
                result = build_fixed(&litlen,&dist);

                if(result != 0)
                    return -1;
            }
            else
            {
                uint32_t HLIT;
                uint32_t HDIST;
                uint32_t HCLEN;
                uint8_t cl[19];
                uint8_t lengths[288 + 32];
                uint8_t ll[288];
                uint8_t dd[32];
                HUFFMAN cl_table;
                static const uint8_t order[19] =
                {
                    16,17,18,0,8,7,9,6,10,5,
                    11,4,12,3,13,2,14,1,15
                };
                uint32_t total;
                uint32_t i;

                cl_table.table = 0;

                HLIT = br_read(&br,5) + 257;
                HDIST = br_read(&br,5) + 1;
                HCLEN = br_read(&br,4) + 4;

                if(HLIT > 288 || HDIST > 32)
                    return -1;

                memset(cl,0,sizeof(cl));

                for(i = 0;i < HCLEN;i++)
                    cl[order[i]] = (uint8_t)br_read(&br,3);

                if(huffman_build(&cl_table,cl,19) != 0)
                    return -1;

                total = HLIT + HDIST;
                i = 0;

                while(i < total)
                {
                    int sym;

                    sym = huffman_decode(&br,&cl_table);

                    if(sym < 0)
                    {
                        huffman_free(&cl_table);
                        return -1;
                    }

                    if(sym <= 15)
                    {
                        lengths[i++] = (uint8_t)sym;
                    }
                    else if(sym == 16)
                    {
                        uint32_t repeat;
                        uint8_t value;

                        if(i == 0)
                        {
                            huffman_free(&cl_table);
                            return -1;
                        }

                        repeat = br_read(&br,2) + 3;
                        value = lengths[i - 1];

                        if(i + repeat > total)
                        {
                            huffman_free(&cl_table);
                            return -1;
                        }

                        while(repeat--)
                            lengths[i++] = value;
                    }
                    else if(sym == 17)
                    {
                        uint32_t repeat;

                        repeat = br_read(&br,3) + 3;

                        if(i + repeat > total)
                        {
                            huffman_free(&cl_table);
                            return -1;
                        }

                        while(repeat--)
                            lengths[i++] = 0;
                    }
                    else if(sym == 18)
                    {
                        uint32_t repeat;

                        repeat = br_read(&br,7) + 11;

                        if(i + repeat > total)
                        {
                            huffman_free(&cl_table);
                            return -1;
                        }

                        while(repeat--)
                            lengths[i++] = 0;
                    }
                    else
                    {
                        huffman_free(&cl_table);
                        return -1;
                    }
                }

                huffman_free(&cl_table);

                memcpy(ll,lengths,HLIT);
                memcpy(dd,lengths + HLIT,HDIST);

                if(huffman_build(&litlen,ll,HLIT) != 0)
                    return -1;

                if(huffman_build(&dist,dd,HDIST) != 0)
                {
                    huffman_free(&litlen);
                    return -1;
                }
            }

            while(1)
            {
                int sym;

                sym = huffman_decode(&br,&litlen);

                if(sym < 0)
                {
                    huffman_free(&litlen);
                    huffman_free(&dist);
                    return -1;
                }

                if(sym < 256)
                {
                    if(outpos >= out_size)
                    {
                        huffman_free(&litlen);
                        huffman_free(&dist);
                        return -1;
                    }

                    out[outpos++] = (uint8_t)sym;
                }
                else if(sym == 256)
                {
                    break;
                }
                else if(sym >= 257 && sym <= 285)
                {
                    uint32_t li;
                    uint32_t length;
                    uint32_t extra;
                    int dsym;
                    uint32_t distance;
                    uint32_t i;

                    li = (uint32_t)(sym - 257);

                    length = length_base[li];
                    extra = length_extra[li];

                    if(extra)
                        length += br_read(&br,(int)extra);

                    dsym = huffman_decode(&br,&dist);

                    if(dsym < 0 || dsym >= 30)
                    {
                        huffman_free(&litlen);
                        huffman_free(&dist);
                        return -1;
                    }

                    distance = dist_base[dsym];

                    if(dist_extra[dsym])
                        distance += br_read(
                            &br,
                            dist_extra[dsym]
                        );

                    if(distance == 0 || distance > outpos)
                    {
                        huffman_free(&litlen);
                        huffman_free(&dist);
                        return -1;
                    }

                    if(outpos + length > out_size)
                    {
                        huffman_free(&litlen);
                        huffman_free(&dist);
                        return -1;
                    }

                    for(i = 0;i < length;i++)
                    {
                        out[outpos] =
                            out[outpos - distance];

                        outpos++;
                    }
                }
                else
                {
                    huffman_free(&litlen);
                    huffman_free(&dist);
                    return -1;
                }
            }

            huffman_free(&litlen);
            huffman_free(&dist);
        }
        else
        {
            return -1;
        }
    }

    if(outpos != out_size)
        return -1;

    return 0;
}

static int png_load(
    const uint8_t *data,
    uint32_t size,
    IMAGE *image
)
{
    uint32_t pos;
    uint32_t width;
    uint32_t height;
    uint32_t idat_size = 0;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t compression;
    uint8_t filter;
    uint8_t interlace;
    uint8_t *idat;
    uint8_t *raw;
    uint32_t *pixels;
    uint32_t stride;
    uint32_t raw_size;
    uint32_t idat_pos;
    uint32_t y;
    uint32_t bpp;
    int has_alpha = 0;

    static const uint8_t signature[8] =
    {
        0x89,0x50,0x4E,0x47,
        0x0D,0x0A,0x1A,0x0A
    };

    if(size < 33)
        return -1;

    if(memcmp(data,signature,8) != 0)
        return -1;

    pos = 8;

    if(be32(data + pos) != 13)
        return -1;

    pos += 4;

    if(memcmp(data + pos,"IHDR",4) != 0)
        return -1;

    pos += 4;

    width = be32(data + pos);
    height = be32(data + pos + 4);

    bit_depth = data[pos + 8];
    color_type = data[pos + 9];
    compression = data[pos + 10];
    filter = data[pos + 11];
    interlace = data[pos + 12];

    pos += 13;
    pos += 4;

    if(width == 0 || height == 0)
        return -1;

    if(width > 65535 || height > 65535)
        return -1;

    if(bit_depth != 8)
        return -2;

    if(color_type != 2 && color_type != 6)
        return -2;

    if(compression != 0 || filter != 0 || interlace != 0)
        return -2;

    // File: Kernel/SourceI/image.c (png_load: bpp phai theo color_type,
    // color_type 2 = RGB = 3 byte/pixel, khong phai luon 4 nhu truoc)
    bpp = (color_type == 6) ? 4 : 3;

    if(width > 0xFFFFFFFFu / bpp)
        return -1;

    stride = width * bpp;

    if(height > 0xFFFFFFFFu / (stride + 1))
        return -1;

    raw_size = (stride + 1) * height;

    if(width > 0xFFFFFFFFu / height)
        return -1;

    if(width * height > 0xFFFFFFFFu / 4u)
        return -1;

    pos = 8;
    idat_size = 0;

    while(pos + 12 <= size)
    {
        uint32_t chunk_size = be32(data + pos);

        if(chunk_size > size - pos - 12)
            return -1;

        if(memcmp(data + pos + 4,"IDAT",4) == 0)
        {
            if(idat_size > 0xFFFFFFFFu - chunk_size)
                return -1;

            idat_size += chunk_size;
        }

        pos += 12 + chunk_size;

        if(pos > size)
            return -1;

        if(pos >= 8)
        {
            if(pos >= size)
                break;
        }
    }

    if(idat_size == 0)
        return -1;

    idat = (uint8_t *)malloc(idat_size);

    if(!idat)
        return -3;

    idat_pos = 0;
    pos = 8;

    while(pos + 12 <= size)
    {
        uint32_t chunk_size = be32(data + pos);

        if(chunk_size > size - pos - 12)
        {
            free(idat);
            return -1;
        }

        if(memcmp(data + pos + 4,"IDAT",4) == 0)
        {
            memcpy(
                idat + idat_pos,
                data + pos + 8,
                chunk_size
            );

            idat_pos += chunk_size;
        }

        pos += 12 + chunk_size;
    }

    raw = (uint8_t *)malloc(raw_size);

    if(!raw)
    {
        free(idat);
        return -3;
    }

    if(inflate_data(idat,idat_size,raw,raw_size) != 0)
    {
        free(raw);
        free(idat);
        return -1;
    }

    free(idat);

    pixels = (uint32_t *)malloc(
        width * height * sizeof(uint32_t)
    );

    if(!pixels)
    {
        free(raw);
        return -3;
    }

    for(y = 0;y < height;y++)
    {
        // File: Kernel/SourceI/image.c (png_load: unfilter + convert gop
        // lam 1 pass, tong quat theo bpp = 3 (RGB) hoac 4 (RGBA))
        uint8_t *row = raw + y * (stride + 1);
        uint8_t *prev = 0;
        uint8_t filter_type = row[0];
        uint8_t *cur = row + 1;
        uint32_t *outrow = pixels + (uint32_t)y * width;
        uint32_t x;
        uint8_t left[4] = {0,0,0,0};
        uint8_t upleft[4] = {0,0,0,0};

        if(y)
            prev = raw + (y - 1) * (stride + 1) + 1;

        if(filter_type > 4)
        {
            free(raw);
            free(pixels);
            return -1;
        }

        for(x = 0;x < width;x++)
        {
            uint8_t *px = cur + x * bpp;
            const uint8_t *up = prev ? prev + x * bpp : 0;
            uint8_t out[4];
            uint32_t c;

            for(c = 0;c < bpp;c++)
            {
                uint8_t u = up ? up[c] : 0;
                uint8_t v;

                switch(filter_type)
                {
                    case 1:
                        v = (uint8_t)(px[c] + left[c]);
                        break;
                    case 2:
                        v = (uint8_t)(px[c] + u);
                        break;
                    case 3:
                        v = (uint8_t)(px[c] + ((left[c] + u) >> 1));
                        break;
                    case 4:
                        v = (uint8_t)(px[c] + paeth(left[c],u,upleft[c]));
                        break;
                    default:
                        v = px[c];
                        break;
                }

                px[c] = v;
                out[c] = v;
                upleft[c] = u;
                left[c] = v;
            }

            if(bpp == 4)
            {
                if(out[3] != 255)
                    has_alpha = 1;

                outrow[x] =
                    ((uint32_t)out[3] << 24) |
                    ((uint32_t)out[0] << 16) |
                    ((uint32_t)out[1] << 8) |
                    (uint32_t)out[2];
            }
            else
            {
                outrow[x] =
                    0xFF000000u |
                    ((uint32_t)out[0] << 16) |
                    ((uint32_t)out[1] << 8) |
                    (uint32_t)out[2];
            }
        }
    }

    free(raw);

    image->pixels = pixels;
    image->width = (uint16_t)width;
    image->height = (uint16_t)height;
    image->size = width * height * 4;
    image->loaded = 1;
    image->has_alpha = has_alpha;

    return 0;
}

static int bmp_load(
    const uint8_t *data,
    uint32_t size,
    IMAGE *image
)
{
    uint32_t pixel_offset;
    uint32_t width;
    uint32_t height;
    uint16_t bpp;
    uint32_t compression;
    uint32_t row_size;
    uint32_t *pixels;
    uint32_t y;
    uint32_t x;
    int top_down = 0;
    int has_alpha = 0;

    if(size < 54)
        return -1;

    if(data[0] != 'B' || data[1] != 'M')
        return -1;

    pixel_offset = rd32(data + 10);

    if(rd32(data + 14) < 40)
        return -2;

    width = rd32(data + 18);
    height = rd32(data + 22);
    bpp = rd16(data + 28);
    compression = rd32(data + 30);

    if(width == 0 || height == 0)
        return -1;

    if(bpp != 24 && bpp != 32)
        return -2;

    if(compression != 0)
        return -2;

    if(height & 0x80000000u)
    {
        height = 0u - height;
        top_down = 1;
    }

    if(width > 65535 || height > 65535)
        return -1;

    if(bpp == 24)
        row_size = (width * 3 + 3) & ~3u;
    else
        row_size = width * 4;

    if(pixel_offset > size)
        return -1;

    if(height > (size - pixel_offset) / row_size)
        return -1;

    pixels = (uint32_t *)malloc(
        width * height * sizeof(uint32_t)
    );

    if(!pixels)
        return -3;

    for(y = 0;y < height;y++)
    {
        uint32_t src_y;

        if(top_down)
            src_y = y;
        else
            src_y = height - 1 - y;

        {
            const uint8_t *row =
                data + pixel_offset + src_y * row_size;

            for(x = 0;x < width;x++)
            {
                uint8_t b = row[x * (bpp / 8) + 0];
                uint8_t g = row[x * (bpp / 8) + 1];
                uint8_t r = row[x * (bpp / 8) + 2];
                uint8_t a = 255;

                if(bpp == 32)
                {
                    a = row[x * 4 + 3];

                    if(a != 255)
                        has_alpha = 1;
                }

                pixels[y * width + x] =
                    ((uint32_t)a << 24) |
                    ((uint32_t)r << 16) |
                    ((uint32_t)g << 8) |
                    (uint32_t)b;
            }
        }
    }

    image->pixels = pixels;
    image->width = (uint16_t)width;
    image->height = (uint16_t)height;
    image->size = width * height * 4;
    image->loaded = 1;
    image->has_alpha = has_alpha;

    return 0;
}

static int tga_load(
    const uint8_t *data,
    uint32_t size,
    IMAGE *image
)
{
    uint8_t id_length;
    uint8_t image_type;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint8_t descriptor;
    uint32_t offset;
    uint32_t count;
    uint32_t *pixels;
    uint32_t i;
    int has_alpha = 0;
    int top_origin;
    int right_origin;

    if(size < 18)
        return -1;

    id_length = data[0];
    image_type = data[2];
    width = rd16(data + 12);
    height = rd16(data + 14);
    bpp = data[16];
    descriptor = data[17];

    if(image_type != 2)
        return -2;

    if(bpp != 24 && bpp != 32)
        return -2;

    if(width == 0 || height == 0)
        return -1;

    offset = 18 + id_length;

    if(offset > size)
        return -1;

    count = (uint32_t)width * height;

    if(count > (size - offset) / (bpp / 8))
        return -1;

    pixels = (uint32_t *)malloc(
        count * sizeof(uint32_t)
    );

    if(!pixels)
        return -3;

    top_origin = (descriptor & 0x20) != 0;
    right_origin = (descriptor & 0x10) != 0;

    for(i = 0;i < count;i++)
    {
        uint32_t sx;
        uint32_t sy;
        uint32_t x;
        uint32_t y;
        const uint8_t *p;
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a = 255;

        sx = i % width;
        sy = i / width;

        x = right_origin ? width - 1 - sx : sx;
        y = top_origin ? sy : height - 1 - sy;

        p = data + offset + i * (bpp / 8);

        b = p[0];
        g = p[1];
        r = p[2];

        if(bpp == 32)
        {
            a = p[3];

            if(a != 255)
                has_alpha = 1;
        }

        pixels[y * width + x] =
            ((uint32_t)a << 24) |
            ((uint32_t)r << 16) |
            ((uint32_t)g << 8) |
            (uint32_t)b;
    }

    image->pixels = pixels;
    image->width = width;
    image->height = height;
    image->size = count * 4;
    image->loaded = 1;
    image->has_alpha = has_alpha;

    return 0;
}

static int has_extension(
    const char *path,
    const char *extension
)
{
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t start = 0;

    if(!path || !extension)
        return 0;

    while(path[i])
    {
        if(path[i] == '.')
            start = i + 1;

        i++;
    }

    while(extension[j])
    {
        char a = path[start + j];
        char b = extension[j];

        if(a >= 'A' && a <= 'Z')
            a += 'a' - 'A';

        if(b >= 'A' && b <= 'Z')
            b += 'a' - 'A';

        if(a != b)
            return 0;

        j++;
    }

    return path[start + j] == 0;
}

IMAGE loadimage(const char *path)
{
    IMAGE image;
    uint32_t file_size;
    uint8_t *data;
    int read_size;
    int result;

    image.pixels = 0;
    image.width = 0;
    image.height = 0;
    image.size = 0;
    image.loaded = 0;
    image.has_alpha = 0;

    image_last_error = IMG_OK;

    if(!path)
    {
        image_last_error = IMG_ERR_INVALID;
        return image;
    }

    if(fat32_open(path) != 0)
    {
        image_last_error = IMG_ERR_OPEN;
        return image;
    }

    file_size = fat32_file_size();

    if(file_size == 0)
    {
        fat32_close();
        image_last_error = IMG_ERR_INVALID;
        return image;
    }

    if(file_size > IMAGE_MAX_BYTES)
    {
        fat32_close();
        image_last_error = IMG_ERR_MEMORY;
        return image;
    }

    data = (uint8_t *)malloc(file_size);

    if(!data)
    {
        fat32_close();
        image_last_error = IMG_ERR_MEMORY;
        return image;
    }

    read_size = fat32_read(data,file_size);

    fat32_close();

    if(read_size < 0 || (uint32_t)read_size != file_size)
    {
        free(data);
        image_last_error = IMG_ERR_READ;
        return image;
    }

    result = -1;

    if(file_size >= 8 &&
       data[0] == 0x89 &&
       data[1] == 0x50 &&
       data[2] == 0x4E &&
       data[3] == 0x47)
    {
        result = png_load(data,file_size,&image);
    }
    else if(file_size >= 2 &&
            data[0] == 'B' &&
            data[1] == 'M')
    {
        result = bmp_load(data,file_size,&image);
    }
    else if(file_size >= 18)
    {
        result = tga_load(data,file_size,&image);
    }

    free(data);

    if(result == 0)
    {
        image_last_error = IMG_OK;
        return image;
    }

    if(result == -2)
        image_last_error = IMG_ERR_UNSUPPORTED;
    else if(result == -3)
        image_last_error = IMG_ERR_MEMORY;
    else
        image_last_error = IMG_ERR_INVALID;

    return image;
}

int useimage(
    const IMAGE *image,
    uint32_t x,
    uint32_t y
)
{
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t *backbuffer;
    uint32_t copy_w;
    uint32_t copy_h;
    uint32_t iy;

    if(!image || !image->loaded || !image->pixels)
        return 0;

    if(!gfx_ready)
        return 0;

    screen_width = gfx_width();
    screen_height = gfx_height();
    backbuffer = gfx_get_backbuffer();

    if(!backbuffer)
        return 0;

    if(x >= screen_width || y >= screen_height)
        return 1;

    copy_w = image->width;
    copy_h = image->height;

    if(copy_w > screen_width - x)
        copy_w = screen_width - x;

    if(copy_h > screen_height - y)
        copy_h = screen_height - y;

    if(!image->has_alpha)
    {
        for(iy = 0;iy < copy_h;iy++)
        {
            uint32_t *dst =
                backbuffer +
                (y + iy) * screen_width +
                x;

            const uint32_t *src =
                image->pixels +
                iy * image->width;

            memcpy(
                dst,
                src,
                copy_w * sizeof(uint32_t)
            );
        }

        return 1;
    }

    // File: Kernel/SourceI/image.c (useimage: blend alpha nhanh hon,
    // thay phep chia /255 bang xap xi (v + 1 + (v >> 8)) >> 8)
    for(iy = 0;iy < copy_h;iy++)
    {
        uint32_t ix;

        uint32_t *dst =
            backbuffer +
            (y + iy) * screen_width +
            x;

        const uint32_t *src =
            image->pixels +
            iy * image->width;

        for(ix = 0;ix < copy_w;ix++)
        {
            uint32_t s = src[ix];
            uint32_t a = s >> 24;

            if(a == 0)
                continue;

            if(a == 255)
            {
                dst[ix] = s;
                continue;
            }

            {
                uint32_t d = dst[ix];
                uint32_t ia = 255 - a;

                uint32_t sr = (s >> 16) & 255;
                uint32_t sg = (s >> 8) & 255;
                uint32_t sb = s & 255;

                uint32_t dr = (d >> 16) & 255;
                uint32_t dg = (d >> 8) & 255;
                uint32_t db = d & 255;

                uint32_t vr = sr * a + dr * ia;
                uint32_t vg = sg * a + dg * ia;
                uint32_t vb = sb * a + db * ia;

                uint32_t r = (vr + 1 + (vr >> 8)) >> 8;
                uint32_t g = (vg + 1 + (vg >> 8)) >> 8;
                uint32_t b = (vb + 1 + (vb >> 8)) >> 8;

                dst[ix] =
                    0xFF000000u |
                    (r << 16) |
                    (g << 8) |
                    b;
            }
        }
    }

    return 1;
}

void freeimage(IMAGE *image)
{
    if(!image)
        return;

    if(image->pixels)
        free(image->pixels);

    image->pixels = 0;
    image->width = 0;
    image->height = 0;
    image->size = 0;
    image->loaded = 0;
    image->has_alpha = 0;
}

uint16_t image_width(const IMAGE *image)
{
    if(!image)
        return 0;

    return image->width;
}

uint16_t image_height(const IMAGE *image)
{
    if(!image)
        return 0;

    return image->height;
}

const uint32_t *image_pixels_ptr(const IMAGE *image)
{
    if(!image)
        return 0;

    return image->pixels;
}

image_error_t loadimage_last_error(void)
{
    return image_last_error;
}