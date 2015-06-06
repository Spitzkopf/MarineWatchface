#include <pebble.h>
#include <math.h>

#define MAX_W 144
#define MAX_H 168

#define SPRITES_W 261
#define SPRITES_H 540

#define SINGLE_SPRITE_W 77
#define SINGLE_SPRITE_H 100

#define MAX_SPRITE_X ((int)floor(SPRITES_W / (double)SINGLE_SPRITE_W) - 1)
#define MAX_SPRITE_Y ((int)floor(SPRITES_H / (double)SINGLE_SPRITE_H) - 1)

#define SPRITE_TIMEOUT 2000

Window* main_window;

Layer* marine_layer;
Layer* time_layer;
Layer* date_layer;

GBitmap* marine_spritesheet_black;
GBitmap* marine_spritesheet_white;

GBitmap* marine_charging_black;
GBitmap* marine_charging_white;
GFont custom_font_24;

AppTimer* marine_animation_timer;

typedef struct {
  int current_x;
  int previous_x;
  BatteryChargeState current_battery_state;
} MarineSpriteAnim;

MarineSpriteAnim anim_data;

typedef struct {
  int corner_radius;
  GCornerMask corner_mask;
  GColor bg_color;
  GColor text_color;
  GTextAlignment text_alignment;
  GTextOverflowMode overflow;
  GFont font;
  const char* text;
} RoundedTextLayerData;

GBitmap* get_sprite_by_index(GBitmap* spritesheet, int x, int y) {
  if (x > MAX_SPRITE_X || y > MAX_SPRITE_Y) {
    return NULL;
  }
  
  GRect sub_rect = GRect((SINGLE_SPRITE_W * x) + (9 * x), (SINGLE_SPRITE_H * y) + (7 * y) , SINGLE_SPRITE_W, SINGLE_SPRITE_H);
  GBitmap* sub_bitmap = gbitmap_create_as_sub_bitmap(spritesheet, sub_rect);
  
  return sub_bitmap;
}

void marine_animate(void* data) {
  layer_mark_dirty(marine_layer);
  marine_animation_timer = app_timer_register(SPRITE_TIMEOUT, marine_animate, NULL);
}

void draw_marine_layer(Layer *layer, GContext *ctx) {
  int next_x = 0;
  int next_y = 5 - ceil(anim_data.current_battery_state.charge_percent / 20.0);
  
  next_y = next_y > MAX_SPRITE_Y ? MAX_SPRITE_Y : next_y; 
  
  bool destroy_new_sprite = false;
  GBitmap* new_sprite_black = NULL;
  GBitmap* new_sprite_white = NULL;
  
  if (!anim_data.current_battery_state.is_charging && !anim_data.current_battery_state.is_plugged) {
      switch (anim_data.current_x) {
        case 2:
          next_x = 1;
          break;
        case 1:
          switch (anim_data.previous_x) {
            case 2:
             next_x = 0;
             break;
            case 0:
             next_x = 2;
             break;
            case 1:
             next_x = 0;
             break;
            default:
              return;
          }
          break;
       case 0:
        next_x = 1;
        break;
       default:
        return;
     }
      
     anim_data.previous_x = anim_data.current_x;
     anim_data.current_x = next_x;
     
     new_sprite_black  = get_sprite_by_index(marine_spritesheet_black, next_x, next_y);
     new_sprite_white  = get_sprite_by_index(marine_spritesheet_black, next_x, next_y);
     
     if (!new_sprite_black || !new_sprite_white) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error getting new sprite! x:%d y:%d", next_x, next_y);
      return;
     } 
     
     destroy_new_sprite = true; 
  } else {
    new_sprite_black = marine_charging_black;
    new_sprite_white = marine_charging_black;
  }
   
  
#ifdef PBL_PLATFORM_BASALT
   GRect image_size_black = gbitmap_get_bounds(new_sprite_black);
   GRect image_size_white = gbitmap_get_bounds(new_sprite_white);
#else 
   GRect image_size_black = new_sprite_black->bounds;
   GRect image_size_white = new_sprite_white->bounds;
#endif  
  
  GRect bounds = layer_get_bounds(layer);
  
  grect_align(&image_size_black, &bounds, GAlignCenter, true);
  grect_align(&image_size_white, &bounds, GAlignCenter, true);

  graphics_context_set_compositing_mode(ctx, GCompOpOr);
  graphics_draw_bitmap_in_rect(ctx, new_sprite_white, image_size_white);
  
  graphics_context_set_compositing_mode(ctx, GCompOpClear);
  graphics_draw_bitmap_in_rect(ctx, new_sprite_black, image_size_black);
  
  if (destroy_new_sprite) {
    gbitmap_destroy(new_sprite_black);
    gbitmap_destroy(new_sprite_white);
  }
}

void draw_rounded_text_layer(Layer *layer, GContext *ctx) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  
  GRect bound = layer_get_bounds(layer);
  
  graphics_context_set_fill_color(ctx, data->bg_color);
  graphics_fill_rect(ctx, bound, data->corner_radius, data->corner_mask);
  
  graphics_context_set_text_color(ctx, data->text_color);
  graphics_draw_text(ctx, data->text, data->font, bound, data->overflow, data->text_alignment, NULL);
}

