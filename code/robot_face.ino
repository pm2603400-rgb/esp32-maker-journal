/*
 * 機器人動態表情（眼睛 + 嘴巴）Robot Face
 * 硬體：ESP32 + 1.3 吋 OLED (SH1106, I2C)
 * 接線：VCC→3V3、GND→GND、SCL→D22、SDA→D21
 *
 * 功能：
 *  - 自動眨眼、眼睛隨機看來看去，嘴巴會跟著臉一起微微轉向
 *  - 6 種表情，眼睛和嘴巴會平滑變形過去：
 *      一般(微笑)、開心(張嘴大笑)、生氣、難過、想睡、驚訝(O 型嘴)
 *  - 每 5 秒自動換表情；按板子上的 BOOT 鍵可立即切換下一個
 *
 * 若畫面右側有雜點，把下面的 SH1106 改成 SSD1306
 */

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const int BTN_PIN = 0;   // 板子上的 BOOT 鍵 = GPIO0，按下時為 LOW

// ===== 版面位置 =====
const float EYE_Y   = 21;   // 眼睛中心高度
const float EYE_GAP = 12;   // 兩眼間距
const float MOUTH_Y = 47;   // 嘴巴中心高度

// ===== 表情定義 =====
enum Mood { NORMAL, HAPPY, ANGRY, SAD, TIRED, SURPRISED, MOOD_COUNT };
const char* moodName[] = {"NORMAL", "HAPPY", "ANGRY", "SAD", "TIRED", "SURPRISED"};

// 每種表情的「目標外型」
// 嘴巴用上唇、下唇兩條曲線描述：數字 > 0 中間往下彎(笑)，< 0 中間往上拱(哭)
// 上下唇差距越大，嘴巴張越開；round = 1 時變成圓形(O 型嘴)
struct Face {
  float eyeW, eyeH;          // 眼睛寬、高
  float mouthW;              // 嘴巴寬
  float mouthTop, mouthBot;  // 上唇、下唇彎曲量
  float mouthRound;          // 0 = 一般曲線，1 = 圓形
};

const Face faces[MOOD_COUNT] = {
  // eyeW eyeH  mouthW  top  bot  round
  {  30,  30,    28,     4,   4,   0 },   // NORMAL    微笑
  {  32,  30,    30,     2,  12,   0 },   // HAPPY     張嘴大笑
  {  30,  30,    24,    -3,  -3,   0 },   // ANGRY     嘴角往下
  {  30,  30,    22,    -6,  -6,   0 },   // SAD       明顯的倒 U
  {  30,  24,    14,     0,   0,   0 },   // TIRED     一字小嘴
  {  34,  34,    16,    -7,   7,   1 },   // SURPRISED O 型嘴
};

Mood mood = NORMAL;

// ===== 動畫參數（目前值 → 目標值，每格慢慢靠近）=====
float eyeW = 30, eyeH = 30;
float lookX = 0, lookY = 0, tLookX = 0, tLookY = 0;
float eyeOpen = 1, tEyeOpen = 1;
float moodAmt = 0;   // 眼睛表情遮罩出現程度 0→1
float mouthW = 28, mouthTop = 4, mouthBot = 4, mouthRound = 0;
bool blinking = false;

unsigned long nextBlink = 0, nextLook = 0, nextMood = 0;
bool lastBtn = HIGH;

float approach(float cur, float target, float speed) {
  return cur + (target - cur) * speed;
}

void setMood(Mood m) {
  mood = m;
  moodAmt = 0;
  Serial.print("Mood: ");
  Serial.println(moodName[m]);
}

// ===== 畫眼睛；side = -1 左眼、+1 右眼 =====
void drawEye(float cx, float cy, int side) {
  int w = (int)eyeW;
  int h = (int)(eyeH * eyeOpen);
  if (h < 2) h = 2;

  int left = (int)(cx - w / 2.0);
  int top = (int)(cy - h / 2.0);
  int right = left + w;
  int bottom = top + h;

  // 1) 白色圓角眼睛
  int r = (w < h ? w : h) / 2 - 1;
  if (r > 8) r = 8;
  u8g2.setDrawColor(1);
  if (r >= 1) u8g2.drawRBox(left, top, w, h, r);
  else        u8g2.drawBox(left, top, w, h);

  // 2) 用黑色挖掉一部分做出表情
  u8g2.setDrawColor(0);
  int d = (int)(h * 0.5 * moodAmt);

  switch (mood) {
    case HAPPY: {   // 下方挖橢圓 → ^ ^ 笑眼
      int ry = (int)(h * 0.6 * moodAmt);
      if (ry > 0) u8g2.drawFilledEllipse((int)cx, bottom + 2, w / 2 + 6, ry);
      break;
    }
    case ANGRY:     // 內側上角斜切
      if (d > 0) u8g2.drawTriangle(left - 1, top - 1, right + 1, top - 1,
                                   side < 0 ? right + 1 : left - 1, top + d);
      break;
    case SAD:       // 外側上角斜切
      if (d > 0) u8g2.drawTriangle(left - 1, top - 1, right + 1, top - 1,
                                   side < 0 ? left - 1 : right + 1, top + d);
      break;
    case TIRED: {   // 上眼皮蓋下來
      int lid = (int)(h * 0.45 * moodAmt);
      if (lid > 0) u8g2.drawBox(left, top, w, lid);
      break;
    }
    default:
      break;
  }
  u8g2.setDrawColor(1);
}

