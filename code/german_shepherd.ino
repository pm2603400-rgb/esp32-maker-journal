/*
 * 德國牧羊犬 OLED 動畫
 * 硬體：ESP32 + 1.3 吋 OLED (SH1106, I2C)
 * 接線：VCC→3V3、GND→GND、SCL→D22、SDA→D21
 *
 * 側面的德國牧羊犬，特徵：大立耳、長嘴、黑鼻、背上的「馬鞍」花紋（網點表現）、蓬鬆尾巴
 * 四隻腳依步態擺動：走路 / 小跑 / 狂奔，姿勢之間會平滑變換
 *
 * 會隨機輪播這些行為：
 *  [散步]   走來走去、停下來張望、搖尾巴，偶爾叫一聲「汪！」
 *  [嗅聞]   低頭沿地面聞來聞去，聞到東西抬頭叫
 *  [撿球]   球從畫面外飛進來彈跳 → 狗狂奔去咬 → 叼回中間 → 坐下放球 → 叫一聲搖尾巴
 *  [坐下休息] 坐著吐舌頭喘氣、尾巴掃地
 *  [睡覺]   趴下 → 頭靠在前腳睡著、冒 Zzz → 耳朵一動醒來
 *  [跑出去] 衝出畫面 → 從另一邊探出頭張望 → 走進來
 *  按板子上的 BOOT 鍵 = 丟球給牠
 *
 * 【WiFi 無線更新 OTA】
 *  1. 把下面的 WIFI_SSID、WIFI_PASS 改成你的手機熱點（2.4GHz）
 *  2. 第一次必須用 USB 線上傳
 *  3. 之後在 Arduino IDE「工具 → 連接埠」選網路連接埠 esp32-dog，按上傳，密碼輸入 OTA_PASS
 *  ※ 之後每一支要上傳的程式都要保留 OTA 這段，否則只能再用 USB 線上傳
 *
 * 若畫面右側有雜點，把下面的 SH1106 改成 SSD1306
 */

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <ArduinoOTA.h>

// ===== WiFi 設定（改成你自己的）=====
const char* WIFI_SSID = "你的WiFi名稱";
const char* WIFI_PASS = "你的WiFi密碼";
const char* OTA_NAME  = "esp32-dog";
const char* OTA_PASS  = "1234";

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

extern const int WANG_W, WANG_H;
extern const unsigned char WANG_bits[];

// =====================================================================
//  型別定義（Arduino 規定要放在所有函式前面）
// =====================================================================
// 一個「姿勢」：用幾個關鍵點描述狗的身體（座標以腳底為原點、面向右）
struct Pose {
  float sx, sy;       // 肩膀（身體前端中心）
  float hx, hy;       // 屁股（身體後端中心）
  float headX, headY; // 頭的中心
  float snout;        // 嘴巴角度：0 = 水平往前，正數 = 往下
  float fu, fl;       // 前腳：上段、下段角度（0 = 垂直往下，正數 = 往前）
  float hu, hl;       // 後腳
  float tail;         // 尾巴角度（正數 = 往上翹）
  float earBack;      // 耳朵往後倒的程度 0~1
};

enum PoseId { P_STAND, P_WALK, P_RUN, P_SIT, P_LIE, P_SLEEP, P_SNIFF, P_BARK };
enum Scene  { S_WANDER, S_SNIFF, S_FETCH, S_SIT, S_SLEEP, S_RUN_OFF, S_EMPTY, S_PEEK };

