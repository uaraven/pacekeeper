#include "pebble.h"

#define REPEAT_INTERVAL_MS 50
#define LONG_CLICK_INTERVAL_MS 500

// This is a custom defined key for saving our count field
#define PACE_PKEY 1
// Custom key for saving inversion status
#define INVERT_PKEY 2

// You can define defaults for values in persistent storage
#define PACE_DEFAULT 90

static Window *s_main_window;

static ActionBarLayer *s_action_bar;
static InverterLayer *s_inverter_layer;
static TextLayer *s_header_layer, *s_body_layer, *s_double_layer;
static BitmapLayer *s_steps_layer;
static GBitmap *s_icon_plus, *s_icon_minus, *s_icon_run, *s_icon_stop;
static GBitmap *s_icon_main, *s_icon_other;

static int s_pace = PACE_DEFAULT;

static int s_bzz_factor = 0;
static int s_step_interval = 0;
static int s_bzz_count = 0;

static bool s_running = false;
static bool s_main_leg = true; // longer buzz for main leg, shorter for other leg
static bool s_inverted = true;

static void update_text() {
  static char s_text[6];
  static char s_double[6];
	
  snprintf(s_text, sizeof(s_text), "%03d", s_pace*2);
  text_layer_set_text(s_body_layer, s_text);
	
  snprintf(s_double, sizeof(s_double), "%03d", s_pace);
  text_layer_set_text(s_double_layer, s_double);
}

static void calc_bzz() {
    s_step_interval = 60 * 1000 / (s_pace * 2);
    int bzz = s_step_interval;
    int factor = 1;
    while (bzz*factor < 1000) {
        factor *= 2;
    }
    s_bzz_factor = factor / 2 + 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "step interval: %d, bzz_factor: %d", s_step_interval, s_bzz_factor);
}

static void start_step_timer();

static void step_timer_callback(void *data) {
    
    APP_LOG(APP_LOG_LEVEL_DEBUG, "main leg? %d, factor count: %d", s_main_leg, s_bzz_count);
    
    if (s_main_leg) {
        if (s_bzz_count == 1) {
            vibes_double_pulse();
        }
        bitmap_layer_set_bitmap(s_steps_layer, s_icon_main);
    } else {
        if (s_bzz_count == 1) {
            vibes_short_pulse();
        }
        bitmap_layer_set_bitmap(s_steps_layer, s_icon_other);
    }
    
    s_bzz_count--;
    if (s_bzz_count == 0) {
        s_bzz_count = s_bzz_factor; 
    }
    
    s_main_leg = !s_main_leg;
    
    if (s_running) {
        app_timer_register(s_step_interval, step_timer_callback, 0);
    } else {
        bitmap_layer_set_bitmap(s_steps_layer, NULL);
    }
}

static void start_step_timer() {
    s_main_leg = true;
    s_bzz_count = s_bzz_factor;
    app_timer_register(s_step_interval, step_timer_callback, 0);
}

static void increment_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pace >= 200) {
	  return;
  }
  s_pace++;
  calc_bzz();
  update_text();
}

static void decrement_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pace <= 50) {
    // Keep the counter at zero
    return;
  }

  s_pace--;
  calc_bzz();
  update_text();
}

static void invert_click_handler(ClickRecognizerRef recognizer, void *context) {
    s_inverted = !s_inverted;
    layer_set_hidden((Layer *) s_inverter_layer, !s_inverted);      
}

static void start_stop_click_handler(ClickRecognizerRef recognizer, void *context) {
	s_running = !s_running;
	
	action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_running ? s_icon_stop : s_icon_run);
    if (s_running) {
        start_step_timer();
    }
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_INTERVAL_MS, increment_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_INTERVAL_MS, decrement_click_handler);
	
  window_single_click_subscribe(BUTTON_ID_SELECT, start_stop_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, LONG_CLICK_INTERVAL_MS, invert_click_handler, NULL);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
    
  s_action_bar = action_bar_layer_create();
  action_bar_layer_set_click_config_provider(s_action_bar, click_config_provider);

  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_UP, s_icon_plus);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_DOWN, s_icon_minus);
  action_bar_layer_set_icon(s_action_bar, BUTTON_ID_SELECT, s_icon_run);
    
  int width = layer_get_frame(window_layer).size.w - ACTION_BAR_WIDTH - 4;
    
  s_header_layer = text_layer_create(GRect(4, 0, width, 30));
  text_layer_set_font(s_header_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(s_header_layer, "Strides/minute");
  text_layer_set_text_alignment(s_header_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_header_layer));

  s_body_layer = text_layer_create(GRect(4, 44, width, 50));
  text_layer_set_font(s_body_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_text_alignment(s_body_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_body_layer));
    
  s_double_layer = text_layer_create(GRect(4, 95, width, 30));
  text_layer_set_font(s_double_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_double_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_double_layer));
    
  s_steps_layer = bitmap_layer_create(GRect(4, 130, width, 20));
  bitmap_layer_set_alignment(s_steps_layer, GAlignCenter);
  bitmap_layer_set_bitmap(s_steps_layer, NULL);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_steps_layer));
    

  s_inverter_layer = inverter_layer_create(GRect(0, 0, 144, 168));
  layer_add_child(window_layer, inverter_layer_get_layer(s_inverter_layer));  
  layer_set_hidden((Layer *) s_inverter_layer, !s_inverted);      

  action_bar_layer_add_to_window(s_action_bar, window);
    
  update_text();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_header_layer);
  text_layer_destroy(s_body_layer);
  text_layer_destroy(s_double_layer);
    
  bitmap_layer_destroy(s_steps_layer);

  action_bar_layer_destroy(s_action_bar);

  inverter_layer_destroy(s_inverter_layer);
}

static void init() {
  s_icon_plus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_PLUS);
  s_icon_minus = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_MINUS);
  s_icon_run = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_RUN);
  s_icon_stop = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ACTION_ICON_STOP);
    
  s_icon_main = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MAIN);
  s_icon_other = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_OTHER);

  // Get the count from persistent storage for use if it exists, otherwise use the default
  s_pace = persist_exists(PACE_PKEY) ? persist_read_int(PACE_PKEY) : PACE_DEFAULT;
  calc_bzz();
    
  s_inverted = persist_exists(INVERT_PKEY) ? persist_read_bool(INVERT_PKEY) : true;

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);
}

static void deinit() {
  // Save the count into persistent storage on app exit
  persist_write_int(PACE_PKEY, s_pace);
  persist_write_bool(INVERT_PKEY, s_inverted);

  window_destroy(s_main_window);

  gbitmap_destroy(s_icon_plus);
  gbitmap_destroy(s_icon_minus);
  gbitmap_destroy(s_icon_run);
  gbitmap_destroy(s_icon_stop);
  
  gbitmap_destroy(s_icon_main);
  gbitmap_destroy(s_icon_other);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
