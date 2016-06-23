//======================================
// TECHRAD watch face
// Copyright Mango Lazi 2015
// Released under GPLv3
// Lots of stuff from Pebble SDK, config stuff thanks to Tom Gidden
//======================================

#include "techrad.h" // hour ticks and hand designs in here
#include "pebble.h"

static Window *window;
static GFont custom_font_numerals;
static GColor color_background, color_ticks, color_maintext, color_cornertext, color_maintextbackground, color_cornertextbackground, color_second, color_hand_fill, color_hand_stroke, color_center_fill, color_center_stroke;
static Layer *s_simple_bg_layer, *s_date_layer, *s_hands_layer;
static TextLayer *s_label_12, *s_label_4, *s_label_8; // number labels
static TextLayer *s_day_label, *s_battery_label, *s_suntimes_label, *s_temperature_label, *s_minmaxtemp_label, *s_city_label, *s_misc_label, *s_fitness_label; // information labels
static char s_day_buffer[18], s_battery_buffer[4]; // buffers for information labels
#if defined(PBL_HEALTH)
static char s_fitness_buffer[10];
#endif

static bool weather_fetched = false; // new weather data fetched for 60 minute period
static bool health_fetched = false; // new fitness data fetched
static bool bluetooth_enabled = false; // check for bluetooth status

// bitmaps and layers for weather, forecast and bluetooth icons
static BitmapLayer *s_icon_layer;
static GBitmap *s_icon_bitmap = NULL;
static BitmapLayer *s_forecasticon_layer;
static GBitmap *s_forecasticon_bitmap = NULL;
static BitmapLayer *s_bluetooth_layer;
static GBitmap *s_bluetooth_bitmap = NULL;

// background hour ticks and arrows
static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;

// appsync stuff
static AppSync s_sync;
static uint8_t s_sync_buffer[256];

// preferences, stored in watch persistent storage
typedef struct persist {
    uint8_t seconds; // show seconds, default false
    uint8_t hourvibes; // vibrate at the start of every hour, default false
    uint8_t reverse; // reverse display: white background with black text, default false
    uint8_t distance; // steps unit: km or miles, default no. of steps
    uint8_t bluetheme; // color theme: blue graphics, default red
} __attribute__((__packed__)) persist;

persist settings = {
    .seconds = 0,   // show seconds, default false
    .hourvibes = 0, // vibrate at the start of every hour, default false
    .reverse = 0,   // reverse display: white background with black text, default false
    .distance = 0,  // show distance, default no. of steps walked
    .bluetheme = 0  // blue theme, default red
};

enum PersistKey {
    PERSIST_SETTINGS = 0,
    PERSIST_WEATHERDATA = 1
};

// struct for cached weather data
typedef struct weatherdata {
    uint8_t icon_current;   // current weather icon
    uint8_t forecasticon;   // current weather icon
    char temperature[5];       // temperature
    char minmaxtemp[15];       // temperature
    char city[20];         // sunrise
    char suntimes[20];         // sunset
    char misc[15];         // city
} __attribute__((__packed__)) weatherdata;

weatherdata cachedWeather = {
    .icon_current = 4, // loading icon
    .forecasticon = 4, // loading icon
    .temperature = "",
    .minmaxtemp = "",
    .city = "",
    .suntimes = "",
    .misc = ""
};

// appkeys, should match stuff in appinfo.json
enum WeatherKey {
    WEATHER_ICON = 0x0,         	// TUPLE_INT
    WEATHER_TEMPERATURE = 0x1,  	// TUPLE_CSTRING
    WEATHER_CITY = 0x2,         	// TUPLE_CSTRING
    WEATHER_SUNTIMES = 0x3, 		// TUPLE_CSTRING
    WEATHER_FORECASTICON = 0x4, 	// TUPLE_INT
    WEATHER_MINMAXTEMP = 0x5,		// TUPLE_CSTRING
    WEATHER_MISC = 0x6,				// TUPLE_CSTRING
    CONFIG_SECONDS = 0x7,			// TUPLE_INT
    CONFIG_HOURVIBES = 0x8,			// TUPLE_INT
    CONFIG_BLUETHEME = 0x9,          // TUPLE_INT
    CONFIG_REVERSE = 0xA,			// TUPLE_INT
    CONFIG_DISTANCE = 0xB          // TUPLE_INT
};

