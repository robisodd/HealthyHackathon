#include <pebble.h>


#define KEY_USERNAME 0
#define KEY_STEPS 1
#define KEY_ICON 2
#define KEY_MESSAGE 3
#define KEY_READY 20
  
// persistent storage keys
#define STORAGE_VERSION_CODE_KEY 1
#define STORAGE_NAME_KEY 2
#define STORAGE_ICON_KEY 3
#define STORAGE_MESSAGE_KEY 4

static Window *s_window;
static Layer *s_window_layer, *s_output_layer;
char s_current_time_buffer[20], text[500] = "not updated yet", msgstr[100];
void dirty() {if(s_window) if(window_get_root_layer(s_window)) layer_mark_dirty(window_get_root_layer(s_window));}

//uint8_t steps_per_minute[MINUTES_PER_DAY];
uint8_t steps_per_minute[1440];
int mode = 1; // mode 0 = quit, 1 = select day, 2 = scroll through minutes
int graphmode = 1;
time_t view_pos;
int width, height;
int s_step_goal = 0;
int s_step_count = 0, s_step_average = 0;

GBitmap *icon[16];
int user_icon_id = 3;
// char *username = "robisodd";


int current_steps = 0;
int current_goal = 0;
int steps_per_day[30];
int max_steps_per_day = 1;
int max_steps_per_minute = 1;
bool emulator = false;
bool ready = false;