// ===== 姿勢表 =====
const Pose POSES[] = {
  //  sx   sy    hx   hy   headX headY snout  fu    fl    hu     hl    tail  earBack
  {   9, -19,  -10, -16,   18,  -28,  0.30,  0,    0,   -0.35, 0.60, -0.8, 0   },  // 站
  {   9, -19,  -10, -16,   19,  -27,  0.35,  0,    0,   -0.35, 0.60, -0.6, 0.1 },  // 走
  {  11, -18,  -11, -17,   21,  -24,  0.20,  0,    0,   -0.30, 0.50,  0.0, 0.8 },  // 跑
  {   6, -19,   -7,  -7,   13,  -30,  0.30,  0.05, 0,    1.45, 0.10, -0.6, 0   },  // 坐
  {   8,  -7,  -10,  -7,   17,  -15,  0.35,  1.50, 0.05, 1.20, 0.40, -0.6, 0   },  // 趴
  {   8,  -7,  -10,  -7,   19,  -10,  0.55,  1.50, 0.05, 1.20, 0.40, -0.7, 0.4 },  // 睡
  {   9, -18,  -10, -17,   21,  -10,  1.15,  0.10, 0,   -0.35, 0.60, -0.2, 0   },  // 聞
  {   9, -20,  -10, -16,   16,  -32, -0.25,  0,    0,   -0.35, 0.60, -0.3, 0   },  // 叫
};

struct Ball { float x, y, vx, vy; bool active, held; };
struct Cap  { float ax, ay, bx, by, ra, rb; };   // 膠囊形（兩端半徑可不同）

// =====================================================================
//  全域狀態
// =====================================================================
const int GROUND = 60;        // 地面高度（狗的腳底）
float dogX = 64, dogVX = 0;
int   face = 1;               // 1 = 面向右，-1 = 面向左
float gait = 0;               // 步態相位
float gaitAmp = 0;            // 腳擺動幅度
Pose  cur = POSES[P_STAND];   // 目前畫面上的姿勢（會慢慢靠近目標姿勢）
PoseId target = P_STAND;
float tailWag = 0.5;          // 搖尾巴力道
bool  mouthOpen = false, mouthBase = false, tongue = false, eyesClosed = false;
unsigned long barkUntil = 0;
char  markC = 0; unsigned long markUntil = 0;
Ball  ball = {0, 0, 0, 0, false, false};

Scene scene = S_WANDER;
unsigned long sceneStart = 0, sceneDur = 10000;
float goalX = 64;
unsigned long nextGoal = 0, nextBark = 0;
int   sceneStep = 0;          // 場景裡的第幾個步驟
int   exitSide = 1;
int   faceLock = 0;            // 非 0 時強制面向這一邊
int   approach = 1;
unsigned long sceneStartAll = 0;
unsigned long frame = 0;
bool  lastBtn = HIGH;

// =====================================================================
//  基本繪圖（自己做邊界檢查）
// =====================================================================
void px(int x, int y) { if (x >= 0 && x < 128 && y >= 0 && y < 64) u8g2.drawPixel(x, y); }

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

void disc(int x0, int y0, int r) {
  for (int j = -r; j <= r; j++)
    for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r + r) px(x0 + i, y0 + j);
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

void blit(const unsigned char* bits, int w, int h, int x, int y) {
  int bpr = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (pgm_read_byte(&bits[j * bpr + i / 8]) & (1 << (i % 8))) px(x + i, y + j);
}

void mark(int x, int y, char c) {
  if (c == '!') {
    line(x, y, x, y + 3); line(x + 1, y, x + 1, y + 3);
    px(x, y + 5); px(x + 1, y + 5);
  } else if (c == '?') {
    px(x, y + 1); px(x + 1, y); px(x + 2, y); px(x + 3, y + 1);
    px(x + 3, y + 2); px(x + 2, y + 3); px(x + 1, y + 4); px(x + 1, y + 6);
  }
}

void zLetter(int x, int y, int s) {   // 畫一個 Z
  line(x, y, x + s, y);
  line(x + s, y, x, y + s);
  line(x, y + s, x + s, y + s);
}

// =====================================================================
//  畫狗：先把身體各部位合成一張「遮罩」，再畫出外框 + 黑色內部 + 馬鞍網點
// =====================================================================
const float SC = 1.3f;           // 狗的整體大小
const float SNOUT_L = 10;        // 嘴巴長度
const int MW = 100, MH = 60;     // 遮罩大小
uint8_t maskBuf[MH][MW];         // 0 = 外面，1 = 身體，2 = 馬鞍花紋
int maskOX, maskOY;              // 遮罩左上角在螢幕的位置

Cap caps[16];
int capN = 0;

void addCap(float ax, float ay, float bx, float by, float ra, float rb) {
  if (capN < 16) caps[capN++] = { ax, ay, bx, by, ra * SC, rb * SC };
}

