#include "menu.h"
#include "button.h"
#include "display.h"

static int selected = 1;

static const char* LABELS[] = {
    "",
    "Status",
    "Spectrum",
    "Scanner",
    "Monitor",
    "Decoder",
    "Nodes",
    "Stats",
    "AutoTrack",
    "Standby",
};

void menu_init() { selected = 1; }

void menu_update() {
    if (button_short_pressed()) {
        selected++;
        if (selected >= MODE_COUNT) selected = 1;
    }
}

void menu_draw() {
    display_clear();
#if HAS_OLED
    display_draw_text_abs(25, 0, DISPLAY_CYAN, "Select Mode");
    display_draw_hline(0, 10, 128, DISPLAY_GRAY);
#else
    display_draw_text_abs(55, 15, DISPLAY_CYAN, "Select Mode");
    display_draw_hline(0, 20, 240, DISPLAY_GRAY);
#endif

    int itemsPerCol = (MODE_COUNT - 1 + 1) / 2;
    for (int i = 1; i < MODE_COUNT; i++) {
        int col = (i - 1) / itemsPerCol;
        int row = (i - 1) % itemsPerCol;
#if HAS_OLED
        int x = col * 64 + 2;
        int y = 14 + row * 9;
        
        if (i == selected) {
            display_fill_rect_abs(col * 64, y - 1, 62, 9, DISPLAY_CYAN);
            display_draw_text_small_abs(x, y, DISPLAY_BLACK, LABELS[i]);
        } else {
            display_draw_text_small_abs(x, y, DISPLAY_WHITE, LABELS[i]);
        }
#else
        int x = col * 120 + 4;
        int y = 36 + row * 17;
        
        if (i == selected) {
            display_fill_rect_abs(col * 120, y - 13, 118, 16, DISPLAY_CYAN);
            display_draw_text_abs(x, y, DISPLAY_BLACK, LABELS[i]);
        } else {
            display_fill_rect_abs(col * 120, y - 13, 118, 16, DISPLAY_BLACK);
            display_draw_text_abs(x, y, DISPLAY_WHITE, LABELS[i]);
        }
#endif
    }
    
#if HAS_OLED
    // Max 21 chars (128px / 6px). "S:next  L:enter" = 15 chars = 90px
    display_draw_text_small_abs(0, 56, DISPLAY_CYAN, "S:next   L:enter");
#else
    display_draw_text_small_abs(4, 126, DISPLAY_CYAN, "Short:next Long:select");
#endif
    
    display_update_buffer();
}

AppMode menu_selection() { return (AppMode)selected; }
