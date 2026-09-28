#include "../Include/mouse.h"

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1,%0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port,uint8_t val)
{
    __asm__ volatile("outb %0,%1" : : "a"(val),"Nd"(port));
}

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

#define PS2_OUT_FULL 0x01
#define PS2_IN_FULL  0x02
#define PS2_AUX      0x20

#define PS2_ACK    0xFA
#define PS2_RESEND 0xFE

#define PS2_TIMEOUT 100000

static int mx=0;
static int my=0;

static uint32_t scr_w=0;
static uint32_t scr_h=0;

static uint8_t packet[3];
static int packet_idx=0;

static int left_btn=0;
static int right_btn=0;
static int middle_btn=0;

static int ps2_wait_write(void)
{
    for(int i=0;i<PS2_TIMEOUT;i++)
    {
        if((inb(PS2_STATUS)&PS2_IN_FULL)==0)
            return 0;

        __asm__ volatile("pause");
    }

    return -1;
}

static int ps2_wait_read(void)
{
    for(int i=0;i<PS2_TIMEOUT;i++)
    {
        if(inb(PS2_STATUS)&PS2_OUT_FULL)
            return 0;

        __asm__ volatile("pause");
    }

    return -1;
}

static int ps2_wait_read_aux(void)
{
    for(int i=0;i<PS2_TIMEOUT;i++)
    {
        uint8_t status=inb(PS2_STATUS);

        if((status&(PS2_OUT_FULL|PS2_AUX))==
           (PS2_OUT_FULL|PS2_AUX))
            return 0;

        __asm__ volatile("pause");
    }

    return -1;
}

static int mouse_write(uint8_t data)
{
    if(ps2_wait_write()<0)
        return -1;

    outb(PS2_CMD,0xD4);

    if(ps2_wait_write()<0)
        return -1;

    outb(PS2_DATA,data);

    return 0;
}

static int mouse_wait_ack(void)
{
    for(int i=0;i<PS2_TIMEOUT;i++)
    {
        uint8_t status=inb(PS2_STATUS);

        if(!(status&PS2_OUT_FULL))
        {
            __asm__ volatile("pause");
            continue;
        }

        if(!(status&PS2_AUX))
        {
            /*
                Keyboard byte.
                Không được ăn mất byte này.
            */
            continue;
        }

        uint8_t b=inb(PS2_DATA);

        if(b==PS2_ACK)
            return 0;

        if(b==PS2_RESEND)
            return -1;
    }

    return -1;
}

static void mouse_flush(void)
{
    for(int i=0;i<256;i++)
    {
        uint8_t status=inb(PS2_STATUS);

        if(!(status&PS2_OUT_FULL))
            break;

        if(!(status&PS2_AUX))
            break;

        inb(PS2_DATA);
    }

    packet_idx=0;
}

void mouse_init(uint32_t screen_w,uint32_t screen_h)
{
    scr_w=screen_w;
    scr_h=screen_h;

    mx=(int)(screen_w/2);
    my=(int)(screen_h/2);

    packet_idx=0;

    left_btn=0;
    right_btn=0;
    middle_btn=0;

    /*
        Enable second PS/2 port.
    */
    if(ps2_wait_write()<0)
        return;

    outb(PS2_CMD,0xA8);

    /*
        Read controller configuration byte.
        0x20 returns controller data, NOT AUX data.
    */
    if(ps2_wait_write()<0)
        return;

    outb(PS2_CMD,0x20);

    if(ps2_wait_read()<0)
        return;

    uint8_t status=inb(PS2_DATA);

    /*
        Enable mouse clock.
        Disable mouse IRQ because we are polling.
    */
    status&=~(1<<5);
    status&=~(1<<1);

    if(ps2_wait_write()<0)
        return;

    outb(PS2_CMD,0x60);

    if(ps2_wait_write()<0)
        return;

    outb(PS2_DATA,status);

    /*
        Clear only old mouse data.
    */
    mouse_flush();

    /*
        Set defaults.
    */
    if(mouse_write(0xF6)==0)
        mouse_wait_ack();

    /*
        Set sample rate = 100.
    */
    if(mouse_write(0xF3)==0)
    {
        if(mouse_wait_ack()==0)
        {
            if(mouse_write(100)==0)
                mouse_wait_ack();
        }
    }

    /*
        Enable mouse data reporting.
    */
    if(mouse_write(0xF4)==0)
        mouse_wait_ack();

    mouse_flush();

    packet_idx=0;
}

void mouse_poll(void)
{
    while(1)
    {
        uint8_t status=inb(PS2_STATUS);

        if(!(status&PS2_OUT_FULL))
            break;

        /*
            Keyboard data.
            Không đọc 0x60.
        */
        if(!(status&PS2_AUX))
            break;

        uint8_t data=inb(PS2_DATA);

        /*
            First byte of a standard PS/2 packet
            must have bit 3 set.
        */
        if(packet_idx==0)
        {
            if(!(data&0x08))
                continue;

            packet[0]=data;
            packet_idx=1;
            continue;
        }

        packet[packet_idx]=data;
        packet_idx++;

        if(packet_idx<3)
            continue;

        packet_idx=0;

        uint8_t flags=packet[0];

        if(!(flags&0x08))
            continue;

        int dx=(int8_t)packet[1];
        int dy=(int8_t)packet[2];

        /*
            Overflow.
        */
        if(flags&0x40)
            dx=0;

        if(flags&0x80)
            dy=0;

        mx+=dx;
        my-=dy;

        if(mx<0)
            mx=0;

        if(my<0)
            my=0;

        if(scr_w>0 && mx>=(int)scr_w)
            mx=(int)scr_w-1;

        if(scr_h>0 && my>=(int)scr_h)
            my=(int)scr_h-1;

        left_btn=(flags>>0)&1;
        right_btn=(flags>>1)&1;
        middle_btn=(flags>>2)&1;
    }
}

int mouse_x(void)
{
    return mx;
}

int mouse_y(void)
{
    return my;
}

int mouse_left_button(void)
{
    return left_btn;
}

int mouse_right_button(void)
{
    return right_btn;
}

int mouse_middle_button(void)
{
    return middle_btn;
}