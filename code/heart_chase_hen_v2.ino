/*
 * 趕雞的心 v2 — 會跑步、會亂跑的追逐動畫
 * 硬體：ESP32 + 1.3 吋 OLED (SH1106, I2C)
 * 接線：VCC→3V3、GND→GND、SCL→D22、SDA→D21
 *
 * 這一版角色是用程式「即時畫出來」的，所以可以：
 *  - 兩隻腳交替跑步（跑越快、腳步越快）
 *  - 雞的翅膀拍動、心揮舞樹枝、另一隻手前後擺
 *  - 往左跑就轉身面向左邊
 *  - 在畫面裡亂跑：雞隨機逃竄，心在後面追；快追到時雞會急轉彎爆衝
 *  - 有前後遠近：誰比較靠下面（靠近你）就擋住另一個
 *  - 地上有影子、跑快時會揚起灰塵、心後面有速度線
 * 下方「趕雞的心」四個字像波浪一樣跳動
 *
 * 若畫面右側有雜點，把下面的 SH1106 改成 SSD1306
 */

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

extern const int TITLE_W, TITLE_H;
extern const unsigned char TITLE_bits[];

// ===== 活動範圍（腳踩的位置）=====
const float MIN_X = 24, MAX_X = 104;
const float MIN_Y = 31, MAX_Y = 48;     // 越下面 = 越靠近觀眾
const int   TITLE_Y = 50;
const int   CHAR_W = 14;

// ===== 型別定義（Arduino 規定要放在所有函式前面）=====
typedef bool (*InsideFn)(int dx, int dy);
int g_face = 1;    // 給判斷函式用的面向

// ===== 角色狀態 =====
struct Runner {
  float x, y;        // 腳底中心位置
  float vx, vy;      // 速度
  float phase;       // 跑步週期（控制腳和手）
  int   face;        // 面向：1 = 右，-1 = 左
};

Runner heart = { 40, 42, 0, 0, 0, 1 };
Runner hen   = { 80, 38, 0, 0, 0, 1 };

float henTargetX = 90, henTargetY = 36;
unsigned long nextTarget = 0;
unsigned long panicUntil = 0;     // 雞爆衝逃跑的時間
unsigned long frame = 0;

// ===== 基本繪圖工具（自己做邊界檢查，跑到邊邊也不會亂畫）=====
void px(int x, int y) {
  if (x >= 0 && x < 128 && y >= 0 && y < 64) u8g2.drawPixel(x, y);
}

