#include <nes.h>

unsigned char const PALETTES[] = {
    // Background palettes.
    0x0F, 0x00, 0x10, 0x20,
    0x0F, 0x06, 0x16, 0x26,
    0x0F, 0x08, 0x18, 0x28,
    0x0F, 0x0A, 0x1A, 0x2A,

    // Sprite palettes.
    0x0F, 0x00, 0x10, 0x20,
    0x0F, 0x06, 0x16, 0x26,
    0x0F, 0x08, 0x18, 0x28,
    0x0F, 0x0A, 0x1A, 0x2A,
};

unsigned char const* MSG_COMMAND_FAILED = "Invalid command.";
unsigned char const* ENABLE_CHARS = "0123456789ABCDEF/.*?";
#define PROMPT_CHAR '>'
#define ENABLE_CHARS_SIZE (20 - 1)

#define SPRITE_ATTR_V_INVERSE 0x80
#define SPRITE_ATTR_H_INVERSE 0x40
#define SPRITE_ATTR_ORDER_BACK 0x10
#define SPRITE_CURSOR 0

unsigned char input_x = 0x00;
unsigned char input_y = 0x01;
unsigned char cursor_x = 0x01;
unsigned char cursor_y = 0x01;
unsigned char cursor_palette = 0x00;

unsigned char joypad_states[8];
unsigned char joypad_prev_states[8];
#define JOYPAD_A 0
#define JOYPAD_B 1
#define JOYPAD_SELECT 2
#define JOYPAD_START 3
#define JOYPAD_UP 4
#define JOYPAD_DOWN 5
#define JOYPAD_LEFT 6
#define JOYPAD_RIGHT 7
#define JOYPAD_EVENT_PRESSED 0
#define JOYPAD_EVENT_RELEASED 1
#define JOYPAD_EVENT_PRESSING 2
#define JOYPAD_EVENT_RELEASING 3

void init_ppu(void);
void set_scroll(unsigned char, unsigned char);
unsigned char newline_positions(unsigned char*, unsigned char*);
unsigned char shift_positions(unsigned char*, unsigned char*);
void print(unsigned char const*);
void println(unsigned char const*);
void printc(unsigned char);
void printc_keep_positions(unsigned char);
void draw_sprite(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char);
void cursor_update(void);
void joypad_load_states(void);
unsigned char joypad_get_event(unsigned char);
unsigned int atoi(unsigned char const*, unsigned char);
void itoa(unsigned char*, unsigned int, unsigned char);
unsigned char is_hex_char(unsigned char);
void execute_command(unsigned char*);


int main(void)
{
    unsigned char input_buffer[32];
    unsigned char input_char;
    unsigned char input_buffer_index = 0;
    unsigned char blink_counter = 0;
    unsigned char current_char_index = 0;
    unsigned char i = 0;

    // Initialize.
    init_ppu();
    while (i < 8) {
        joypad_prev_states[i] = 0;
        ++i;
    }

    printc(PROMPT_CHAR);
    printc_keep_positions(ENABLE_CHARS[current_char_index]);

    // Main loop.
    while (1) {
        joypad_load_states();
        if ((PPU.status & 0x80) == 0) {
            // In case of NOT vBlank.
            continue;
        }

        // Blink corsor.
        ++blink_counter;
        if (blink_counter == 20) {
            cursor_palette ^= 0x03;
            cursor_update();
            blink_counter = 0;
        }

        // Input character.
        if (joypad_get_event(JOYPAD_LEFT) == JOYPAD_EVENT_PRESSED) {
            // Show prev character.
            if (current_char_index == 0) {
                current_char_index = ENABLE_CHARS_SIZE;
            } else {
                --current_char_index;
            }
            printc_keep_positions(ENABLE_CHARS[current_char_index]);
        } else if (joypad_get_event(JOYPAD_RIGHT) == JOYPAD_EVENT_PRESSED) {
            // Show next character.
            if (current_char_index == ENABLE_CHARS_SIZE) {
                current_char_index = 0;
            } else {
                ++current_char_index;
            }
            printc_keep_positions(ENABLE_CHARS[current_char_index]);
        } else if (joypad_get_event(JOYPAD_START) == JOYPAD_EVENT_PRESSED) {
            // Execute input command.
            if (input_buffer_index == 0) {
                continue;
            }

            // Clear character at current position.
            printc_keep_positions(' ');

            // Inputting line is finished.
            input_buffer[input_buffer_index] = '\0';
            input_buffer_index = 0;

            newline_positions(&input_x, &input_y);
            execute_command(input_buffer);

            // Synchronize position.
            cursor_y = input_y;
            cursor_x = 1;

            // Print newline prompt.
            printc(PROMPT_CHAR);

            cursor_update();
            current_char_index = 0;
            printc_keep_positions(ENABLE_CHARS[current_char_index]);
        } else {
            input_char = 0;
            if (joypad_get_event(JOYPAD_A) == JOYPAD_EVENT_PRESSED) {
                // Input a character.
                input_char = ENABLE_CHARS[current_char_index];
            } else if (joypad_get_event(JOYPAD_B) == JOYPAD_EVENT_PRESSED) {
                // Input space.
                input_char = ' ';

                // Clear character at current position.
                printc_keep_positions(' ');
            }

            if (input_char != 0) {
                // Input character is decided.
                input_buffer[input_buffer_index] = input_char;
                ++input_buffer_index;

                // Prepare for inputting next character.
                shift_positions(&input_x, &input_y);
                shift_positions(&cursor_x, &cursor_y);

                cursor_update();
                current_char_index = 0;
                printc_keep_positions(ENABLE_CHARS[current_char_index]);
            }
        }
    }

    return 0;
}