// array for weather and forecast icons
static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_IMAGE_SUN,    //0
  RESOURCE_ID_IMAGE_CLOUD,  //1
  RESOURCE_ID_IMAGE_RAIN,   //2
  RESOURCE_ID_IMAGE_SNOW,   //3
  RESOURCE_ID_IMAGE_LOADING //4
};

static const uint32_t WEATHER_ICONS_SMALL[] = {
  RESOURCE_ID_IMAGE_SUN_SMALL,      //0
  RESOURCE_ID_IMAGE_CLOUD_SMALL,    //1
  RESOURCE_ID_IMAGE_RAIN_SMALL,     //2
  RESOURCE_ID_IMAGE_SNOW_SMALL,     //3
  RESOURCE_ID_IMAGE_LOADING_SMALL   //4
};

static const uint32_t WEATHER_ICONS_REVERSE[] = {
    RESOURCE_ID_IMAGE_SUN_REVERSE,      //0
    RESOURCE_ID_IMAGE_CLOUD_REVERSE,    //1
    RESOURCE_ID_IMAGE_RAIN_REVERSE,     //2
    RESOURCE_ID_IMAGE_SNOW_REVERSE,     //3
    RESOURCE_ID_IMAGE_LOADING_REVERSE   //4
};

static const uint32_t WEATHER_ICONS_SMALL_REVERSE[] = {
    RESOURCE_ID_IMAGE_SUN_SMALL_REVERSE,    //0
    RESOURCE_ID_IMAGE_CLOUD_SMALL_REVERSE,  //1
    RESOURCE_ID_IMAGE_RAIN_SMALL_REVERSE,   //2
    RESOURCE_ID_IMAGE_SNOW_SMALL_REVERSE,   //3
    RESOURCE_ID_IMAGE_LOADING_SMALL_REVERSE //4
};

//======================================
// REQUEST WEATHER USING PHONE
//======================================
// send out an empty dictionary for syncing
// someday I'll figure out how to do bidirectional syncing
static void request_weather(void) {
	DictionaryIterator *iter;
    weather_fetched = true;
    
	app_message_outbox_begin(&iter);

	if (!iter) {
		// Error creating outbound message
	    return;
		}

    int value = 0;
    dict_write_int(iter, 0, &value, sizeof(int), true);
    dict_write_end(iter);
    app_message_outbox_send();
    
    // show loading icon
    if (s_icon_bitmap) {
        gbitmap_destroy(s_icon_bitmap);
    }
    if (settings.reverse == 1) {
        s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[3]);
    }
    else {
        s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[3]);
    }
    bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
    text_layer_set_text(s_temperature_label, "");}


//======================================
// HEALTH UPDATER
//======================================
// get total steps for the day
#if defined(PBL_HEALTH)
static void request_health() {
    time_t start = time_start_of_today();
    time_t end = time(NULL);
    HealthMetric metric;
    
    if (settings.distance == 1) { // show distance walked
        metric = HealthMetricWalkedDistanceMeters;
    }
    else { // show no. of steps walked
        metric = HealthMetricStepCount;
    }
    
    // Check the metric has data available for today
    HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);
    
    if (mask & HealthServiceAccessibilityMaskAvailable) {
        if (settings.distance == 1) { // show distance walked
            snprintf(s_fitness_buffer, sizeof(s_fitness_buffer), "%d m", (int)health_service_sum_today(metric));
        }
        else { // show no. of steps walked
            snprintf(s_fitness_buffer, sizeof(s_fitness_buffer), "%dx", (int)health_service_sum_today(metric));
        }
        text_layer_set_text(s_fitness_label, s_fitness_buffer);
    }
}
#endif


