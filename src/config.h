#ifndef CONFIG_H
#define CONFIG_H

/* Vita screen dimensions */
#define SCREEN_W 960
#define SCREEN_H 544

/* UI layout */
#define HEADER_H 50
#define TAB_BAR_H 60
#define CONTENT_Y (HEADER_H + 10)
#define CONTENT_BOTTOM (SCREEN_H - TAB_BAR_H)
#define CONTENT_H (CONTENT_BOTTOM - CONTENT_Y)

/* Colors (RGBA) */
#define COLOR_BG        RGBA8(24, 24, 32, 255)
#define COLOR_HEADER    RGBA8(33, 33, 48, 255)
#define COLOR_TAB_BG    RGBA8(33, 33, 48, 255)
#define COLOR_TAB_SEL   RGBA8(66, 133, 244, 255)
#define COLOR_CARD_BG   RGBA8(44, 44, 60, 255)
#define COLOR_CARD_SEL  RGBA8(55, 55, 80, 255)
#define COLOR_ON        RGBA8(66, 200, 120, 255)
#define COLOR_OFF       RGBA8(120, 120, 140, 255)
#define COLOR_TEXT       RGBA8(240, 240, 245, 255)
#define COLOR_TEXT_DIM   RGBA8(160, 160, 175, 255)
#define COLOR_ACCENT    RGBA8(66, 133, 244, 255)
#define COLOR_WARM      RGBA8(255, 180, 50, 255)
#define COLOR_RED       RGBA8(220, 60, 60, 255)

/* Tabs */
#define TAB_HOME    0
#define TAB_LIGHTS  1
#define TAB_CLIMATE 2
#define TAB_SCENES  3
#define TAB_DEVICES 4
#define TAB_COUNT   5

/* Max entities */
#define MAX_ENTITIES 64

/* Card dimensions */
#define CARD_H 65
#define CARD_PAD 8
#define CARD_FULL_H (CARD_H + CARD_PAD)

#endif
