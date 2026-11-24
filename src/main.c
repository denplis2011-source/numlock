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

static void rect(int x, int y, int w, int h, eadk_color_t c) {
  eadk_rect_t r = {x, y, w, h};
  eadk_display_push_rect_uniform(r, c);
}

static void text(const char *s, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  eadk_point_t p = {x, y};
  eadk_display_draw_string(s, p, false, fg, bg);
}

static void lock_screen(void) {
  rect(0, 0, 320, 240, BG);
  rect(130, 30, 60, 8, GREY);
  rect(130, 30, 8, 36, GREY);
  rect(182, 30, 8, 36, GREY);
  rect(115, 66, 90, 60, GOLD);
  rect(156, 86, 8, 22, BG);
  text("CALCULATRICE VERROUILLEE", 45, 140, RED, BG);
  text("Pour deverrouiller,", 80, 165, WHITE, BG);
  text("rendez-la au proprietaire", 52, 185, WHITE, BG);
}

static void message(const char *s) {
  rect(0, 0, 320, 240, BLACK);
  text(s, 100, 110, GREEN, BLACK);
}

static bool unlock_pressed(eadk_keyboard_state_t k) {
  return eadk_keyboard_key_down(k, eadk_key_shift) &&
         eadk_keyboard_key_down(k, eadk_key_one) &&
         eadk_keyboard_key_down(k, eadk_key_ans);
}

static bool hidden_pressed(eadk_keyboard_state_t k) {
  return eadk_keyboard_key_down(k, eadk_key_shift) &&
         eadk_keyboard_key_down(k, eadk_key_two) &&
         eadk_keyboard_key_down(k, eadk_key_ans);
}

int main(int argc, char *argv[]) {
  lock_screen();
  while (true) {
    eadk_keyboard_state_t k = eadk_keyboard_scan();
    if (unlock_pressed(k)) {
      message("Deverrouille");
      eadk_timing_msleep(1000);
      return 0;
    }
    if (hidden_pressed(k)) {
      message("Mode cache !");
      eadk_timing_msleep(2000);
      lock_screen();
    }
    eadk_timing_msleep(50);
  }
}
