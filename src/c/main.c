#include <ctype.h>
#include <pebble.h>
//#include "main.h"

#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
#define KEY_CONDITIONS_DESC 2
#define SETTINGS_KEY 1

static Window *s_main_window;

static TextLayer *s_time_layer_l;
static TextLayer *s_time_layer_r;
static TextLayer *t_dialogue_text;
static TextLayer *t_temp, *t_day, *t_month, *t_year;
static TextLayer *t_dotw;
static Layer *window_layer;
static GFont transport_med_small;
static GFont transport_med;
static GFont helv;
static GFont seven_seg, seven_seg_sml;
static GFont fivenine;
static GFont noto;

static BitmapLayer *s_background_layer;

static GBitmap *s_background_bitmap, *s_on_bitmap, *s_bt_bitmap, *s_day_bitmap, *s_temp_bitmap;
static GBitmap *s_background_j_bitmap, *spritesheet;
static GBitmap *s_bat_bg_bitmap, *s_bat_bar_bitmap, *s_am_bitmap, *s_pm_bitmap, *image;
static GBitmap *three_worlds, *b_net_on, *b_moon1, *b_moon2, *b_moon3, *b_moon4;
static GBitmap *b_bat0, *b_bat1, *b_bat2, *b_bat3, *b_bat4, *b_bat5, *b_bat6, *b_bat7, *b_bat8;
static GBitmap *b_w_sun, *b_w_suncloud, *b_w_cloud, *b_w_rain, *b_w_snow, *b_w_lightning;
static GBitmap *b_lines, *b_net_off, *world;
static BitmapLayer *bl_w_sun, *bl_w_suncloud, *bl_w_cloud, *bl_w_rain, *bl_w_snow, *bl_w_lightning;
static BitmapLayer *bl_battery;
static BitmapLayer *bl_net_off;
static Layer *canvas;
static Layer *l_binary_clock;
static BitmapLayer *s_background_layer, *s_on_layer, *s_temp_layer, *s_am_layer, *s_pm_layer;
static BitmapLayer *bl_net_on, *bl_moon;
static BitmapLayer *bl_lines;
static struct tm *now = NULL;
static int s_battery_level;
static int s_last_battery_draw_pct = 100;
int last_warn = 0;
int WIDTH = 100;
int HEIGHT = 41;
int LOCAL_X = -1;
int LOCAL_Y = -1;
int REDRAW_INTERVAL = 15;
int redraw_counter;

typedef struct ClaySettings {
    bool fahrenheit;
    bool hideTemp;
    char customKey[32];
    char customLoc[32];
    char colourScheme[32];
    char secret[32];
    char bwBgColor[32];
} __attribute__((__packed__)) ClaySettings;

static ClaySettings settings;

static void default_settings() {
    settings.fahrenheit = false;
    settings.hideTemp = false;
    strcpy(settings.customKey, "");
    strcpy(settings.customLoc, "");
    strcpy(settings.colourScheme, "c");
    strcpy(settings.secret, "");
    strcpy(settings.bwBgColor, "b");
}

static void save_settings() {
    int wrote = persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void load_settings() {
    default_settings();
    int read = persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void set_warn(int warn) {
    layer_set_hidden(bitmap_layer_get_layer(bl_lines), warn == 0);
    if (warn != 7)
        last_warn = warn;
    switch (warn) {
    case 0:
        text_layer_set_text(t_dialogue_text, "OK");
        break;
    case 1:
        text_layer_set_text(t_dialogue_text, "Tornado");
        break;
    case 2:
        text_layer_set_text(t_dialogue_text, "Squall");
        break;
    case 3:
        text_layer_set_text(t_dialogue_text, "Haze");
        break;
    case 4:
        text_layer_set_text(t_dialogue_text, "Ash");
        break;
    case 5:
        text_layer_set_text(t_dialogue_text, "Sand");
        break;
    case 6:
        text_layer_set_text(t_dialogue_text, "Dust");
        break;
    case 7:
        text_layer_set_text(t_dialogue_text, "BT lost");
        break;
    case 8:
        text_layer_set_text(t_dialogue_text, "Smoke");
        break;
    case 9:
        text_layer_set_text(t_dialogue_text, "Mist");
        break;
    case 10:
        text_layer_set_text(t_dialogue_text, "Fog");
        break;
    }
}

static void bluetooth_callback(bool connected) {
    layer_set_hidden(bitmap_layer_get_layer(bl_net_off), connected);
    layer_set_hidden(bitmap_layer_get_layer(bl_net_on), !connected);

    if (!connected) {
        vibes_double_pulse();
        set_warn(7);
    } else {
        set_warn(last_warn);
    }
}

static void tornado_warn() {
    vibes_short_pulse();
}

static void outbox_iter() {
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    dict_write_uint8(iter, 0, 0);

    app_message_outbox_send();
}

static GColor clock_colour() {
#ifdef PBL_COLOR
    return (strcmp(settings.colourScheme, "m") == 0) ? GColorShockingPink : GColorCyan;
#else
    return GColorWhite;
#endif
}

static void update_temp_format() {
    if (!s_temp_layer)
        return;
    gbitmap_destroy(s_temp_bitmap);
    if (settings.fahrenheit && strcmp(settings.colourScheme, "c") == 0)
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPF);
    else if (settings.fahrenheit)
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPF2);
    else if (strcmp(settings.colourScheme, "c") == 0)
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPC);
    else
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPC2);
    bitmap_layer_set_bitmap(s_temp_layer, s_temp_bitmap);
}

