/*
 * 機器人動態表情 Robot Eyes
 * 硬體：ESP32 + 1.3 吋 OLED (SH1106, I2C)
 * 接線：VCC→3V3、GND→GND、SCL→D22、SDA→D21
 *
 * 功能：
 *  - 自動眨眼、眼睛隨機左右上下看
 *  - 6 種表情：一般、開心、生氣、難過、想睡、驚訝
 *  - 每 5 秒自動換表情；按板子上的 BOOT 鍵可立即切換下一個
 *
 * 若畫面右側有雜點，把下面的 SH1106 改成 SSD1306
 */

#include <U8g2lib.h>
#include <Wire.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const int BTN_PIN = 0;   // 板子上的 BOOT 鍵 = GPIO0，按下時為 LOW

// ===== 表情定義 =====
enum Mood { NORMAL, HAPPY, ANGRY, SAD, TIRED, SURPRISED, MOOD_COUNT };
const char* moodName[] = {"NORMAL", "HAPPY", "ANGRY", "SAD", "TIRED", "SURPRISED"};
Mood mood = NORMAL;

// ===== 動畫參數（目前值 → 目標值，每格慢慢靠近，看起來就會很順）=====
float eyeW = 36, eyeH = 36, tEyeW = 36, tEyeH = 36;   // 眼睛寬高
float lookX = 0, lookY = 0, tLookX = 0, tLookY = 0;   // 視線方向
float eyeOpen = 1, tEyeOpen = 1;                      // 睜眼程度 1=全開 0=閉上
float moodAmt = 0;                                    // 表情遮罩的出現程度 0→1
bool blinking = false;

unsigned long nextBlink = 0, nextLook = 0, nextMood = 0;
bool lastBtn = HIGH;

// 讓 cur 往 target 靠近 speed 的比例
float approach(float cur, float target, float speed) {
  return cur + (target - cur) * speed;
}

void setMood(Mood m) {
  mood = m;
  moodAmt = 0;           // 表情遮罩從 0 開始長出來
  tEyeW = 36;
  tEyeH = 36;
  if (m == HAPPY)     { tEyeW = 38; }
  if (m == TIRED)     { tEyeH = 30; }
  if (m == SURPRISED) { tEyeW = 40; tEyeH = 46; }
  Serial.print("Mood: ");
  Serial.println(moodName[m]);
}

// 畫一隻眼睛；side = -1 左眼、+1 右眼
void drawEye(float cx, float cy, int side) {
  int w = (int)eyeW;
  int h = (int)(eyeH * eyeOpen);
  if (h < 2) h = 2;

  int left = (int)(cx - w / 2.0);
  int top = (int)(cy - h / 2.0);
  int right = left + w;
  int bottom = top + h;

  // 1) 先畫白色圓角眼睛
  int r = (w < h ? w : h) / 2 - 1;
  if (r > 10) r = 10;
  u8g2.setDrawColor(1);
  if (r >= 1) u8g2.drawRBox(left, top, w, h, r);
  else        u8g2.drawBox(left, top, w, h);

  // 2) 再用黑色「挖掉」一部分，做出表情
  u8g2.setDrawColor(0);
  int d = (int)(h * 0.5 * moodAmt);   // 斜切深度

  switch (mood) {
    case HAPPY: {   // 下方挖一個大橢圓 → 變成 ^ ^ 的笑眼
      int ry = (int)(h * 0.6 * moodAmt);
      if (ry > 0) u8g2.drawFilledEllipse((int)cx, bottom + 2, w / 2 + 6, ry);
      break;
    }
    case ANGRY:     // 內側上角斜切 → 眉頭往中間壓
      if (d > 0) u8g2.drawTriangle(left - 1, top - 1, right + 1, top - 1,
                                   side < 0 ? right + 1 : left - 1, top + d);
      break;
    case SAD:       // 外側上角斜切 → 八字眉
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

void drawFace() {
  u8g2.clearBuffer();
  const float gap = 12;                       // 兩眼間距
  float cy = 32 + lookY;
  float lcx = 64 - gap / 2 - eyeW / 2 + lookX;
  float rcx = 64 + gap / 2 + eyeW / 2 + lookX;
  drawEye(lcx, cy, -1);
  drawEye(rcx, cy, 1);
  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  u8g2.setBusClock(400000);   // I2C 加速到 400kHz，動畫才不會卡
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
    nextMood = now + 8000;   // 手動切換後多停留一下
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
  if (blinking && eyeOpen < 0.1) {   // 閉到底了就睜開
    tEyeOpen = 1;
    blinking = false;
    nextBlink = now + random(2000, 5000);
  }

  // --- 隨機看來看去 ---
  if (now >= nextLook) {
    tLookX = random(-16, 17);
    tLookY = random(-6, 7);
    nextLook = now + random(1000, 3000);
  }

  // --- 所有數值往目標靠近（產生平滑動畫）---
  eyeW    = approach(eyeW, tEyeW, 0.25);
  eyeH    = approach(eyeH, tEyeH, 0.25);
  lookX   = approach(lookX, tLookX, 0.2);
  lookY   = approach(lookY, tLookY, 0.2);
  eyeOpen = approach(eyeOpen, tEyeOpen, 0.5);
  moodAmt = approach(moodAmt, 1, 0.2);

  drawFace();
  delay(10);
}
