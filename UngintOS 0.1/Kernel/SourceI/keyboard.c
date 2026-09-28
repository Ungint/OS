#include "../Include/keyboard.h"

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

#define PS2_OUT_FULL 0x01
#define PS2_IN_FULL  0x02
#define PS2_AUX      0x20

#define KEY_BUFFER_SIZE 256

static int shift_pressed=0;
static int capslock_on=0;

static char key_buffer[KEY_BUFFER_SIZE];
static volatile uint16_t key_head=0;
static volatile uint16_t key_tail=0;

static const char ascii_table[]={
    0,0,'1','2','3','4','5','6','7','8','9','0',
    '-','=',0,0,'q','w','e','r','t','y','u','i',
    'o','p','[',']',0,0,'a','s','d','f','g','h',
    'j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' '
};

static const char ascii_shift_table[]={
    0,0,'!','@','#','$','%','^','&','*','(',')',
    '_','+',0,0,'Q','W','E','R','T','Y','U','I',
    'O','P','{','}',0,0,'A','S','D','F','G','H',
    'J','K','L',':','"','~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,' '
};

static void ps2_wait_write(void)
{
    for(;;)
    {
        if((inb(PS2_STATUS)&PS2_IN_FULL)==0)
            return;

        __asm__ volatile("pause");
    }
}

static void ps2_wait_read_keyboard(void)
{
    for(;;)
    {
        uint8_t status=inb(PS2_STATUS);

        if((status&PS2_OUT_FULL) && !(status&PS2_AUX))
            return;

        __asm__ volatile("pause");
    }
}

static void key_push(char c)
{
    if(c==0)
        return;

    uint16_t next=(uint16_t)((key_head+1)%KEY_BUFFER_SIZE);

    if(next==key_tail)
        return;

    key_buffer[key_head]=c;
    key_head=next;
}

static char key_pop(void)
{
    if(key_head==key_tail)
        return 0;

    char c=key_buffer[key_tail];

    key_tail=(uint16_t)((key_tail+1)%KEY_BUFFER_SIZE);

    return c;
}

void keyboard_init(void)
{
    shift_pressed=0;
    capslock_on=0;

    key_head=0;
    key_tail=0;

    /*
        Enable keyboard interface.
    */
    ps2_wait_write();
    outb(PS2_STATUS,0xAE);

    /*
        Read controller configuration.
    */
    ps2_wait_write();
    outb(PS2_STATUS,0x20);

    ps2_wait_read_keyboard();

    uint8_t status=inb(PS2_DATA);

    /*
        Keyboard clock enabled.
        Keyboard IRQ disabled because we poll.
        Mouse IRQ disabled because we poll.
    */
    status|=(1<<6);
    status&=~(1<<0);
    status&=~(1<<1);

    ps2_wait_write();
    outb(PS2_STATUS,0x60);

    ps2_wait_write();
    outb(PS2_DATA,status);

    /*
        Enable keyboard scanning.
    */
    ps2_wait_write();
    outb(PS2_DATA,0xF4);

    /*
        Consume keyboard ACK if available.
    */
    for(int i=0;i<100000;i++)
    {
        uint8_t st=inb(PS2_STATUS);

        if(st&PS2_OUT_FULL)
        {
            if(!(st&PS2_AUX))
                inb(PS2_DATA);

            break;
        }

        __asm__ volatile("pause");
    }
}

uint8_t read_scancode(void)
{
    for(;;)
    {
        uint8_t status=inb(PS2_STATUS);

        if((status&PS2_OUT_FULL) && !(status&PS2_AUX))
            return inb(PS2_DATA);

        __asm__ volatile("pause");
    }
}

char scancode_to_ascii(uint8_t scancode)
{
    if(scancode&0x80)
    {
        uint8_t released=scancode&0x7F;

        if(released==0x2A || released==0x36)
            shift_pressed=0;

        return 0;
    }

    if(scancode==0x2A || scancode==0x36)
    {
        shift_pressed=1;
        return 0;
    }

    if(scancode==0x3A)
    {
        capslock_on=!capslock_on;
        return 0;
    }

    if(scancode==0x1C)
        return '\n';

    if(scancode==0x0E)
        return '\b';

    if(scancode==0x0F)
        return '\t';

    if(scancode==0x01)
        return 27;

    char c=0;

    if(scancode<sizeof(ascii_table))
    {
        char base_char=ascii_table[scancode];

        if(base_char>='a' && base_char<='z')
        {
            if(shift_pressed^capslock_on)
                c=ascii_shift_table[scancode];
            else
                c=base_char;
        }
        else
        {
            if(shift_pressed)
                c=ascii_shift_table[scancode];
            else
                c=base_char;
        }
    }

    return c;
}

/*
    Đọc toàn bộ keyboard data đang nằm trong PS/2 buffer.

    QUAN TRỌNG:
    Không đụng byte AUX/mouse.
*/
void keyboard_poll(void)
{
    while(1)
    {
        uint8_t status=inb(PS2_STATUS);

        if(!(status&PS2_OUT_FULL))
            break;

        if(status&PS2_AUX)
            break;

        uint8_t scancode=inb(PS2_DATA);

        char c=scancode_to_ascii(scancode);

        if(c)
            key_push(c);
    }
}

char getch(void)
{
    while(key_head==key_tail)
    {
        keyboard_poll();
        __asm__ volatile("pause");
    }

    return key_pop();
}

int kbhit(void)
{
    return key_head!=key_tail;
}

int is_shift_pressed(void)
{
    return shift_pressed;
}

int is_capslock_on(void)
{
    return capslock_on;
}