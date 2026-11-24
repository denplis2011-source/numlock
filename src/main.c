#if defined(__has_include)
#  if __has_include("libs/eadk.h")
#    include "libs/eadk.h"
#  else
#    include "eadk.h"
#  endif
#else
#  include "eadk.h"
#endif
#include <stdbool.h>
#include <stdint.h>

#ifndef SIMULATOR
#define SIMULATOR 0
#endif

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "NumLock";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

// ---------- Reglages ----------
#define CODE_LEN 6
static const char SECRET[CODE_LEN + 1] = "210921";   // mot de passe
#define MAX_TRIES 3                                   // essais avant blocage
#define LOCKOUT_SECONDS 120                           // 2 minutes
#define IDLE_LOOPS 2000                               // ~1 min sans touche -> reverrouille

#define RGB(r, g, b) ((eadk_color_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define GREY  RGB(200, 200, 200)
#define GOLD  RGB(230, 180, 0)
#define RED   RGB(255, 80, 80)
#define GREEN RGB(0, 255, 0)

// ---------- Verrouillage clavier (meme technique que ta version qui marche) ----------
#if SIMULATOR
void disable_keyboard(){}
void off_screen(){}
void restore_keyboard(){}
#else
void disable_keyboard(){ __asm__ volatile("svc #54"); }
void off_screen(){ __asm__ volatile("svc #44"); }
void restore_keyboard(){ __asm__ volatile("svc #51"); }
#endif

// ---------- Touches ----------
typedef struct { eadk_key_t key; char ch; } KeyChar;

static const KeyChar digit_keys[] = {
  { eadk_key_zero, '0' }, { eadk_key_one, '1' }, { eadk_key_two, '2' },
  { eadk_key_three, '3' }, { eadk_key_four, '4' }, { eadk_key_five, '5' },
  { eadk_key_six, '6' }, { eadk_key_seven, '7' }, { eadk_key_eight, '8' },
  { eadk_key_nine, '9' }
};
#define NDIGITS ((int)(sizeof(digit_keys) / sizeof(digit_keys[0])))

// touches en plus pour la calculatrice
static const KeyChar calc_keys[] = {
  { eadk_key_dot, '.' }, { eadk_key_plus, '+' }, { eadk_key_minus, '-' },
  { eadk_key_multiplication, '*' }, { eadk_key_division, '/' },
  { eadk_key_left_parenthesis, '(' }, { eadk_key_right_parenthesis, ')' }
};
#define NCALC ((int)(sizeof(calc_keys) / sizeof(calc_keys[0])))

static void wait_release(eadk_key_t key) {
  eadk_keyboard_state_t s = eadk_keyboard_scan();
  while (eadk_keyboard_key_down(s, key)) {
    eadk_timing_msleep(10);
    s = eadk_keyboard_scan();
  }
}

// ---------- Affichage ----------
static void rect(int x, int y, int w, int h, eadk_color_t c) {
  eadk_display_push_rect_uniform((eadk_rect_t){x, y, w, h}, c);
}

static void text(const char *s, int x, int y, eadk_color_t fg, eadk_color_t bg) {
  eadk_display_draw_string(s, (eadk_point_t){x, y}, true, fg, bg);
}

static void draw_lock(int tries_left) {
  rect(0, 0, 320, 240, eadk_color_black);
  rect(135, 10, 50, 7, GREY);
  rect(135, 10, 7, 28, GREY);
  rect(178, 10, 7, 28, GREY);
  rect(120, 38, 80, 50, GOLD);
  rect(156, 52, 8, 20, eadk_color_black);
  text("Mot de passe :", 85, 105, eadk_color_white, eadk_color_black);
  char t[24] = "Essais restants : X";
  t[18] = '0' + tries_left;
  text(t, 50, 185, GREY, eadk_color_black);
}

static void draw_code(int n) {
  for (int i = 0; i < CODE_LEN; i++) {
    char s[2] = { i < n ? '*' : '_', 0 };
    text(s, 320 / 2 - CODE_LEN * 10 + i * 20 + 5, 140, eadk_color_white, eadk_color_black);
  }
}

