/* =====================================================================
   網站內容資料：想改文字、加作品、加章節，改這個檔案就好
   ===================================================================== */

window.SITE = {
  title: "ESP32 自造筆記",
  subtitle: "從零開始的 ESP32 + OLED 學習歷程與作品集",
  author: "L",
  startDate: "2026-10-23",
};

/* ---------------- 作品展示 ---------------- */
window.WORKS = [
  {
    id: "shepherd-v2",
    title: "德國牧羊犬（AI 繪圖版）",
    date: "2026-10-23",
    status: "使用中",
    featured: true,
    cover: "assets/img/shepherd-v2.gif",
    tags: ["動畫", "AI 繪圖", "WiFi 更新", "按鈕互動"],
    summary: "用 AI 畫好 sprite sheet，再轉成 OLED 點陣圖。白底黑線、會撿球、睡覺、探頭，按 BOOT 鍵就丟球給牠。",
    highlights: [
      "AI 畫圖 → Python 自動切格、縮小、轉 1-bit 點陣圖",
      "整個畫面黑白反轉，看起來跟原圖一樣（白底黑線）",
      "劇情：散步、嗅聞、撿球、坐下、睡覺、跑出畫面再探頭",
      "內建 WiFi 無線更新（OTA），之後不用接線就能改程式",
    ],
    lessons: [
      "黑底白線（負片）會讓輪廓看不懂，維持原圖的明暗最重要",
      "AI 很容易畫出重複的格子 → 走路像跛腳，最後決定拿掉走路、移動一律用跑的",
      "圖片沒有腳部動作時，角色移動就會像在「飄」",
    ],
    gallery: ["assets/img/shepherd-spritesheet.jpg", "assets/img/shepherd-walk-strip.jpg"],
    code: "code/german_shepherd_v2.ino",
  },
  {
    id: "heart-hen-v3",
    title: "趕雞的心 v3：互動小劇場",
    date: "2026-10-23",
    status: "完成",
    cover: "assets/img/heart-hen-v3.gif",
    tags: ["動畫", "程式繪圖", "狀態機"],
    summary: "心拿樹枝追雞，四段劇情隨機輪播：追逐、跑出畫面探頭、心累了雞挑釁、雞反擊。",
    highlights: [
      "角色用程式即時畫出：腳交替、翅膀拍、會轉身、有影子",
      "場景狀態機：CHASE → RUN_OFF → EMPTY → HEN_PEEK → …",
      "粒子效果：羽毛飄落、汗滴",
    ],
    lessons: [
      "Arduino 會自動補函式宣告，自訂型別（typedef / struct / enum）要放在所有函式前面，不然會出現 has not been declared",
      "像素太少時腮紅反而像雜線，細節要敢刪",
    ],
    gallery: ["assets/img/heart-hen-original.jpg", "assets/img/heart-hen-v2.gif"],
    code: "code/heart_chase_hen_v3.ino",
    extraCode: [{ label: "WiFi 無線更新版", href: "code/heart_chase_hen_ota.ino" }],
  },
  {
    id: "heart-hen-v1",
    title: "趕雞的心 v1：圖片轉動畫",
    date: "2026-10-23",
    status: "封存",
    cover: "assets/img/heart-hen-v1.gif",
    tags: ["動畫", "點陣圖", "中文字"],
    summary: "把一張插畫切成「心」和「雞」兩塊點陣圖，加上跳動與移動做成動畫，底下「趕雞的心」四個字會波浪跳動。",
    highlights: ["用連通區塊自動把圖拆成兩個角色", "中文字直接轉成點陣圖，不用裝字型"],
    lessons: ["只有一張圖時，動作只能靠「移動和跳動」，腳不會動"],
    gallery: ["assets/img/heart-hen-original.jpg"],
    code: "code/heart_chase_hen.ino",
  },
  {
    id: "robot-face",
    title: "機器人表情（眼睛 + 嘴巴）",
    date: "2026-10-23",
    status: "完成",
    cover: "assets/img/robot-face.gif",
    tags: ["表情", "程式繪圖"],
    summary: "6 種表情平滑切換：一般、開心、生氣、難過、想睡、驚訝，眼睛會眨、會看來看去。",
    highlights: ["用「白色形狀 + 黑色挖空」做出各種眼型", "嘴巴用上下唇兩條曲線描述，參數一改就是新表情"],
    lessons: ["數值往目標慢慢靠近（approach / lerp），動畫就會很順"],
    gallery: ["assets/img/robot-face-moods.png"],
    code: "code/robot_face.ino",
  },
  {
    id: "robot-eyes",
    title: "機器人眼睛",
    date: "2026-10-23",
    status: "完成",
    cover: "assets/img/robot-eyes.gif",
    tags: ["表情", "第一個動畫"],
    summary: "第一個動畫作品：會眨眼、會亂看的機器人眼睛，按 BOOT 鍵切換表情。",
    highlights: ["隨機眨眼、隨機視線", "I2C 加速到 400kHz，動畫才順"],
    lessons: ["OLED 一次要整張畫面重畫（clearBuffer → 畫 → sendBuffer）"],
    gallery: [],
    code: "code/robot_eyes.ino",
  },
  {
    id: "shepherd-v1",
    title: "德國牧羊犬（程式繪圖版）",
    date: "2026-10-23",
    status: "失敗作",
    cover: "assets/img/shepherd-v1.gif",
    tags: ["程式繪圖", "失敗也是紀錄"],
    summary: "用程式把整隻狗畫出來，四隻腳照真實步態擺動。功能完整，但「很醜」。",
    highlights: ["8 種姿勢平滑變換", "四腳步態：對角步 / 奔馳"],
    lessons: ["程式畫圖適合幾何角色（機器人、愛心），寫實動物交給繪圖 AI 會好很多"],
    gallery: [],
    code: "code/german_shepherd.ino",
  },
];

