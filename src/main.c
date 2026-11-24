#include "eadk.h"
#include <stdbool.h>

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumLock";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

#define RGB(r, g, b) ((eadk_color_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define BG     RGB(20, 20, 30)
#define GREY   RGB(200, 200, 200)
#define GOLD   RGB(230, 180, 0)
#define WHITE  RGB(255, 255, 255)
#define RED    RGB(255, 80, 80)
#define GREEN  RGB(0, 255, 0)
#define BLACK  RGB(0, 0, 0)

#define CODE_LEN 6
// >>> TON CODE SECRET : 6 touches a modifier ici <<<
static const eadk_key_t CODE[CODE_LEN] = {
  eadk_key_one, eadk_key_two, eadk_key_three,
  eadk_key_four, eadk_key_five, eadk_key_six
};

static void rect(int x, int y, int w, int h, eadk_color_t c) {
  eadk_rect_t r = {x, y, w, h};
  eadk_display_push_rect_uniform(r, c);
}

static void text(const char *s, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  eadk_point_t p = {x, y};
  eadk_display_draw_string(s, p, false, fg, bg);
}

static void draw_stars(int n) {
  rect(0, 205, 320, 20, BG);
  char s[CODE_LEN * 2 + 1];
  int i, j = 0;
  for (i = 0; i < CODE_LEN; i++) {
    s[j++] = (i < n) ? '*' : '_';
    s[j++] = ' ';
  }
  s[j] = 0;
  text(s, 110, 207, WHITE, BG);
}

static void lock_screen(int n) {
  rect(0, 0, 320, 240, BG);
  rect(130, 20, 60, 8, GREY);
  rect(130, 20, 8, 30, GREY);
  rect(182, 20, 8, 30, GREY);
  rect(115, 50, 90, 50, GOLD);
  rect(156, 66, 8, 20, BG);
  text("CALCULATRICE VERROUILLEE", 45, 115, RED, BG);
  text("Pour deverrouiller,", 80, 140, WHITE, BG);
  text("rendez-la au proprietaire", 52, 160, WHITE, BG);
  text("Code :", 20, 207, GREY, BG);
  draw_stars(n);
}

static void message(const char *s, eadk_color_t c) {
  rect(0, 0, 320, 240, BLACK);
  text(s, 100, 110, c, BLACK);
}

static bool hidden_pressed(eadk_keyboard_state_t k) {
  return eadk_keyboard_key_down(k, eadk_key_shift) &&
         eadk_keyboard_key_down(k, eadk_key_two) &&
         eadk_keyboard_key_down(k, eadk_key_ans);
}

int main(int argc, char *argv[]) {
  int n = 0;
  eadk_key_t entered[CODE_LEN];
  eadk_keyboard_state_t prev = eadk_keyboard_scan();
  lock_screen(0);
  while (true) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (hidden_pressed(k)) {
      message("Mode cache !", GREEN);
      eadk_timing_msleep(2000);
      n = 0;
      lock_screen(0);
      prev = eadk_keyboard_scan();
      continue;
    }
    for (int key = 0; key < 53; key++) {
      bool now = eadk_keyboard_key_down(k, (eadk_key_t)key);
      bool before = eadk_keyboard_key_down(prev, (eadk_key_t)key);
      if (now && !before) {
        if (key == eadk_key_back) {
          if (n > 0) n--;
        } else if (n < CODE_LEN) {
          entered[n++] = (eadk_key_t)key;
        }
        draw_stars(n);
        if (n == CODE_LEN) {
          bool ok = true;
          for (int i = 0; i < CODE_LEN; i++)
            if (entered[i] != CODE[i]) ok = false;
          if (ok) {
            message("Deverrouille", GREEN);
            eadk_timing_msleep(1000);
            return 0;
          }
          message("Code faux", RED);
          eadk_timing_msleep(1500);
          n = 0;
          lock_screen(0);
        }
      }
    }
    prev = k;
    eadk_timing_msleep(30);
  }
}
