/*
 * 趕雞的心 v3 — 會互動的追逐小劇場
 * 硬體：ESP32 + 1.3 吋 OLED (SH1106, I2C)
 * 接線：VCC→3V3、GND→GND、SCL→D22、SDA→D21
 *
 * 角色用程式即時畫出：腳交替跑步、翅膀拍動、揮樹枝、會轉身、有影子
 *
 * 會隨機輪播這些劇情：
 *  [追逐]   雞在畫面亂跑，心在後面追；快追到時雞跳起來、掉羽毛、急轉彎爆衝
 *  [跑出去] 兩個一起衝出畫面 → 空場 → 雞從另一邊「探出頭」張望
 *           → 雞走進來東張西望 → 心從邊邊探出半個身體 → 雞嚇到跳起來 → 繼續追
 *  [累了]   心跑不動停下來喘、流汗；雞停在旁邊轉身跳來跳去挑釁 → 心生氣暴衝
 *  [反擊]   雞突然生氣反過來追心，心嚇到流汗逃跑 → 心回神再追回去
 *
 * 下方「趕雞的心」四個字像波浪一樣跳動
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
const float MIN_Y = 30, MAX_Y = 48;     // 越下面 = 越靠近觀眾
const int   TITLE_Y = 50;
const int   CHAR_W = 14;

// ===== 型別定義（Arduino 規定要放在所有函式前面）=====
typedef bool (*InsideFn)(int dx, int dy);
int g_face = 1;
enum Scene { CHASE, RUN_OFF, EMPTY, HEN_PEEK, HEN_WANDER, HEART_PEEK, TIRED, REVERSE };

// ===== 角色 =====
struct Runner {
  float x, y;       // 腳底位置
  float vx, vy;     // 速度
  float phase;      // 跑步週期
  int   face;       // 1 = 面向右，-1 = 面向左
  float z, vz;      // 跳躍高度與速度
};

Runner heart = { 40, 42, 0, 0, 0, 1, 0, 0 };
Runner hen   = { 80, 38, 0, 0, 0, 1, 0, 0 };

// 心的狀態：0 = 追趕(生氣)、1 = 累了、2 = 被反追(害怕)
int  heartMood = 0;
bool henAngry = false;
unsigned long panicUntil = 0;       // 雞慌張逃跑
unsigned long heartBoostUntil = 0;  // 心暴衝
char heartMark = 0, henMark = 0;    // 頭上的 ! 或 ?
unsigned long heartMarkUntil = 0, henMarkUntil = 0;
unsigned long frame = 0;

// ===== 粒子：羽毛、汗滴 =====
struct Particle { float x, y, vx, vy; int life; bool sweat; };
const int MAX_P = 12;
Particle parts[MAX_P];

// =====================================================================
//  基本繪圖（全部自己做邊界檢查，角色跑出畫面也不會亂畫）
// =====================================================================
void px(int x, int y) {
  if (x >= 0 && x < 128 && y >= 0 && y < 64) u8g2.drawPixel(x, y);
}

void line(int x0, int y0, int x1, int y1) {
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

void thick(int x0, int y0, int x1, int y1) {
  line(x0, y0, x1, y1);
  line(x0 + 1, y0, x1 + 1, y1);
}

void box(int x, int y, int w, int h) {
  for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j);
}

void disc(int x0, int y0, int r) {
  for (int j = -r; j <= r; j++)
    for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r + r) px(x0 + i, y0 + j);
}

void lowerHalfDisc(int x0, int y0, int r) {
  for (int j = 0; j <= r; j++)
    for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r + r) px(x0 + i, y0 + j);
}

void ellipseF(int x0, int y0, int rx, int ry) {
  for (int j = -ry; j <= ry; j++)
    for (int i = -rx; i <= rx; i++)
      if ((float)(i * i) / (rx * rx + 0.5f) + (float)(j * j) / (ry * ry + 0.5f) <= 1) px(x0 + i, y0 + j);
}

void triF(int x0, int y0, int x1, int y1, int x2, int y2) {
  int minX = min(x0, min(x1, x2)), maxX = max(x0, max(x1, x2));
  int minY = min(y0, min(y1, y2)), maxY = max(y0, max(y1, y2));
  for (int y = minY; y <= maxY; y++)
    for (int x = minX; x <= maxX; x++) {
      int d1 = (x - x1) * (y0 - y1) - (x0 - x1) * (y - y1);
      int d2 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2);
      int d3 = (x - x0) * (y2 - y0) - (x2 - x0) * (y - y0);
      bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
      if (!(neg && pos)) px(x, y);
    }
}

void blit(const unsigned char* bits, int w, int h, int x, int y, int c0, int c1) {
  int bpr = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = c0; i < c1; i++)
      if (pgm_read_byte(&bits[j * bpr + i / 8]) & (1 << (i % 8))) px(x + i, y + j);
}

void shadow(int cx, int cy, int rx) {
  for (int i = -rx; i <= rx; i += 2) px(cx + i, cy + 1);
  for (int i = -rx + 3; i <= rx - 3; i += 2) px(cx + i + 1, cy + 2);
}

// 頭上的符號：! 或 ?
void mark(int x, int y, char c) {
  if (c == '!') {
    line(x, y, x, y + 3); line(x + 1, y, x + 1, y + 3);
    px(x, y + 5); px(x + 1, y + 5);
  } else if (c == '?') {
    px(x, y + 1); px(x + 1, y); px(x + 2, y); px(x + 3, y + 1);
    px(x + 3, y + 2); px(x + 2, y + 3); px(x + 1, y + 4); px(x + 1, y + 6);
  }
}

// 形狀填色：邊緣白色外框、裡面黑色（會擋住後面的東西）

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

// =====================================================================
//  畫心
// =====================================================================
const float HS = 9.0f;   // 心的大小
bool heartInside(int dx, int dy) {
  float x = dx / HS, y = -dy / HS + 0.05f;
  float a = x * x + y * y - 1;
  return a * a * a - x * x * y * y * y <= 0;
}

void drawHeart(const Runner& h) {
  int d = h.face;
  float s = sinf(h.phase), c = cosf(h.phase);
  int fx = (int)h.x, ground = (int)h.y;
  int fy = ground - (int)h.z;                   // 跳起來時整個身體往上
  int bob = (int)(fabsf(s) * 1.5f);
  if (heartMood == 1) bob = (int)(fabsf(sinf(frame * 0.5f)) * 1.5f);   // 喘氣
  int cx = fx, cy = fy - 14 - bob;             // 心的中心（短腿）

  shadow(fx, ground, 9);

  // --- 短腿 + 實心鞋 ---
  for (int k = 0; k < 2; k++) {
    float ph = h.phase + k * PI;
    int hipX = cx - 2 + k * 3;
    int hipY = cy + 7;
    int footX = fx - 1 + k * 2 + (int)(sinf(ph) * 3) * d;
    int lift = cosf(ph) > 0 ? (int)(cosf(ph) * 2) : 0;
    if (h.z > 0.5f) lift = 2;                   // 跳起來時腳縮起
    int footY = fy - 2 - lift;
    thick(hipX, hipY, footX, footY);
    ellipseF(footX + d * 2, footY + 1, 2, 1);
  }

  drawShape(cx, cy, 11, 12, 10, heartInside, 2);

  // --- 臉 ---
  int ex = cx + d;
  if (heartMood == 1) {                          // 累：半閉眼、平眉、喘氣的 o 嘴
    box(ex - 4, cy - 2, 2, 1); box(ex + 3, cy - 2, 2, 1);
    line(ex - 6, cy - 5, ex - 3, cy - 5); line(ex + 4, cy - 5, ex + 7, cy - 5);
    disc(ex, cy + 2, 1);
  } else if (heartMood == 2) {                   // 害怕：八字眉、波浪嘴
    box(ex - 4, cy - 3, 2, 3); box(ex + 3, cy - 3, 2, 3);
    line(ex - 6, cy - 5, ex - 3, cy - 7); line(ex + 7, cy - 5, ex + 4, cy - 7);
    px(ex - 2, cy + 2); px(ex - 1, cy + 1); px(ex, cy + 2); px(ex + 1, cy + 1); px(ex + 2, cy + 2);
  } else {                                       // 追趕：生氣眉、張嘴笑
    box(ex - 4, cy - 3, 2, 3); box(ex + 3, cy - 3, 2, 3);
    line(ex - 6, cy - 7, ex - 3, cy - 5); line(ex + 7, cy - 7, ex + 4, cy - 5);
    lowerHalfDisc(ex, cy + 1, 2);
  }

  // --- 後手：握拳擺動 ---
  int bx = cx - d * 10, by = cy + 1;
  int fistX = bx - d * 3 + (int)(s * 3) * d, fistY = by + 4 - (int)(c * 2);
  if (heartMood == 1) { fistX = bx - d; fistY = by + 5; }
  line(bx, by, fistX, fistY);
  disc(fistX, fistY, 1);

  // --- 前手：拿樹枝 ---
  int ax = cx + d * 10, ay = cy;
  int handX = ax + d * 4, handY = ay + 1;
  if (heartMood == 1) { handX = ax + d * 2; handY = ay + 4; }
  line(ax, ay, handX, handY);
  disc(handX, handY, 1);
  float ang = -1.0f + sinf(h.phase * 0.5f) * 0.45f;    // 追趕：揮來揮去
  if (heartMood == 1) ang = 1.1f;                       // 累：樹枝垂下
  if (heartMood == 2) ang = -1.5f + sinf(frame * 0.6f) * 0.15f;  // 害怕：舉高發抖
  float ca = cosf(ang), sa = sinf(ang);
  int tipX = handX + (int)(ca * 13) * d, tipY = handY + (int)(sa * 13);
  line(handX, handY, tipX, tipY);
  int mX = handX + (int)(ca * 8) * d, mY = handY + (int)(sa * 8);
  line(mX, mY, mX + (int)(cosf(ang - 0.8f) * 5) * d, mY + (int)(sinf(ang - 0.8f) * 5));
  line(mX, mY, mX + (int)(cosf(ang + 0.8f) * 5) * d, mY + (int)(sinf(ang + 0.8f) * 5));

  // --- 速度線 ---
  float sp = sqrtf(h.vx * h.vx + h.vy * h.vy);
  if (sp > 0.9f && (frame / 2) % 2 == 0) {
    int lx = cx - d * 15;
    line(lx, cy - 4, lx - d * 5, cy - 4);
    line(lx - d * 2, cy + 1, lx - d * 7, cy + 1);
  }

  if (heartMark && millis() < heartMarkUntil) mark(cx + d * 5, cy - 20, heartMark);
}

// =====================================================================
//  畫雞
// =====================================================================
bool henInside(int dx, int dy) {
  float ex = dx / 10.5f, ey = dy / 7.5f;
  if (ex * ex + ey * ey <= 1) return true;
  int hx = dx - g_face * 7, hy = dy + 8;
  return hx * hx + hy * hy <= 40;
}

void drawHen(const Runner& h, bool panic) {
  int d = h.face;
  float s = sinf(h.phase);
  int fx = (int)h.x, ground = (int)h.y;
  int fy = ground - (int)h.z;
  int bob = (int)(fabsf(s) * 1.5f);
  int bx = fx, by = fy - 12 - bob;
  int hx = bx + d * 7, hy = by - 8;

  shadow(fx, ground, 8);

  // --- 腳 ---
  for (int k = 0; k < 2; k++) {
    float ph = h.phase + k * PI;
    int hipX = bx - 2 + k * 4;
    int hipY = by + 6;
    int footX = fx - 1 + k * 2 + (int)(sinf(ph) * 3) * d;
    int lift = cosf(ph) > 0 ? (int)(cosf(ph) * 2) : 0;
    if (h.z > 0.5f) lift = 2;
    int footY = fy - 1 - lift;
    line(hipX, hipY, footX, footY);
    line(footX, footY, footX + d * 3, footY);
    line(footX, footY, footX + d * 2, footY - 2);
    px(footX - d, footY);
  }

  // --- 尾巴 ---
  int tx = bx - d * 9, ty = by - 3;
  thick(tx, ty, tx - d * 4, ty - 5);
  line(tx, ty + 2, tx - d * 5, ty);

  // --- 身體 + 頭 ---
  g_face = d;
  drawShape(bx, by, 14, 15, 8, henInside, 2);

  // --- 雞冠、嘴、肉垂、眼睛 ---
  disc(hx - d * 3, hy - 5, 2);
  disc(hx, hy - 6, 2);
  disc(hx - d * 5, hy - 2, 1);
  triF(hx + d * 6, hy - 1, hx + d * 10, hy + 1, hx + d * 6, hy + 2);
  px(hx + d * 5, hy + 4); px(hx + d * 6, hy + 4); px(hx + d * 5, hy + 5);
  box(hx + (d > 0 ? 1 : -2), hy - 3, 2, 3);
  if (henAngry) line(hx - d, hy - 5, hx + d * 3, hy - 4);   // 生氣眉

  // --- 翅膀：跑步或慌張時拍動 ---
  float sp = sqrtf(h.vx * h.vx + h.vy * h.vy);
  bool flap = sp > 0.3f || panic || h.z > 0.5f;
  bool up = flap && ((int)(h.phase * 1.2f + frame * 0.1f)) % 2 == 0;
  if (up) {
    line(bx + d * 3, by + 1, bx - d * 3, by - 2);
    line(bx - d * 3, by - 2, bx - d * 9, by - 8);
    line(bx - d * 9, by - 8, bx - d * 8, by - 3);
  } else {
    line(bx - d * 5, by - 1, bx - d * 3, by + 2);
    line(bx - d * 3, by + 2, bx + d * 2, by + 2);
    line(bx + d * 2, by + 2, bx + d * 4, by);
  }

  // --- 慌張線 ---
  if (panic && (frame / 3) % 2 == 0) {
    line(hx + d * 5, hy - 9, hx + d * 7, hy - 12);
    line(hx + d * 8, hy - 6, hx + d * 11, hy - 7);
  }

  // --- 灰塵 ---
  if (sp > 1.2f && h.z < 0.5f) {
    int dx = fx - d * (9 + (int)(frame % 4));
    px(dx, ground - 1); px(dx - d * 2, ground - 3); px(dx - d * 3, ground);
  }

  if (henMark && millis() < henMarkUntil) mark(hx + d * 2, hy - 17, henMark);
}

// =====================================================================
//  物理、粒子
// =====================================================================
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void steer(Runner& r, float tx, float ty, float maxSpeed, float turn) {
  float dx = tx - r.x, dy = (ty - r.y) * 1.6f;
  float dist = sqrtf(dx * dx + dy * dy);
  float wantX = 0, wantY = 0;
  if (dist > 1.5f) {
    float sp = maxSpeed;
    if (dist < 8) sp *= dist / 8;               // 快到目標時放慢
    wantX = dx / dist * sp; wantY = dy / dist * sp * 0.6f;
  }
  r.vx += (wantX - r.vx) * turn;
  r.vy += (wantY - r.vy) * turn;
}

// keepInside = true 時碰到邊界會彈回來；false 時可以跑出畫面
void moveRunner(Runner& r, bool keepInside, bool autoFace) {
  r.x += r.vx; r.y += r.vy;
  if (keepInside) {
    if (r.x < MIN_X && r.vx < 0) r.vx = -r.vx * 0.6f;
    if (r.x > MAX_X && r.vx > 0) r.vx = -r.vx * 0.6f;
  }
  if (r.y < MIN_Y) { r.y = MIN_Y; r.vy = fabsf(r.vy); }
  if (r.y > MAX_Y) { r.y = MAX_Y; r.vy = -fabsf(r.vy); }
  // 跳躍
  if (r.z > 0 || r.vz > 0) {
    r.z += r.vz; r.vz -= 0.35f;
    if (r.z < 0) { r.z = 0; r.vz = 0; }
  }
  float sp = sqrtf(r.vx * r.vx + r.vy * r.vy);
  r.phase += sp * 0.5f + 0.02f;
  if (autoFace) {
    if (r.vx > 0.15f) r.face = 1;
    if (r.vx < -0.15f) r.face = -1;
  }
}

void hop(Runner& r, float power) { if (r.z <= 0.01f) r.vz = power; }

void spawn(float x, float y, int n, bool sweat) {
  for (int i = 0; i < MAX_P && n > 0; i++) {
    if (parts[i].life > 0) continue;
    parts[i].x = x + random(-3, 4);
    parts[i].y = y;
    parts[i].vx = sweat ? random(-6, 7) / 10.0f : random(-12, 13) / 10.0f;
    parts[i].vy = sweat ? -random(3, 8) / 10.0f : -random(5, 16) / 10.0f;
    parts[i].life = sweat ? random(10, 18) : random(20, 32);
    parts[i].sweat = sweat;
    n--;
  }
}

void updateParticles() {
  for (int i = 0; i < MAX_P; i++) {
    Particle& p = parts[i];
    if (p.life <= 0) continue;
    p.x += p.vx; p.y += p.vy;
    p.vy += p.sweat ? 0.15f : 0.06f;            // 羽毛飄比較慢
    p.vx *= 0.95f;
    p.life--;
  }
}

void drawParticles() {
  for (int i = 0; i < MAX_P; i++) {
    Particle& p = parts[i];
    if (p.life <= 0) continue;
    int x = (int)p.x, y = (int)p.y;
    if (p.sweat) { px(x, y); px(x, y + 1); }
    else { px(x, y); px(x + 1, y - 1); px(x - 1, y); }   // 小羽毛
  }
}

void setHeartMark(char c, unsigned long ms) { heartMark = c; heartMarkUntil = millis() + ms; }
void setHenMark(char c, unsigned long ms)   { henMark = c;   henMarkUntil = millis() + ms; }

// =====================================================================
//  劇情（場景）控制
// =====================================================================
Scene scene = CHASE;
unsigned long sceneStart = 0, sceneDur = 8000;
int side = 1;                 // 跑出去的方向
int peekSide = 1;             // 從哪一邊探頭
float henTX = 90, henTY = 36; // 雞的目標點
float heartTX = 40, heartTY = 40;
unsigned long nextTarget = 0;
unsigned long lastTaunt = 0;

float edgeX(int s, float hidden) { return s > 0 ? 128 + hidden : -hidden; }

void henFleeTarget() {
  float ang = atan2f(hen.y - heart.y, hen.x - heart.x) + random(-70, 71) / 100.0f;
  henTX = clampf(hen.x + cosf(ang) * 60, MIN_X, MAX_X);
  henTY = clampf(hen.y + sinf(ang) * 20, MIN_Y, MAX_Y);
  if (fabsf(henTX - hen.x) < 15) henTX = hen.x < 64 ? MAX_X : MIN_X;   // 被逼到牆邊就往另一邊衝
}

void heartFleeTarget() {
  float ang = atan2f(heart.y - hen.y, heart.x - hen.x) + random(-70, 71) / 100.0f;
  heartTX = clampf(heart.x + cosf(ang) * 60, MIN_X, MAX_X);
  heartTY = clampf(heart.y + sinf(ang) * 20, MIN_Y, MAX_Y);
  if (fabsf(heartTX - heart.x) < 15) heartTX = heart.x < 64 ? MAX_X : MIN_X;
}

void startScene(Scene s, unsigned long now) {
  scene = s;
  sceneStart = now;
  henAngry = false;
  heartMood = 0;

  switch (s) {
    case CHASE:
      sceneDur = random(7000, 11000);
      break;
    case RUN_OFF:
      side = hen.x < 64 ? -1 : 1;
      panicUntil = now + 800;
      setHenMark('!', 600);
      hop(hen, 2.2f);
      break;
    case EMPTY:
      peekSide = -side;                          // 從另一邊繞回來
      hen.x = edgeX(peekSide, 20); hen.y = random((long)MIN_Y, (long)MAX_Y);
      hen.vx = hen.vy = 0; hen.face = -peekSide; hen.z = 0;
      heart.x = edgeX(peekSide, 70); heart.y = random((long)MIN_Y, (long)MAX_Y);
      heart.vx = heart.vy = 0; heart.face = -peekSide;
      break;
    case HEN_WANDER:
      henTX = 64 - peekSide * 12;
      henTY = clampf(hen.y + random(-6, 7), MIN_Y, MAX_Y);
      break;
    case HEART_PEEK:
      heart.y = clampf(hen.y + random(-4, 5), MIN_Y, MAX_Y);
      heart.x = edgeX(peekSide, 32);
      heart.face = -peekSide;
      break;
    case TIRED:
      heartMood = 1;
      break;
    case REVERSE:
      henAngry = true;
      heartMood = 2;
      hen.face = heart.x > hen.x ? 1 : -1;
      setHenMark('!', 900);
      hop(hen, 2.5f);
      heartFleeTarget();
      nextTarget = now + 900;
      break;
    default:
      break;
  }
}

// 回到追逐：雞嚇一跳、掉羽毛、往反方向爆衝
void backToChase(unsigned long now) {
  startScene(CHASE, now);
  panicUntil = now + 1100;
  setHenMark('!', 700);
  hop(hen, 2.5f);
  spawn(hen.x, hen.y - 16, 5, false);
  henFleeTarget();
  nextTarget = now + 1100;
}

void updateScene(unsigned long now) {
  unsigned long t = now - sceneStart;
  float dx = hen.x - heart.x, dy = hen.y - heart.y;
  float dist = sqrtf(dx * dx + dy * dy * 2.5f);
  float behind = hen.x > heart.x ? 1 : -1;

  switch (scene) {

    // ---------- 追逐 ----------
    case CHASE: {
      if (dist < 20 && now > panicUntil) {      // 快被追到
        panicUntil = now + 900;
        hop(hen, 2.3f);
        spawn(hen.x, hen.y - 16, 4, false);
        henFleeTarget();
        nextTarget = now + 900;
      }
      if (now > nextTarget) {
        if (dist < 40) henFleeTarget();
        else { henTX = random((long)MIN_X, (long)MAX_X + 1); henTY = random((long)MIN_Y, (long)MAX_Y + 1); }
        nextTarget = now + random(700, 1800);
      }
      bool panic = now < panicUntil;
      steer(hen, henTX, henTY, panic ? 2.0f : 1.1f, panic ? 0.35f : 0.12f);
      float hs = now < heartBoostUntil ? 2.2f : 1.35f;
      steer(heart, hen.x - behind * 18, hen.y, hs, 0.08f);
      moveRunner(hen, true, true);
      moveRunner(heart, true, true);

      if (t > sceneDur) {
        int r = random(0, 3);
        if (r == 0) startScene(RUN_OFF, now);
        else if (r == 1) startScene(TIRED, now);
        else startScene(REVERSE, now);
      }
      break;
    }

    // ---------- 一起衝出畫面 ----------
    case RUN_OFF: {
      panicUntil = now + 200;
      steer(hen, side > 0 ? 180 : -52, hen.y, 2.0f, 0.3f);
      steer(heart, hen.x - side * 18, hen.y, 1.6f, 0.12f);
      moveRunner(hen, false, true);
      moveRunner(heart, false, true);
      bool henOut = hen.x < -30 || hen.x > 158;
      bool heartOut = heart.x < -32 || heart.x > 160;
      if ((henOut && heartOut) || t > 7000) startScene(EMPTY, now);
      break;
    }

    // ---------- 空場 ----------
    case EMPTY:
      if (t > 1200) startScene(HEN_PEEK, now);
      break;

    // ---------- 雞探頭 ----------
    case HEN_PEEK: {
      float p;
      if (t < 600)       p = t / 600.0f;
      else if (t < 1500) p = 1;
      else if (t < 1900) p = 1 - (t - 1500) / 400.0f * 0.8f;   // 縮回去
      else if (t < 2500) p = 0.2f + (t - 1900) / 600.0f * 0.8f; // 再探出來
      else               p = 1;
      hen.x = edgeX(peekSide, 20) - peekSide * 18 * p;
      hen.face = -peekSide;
      if (t > 700 && t < 720) setHenMark('?', 800);
      if (t > 2600 && t < 2620) setHenMark('?', 600);
      hen.phase += 0.02f;
      if (t > 3200) startScene(HEN_WANDER, now);
      break;
    }

    // ---------- 雞走進來東張西望 ----------
    case HEN_WANDER: {
      steer(hen, henTX, henTY, 0.7f, 0.1f);
      bool lookBack = (t > 1300 && t < 1800) || (t > 2300 && t < 2600);
      moveRunner(hen, false, !lookBack);
      if (lookBack) hen.face = peekSide;          // 回頭看
      if (t > 1300 && t < 1320) setHenMark('?', 700);
      if (t > 3000) startScene(HEART_PEEK, now);
      break;
    }

    // ---------- 心從邊邊探出來 ----------
    case HEART_PEEK: {
      steer(hen, hen.x, hen.y, 0, 0.2f);
      moveRunner(hen, false, false);
      float p = t < 500 ? t / 500.0f : 1;
      heart.x = edgeX(peekSide, 32) - peekSide * 30 * p;   // 只露出半個心
      heart.face = -peekSide;
      heart.phase += 0.03f;
      if (t < 1300) hen.face = -peekSide;          // 雞還沒發現
      else hen.face = peekSide;                    // 發現了！
      if (t > 1300 && t < 1320) { setHenMark('!', 800); hop(hen, 2.8f); spawn(hen.x, hen.y - 16, 4, false); }
      if (t > 900 && t < 920) setHeartMark('!', 600);
      if (t > 1900) {
        backToChase(now);
        hop(hen, 0);                               // 剛剛已經跳過
      }
      break;
    }

    // ---------- 心累了，雞挑釁 ----------
    case TIRED: {
      steer(heart, heart.x, heart.y, 0, 0.08f);    // 慢慢停下來
      float spot = clampf(heart.x + behind * 32, MIN_X, MAX_X);
      steer(hen, spot, heart.y, 0.9f, 0.1f);
      moveRunner(heart, true, false);
      float hsp = sqrtf(hen.vx * hen.vx + hen.vy * hen.vy);
      moveRunner(hen, true, hsp > 0.3f);
      if (hsp < 0.3f) {
        hen.face = heart.x > hen.x ? 1 : -1;       // 轉身面向心
        if (now - lastTaunt > 650) { hop(hen, 2.0f); lastTaunt = now; }   // 跳來跳去挑釁
      }
      if (frame % 12 == 0) spawn(heart.x + heart.face * 6, heart.y - 25, 1, true);   // 流汗
      if (t > 3300 && t < 3320) { setHeartMark('!', 700); hop(heart, 2.2f); }
      if (t > 4000) {
        heart.face = hen.x > heart.x ? 1 : -1;
        heartBoostUntil = now + 1300;              // 生氣暴衝
        backToChase(now);
      }
      break;
    }

    // ---------- 雞反擊，心逃跑 ----------
    case REVERSE: {
      if (now > nextTarget) { heartFleeTarget(); nextTarget = now + random(800, 1300); }
      float back = heart.x > hen.x ? 1 : -1;
      steer(hen, heart.x - back * 14, heart.y, 1.45f, 0.1f);
      steer(heart, heartTX, heartTY, 1.6f, 0.2f);
      moveRunner(hen, true, true);
      moveRunner(heart, true, true);
      if (frame % 10 == 0) spawn(heart.x - heart.face * 6, heart.y - 25, 1, true);
      if (t > 4500) {
        heart.face = hen.x > heart.x ? 1 : -1;     // 回神轉身
        setHeartMark('!', 700);
        hop(heart, 2.0f);
        backToChase(now);
      }
      break;
    }
  }
}

// =====================================================================
//  文字
// =====================================================================
void drawTitle() {
  int x0 = (128 - TITLE_W) / 2;
  for (int c = 0; c < TITLE_W / CHAR_W; c++) {
    float s = sinf(frame * 0.15f - c * 0.9f);
    blit(TITLE_bits, TITLE_W, TITLE_H, x0, TITLE_Y + (s > 0.6f ? -1 : 0), c * CHAR_W, (c + 1) * CHAR_W);
  }
}

// =====================================================================
void setup() {
  u8g2.setBusClock(400000);
  u8g2.begin();
  randomSeed(esp_random());
  startScene(CHASE, millis());
}

void loop() {
  unsigned long now = millis();
  updateScene(now);
  updateParticles();

  u8g2.clearBuffer();
  if (scene != EMPTY) {
    bool panic = now < panicUntil || (scene == HEART_PEEK && now - sceneStart > 1300);
    if (heart.y < hen.y) { drawHeart(heart); drawHen(hen, panic); }   // 遠的先畫
    else                 { drawHen(hen, panic); drawHeart(heart); }
  }
  drawParticles();
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