bool inCap(const Cap& c, float x, float y, float* tOut) {
  float dx = c.bx - c.ax, dy = c.by - c.ay;
  float l2 = dx * dx + dy * dy;
  float t = l2 > 0 ? ((x - c.ax) * dx + (y - c.ay) * dy) / l2 : 0;
  if (t < 0) t = 0; if (t > 1) t = 1;
  float ex = c.ax + dx * t - x, ey = c.ay + dy * t - y;
  float r = c.ra + (c.rb - c.ra) * t;
  if (tOut) *tOut = t;
  return ex * ex + ey * ey <= r * r;
}

// 把「面向右的局部座標」轉成螢幕座標
float WX(float lx) { return dogX + lx * face * SC; }
float WY(float ly) { return GROUND + ly * SC; }

// 一隻腳：從關節出發，上段 + 下段
void legPoints(float ax, float ay, float u, float l, float* kx, float* ky, float* fx, float* fy) {
  const float L1 = 6.5f, L2 = 6.5f;
  *kx = ax + sinf(u) * L1;      *ky = ay + cosf(u) * L1;
  *fx = *kx + sinf(u + l) * L2; *fy = *ky + cosf(u + l) * L2;
}

void drawDog() {
  Pose p = cur;
  float bob = 0;
  if (gaitAmp > 0.05f) bob = -fabsf(sinf(gait)) * gaitAmp * 1.6f;
  if (target == P_SIT || target == P_LIE || target == P_SLEEP)   // 呼吸起伏
    bob = sinf(frame * (tongue ? 0.5f : 0.08f)) * 0.6f;
  p.sy += bob; p.hy += bob; p.headY += bob;

  // ---- 1. 組合身體形狀（局部座標 → 螢幕座標）----
  capN = 0;
  // 身體（屁股端細、胸口粗）
  addCap(WX(p.hx), WY(p.hy), WX(p.sx), WY(p.sy), 5.8f, 7);
  addCap(WX(p.sx), WY(p.sy), WX(p.sx + 1), WY(p.sy + 3), 6.8f, 5.5f);  // 深胸口
  // 脖子、頭、嘴
  addCap(WX(p.sx + 1), WY(p.sy - 2), WX(p.headX), WY(p.headY), 5, 3.5f);   // 粗脖子
  addCap(WX(p.headX), WY(p.headY), WX(p.headX), WY(p.headY), 5, 5);
  float dx = cosf(p.snout), dy = sinf(p.snout);
  float tipX = p.headX + dx * SNOUT_L, tipY = p.headY + dy * SNOUT_L;
  addCap(WX(p.headX + dx * 2), WY(p.headY + dy * 2), WX(tipX), WY(tipY), 3.2f, 2.1f);
  // 尾巴：兩段，蓬鬆
  float wag = sinf(frame * (0.15f + tailWag * 0.35f)) * 0.35f * tailWag;
  float th1 = p.tail + wag, th2 = th1 - 0.6f;
  float t0x = p.hx - 5, t0y = p.hy - 2;
  float t1x = t0x - cosf(th1) * 6, t1y = t0y - sinf(th1) * 6;
  float t2x = t1x - cosf(th2) * 7, t2y = t1y - sinf(th2) * 7;
  if (t1y > -2) t1y = -2;
  if (t2y > -1) t2y = -1;                            // 尾巴不鑽到地下
  addCap(WX(t0x), WY(t0y), WX(t1x), WY(t1y), 2, 2.6f);
  addCap(WX(t1x), WY(t1y), WX(t2x), WY(t2y), 2.6f, 1);
  // 四隻腳：0 前近、1 前遠、2 後近、3 後遠
  bool running = cur.earBack > 0.5f;
  float phs[4];
  if (running) { phs[0] = gait; phs[1] = gait + 0.5f; phs[2] = gait + PI; phs[3] = gait + PI + 0.5f; }  // 奔跑
  else         { phs[0] = gait; phs[1] = gait + PI; phs[2] = gait + PI; phs[3] = gait; }               // 對角步
  float feet[4][2];
  for (int k = 0; k < 4; k++) {
    bool front = k < 2;
    float ax = front ? p.sx : p.hx, ay = (front ? p.sy + 4 : p.hy + 3);
    ax += (k % 2) ? -1.5f : 0;
    float u = front ? p.fu : p.hu, l = front ? p.fl : p.hl;
    float a = gaitAmp * (running ? 0.85f : 0.55f);
    u += a * sinf(phs[k]);
    float lift = cosf(phs[k]) > 0 ? cosf(phs[k]) : 0;
    float bend = running ? 0.5f : 1.3f;
    l += front ? -a * bend * lift : a * bend * 0.8f * lift;   // 抬腳時膝蓋彎
    float kx, ky, fx, fy;
    legPoints(ax, ay, u, l, &kx, &ky, &fx, &fy);
    if (fy > 0) fy = 0;
    feet[k][0] = fx; feet[k][1] = fy;
    float r = (k % 2) ? 1.4f : 1.8f;                      // 遠的腳細一點
    addCap(WX(ax), WY(ay), WX(kx), WY(ky), 2.2f, r);
    addCap(WX(kx), WY(ky), WX(fx), WY(fy), r, r);
  }

  // ---- 2. 算出遮罩 ----
  maskOX = (int)dogX - MW / 2;
  maskOY = GROUND - MH + 3;
  for (int j = 0; j < MH; j++) {
    for (int i = 0; i < MW; i++) {
      float x = maskOX + i + 0.5f, y = maskOY + j + 0.5f;
      uint8_t v = 0;
      for (int c = 0; c < capN; c++) {
        float t;
        if (inCap(caps[c], x, y, &t)) {
          v = 1;
          if (c == 0 && t > 0.1f && t < 0.85f) {           // 身體上半部 → 馬鞍
            float cy = caps[0].ay + (caps[0].by - caps[0].ay) * t;
            if (y < cy + 0.5f) v = 2;
          }
          if (v == 2 || c > 0) break;
        }
      }
      maskBuf[j][i] = v;
    }
  }

  // ---- 3. 畫出：邊緣白、內部黑、馬鞍網點 ----
  for (int j = 0; j < MH; j++) {
    for (int i = 0; i < MW; i++) {
      uint8_t v = maskBuf[j][i];
      if (!v) continue;
      bool edge = i == 0 || j == 0 || i == MW - 1 || j == MH - 1 ||
                  !maskBuf[j][i - 1] || !maskBuf[j][i + 1] || !maskBuf[j - 1][i] || !maskBuf[j + 1][i];
      int sxp = maskOX + i, syp = maskOY + j;
      bool on = edge || (v == 2 && ((sxp + syp) & 1) == 0);
      u8g2.setDrawColor(on ? 1 : 0);
      px(sxp, syp);
    }
  }
  u8g2.setDrawColor(1);

  // ---- 4. 細節：耳朵、眼睛、鼻子、嘴巴、舌頭、腳掌 ----
  float eb = p.earBack;
  for (int k = 0; k < 2; k++) {                      // 兩隻立耳（後面那隻稍微往後）
    float ox = -k * 3.0f;
    float bx1 = p.headX - 3 + ox, by1 = p.headY - 3.5f;
    float bx2 = p.headX + 2 + ox, by2 = p.headY - 4.5f;
    float tx = p.headX - 1.5f + ox - eb * 5, ty = p.headY - 12 + eb * 4;
    line(WX(bx1), WY(by1), WX(tx), WY(ty));
    line(WX(tx), WY(ty), WX(bx2), WY(by2));
    if (k == 0) line(WX(bx1 + 1), WY(by1 - 1), WX(tx + 0.5f), WY(ty + 3));   // 耳朵內側
  }
  // 眼睛
  int ex = (int)WX(p.headX + 1.5f), ey = (int)WY(p.headY - 1.5f);
  if (eyesClosed) { px(ex, ey + 1); px(ex + face, ey + 1); px(ex - face, ey); }
  else { px(ex, ey); px(ex + face, ey); px(ex, ey + 1); px(ex + face, ey + 1); }
  // 鼻子
  disc((int)WX(tipX), (int)WY(tipY), 1);
  // 嘴巴打開（叫、喘氣）
  float nx = -dy, ny = dx;                          // 嘴巴往下的方向
  if (mouthOpen) {
    float m0x = p.headX + dx * 3 + nx * 1.5f, m0y = p.headY + dy * 3 + ny * 1.5f;
    float m1x = tipX - dx * 1 + nx * 1.2f,    m1y = tipY - dy * 1 + ny * 1.2f;
    float m2x = tipX - dx * 2 + nx * 4.5f,    m2y = tipY - dy * 2 + ny * 4.5f;
    u8g2.setDrawColor(0);
    triF(WX(m0x), WY(m0y), WX(m1x), WY(m1y), WX(m2x), WY(m2y));
    u8g2.setDrawColor(1);
    line(WX(m0x), WY(m0y), WX(m2x), WY(m2y));      // 下巴
  }
  if (tongue) {                                      // 吐舌頭
    float qx = p.headX + dx * 6 + nx * 2.5f, qy = p.headY + dy * 6 + ny * 2.5f;
    int tl = 3 + (int)(fabsf(sinf(frame * 0.5f)) * 2);
    line(WX(qx), WY(qy), WX(qx), WY(qy) + tl);
    line(WX(qx) + face, WY(qy), WX(qx) + face, WY(qy) + tl);
  }
  // 腳掌（近的兩隻）
  for (int k = 0; k < 4; k += 2) {
    int fx = (int)WX(feet[k][0] + 1), fy = (int)WY(feet[k][1]);
    line(fx - face, fy, fx + face * 2, fy);
  }

  // ---- 5. 頭上的符號、叫聲泡泡、Zzz ----
  int hx = (int)WX(p.headX), hy = (int)WY(p.headY);
  if (markC && millis() < markUntil) mark(hx + face * 4, max(0, hy - 24), markC);
  if (millis() < barkUntil) {
    int bxp = face > 0 ? hx + 16 : hx - 16 - 22;   // 泡泡放在嘴巴前上方
    int byp = hy - 16;
    if (bxp < 0) bxp = 0; if (bxp > 105) bxp = 105; if (byp < 0) byp = 0;
    u8g2.drawRFrame(bxp, byp, 22, 15, 3);
    blit(WANG_bits, WANG_W, WANG_H, bxp + 2, byp + 2);
    line(bxp + 17, byp + 3, bxp + 17, byp + 9); line(bxp + 18, byp + 3, bxp + 18, byp + 9);
    px(bxp + 17, byp + 11); px(bxp + 18, byp + 11);
  }
  if (eyesClosed && target == P_SLEEP) {
    for (int k = 0; k < 3; k++) {
      float t = fmodf(frame * 0.02f + k / 3.0f, 1.0f);
      int zx = hx + face * (6 + (int)(t * 10)), zy = hy - 8 - (int)(t * 18);
      zLetter(zx, zy, 2 + (int)(t * 3));
    }
  }
}