// persistent storage version (i.e. app version at last write)
static int s_storage_version_code = 0;
static char s_username[64] = "zero cool";
// static int s_icon = 0;
char s_leader[256];
static char s_message[256];
void send_steps() {
    
  // Send Steps to FireBase
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, KEY_USERNAME, s_username);
  dict_write_int32(iter, KEY_STEPS, current_steps); // UP
  app_message_outbox_send();

}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");

  // read username
  Tuple *username_t = dict_find(iterator, KEY_USERNAME);
  if(username_t) {
    snprintf(s_username, sizeof(s_username), "%s", username_t->value->cstring);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", s_username);

  // read icon
  Tuple *icon_t = dict_find(iterator, KEY_ICON);
  if(icon_t) {
    user_icon_id = icon_t->value->int32;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "s_icon: %d ", user_icon_id);

  // read message
  Tuple *message_t = dict_find(iterator, KEY_MESSAGE);
  if(message_t) {
    snprintf(s_message, sizeof(s_message), "%s", message_t->value->cstring);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", s_message);

  // ready
  Tuple *s_temp = dict_find(iterator, KEY_READY);
  if(s_temp) {
    snprintf(s_leader, sizeof(s_leader), "%s", s_temp->value->cstring);
    if(!ready)
      send_steps();
    ready = true;
  }
    dirty();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}



// static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
//   text_layer_set_text(text_layer, "Select");


//   // Begin dictionary
//   DictionaryIterator *iter;
//   app_message_outbox_begin(&iter);

//   int step_count = 0;
//   if(HealthServiceAccessibilityMaskAvailable & health_service_metric_accessible(HealthMetricStepCount, 
//     time_start_of_today(), time(NULL))) {
//     step_count = health_service_sum_today(HealthMetricStepCount);
//   } 

//   // Add a key-value pair
//   dict_write_cstring(iter, KEY_USERNAME, s_username);
//   dict_write_int32(iter, KEY_STEPS, step_count); // STEPS?!?!

//   // Send the message!
//   app_message_outbox_send();
// }

// static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
//   text_layer_set_text(text_layer, "Up");


//   // Begin dictionary
//   DictionaryIterator *iter;
//   app_message_outbox_begin(&iter);

//   // Add a key-value pair
//   dict_write_cstring(iter, KEY_USERNAME, s_username);
//   dict_write_uint8(iter, KEY_STEPS, 1); // UP

//   // Send the message!
//   app_message_outbox_send();
// }

// static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
//   text_layer_set_text(text_layer, "Down");


//   // Begin dictionary
//   DictionaryIterator *iter;
//   app_message_outbox_begin(&iter);

//   // Add a key-value pair
//   dict_write_cstring(iter, KEY_USERNAME, s_username);
//   dict_write_uint8(iter, KEY_STEPS, 0); // DOWN

//   // Send the message!
//   app_message_outbox_send();
// }




#include <pebble.h>
#define logging true  // Enable/Disable logging for debugging
//Note: printf uses APP_LOG_LEVEL_DEBUG
#if logging
  #define LOG(...) (printf(__VA_ARGS__))
#else
  #define LOG(...)
#endif

// typedef struct PersistantStorageData {
//   int32_t version;
//   char username[16];
//   uint8_t steps[1440];
//   time_t timestamp;
// } PersistantStorageData;

// PersistantStorageData persist_data;


// Is step data available?
bool step_data_is_available() {
  return HealthServiceAccessibilityMaskAvailable & health_service_metric_accessible(HealthMetricStepCount, time_start_of_today(), time(NULL));
}
int range = 0;

void log_steps_per_hour() {
  char startstr[100], endstr[100];
  const time_t end = time(NULL) - (SECONDS_PER_HOUR * range);
  const time_t start = end - SECONDS_PER_HOUR;
  int steps = health_service_sum(HealthMetricStepCount, start, end);
  strftime(startstr, sizeof(startstr), "%c", localtime(&start));
  strftime(endstr, sizeof(endstr), "%c", localtime(&end));
  LOG("%s to %s: %d Steps", startstr, endstr, steps);
}

int log_steps_per_day(int range) {
  const time_t start = time_start_of_today() - (SECONDS_PER_DAY * range);
  const time_t end = start + SECONDS_PER_DAY;
  int steps = health_service_sum(HealthMetricStepCount, start, end);
  strftime(msgstr, sizeof(msgstr), "%D", localtime(&start));
  LOG("%s: %d Steps", msgstr, steps);
  return steps;
}



static void health_handler(HealthEventType event, void *context) {

//   const time_t start = time_start_of_today();
//   const time_t end = start + SECONDS_PER_DAY;

//   if(event == HealthEventSignificantUpdate) {
//     // Daily step goal
//     s_step_goal = health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
//   }

//   if(event != HealthEventSleepUpdate) {
//     s_step_count = health_service_sum_today(HealthMetricStepCount);  // Todays current step count

//     // Average daily step count for this time of day
//     const time_t start = time_start_of_today();
//     const time_t end = time(NULL);
//     s_step_average = health_service_sum_averaged(HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);

//     layer_mark_dirty(s_output_layer);
//   }

//   int ActiveSecondsToday = health_service_sum_today(HealthMetricActiveSeconds);
//   int DistanceToday = health_service_sum_today(HealthMetricWalkedDistanceMeters);
  
//   static char metersorfeet[10];
//   switch(health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters)) {
//     case MeasurementSystemMetric:
//     strcpy(metersorfeet, "meters");
//     break;
//     case MeasurementSystemImperial: {
//       DistanceToday = (DistanceToday * 328)/100; // Convert to imperial
//       strcpy(metersorfeet, "feet");
//     } break;
//     case MeasurementSystemUnknown:
//     default:
//     strcpy(metersorfeet, "unknown");
//   }

//   snprintf(text, sizeof(text), "StepCount: %d\nAverage: %d\nActive Seconds Today:\n%d\nDistance Today:%d %s\nGoal: %d", s_step_count, s_step_average, ActiveSecondsToday, DistanceToday, metersorfeet, s_step_goal);
  
  current_steps = health_service_sum_today(HealthMetricStepCount);
  current_goal = (int)health_service_sum_averaged(HealthMetricStepCount, time_start_of_today(), time_start_of_today() + SECONDS_PER_DAY, HealthServiceTimeScopeDaily);
  if(emulator)
    current_steps = 4985;
  if(emulator)
    current_goal = 13341;
  if(ready) send_steps();
  
//   int steps_per_day[30];
for(int i = 0; i < 30; i++) {
  if(emulator)
    steps_per_day[i] = rand()%500;
  else
    steps_per_day[i] = log_steps_per_day(i);
  if(steps_per_day[i] > max_steps_per_day)
    max_steps_per_day = steps_per_day[i];
}
  
for(int i = 0; i < ((time(NULL) - time_start_of_today())/60); i++) {
  if(emulator)
    steps_per_minute[i] = rand()%200;
  else
    steps_per_minute[i] = rand()%200;
  if(steps_per_minute[i] > max_steps_per_minute)
    max_steps_per_minute = steps_per_minute[i];
}
  
//   int ActiveSecondsToday = health_service_sum_today(HealthMetricActiveSeconds);
//   int DistanceToday = health_service_sum_today(HealthMetricWalkedDistanceMeters);

  dirty();
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  dirty();
}

void get_steps_per_minute() {
  memset(steps_per_minute, 0, 180);  // Clear steps
  
  const uint32_t max_records = width;
  HealthMinuteData *data = (HealthMinuteData*) malloc(max_records * sizeof(HealthMinuteData));

  time_t start = view_pos - (width * 30);
  time_t end = start + (width * SECONDS_PER_MINUTE);
  uint32_t num_records = health_service_get_minute_history(data, max_records, &start, &end);
  for(uint32_t i = 0; i<num_records; i++) {
    if(data[i].is_invalid) {
      steps_per_minute[num_records - i - 1] = 0;
    } else {
      steps_per_minute[num_records - i - 1] = data[i].steps;
    }
  }
  free(data); // Free the array
}


static void output_layer_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_text_color(ctx, GColorWhite);
  GRect textbox = layer_get_frame(layer);

  
//   static char metersorfeet[10];
//     switch(health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters)) {
//     case MeasurementSystemMetric:
//     strcpy(metersorfeet, "meters");
//     break;
//     case MeasurementSystemImperial: {
//       DistanceToday = (DistanceToday * 328)/100; // Convert to imperial
//       strcpy(metersorfeet, "feet");
//     } break;
//     case MeasurementSystemUnknown:
//     default:
//     strcpy(metersorfeet, "unknown");

  
  


  
  
  #if defined(PBL_RECT)
  time_t t = time(NULL);
  strftime(text, sizeof(text), clock_is_24h_style() ? "%H:%M %b %e" : "%l:%M%P %b %e", localtime(&t));  // Rect
  textbox = layer_get_frame(layer);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_line(ctx, GPoint(0, 20), GPoint(width, 20));  // Rect

  textbox.origin.y += 20;
  textbox.size.w = 92;
  textbox.origin.x += 1;
  graphics_draw_text(ctx, s_username, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  textbox.origin.y += 17;
  snprintf(text, sizeof(text), "Goal:\nSteps:");
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  

  int s_thousands = current_steps / 1000;
  int s_hundreds = current_steps % 1000;
  int g_thousands = current_goal / 1000;
  int g_hundreds = current_goal % 1000;
  if(s_thousands > 0) {
    if(g_thousands > 0) {
      snprintf(text, sizeof(text), "%d,%03d\n%d,%03d", g_thousands, g_hundreds, s_thousands, s_hundreds);
    } else {
      snprintf(text, sizeof(text), "%d\n%d,%03d", g_hundreds, s_thousands, s_hundreds);
    }    
  } else {
    if(g_thousands > 0) {
      snprintf(text, sizeof(text), "%d,%03d\n%d", g_thousands, g_hundreds, s_hundreds);
    } else {
      snprintf(text, sizeof(text), "%d\n%d", g_hundreds, s_hundreds);
    }    
  }

  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  
  //ICON
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  GRect iconrect = GRect(95, 26, 48, 48);
  graphics_fill_rect(ctx, iconrect, 0, GCornerNone);
  graphics_draw_bitmap_in_rect(ctx, icon[user_icon_id], iconrect);
  graphics_draw_rect(ctx, iconrect);
  
  
  
//   graphics_draw_line(ctx, GPoint(0, 76), GPoint(width, 76));

    
  #define GRAPH_HEIGHT 80
  #define GRAPH_BOTTOM 164
  if(mode==1) {
  //GRAPH BACKGROUND
  //GRect graphbox = GRect(18, 83, 122, GRAPH_HEIGHT+2);
  GRect graphbox = GRect(18, GRAPH_BOTTOM - GRAPH_HEIGHT - 1, 122, GRAPH_HEIGHT+2);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, graphbox, 0, GCornerNone);
  graphics_draw_rect(ctx, graphbox);  // Rect

  
  
  //GRAPH TEXT AND DATA
  graphics_context_set_fill_color(ctx, GColorMintGreen);
  GRect graphtext;
  if(graphmode==1) {
    graphtext = GRect(0,83,18,GRAPH_HEIGHT+2);
    snprintf(text, sizeof(text), "W\nE\nE\nK");
    for(int i = 0; i < 7; i++) {
      graphics_fill_rect(ctx, GRect((6-i)*17 + 20, GRAPH_BOTTOM - ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day), 15, ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day)), 0, GCornerNone);
    }
  }
  if(graphmode==2) {
  graphtext = GRect(0,74,18,GRAPH_HEIGHT+2);
    snprintf(text, sizeof(text), "M\nO\nN\nT\nH");
    for(int i = 0; i < 30; i++) {
      graphics_fill_rect(ctx, GRect((29-i)*4 + 19, GRAPH_BOTTOM - ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day), 4, ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day)), 0, GCornerNone);
    }
  }
  if(graphmode==3) {
    graphics_context_set_stroke_color(ctx, GColorMintGreen);
    graphtext = GRect(0,74,18,GRAPH_HEIGHT+2);
    snprintf(text, sizeof(text), "\nD\nA\nY");
    for(int i = 0; i < 1440; i++) {
      //graphics_fill_rect(ctx, GRect((i*120)/1440 + 19, GRAPH_BOTTOM - ((GRAPH_HEIGHT * steps_per_minute[i])/max_steps_per_minute), 1, ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day)), 0, GCornerNone);
      graphics_draw_line(ctx, GPoint((i*120)/1440 + 19, GRAPH_BOTTOM), GPoint((i*120)/1440 + 19, GRAPH_BOTTOM - ((30 * (int)steps_per_minute[i])/max_steps_per_minute)));
    }
    graphics_context_set_stroke_color(ctx, GColorWhite);
  }
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), graphtext, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else {
    //mode 2
      //GRAPH BACKGROUND
  //GRect graphbox = GRect(18, 83, 122, GRAPH_HEIGHT+2);
  GRect graphbox = GRect(18, GRAPH_BOTTOM - GRAPH_HEIGHT - 1, 122, GRAPH_HEIGHT+2);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, graphbox, 0, GCornerNone);
  graphics_draw_rect(ctx, graphbox);  // Rect
