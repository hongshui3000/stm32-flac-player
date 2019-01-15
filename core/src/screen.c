#include "core/include/screen.h"

#include <memory.h>
#include <stdlib.h>

#include "Drivers/BSP/STM32746G-Discovery/stm32746g_discovery_lcd.h"
#include "Drivers/BSP/STM32746G-Discovery/stm32746g_discovery_ts.h"
#include "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.h"

#define LCD_X_SIZE RK043FN48H_WIDTH
#define LCD_Y_SIZE RK043FN48H_HEIGHT

#define COUNT_OF(x) (sizeof(x)/sizeof(x[0]))

static uint8_t lcd_buffer_0[LCD_X_SIZE * LCD_Y_SIZE * 4] __attribute__((section(".sdram")));
static uint8_t lcd_buffer_1[LCD_X_SIZE * LCD_Y_SIZE * 4] __attribute__((section(".sdram")));

static int visible_layer = 0;

static const Point icon_back_position = {98,
                                         172};
static const Point icon_back_points_1[] = {
    {16, 16},
    {20, 16},
    {20, 47},
    {16, 47}
};
static const Point icon_back_points_2[] = {
    {26, 31},
    {47, 16},
    {47, 47},
    {26, 32}
};

static const Point icon_next_position = {318,
                                         172};
static const Point icon_next_points_1[] = {
    {16, 16},
    {37, 31},
    {37, 32},
    {16, 47}
};
static const Point icon_next_points_2[] = {
    {43, 16},
    {47, 16},
    {47, 47},
    {43, 47}
};

static const Point icon_play_pause_position = {192, 156};
static const Point icon_play_pause_circle_center = {48, 48};
static const int icon_play_pause_circle_radius = 40;
static const Point icon_play_points[] = {
    {40, 30},
    {62, 47},
    {62, 48},
    {40, 65}
};
static const Point icon_pause_points_1[] = {
    {36, 32},
    {43, 32},
    {43, 63},
    {36, 63}
};
static const Point icon_pause_points_2[] = {
    {52, 32},
    {59, 32},
    {59, 63},
    {52, 63}
};

static const Point progress_position = {17, 132};
static const Point progress_size = {446, 8};

typedef struct {
    const Point position;
    const Point size;
    bool is_touched;
    unsigned last_changed_at;
    bool was_touched_event;
} Button;

static Button button_play_pause = {
    .position = {192, 156},
    .size = {96, 96},
    .is_touched = false,
    .last_changed_at = 0
};

static Button button_back = {
    .position = {98, 172},
    .size = {64, 64},
    .is_touched = false,
    .last_changed_at = 0
};

static Button button_next = {
    .position = {318, 172},
    .size = {64, 64},
    .is_touched = false,
    .last_changed_at = 0
};

static void SwapLayers(void) {
    // wait for VSYNC
    while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS)) {}

    if (visible_layer == 0) {
        visible_layer = 1;
        BSP_LCD_SetLayerVisible(1, ENABLE);
        BSP_LCD_SetLayerVisible(0, DISABLE);
        BSP_LCD_SelectLayer(0);
    } else {
        visible_layer = 0;
        BSP_LCD_SetLayerVisible(0, ENABLE);
        BSP_LCD_SetLayerVisible(1, DISABLE);
        BSP_LCD_SelectLayer(1);
    }
}

void Screen_Initialize(void) {
    /* LCD */
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, (uint32_t) lcd_buffer_0);
    BSP_LCD_LayerDefaultInit(1, (uint32_t) lcd_buffer_1);

    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_WHITE);
    BSP_LCD_SelectLayer(1);
    BSP_LCD_Clear(LCD_COLOR_WHITE);

    visible_layer = 0;
    SwapLayers();

    BSP_LCD_DisplayOn();

    /* Touch screen */
    BSP_TS_Init(LCD_X_SIZE, LCD_Y_SIZE);
}

static void LcdFillPolygon(Point position, const Point *points, uint16_t point_count) {
    Point *translated_points = malloc(point_count * sizeof(Point));
    for (int i = 0; i < point_count; i++) {
        translated_points[i] = (Point) {
            .X = points[i].X + position.X,
            .Y = points[i].Y + position.Y
        };
    }
    BSP_LCD_FillPolygon(translated_points, point_count);
    free(translated_points);
}

void Screen_RenderInfo(const char *info) {
    BSP_LCD_Clear(LCD_COLOR_WHITE);

    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_DisplayStringAt(0, LCD_Y_SIZE / 2, (uint8_t *) info, CENTER_MODE);

    SwapLayers();
}