static void update_date() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    static char dat_buffer[4];
    static char day_buffer[4];
    static char month_buffer[4];
    static char year_buffer[4];
#if defined(PBL_RECT)
#endif
    strftime(day_buffer, sizeof(day_buffer), "%d", tick_time);
    strftime(month_buffer, sizeof(month_buffer), "%m", tick_time);
    strftime(year_buffer, sizeof(year_buffer), "%y", tick_time);
    strftime(dat_buffer, sizeof(dat_buffer), "%a", tick_time);
    for (int i = 0; i < 4; i++) {
        dat_buffer[i] = toupper((unsigned char)dat_buffer[i]);
    }
    text_layer_set_text(t_dotw, dat_buffer);
    text_layer_set_text(t_day, day_buffer);
    text_layer_set_text(t_month, month_buffer);
    text_layer_set_text(t_year, year_buffer);
}

// 0 = off,
// 1 = CLEAR
// 2 = CLOUD
// 3 = PRECIP
// 4 = ATMO
static void set_weather(int status) {
    layer_set_hidden(bitmap_layer_get_layer(bl_w_sun), !(status == 0));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_suncloud), !(status == 1));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_cloud), !(status == 2));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_rain), !(status == 3));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_snow), !(status == 4));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_lightning), !(status == 5));
}

