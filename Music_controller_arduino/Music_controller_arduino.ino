/*
NB: This depends on having "Pin numbering: by GPIO (legacy)" set in the Aeduino IDE

Do not upgrade the Arduino ESP32 board [ackage updated past 2.0.13, without checking 
the TFT_eSPI package first, as of 2025-06 it\s not compatible with any newer versions.
*/

#include "TFT_eSPI.h"

//#include "Krungthep8.h"
//#define AA_FONT_SMALL Krungthep8
//#define AA_FONT_MEDIUM Krungthep12
//#define CHAR_WIDTH 10 // Approx width of digit in pixels

#include "SF-Pro-Display-Regular8.h"
#include "SF-Pro-Display-Regular10.h"
#define AA_FONT_SMALL SFProDisplayRegular8
#define AA_FONT_MEDIUM SFProDisplayRegular10
#define CHAR_WIDTH 9.2 // Approx width of digit in pixels

#include "TestData.h"

//#define GFXFF 1 XXX think this is obsolete - REMOVE ME

//#define LED_IND 1 // Turn on the onboard LEDs, will mess with the LCD

#define BACKLIGHT_PIN 21 // D10

#define BAR_OUTLINE TFT_WHITE
#define BAR_FILL    TFT_YELLOW
#define BAR_BG      TFT_DARKGREY

#define SW_DOWN 0
#define SW_UP   1

#define PLAYER 0
#define OPTIONS 1

#define OPTIONS_PER_COL 12
#define OPTION_X(n) ((n / OPTIONS_PER_COL) * xmax/2)
#define OPTION_Y(n) ((n % OPTIONS_PER_COL) * ymax / OPTIONS_PER_COL)

// Use hardware SPI
TFT_eSPI tft = TFT_eSPI();

const int xmax = 320;
const int ymax = 240;
const int line_height = 24;
const int line_space = 24;
const int bar_height = 20;
const int bar_x_margin = 12;
const int bar_y_margin = 40;
const int bar_max = xmax - bar_x_margin * 2 - 2;
const int time_width = 54;

const uint8_t *current_font = NULL;

int track_len = 0;
int track_pos = 0;
int track_pos_dr = 0;
int last_progress = 0;
int last_pos = 0;
long last_tp_update = 0;
long last_play_time = 0;

int screen_state = PLAYER;

int num_options = 0;

volatile int playing = 0;

char track_md[3][80] = {
  "-",
  "-",
  "-"
};
byte track_md_update[3] = {1, 1, 1};

#define MAX_COMMAND 4096
char cmd[MAX_COMMAND+1];
int cmd_pos = 0;

int option_num = 0;
char *option_buffer = NULL;
char *option_idx[OPTIONS_PER_COL*2];

int8_t rotary_states[4][4] = {
   { 0, -1,  0, -1},
   { 0,  0, +1,  0},
   { 0,  0, +1,  0},
   { 0,  0,  0,  0},
};

volatile int rotary = 0;

#define ROTARY_CLK D2
#define ROTARY_DT  D3

void IRAM_ATTR rotary_isr() {
   static int last_state = digitalRead(D2) * 2 + digitalRead(D3);
   const int clk = digitalRead(D2);
   const int dt = digitalRead(D3);
   const int state = clk*2 + dt;
   rotary += rotary_states[last_state][state];
   last_state = state;
}

void switch_rotary() {
  if (screen_state == PLAYER) {
    Serial.println("LL");
  } else if (screen_state == OPTIONS) {
    Serial.print("SH ");
    Serial.println(option_idx[option_num]);
    free(option_buffer);
    screen_state = PLAYER;
    drawAll();
  }
}

void switch_pp() {
  if (screen_state == PLAYER) {
    Serial.println("PP");
  } else if (screen_state == OPTIONS) {
    Serial.print("PL ");
    Serial.println(option_idx[option_num]);
    free(option_buffer);
    screen_state = PLAYER;
    drawAll();
  }
}

void switch_ll() {
  if (screen_state == PLAYER) {
    Serial.println("LL");
    tft.fillScreen(TFT_BLACK);
    screen_state = OPTIONS;
    rotary = 0;
  } else {
    screen_state = PLAYER;
    drawAll();
  }
}

void switch_fa() {
  if (screen_state == PLAYER) {
    Serial.println("FA");
  } else if (screen_state == OPTIONS) {
    clearOption();
    // This compiler's % wraps to negative numbers :-(
    option_num--;
    if (option_num < 0) {
      option_num += num_options;
    }
    setOption();
  }
}

void switch_sk() {
  if (screen_state == PLAYER) {
    Serial.println("SK");
  } else if (screen_state == OPTIONS) {
    clearOption();
    option_num = (option_num+1) % num_options;
    setOption();
  }
}

#define SWITCHES 5
const byte switch_pin[SWITCHES] = {A0, A1, A2, A3, A4};
byte switch_state[SWITCHES] = {SW_UP, SW_UP, SW_UP, SW_UP, SW_UP};
void (*switch_cmd[SWITCHES])() = {switch_rotary, switch_pp, switch_ll, switch_fa, switch_sk};