void line(int x0, int y0, int x1, int y1) {          // Bresenham 畫線
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    px(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void thick(int x0, int y0, int x1, int y1) {          // 2px 粗線
  line(x0, y0, x1, y1);
  line(x0 + 1, y0, x1 + 1, y1);
}

void blit(const unsigned char* bits, int w, int h, int x, int y, int c0, int c1) {
  int bpr = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = c0; i < c1; i++)
      if (pgm_read_byte(&bits[j * bpr + i / 8]) & (1 << (i % 8))) px(x + i, y + j);
}

// 地上的影子（點狀橢圓）
void shadow(int cx, int cy, int rx) {
  for (int i = -rx; i <= rx; i += 2) px(cx + i, cy + 1);
  for (int i = -rx + 3; i <= rx - 3; i += 2) px(cx + i + 1, cy + 2);
}

// ===== 形狀填色工具 =====
// 給一個「這個點在不在形狀裡」的判斷函式，自動畫出：
// 邊緣 = 白色外框、裡面 = 黑色（會把後面的東西擋住，產生前後遠近感）

void drawShape(int cx, int cy, int rx, int ryTop, int ryBot, InsideFn inside, int t) {
  for (int dy = -ryTop; dy <= ryBot; dy++) {
    for (int dx = -rx; dx <= rx; dx++) {
      if (!inside(dx, dy)) continue;
      bool edge = !inside(dx - t, dy) || !inside(dx + t, dy) ||
                  !inside(dx, dy - t) || !inside(dx, dy + t) ||
                  !inside(dx - 1, dy - 1) || !inside(dx + 1, dy - 1) ||
                  !inside(dx - 1, dy + 1) || !inside(dx + 1, dy + 1);
      u8g2.setDrawColor(edge ? 1 : 0);
      px(cx + dx, cy + dy);
    }
  }
  u8g2.setDrawColor(1);
}

// ===== 畫心 =====
const float HS = 9.0f;   // 心的大小
bool heartInside(int dx, int dy) {
  // 經典心形方程式 (x²+y²-1)³ - x²y³ ≤ 0
  float x = dx / HS, y = -dy / HS + 0.05f;
  float a = x * x + y * y - 1;
  return a * a * a - x * x * y * y * y <= 0;
}

void drawHeart(const Runner& h) {
  int d = h.face;
  float s = sinf(h.phase), c = cosf(h.phase);
  int bob = (int)(fabsf(s) * 2);              // 跑步時身體上下彈
  int fx = (int)h.x, fy = (int)h.y;           // 腳底
  int cx = fx, cy = fy - 21 - bob;            // 心的中心

  shadow(fx, fy, 9);

  // --- 腳：兩腳相位相反，前後擺動，往前踏時抬起 ---
  for (int k = 0; k < 2; k++) {
    float ph = h.phase + k * PI;
    int hipX = cx - 2 + k * 3;
    int hipY = cy + 6;
    int footX = fx - 1 + k * 2 + (int)(sinf(ph) * 6) * d;
    int lift = cosf(ph) > 0 ? (int)(cosf(ph) * 3) : 0;
    int footY = fy - 2 - lift;
    thick(hipX, hipY, footX, footY);
    u8g2.drawFilledEllipse(footX + d * 2, footY + 1, 3, 2);   // 實心鞋子
  }

  // --- 心本體（會蓋住腳的上半段）---
  drawShape(cx, cy, 11, 12, 10, heartInside, 2);

  // --- 臉：往面向的方向偏一點 ---
  int ex = cx + d * 1;
  u8g2.drawBox(ex - 4, cy - 3, 2, 3);         // 眼睛
  u8g2.drawBox(ex + 3, cy - 3, 2, 3);
  line(ex - 6, cy - 7, ex - 3, cy - 5);       // 生氣眉毛，往中間壓
  line(ex + 7, cy - 7, ex + 4, cy - 5);
  u8g2.drawDisc(ex, cy + 1, 2, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);  // 張嘴大笑
  px(ex - 7, cy); px(ex - 6, cy + 1);         // 腮紅
  px(ex + 7, cy); px(ex + 8, cy - 1);

  // --- 後手：握拳前後擺 ---
  int bx = cx - d * 10, by = cy + 1;
  int fistX = bx - d * 3 + (int)(s * 3) * d, fistY = by + 4 - (int)(c * 2);
  line(bx, by, fistX, fistY);
  u8g2.drawDisc(fistX, fistY, 1);

  // --- 前手：拿樹枝往前揮 ---
  int ax = cx + d * 10, ay = cy;
  int handX = ax + d * 4, handY = ay + 1;
  line(ax, ay, handX, handY);
  u8g2.drawDisc(handX, handY, 1);
  float ang = -1.0f + sinf(h.phase * 0.5f) * 0.45f;  // 樹枝揮舞角度
  float ca = cosf(ang), sa = sinf(ang);
  int tipX = handX + (int)(ca * 13) * d, tipY = handY + (int)(sa * 13);
  line(handX, handY, tipX, tipY);
  int mX = handX + (int)(ca * 8) * d, mY = handY + (int)(sa * 8);
  line(mX, mY, mX + (int)(cosf(ang - 0.8f) * 5) * d, mY + (int)(sinf(ang - 0.8f) * 5));
  line(mX, mY, mX + (int)(cosf(ang + 0.8f) * 5) * d, mY + (int)(sinf(ang + 0.8f) * 5));

  // --- 速度線（跑快時出現在背後）---
  float sp = sqrtf(h.vx * h.vx + h.vy * h.vy);
  if (sp > 0.9f && (frame / 2) % 2 == 0) {
    int lx = cx - d * 15;
    line(lx, cy - 4, lx - d * 5, cy - 4);
    line(lx - d * 2, cy + 1, lx - d * 7, cy + 1);
  }
}

// ===== 畫雞 =====
bool henInside(int dx, int dy) {
  // 身體（橢圓）+ 頭（圓）合在一起
  float ex = dx / 10.5f, ey = dy / 7.5f;
  if (ex * ex + ey * ey <= 1) return true;
  int hx = dx - g_face * 7, hy = dy + 8;
  return hx * hx + hy * hy <= 40;
}

void drawHen(const Runner& h, bool panic) {
  int d = h.face;
  float s = sinf(h.phase);
  int bob = (int)(fabsf(s) * 2);
  int fx = (int)h.x, fy = (int)h.y;
  int bx = fx, by = fy - 13 - bob;            // 身體中心
  int hx = bx + d * 7, hy = by - 8;           // 頭中心

  shadow(fx, fy, 8);

  // --- 腳：細腿 + 腳趾，跑很快 ---
  for (int k = 0; k < 2; k++) {
    float ph = h.phase + k * PI;
    int hipX = bx - 2 + k * 4;
    int hipY = by + 6;
    int footX = fx - 1 + k * 2 + (int)(sinf(ph) * 4) * d;
    int lift = cosf(ph) > 0 ? (int)(cosf(ph) * 3) : 0;
    int footY = fy - 1 - lift;
    line(hipX, hipY, footX, footY);
    line(footX, footY, footX + d * 3, footY);       // 前趾
    line(footX, footY, footX + d * 2, footY - 2);
    px(footX - d, footY);                           // 後趾
  }

  // --- 尾巴羽毛（畫在身體後面）---
  int tx = bx - d * 9, ty = by - 3;
  thick(tx, ty, tx - d * 4, ty - 5);
  line(tx, ty + 2, tx - d * 5, ty);

  // --- 身體 + 頭 ---
  g_face = d;
  drawShape(bx, by, 14, 15, 8, henInside, 2);

  // --- 雞冠（實心，在頭頂偏後）---
  u8g2.drawDisc(hx - d * 3, hy - 5, 2);
  u8g2.drawDisc(hx, hy - 6, 2);
  u8g2.drawDisc(hx - d * 5, hy - 2, 1);
  // --- 嘴、肉垂、眼睛 ---
  u8g2.drawTriangle(hx + d * 6, hy - 1, hx + d * 10, hy + 1, hx + d * 6, hy + 2);
  px(hx + d * 5, hy + 4); px(hx + d * 6, hy + 4); px(hx + d * 5, hy + 5);
  u8g2.drawBox(hx + (d > 0 ? 1 : -2), hy - 3, 2, 3);

  // --- 翅膀：一上一下拍 ---
  bool up = ((int)(h.phase * 1.2f)) % 2 == 0;
  if (up) {   // 翅膀往上拍，翅尖翹出背部
    line(bx + d * 3, by + 1, bx - d * 3, by - 2);
    line(bx - d * 3, by - 2, bx - d * 9, by - 8);
    line(bx - d * 9, by - 8, bx - d * 8, by - 3);
  } else {    // 翅膀收在身上：像原圖的小弧線
    line(bx - d * 5, by - 1, bx - d * 3, by + 2);
    line(bx - d * 3, by + 2, bx + d * 2, by + 2);
    line(bx + d * 2, by + 2, bx + d * 4, by);
  }

  // --- 慌張的驚嚇線 ---
  if (panic && (frame / 3) % 2 == 0) {
    line(hx + d * 5, hy - 9, hx + d * 7, hy - 12);
    line(hx + d * 8, hy - 6, hx + d * 11, hy - 7);
  }

  // --- 跑快時腳後揚起的灰塵 ---
  float sp = sqrtf(h.vx * h.vx + h.vy * h.vy);
  if (sp > 1.2f) {
    int dx = fx - d * (9 + (int)(frame % 4));
    px(dx, fy - 1); px(dx - d * 2, fy - 3); px(dx - d * 3, fy);
  }
}

// ===== 移動邏輯 =====
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void steer(Runner& r, float tx, float ty, float maxSpeed, float turn) {
  float dx = tx - r.x, dy = (ty - r.y) * 1.6f;
  float dist = sqrtf(dx * dx + dy * dy);
  float wantX = 0, wantY = 0;
  if (dist > 0.5f) { wantX = dx / dist * maxSpeed; wantY = dy / dist * maxSpeed * 0.6f; }
  r.vx += (wantX - r.vx) * turn;              // 慢慢轉向，看起來比較自然
  r.vy += (wantY - r.vy) * turn;
}

void moveRunner(Runner& r) {
  r.x += r.vx; r.y += r.vy;
  if (r.x < MIN_X) { r.x = MIN_X; r.vx = fabsf(r.vx); }
  if (r.x > MAX_X) { r.x = MAX_X; r.vx = -fabsf(r.vx); }
  if (r.y < MIN_Y) { r.y = MIN_Y; r.vy = fabsf(r.vy); }
  if (r.y > MAX_Y) { r.y = MAX_Y; r.vy = -fabsf(r.vy); }
  float sp = sqrtf(r.vx * r.vx + r.vy * r.vy);
  r.phase += sp * 0.45f + 0.02f;              // 跑越快，腳步越快
  if (r.vx > 0.15f) r.face = 1;               // 依照移動方向轉身
  if (r.vx < -0.15f) r.face = -1;
}

void pickHenTarget(bool awayFromHeart) {
  if (awayFromHeart) {
    // 往心的反方向逃，並加一點隨機角度
    float ax = hen.x - heart.x, ay = hen.y - heart.y;
    float ang = atan2f(ay, ax) + random(-70, 71) / 100.0f;
    henTargetX = clampf(hen.x + cosf(ang) * 60, MIN_X, MAX_X);
    henTargetY = clampf(hen.y + sinf(ang) * 20, MIN_Y, MAX_Y);
    // 被逼到牆邊時，改往另一側衝
    if (fabsf(henTargetX - hen.x) < 15) henTargetX = hen.x < 64 ? MAX_X : MIN_X;
  } else {
    henTargetX = random((long)MIN_X, (long)MAX_X + 1);
    henTargetY = random((long)MIN_Y, (long)MAX_Y + 1);
  }
}

void updateAI(unsigned long now) {
  float dx = hen.x - heart.x, dy = hen.y - heart.y;
  float dist = sqrtf(dx * dx + dy * dy * 2.5f);

  // 心快追到 → 雞急轉彎爆衝
  if (dist < 20 && now > panicUntil) {
    panicUntil = now + 900;
    pickHenTarget(true);
    nextTarget = now + 900;
  }
  // 平常隨機亂跑，每隔一段時間換目標
  if (now > nextTarget) {
    pickHenTarget(dist < 40);
    nextTarget = now + random(700, 1800);
  }

  bool panic = now < panicUntil;
  steer(hen, henTargetX, henTargetY, panic ? 2.0f : 1.1f, panic ? 0.35f : 0.12f);
  // 心一直追著雞跑，速度介於雞的散步和爆衝之間
  float side = hen.x > heart.x ? 1 : -1;      // 追到雞的後方
  steer(heart, hen.x - side * 18, hen.y, 1.35f, 0.08f);

  moveRunner(hen);
  moveRunner(heart);
}

// ===== 文字 =====
void drawTitle() {
  int x0 = (128 - TITLE_W) / 2;
  for (int c = 0; c < TITLE_W / CHAR_W; c++) {
    float s = sinf(frame * 0.15f - c * 0.9f);
    blit(TITLE_bits, TITLE_W, TITLE_H, x0, TITLE_Y + (s > 0.6f ? -1 : 0), c * CHAR_W, (c + 1) * CHAR_W);
  }
}

void setup() {
  u8g2.setBusClock(400000);
  u8g2.begin();
  randomSeed(esp_random());
}

void loop() {
  unsigned long now = millis();
  updateAI(now);

  u8g2.clearBuffer();
  // 遠的先畫、近的後畫 → 近的會擋住遠的
  bool panic = now < panicUntil;
  if (heart.y < hen.y) { drawHeart(heart); drawHen(hen, panic); }
  else                 { drawHen(hen, panic); drawHeart(heart); }
  drawTitle();
  u8g2.sendBuffer();

  frame++;
  delay(12);
}

// =====================================================================
//  「趕雞的心」文字點陣圖（請勿手動修改）
// =====================================================================
const int TITLE_W = 56, TITLE_H = 14;
const unsigned char TITLE_bits[] PROGMEM = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x90,0x0f,0x5c,0x81,0x11,0x80,0x01,0xfc,0x08,0x4f,0x81,
  0x19,0x80,0x03,0x98,0x0f,0x75,0xc3,0xfb,0x00,0x07,0xb8,0x0f,0xe7,0x47,0x9e,0x00,0x06,0x7e,
  0x0f,0x7b,0x41,0x8e,0xc0,0x10,0x90,0x0f,0xef,0x43,0x96,0xf0,0x30,0x14,0x02,0x7e,0xc1,0x93,
  0xf0,0x30,0xf4,0x1f,0x7f,0x41,0xb2,0xd0,0x20,0x1c,0x82,0xff,0x43,0xa2,0xd0,0x68,0x1c,0x02,
  0x66,0x41,0x82,0xd0,0x38,0x3e,0x02,0xff,0xc7,0xc3,0x80,0x09,0xe6,0x0f,0x71,0x40,0x70,0x80,
  0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