static void lockout(void) {
  for (int s = LOCKOUT_SECONDS; s > 0; s--) {
    rect(0, 0, 320, 240, eadk_color_black);
    text("Calculatrice verrouillee", 40, 70, RED, eadk_color_black);
    char t[24] = "Temps restant : 00:00";
    t[16] = '0' + (s / 60) / 10;
    t[17] = '0' + (s / 60) % 10;
    t[19] = '0' + (s % 60) / 10;
    t[20] = '0' + (s % 60) % 10;
    text(t, 50, 120, eadk_color_white, eadk_color_black);
    eadk_timing_msleep(1000);
  }
}

// ---------- Mini calculatrice ----------
static const char *ep;     // position dans l'expression
static bool eerr;          // erreur de calcul

static double parse_expr(void);

static double parse_factor(void) {
  if (*ep == '-') { ep++; return -parse_factor(); }
  if (*ep == '+') { ep++; return parse_factor(); }
  if (*ep == '(') {
    ep++;
    double v = parse_expr();
    if (*ep == ')') ep++; else eerr = true;
    return v;
  }
  if ((*ep >= '0' && *ep <= '9') || *ep == '.') {
    double v = 0;
    while (*ep >= '0' && *ep <= '9') { v = v * 10 + (*ep - '0'); ep++; }
    if (*ep == '.') {
      ep++;
      double d = 0.1;
      while (*ep >= '0' && *ep <= '9') { v += (*ep - '0') * d; d /= 10; ep++; }
    }
    return v;
  }
  eerr = true;
  return 0;
}

static double parse_term(void) {
  double v = parse_factor();
  while (*ep == '*' || *ep == '/') {
    char op = *ep++;
    double r = parse_factor();
    if (op == '*') v *= r;
    else { if (r == 0) eerr = true; else v /= r; }
  }
  return v;
}

static double parse_expr(void) {
  double v = parse_term();
  while (*ep == '+' || *ep == '-') {
    char op = *ep++;
    double r = parse_term();
    v = (op == '+') ? v + r : v - r;
  }
  return v;
}

static void copy_str(char *dst, const char *src) {
  while ((*dst++ = *src++)) {}
}

// evalue l'expression et ecrit le resultat dans out (>= 24 caracteres)
static void calc_eval(const char *expr, char *out) {
  ep = expr;
  eerr = false;
  double v = parse_expr();
  if (*ep != '\0') eerr = true;
  if (eerr || v != v) { copy_str(out, "Erreur"); return; }
  int i = 0;
  if (v < 0) { out[i++] = '-'; v = -v; }
  if (v >= 1e12) { copy_str(out, "Trop grand"); return; }
  long long ip = (long long)v;
  long long frac = (long long)((v - (double)ip) * 1000000.0 + 0.5);
  if (frac >= 1000000) { ip++; frac -= 1000000; }
  char tmp[16];
  int n = 0;
  if (ip == 0) tmp[n++] = '0';
  while (ip > 0) { tmp[n++] = '0' + (int)(ip % 10); ip /= 10; }
  while (n > 0) out[i++] = tmp[--n];
  if (frac > 0) {
    char f[7];
    for (int k = 5; k >= 0; k--) { f[k] = '0' + (int)(frac % 10); frac /= 10; }
    int last = 5;
    while (last >= 0 && f[last] == '0') last--;
    out[i++] = '.';
    for (int k = 0; k <= last; k++) out[i++] = f[k];
  }
  out[i] = '\0';
}

#define EXPR_MAX 28

static void draw_calc(const char *expr, const char *res) {
  rect(0, 0, 320, 240, eadk_color_black);
  text("NumLock - calculatrice", 40, 10, GOLD, eadk_color_black);
  rect(10, 60, 300, 2, GREY);
  text(expr, 10, 80, eadk_color_white, eadk_color_black);
  text(res, 10, 130, GREEN, eadk_color_black);
  text("EXE = calculer  Retour = effacer", 10, 195, GREY, eadk_color_black);
  text("Home = quitter   Off = verrouiller", 10, 215, GREY, eadk_color_black);
}