// =====================================================================
//  動作控制
// =====================================================================
void setPose(PoseId id) { target = id; }

void blendPose(float k) {
  const Pose& t = POSES[target];
  float* a = (float*)&cur;
  const float* b = (const float*)&t;
  for (unsigned i = 0; i < sizeof(Pose) / sizeof(float); i++) a[i] += (b[i] - a[i]) * k;
}

void bark(unsigned long ms) {
  barkUntil = millis() + ms;
}

void setMark(char c, unsigned long ms) { markC = c; markUntil = millis() + ms; }

// 走到 goal，到了回傳 true
bool walkTo(float goal, float speed) {
  float d = goal - dogX;
  float want = 0;
  if (fabsf(d) > 2) want = (d > 0 ? 1 : -1) * speed;
  if (fabsf(d) < 12) want *= fabsf(d) / 12;          // 快到時放慢
  dogVX += (want - dogVX) * 0.12f;
  return fabsf(d) <= 2.5f && fabsf(dogVX) < 0.2f;
}

void updateMotion() {
  dogX += dogVX;
  if (dogVX > 0.1f) face = 1;
  if (dogVX < -0.1f) face = -1;
  if (faceLock) face = faceLock;
  float sp = fabsf(dogVX);
  gait += sp * 0.32f;
  float wantAmp = sp < 0.05f ? 0 : (sp > 1.3f ? 1.0f : 0.35f + sp * 0.45f);
  gaitAmp += (wantAmp - gaitAmp) * 0.15f;
}