/* ---------------- 學習地圖 ---------------- */
window.LESSONS = [
  {
    id: "hardware",
    no: "01",
    title: "認識手上的零件",
    level: "入門",
    body: `
<p>第一批買的材料：</p>
<ul>
  <li><b>ESP32E 開發板 30P（Type-C）</b>：大腦，有 WiFi 和藍牙。板上的 <code>BOOT</code>、<code>EN</code> 鍵分別是「燒錄模式」和「重新開機」。</li>
  <li><b>1.3 吋 OLED 128×64</b>：單色螢幕，驅動晶片通常是 <code>SH1106</code>（不是 SSD1306）。</li>
  <li><b>ESP32S 擴展板 38P</b>：<span class="warn">買錯了</span>，跟 30P 開發板腳位對不上。</li>
  <li><b>充電升壓模組（X0178-1）</b>：Type-C 充電、輸出可調 5～27V，<span class="warn">接 ESP32 前一定要先調到 5V</span>。</li>
  <li><b>3.7V 600mAh 軟包鋰電池（802530）</b>：不是 18650，體積小很多。</li>
  <li><b>杜邦線</b>：顏色只是方便辨認，功能都一樣；母對母用來接「針對針」。</li>
</ul>
<div class="figs">
  <figure><img src="assets/img/esp32e-board.jpg" alt="ESP32E 30P 開發板"><figcaption>ESP32E 30P：左排 3V3…D23、右排 VIN…EN</figcaption></figure>
  <figure><img src="assets/img/expansion-38p.jpg" alt="38P 擴展板"><figcaption>38P 擴展板：腳位排列和 30P 不同</figcaption></figure>
  <figure><img src="assets/img/battery-module.jpg" alt="電池與充電模組"><figcaption>600mAh 電池 + 充電升壓模組</figcaption></figure>
</div>
<p class="tip">💡 30P 和 38P 中間差了 GND、GPIO0 等腳位，就算塞得進去，GPIO19、GPIO13 會被接地，電源也接不到。<b>看腳位文字對不對得上，比看孔數重要。</b></p>`,
  },
  {
    id: "setup",
    no: "02",
    title: "開發環境：Arduino IDE + ESP32",
    level: "入門",
    body: `
<ol>
  <li>下載 <b>Arduino IDE 2.x</b>（Windows 選 Win 10 or newer 64-bit）。</li>
  <li>檔案 → 偏好設定 → 其他開發板管理員網址：<pre><code>https://espressif.github.io/arduino-esp32/package_esp32_index.json</code></pre></li>
  <li>開發板管理員搜尋 <code>esp32</code> → 安裝 <b>esp32 by Espressif Systems</b>。</li>
  <li>裝置管理員出現「USB Serial / 代碼 28」→ 安裝 <b>CH340 驅動（CH341SER.EXE，WCH 官網）</b>。</li>
  <li>選開發板 <b>ESP32 Dev Module</b>，連接埠選有 <b>(USB)</b> 的那個（我的是 COM5）。</li>
</ol>
<div class="figs"><figure><img src="assets/img/fix-ch340-code28.jpg" alt="代碼 28"><figcaption>抓到板子但缺驅動：代碼 28</figcaption></figure></div>
<p class="tip">💡 有些 ESP32 Type-C 板接 <b>C 對 C 線</b>不會供電，改用 A 對 C；只能充電的線也抓不到板子。</p>`,
  },
  {
    id: "blink",
    no: "03",
    title: "第一支程式：讓燈閃爍",
    level: "入門",
    body: `
<pre><code>void setup() {
  pinMode(2, OUTPUT);      // 板上藍色 LED 在 GPIO2
}

void loop() {
  digitalWrite(2, HIGH);   // 亮
  delay(500);
  digitalWrite(2, LOW);    // 滅
  delay(500);
}</code></pre>
<ul>
  <li><code>setup()</code> 開機只跑一次；<code>loop()</code> 無限重複。</li>
  <li>上傳卡在 <code>Connecting...</code> → 按住 <b>BOOT</b> 鍵直到開始跑百分比。</li>
  <li>程式存在快閃記憶體，拔電不會消失，下次一通電就自動執行。</li>
</ul>`,
  },
  {
    id: "oled",
    no: "04",
    title: "OLED 顯示文字（U8g2）",
    level: "入門",
    body: `
<table class="mini"><tr><th>OLED</th><th>ESP32</th><th>建議線色</th></tr>
<tr><td>VCC</td><td>3V3</td><td>紅</td></tr><tr><td>GND</td><td>GND</td><td>黑</td></tr>
<tr><td>SCL</td><td>D22</td><td>黃</td></tr><tr><td>SDA</td><td>D21</td><td>綠</td></tr></table>
<pre><code>#include &lt;U8g2lib.h&gt;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() { u8g2.begin(); }

void loop() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 20, "Hello L!");
  u8g2.sendBuffer();
}</code></pre>
<p class="tip">💡 畫面右側有一條雜點 → 你的螢幕是 SSD1306，把 <code>SH1106</code> 改成 <code>SSD1306</code>。杜邦線要插到底，接觸不良會閃爍或不亮。</p>`,
  },
  {
    id: "animation",
    no: "05",
    title: "動畫的基本功",
    level: "進階",
    body: `
<ul>
  <li><b>每一格整張重畫</b>：<code>clearBuffer()</code> → 畫 → <code>sendBuffer()</code>。</li>
  <li><b>I2C 加速</b>：<code>u8g2.setBusClock(400000);</code> 不然一張畫面要 100ms。</li>
  <li><b>慢慢靠近目標值</b>：<code>cur += (target - cur) * 0.2;</code> 表情、位置都用這招，看起來很順。</li>
  <li><b>狀態機（場景）</b>：用 <code>enum</code> 列出劇情，每段劇情有自己的計時與下一步。</li>
  <li><b>白色形狀 + 黑色挖空</b>：先畫實心，再用黑色蓋掉一部分，做出表情和外框。</li>
  <li><b>點陣圖（sprite）</b>：圖片轉成 <code>0x00, 0x04…</code> 陣列放進程式，用 <code>drawPixel</code> 畫出來，可以左右翻轉。</li>
</ul>
<p class="tip">⚠️ Arduino IDE 會自動在最前面補函式宣告，所以 <b>struct / enum / typedef 要放在所有函式之前</b>，否則會出現 <code>'InsideFn' has not been declared</code> 這類錯誤。</p>`,
  },
  {
    id: "ai-sprite",
    no: "06",
    title: "用 AI 畫角色，轉成 OLED 動畫",
    level: "進階",
    body: `
<ol>
  <li>先請繪圖 AI 定稿角色：<b>側面、面向右、白底、粗黑線、純黑白、不要灰色和陰影</b>。</li>
  <li>附上定稿圖，請它畫 <b>sprite sheet</b>（每列一個動作）。</li>
  <li>每格的腳要「寫清楚」：第 1 格哪隻腳往前、第 2 格收在身體下…，並要求<b>每格都不同</b>。</li>
  <li>Python 自動切格 → 縮小到約 43px 高 → 轉 1-bit → 輸出成陣列。</li>
  <li>維持<b>白底黑線</b>（整個畫面反轉），輪廓最清楚。</li>
</ol>
<div class="figs"><figure><img src="assets/img/shepherd-spritesheet.jpg" alt="sprite sheet"><figcaption>AI 畫的 sprite sheet</figcaption></figure></div>
<pre><code>IMPORTANT: Each frame will be shrunk to about 48 x 40 pixels for a 1-bit OLED.
Very thick outlines, large simple shapes, minimal interior lines,
eyes as solid dots, keep markings as simple solid shapes.</code></pre>
<p class="tip">💡 畫圖 AI 沒辦法精準照像素格子畫，重點是「線夠粗、細節夠少」。</p>`,
  },
  {
    id: "ota",
    no: "07",
    title: "WiFi 無線更新（OTA）",
    level: "進階",
    body: `
<ol>
  <li>程式填入<b>手機熱點</b>帳密（只支援 2.4GHz；iPhone 開「最大化相容性」）。</li>
  <li>第一次用 USB 上傳；之後「工具 → 連接埠」選網路連接埠（例如 <code>esp32-dog</code>）。</li>
  <li><b>每一支程式都要保留 OTA 程式碼</b>，否則只能再用 USB。</li>
</ol>
<pre><code>#include &lt;WiFi.h&gt;
#include &lt;ArduinoOTA.h&gt;
// setup():
WiFi.begin(WIFI_SSID, WIFI_PASS);
ArduinoOTA.setHostname("esp32-dog");
ArduinoOTA.setPassword("1234");
// 連上 WiFi 後：ArduinoOTA.begin();
// loop():      ArduinoOTA.handle();</code></pre>
<div class="figs">
  <figure><img src="assets/img/fix-private-network.jpg" alt="私人網路"><figcaption>電腦要把熱點設成「私人」網路</figcaption></figure>
  <figure><img src="assets/img/fix-ota-timeout.jpg" alt="OTA timeout"><figcaption>100% Done 後的 Timeout 警告可忽略</figcaption></figure>
</div>
<p class="tip">🔒 公開程式碼前，記得把 WiFi 名稱和密碼換掉。</p>`,
  },
  {
    id: "power",
    no: "08",
    title: "電池與安全",
    level: "注意",
    body: `
<ul>
  <li>升壓模組輸出可調到 27V，<b>先用三用電錶量好 5.0V</b> 再接 ESP32 的 VIN。</li>
  <li>模組充電電流 1A，對 600mAh 電池偏大；建議換 <b>1000mAh 以上、有保護板</b>的電池。</li>
  <li><b>不要用烙鐵直接焊鋰電池本體</b>；用電池座或附線電池。</li>
  <li>電池不能封死在黏土裡：要能拿出來、周圍墊泡棉、遠離發熱處。</li>
  <li>電源開關接在電池正極和模組 BAT+ 之間；充電時開關要打開。</li>
</ul>`,
  },
  {
    id: "network",
    no: "09",
    title: "上班要用：網路怎麼規劃",
    level: "規劃",
    body: `
<ul>
  <li>醫院／公司網路常有：帳號認證、裝置隔離、對外白名單 → <b>ESP32 不接公司網路</b>。</li>
  <li>改用<b>手機熱點 + 雲端資料</b>：手機改內容，ESP32 定時去抓。</li>
  <li>讓雲端把內容「畫成圖片」，ESP32 只負責顯示 → 中文排版沒問題，之後換彩色大螢幕也好改。</li>
  <li>便利貼內容不要放病人資料等敏感資訊。</li>
</ul>`,
  },
];