void init_ppu(void)
{
    unsigned char i;
    unsigned int j;

    // Set PPU config.
    // Sprite size: 8x8
    // Background pattern table address
    //  -> 0x0000
    // Sprite pattern table address
    //  -> 0x0000
    // This means sprite and background uses same patterns.
    // Name table address
    //  -> 0x2000
    PPU.control = 0x00;

    // Turn off background and sprite for initializing.
    PPU.mask = 0x00;

    // Init pallettes for background and sprite.
    PPU.vram.address = 0x3F;
    PPU.vram.address = 0x00;
    for (i = 0; i < 32; ++i) {
        PPU.vram.data = PALETTES[i];
    }

    // Init name table.
    PPU.vram.address = 0x20;
    PPU.vram.address = 0x00;
    for (j = 0; j < 960; ++j) {
        PPU.vram.data = 0x00;
    }

    // Init attribute table.
    PPU.vram.address = 0x23;
    PPU.vram.address = 0xC0;
    for (i = 0; i < 64; ++i) {
        // 00 00 00 00 -> palette 0
        // Right bottom, left bottom, right top, left top
        PPU.vram.data = 0x00;
    }

    waitvblank();

    // Reset scroll.
    set_scroll(0, 0);

    // Turn on background and sprite.
    PPU.mask = 0x1E;
}


void set_scroll(unsigned char x, unsigned char y)
{
    PPU.scroll = x;
    PPU.scroll = y;
}


unsigned char newline_positions(unsigned char* x, unsigned char* y)
{
    *x = 0;
    if (*y == 32) {
        *y == 1;
        return 1;
    } else {
        ++*y;
    }

    return 0;
}


unsigned char shift_positions(unsigned char* x, unsigned char* y)
{
    *x += 1;
    if (*x == 32) {
        ++*y;
        *x = 0;
        if (*y == 32) {
            *y = 1;
            return 1;
        }
    }

    return 0;
}


void print(unsigned char const* str)
{
    while (*str != '\0') {
        printc(*str);
        ++str;
    }
}


void println(unsigned char const* str)
{
    print(str);
    newline_positions(&input_x, &input_y);
}


void printc(unsigned char c)
{
    if (c == '\n') {
        newline_positions(&input_x, &input_y);
    } else {
        printc_keep_positions(c);
        shift_positions(&input_x, &input_y);
    }
}


void printc_keep_positions(unsigned char c)
{
    if (c != '\n') {
        unsigned int addr = 0x2000 + input_y * 32 + input_x;

        waitvblank();

        PPU.vram.address = addr >> 8;
        PPU.vram.address = addr & 0xFF;
        PPU.vram.data = c;

        // Reset scroll.
        set_scroll(0, 0);
    }
}