//======================================
// COLOR HANDLER
//======================================
// white on black default, black on white reverse
// blue or red theme for ticks and center box
static void color_handler() {
    if (settings.reverse == 1) { // reverse layout
        color_background = GColorWhite;
        color_center_fill = COLOR_FALLBACK(GColorWhite, GColorWhite);
//        color_center_stroke = COLOR_FALLBACK(GColorBlue, GColorBlack);
//        color_ticks = COLOR_FALLBACK(GColorBlue, GColorBlack);
        color_second = COLOR_FALLBACK(GColorDarkGray, GColorBlack);
//        color_hand_fill = COLOR_FALLBACK(GColorBlack, GColorBlack);;
        color_hand_stroke = COLOR_FALLBACK(GColorWhite, GColorWhite);
        color_maintext = COLOR_FALLBACK(GColorBlack, GColorBlack); // for numerals, date, weather
        color_cornertext = COLOR_FALLBACK(GColorBlack, GColorBlack); // for battery, BT, misc, misc2
        color_maintextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for main text
        color_cornertextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for corner text
        
        if (settings.bluetheme == 1) { // blue theme on reverse
            color_hand_fill = COLOR_FALLBACK(GColorBlue, GColorBlack);;
            color_center_stroke = COLOR_FALLBACK(GColorBlueMoon, GColorBlack);
            color_ticks = COLOR_FALLBACK(GColorBlueMoon, GColorBlack);
        }
        else { // red theme on reverse
            color_hand_fill = COLOR_FALLBACK(GColorRed, GColorBlack);;
            color_center_stroke = COLOR_FALLBACK(GColorRed, GColorBlack);
            color_ticks = COLOR_FALLBACK(GColorRed, GColorBlack);
        }
    }
    else { // default black background
        color_background = GColorBlack;
        color_center_fill = COLOR_FALLBACK(GColorBlack, GColorBlack);
//        color_center_stroke = COLOR_FALLBACK(GColorRed, GColorWhite);
//        color_ticks = COLOR_FALLBACK(GColorRed, GColorWhite);
        color_second = COLOR_FALLBACK(GColorChromeYellow, GColorWhite);
//        color_hand_fill = COLOR_FALLBACK(GColorWhite, GColorWhite);;
        color_hand_stroke = COLOR_FALLBACK(GColorBlack, GColorBlack);
        color_maintext = COLOR_FALLBACK(GColorChromeYellow, GColorWhite); // for numerals, date, weather
        color_cornertext = COLOR_FALLBACK(GColorWhite, GColorWhite); // for battery, BT, misc, misc2
        color_maintextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for main text
        color_cornertextbackground = COLOR_FALLBACK(GColorClear, GColorClear); // background for corner text
        
        if (settings.bluetheme == 1) { // blue theme on black
            color_hand_fill = COLOR_FALLBACK(GColorWhite, GColorWhite);;
            color_center_stroke = COLOR_FALLBACK(GColorVividCerulean, GColorWhite);
            color_ticks = COLOR_FALLBACK(GColorVividCerulean, GColorWhite);
        }
        else { // red theme on black
            color_hand_fill = COLOR_FALLBACK(GColorWhite, GColorWhite);;
            color_center_stroke = COLOR_FALLBACK(GColorRed, GColorWhite);
            color_ticks = COLOR_FALLBACK(GColorRed, GColorWhite);
        }

    }

}


//======================================
// BACKGROUND UPDATER
//======================================
static void bg_update_proc(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, color_background);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, color_ticks);
    graphics_context_set_stroke_color(ctx, color_ticks);
    for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
        gpath_draw_filled(ctx, s_tick_paths[i]);
    }
}