Layer* rounded_text_layer_create(GRect rect, int corner_radius, GCornerMask mask) {
  Layer* layer = layer_create_with_data(rect, sizeof(RoundedTextLayerData));
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  
  data->corner_radius = corner_radius;
  data->corner_mask = mask;
  data->bg_color = GColorBlack;
  data->text_color = GColorWhite;
  data->text_alignment = GTextAlignmentCenter;
  data->overflow = GTextOverflowModeTrailingEllipsis;
  data->font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  data->text = NULL;
  
  layer_set_update_proc(layer, draw_rounded_text_layer);
  return layer;
}

void rounded_text_layer_set_corner_radius(Layer* layer, int radius) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->corner_radius = radius;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_corner_mask(Layer* layer, GCornerMask mask) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->corner_mask = mask;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_background_color(Layer* layer, GColor color) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->bg_color = color;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_text_color(Layer* layer, GColor color) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->text_color = color;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_text(Layer* layer, const char* text) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->text = text;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_alignment(Layer* layer, GTextAlignment align) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->text_alignment = align;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_overflow(Layer* layer, GTextOverflowMode overflow) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->overflow = overflow;
  layer_mark_dirty(layer);
}

void rounded_text_layer_set_font(Layer* layer, GFont font) {
  RoundedTextLayerData* data = (RoundedTextLayerData*)layer_get_data(layer);
  data->font = font;
  layer_mark_dirty(layer);
}

void battery_state_changed_handler(BatteryChargeState state) {
  anim_data.current_battery_state = state;
  layer_mark_dirty(marine_layer);
}

void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  static char buffer[] = "00:00";
  
  if(clock_is_24h_style() == true) {
    strftime(buffer, sizeof(buffer), "%H:%M", tick_time);
  } else {
    strftime(buffer, sizeof(buffer), "%I:%M", tick_time);
  }
  
  rounded_text_layer_set_text(time_layer, buffer);
}

void update_date() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  static char buffer[] = "00/00/0000";
  
  strftime(buffer, sizeof(buffer), "%d/%m/%y", tick_time);
  
  rounded_text_layer_set_text(date_layer, buffer);
}

void time_changed_handler(struct tm *tick_time, TimeUnits units_changed) {
  if((units_changed & MINUTE_UNIT) != 0) {
    update_time();
  }

  if((units_changed & DAY_UNIT) != 0) {
    update_date();
  }
}

void main_window_load(Window *window) {
  marine_spritesheet_black = gbitmap_create_with_resource(RESOURCE_ID_DOOM_MARINE_SPRITES_BLACK);
  marine_spritesheet_white = gbitmap_create_with_resource(RESOURCE_ID_DOOM_MARINE_SPRITES_WHITE);
  
  marine_charging_black = gbitmap_create_with_resource(RESOURCE_ID_DOOM_MARINE_SPRITE_CHARGING_BLACK);
  marine_charging_white = gbitmap_create_with_resource(RESOURCE_ID_DOOM_MARINE_SPRITE_CHARGING_WHITE);
  
  marine_layer = layer_create(GRect(0, 0, 144, 168));
  layer_set_update_proc(marine_layer, draw_marine_layer);
  layer_add_child(window_get_root_layer(window), marine_layer);
  
  time_layer = rounded_text_layer_create(GRect(0, 140, 144, 28), 8, GCornersTop);
  rounded_text_layer_set_font(time_layer, custom_font_24);
  layer_add_child(window_get_root_layer(window), time_layer);
  
  date_layer = rounded_text_layer_create(GRect(0, 0, 144, 28), 8, GCornersBottom);
  rounded_text_layer_set_font(date_layer, custom_font_24);
  layer_add_child(window_get_root_layer(window), date_layer);
  
  anim_data.current_x = 1;
  anim_data.previous_x = 1;
  anim_data.current_battery_state = battery_state_service_peek();
  
  update_time();
  update_date();
  layer_mark_dirty(marine_layer);
  
  marine_animation_timer = app_timer_register(SPRITE_TIMEOUT, marine_animate, NULL);
}

void main_window_unload(Window *window) {
  gbitmap_destroy(marine_spritesheet_black);
  gbitmap_destroy(marine_spritesheet_white);
  gbitmap_destroy(marine_charging_black);
  gbitmap_destroy(marine_charging_white);
  
  layer_destroy(marine_layer);
  layer_destroy(time_layer);
  layer_destroy(date_layer);
}

void init() {
  main_window = window_create();
  
  custom_font_24 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CUSTOM_24));
  
  battery_state_service_subscribe(battery_state_changed_handler);
  tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, time_changed_handler);
  
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  window_stack_push(main_window, true);
}

void deinit() {
  window_destroy(main_window);
  
  fonts_unload_custom_font(custom_font_24);
  battery_state_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}