// ===== 球 =====
void throwBall() {
  int s = random(0, 2) ? 1 : -1;
  ball.x = s > 0 ? 130 : -2;
  ball.y = 30;
  ball.vx = -s * (random(14, 24) / 10.0f);
  ball.vy = -1.5f;
  ball.active = true; ball.held = false;
}

void updateBall() {
  if (!ball.active) return;
  if (ball.held) {                                  // 叼在嘴裡
    float dx = cosf(cur.snout), dy = sinf(cur.snout);
    ball.x = WX(cur.headX + dx * (SNOUT_L - 1) + (-dy) * 2.5f);
    ball.y = WY(cur.headY + dy * (SNOUT_L - 1) + dx * 2.5f) + 1;
    return;
  }
  ball.vy += 0.25f;
  ball.x += ball.vx; ball.y += ball.vy;
  if (ball.y > GROUND - 2) {                        // 落地彈跳
    ball.y = GROUND - 2;
    ball.vy = -ball.vy * 0.55f;
    if (fabsf(ball.vy) < 0.6f) ball.vy = 0;
    ball.vx *= 0.92f;
  }
  if (ball.x < 3)   { ball.x = 3;   ball.vx = fabsf(ball.vx) * 0.6f; }
  if (ball.x > 124) { ball.x = 124; ball.vx = -fabsf(ball.vx) * 0.6f; }
}