void setup(void) {
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  selectFont(AA_FONT_MEDIUM);
  tft.setTextWrap(false);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK, true);
  Serial.begin(115200);
  while (!Serial); // Wait for virtual serial port to intialise
  Serial.setTimeout(10);
  Serial.println("!! Initialising");
  drawBar();

  for (int i=0; i<SWITCHES; i++) {
    pinMode(switch_pin[i], INPUT_PULLUP);
  }

  pinMode(ROTARY_CLK, INPUT_PULLUP);
  pinMode(ROTARY_DT, INPUT_PULLUP);
  attachInterrupt(ROTARY_CLK, rotary_isr, CHANGE);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  setBacklight(255);

#ifdef LED_IND
  #define G_LED_RED 46
  #define G_LED_GREEN 0
  #define G_LED_BLUE 45
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
#endif
}

void selectFont(const uint8_t *font) {
  if (current_font == font) {
    return;
  }

  if (font != NULL) {
    tft.unloadFont();
  }

  tft.loadFont(font);
  current_font = font;
}

void setBacklight(int n) {
  analogWrite(BACKLIGHT_PIN, n);
}

void drawBar() {
  if (screen_state != PLAYER)
    return;
  tft.drawFastHLine(bar_x_margin, ymax-bar_y_margin, xmax - bar_x_margin*2, BAR_OUTLINE);
  tft.drawFastHLine(bar_x_margin, ymax-bar_y_margin-bar_height, xmax - bar_x_margin*2, BAR_OUTLINE);
  tft.drawFastVLine(bar_x_margin, ymax-bar_y_margin-bar_height, bar_height, BAR_OUTLINE);
  tft.drawFastVLine(xmax - bar_x_margin, ymax-bar_y_margin-bar_height, bar_height, BAR_OUTLINE);
  tft.fillRect(bar_x_margin + 1, ymax - bar_height+1 - bar_y_margin, bar_max, bar_height - 2, BAR_BG);
}

void updateText(int line, char *text) {
  if (screen_state != PLAYER)
    return;
  int ytop = 20 + (line_height + line_space) * line;
  tft.fillRect(0, ytop, 320, line_height + line_space, TFT_BLACK);
  tft.setCursor(bar_x_margin, ytop);
  selectFont(AA_FONT_MEDIUM);
  tft.println(text);
}

void drawAll() {
  tft.fillScreen(TFT_BLACK);
  for (int l=0; l<3; l++) {
    track_md_update[l] = 1;
  }
  drawBar();
  updateTrackLength();
  updateTrackPosition(track_len - track_pos_dr);
  last_progress = 0;
}

void drawTime(int x, int y, int seconds, const char *prefix, int datum) {
  if (screen_state != PLAYER)
    return;
  int mins = seconds / 60;
  int secs = seconds % 60;
  char text[9];
  if (datum == TL_DATUM) {
    snprintf(text, 8, "%s%02d:%02d ", prefix, mins, secs);
  } else {
    snprintf(text, 8, " %s%02d:%02d", prefix, mins, secs);
  }
  tft.setTextDatum(datum);
  tft.setCursor(x, y);
  selectFont(AA_FONT_SMALL);
  tft.println(text);
}

void updateTrackLength() {
  if (screen_state == PLAYER)
    drawTime((int)(xmax - bar_x_margin - 5*CHAR_WIDTH), ymax - bar_y_margin + 4, track_len / 10, "", TR_DATUM);
}

void updateTrackPosition(int t) {
  if (screen_state == PLAYER)
    drawTime(bar_x_margin - 4, ymax - bar_y_margin + 4, t / 10, "-", TL_DATUM);
}

void processCommand() {
  setBacklight(255);
  // Debug serial messages Serial.print("!! "); Serial.println(cmd);
  int update_field = -1;
  if (strncmp(cmd, "PI", 3) == 0) {
    Serial.println("!! Pong");
  } else if (strncmp(cmd, "PL", 3) == 0) {
    playing = 1;
  } else if (strncmp(cmd, "PA", 3) == 0) {
    playing = 0;
  } else if (strncmp(cmd, "BA ", 3) == 0) {
    update_field = 0;
  } else if (strncmp(cmd, "AL ", 3) == 0) {
    update_field = 1;
  } else if (strncmp(cmd, "TR ", 3) == 0) {
    update_field = 2;
  } else if (strncmp(cmd, "TL ", 3) == 0) {
    track_len = atoi(cmd+3);
    updateTrackLength();
  } else if (strncmp(cmd, "TP ", 3) == 0) {
    track_pos = atoi(cmd+3);
    track_pos_dr = track_pos;
    last_tp_update = millis();
    updateTrackPosition(track_len - track_pos);
  } else if (strncmp(cmd, "OP ", 3) == 0) {
    screen_state = OPTIONS;
    char buffer[100];
    option_buffer = strdup(cmd+3);
    char *pos = option_buffer;
    for (int i=0; i<OPTIONS_PER_COL*2; i++) {
      option_idx[i] = NULL;
    }
    int opt_count = 0;
    selectFont(AA_FONT_SMALL);
    while (1) {
      char *next_pos = strchr(pos, '|');
      option_idx[opt_count] = pos;

      if (next_pos != NULL) {
        *next_pos = '\0';
      }
      tft.setCursor(OPTION_X(opt_count) + 16, OPTION_Y(opt_count));
      tft.println(pos);
      if (next_pos == NULL) {
        break;
      }
      pos = next_pos + 1;
      if (opt_count == 23) {
        break;
      }
      opt_count++;
    }
    num_options = opt_count+1;
    option_num = 0;
    setOption();
  } else {
    Serial.print("!! Unknown command: ");
    Serial.println(cmd);
  }
  if (update_field > -1) {
    strncpy(track_md[update_field], cmd+3, 80);
    track_md_update[update_field] = 1;
  }
}