//======================================
// HANDS UPDATER
//======================================
// draw second hand if config_seconds set to 1
// otherwise minute and hour hands only
static void hands_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	
	// minute hand
	graphics_context_set_fill_color(ctx, color_hand_fill);
	graphics_context_set_stroke_color(ctx, color_hand_stroke);
	gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
	gpath_draw_filled(ctx, s_minute_arrow);
	gpath_draw_outline(ctx, s_minute_arrow);

	// hour hand
	graphics_context_set_fill_color(ctx, color_hand_fill);
	graphics_context_set_stroke_color(ctx, color_hand_stroke);
	gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6)); // from Pebble SDK example
	gpath_draw_filled(ctx, s_hour_arrow);
	gpath_draw_outline(ctx, s_hour_arrow);
    
    // draw second hand if config is set
    if (settings.seconds == 1) {
        GPoint center = grect_center_point(&bounds);
        int16_t second_hand_length = bounds.size.h / 2;
        int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
        GPoint second_hand = {
            .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
            .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
        };
        #ifdef PBL_PLATFORM_BASALT
                graphics_context_set_stroke_width(ctx, 2);
        #endif
        graphics_context_set_stroke_color(ctx, color_second);
        graphics_draw_line(ctx, second_hand, center);
    }

	// rectangle in the middle for weather data
    #ifdef PBL_PLATFORM_BASALT
        graphics_context_set_stroke_width(ctx, 2);
    #endif
	graphics_context_set_fill_color(ctx, color_center_fill);
	graphics_context_set_stroke_color(ctx, color_center_stroke);
	graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 19, bounds.size.h / 2 - 24, 38, 49), 9, GCornersAll);
	graphics_draw_round_rect(ctx, GRect(bounds.size.w / 2 - 19, bounds.size.h / 2 - 24, 38, 49), 9);

    // update weather with phone every 01st and 31st minutes, but phone fetches fresh online data only every hour
    // reset update status at 59 minutes
    //	if((t->tm_min == 1) && (t->tm_sec == 0)) {
    if((t->tm_min == 1) && (weather_fetched == false)) {
        if (bluetooth_enabled == true) {
            request_weather();
            weather_fetched = true;
        }
    }

    // update fitness data with phone every 5 minutes
    #if defined(PBL_HEALTH)
    if(t->tm_min % 4 == 0) {
        health_fetched = false;
    }
    if((t->tm_min % 5 == 0) && (health_fetched == false)) {
        request_health();
        health_fetched = true;
    }
    #endif
    
    if(t->tm_min == 59) {
        weather_fetched = false;
        health_fetched = false;
    }
    

	
	// vibrate at start of every hour
	if(settings.hourvibes == 1) {
		if((t->tm_min == 0) && (t->tm_sec == 1)) {
            vibes_long_pulse();
		}
	}
}

//======================================
// TIME TICK HANDLER
//======================================
// ticks per second or minute, see init below, also allows changes through appsync
static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(window));
}


//======================================
// DATE UPDATER
//======================================
// should do localization here later
static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(s_day_buffer, sizeof(s_day_buffer), "%a %d", t);
  text_layer_set_text(s_day_label, s_day_buffer);
}


//======================================
// APPSYNC STUFF
//======================================

//======================================
// SYNC ERROR CALLBACK
//======================================
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

//======================================
// TUPLE CHANGED CALLBACK
//======================================
// Called every time watch or phone sends appsync dictionary
// Save settings to watch storage
static void sync_tuple_changed_callback(const uint32_t key, const Tuple* t, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_ICON:
        cachedWeather.icon_current = t->value->uint8;
        if (s_icon_bitmap) {
            gbitmap_destroy(s_icon_bitmap);
        }
          if (settings.reverse == 1) {
              s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[cachedWeather.icon_current]);
          }
          else {
              s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[cachedWeather.icon_current]);
          }
        bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
    break;

    case WEATHER_TEMPERATURE:
        snprintf(cachedWeather.temperature, sizeof(cachedWeather.temperature), "%s",  t->value->cstring);
      	text_layer_set_text(s_temperature_label, cachedWeather.temperature);
    break;

    case WEATHER_CITY:
        snprintf(cachedWeather.city, sizeof(cachedWeather.city), "%s",  t->value->cstring);
      	text_layer_set_text(s_city_label, cachedWeather.city);
    break;
	  
	case WEATHER_SUNTIMES:
        snprintf(cachedWeather.suntimes, sizeof(cachedWeather.suntimes), "%s",  t->value->cstring);
      	text_layer_set_text(s_suntimes_label, cachedWeather.suntimes);
    break;
	  
 	case WEATHER_FORECASTICON:
        cachedWeather.forecasticon = t->value->uint8;
		if (s_forecasticon_bitmap) {
        	gbitmap_destroy(s_forecasticon_bitmap);
      	}
          if (settings.reverse == 1) {
              s_forecasticon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_SMALL_REVERSE[cachedWeather.forecasticon]);
          }
          else {
              s_forecasticon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_SMALL[cachedWeather.forecasticon]);
          }
      	bitmap_layer_set_bitmap(s_forecasticon_layer, s_forecasticon_bitmap);
	break;

  	case WEATHER_MINMAXTEMP:
		snprintf(cachedWeather.minmaxtemp, sizeof(cachedWeather.minmaxtemp), "%s",  t->value->cstring);
      	text_layer_set_text(s_minmaxtemp_label, cachedWeather.minmaxtemp);
	break;

  	case WEATHER_MISC:
		snprintf(cachedWeather.misc, sizeof(cachedWeather.misc), "%s",  t->value->cstring);
      	text_layer_set_text(s_misc_label, cachedWeather.misc);
        // write data to weather cache on watch
        persist_write_data(PERSIST_WEATHERDATA, &cachedWeather, sizeof(cachedWeather));
	break;
					
	case CONFIG_SECONDS:
        settings.seconds = t->value->uint8;
        if (t->value->uint8 == 1) {
          tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
        }
        else {
          tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
        }
	break;

	case CONFIG_HOURVIBES:
        settings.hourvibes = t->value->uint8;
	break;
          
    case CONFIG_DISTANCE:
        settings.distance = t->value->uint8;
    #if defined(PBL_HEALTH)
        request_health();
    #endif
    break;
          
    case CONFIG_BLUETHEME:
        settings.bluetheme = t->value->uint8;