/* ---------------- 問題排除 ---------------- */
window.FIXES = [
  { q: "擴展板腳位文字對不上", cause: "擴展板是 38P，開發板是 30P", fix: "換 30P 擴展板，或不用擴展板直接用杜邦線接" },
  { q: "連接埠清單沒有新的 COM", cause: "C 對 C 線不供電、線只能充電、或缺 CH340 驅動", fix: "改用 A 對 C 傳輸線；裝置管理員看到代碼 28 就安裝 CH341SER.EXE" },
  { q: "開發板清單找不到 ESP32 Dev Module", cause: "ESP32 開發板套件還沒裝好", fix: "開發板管理員安裝 esp32 by Espressif Systems，等出現「已安裝」" },
  { q: "上傳卡在 Connecting......", cause: "板子沒進入燒錄模式", fix: "按住 BOOT 鍵直到開始跑百分比再放開" },
  { q: "OLED 右側一條雜點", cause: "螢幕晶片是 SSD1306 不是 SH1106", fix: "程式裡的 SH1106 改成 SSD1306" },
  { q: "'InsideFn' has not been declared", cause: "Arduino 自動產生的函式宣告跑到型別定義前面", fix: "把 typedef / struct / enum 全部移到程式最上面" },
  { q: "OTA：Authenticating OK 後 No response from device", cause: "Windows 防火牆擋住 ESP32 回連電腦", fix: "熱點網路設成「私人」，或防火牆允許 espota.exe" },
  { q: "OTA：invalid int value '{upload.port.properties.port}'", cause: "IDE 的網路連接埠資料過期", fix: "工具 → 連接埠 重新選一次，或重開 Arduino IDE" },
  { q: "OTA：找不到網路連接埠", cause: "程式裡 WiFi 帳密是預設值，ESP32 根本沒連上", fix: "填好熱點帳密，再用 USB 上傳一次" },
  { q: "OTA：100% Done 後一直 Timeout", cause: "ESP32 重開機太快，沒回傳完成訊號", fix: "螢幕已更新就按 CANCEL，可忽略" },
];