void drawBall() {
  if (!ball.active) return;
  int x = (int)ball.x, y = (int)ball.y;
  disc(x, y, 2);
  u8g2.setDrawColor(0); px(x, y); u8g2.setDrawColor(1);
  if (!ball.held && ball.y >= GROUND - 2.5f) { px(x - 2, GROUND + 1); px(x + 2, GROUND + 1); }  // 影子
}

// =====================================================================
//  場景（劇情）控制
// =====================================================================
void startScene(Scene s, unsigned long now) {
  scene = s; sceneStart = now; sceneStartAll = now; sceneStep = 0; faceLock = 0;
  mouthBase = false; tongue = false; eyesClosed = false;
  switch (s) {
    case S_WANDER: sceneDur = random(8000, 13000); tailWag = 0.6f; nextGoal = now; break;
    case S_SNIFF:  sceneDur = random(6000, 9000);  tailWag = 0.8f; setPose(P_SNIFF);
                   goalX = dogX + face * 60; break;
    case S_FETCH:  sceneDur = 30000; throwBall(); setMark('!', 800); setPose(P_STAND); tailWag = 1; break;
    case S_SIT:    sceneDur = random(5000, 8000); setPose(P_SIT); tailWag = 0.8f; tongue = true; mouthBase = true;
                   nextBark = now + random(1500, 4000); break;
    case S_SLEEP:  sceneDur = 9000; setPose(P_LIE); tailWag = 0.1f; break;
    case S_RUN_OFF: exitSide = dogX < 64 ? -1 : 1; setPose(P_RUN); tailWag = 0.3f; break;
    case S_EMPTY:  break;
    case S_PEEK:   dogX = exitSide > 0 ? -55 : 183;  // 從另一邊回來
                   face = exitSide > 0 ? 1 : -1; dogVX = 0; setPose(P_STAND);
                   cur = POSES[P_STAND]; tailWag = 0.5f; break;
  }
}

void nextScene(unsigned long now) {
  const Scene opts[] = { S_WANDER, S_SNIFF, S_FETCH, S_SIT, S_SLEEP, S_RUN_OFF };
  Scene s;
  do { s = opts[random(0, 6)]; } while (s == scene);
  startScene(s, now);
}