void draw_sprite(unsigned char sprite_number, unsigned char tile_number, unsigned char x, unsigned char y, unsigned char attr)
{
    waitvblank();
    PPU.sprite.address = sprite_number * 4;
    PPU.sprite.data = y;
    PPU.sprite.data = tile_number;
    PPU.sprite.data = attr;
    PPU.sprite.data = x;
}


void cursor_update(void)
{
    draw_sprite(SPRITE_CURSOR, '_', cursor_x * 8, cursor_y * 8, cursor_palette);
}


void joypad_load_states(void)
{
    unsigned char i = 0;

    // Polling joypad.
    JOYPAD[0] = 0x01;
    JOYPAD[0] = 0x00;
    while (i < 8) {
        joypad_states[i] = JOYPAD[0] & 0x01;
        ++i;
    }
}


unsigned char joypad_get_event(unsigned char button)
{
    if (joypad_states[button] == 1) {
        if (joypad_prev_states[button] == 0) {
            // On pressed.
            joypad_prev_states[button] = 1;
            return JOYPAD_EVENT_PRESSED;
        }

        return JOYPAD_EVENT_PRESSING;
    } else {
        if (joypad_prev_states[button] == 1) {
            // On released.
            joypad_prev_states[button] = 0;
            return JOYPAD_EVENT_RELEASED;
        }

        return JOYPAD_EVENT_RELEASING;
    }
}


unsigned int atoi(unsigned char const* str, unsigned char len)
{
    /* 16^3 16^2 16^1 16^0 */
    /* 1111 1111 1111 1111 */
    unsigned int tmp;
    unsigned char i = 0;
    unsigned int n = 0;

    // while ((i < 4) && (str[i] != '\0')) {
    //     ++i;
    // }
    // len = i;

    for (i = 0; i < len; ++i) {
        tmp = (str[i] - '0');
        if (9 < tmp) {
            tmp = (10 + str[i] - 'A');
        }
        n += (tmp << ((len - i - 1) * 4));
    }

    return n;
}


void itoa(unsigned char* buf, unsigned int n, unsigned char len)
{
    unsigned char tmp;
    unsigned char i;
    for (i = 0; i < len; ++i) {
        tmp = (n >> ((len - 1 - i) * 4)) & 0xF;

        if (tmp <= 9) {
            *buf = '0' + tmp;
        } else {
            *buf = 'A' + (tmp - 10);
        }

        ++buf;
    }
    *buf = '\0';
}


unsigned char is_hex_char(unsigned char c)
{
    return (('0' <= c) && (c <= '9')) ? (1) : (0);
}


void execute_command(unsigned char* str)
{
    static unsigned int addr = 0x400;
    void (*func_ptr)();
    unsigned int len;
    unsigned char i;
    unsigned char cmd;
    unsigned char buf[5];

    if (is_hex_char(str[0]) == 1) {
        // Head part is addr
        if (is_hex_char(str[2]) == 0) {
            // Relative addressing.
            addr = (addr & 0xFF00) | (atoi(str, 2) & 0x00FF);
            cmd = str[2];
            str = &str[3];
        } else {
            // Absolute addressing.
            addr = atoi(str, 4);
            cmd = str[4];
            str = &str[5];
        }
    } else {
        // Head part is command.
        cmd = str[0];
        ++str;
    }

    switch (cmd) {
        case '?':
            // Print value at given addr.
            // After that increment addr.
            len = addr + ((*str == '\0') ? (1) : (atoi(str, 1)));
            itoa(buf, addr, 4);
            print(buf);
            print(": ");
            while (addr < len) {
                itoa(buf, *((unsigned char*)addr), 2);
                print(buf);
                printc(' ');
                ++addr;
            }
            printc('\n');
            break;
        case '/':
        case '.':
            // Write value at given addr.
            // After that increment addr if command if '/'.
            while (1) {
                *((unsigned char*)addr) = atoi(str, 2) & 0xFF;
                if (str[2] == '\0') {
                    break;
                }
                str += 3;
                if (cmd == '/') {
                    ++addr;
                }
            }
            break;
        case '*':
            // Execute program from given addr.
            func_ptr = (void (*)())addr;
            func_ptr();
            break;
        default:
            // Failed executing.
            println(MSG_COMMAND_FAILED);
    }
}