void toggleLED() {
  if (digitalRead(LED_BUILTIN) == LOW){
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
   digitalWrite(LED_BUILTIN, LOW);
  }
}

int last_rotary = 0;

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      cmd[cmd_pos] = '\0';
      processCommand();
      cmd_pos = 0;
    } else if (cmd_pos < MAX_COMMAND) {
      cmd[cmd_pos] = inChar;
      cmd_pos++;
    }
  }

  for (int l=0; l<3; l++) {
    if (track_md_update[l]) {
      track_md_update[l] = 0;
      updateText(l, track_md[l]);
    }
  }

  long now = millis();
  if (playing) {
    last_play_time = now;
    if (now > last_tp_update + 1000) {
      track_pos_dr += (now - last_tp_update) / 100;
      if (track_pos_dr > track_len) {
        track_pos_dr = track_len;
        Serial.println("SY");
      }
      last_tp_update = now;
    }
  } else {
    last_tp_update = now;
    if (now - last_play_time > 60000) {
      setBacklight(16);
    }
  }

  float prop = float(track_pos_dr) / float(track_len);
  if (track_len == 0) {
    prop = 0.0;
  }
  int progress = bar_max * prop;
  if (progress > bar_max) {
    progress = bar_max;
  } else if (progress < 0) {
    progress = 0;
  }

  if (screen_state == PLAYER) {
    if (progress < last_progress) {
      tft.fillRect(bar_x_margin + progress + 1, ymax - bar_height+1 - bar_y_margin, last_progress - progress, bar_height - 1, BAR_BG);
    } else if (progress > last_progress) {
      tft.fillRect(bar_x_margin + last_progress + 1, ymax - bar_height+1 - bar_y_margin, progress - last_progress, bar_height - 1, BAR_FILL);
    }
    if (track_pos_dr / 10 != last_pos / 10) {
      last_pos = track_pos_dr;
      updateTrackPosition(track_len - track_pos_dr);
    }
    last_progress = progress;
  }

  
  if (rotary != last_rotary) {
    if (screen_state == OPTIONS) {
      clearOption();
      option_num = (option_num + rotary - last_rotary) % num_options;
      if (option_num < 0) {
        option_num += num_options;
      }
      setOption();
    } else if (screen_state == PLAYER) {
      int jump = (rotary-last_rotary) * 10;
      Serial.printf("JU %+d\r\n", jump);
    }
    last_rotary = rotary;
  }

  for (int i=0; i<SWITCHES; i++) {
    int v = digitalRead(switch_pin[i]);
    if (v == SW_DOWN and switch_state[i] == SW_UP) {
      switch_cmd[i]();
    }
    switch_state[i] = v;
  }

  /* Test mode */
  if (switch_state[0] == SW_DOWN && switch_state[1] == SW_DOWN && millis() - test_trigger > 1000) {
    playing = 1;
    strcpy(track_md[0], test_data[test_num].artist);
    strcpy(track_md[1], test_data[test_num].album);
    strcpy(track_md[2], test_data[test_num].title);
    track_len = test_data[test_num].length;
    for (int l=0; l<3; l++) {
      track_md_update[l] = 1;
    }
    progress = 0;
    track_pos = 0;
    track_pos_dr = track_pos;
    last_tp_update = millis();
    test_trigger = last_tp_update;
    updateTrackPosition(0);
    updateTrackLength();
    test_num = (test_num + 1) % NUM_TESTS;
  }

#ifdef LED_IND
  toggleLED();
  if (playing) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, LOW);
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
  }
#endif

  delay(100);
}

void clearOption() {
  int x = OPTION_X(option_num);
  int y = OPTION_Y(option_num);
  tft.fillRect(x, y, 16, 16, TFT_BLACK);
}

void setOption() {
  int x = OPTION_X(option_num);
  int y = OPTION_Y(option_num);
  tft.fillRect(x+2, y+2, 11, 11, TFT_WHITE);
}