// graphbox.origin.y += (GRAPH_HEIGHT-20)/2;
    graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_leader, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), graphbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  
  }
  
  
  
  
  
  
  
  
  
  
  
  #elif defined(PBL_ROUND)
  time_t t = time(NULL);
  strftime(text, sizeof(text), clock_is_24h_style() ? "%H:%M" : "%l:%M%P", localtime(&t));  // Round
  textbox = layer_get_frame(layer);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_line(ctx, GPoint(0, 20), GPoint(width, 20));  // Round
  

  textbox.origin.y += 20;
  textbox.origin.x += 103 - textbox.size.w;
  
  graphics_draw_text(ctx, s_username, fonts_get_system_font(FONT_KEY_GOTHIC_18), textbox, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  textbox.origin.y += 17;
  
  
  
  //snprintf(text, sizeof(text), "Goal: 12,300\nSteps: 10,384");
  
  
  snprintf(text, sizeof(text), "Goal:\nSteps:");
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  

  int s_thousands = current_steps / 1000;
  int s_hundreds = current_steps % 1000;
  int g_thousands = current_goal / 1000;
  int g_hundreds = current_goal % 1000;
  if(s_thousands > 0) {
    if(g_thousands > 0) {
      snprintf(text, sizeof(text), "%d,%03d\n%d,%03d", g_thousands, g_hundreds, s_thousands, s_hundreds);
    } else {
      snprintf(text, sizeof(text), "%d\n%d,%03d", g_hundreds, s_thousands, s_hundreds);
    }    
  } else {
    if(g_thousands > 0) {
      snprintf(text, sizeof(text), "%d,%03d\n%d", g_thousands, g_hundreds, s_hundreds);
    } else {
      snprintf(text, sizeof(text), "%d\n%d", g_hundreds, s_hundreds);
    }    
  }

  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  
  
  
  
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);

  // ICON
  GRect iconrect = GRect(107, 27, 48, 48);
  graphics_context_set_fill_color(ctx, GColorOrange);
  graphics_fill_rect(ctx, iconrect, 0, GCornerNone);
  graphics_draw_bitmap_in_rect(ctx, icon[user_icon_id], iconrect);

  graphics_draw_rect(ctx, iconrect);


  graphics_draw_line(ctx, GPoint(0, 80), GPoint(width, 80));

  #define GRAPH_HEIGHT 80
  #define GRAPH_BOTTOM 160
  if(mode==1) {
  GRect graphtext;
  
  
  // GRAPH
  GRect graphbox = GRect(30, 80, 122, 82);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, graphbox, 0, GCornerNone);
  graphics_draw_rect(ctx, graphbox);  // Rect
  
  
  graphics_context_set_fill_color(ctx, GColorMintGreen);
  
  if(graphmode==1) {
    snprintf(text, sizeof(text), "WEEK");
    for(int i = 0; i < 7; i++) {
      graphics_fill_rect(ctx, GRect((6-i)*17 + 32, GRAPH_BOTTOM - ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day), 15, ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day)), 0, GCornerNone);
    }
  }
  if(graphmode==2) {
  graphtext = GRect(0,74,18,GRAPH_HEIGHT+2);
    snprintf(text, sizeof(text), "MONTH");
    for(int i = 0; i < 30; i++) {
      graphics_fill_rect(ctx, GRect((29-i)*4 + 10+21, GRAPH_BOTTOM - ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day), 4, ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day)), 0, GCornerNone);
    }
  }
  if(graphmode==3) {
    graphics_context_set_stroke_color(ctx, GColorMintGreen);
    graphtext = GRect(0,74,18,GRAPH_HEIGHT+2);
    snprintf(text, sizeof(text), "DAY");
    for(int i = 0; i < 1440; i++) {
      //graphics_fill_rect(ctx, GRect((i*120)/1440 + 19, GRAPH_BOTTOM - ((GRAPH_HEIGHT * steps_per_minute[i])/max_steps_per_minute), 1, ((GRAPH_HEIGHT * steps_per_day[i])/max_steps_per_day)), 0, GCornerNone);
      graphics_draw_line(ctx, GPoint((i*120)/1440 + 21+10, GRAPH_BOTTOM), GPoint((i*120)/1440 + 21+10, GRAPH_BOTTOM - ((30 * (int)steps_per_minute[i])/max_steps_per_minute)));
    }
    graphics_context_set_stroke_color(ctx, GColorWhite);
  }
  
  
  graphtext = GRect(0,180-17,width,17);
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), graphtext, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  
  
  
    } else {
    //mode 2
      //GRAPH BACKGROUND
  //GRect graphbox = GRect(18, 83, 122, GRAPH_HEIGHT+2);
    
  GRect graphbox = GRect(30, 80, 122, 82);
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, graphbox, 0, GCornerNone);
  graphics_draw_rect(ctx, graphbox);  // Rect
    