// ---------- Programme ----------
enum { MODE_LOCK, MODE_CALC };

int main(void) {
  char buf[CODE_LEN + 1] = "";
  int len = 0, tries = 0, mode = MODE_LOCK, idle = 0;
  char expr[EXPR_MAX + 1] = "";
  int elen = 0;
  char res[24] = "";
  eadk_keyboard_state_t state;

  disable_keyboard();
  off_screen();
  draw_lock(MAX_TRIES - tries);
  draw_code(len);

  for (;;) {
    state = eadk_keyboard_scan();

    if (mode == MODE_LOCK) {
      bool changed = false;

      for (int k = 0; k < NDIGITS; ++k) {
        if (eadk_keyboard_key_down(state, digit_keys[k].key)) {
          if (len < CODE_LEN) { buf[len++] = digit_keys[k].ch; buf[len] = '\0'; }
          changed = true;
          wait_release(digit_keys[k].key);
        }
      }
      if (eadk_keyboard_key_down(state, eadk_key_backspace)) {
        if (len > 0) buf[--len] = '\0';
        changed = true;
        wait_release(eadk_key_backspace);
      }
      if (changed) draw_code(len);

      if (eadk_keyboard_key_down(state, eadk_key_ok)) {
        wait_release(eadk_key_ok);
        bool ok = (len == CODE_LEN);
        for (int i = 0; i < CODE_LEN && ok; i++)
          if (buf[i] != SECRET[i]) ok = false;
        len = 0; buf[0] = '\0';
        if (ok) {
          tries = 0;
          mode = MODE_CALC;
          elen = 0; expr[0] = '\0'; res[0] = '\0';
          idle = 0;
          draw_calc(expr, res);
          continue;
        }
        tries++;
        if (tries >= MAX_TRIES) { lockout(); tries = 0; }
        draw_lock(MAX_TRIES - tries);
        draw_code(len);
      }

      if (eadk_keyboard_key_down(state, eadk_key_on_off)) {
        off_screen();
        wait_release(eadk_key_on_off);
        draw_lock(MAX_TRIES - tries);
        draw_code(len);
      }
    } else {
      bool changed = false;

      for (int k = 0; k < NDIGITS; ++k) {
        if (eadk_keyboard_key_down(state, digit_keys[k].key)) {
          if (elen < EXPR_MAX) { expr[elen++] = digit_keys[k].ch; expr[elen] = '\0'; }
          changed = true;
          wait_release(digit_keys[k].key);
        }
      }
      for (int k = 0; k < NCALC; ++k) {
        if (eadk_keyboard_key_down(state, calc_keys[k].key)) {
          if (elen < EXPR_MAX) { expr[elen++] = calc_keys[k].ch; expr[elen] = '\0'; }
          changed = true;
          wait_release(calc_keys[k].key);
        }
      }
      if (eadk_keyboard_key_down(state, eadk_key_backspace)) {
        if (elen > 0) expr[--elen] = '\0'; else res[0] = '\0';
        changed = true;
        wait_release(eadk_key_backspace);
      }
      if (eadk_keyboard_key_down(state, eadk_key_exe) ||
          eadk_keyboard_key_down(state, eadk_key_ok)) {
        if (elen > 0) calc_eval(expr, res);
        changed = true;
        wait_release(eadk_key_exe);
        wait_release(eadk_key_ok);
      }
      if (changed) { draw_calc(expr, res); idle = 0; }

      if (eadk_keyboard_key_down(state, eadk_key_home)) {
        restore_keyboard();      // quitte vers le menu NumWorks
        return 0;
      }
      if (eadk_keyboard_key_down(state, eadk_key_on_off) || ++idle > IDLE_LOOPS) {
        // verrouille a nouveau : eteint, puis redemande le code au reveil
        elen = 0; expr[0] = '\0'; res[0] = '\0';
        off_screen();
        wait_release(eadk_key_on_off);
        mode = MODE_LOCK;
        len = 0; buf[0] = '\0';
        draw_lock(MAX_TRIES - tries);
        draw_code(len);
      }
    }
    eadk_timing_msleep(30);
  }
}

