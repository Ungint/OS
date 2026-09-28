// Kernel/Source/fat32.c

#include "../Include/fat32.h"
#include "../Include/ata.h"
#include "../Include/mbr.h"
#include "../Include/vga.h"
#include "../Include/string.h"

#define MAX_PATH 128
#define FAT32_EOC_MIN 0x0FFFFFF8
#define MAX_LFN_ENTRIES 20

// ============================================
// LFN ENTRY
// ============================================

typedef struct {
    uint8_t order;

    uint16_t name1[5];

    uint8_t attr;
    uint8_t type;
    uint8_t checksum;

    uint16_t name2[6];

    uint16_t first_cluster;

    uint16_t name3[2];

} __attribute__((packed)) lfn_entry_t;


// ============================================
// GLOBAL
// ============================================

static uint8_t fat32_drive=0x81;

static fat32_bpb_t bpb;

static uint32_t partition_lba_start=0;
static uint32_t fat_begin_lba=0;
static uint32_t cluster_begin_lba=0;

static uint32_t sectors_per_cluster=0;
static uint32_t root_dir_cluster=0;
static uint32_t bytes_per_cluster=0;

static int fs_ready=0;

static uint32_t current_dir_cluster=0;

static uint8_t sector_buf[512];
static uint8_t cluster_buf[512*8];

static int file_is_open=0;
static int file_write_mode=0;

static uint32_t file_first_cluster=0;
static uint32_t file_current_cluster=0;

static uint32_t file_size=0;
static uint32_t file_pos=0;
static uint32_t file_cluster_offset=0;

static uint32_t open_dirent_cluster=0;
static int open_dirent_index=-1;


// ============================================
// ASCII CASE-INSENSITIVE COMPARE
// ============================================

static char ascii_lower(char c)
{
    if(c>='A'&&c<='Z')
        return c+('a'-'A');

    return c;
}

static int name_equal(const char *a,const char *b)
{
    int i=0;

    while(a[i]&&b[i])
    {
        if(ascii_lower(a[i])!=ascii_lower(b[i]))
            return 0;

        i++;
    }

    return a[i]==0&&b[i]==0;
}


// ============================================
// LFN CHECKSUM
// ============================================

static uint8_t lfn_checksum(const uint8_t *name)
{
    uint8_t sum=0;

    for(int i=0;i<11;i++)
    {
        sum=((sum&1)?0x80:0)+(sum>>1)+name[i];
    }

    return sum;
}


// ============================================
// PARSE LFN
// ============================================

static void fat32_parse_lfn(
    lfn_entry_t *entries,
    int count,
    char *out
)
{
    int pos=0;

    /*
     * FAT directory stores LFN entries like:

         order 2
         order 1
         normal entry

     * Therefore reading the collected entries backwards
     * reconstructs the filename correctly.
     */

    for(int i=count-1;i>=0;i--)
    {
        uint16_t *parts[3]={
            entries[i].name1,
            entries[i].name2,
            entries[i].name3
        };

        int part_len[3]={
            5,
            6,
            2
        };

        for(int p=0;p<3;p++)
        {
            for(int c=0;c<part_len[p];c++)
            {
                uint16_t ch=parts[p][c];

                if(ch==0x0000||ch==0xFFFF)
                {
                    out[pos]=0;
                    return;
                }

                /*
                 * Current kernel only supports ASCII.
                 */
                if(ch>=0x80)
                    continue;

                if(pos>=254)
                {
                    out[pos]=0;
                    return;
                }

                out[pos++]=(char)(ch&0xFF);
            }
        }
    }

    out[pos]=0;
}


// ============================================
// CLUSTER -> LBA
// ============================================

static inline uint32_t cluster_to_lba(uint32_t cluster)
{
    return cluster_begin_lba+
           (cluster-2)*sectors_per_cluster;
}


// ============================================
// READ CLUSTER
// ============================================

static int read_cluster(
    uint32_t cluster,
    uint8_t *out
)
{
    if(cluster<2)
        return -1;

    uint32_t lba=cluster_to_lba(cluster);

    return ata_read_sectors(
        fat32_drive,
        lba,
        sectors_per_cluster,
        out
    );
}


// ============================================
// WRITE CLUSTER
// ============================================