// graphbox.origin.y //+= (GRAPH_HEIGHT-20)/2;
    graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_leader, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), graphbox, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  
  }
  
  
#endif
  
  //get_steps_per_minute();
//   for(int i = 0; i<width; i++)
//     graphics_draw_line(ctx, GPoint(i, height - 20), GPoint(i, height - steps_per_minute[i] - 20));
  
 // graphics_draw_line(ctx, GPoint(0, height - 20), GPoint(width, height - 20));

//   time_t temp;
//   temp = view_pos - (width * 30);
//   strftime(text, sizeof(text), clock_is_24h_style() ? "%H:%M" : "%l:%M%P", localtime(&temp));
//   textbox.origin.y = height - 20;
//   graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  
//   temp = view_pos + (width * 60);
//   strftime(text, sizeof(text), clock_is_24h_style() ? "%H:%M" : "%l:%M%P", localtime(&temp));
//   graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), textbox, GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
}

// ------------------------------------------------------------------------ //
//  Button Pushing
// ------------------------------------------------------------------------ //
void up_click_handler(ClickRecognizerRef recognizer, void *context) {  //   UP   button was pushed in
  if(mode==1)
    view_pos -= SECONDS_PER_DAY;
  if(mode==2)
    view_pos -= SECONDS_PER_HOUR;
  if(mode==3)
    view_pos -= SECONDS_PER_MINUTE*10;
  dirty();
}