// ===== 畫嘴巴：逐欄畫直線，把上唇到下唇之間填滿 =====
void drawMouth(float mx, float my, float extraBot) {
  float halfW = mouthW / 2;
  int half = (int)halfW;
  for (int x = -half; x <= half; x++) {
    float t = x / halfW;                 // -1(左嘴角) ~ 1(右嘴角)
    if (t > 1) t = 1;
    if (t < -1) t = -1;
    float para = 1 - t * t;              // 拋物線：一般嘴型
    float circ = sqrtf(1 - t * t);       // 半圓：O 型嘴
    float f = para + (circ - para) * mouthRound;

    float yT = my + mouthTop * f;
    float yB = my + (mouthBot + extraBot) * f;
    float y1 = fminf(yT, yB);
    float y2 = fmaxf(yT, yB) + 3;        // 線條粗 3px
    int len = (int)(y2 - y1 + 0.5f);
    if (len < 1) len = 1;
    u8g2.drawVLine((int)(mx + x), (int)y1, len);
  }
}

void drawFace() {
  u8g2.clearBuffer();

  float cy = EYE_Y + lookY;
  float lcx = 64 - EYE_GAP / 2 - eyeW / 2 + lookX;
  float rcx = 64 + EYE_GAP / 2 + eyeW / 2 + lookX;
  drawEye(lcx, cy, -1);
  drawEye(rcx, cy, 1);

  // 開心時嘴巴會微微彈跳，像在笑
  float bounce = 0;
  if (mood == HAPPY) bounce = sinf(millis() / 120.0f) * 1.5f * moodAmt;

  // 嘴巴最後畫，才不會被眼睛的黑色遮罩蓋掉
  drawMouth(64 + lookX * 0.5f, MOUTH_Y, bounce);

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  u8g2.setBusClock(400000);   // I2C 加速到 400kHz，動畫才順
  u8g2.begin();
  setMood(NORMAL);

  unsigned long now = millis();
  nextBlink = now + 2000;
  nextLook = now + 1000;
  nextMood = now + 5000;
}

void loop() {
  unsigned long now = millis();

  // --- BOOT 鍵：按一下換下一個表情 ---
  bool btn = digitalRead(BTN_PIN);
  if (lastBtn == HIGH && btn == LOW) {
    setMood((Mood)((mood + 1) % MOOD_COUNT));
    nextMood = now + 8000;
  }
  lastBtn = btn;

  // --- 自動換表情 ---
  if (now >= nextMood) {
    setMood((Mood)((mood + 1) % MOOD_COUNT));
    nextMood = now + 5000;
  }

  // --- 隨機眨眼 ---
  if (!blinking && now >= nextBlink) {
    tEyeOpen = 0;
    blinking = true;
  }
  if (blinking && eyeOpen < 0.1) {
    tEyeOpen = 1;
    blinking = false;
    nextBlink = now + random(2000, 5000);
  }

  // --- 隨機看來看去 ---
  if (now >= nextLook) {
    tLookX = random(-14, 15);
    tLookY = random(-3, 4);
    nextLook = now + random(1000, 3000);
  }

  // --- 所有數值往目標靠近 ---
  const Face& f = faces[mood];
  eyeW       = approach(eyeW, f.eyeW, 0.25);
  eyeH       = approach(eyeH, f.eyeH, 0.25);
  mouthW     = approach(mouthW, f.mouthW, 0.2);
  mouthTop   = approach(mouthTop, f.mouthTop, 0.2);
  mouthBot   = approach(mouthBot, f.mouthBot, 0.2);
  mouthRound = approach(mouthRound, f.mouthRound, 0.2);
  lookX      = approach(lookX, tLookX, 0.2);
  lookY      = approach(lookY, tLookY, 0.2);
  eyeOpen    = approach(eyeOpen, tEyeOpen, 0.5);
  moodAmt    = approach(moodAmt, 1, 0.2);

  drawFace();
  delay(10);
}