static int write_cluster(
    uint32_t cluster,
    const uint8_t *in
)
{
    if(cluster<2)
        return -1;

    uint32_t lba=cluster_to_lba(cluster);

    for(uint32_t s=0;s<sectors_per_cluster;s++)
    {
        if(ata_write_sector(
            fat32_drive,
            lba+s,
            in+s*512
        )!=0)
        {
            return -1;
        }
    }

    return 0;
}


// ============================================
// FAT GET NEXT
// ============================================

static uint32_t fat_get_next_cluster(uint32_t cluster)
{
    uint32_t fat_offset=cluster*4;

    uint32_t fat_sector=
        fat_begin_lba+(fat_offset/512);

    uint32_t entry_offset=
        fat_offset%512;

    /*
     * FAT32 entry is 4 bytes and cannot cross
     * a 512-byte sector boundary because cluster
     * numbers are aligned to 4-byte entries.
     */

    if(ata_read_sector(
        fat32_drive,
        fat_sector,
        sector_buf
    )!=0)
    {
        return FAT32_EOC_MIN;
    }

    uint32_t value=
        *(uint32_t*)&sector_buf[entry_offset];

    return value&0x0FFFFFFF;
}


// ============================================
// FAT SET NEXT
// ============================================

static int fat_set_next_cluster(
    uint32_t cluster,
    uint32_t value
)
{
    uint32_t fat_offset=cluster*4;

    uint32_t sector_in_fat=
        fat_offset/512;

    uint32_t entry_offset=
        fat_offset%512;

    for(uint32_t f=0;f<bpb.num_fats;f++)
    {
        uint32_t fat_sector=
            fat_begin_lba+
            f*bpb.fat_size_32+
            sector_in_fat;

        if(ata_read_sector(
            fat32_drive,
            fat_sector,
            sector_buf
        )!=0)
        {
            return -1;
        }

        uint32_t old_val=
            *(uint32_t*)&sector_buf[entry_offset];

        uint32_t new_val=
            (old_val&0xF0000000)|
            (value&0x0FFFFFFF);

        *(uint32_t*)&sector_buf[entry_offset]=new_val;

        if(ata_write_sector(
            fat32_drive,
            fat_sector,
            sector_buf
        )!=0)
        {
            return -1;
        }
    }

    return 0;
}


// ============================================
// FIND FREE CLUSTER
// ============================================

static uint32_t find_free_cluster(void)
{
    uint32_t max_entries=
        (bpb.fat_size_32*512)/4;

    for(uint32_t sec=0;
        sec<bpb.fat_size_32;
        sec++)
    {
        if(ata_read_sector(
            fat32_drive,
            fat_begin_lba+sec,
            sector_buf
        )!=0)
        {
            return 0;
        }

        uint32_t *entries=
            (uint32_t*)sector_buf;

        for(int i=0;i<128;i++)
        {
            uint32_t cluster=
                sec*128+(uint32_t)i;

            if(cluster<2||
               cluster>=max_entries)
            {
                continue;
            }

            if((entries[i]&0x0FFFFFFF)==0)
                return cluster;
        }
    }

    return 0;
}


// ============================================
// FAT32 INIT
// ============================================