/* ---------------- 零件清單 ---------------- */
window.PARTS = [
  { name: "ESP32E 開發板 30P Type-C", spec: "ESP-32E、CH340", price: 315, status: "已有" },
  { name: "1.3 吋 OLED 128×64", spec: "I2C、SH1106、白色", price: null, status: "已有" },
  { name: "ESP32S 擴展板 38PIN", spec: "X0194-1", price: 80, status: "不相容" },
  { name: "鋰電池充電升壓模組", spec: "X0178-1、Type-C、輸出 5～27V", price: 35, status: "已有" },
  { name: "3.7V 軟包鋰電池", spec: "802530、600mAh", price: null, status: "已有" },
  { name: "杜邦線", spec: "公對公、母對母", price: null, status: "已有" },
  { name: "三用電錶", spec: "數位式，DC 電壓檔", price: null, status: "待買" },
  { name: "軟包鋰電池（有保護板）", spec: "1000～2000mAh，例如 103450", price: null, status: "待買" },
  { name: "小型滑動開關", spec: "SS12D00", price: null, status: "待買" },
  { name: "矽膠線、熱縮套管", spec: "26～28 AWG", price: null, status: "待買" },
  { name: "ESP32-C3 SuperMini", spec: "無線小鍵盤用", price: null, status: "規劃中" },
  { name: "機械軸 + 透明可換卡鍵帽 + EC11 旋鈕", spec: "無線小鍵盤用", price: null, status: "規劃中" },
  { name: "彩色大螢幕 / 電子紙", spec: "ESP32-S3 一體板 或 4.2～7.5 吋電子紙", price: null, status: "規劃中" },
];