void Screen_RenderPlayer(
    int number_of_files,
    int current_file_index,
    const char *current_file_name,
    int progress,
    bool is_playing
) {
    BSP_LCD_Clear(LCD_COLOR_WHITE);

    // back
    BSP_LCD_SetTextColor(button_back.is_touched ? LCD_COLOR_GRAY : LCD_COLOR_BLACK);
    LcdFillPolygon(icon_back_position, icon_back_points_1, COUNT_OF(icon_back_points_1));
    LcdFillPolygon(icon_back_position, icon_back_points_2, COUNT_OF(icon_back_points_2));

    // next
    BSP_LCD_SetTextColor(button_next.is_touched ? LCD_COLOR_GRAY : LCD_COLOR_BLACK);
    LcdFillPolygon(icon_next_position, icon_next_points_1, COUNT_OF(icon_next_points_1));
    LcdFillPolygon(icon_next_position, icon_next_points_2, COUNT_OF(icon_next_points_2));

    // play / pause
    BSP_LCD_SetTextColor(button_play_pause.is_touched ? LCD_COLOR_GRAY : LCD_COLOR_BLACK);
    BSP_LCD_FillCircle((uint16_t) (icon_play_pause_position.X + icon_play_pause_circle_center.X),
                       (uint16_t) (icon_play_pause_position.Y + icon_play_pause_circle_center.Y),
                       (uint16_t) icon_play_pause_circle_radius);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    if (is_playing) {
        LcdFillPolygon(icon_play_pause_position, icon_pause_points_1, COUNT_OF(icon_pause_points_1));
        LcdFillPolygon(icon_play_pause_position, icon_pause_points_2, COUNT_OF(icon_pause_points_2));
    } else {
        LcdFillPolygon(icon_play_pause_position, icon_play_points, COUNT_OF(icon_play_points));
    }

    // progress
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
    BSP_LCD_DrawRect((uint16_t) progress_position.X,
                     (uint16_t) progress_position.Y,
                     (uint16_t) progress_size.X,
                     (uint16_t) progress_size.Y);
    if (progress > 0) {
        BSP_LCD_FillRect((uint16_t) progress_position.X,
                         (uint16_t) progress_position.Y,
                         (uint16_t) (progress_size.X * progress / 1000),
                         (uint16_t) progress_size.Y);
    }

    if (current_file_name) {
        BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
        BSP_LCD_DisplayStringAt(17, 91, (uint8_t *) current_file_name, LEFT_MODE);
    }

    if (current_file_index != -1 && number_of_files != -1) {
        char text[32];
        snprintf(text, COUNT_OF(text), "%d/%d", current_file_index + 1, number_of_files);
        BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
        BSP_LCD_DisplayStringAt(17, 60, (uint8_t *) text, LEFT_MODE);
    }

    SwapLayers();
}

static bool IsRegionTouched(const TS_StateTypeDef *touch_state, Point position, Point size) {
    if (touch_state->touchDetected == 0) return false;

    unsigned x = touch_state->touchX[0];
    unsigned y = touch_state->touchY[0];

    return x >= position.X &&
           x <= position.X + size.X &&
           y >= position.Y &&
           y <= position.Y + size.Y;
}

void Screen_HandleTouch(void) {
    unsigned time = osKernelSysTick();
    TS_StateTypeDef touch_state;
    BSP_TS_GetState(&touch_state);

    Button *buttons[] = {&button_back, &button_next, &button_play_pause, NULL};
    for (int i = 0; buttons[i]; i++) {
        Button *button = buttons[i];

        bool is_touched = IsRegionTouched(&touch_state, button->position, button->size);
        if (is_touched != button->is_touched) {
            if (time - button->last_changed_at >= 100) {
                if (!button->is_touched && is_touched) {
                    button->was_touched_event = true;
                }
                button->is_touched = is_touched;
            }
            button->last_changed_at = time;
        }
    }
}

bool Screen_IsBackButtonTouched(void) {
    bool val = button_back.was_touched_event;
    button_back.was_touched_event = false;
    return val;
}

bool Screen_IsPlayPauseButtonTouched(void) {
    bool val = button_play_pause.was_touched_event;
    button_play_pause.was_touched_event = false;
    return val;
}

bool Screen_IsNextButtonTouched(void) {
    bool val = button_next.was_touched_event;
    button_next.was_touched_event = false;
    return val;
}