int fat32_init(uint8_t drive)
{
    fat32_drive=drive;

    print(
        "FAT32: Initializing on drive 0x",
        VGA_COLOR_CYAN,
        VGA_BG_BLACK
    );

    print_hex8(
        drive
    );

    print(
        "\n",
        VGA_COLOR_CYAN,
        VGA_BG_BLACK
    );

    partition_lba_start=0;

    if(mbr_init(fat32_drive)==0)
    {
        uint32_t lba_start;
        uint32_t sector_count;

        if(
            mbr_find_partition(
                0x0C,
                &lba_start,
                &sector_count
            )==0
            ||
            mbr_find_partition(
                0x0B,
                &lba_start,
                &sector_count
            )==0
        )
        {
            partition_lba_start=lba_start;

            print(
                "FAT32: Found FAT32 partition at LBA ",
                VGA_COLOR_GREEN,
                VGA_BG_BLACK
            );

            print_dec(
                partition_lba_start
            );

            print(
                "\n",
                VGA_COLOR_GREEN,
                VGA_BG_BLACK
            );
        }
        else
        {
            print(
                "FAT32: No FAT32 partition in MBR, trying LBA 0 as boot sector.\n",
                VGA_COLOR_YELLOW,
                VGA_BG_BLACK
            );
        }
    }
    else
    {
        print(
            "FAT32: No valid MBR, trying LBA 0 as boot sector directly.\n",
            VGA_COLOR_YELLOW,
            VGA_BG_BLACK
        );
    }

    if(ata_read_sector(
        fat32_drive,
        partition_lba_start,
        sector_buf
    )!=0)
    {
        print(
            "FAT32: Failed to read boot sector!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        fs_ready=0;

        return -1;
    }

    memcpy(
        &bpb,
        sector_buf,
        sizeof(fat32_bpb_t)
    );

    if(
        sector_buf[510]!=0x55||
        sector_buf[511]!=0xAA
    )
    {
        print(
            "FAT32: Invalid boot sector signature!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        fs_ready=0;

        return -1;
    }

    if(
        bpb.fat_size_16!=0||
        bpb.fat_size_32==0||
        bpb.bytes_per_sector!=512
    )
    {
        print(
            "FAT32: This does not look like a FAT32 volume!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        fs_ready=0;

        return -1;
    }

    if(
        bpb.sectors_per_cluster==0||
        bpb.sectors_per_cluster>8
    )
    {
        print(
            "FAT32: Unsupported sectors_per_cluster!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        fs_ready=0;

        return -1;
    }

    fat_begin_lba=
        partition_lba_start+
        bpb.reserved_sectors;

    cluster_begin_lba=
        fat_begin_lba+
        bpb.num_fats*bpb.fat_size_32;

    sectors_per_cluster=
        bpb.sectors_per_cluster;

    root_dir_cluster=
        bpb.root_cluster;

    bytes_per_cluster=
        sectors_per_cluster*
        bpb.bytes_per_sector;

    current_dir_cluster=
        root_dir_cluster;

    fs_ready=1;

    print(
        "FAT32: OK! bytes/sector=",
        VGA_COLOR_GREEN,
        VGA_BG_BLACK
    );

    print_dec(
        bpb.bytes_per_sector
    );

    print(
        ", sectors/cluster=",
        VGA_COLOR_GREEN,
        VGA_BG_BLACK
    );

    print_dec(
        sectors_per_cluster
    );

    print(
        ", root_cluster=",
        VGA_COLOR_GREEN,
        VGA_BG_BLACK
    );

    print_dec(
        root_dir_cluster
    );

    print(
        "\n",
        VGA_COLOR_GREEN,
        VGA_BG_BLACK
    );

    return 0;
}


// ============================================
// FORMAT 8.3
// ============================================

static void format_83_name(
    const char *input,
    uint8_t out[11]
)
{
    for(int i=0;i<11;i++)
        out[i]=' ';

    int i=0;
    int j=0;

    while(
        input[i]!=0&&
        input[i]!='.'&&
        j<8
    )
    {
        out[j++]=to_upper(input[i++]);
    }

    while(
        input[i]!=0&&
        input[i]!='.'
    )
    {
        i++;
    }

    if(input[i]=='.')
    {
        i++;

        int k=8;

        while(
            input[i]!=0&&
            k<11
        )
        {
            out[k++]=to_upper(input[i++]);
        }
    }
}


// ============================================
// FAT32 8.3 -> STRING
// ============================================

static void entry_name_83(
    const fat32_dir_entry_t *entry,
    char *out
)
{
    int pos=0;

    for(int i=0;i<8;i++)
    {
        if(entry->name[i]==' ')
            break;

        out[pos++]=(char)entry->name[i];
    }

    int has_ext=0;

    for(int i=8;i<11;i++)
    {
        if(entry->name[i]!=' ')
        {
            has_ext=1;
            break;
        }
    }

    if(has_ext)
    {
        out[pos++]='.';

        for(int i=8;i<11;i++)
        {
            if(entry->name[i]==' ')
                break;

            out[pos++]=(char)entry->name[i];
        }
    }

    out[pos]=0;
}


// ============================================
// GET ENTRY NAME
// ============================================

static void get_entry_name(
    fat32_dir_entry_t *entry,
    lfn_entry_t *lfn_buffer,
    int lfn_count,
    char *out
)
{
    if(lfn_count>0)
    {
        fat32_parse_lfn(
            lfn_buffer,
            lfn_count,
            out
        );

        if(out[0]!=0)
            return;
    }

    entry_name_83(
        entry,
        out
    );
}


// ============================================
// WALK DIR
// ============================================

static int walk_dir(
    uint32_t dir_cluster,
    const uint8_t *match_name,
    fat32_dir_entry_t *found_out,
    int print_list
)
{
    uint32_t cluster=dir_cluster;

    lfn_entry_t lfn_buffer[MAX_LFN_ENTRIES];

    int lfn_count=0;

    char long_name[256];

    while(
        cluster<FAT32_EOC_MIN&&
        cluster>=2
    )
    {
        if(read_cluster(
            cluster,
            cluster_buf
        )!=0)
        {
            print(
                "FAT32: Error reading directory cluster!\n",
                VGA_COLOR_RED,
                VGA_BG_BLACK
            );

            return -1;
        }

        int entries_per_cluster=
            bytes_per_cluster/
            sizeof(fat32_dir_entry_t);

        fat32_dir_entry_t *entries=
            (fat32_dir_entry_t*)cluster_buf;

        for(
            int i=0;
            i<entries_per_cluster;
            i++
        )
        {
            uint8_t first_byte=
                entries[i].name[0];

            if(first_byte==0x00)
            {
                return match_name?-1:0;
            }

            if(first_byte==0xE5)
            {
                lfn_count=0;
                continue;
            }

            // ------------------------------
            // LFN
            // ------------------------------

            if(entries[i].attr==FAT32_ATTR_LFN)
            {
                lfn_entry_t *lfn=
                    (lfn_entry_t*)&entries[i];

                uint8_t order=
                    lfn->order&0x1F;

                /*
                 * Only accept valid LFN orders.
                 */
                if(
                    order>=1&&
                    order<=MAX_LFN_ENTRIES&&
                    lfn_count<MAX_LFN_ENTRIES
                )
                {
                    /*
                     * Directory order is reversed:
                     *
                     * 02
                     * 01
                     * FILE
                     *
                     * We store them in the same order,
                     * then fat32_parse_lfn() reads backwards.
                     */
                    memcpy(
                        &lfn_buffer[lfn_count],
                        lfn,
                        sizeof(lfn_entry_t)
                    );

                    lfn_count++;
                }

                continue;
            }

            // ------------------------------
            // NORMAL ENTRY
            // ------------------------------

            if(entries[i].attr&FAT32_ATTR_VOLUME_ID)
            {
                lfn_count=0;
                continue;
            }

            get_entry_name(
                &entries[i],
                lfn_buffer,
                lfn_count,
                long_name
            );

            // ------------------------------
            // PRINT
            // ------------------------------

            if(print_list)
            {
                print(
                    "  ",
                    VGA_COLOR_WHITE,
                    VGA_BG_BLACK
                );

                print(
                    long_name,
                    VGA_COLOR_WHITE,
                    VGA_BG_BLACK
                );

                if(entries[i].attr&FAT32_ATTR_DIRECTORY)
                {
                    print(
                        "  <DIR>",
                        VGA_COLOR_LIGHT_BLUE,
                        VGA_BG_BLACK
                    );
                }
                else
                {
                    print(
                        "  ",
                        VGA_COLOR_WHITE,
                        VGA_BG_BLACK
                    );

                    print_dec(
                        entries[i].file_size
                    );

                    print(
                        " bytes",
                        VGA_COLOR_LIGHT_GRAY,
                        VGA_BG_BLACK
                    );
                }

                print(
                    "\n",
                    VGA_COLOR_WHITE,
                    VGA_BG_BLACK
                );
            }

            // ------------------------------
            // MATCH
            // ------------------------------

            if(match_name)
            {
                uint8_t name83[11];

                format_83_name(
                    (const char*)match_name,
                    name83
                );

                /*
                 * First try normal 8.3.
                 */
                if(memcmp(
                    entries[i].name,
                    name83,
                    11
                )==0)
                {
                    if(found_out)
                        *found_out=entries[i];

                    return 0;
                }

                /*
                 * Then try LFN.
                 */
                if(
                    lfn_count>0&&
                    name_equal(
                        long_name,
                        (const char*)match_name
                    )
                )
                {
                    if(found_out)
                        *found_out=entries[i];

                    return 0;
                }
            }

            lfn_count=0;
        }

        cluster=
            fat_get_next_cluster(cluster);
    }

    return match_name?-1:0;
}


// ============================================
// RESOLVE DIRECTORY PATH
// ============================================

static int resolve_dir_cluster(
    const char *path,
    uint32_t *cluster_out
)
{
    if(!path||!cluster_out)
        return -1;

    if(
        strcmp(path,"/")==0||
        path[0]==0
    )
    {
        *cluster_out=root_dir_cluster;
        return 0;
    }

    char path_copy[MAX_PATH];

    int path_len=strlen(path);

    if(path_len>=MAX_PATH)
        return -1;

    strcpy(
        path_copy,
        path
    );

    char *p=path_copy;

    while(*p=='/')
        p++;

    if(*p==0)
    {
        *cluster_out=root_dir_cluster;
        return 0;
    }

    uint32_t cur=root_dir_cluster;

    while(*p)
    {
        char token[256];

        int token_len=0;

        while(
            p[token_len]!=0&&
            p[token_len]!='/'
        )
        {
            if(token_len>=255)
                return -1;

            token_len++;
        }

        memcpy(
            token,
            p,
            token_len
        );

        token[token_len]=0;

        while(
            p[token_len]=='/'
        )
        {
            token_len++;
        }

        p+=token_len;

        if(token[0]==0)
            continue;

        fat32_dir_entry_t entry;

        if(walk_dir(
            cur,
            (const uint8_t*)token,
            &entry,
            0
        )!=0)
        {
            return -1;
        }

        if(
            !(entry.attr&
              FAT32_ATTR_DIRECTORY)
        )
        {
            return -1;
        }

        cur=
            ((uint32_t)entry.first_cluster_hi<<16)|
            entry.first_cluster_lo;

        if(cur==0)
            cur=root_dir_cluster;
    }

    *cluster_out=cur;

    return 0;
}


// ============================================
// FIND OR ALLOC DIRENT
// ============================================

static int find_or_alloc_dirent(
    uint32_t dir_cluster,
    const uint8_t name83[11],
    uint32_t *out_cluster,
    int *out_index,
    uint8_t *buf
)
{
    uint32_t cluster=dir_cluster;

    uint32_t free_cluster=0;
    int free_index=-1;

    while(
        cluster<FAT32_EOC_MIN&&
        cluster>=2
    )
    {
        if(read_cluster(
            cluster,
            buf
        )!=0)
        {
            return -1;
        }

        int entries_per_cluster=
            bytes_per_cluster/
            sizeof(fat32_dir_entry_t);

        fat32_dir_entry_t *entries=
            (fat32_dir_entry_t*)buf;

        for(
            int i=0;
            i<entries_per_cluster;
            i++
        )
        {
            uint8_t fb=
                entries[i].name[0];

            if(fb==0x00)
            {
                if(free_index<0)
                {
                    free_cluster=cluster;
                    free_index=i;
                }

                *out_cluster=free_cluster;
                *out_index=free_index;

                return 1;
            }

            if(fb==0xE5)
            {
                if(free_index<0)
                {
                    free_cluster=cluster;
                    free_index=i;
                }

                continue;
            }

            if(entries[i].attr==FAT32_ATTR_LFN)
                continue;

            if(memcmp(
                entries[i].name,
                name83,
                11
            )==0)
            {
                *out_cluster=cluster;
                *out_index=i;

                return 0;
            }
        }

        cluster=
            fat_get_next_cluster(cluster);
    }

    if(free_index>=0)
    {
        *out_cluster=free_cluster;
        *out_index=free_index;

        return 1;
    }

    return -1;
}


// ============================================
// FAT32 LS
// ============================================

int fat32_ls(void)
{
    if(!fs_ready)
    {
        print(
            "FAT32: Filesystem not initialized!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    print(
        "Files in current directory:\n",
        VGA_COLOR_CYAN,
        VGA_BG_BLACK
    );

    return walk_dir(
        current_dir_cluster,
        0,
        0,
        1
    );
}


// ============================================
// FAT32 OPEN
// ============================================

int fat32_open(const char *filename)
{
    if(!fs_ready)
        return -1;

    if(!filename||filename[0]==0)
        return -1;

    if(file_is_open)
        fat32_close();

    fat32_dir_entry_t entry;

    uint32_t search_cluster=
        current_dir_cluster;

    char final_name[256];

    /*
     * ----------------------------------------
     * ABSOLUTE PATH
     *
     * Example:
     *
     * /.OS/Image/Mouse.bmp
     *
     * directory:
     * /.OS/Image
     *
     * filename:
     * Mouse.bmp
     * ----------------------------------------
     */

    if(filename[0]=='/')
    {
        int len=strlen(filename);

        if(len<=1)
            return -1;

        if(len>=MAX_PATH)
            return -1;

        char path_copy[MAX_PATH];

        strcpy(
            path_copy,
            filename
        );

        /*
         * Find final '/'.
         */
        int last_slash=-1;

        for(int i=0;i<len;i++)
        {
            if(path_copy[i]=='/')
                last_slash=i;
        }

        /*
         * Extract filename.
         */
        int name_start=
            last_slash+1;

        int name_len=
            len-name_start;

        if(name_len<=0||name_len>=256)
            return -1;

        memcpy(
            final_name,
            &path_copy[name_start],
            name_len
        );

        final_name[name_len]=0;

        /*
         * Build directory path.
         */
        char dir_path[MAX_PATH];

        if(last_slash==0)
        {
            dir_path[0]='/';
            dir_path[1]=0;
        }
        else
        {
            memcpy(
                dir_path,
                path_copy,
                last_slash
            );

            dir_path[last_slash]=0;
        }

        /*
         * Resolve:
         *
         * /.OS/Image
         *
         * -> cluster of Image
         */
        if(resolve_dir_cluster(
            dir_path,
            &search_cluster
        )!=0)
        {
            return -1;
        }
    }
    else
    {
        /*
         * Relative filename:
         *
         * Mouse.bmp
         *
         * searches current_dir_cluster.
         */
        int len=strlen(filename);

        if(len<=0||len>=256)
            return -1;

        strcpy(
            final_name,
            filename
        );
    }

    /*
     * Now search ONLY the final filename
     * inside the correct directory cluster.
     */
    if(walk_dir(
        search_cluster,
        (const uint8_t*)final_name,
        &entry,
        0
    )!=0)
    {
        return -1;
    }

    if(entry.attr&FAT32_ATTR_DIRECTORY)
        return -1;

    file_first_cluster=
        ((uint32_t)entry.first_cluster_hi<<16)|
        entry.first_cluster_lo;

    file_current_cluster=
        file_first_cluster;

    file_size=
        entry.file_size;

    file_pos=0;

    file_cluster_offset=0;

    file_is_open=1;
    file_write_mode=0;

    if(file_size>0)
    {
        if(read_cluster(
            file_current_cluster,
            cluster_buf
        )!=0)
        {
            file_is_open=0;
            return -1;
        }
    }

    return 0;
}


// ============================================
// FAT32 READ
// ============================================

int fat32_read(
    void *buffer,
    uint32_t bytes
)
{
    if(
        !file_is_open||
        file_write_mode
    )
    {
        print(
            "FAT32: No file is open for reading!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    uint8_t *out=
        (uint8_t*)buffer;

    uint32_t total_read=0;

    while(
        total_read<bytes&&
        file_pos<file_size
    )
    {
        if(
            file_cluster_offset>=
            bytes_per_cluster
        )
        {
            file_current_cluster=
                fat_get_next_cluster(
                    file_current_cluster
                );

            if(
                file_current_cluster>=
                FAT32_EOC_MIN
            )
            {
                break;
            }

            if(read_cluster(
                file_current_cluster,
                cluster_buf
            )!=0)
            {
                break;
            }

            file_cluster_offset=0;
        }

        uint32_t remaining_in_cluster=
            bytes_per_cluster-
            file_cluster_offset;

        uint32_t remaining_in_file=
            file_size-
            file_pos;

        uint32_t remaining_requested=
            bytes-
            total_read;

        uint32_t chunk=
            remaining_in_cluster;

        if(remaining_in_file<chunk)
            chunk=remaining_in_file;

        if(remaining_requested<chunk)
            chunk=remaining_requested;

        memcpy(
            out+total_read,
            cluster_buf+file_cluster_offset,
            chunk
        );

        total_read+=chunk;
        file_pos+=chunk;
        file_cluster_offset+=chunk;
    }

    return (int)total_read;
}


// ============================================
// FAT32 CREATE
// ============================================

int fat32_create(
    const char *filename
)
{
    if(!fs_ready)
    {
        print(
            "FAT32: Filesystem not initialized!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    if(file_is_open)
        fat32_close();

    uint8_t name83[11];

    format_83_name(
        filename,
        name83
    );

    static uint8_t dirent_buf[512*8];

    uint32_t d_cluster;
    int d_index;

    int r=find_or_alloc_dirent(
        current_dir_cluster,
        name83,
        &d_cluster,
        &d_index,
        dirent_buf
    );

    if(r<0)
    {
        print(
            "FAT32: Directory is full!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    fat32_dir_entry_t *entries=
        (fat32_dir_entry_t*)dirent_buf;

    if(
        r==0&&
        (entries[d_index].attr&
         FAT32_ATTR_DIRECTORY)
    )
    {
        print(
            "FAT32: A directory with that name already exists.\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    uint32_t first_cluster=
        find_free_cluster();

    if(first_cluster==0)
    {
        print(
            "FAT32: Disk full, no free cluster available!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    if(fat_set_next_cluster(
        first_cluster,
        FAT32_EOC_MIN
    )!=0)
    {
        print(
            "FAT32: Failed to update FAT!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    memcpy(
        entries[d_index].name,
        name83,
        11
    );

    entries[d_index].attr=
        FAT32_ATTR_ARCHIVE;

    entries[d_index].reserved=0;
    entries[d_index].ctime_ms=0;
    entries[d_index].ctime=0;
    entries[d_index].cdate=0;
    entries[d_index].adate=0;

    entries[d_index].first_cluster_hi=
        (uint16_t)(first_cluster>>16);

    entries[d_index].mtime=0;
    entries[d_index].mdate=0;

    entries[d_index].first_cluster_lo=
        (uint16_t)(first_cluster&0xFFFF);

    entries[d_index].file_size=0;

    if(write_cluster(
        d_cluster,
        dirent_buf
    )!=0)
    {
        print(
            "FAT32: Failed to write directory entry!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    open_dirent_cluster=d_cluster;
    open_dirent_index=d_index;

    file_first_cluster=first_cluster;
    file_current_cluster=first_cluster;

    file_size=0;
    file_pos=0;
    file_cluster_offset=0;

    file_is_open=1;
    file_write_mode=1;

    memset(
        cluster_buf,
        0,
        sizeof(cluster_buf)
    );

    return 0;
}


// ============================================
// FAT32 WRITE
// ============================================

int fat32_write(
    const void *buffer,
    uint32_t bytes
)
{
    if(
        !file_is_open||
        !file_write_mode
    )
    {
        print(
            "FAT32: No file is open for writing!\n",
            VGA_COLOR_RED,
            VGA_BG_BLACK
        );

        return -1;
    }

    const uint8_t *in=
        (const uint8_t*)buffer;

    uint32_t total_written=0;

    while(total_written<bytes)
    {
        if(
            file_cluster_offset>=
            bytes_per_cluster
        )
        {
            if(write_cluster(
                file_current_cluster,
                cluster_buf
            )!=0)
            {
                print(
                    "FAT32: Disk write error!\n",
                    VGA_COLOR_RED,
                    VGA_BG_BLACK
                );

                break;
            }

            uint32_t next=
                find_free_cluster();

            if(next==0)
            {
                print(
                    "FAT32: Disk full while writing!\n",
                    VGA_COLOR_RED,
                    VGA_BG_BLACK
                );

                break;
            }

            if(
                fat_set_next_cluster(
                    file_current_cluster,
                    next
                )!=0||
                fat_set_next_cluster(
                    next,
                    FAT32_EOC_MIN
                )!=0
            )
            {
                print(
                    "FAT32: Failed to update FAT while writing!\n",
                    VGA_COLOR_RED,
                    VGA_BG_BLACK
                );

                break;
            }

            file_current_cluster=next;

            file_cluster_offset=0;

            memset(
                cluster_buf,
                0,
                sizeof(cluster_buf)
            );
        }

        uint32_t remaining_in_cluster=
            bytes_per_cluster-
            file_cluster_offset;

        uint32_t remaining_requested=
            bytes-
            total_written;

        uint32_t chunk=
            remaining_in_cluster<
            remaining_requested
            ?
            remaining_in_cluster
            :
            remaining_requested;

        memcpy(
            cluster_buf+
            file_cluster_offset,

            in+
            total_written,

            chunk
        );

        total_written+=chunk;
        file_pos+=chunk;
        file_cluster_offset+=chunk;
        file_size+=chunk;
    }

    return (int)total_written;
}


// ============================================
// FAT32 CLOSE
// ============================================

void fat32_close(void)
{
    if(
        file_is_open&&
        file_write_mode
    )
    {
        write_cluster(
            file_current_cluster,
            cluster_buf
        );

        static uint8_t dirent_buf[512*8];

        if(
            open_dirent_index>=0&&
            read_cluster(
                open_dirent_cluster,
                dirent_buf
            )==0
        )
        {
            fat32_dir_entry_t *entries=
                (fat32_dir_entry_t*)dirent_buf;

            entries[
                open_dirent_index
            ].file_size=file_size;

            write_cluster(
                open_dirent_cluster,
                dirent_buf
            );
        }
    }

    file_is_open=0;
    file_write_mode=0;

    open_dirent_index=-1;

    file_first_cluster=0;
    file_current_cluster=0;

    file_size=0;
    file_pos=0;
    file_cluster_offset=0;
}


// ============================================
// DIR EXISTS
// ============================================

int fat32_dir_exists(
    const char *path
)
{
    if(!fs_ready)
        return 0;

    uint32_t c;

    return resolve_dir_cluster(
        path,
        &c
    )==0?1:0;
}


// ============================================
// LIST DIRECTORY
// ============================================

int fat32_list_dir(
    const char *path,
    char *names_out,
    int name_cap,
    int *is_dir_out,
    uint32_t *sizes_out,
    int max_entries
)
{
    if(!fs_ready)
        return -1;

    if(
        !names_out||
        name_cap<=0||
        max_entries<=0
    )
    {
        return -1;
    }

    uint32_t dir_cluster;

    if(resolve_dir_cluster(
        path,
        &dir_cluster
    )!=0)
    {
        return -1;
    }

    static uint8_t list_buf[512*8];

    uint32_t cluster=dir_cluster;

    lfn_entry_t lfn_buffer[MAX_LFN_ENTRIES];

    int lfn_count=0;

    char long_name[256];

    int found=0;

    while(
        cluster<FAT32_EOC_MIN&&
        cluster>=2
    )
    {
        if(read_cluster(
            cluster,
            list_buf
        )!=0)
        {
            return -1;
        }

        int entries_per_cluster=
            bytes_per_cluster/
            sizeof(fat32_dir_entry_t);

        fat32_dir_entry_t *entries=
            (fat32_dir_entry_t*)list_buf;

        for(
            int i=0;
            i<entries_per_cluster;
            i++
        )
        {
            uint8_t first_byte=
                entries[i].name[0];

            if(first_byte==0x00)
                return found;

            if(first_byte==0xE5)
            {
                lfn_count=0;
                continue;
            }

            if(entries[i].attr==FAT32_ATTR_LFN)
            {
                lfn_entry_t *lfn=
                    (lfn_entry_t*)&entries[i];

                uint8_t order=
                    lfn->order&0x1F;

                if(
                    order>=1&&
                    order<=MAX_LFN_ENTRIES&&
                    lfn_count<MAX_LFN_ENTRIES
                )
                {
                    memcpy(
                        &lfn_buffer[lfn_count],
                        lfn,
                        sizeof(lfn_entry_t)
                    );

                    lfn_count++;
                }

                continue;
            }

            if(
                entries[i].attr&
                FAT32_ATTR_VOLUME_ID
            )
            {
                lfn_count=0;
                continue;
            }

            get_entry_name(
                &entries[i],
                lfn_buffer,
                lfn_count,
                long_name
            );

            if(found<max_entries)
            {
                char *dest=
                    names_out+
                    found*name_cap;

                int k=0;

                while(
                    long_name[k]!=0&&
                    k<name_cap-1
                )
                {
                    dest[k]=long_name[k];
                    k++;
                }

                dest[k]=0;

                if(is_dir_out)
                {
                    is_dir_out[found]=
                        (
                            entries[i].attr&
                            FAT32_ATTR_DIRECTORY
                        )
                        ?
                        1
                        :
                        0;
                }

                if(sizes_out)
                {
                    sizes_out[found]=
                        entries[i].file_size;
                }

                found++;
            }

            lfn_count=0;
        }

        cluster=
            fat_get_next_cluster(cluster);
    }

    return found;
}


// ============================================
// CHDIR
// ============================================

int fat32_chdir(
    const char *path
)
{
    if(!fs_ready)
        return -1;

    uint32_t c;

    if(resolve_dir_cluster(
        path,
        &c
    )!=0)
    {
        return -1;
    }

    current_dir_cluster=c;

    return 0;
}
uint32_t fat32_file_size(void)
{
    if(!file_is_open)
        return 0;

    return file_size;
}