/* ---------------- 下一步 ---------------- */
window.ROADMAP = [
  { id: "r1", group: "硬體", text: "買三用電錶，把升壓模組調到 5.0V" },
  { id: "r2", group: "硬體", text: "換 1000mAh 以上有保護板的電池，接上開關" },
  { id: "r3", group: "公仔", text: "決定造型（雪納瑞 / 德國牧羊犬），做輕黏土中空外殼" },
  { id: "r4", group: "公仔", text: "改用短線焊接，把零件裝進公仔" },
  { id: "r5", group: "便利貼螢幕", text: "雲端存便利貼 + 手機編輯網頁" },
  { id: "r6", group: "便利貼螢幕", text: "ESP32 透過手機熱點定時抓內容並顯示" },
  { id: "r7", group: "便利貼螢幕", text: "加入 AI 新聞、單字卡、倒數，和狗狗動畫輪流" },
  { id: "r8", group: "無線小鍵盤", text: "ESP32-C3 + 機械軸 + 旋鈕，用 ESP-NOW 控制螢幕" },
  { id: "r9", group: "無線小鍵盤", text: "用 OpenSCAD 產生外殼 STL，找代印" },
  { id: "r10", group: "升級", text: "買彩色大螢幕或電子紙，換掉畫面輸出層" },
];