//        color_handler();
//          APP_LOG(APP_LOG_LEVEL_DEBUG, "bluetheme: %d", settings.bluetheme);
    break;

    case CONFIG_REVERSE:
          settings.reverse = t->value->uint8;
          color_handler();
          
          // numerals for 12, 4, 8 o'clock
          text_layer_set_text_color(s_label_12, color_maintext);
          text_layer_set_background_color(s_label_12, color_maintextbackground);
          text_layer_set_text_color(s_label_4, color_maintext);
          text_layer_set_background_color(s_label_4, color_maintextbackground);
          text_layer_set_text_color(s_label_8, color_maintext);
          text_layer_set_background_color(s_label_8, color_maintextbackground);
          
          // date label
          text_layer_set_text_color(s_day_label, color_maintext);
          text_layer_set_background_color(s_day_label, color_maintextbackground);
          
          // battery label
          text_layer_set_text_color(s_battery_label, color_cornertext);
          text_layer_set_background_color(s_battery_label, color_cornertextbackground);
          
          // main temperature label
          text_layer_set_text_color(s_temperature_label, color_maintext);
          text_layer_set_background_color(s_temperature_label, color_maintextbackground);
          
          // city label
          text_layer_set_text_color(s_city_label, color_cornertext);
          text_layer_set_background_color(s_city_label, color_cornertextbackground);
          
          // sunrise sunset label
          text_layer_set_text_color(s_suntimes_label, color_cornertext);
          text_layer_set_background_color(s_suntimes_label, color_cornertextbackground);

          // minmax temperature label
          text_layer_set_text_color(s_minmaxtemp_label, color_cornertext);
          text_layer_set_background_color(s_minmaxtemp_label, color_cornertextbackground);

          // misc label
          text_layer_set_text_color(s_misc_label, color_cornertext);
          text_layer_set_background_color(s_misc_label, color_cornertextbackground);
          
          // fitness label
          text_layer_set_text_color(s_fitness_label, color_cornertext);
          text_layer_set_background_color(s_fitness_label, color_cornertextbackground);
          
          // set main weather icon
          if (s_icon_bitmap) {
              gbitmap_destroy(s_icon_bitmap);
          }
          if (settings.reverse == 1) {
              s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[cachedWeather.icon_current]);
          }
          else {
              s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[cachedWeather.icon_current]);
          }
          bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);

          // set forecast icon
          if (s_forecasticon_bitmap) {
              gbitmap_destroy(s_forecasticon_bitmap);
          }
          if (settings.reverse == 1) {
              s_forecasticon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_SMALL_REVERSE[cachedWeather.forecasticon]);
          }
          else {
              s_forecasticon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_SMALL[cachedWeather.forecasticon]);
          }
          bitmap_layer_set_bitmap(s_forecasticon_layer, s_forecasticon_bitmap);
          
          break;


  }
}


//======================================
// BATTERY LEVEL HANDLER
//======================================
static void handle_battery(BatteryChargeState charge_state) {
  if (charge_state.is_charging) {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "+%d",  charge_state.charge_percent);
  } else {
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d", charge_state.charge_percent);
  }
  text_layer_set_text(s_battery_label, s_battery_buffer);
}