static void update_ht() {
    Layer *l, *r;
    l = text_layer_get_layer(s_time_layer_l);
    r = text_layer_get_layer(s_time_layer_r);
    if (!l || !r)
        return;
    if (strcmp(settings.secret, "ht") == 0) {
        layer_set_hidden(l, 1);
        layer_set_hidden(r, 1);
    } else {
        layer_set_hidden(l, 0);
        layer_set_hidden(r, 0);
    }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
char *translate_error(AppMessageResult result) {
    switch (result) {
    case APP_MSG_OK:
        return "APP_MSG_OK";
    case APP_MSG_SEND_TIMEOUT:
        return "APP_MSG_SEND_TIMEOUT";
    case APP_MSG_SEND_REJECTED:
        return "APP_MSG_SEND_REJECTED";
    case APP_MSG_NOT_CONNECTED:
        return "APP_MSG_NOT_CONNECTED";
    case APP_MSG_APP_NOT_RUNNING:
        return "APP_MSG_APP_NOT_RUNNING";
    case APP_MSG_INVALID_ARGS:
        return "APP_MSG_INVALID_ARGS";
    case APP_MSG_BUSY:
        return "APP_MSG_BUSY";
    case APP_MSG_BUFFER_OVERFLOW:
        return "APP_MSG_BUFFER_OVERFLOW";
    case APP_MSG_ALREADY_RELEASED:
        return "APP_MSG_ALREADY_RELEASED";
    case APP_MSG_CALLBACK_ALREADY_REGISTERED:
        return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
    case APP_MSG_CALLBACK_NOT_REGISTERED:
        return "APP_MSG_CALLBACK_NOT_REGISTERED";
    case APP_MSG_OUT_OF_MEMORY:
        return "APP_MSG_OUT_OF_MEMORY";
    case APP_MSG_CLOSED:
        return "APP_MSG_CLOSED";
    case APP_MSG_INTERNAL_ERROR:
        return "APP_MSG_INTERNAL_ERROR";
    default:
        return "UNKNOWN ERROR";
    }
}
static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
    outbox_iter();
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void update_battery() {
    if (s_battery_level >= 90)
        bitmap_layer_set_bitmap(bl_battery, b_bat8);
    else if (s_battery_level >= 80)
        bitmap_layer_set_bitmap(bl_battery, b_bat7);
    else if (s_battery_level >= 70)
        bitmap_layer_set_bitmap(bl_battery, b_bat6);
    else if (s_battery_level >= 60)
        bitmap_layer_set_bitmap(bl_battery, b_bat5);
    else if (s_battery_level >= 50)
        bitmap_layer_set_bitmap(bl_battery, b_bat4);
    else if (s_battery_level >= 30)
        bitmap_layer_set_bitmap(bl_battery, b_bat3);
    else if (s_battery_level >= 20)
        bitmap_layer_set_bitmap(bl_battery, b_bat2);
    else if (s_battery_level >= 10)
        bitmap_layer_set_bitmap(bl_battery, b_bat1);
    else
        bitmap_layer_set_bitmap(bl_battery, b_bat0);
    layer_set_hidden(bitmap_layer_get_layer(bl_battery), 0);
}

static void draw_earth() {
#ifdef PBL_SDK_2
    int now = (int)time(NULL) + time_offset;
#else
    int now = (int)time(NULL);
#endif
    float day_of_year;
    float time_of_day;
    int leap_years = (int)((float)now / 131487192.0);
    day_of_year = now - (((int)((float)now / 31556926.0) * 365 + leap_years) * 86400);
    day_of_year = day_of_year / 86400.0;
    time_of_day = day_of_year - (int)day_of_year;
    day_of_year = day_of_year / 365.0;
    int sun_x = (int)((float)TRIG_MAX_ANGLE * (1.0 - time_of_day));
    int sun_y = -sin_lookup((day_of_year - 0.2164) * TRIG_MAX_ANGLE) * .26 * .25;
    int x, y;
    for (x = 0; x < WIDTH; x++) {
        int x_angle = (int)((float)TRIG_MAX_ANGLE * (float)x / (float)(WIDTH));
        for (y = 0; y < HEIGHT; y++) {
            int y_angle = (int)((float)TRIG_MAX_ANGLE * (float)y / (float)(HEIGHT * 2)) - TRIG_MAX_ANGLE / 4;
            // spherical law of cosines
            float angle = ((float)sin_lookup(sun_y) / (float)TRIG_MAX_RATIO) * ((float)sin_lookup(y_angle) / (float)TRIG_MAX_RATIO);
            angle = angle + ((float)cos_lookup(sun_y) / (float)TRIG_MAX_RATIO) * ((float)cos_lookup(y_angle) / (float)TRIG_MAX_RATIO) * ((float)cos_lookup(sun_x - x_angle) / (float)TRIG_MAX_RATIO);
#ifdef PBL_BW
            // int byte = y * gbitmap_get_bytes_per_row(image) + (int)(x / 8);
            // if ((angle < 0) ^ (0x1 & (((char *)gbitmap_get_data(world_bitmap))[byte] >> (x % 8)))) {
            //     // white pixel
            //     ((char *)gbitmap_get_data(image))[byte] = ((char *)gbitmap_get_data(image))[byte] | (0x1 << (x % 8));
            // } else {
            //     // black pixel
            //     ((char *)gbitmap_get_data(image))[byte] = ((char *)gbitmap_get_data(image))[byte] & ~(0x1 << (x % 8));
            // }
#else
            int byte = y * gbitmap_get_bytes_per_row(three_worlds) + (int)(x / 2);
            if (angle < 0) { // dark pixel
                ((char *)gbitmap_get_data(three_worlds))[byte] = ((char *)gbitmap_get_data(three_worlds))[(int)(WIDTH * HEIGHT / 2) + byte];
                if (x == LOCAL_X && y == LOCAL_Y) {
                    //flip_color(1);
                }
            } else { // light pixel
                ((char *)gbitmap_get_data(three_worlds))[byte] = ((char *)gbitmap_get_data(three_worlds))[WIDTH * HEIGHT + byte];
                if (x == LOCAL_X && y == LOCAL_Y) {
                    //flip_color(0);
                }
            }
#endif
        }
    }
    layer_mark_dirty(canvas);
}

static void draw_map(struct Layer *layer, GContext *ctx) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, image, gbitmap_get_bounds(image));
}

static int moon_phase(int y, int m, int d) {
    //  calculates the moon phase (0-7), accurate to 1 segment.
    //  0 = > new moon.
    //  4 => full moon.

    int c, e;
    double jd;
    int b;

    if (m < 3) {
        y--;
        m += 12;
    }
    ++m;
    c = 365.25 * y;
    e = 30.6 * m;
    jd = c + e + d - 694039.09; // jd is total days elapsed
    jd /= 29.53;                // divide by the moon cycle (29.53 days)
    b = jd;                     // int(jd) -> b, take integer part of jd
    jd -= b;                    // subtract integer part to leave fractional part of original jd
    b = jd * 8 + 0.5;           // scale fraction from 0-8 and round by adding 0.5
    b = b & 7;                  // 0 and 8 are the same so turn 8 into 0
    return b;
}

static void update_moon() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    int day = tick_time->tm_mday;
    int month = tick_time->tm_mon + 1;
    int year = tick_time->tm_year + 1900;
    int phase = moon_phase(year, month, day);    
    printf("Phase is %d\n", phase);
    layer_set_hidden(bitmap_layer_get_layer(bl_moon), phase == 0);
    switch (phase) {
    case 4:
        bitmap_layer_set_bitmap(bl_moon, b_moon1);
        break;
    case 1:
        bitmap_layer_set_bitmap(bl_moon, b_moon4);
        break;
    case 7:
        bitmap_layer_set_bitmap(bl_moon, b_moon4);
        break;
    case 2:
        bitmap_layer_set_bitmap(bl_moon, b_moon3);
        break;
    case 6:
        bitmap_layer_set_bitmap(bl_moon, b_moon3);
        break;
    case 3:
        bitmap_layer_set_bitmap(bl_moon, b_moon2);
        break;
    case 5:
        bitmap_layer_set_bitmap(bl_moon, b_moon2);
        break;
    case 0:
        break;
    }
    
}

