#include <pebble.h>
#include "timeout.h"
#include "storage.h"
#include "localize.h"

#define MENU_ROW_COUNT 5

#define MENU_ROW_LONGPRESS     0
#define MENU_ROW_VIBRATION     2
#define MENU_ROW_FLIP          1
#define MENU_ROW_DURATION      3
#define MENU_ROW_AUTO_SNOOZE   4

static void window_load(Window* window);
static void window_unload(Window* window);
static void window_appear(Window* window);
static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context);
static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context);
static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context);
static int16_t menu_header_height(struct MenuLayer *menu, uint16_t section_index, void *callback_context);
static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context);
static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context);
static void menu_selection_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context);
static void scroll_timer_callback(void* data);

static Window*    s_window;
static MenuLayer* s_menu;
static bool s_longpress_dismiss;
static int s_vibration_pattern;
static bool s_flip_to_snooze;
static int s_vibration_duration;
static bool s_auto_snooze;

static int16_t s_scroll_index;
static int16_t s_scroll_row_index;
static AppTimer *s_scroll_timer;

void win_advanced_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
    .appear = window_appear
  });
  s_longpress_dismiss = load_persistent_storage_bool(LONGPRESS_DISMISS_KEY,false);
  s_vibration_pattern = load_persistent_storage_int(VIBRATION_PATTERN_KEY,0);
  s_flip_to_snooze = load_persistent_storage_bool(FLIP_TO_SNOOZE_KEY, false);
  s_vibration_duration = load_persistent_storage_int(VIBRATION_DURATION_KEY,2);
  s_auto_snooze = load_persistent_storage_bool(AUTO_SNOOZE_KEY,true);
  refresh_timeout();
  s_scroll_timer = app_timer_register(500,scroll_timer_callback,NULL);
}

void win_advanced_show(void) {
  window_stack_push(s_window, true);
}

static void window_load(Window* window) {
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_frame(window_layer);
  
  // Create the menu layer
  s_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_sections = menu_num_sections,
    .get_num_rows = menu_num_rows,
    .get_cell_height = menu_cell_height,
    .draw_row = menu_draw_row,
    .select_click = menu_select,
    .get_header_height = menu_header_height,
    .selection_changed = menu_selection_changed,
  });
  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(s_menu, s_window);
  
  // Add it to the window for display
  layer_add_child(window_layer, menu_layer_get_layer(s_menu));
  
}

static void window_unload(Window* window) {
  menu_layer_destroy(s_menu);
}

static void window_appear(Window* window) {
  layer_mark_dirty(menu_layer_get_layer(s_menu));
}

static uint16_t menu_num_sections(struct MenuLayer* menu, void* callback_context) {
  return 1;
}

static uint16_t menu_num_rows(struct MenuLayer* menu, uint16_t section_index, void* callback_context) {
  return MENU_ROW_COUNT;
}

static int16_t menu_cell_height(struct MenuLayer *menu, MenuIndex *cell_index, void *callback_context) {
  return 38;
}

static int16_t menu_header_height(struct MenuLayer *menu, uint16_t section_index, void *callback_context) {
  return 0;
}


static void menu_cell_animated_draw(GContext* ctx, const Layer* cell_layer, char* text, char* subtext, bool animate)
{
  if(animate && s_scroll_index>0)
  {
    if(((int16_t)strlen(text)-15-s_scroll_index)>0)
    text+=s_scroll_index;
  }
  menu_cell_basic_draw(ctx,cell_layer,text,subtext,NULL);
}

static void menu_draw_row(GContext* ctx, const Layer* cell_layer, MenuIndex* cell_index, void* callback_context) {
  bool animate = cell_index->row==s_scroll_row_index;
  switch (cell_index->row) {
    case MENU_ROW_LONGPRESS:
      menu_cell_animated_draw(ctx, cell_layer, _("Dismiss Alarm"), s_longpress_dismiss?_("Long press"):_("Short press"), animate);
      break;
    case MENU_ROW_FLIP:
      menu_cell_animated_draw(ctx, cell_layer, _("Shake to Snooze"), s_flip_to_snooze?_("Enabled"):_("Disabled"), animate);
      break;
    case MENU_ROW_VIBRATION:
    switch (s_vibration_pattern) {
      case 0:
        menu_cell_animated_draw(ctx, cell_layer, _("Vibration Strength"), _("Constant"), animate);
        break;
      case 1:
        menu_cell_animated_draw(ctx, cell_layer, _("Vibration Strength"), _("Increasing 10s"), animate);
        break;
      case 2:
        menu_cell_animated_draw(ctx, cell_layer, _("Vibration Strength"), _("Increasing 20s"), animate);
        break;
      case 3:
        menu_cell_animated_draw(ctx, cell_layer, _("Vibration Strength"), _("Increasing 30s"), animate);
        break;
      default:
        break;
    }
    break;
    case MENU_ROW_AUTO_SNOOZE:
    menu_cell_animated_draw(ctx, cell_layer, _("Snooze after Vibration End"), s_auto_snooze?_("ON"):_("OFF"), animate);
      break;
    case MENU_ROW_DURATION:
    switch (s_vibration_duration) {
      case 0:
      menu_cell_animated_draw(ctx, cell_layer, _("Vibration Duration"), _("30 seconds"), animate);
      break;
      case 1:
      menu_cell_animated_draw(ctx, cell_layer, _("Vibration Duration"), _("1 minute"), animate);
      break;
      case 2:
      menu_cell_animated_draw(ctx, cell_layer, _("Vibration Duration"), _("2 minutes"), animate);
      break;
      case 3:
      menu_cell_animated_draw(ctx, cell_layer, _("Vibration Duration"), _("5 minutes"), animate);
      break;
      default:
      break;
    }
    break;
    
  }

}

static void scroll_timer_callback(void *data)
{
  s_scroll_index+=3;
  if(s_scroll_row_index>0)
  layer_mark_dirty(menu_layer_get_layer(s_menu));
  s_scroll_timer = app_timer_register(1000,scroll_timer_callback,NULL);
}

static void menu_selection_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
  s_scroll_index=0;
  s_scroll_row_index = new_index.row;
  app_timer_reschedule(s_scroll_timer,1000);
  refresh_timeout();
}

static void menu_select(struct MenuLayer* menu, MenuIndex* cell_index, void* callback_context) {
  switch (cell_index->row) {
    case MENU_ROW_LONGPRESS:
      s_longpress_dismiss=!s_longpress_dismiss;
      persist_write_bool(LONGPRESS_DISMISS_KEY,s_longpress_dismiss);
      break;
    case MENU_ROW_FLIP:
      s_flip_to_snooze=!s_flip_to_snooze;
      persist_write_bool(FLIP_TO_SNOOZE_KEY,s_flip_to_snooze);
      break;
    case MENU_ROW_AUTO_SNOOZE:
      s_auto_snooze=!s_auto_snooze;
      persist_write_bool(AUTO_SNOOZE_KEY,s_auto_snooze);
      break;
    case MENU_ROW_VIBRATION:
      s_vibration_pattern++;
      if(s_vibration_pattern>3)
        s_vibration_pattern=0;
      persist_write_int(VIBRATION_PATTERN_KEY,s_vibration_pattern);
      break;
    case MENU_ROW_DURATION:
      s_vibration_duration++;
      if(s_vibration_duration>3)
        s_vibration_duration=0;
      persist_write_int(VIBRATION_DURATION_KEY,s_vibration_duration);
    break;
  }
  refresh_timeout();
  layer_mark_dirty(menu_layer_get_layer(menu));
}