void dn_click_handler(ClickRecognizerRef recognizer, void *context) {  //  DOWN  button was released
  if(mode==1)
    mode=2;
  else if(mode==2)
    mode=1;
  LOG("DOWN %d %s", mode, s_leader);
  
  dirty();
}



void get_steps_per_minute_for_day(int day_offset) {
  memset(steps_per_minute, 0, MINUTES_PER_DAY);  // Clear steps
  
  const uint32_t max_records = 60;
  HealthMinuteData *data = (HealthMinuteData*) malloc(max_records * sizeof(HealthMinuteData));
  int pos = 0;
  int hour_total;
  time_t current = time(NULL);
  time_t start = time_start_of_today() - (SECONDS_PER_DAY * day_offset);
  do {
    hour_total = 0;
    time_t end = start + SECONDS_PER_HOUR;
    uint32_t num_records = health_service_get_minute_history(data, max_records, &start, &end);
    for(uint32_t i = 0; i<num_records && pos<MINUTES_PER_DAY; i++, pos++) {
      if(data[i].is_invalid) {
        steps_per_minute[pos] = 0;
      } else {
        steps_per_minute[pos] = data[i].steps;
        hour_total += data[i].steps;
      }
    }
    strftime(msgstr, sizeof(msgstr), "%c", localtime(&start));
    LOG("%s: %d Steps per hour", msgstr, hour_total);
    start += SECONDS_PER_HOUR;
  } while(start<current && pos<MINUTES_PER_DAY);
  free(data); // Free the array
}