static void draw_digit(Layer *layer, GContext *ctx,
                       int col, int bits, int val) {
    const GRect bounds = layer_get_bounds(layer);
    const int16_t x_coord = 6 * col;
    GRect rect;
    int i;

    for (i = 0; i < bits; i++) {
        rect = GRect(x_coord, bounds.size.h - (6 * i + 4), 4, 4);
        if (val & 1)
            graphics_fill_rect(ctx, rect, 0, GCornersAll);

        val >>= 1;
    }
}

static void binary_update_proc(Layer *layer, GContext *ctx) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    graphics_context_set_fill_color(ctx, clock_colour());
    GRect bounds = GRect(0, 0, 4, 4);

    draw_digit(layer, ctx, 0, 2, tick_time->tm_hour / 10);
    draw_digit(layer, ctx, 1, 4, tick_time->tm_hour % 10);
    draw_digit(layer, ctx, 2, 3, tick_time->tm_min / 10);
    draw_digit(layer, ctx, 3, 4, tick_time->tm_min % 10);
}
static void update_time() {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    static char s_buffer_l[4];
    static char s_buffer_r[4];

    strftime(s_buffer_l, sizeof(s_buffer_l), clock_is_24h_style() ? "%H" : "%I", tick_time);
    strftime(s_buffer_r, sizeof(s_buffer_r), "%M", tick_time);
    text_layer_set_text(s_time_layer_r, s_buffer_r);
    text_layer_set_text(s_time_layer_l, s_buffer_l);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {

    int hour = tick_time->tm_hour;
    if (units_changed & DAY_UNIT) {
        update_date();
        update_moon();
    }

    if (units_changed & HOUR_UNIT)
        outbox_iter();
    layer_set_hidden(bitmap_layer_get_layer(s_pm_layer), hour < 12);
    layer_set_hidden(bitmap_layer_get_layer(s_am_layer), hour >= 12);
    redraw_counter++;
    if (redraw_counter >= REDRAW_INTERVAL) {
        draw_earth();
        redraw_counter = 0;
    }
    if (units_changed & MINUTE_UNIT)
        update_time();
}

// i know this is fucked but i get a horrible bug if i do anything else!
static void main_window_load(Window *window) {
    window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    s_background_layer = bitmap_layer_create(bounds);
    int x = 0;
#ifdef PBL_BW
    x = 1;
#endif
    if (strcmp(settings.colourScheme, "c") == 0 || x) {
        
        three_worlds = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_THREE_WORLDS);
        if ((x && strcmp(settings.bwBgColor, "b") == 0) || strcmp(settings.colourScheme, "c") == 0)
            s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND1);
        else
            s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND2);
        b_lines = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WARN);
        b_w_sun = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN);
        b_w_suncloud = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNCLOUD);
        b_w_cloud = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOUD);
        b_w_rain = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RAIN);
        b_w_snow = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SNOW);
        b_w_lightning = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LIGHTNING);

        // b_moon1 = gbitmap_create_as_sub_bitmap(spritesheet, GRect(2, 184, 16, 37));
        // b_moon2 = gbitmap_create_as_sub_bitmap(spritesheet, GRect(2, 221, 16, 37));
        // b_moon3 = gbitmap_create_as_sub_bitmap(spritesheet, GRect(2, 258, 16, 37));
        // b_moon4 = gbitmap_create_as_sub_bitmap(spritesheet, GRect(2, 195, 16, 37));
        b_moon1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON1);
        b_moon2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON2);
        b_moon3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON3);
        b_moon4 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON4);

        b_bat0 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT0);
        b_bat1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT1);
        b_bat2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT2);
        b_bat3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT3);
        b_bat4 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT4);
        b_bat5 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT5);
        b_bat6 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT6);
        b_bat7 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT7);
        b_bat8 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT8);

        b_net_on = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NET_ON);
        b_net_off = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NET_OFF);
        s_am_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_AM);
        s_pm_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PM);
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPF);
    } else if (strcmp(settings.colourScheme, "b") == 0) {
        s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND3);
        three_worlds = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_THREE_WORLDS2);

        b_lines = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WARN);
        b_w_sun = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN2);
        b_w_suncloud = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNCLOUD2);
        b_w_cloud = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOUD2);
        b_w_rain = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RAIN2);
        b_w_snow = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SNOW2);
        b_w_lightning = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LIGHTNING2);

        b_moon1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON12);
        b_moon2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON22);
        b_moon3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON32);
        b_moon4 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON42);

        b_bat0 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT0);
        b_bat1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT1);
        b_bat2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT2);
        b_bat3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT3);
        b_bat4 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT4);
        b_bat5 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT5);
        b_bat6 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT6);
        b_bat7 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT7);
        b_bat8 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT8);

        b_net_on = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NET_ON2);
        b_net_off = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NET_OFF2);
        s_am_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_AM);
        s_pm_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PM);
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPF2);
    } else {
        s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND2);
        three_worlds = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_THREE_WORLDS2);

        b_lines = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_WARN);
        b_w_sun = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN2);
        b_w_suncloud = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUNCLOUD2);
        b_w_cloud = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CLOUD2);
        b_w_rain = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RAIN2);
        b_w_snow = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SNOW2);
        b_w_lightning = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_LIGHTNING2);

        b_moon1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON12);
        b_moon2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON22);
        b_moon3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON32);
        b_moon4 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON42);

        b_bat0 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT02);
        b_bat1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT12);
        b_bat2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT22);
        b_bat3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT32);
        b_bat4 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT42);
        b_bat5 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT52);
        b_bat6 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT62);
        b_bat7 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT72);
        b_bat8 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BAT82);

        b_net_on = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NET_ON2);
        b_net_off = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_NET_OFF2);
        s_am_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_AM2);
        s_pm_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PM2);
        s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPF2);
    }
    bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
    // s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ICON);
    seven_seg = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DSEG_33));
    seven_seg_sml = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DSEG_14));
    helv = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_HELV_10));
    fivenine = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FIVEXNINE_16));

    s_time_layer_l = text_layer_create(GRect(PBL_IF_RECT_ELSE(2, 55), PBL_IF_RECT_ELSE(4, 72), bounds.size.w, 50));
    text_layer_set_background_color(s_time_layer_l, GColorClear);

    text_layer_set_text_color(s_time_layer_l, clock_colour());
    text_layer_set_font(s_time_layer_l, seven_seg);
    text_layer_set_text_alignment(s_time_layer_l, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer_l));

    s_time_layer_r = text_layer_create(GRect(PBL_IF_RECT_ELSE(56, 110), PBL_IF_RECT_ELSE(4, 72), bounds.size.w, 50));
    text_layer_set_background_color(s_time_layer_r, GColorClear);
    text_layer_set_text_color(s_time_layer_r, clock_colour());

    text_layer_set_font(s_time_layer_r, seven_seg);
    text_layer_set_text_alignment(s_time_layer_r, GTextAlignmentLeft);

    layer_add_child(window_layer, text_layer_get_layer(s_time_layer_r));
    canvas = layer_create(GRect(40, 42, bounds.size.w, bounds.size.h));
    layer_set_update_proc(canvas, draw_map);
    layer_add_child(window_layer, canvas);
    image = gbitmap_create_as_sub_bitmap(three_worlds, GRect(0, 0, WIDTH, HEIGHT));
    draw_earth();
    // s_bt_icon_layer = bitmap_layer_create(GRect(15, 63, 30, 30));

    GRect bin_bounds = GRect(116, 130, 22, 22);
    l_binary_clock = layer_create(bin_bounds);
    layer_set_update_proc(l_binary_clock, binary_update_proc);
    layer_add_child(window_get_root_layer(window), l_binary_clock);

    int pos_am_pm = 105;

    t_dotw = text_layer_create(GRect(PBL_IF_RECT_ELSE(110, 0), 23, bounds.size.w, 50));
    text_layer_set_background_color(t_dotw, GColorClear);
    text_layer_set_text_color(t_dotw, clock_colour());
    text_layer_set_font(t_dotw, seven_seg_sml);
    text_layer_set_text_alignment(t_dotw, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(t_dotw));
    text_layer_set_text(t_dotw, "");

    t_dialogue_text = text_layer_create(GRect(PBL_IF_RECT_ELSE(7, 0), 105, bounds.size.w, 50));
    text_layer_set_background_color(t_dialogue_text, GColorClear);
    text_layer_set_text_color(t_dialogue_text, clock_colour());
    text_layer_set_font(t_dialogue_text, helv);
    text_layer_set_text_alignment(t_dialogue_text, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(t_dialogue_text));
    text_layer_set_text(t_dialogue_text, "");
    //layer_set_hidden(text_layer_get_layer(t_dialogue_text), 1);

    bl_lines = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(51, 0), PBL_IF_RECT_ELSE(120, 0), 55, 11));
    bitmap_layer_set_bitmap(bl_lines, b_lines);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_lines));

    bl_w_sun = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(2, 0), PBL_IF_RECT_ELSE(136, 0), 15, 15));
    bitmap_layer_set_bitmap(bl_w_sun, b_w_sun);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_w_sun));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_sun), 1);

    bl_w_suncloud = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(19, 0), PBL_IF_RECT_ELSE(136, 0), 15, 15));
    bitmap_layer_set_bitmap(bl_w_suncloud, b_w_suncloud);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_w_suncloud));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_suncloud), 1);

    bl_w_cloud = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(35, 0), PBL_IF_RECT_ELSE(138, 0), 16, 12));
    bitmap_layer_set_bitmap(bl_w_cloud, b_w_cloud);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_w_cloud));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_cloud), 1);

    bl_w_rain = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(53, 0), PBL_IF_RECT_ELSE(138, 0), 13, 13));
    bitmap_layer_set_bitmap(bl_w_rain, b_w_rain);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_w_rain));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_rain), 1);

    bl_w_snow = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(68, 0), PBL_IF_RECT_ELSE(138, 0), 13, 13));
    bitmap_layer_set_bitmap(bl_w_snow, b_w_snow);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_w_snow));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_snow), 1);

    bl_w_lightning = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(84, 0), PBL_IF_RECT_ELSE(138, 0), 10, 13));
    bitmap_layer_set_bitmap(bl_w_lightning, b_w_lightning);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_w_lightning));
    layer_set_hidden(bitmap_layer_get_layer(bl_w_lightning), 1);

    bl_moon = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(3, 0), PBL_IF_RECT_ELSE(44, 0), 16, 37));
    bitmap_layer_set_bitmap(bl_moon, b_moon1);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_moon));
    layer_set_hidden(bitmap_layer_get_layer(bl_moon), 1);

    bl_battery = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(113, 0), PBL_IF_RECT_ELSE(89, 0), 10, 33));
    bitmap_layer_set_bitmap(bl_battery, b_bat5);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_battery));
    layer_set_hidden(bitmap_layer_get_layer(bl_battery), 1);

    bl_net_on = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(1, 0), PBL_IF_RECT_ELSE(160, 0), 9, 7));
    bitmap_layer_set_bitmap(bl_net_on, b_net_on);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_net_on));
    layer_set_hidden(bitmap_layer_get_layer(bl_net_on), 1);

    bl_net_off = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(1, 0), PBL_IF_RECT_ELSE(160, 0), 7, 7));
    bitmap_layer_set_bitmap(bl_net_off, b_net_off);
    layer_add_child(window_layer, bitmap_layer_get_layer(bl_net_off));
    layer_set_hidden(bitmap_layer_get_layer(bl_net_off), 1);

    s_am_layer = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(pos_am_pm, 0), PBL_IF_RECT_ELSE(4, 0), 26, 12));
    bitmap_layer_set_bitmap(s_am_layer, s_am_bitmap);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_am_layer));
    layer_set_hidden(bitmap_layer_get_layer(s_am_layer), 1);

    s_pm_layer = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(pos_am_pm + 16, 0), PBL_IF_RECT_ELSE(4, 0), 26, 12));
    bitmap_layer_set_bitmap(s_pm_layer, s_pm_bitmap);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_pm_layer));
    layer_set_hidden(bitmap_layer_get_layer(s_pm_layer), 1);

    t_temp = text_layer_create(GRect(PBL_IF_RECT_ELSE(-95, 0), PBL_IF_RECT_ELSE(149, 0), bounds.size.w, 50));
    text_layer_set_background_color(t_temp, GColorClear);
    text_layer_set_text_color(t_temp, clock_colour());
    text_layer_set_font(t_temp, fivenine);
    text_layer_set_text_alignment(t_temp, GTextAlignmentRight);
    text_layer_set_text(t_temp, "?");
    layer_add_child(window_layer, text_layer_get_layer(t_temp));

    t_day = text_layer_create(GRect(PBL_IF_RECT_ELSE(62, 0), PBL_IF_RECT_ELSE(149, 0), bounds.size.w, 50));
    text_layer_set_background_color(t_day, GColorClear);
    text_layer_set_text_color(t_day, clock_colour());
    text_layer_set_font(t_day, fivenine);
    text_layer_set_text_alignment(t_day, GTextAlignmentLeft);
    text_layer_set_text(t_day, "00 00 00");
    layer_add_child(window_layer, text_layer_get_layer(t_day));

    t_month = text_layer_create(GRect(PBL_IF_RECT_ELSE(75, 0), PBL_IF_RECT_ELSE(149, 0), bounds.size.w, 50));
    text_layer_set_background_color(t_month, GColorClear);
    text_layer_set_text_color(t_month, clock_colour());
    text_layer_set_font(t_month, fivenine);
    text_layer_set_text_alignment(t_month, GTextAlignmentLeft);
    text_layer_set_text(t_month, "00 00 00");
    layer_add_child(window_layer, text_layer_get_layer(t_month));

    t_year = text_layer_create(GRect(PBL_IF_RECT_ELSE(88, 0), PBL_IF_RECT_ELSE(149, 0), bounds.size.w, 50));
    text_layer_set_background_color(t_year, GColorClear);
    text_layer_set_text_color(t_year, clock_colour());
    text_layer_set_font(t_year, fivenine);
    text_layer_set_text_alignment(t_year, GTextAlignmentLeft);
    text_layer_set_text(t_year, "00 00 00");
    layer_add_child(window_layer, text_layer_get_layer(t_year));

    // vvvvv OLD vvvvv

    GRect boundsc = GRect(49, 156, 5, 11);

    // s_temp_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TEMPF);
    s_temp_layer = bitmap_layer_create(boundsc);
    bitmap_layer_set_bitmap(s_temp_layer, s_temp_bitmap);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_temp_layer));

    const time_t now_time = time(NULL);
    tick_handler(localtime(&now_time), MINUTE_UNIT);
    bluetooth_callback(connection_service_peek_pebble_app_connection());
    update_battery();
    // update_jp();
    update_temp_format();
    update_ht();
    update_date();
    //update_colours();
    update_moon();
    outbox_iter();
}