/* ---------------- 預設學習日誌（之後自己新增的會存在瀏覽器） ---------------- */
window.BASE_JOURNAL = [
  { id: "b1", date: "2026-10-23", title: "開箱：材料到齊，也買錯了一片", tags: ["硬體"], mood: "興奮",
    body: "ESP32E 30P、1.3 吋 OLED、充電升壓模組、杜邦線。照著擴展板的字對腳位才發現：擴展板是 38P，跟 30P 開發板對不上。\n**學到：買擴展板要先確認腳數和腳位排列。**" },
  { id: "b2", date: "2026-10-23", title: "環境建置：卡在驅動", tags: ["開發環境"], mood: "卡關",
    body: "裝好 Arduino IDE 和 ESP32 套件後，COM3、COM4 都不是板子。裝置管理員顯示 USB Serial 代碼 28 → 裝 CH340 驅動後出現 COM5 (USB)。" },
  { id: "b3", date: "2026-10-23", title: "第一次成功：燈會閃、OLED 會顯示字", tags: ["里程碑"], mood: "成就感",
    body: "Blink 程式讓板子上的藍燈閃起來，接著用 U8g2 在 OLED 顯示 `Hello L!`，秒數一直往上加。" },
  { id: "b4", date: "2026-10-23", title: "會動的表情", tags: ["作品", "動畫"], mood: "好玩",
    body: "做了機器人眼睛，再加上嘴巴，6 種表情平滑切換。開始思考要做成公仔，用輕黏土當外殼。" },
  { id: "b5", date: "2026-10-23", title: "電池其實不是 18650", tags: ["硬體", "安全"], mood: "學到了",
    body: "拍照確認後發現手上是 600mAh 軟包鋰電池，模組輸出還可以調到 27V。**接 ESP32 前一定要先量好 5V。**" },
  { id: "b6", date: "2026-10-23", title: "趕雞的心：從圖片到小劇場", tags: ["作品", "動畫"], mood: "好玩",
    body: "先把插畫轉成點陣圖動畫，再改成程式即時繪製，加入跑出畫面、探頭、累了、反擊等劇情。中途遇到 `InsideFn has not been declared`，原來是 Arduino 自動宣告的順序問題。" },
  { id: "b7", date: "2026-10-23", title: "WiFi 無線更新成功", tags: ["里程碑", "WiFi"], mood: "成就感",
    body: "用手機熱點讓 ESP32 和電腦在同一個網路。驗證成功卻 No response → Windows 防火牆；把熱點設成私人網路後就能無線上傳了。" },
  { id: "b8", date: "2026-10-23", title: "德國牧羊犬：程式畫不好看，交給 AI 畫", tags: ["作品", "AI 繪圖"], mood: "反覆修改",
    body: "程式畫的狗很醜 → 請繪圖 AI 畫 sprite sheet → 轉成點陣圖。負片很怪，改成白底黑線就像原圖了。走路一直不自然，最後決定拿掉走路，移動都用跑的。" },
  { id: "b9", date: "2026-10-23", title: "下一步：便利貼螢幕與無線小鍵盤", tags: ["規劃"], mood: "期待",
    body: "上班想放一個桌面小螢幕顯示待辦和 AI 新聞，走手機熱點避開公司網路。之後想買彩色大螢幕，搭配放在手邊的無線機械鍵小鍵盤。" },
];