void sl_click_handler(ClickRecognizerRef recognizer, void *context) {  // SELECT button was pushed in
  graphmode = (graphmode % 3)+1;
  dirty();
//   get_steps_per_minute_for_day(0);
}

void bk_click_handler(ClickRecognizerRef recognizer, void *context) {  //  BACK button was released
  window_stack_pop_all(false);
}

void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP,     up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN,   dn_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, sl_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK,   bk_click_handler);
}

// ------------------------------------------------------------------------ //
//  Window and Init
// ------------------------------------------------------------------------ //
static void window_load(Window *window) {
  GRect window_bounds = layer_get_bounds(s_window_layer);
  view_pos = time(NULL);// - window_bounds.size.w*30;
  width = window_bounds.size.w;
  height = window_bounds.size.h;

  // Average indicator
  s_output_layer = layer_create(window_bounds);
  layer_set_update_proc(s_output_layer, output_layer_update_proc);
  layer_add_child(s_window_layer, s_output_layer);

//   // Create a layer to hold the current time
//   s_time_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(0, 0), window_bounds.size.w, 38));
//   text_layer_set_text_color(s_time_layer, GColorWhite);
//   text_layer_set_background_color(s_time_layer, GColorClear);
//   text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
//   text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
//   layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));

  // Subscribe to health events if we can
  if(step_data_is_available()) {
    health_service_events_subscribe(health_handler, NULL);
  } else {
    strcpy(text, "Health Is Not Available");
  }
  window_set_click_config_provider(window, click_config_provider);
  
  
  icon[0] = gbitmap_create_with_resource(RESOURCE_ID_BOY);
  icon[1] = gbitmap_create_with_resource(RESOURCE_ID_FACE);
  icon[2] = gbitmap_create_with_resource(RESOURCE_ID_BAT);
  icon[3] = gbitmap_create_with_resource(RESOURCE_ID_MAC);
  icon[4] = gbitmap_create_with_resource(RESOURCE_ID_MAN);
  icon[5] = gbitmap_create_with_resource(RESOURCE_ID_GIRL);
  icon[6] = gbitmap_create_with_resource(RESOURCE_ID_USER);
  icon[7] = gbitmap_create_with_resource(RESOURCE_ID_SCREAM);

}