//======================================
// BLUETOOTH CONNECTION HANDLER
//======================================
static void handle_bluetooth(bool connected_state) {
    if (connected_state == true) {
        bluetooth_enabled = true;
        if (s_bluetooth_bitmap) {
            gbitmap_destroy(s_bluetooth_bitmap);
        }
    }
    else {
        vibes_double_pulse(); // double pulse vibration if connection lost
        bluetooth_enabled = false;
        if (s_bluetooth_bitmap) {
            gbitmap_destroy(s_bluetooth_bitmap);
        }
        s_bluetooth_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH);
        bitmap_layer_set_bitmap(s_bluetooth_layer, s_bluetooth_bitmap);
    }
}


//======================================
// MAIN WINDOW LOADER
//======================================
static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

    // create custom GFont
    custom_font_numerals = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROUNDY_34_BOLD));
    
	// black background
	s_simple_bg_layer = layer_create(bounds);
	layer_set_update_proc(s_simple_bg_layer, bg_update_proc);
	layer_add_child(window_layer, s_simple_bg_layer);

	// add numerals for 12, 4, 8 o'clock
	s_label_12 = text_layer_create(GRect(bounds.size.w / 2 - 25, -9, 50, 45));
	text_layer_set_text(s_label_12, "12");
	text_layer_set_text_color(s_label_12, color_maintext);
    text_layer_set_background_color(s_label_12, color_maintextbackground);
//    text_layer_set_font(s_label_12, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    text_layer_set_font(s_label_12, custom_font_numerals);
	text_layer_set_text_alignment(s_label_12, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(s_label_12));

	s_label_4 = text_layer_create(GRect(112, 100, 30, 40));
	text_layer_set_text(s_label_4, "4");
	text_layer_set_text_color(s_label_4, color_maintext);
    text_layer_set_background_color(s_label_4, color_maintextbackground);
//    text_layer_set_font(s_label_4, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    text_layer_set_font(s_label_4, custom_font_numerals);
	text_layer_set_text_alignment(s_label_4, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(s_label_4));

	s_label_8 = text_layer_create(GRect(2, 100, 30, 40));
	text_layer_set_text(s_label_8, "8");
	text_layer_set_text_color(s_label_8, color_maintext);
    text_layer_set_background_color(s_label_8, color_maintextbackground);