void updateScene(unsigned long now) {
  unsigned long t = now - sceneStart;
  bool barking = now < barkUntil;

  switch (scene) {

    case S_WANDER: {                                  // 散步、張望
      if (now > nextGoal) {
        goalX = random(38, 91);
        nextGoal = now + random(2500, 5000);
      }
      bool arrived = walkTo(goalX, 0.7f);
      if (barking) setPose(P_BARK);
      else setPose(arrived ? P_STAND : P_WALK);
      if (arrived && random(0, 400) == 0) face = -face;          // 回頭張望
      if (arrived && random(0, 500) == 0) bark(700);
      if (t > sceneDur) nextScene(now);
      break;
    }

    case S_SNIFF: {                                   // 低頭聞
      if (sceneStep == 0) {
        if (goalX < 36 || goalX > 92) goalX = dogX < 64 ? 90 : 38;
        walkTo(goalX, 0.35f);
        setPose(P_SNIFF);
        if (fabsf(goalX - dogX) < 4) goalX = dogX < 64 ? 90 : 38;
        if (random(0, 250) == 0) setMark('?', 700);
        if (t > sceneDur) { sceneStep = 1; sceneStart = now; setMark('!', 600); }
      } else {                                         // 聞到了！抬頭叫
        walkTo(dogX, 0);
        setPose(barking ? P_BARK : P_STAND);
        if (t > 300 && t < 340) bark(900);
        if (t > 1800) nextScene(now);
      }
      break;
    }

    case S_FETCH: {                                   // 撿球
      const float REACH = 30;                          // 低頭時嘴巴到身體中心的距離
      if (sceneStep == 0) {                            // 看到球飛進來
        walkTo(dogX, 0);
        setPose(P_STAND);
        faceLock = ball.x > dogX ? 1 : -1;
        if (t > 500) { sceneStep = 1; approach = faceLock; }
      } else if (sceneStep == 1) {                     // 衝過去
        if ((ball.x - dogX) * approach < 0 && fabsf(ball.vx) < 0.3f) approach = -approach;
        float goal = ball.x - approach * REACH;
        goal = constrain(goal, -10.0f, 138.0f);
        walkTo(goal, 2.2f);
        setPose(fabsf(dogVX) > 1.2f ? P_RUN : P_WALK);
        faceLock = fabsf(goal - dogX) < 15 ? approach : 0;
        if (fabsf(dogX - goal) < 4 && fabsf(ball.vx) < 0.3f && ball.y >= GROUND - 3) {
          sceneStep = 2; sceneStart = now; faceLock = approach;
        }
      } else if (sceneStep == 2) {                     // 低頭咬起來
        walkTo(dogX, 0);
        setPose(P_SNIFF);
        if (t > 450) { ball.held = true; tailWag = 1; faceLock = 0; sceneStep = 3; }
      } else if (sceneStep == 3) {                     // 叼回中間
        bool arrived = walkTo(64, 0.9f);
        setPose(P_WALK);
        if (arrived) { sceneStep = 4; sceneStart = now; }
      } else if (sceneStep == 4) {                     // 坐下、放球、叫一聲
        walkTo(dogX, 0);
        setPose(P_SIT);
        if (t > 600 && ball.held) { ball.held = false; ball.vx = face * 0.8f; ball.vy = -1; }
        if (t > 1100 && t < 1140) bark(800);
        if (t > 3200) { ball.active = false; nextScene(now); }
      }
      if (now - sceneStartAll > 25000) { ball.active = false; faceLock = 0; nextScene(now); }   // 保險
      break;
    }

    case S_SIT: {                                     // 坐著喘氣
      walkTo(dogX, 0);
      setPose(P_SIT);
      if (now > nextBark) { bark(800); nextBark = now + 99999; }
      if (t > sceneDur) nextScene(now);
      break;
    }

    case S_SLEEP: {                                   // 趴下 → 睡 → 醒
      walkTo(dogX, 0);
      if (t < 1600)      { setPose(P_LIE); }
      else if (t < 7000) { setPose(P_SLEEP); eyesClosed = true; tailWag = 0; }
      else {
        eyesClosed = false;
        if (t < 7100) setMark('!', 600);
        setPose(t < 7800 ? P_LIE : P_STAND);
        tailWag = 0.7f;
      }
      if (t > sceneDur) nextScene(now);
      break;
    }

    case S_RUN_OFF: {                                 // 衝出畫面
      walkTo(exitSide > 0 ? 230 : -102, 2.4f);
      setPose(P_RUN);
      if (dogX > 182 || dogX < -54) startScene(S_EMPTY, now);
      break;
    }

    case S_EMPTY:
      if (t > 1200) startScene(S_PEEK, now);
      break;

    case S_PEEK: {                                    // 從邊邊探頭
      float edge = exitSide > 0 ? 0 : 128;             // 回來的那一邊
      float hidden = edge - face * 50;                 // 完全看不到
      float peek   = edge - face * 15;                 // 只露出頭
      float p;
      if (t < 600)       p = t / 600.0f;
      else if (t < 1600) p = 1;
      else if (t < 2000) p = 1 - (t - 1600) / 400.0f * 0.8f;
      else if (t < 2600) p = 0.2f + (t - 2000) / 600.0f * 0.8f;
      else               p = 1;
      if (t < 3200) {
        dogX = hidden + (peek - hidden) * p;
        dogVX = 0;
        setPose(P_STAND);
        if (t > 700 && t < 740) setMark('?', 800);
        cur.earBack = (t / 300) % 2 ? 0.3f : 0;        // 耳朵動一動
      } else {
        bool arrived = walkTo(64, 0.8f);
        setPose(arrived ? P_STAND : P_WALK);
        if (arrived) { bark(700); startScene(S_WANDER, now); }
      }
      break;
    }
  }
}