static void main_window_unload(Window *window) {
    gbitmap_destroy(s_background_bitmap);
    gbitmap_destroy(three_worlds);
    gbitmap_destroy(b_lines);
    gbitmap_destroy(b_w_sun);
    gbitmap_destroy(b_w_suncloud);
    gbitmap_destroy(b_w_cloud);
    gbitmap_destroy(b_w_rain);
    gbitmap_destroy(b_w_snow);
    gbitmap_destroy(b_w_lightning);
    gbitmap_destroy(b_moon1);
    gbitmap_destroy(b_moon2);
    gbitmap_destroy(b_moon3);
    gbitmap_destroy(b_moon4);
    gbitmap_destroy(b_bat0);
    gbitmap_destroy(b_bat1);
    gbitmap_destroy(b_bat2);
    gbitmap_destroy(b_bat3);
    gbitmap_destroy(b_bat4);
    gbitmap_destroy(b_bat5);
    gbitmap_destroy(b_bat6);
    gbitmap_destroy(b_bat7);
    gbitmap_destroy(b_bat8);
    gbitmap_destroy(b_net_on);
    gbitmap_destroy(b_net_off);
    gbitmap_destroy(s_am_bitmap);
    gbitmap_destroy(s_pm_bitmap);
    gbitmap_destroy(s_temp_bitmap);
    gbitmap_destroy(image);

    bitmap_layer_destroy(s_temp_layer);
    bitmap_layer_destroy(s_background_layer);
    bitmap_layer_destroy(bl_moon);
    bitmap_layer_destroy(bl_w_sun);
    bitmap_layer_destroy(bl_w_suncloud);
    bitmap_layer_destroy(bl_w_cloud);
    bitmap_layer_destroy(bl_w_rain);
    bitmap_layer_destroy(bl_w_snow);
    bitmap_layer_destroy(bl_w_lightning);
    bitmap_layer_destroy(bl_net_off);
    bitmap_layer_destroy(bl_net_on);
    bitmap_layer_destroy(bl_lines);
    bitmap_layer_destroy(s_on_layer);
    bitmap_layer_destroy(bl_battery);
    bitmap_layer_destroy(s_am_layer);
    bitmap_layer_destroy(s_pm_layer);

    text_layer_destroy(s_time_layer_l);
    text_layer_destroy(s_time_layer_r);
    text_layer_destroy(t_dialogue_text);
    text_layer_destroy(t_temp);
    text_layer_destroy(t_day);
    text_layer_destroy(t_month);
    text_layer_destroy(t_year);
    text_layer_destroy(t_dotw);
    // bitmap_layer_destroy(s_bat_bg_layer);
    layer_destroy(canvas);
    layer_destroy(l_binary_clock);

    fonts_unload_custom_font(helv);
    fonts_unload_custom_font(seven_seg);
    fonts_unload_custom_font(fivenine);
    fonts_unload_custom_font(seven_seg_sml);
}