//    text_layer_set_font(s_label_8, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
    text_layer_set_font(s_label_8, custom_font_numerals);
	text_layer_set_text_alignment(s_label_8, GTextAlignmentLeft);
	layer_add_child(window_layer, text_layer_get_layer(s_label_8));

	// add battery label
	s_battery_label = text_layer_create(GRect(3, 0, 30, 20));
	text_layer_set_text_color(s_battery_label, color_cornertext);
    text_layer_set_background_color(s_battery_label, color_cornertextbackground);
	text_layer_set_font(s_battery_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_battery_label, GTextAlignmentLeft);
	layer_add_child(window_layer, text_layer_get_layer(s_battery_label));

    // fitness label
    s_fitness_label = text_layer_create(GRect(bounds.size.w / 2 - 20, 150, 40, 20));
    text_layer_set_font(s_fitness_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(s_fitness_label, GTextAlignmentCenter);
    text_layer_set_text_color(s_fitness_label, color_cornertext);
    text_layer_set_background_color(s_fitness_label, color_cornertextbackground);
    layer_add_child(window_layer, text_layer_get_layer(s_fitness_label));
    
    // add date label
    s_date_layer = layer_create(bounds);
    layer_set_update_proc(s_date_layer, date_update_proc);
    layer_add_child(window_layer, s_date_layer);
    s_day_label = text_layer_create(GRect(bounds.size.w / 2 - 30, 133, 60, 25));
    text_layer_set_text(s_day_label, s_day_buffer);
    text_layer_set_text_color(s_day_label, color_maintext);
    text_layer_set_background_color(s_day_label, color_maintextbackground);
    text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_day_label, GTextAlignmentCenter);
    layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));
    
    // bluetooth icon
    s_bluetooth_layer = bitmap_layer_create(GRect(128, 0, 10, 15));
    bitmap_layer_set_compositing_mode(s_bluetooth_layer, GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_bluetooth_layer));

	// add sunrise sunset labels
	s_suntimes_label = text_layer_create(GRect(0, 30, 40, 30));
	text_layer_set_text_color(s_suntimes_label, color_cornertext);
    text_layer_set_background_color(s_suntimes_label, color_cornertextbackground);
    text_layer_set_font(s_suntimes_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_suntimes_label, GTextAlignmentLeft);
	layer_add_child(window_layer, text_layer_get_layer(s_suntimes_label));

    // add forecast icon
    s_forecasticon_layer = bitmap_layer_create(GRect(bounds.size.w / 2 - 7, bounds.size.h / 2 - 45, 15, 15));
    layer_add_child(window_layer, bitmap_layer_get_layer(s_forecasticon_layer));
    if (s_forecasticon_bitmap) {
        gbitmap_destroy(s_forecasticon_bitmap);
    }
    if (settings.reverse == 1) {
        s_forecasticon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_SMALL_REVERSE[cachedWeather.forecasticon]);
    }
    else {
        s_forecasticon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_SMALL[cachedWeather.forecasticon]);
    }
    bitmap_layer_set_bitmap(s_forecasticon_layer, s_forecasticon_bitmap);

    // add minmax temp label
	s_minmaxtemp_label = text_layer_create(GRect(102, 30, 40, 15));
	text_layer_set_text_color(s_minmaxtemp_label, color_cornertext);
	text_layer_set_background_color(s_minmaxtemp_label, color_cornertextbackground);
	text_layer_set_font(s_minmaxtemp_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_minmaxtemp_label, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(s_minmaxtemp_label));

    // add misc label for windspeed or humidity
	s_misc_label = text_layer_create(GRect(82, 45, 60, 15));
	text_layer_set_text_color(s_misc_label, color_cornertext);
	text_layer_set_background_color(s_misc_label, color_cornertextbackground);
	text_layer_set_font(s_misc_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_misc_label, GTextAlignmentRight);
	layer_add_child(window_layer, text_layer_get_layer(s_misc_label));

	// add city label
	s_city_label = text_layer_create(GRect(bounds.size.w / 2 - 40, 107, 80, 30));
	text_layer_set_text_color(s_city_label, color_cornertext);
	text_layer_set_background_color(s_city_label, color_cornertextbackground);
	text_layer_set_font(s_city_label, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_city_label, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(s_city_label));

	// show hands
	s_hands_layer = layer_create(bounds);
	layer_set_update_proc(s_hands_layer, hands_update_proc);
	layer_add_child(window_layer, s_hands_layer);  

	// add current weather icon
	s_icon_layer = bitmap_layer_create(GRect(bounds.size.w / 2 - 12, bounds.size.h / 2 - 20, 25, 25));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_icon_layer));
    if (s_icon_bitmap) {
        gbitmap_destroy(s_icon_bitmap);
    }
    if (settings.reverse == 1) {
        s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS_REVERSE[cachedWeather.icon_current]);
    }
    else {
        s_icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[cachedWeather.icon_current]);
    }
    bitmap_layer_set_bitmap(s_icon_layer, s_icon_bitmap);
    
    // add current temperature label
	s_temperature_label = text_layer_create(GRect(bounds.size.w / 2 - 12, bounds.size.h / 2 + 2, 30, 21));
	text_layer_set_text_color(s_temperature_label, color_maintext);
	text_layer_set_background_color(s_temperature_label, color_maintextbackground);
	text_layer_set_font(s_temperature_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_temperature_label, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(s_temperature_label));

	// appsync dictionary initial setup
	// if I don't sync all appkeys, I get sync errors, but no idea why...
	Tuplet initial_values[] = {
		TupletInteger(WEATHER_ICON, (uint8_t) cachedWeather.icon_current),
		TupletCString(WEATHER_TEMPERATURE, cachedWeather.temperature),
		TupletCString(WEATHER_CITY, cachedWeather.city),
		TupletCString(WEATHER_SUNTIMES, cachedWeather.suntimes),
		TupletInteger(WEATHER_FORECASTICON, (uint8_t) cachedWeather.forecasticon),
		TupletCString(WEATHER_MINMAXTEMP, cachedWeather.minmaxtemp),
		TupletCString(WEATHER_MISC, cachedWeather.misc),
		TupletInteger(CONFIG_SECONDS, (uint8_t) settings.seconds),
		TupletInteger(CONFIG_HOURVIBES, (uint8_t) settings.hourvibes),
        TupletInteger(CONFIG_REVERSE, (uint8_t) settings.reverse),
        TupletInteger(CONFIG_DISTANCE, (uint8_t) settings.distance),
        TupletInteger(CONFIG_BLUETHEME, (uint8_t) settings.bluetheme)
	};

	app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
	  initial_values, ARRAY_LENGTH(initial_values),
	  sync_tuple_changed_callback, sync_error_callback, NULL
	);

	// init status labels
	handle_battery(battery_state_service_peek());
	handle_bluetooth(bluetooth_connection_service_peek());

  	// get weather on load
    // 	request_weather();
    #if defined(PBL_HEALTH)
        request_health();
    #endif

}


