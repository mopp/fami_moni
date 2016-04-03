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
unsigned char const* ENABLE_CHARS = "0123456789ABCDEF /.*?;";
#define PROMPT_CHAR '>'
#define ENTER_CHAR ';'
#define ENABLE_CHARS_SIZE (22 - 1)

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
void puts(unsigned char const*);
void putchar(unsigned char);
void putchar_keep_positions(unsigned char);
void draw_sprite(unsigned char, unsigned char, unsigned char, unsigned char, unsigned char);
void cursor_update(void);
void joypad_load_states(void);
unsigned char joypad_get_event(unsigned char);
unsigned int atoi(unsigned char const*);
void itoa(unsigned char*, unsigned int, unsigned char);
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

    putchar(PROMPT_CHAR);
    putchar_keep_positions(ENABLE_CHARS[current_char_index]);

    // Main loop.
    while (1) {
        joypad_load_states();
        if ((PPU.status & 0x80) == 0) {
            // In case of NOT vBlank.
            continue;
        }

        // Blink corsor.
        ++blink_counter;
        if (blink_counter == 30) {
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
            putchar_keep_positions(ENABLE_CHARS[current_char_index]);
        } else if (joypad_get_event(JOYPAD_RIGHT) == JOYPAD_EVENT_PRESSED) {
            // Show next character.
            if (current_char_index == ENABLE_CHARS_SIZE) {
                current_char_index = 0;
            } else {
                ++current_char_index;
            }
            putchar_keep_positions(ENABLE_CHARS[current_char_index]);
        } else if (joypad_get_event(JOYPAD_A) == JOYPAD_EVENT_PRESSED) {
            input_char = ENABLE_CHARS[current_char_index];
            if (input_char == ENTER_CHAR) {
                if (input_buffer_index == 0) {
                    continue;
                }

                // Inputting line is finished.
                input_buffer[input_buffer_index] = '\0';
                input_buffer_index = 0;

                newline_positions(&input_x, &input_y);
                execute_command(input_buffer);

                // Synchronize position.
                cursor_y = input_y;
                cursor_x = 1;

                // Print newline prompt.
                putchar(PROMPT_CHAR);
            } else {
                // Input character is decided.
                input_buffer[input_buffer_index] = ENABLE_CHARS[current_char_index];
                ++input_buffer_index;

                // Prepare for inputting next character.
                shift_positions(&input_x, &input_y);
                shift_positions(&cursor_x, &cursor_y);
            }

            cursor_update();
            current_char_index = 0;
            putchar_keep_positions(ENABLE_CHARS[current_char_index]);
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


void puts(unsigned char const* str)
{
    while (*str != '\0') {
        putchar(*str);
        ++str;
    }
    newline_positions(&input_x, &input_y);
}


void putchar(unsigned char c)
{
    if (c == '\n') {
        newline_positions(&input_x, &input_y);
    } else {
        putchar_keep_positions(c);
        shift_positions(&input_x, &input_y);
    }
}


void putchar_keep_positions(unsigned char c)
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


unsigned int atoi(unsigned char const* str)
{
    /* 16^3 16^2 16^1 16^0 */
    /* 1111 1111 1111 1111 */
    unsigned char tmp;
    unsigned char i;
    unsigned int n = 0;
    for (i = 0; i < 4; ++i) {
        if (str[i] == '\0') {
            return n;
        }

        tmp = (str[i] - '0');
        if (9 < tmp) {
            tmp = (10 + str[i] - 'A');
        }
        n += (tmp << ((3 - i) * 4));
    }

    return n;
}


void itoa(unsigned char* buf, unsigned int n, unsigned char len)
{
    unsigned char tmp;
    unsigned char i;
    for (i = 0; i < len; ++i) {
        tmp = (n >> ((len - 1 - i) * 4)) & 0xF;

        if (tmp < 9) {
            *buf = '0' + tmp;
        } else {
            *buf = 'A' + (tmp - 10);
        }

        ++buf;
    }
    *buf = '\0';
}


void execute_command(unsigned char* str)
{
    static unsigned int addr = 0x400;
    unsigned char tmp;
    unsigned char i;
    unsigned char cmd;
    unsigned char buf[5];

    if ((*str != '*') && (*str != '/') && (*str != '.') && (*str != '*')) {
        // Head part is addr
        addr = atoi(str);
        cmd = str[4];
        str += 5;
    } else {
        cmd = str[0];
        ++str;
    }

    switch (cmd) {
        case '?':
            // Print value at given addr.
            itoa(buf, *((unsigned char*)addr), 2);
            puts(buf);
            break;
        case '/':
            // Write value at given addr.
            // After that increment addr.
            *((unsigned char*)addr) = atoi(str) & 0xFF;
            ++addr;
            break;
        case '.':
            // Write value at given addr.
            break;
        case '*':
            // Execute program from given addr.
            break;
        default:
            // Failed executing.
            puts(MSG_COMMAND_FAILED);
    }
}