static void window_unload(Window *window) {
  layer_destroy(s_output_layer);
}

void init() {
  
  //-----------------------------------------------------------------
  // Set up window
  //-----------------------------------------------------------------
  
  s_window = window_create();
  s_window_layer = window_get_root_layer(s_window);
  window_set_background_color(s_window, GColorBlack);

  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  emulator = watch_info_get_model()==WATCH_INFO_MODEL_UNKNOWN;
  if(emulator)
    light_enable(true);
  
  
  
  
  int username_read_result = 0;
  int message_read_result = 0;

  // get initial storage version, or set to 0 if none
  s_storage_version_code = persist_exists(STORAGE_VERSION_CODE_KEY) ? persist_read_int(STORAGE_VERSION_CODE_KEY) : 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "s_storage_version_code: %d ", s_storage_version_code);

  // get username if set
  if (persist_exists(STORAGE_NAME_KEY)) {
    // TODO: handle return value etc. ?
    username_read_result = persist_read_string(STORAGE_NAME_KEY, s_username, sizeof(s_username));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists; persist_read_string() result %d ", username_read_result);
  } else {
    snprintf(s_username, sizeof(s_username), "acid burn");
    APP_LOG(APP_LOG_LEVEL_DEBUG, "!persist_exists");
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", s_username);

  // get icon, or set to 0 if none
  user_icon_id = persist_exists(STORAGE_ICON_KEY) ? persist_read_int(STORAGE_ICON_KEY) : rand()%8;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "s_icon: %d ", user_icon_id);

  // get message if set
  if (persist_exists(STORAGE_MESSAGE_KEY)) {
    // TODO: handle return value etc. ?
    message_read_result = persist_read_string(STORAGE_MESSAGE_KEY, s_message, sizeof(s_message));

    APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_exists; persist_read_string() result %d ", message_read_result);

  } else {
    snprintf(s_message, sizeof(s_message), "no message");

    APP_LOG(APP_LOG_LEVEL_DEBUG, "!persist_exists");
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", s_message);


  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage with sensible buffer sizes
  app_message_open(64, 64);
  // TODO: ^^^ unsure why small (accurate?!) values fail after several messages?

  

  window_stack_push(s_window, true);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

void deinit() {
  int username_write_result = 0;
  int message_write_result = 0;

  // persist storage version, username, icon, message between launches
  persist_write_int(STORAGE_VERSION_CODE_KEY, s_storage_version_code);

  username_write_result = persist_write_string(STORAGE_NAME_KEY, s_username);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_write_string() result %d ", username_write_result);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", s_username);

  persist_write_int(STORAGE_ICON_KEY, user_icon_id);

  message_write_result = persist_write_string(STORAGE_MESSAGE_KEY, s_message);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "persist_write_string() result %d ", message_write_result);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", s_message);
}

int main() {
//   LOG("Seconds: %d", (int)(time(NULL) - time_start_of_today()));
  init();
  app_event_loop();
  deinit();
}