//======================================
// WINDOW UNLOAD
//======================================
static void window_unload(Window *window) {
    layer_destroy(s_simple_bg_layer);
    layer_destroy(s_date_layer);

    text_layer_destroy(s_label_12);
    text_layer_destroy(s_label_4);
    text_layer_destroy(s_label_8);
    text_layer_destroy(s_day_label);
    text_layer_destroy(s_battery_label);
    text_layer_destroy(s_city_label);
    text_layer_destroy(s_temperature_label);
    text_layer_destroy(s_suntimes_label);
    text_layer_destroy(s_minmaxtemp_label);
    text_layer_destroy(s_misc_label);

    if (s_icon_bitmap) {
    gbitmap_destroy(s_icon_bitmap);
    }
    if (s_forecasticon_bitmap) {
    gbitmap_destroy(s_forecasticon_bitmap);
    }
    if (s_bluetooth_bitmap) {
        gbitmap_destroy(s_bluetooth_bitmap);
    }

    fonts_unload_custom_font(custom_font_numerals);

    bitmap_layer_destroy(s_icon_layer);
    bitmap_layer_destroy(s_forecasticon_layer);
    bitmap_layer_destroy(s_bluetooth_layer);
    layer_destroy(s_hands_layer);
}


//======================================
// INIT
//======================================
static void init() {
    // load persistent settings from struct
    if (persist_exists(PERSIST_SETTINGS)) {
        persist_read_data(PERSIST_SETTINGS, &settings, sizeof(settings));
    }
    
    // load cached data from struct
    if (persist_exists(PERSIST_WEATHERDATA)) {
        persist_read_data(PERSIST_WEATHERDATA, &cachedWeather, sizeof(cachedWeather));
    }
    
    // set second or minute updates
    if (settings.seconds == 1) {
        tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
    }
    else {
        tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
    }
    
    // set color settings
    color_handler();

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
	.load = window_load,
	.unload = window_unload,
	});
	window_stack_push(window, true);
	app_message_open(256, 256);

//	s_day_buffer[0] = '\0';
//    s_battery_buffer[0] = '\0';
//    s_bluetooth_buffer[0] = '\0';
//	s_suntimes_buffer[0] = '\0';
//	s_temperature_buffer[0] = '\0';
//	s_minmaxtemp_buffer[0] = '\0';
//	s_misc_buffer[0] = '\0';
//	s_city_buffer[0] = '\0';

	// init hand paths
	s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
	s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	GPoint center = grect_center_point(&bounds);
	gpath_move_to(s_minute_arrow, center);
	gpath_move_to(s_hour_arrow, center);

	// init hour ticks on background
	for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
        s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
	}

	// init battery and bluetooth handlers
	battery_state_service_subscribe(handle_battery);
	bluetooth_connection_service_subscribe(handle_bluetooth);
}


//======================================
// DEINIT
//======================================
static void deinit() {
    // write persists
    persist_write_data(PERSIST_SETTINGS, &settings, sizeof(settings));
    persist_write_data(PERSIST_WEATHERDATA, &cachedWeather, sizeof(cachedWeather));

    app_sync_deinit(&s_sync);
    
    gpath_destroy(s_minute_arrow);
    gpath_destroy(s_hour_arrow);
    for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
        gpath_destroy(s_tick_paths[i]);
    }
    
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    window_destroy(window);
}


//======================================
// MAIN
//======================================
int main() {
  init();
  app_event_loop();
  deinit();
}
