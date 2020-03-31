#include "bios_video.h"
#include <stddef.h>

#define TERM_COLS 40
#define TERM_ROWS 30

// standard colour: light gray text on black background
#define TERM_COLOUR 0x70

uint32_t row, col;

void softterm_init()
{
    if (VIDEO_MODE != VIDEOMODE_TEXT_40) {
        return;
    }

    char* textbase = (char*)VIDEO_BASE;
    row = 0;
    col = 0;

    // clear video text buffer;
    for (uint32_t i = 0; i < (TERM_COLS * TERM_ROWS * 2); i += 2) {
        textbase[i] = ' ';
        // set fg colour to light gray, bg colour to black
        textbase[i + 1] = TERM_COLOUR;
    }
}

void softterm_scroll()
{
    char* textbase = (char*)VIDEO_BASE;

    // copy row contents from row below
    for (uint32_t offset = (TERM_COLS * 2); offset < (TERM_COLS * TERM_ROWS * 2); offset++) {
        textbase[offset - (TERM_COLS * 2)] = textbase[offset];
    }

    // clear last line
    for (uint32_t offset = (TERM_ROWS - 1) * (TERM_COLS * 2); offset < (TERM_COLS * TERM_ROWS * 2); offset += 2) {
        textbase[offset] = ' ';
        textbase[offset + 1] = TERM_COLOUR;
    }

    row--;
}

void softterm_check_cursor()
{
    if (col >= TERM_COLS) {
        row++;
        col = 0;
    }

    while (row >= TERM_ROWS) {
        softterm_scroll();
    }
}

void softterm_write_char(char c)
{
    char* textbase = (char*)VIDEO_BASE;
    uint32_t offset = (2 * ((row * TERM_COLS) + col));

    // make cursor invisible
    textbase[offset] = ' ';

    if (c == '\n') {
        row++;
        col = 0;
    } else if (c == '\r') {
        col = 0;
    } else {
        textbase[offset] = c;
        textbase[offset + 1] = (char)0x70; // light gray on black background
        col++;
    }

    softterm_check_cursor();
    // draw cursor
    textbase[(2 * ((row * TERM_COLS) + col))] = 0xDB;
}

result_t bios_video_set_mode(videomode_t mode, void* videobase, void* fontbase)
{

    result_t result;

    VIDEO_MODE = mode;
    VIDEO_BASE = (uint32_t)videobase;
    VIDEO_FONT = (uint32_t)fontbase;

    switch (mode) {
    case VIDEOMODE_TEXT_40:
        softterm_init();
    case VIDEOMODE_OFF:
    case VIDEOMODE_GRAPHICS_640:
    case VIDEOMODE_GRAPHICS_320:
        result = RESULT_OK;
        break;
    default:
        result = RESULT_ERR;
    }

    if (result != RESULT_OK) {
        return result;
    }

    return result;
}

result_t bios_video_set_palette(uint8_t* palette)
{
    uint8_t idx = 0;
    while (1) {
        uint32_t offset = idx * 3;
        uint8_t r = palette[offset];
        uint8_t g = palette[offset + 1];
        uint8_t b = palette[offset + 2];

        VIDEO_PALETTE = (idx << 24 | (r << 16) | (g << 8) | b);

        if (idx == 0xFF) {
            break;
        }
        idx++;
    }

    return RESULT_OK;
}

uint32_t bios_video_get_videobase()
{
    return VIDEO_BASE;
}

uint32_t bios_video_get_fontbase()
{
    return VIDEO_FONT;
}

uint32_t bios_video_getcols()
{
    switch (VIDEO_MODE) {
    VIDEOMODE_TEXT_40:
        return 40;

    VIDEOMODE_GRAPHICS_640:
        return 640;

    VIDEOMODE_GRAPHICS_320:
        return 320;

    default:
        return 0;
    }
}

uint32_t bios_video_getrows()
{
    switch (VIDEO_MODE) {
    VIDEOMODE_TEXT_40:
        return 30;

    VIDEOMODE_GRAPHICS_640:
        return 480;

    VIDEOMODE_GRAPHICS_320:
        return 240;

    default:
        return 0;
    }
}

void bios_video_write(struct request_readwrite_stream_t* request)
{
    if (VIDEO_MODE != VIDEOMODE_TEXT_40) {
        return;
    }

    uint8_t* buffer = request->buf;
    for (uint32_t i = 0; i < request->len; ++i) {
        softterm_write_char((char)buffer[i]);
    }
}