static void window_disappear(Window *window) {
    bluetooth_connection_service_unsubscribe();
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
}

static void battery_callback(BatteryChargeState state) {
    s_battery_level = state.charge_percent;
    update_battery();
}
static void window_appear(Window *window) {
    const time_t now_time = time(NULL);

    tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT | HOUR_UNIT, tick_handler);

    battery_state_service_subscribe(battery_callback);
    battery_callback(battery_state_service_peek());
    connection_service_subscribe((ConnectionHandlers){
        .pebble_app_connection_handler = bluetooth_callback});
    outbox_iter();
}

static void init();
static void deinit();

static void inbox_received_callback(DictionaryIterator *iterator,
                                    void *context) {
    static char temperature_buffer[8];
    static char conditions_buffer[32];
    static char conditions_desc_buffer[64];
    static char weather_layer_buffer[32];

    Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
    Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);
    Tuple *conditions_desc_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS_DESC);

    Tuple *fahrenheit_t = dict_find(iterator, MESSAGE_KEY_fahrenheit);
    if (fahrenheit_t) {
        settings.fahrenheit = fahrenheit_t->value->int32 == 1;
        persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
        update_temp_format();

    }

    Tuple *customKey_t = dict_find(iterator, MESSAGE_KEY_customKey);
    if (customKey_t) {
        strcpy(settings.customKey, customKey_t->value->cstring);
        
        save_settings();
    }

    Tuple *customLoc_t = dict_find(iterator, MESSAGE_KEY_customLoc);
    if (customLoc_t) {
        strcpy(settings.customLoc, customLoc_t->value->cstring);
        save_settings();
    }

    Tuple *colourScheme_t = dict_find(iterator, MESSAGE_KEY_colourScheme);
    if (colourScheme_t) {
        strcpy(settings.colourScheme, colourScheme_t->value->cstring);
        //update_colours();
        // update_jp();
        save_settings();
        main_window_unload(s_main_window);
        main_window_load(s_main_window);
    }

    Tuple *secret_t = dict_find(iterator, MESSAGE_KEY_secret);
    if (secret_t) {
        strcpy(settings.secret, secret_t->value->cstring);
        save_settings();
        update_ht();
    }

    Tuple *bwBgColor_t = dict_find(iterator, MESSAGE_KEY_bwBgColor);
    if (bwBgColor_t) {
        strcpy(settings.bwBgColor, bwBgColor_t->value->cstring);
        save_settings();
    }
    if (fahrenheit_t || customKey_t || customLoc_t || colourScheme_t || secret_t)
        outbox_iter();

    if (temp_tuple && conditions_tuple && conditions_desc_tuple) {
        double temp = (int)temp_tuple->value->int32 / 10;
        int weather_category = 0;
        if (settings.fahrenheit)
            temp = ((9 * temp) / 5) + 32;
        if (settings.fahrenheit)
            snprintf(temperature_buffer, sizeof(temperature_buffer), "%d", (int)temp);
        else
            snprintf(temperature_buffer, sizeof(temperature_buffer), "%d", (int)temp);
        snprintf(conditions_buffer, sizeof(conditions_buffer), "%.3s", conditions_tuple->value->cstring);
        snprintf(conditions_desc_buffer, sizeof(conditions_desc_buffer), "%.3s", conditions_desc_tuple->value->cstring);
        text_layer_set_text(t_temp, temperature_buffer);
        if (strcmp(conditions_buffer, "Cle") == 0) {
            weather_category = 0;
        } else if (strcmp(conditions_buffer, "Clo") == 0) {
            if (strcmp(conditions_desc_buffer, "few") == 0)
                weather_category = 1;
            else
                weather_category = 2;
        } else if (strcmp(conditions_buffer, "Thu") == 0) {
            weather_category = 5;
        } else if (strcmp(conditions_buffer, "Dri") == 0) {
            weather_category = 3;
        } else if (strcmp(conditions_buffer, "Rai") == 0) {
            weather_category = 3;
        } else if (strcmp(conditions_buffer, "Fog") == 0) {
            weather_category = 7;
            set_warn(10);
        } else if (strcmp(conditions_buffer, "Sno") == 0) {
            weather_category = 4;
        } else if (strcmp(conditions_buffer, "Mis") == 0) {
            weather_category = 7;
            set_warn(9);
        } else if (strcmp(conditions_buffer, "Smo") == 0) {
            weather_category = 6;
            set_warn(8);
        } else if (strcmp(conditions_buffer, "Haz") == 0) {
            weather_category = 7;
            set_warn(3);
        } else if (strcmp(conditions_buffer, "Dus") == 0) {
            weather_category = 6;
            set_warn(1);
        } else if (strcmp(conditions_buffer, "San") == 0) {
            weather_category = 6;
            set_warn(5);
        } else if (strcmp(conditions_buffer, "Ash") == 0) {
            weather_category = 6;
            set_warn(4);
        } else if (strcmp(conditions_buffer, "Squ") == 0) {
            weather_category = 6;
            set_warn(2);
        } else if (strcmp(conditions_buffer, "Tor") == 0) {
            weather_category = 6;
            set_warn(1);
            tornado_warn();
        }
        if (weather_category != 6 && weather_category != 7)
            set_warn(0);
        set_weather(weather_category);
    }
}

static void init() {
    load_settings();

    redraw_counter++;

    const uint32_t inbox_size = 128;
    const uint32_t outbox_size = 128;
    s_main_window = window_create();

    app_message_register_inbox_received(inbox_received_callback);
    app_message_open(inbox_size, outbox_size);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);

    window_set_window_handlers(
        s_main_window,
        (WindowHandlers){
            .load = main_window_load,
            .appear = window_appear,
            .disappear = window_disappear,
            .unload = main_window_unload});

    window_stack_push(s_main_window, true);
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
    init();
    app_event_loop();
    deinit();
}