// =====================================================================
//  WiFi 無線更新（OTA）
// =====================================================================
bool otaReady = false;

void drawOtaProgress(int pct) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(22, 18, "OTA Updating...");
  u8g2.drawFrame(14, 28, 100, 12);
  u8g2.drawBox(16, 30, pct * 96 / 100, 8);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  u8g2.drawStr(54, 56, buf);
  u8g2.sendBuffer();
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_NAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() { drawOtaProgress(0); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int last = -1;
    int pct = done * 100 / total;
    if (pct != last) { last = pct; drawOtaProgress(pct); }
  });
  ArduinoOTA.onEnd([]() { drawOtaProgress(100); });
}

// 自動重連：熱點晚開、斷線再回來，都會自己連回去
void handleWiFi(unsigned long now) {
  static bool wasConnected = false;
  static unsigned long lastTry = 0;
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected) {
    if (otaReady) ArduinoOTA.end();
    ArduinoOTA.begin();
    otaReady = true;
  }
  if (!connected && now - lastTry > 15000) {
    lastTry = now;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  wasConnected = connected;
  if (connected && otaReady) ArduinoOTA.handle();
}

// =====================================================================
void setup() {
  pinMode(0, INPUT_PULLUP);          // BOOT 鍵
  u8g2.setBusClock(400000);
  u8g2.begin();
  randomSeed(esp_random());
  setupWiFi();
  startScene(S_WANDER, millis());
}

void loop() {
  unsigned long now = millis();
  handleWiFi(now);

  // BOOT 鍵：丟球
  bool btn = digitalRead(0);
  if (lastBtn == HIGH && btn == LOW && scene != S_FETCH && scene != S_EMPTY && scene != S_RUN_OFF)
    startScene(S_FETCH, now);
  lastBtn = btn;

  updateScene(now);
  updateMotion();
  updateBall();
  blendPose(target == P_RUN || target == P_BARK ? 0.25f : 0.12f);
  mouthOpen = mouthBase || (now < barkUntil && (now / 150) % 2 == 0);   // 叫的時候嘴巴一開一合

  u8g2.clearBuffer();
  for (int x = 0; x < 128; x += 4) px(x, GROUND + 2);   // 地面
  if (scene != S_EMPTY) drawDog();
  drawBall();
  u8g2.sendBuffer();

  frame++;
  delay(10);
}

// =====================================================================
//  「汪」字點陣圖（請勿手動修改）
// =====================================================================
const int WANG_W = 12, WANG_H = 12;
const unsigned char WANG_bits[] PROGMEM = {
  0x06,0x00,0xee,0x07,0x88,0x01,0x80,0x01,0x86,0x01,0x86,0x01,0xe0,0x07,0x88,0x01,0x8c,0x01,0x84,0x01,0xf6,0x0f,0x02,0x00
};
