# ESP32 自造筆記

從零開始學 ESP32 + OLED 的學習網站：學習地圖、作品展示、學習日誌、問題排除、零件清單、下一步。
純靜態網站（HTML / CSS / JS），不需要伺服器、資料庫，可以免費部署。

## 資料夾結構

```
index.html          網站入口
style.css           外觀
app.js              網站程式
data/content.js     作品、學習單元、問題排除、零件、下一步、預設日誌（想改內容改這裡）
data/my-journal.js  你的學習日誌（永久保存版）
assets/img/         動畫 GIF 與照片
code/               各作品的 Arduino 程式（WiFi 帳密已換成預設文字）
```

## 部署到免費平台

### 方法 A：GitHub Pages（推薦，之後改內容最方便）
1. 到 github.com 新增一個 repository，例如 `esp32-notes`（設為 Public）。
2. 按「uploading an existing file」，把這個資料夾**裡面的所有檔案**拖進去，Commit。
3. Settings → Pages → Source 選 `Deploy from a branch`，Branch 選 `main` / `(root)`，Save。
4. 約 1 分鐘後網址會是 `https://你的帳號.github.io/esp32-notes/`。

### 方法 B：Netlify Drop（最快，免註冊也能先試）
1. 打開 https://app.netlify.com/drop
2. 把整個資料夾拖進去，馬上得到網址。註冊後可以保留並改名稱。

### 方法 C：Vercel / Cloudflare Pages
建立新專案、上傳資料夾即可，不需要任何建置設定（Framework 選 Other / None）。

## 學習日誌怎麼保存

- 在「學習日誌」頁新增的紀錄，會先存在**這台電腦的瀏覽器**（換電腦或清除瀏覽資料就看不到）。
- 想讓所有裝置、所有人都看得到：
  1. 按「⬇ 匯出成網站檔案」，會下載 `my-journal.js`
  2. 用它取代 `data/my-journal.js`
     - GitHub：進到 repo 的 `data` 資料夾 → Add file → Upload files → 拖進去覆蓋 → Commit
     - Netlify：重新拖整個資料夾
  3. 網站更新後，日誌就永久保存在網站上了
- 「備份 JSON」「匯入 JSON」用來在不同電腦之間搬移還沒發布的紀錄。
- 附圖會自動壓縮，但存太多張圖可能超過瀏覽器空間，建議定期匯出。

## 新增作品

1. 把動畫 GIF 放進 `assets/img/`，程式放進 `code/`
2. 打開 `data/content.js`，在 `window.WORKS` 最前面複製一筆，改標題、圖片路徑、程式路徑

## 注意

- 網站是公開的：程式碼裡的 **WiFi 名稱和密碼**已經換成預設文字，之後上傳新程式前也要記得換掉。
- 日誌不要寫工作上的敏感資訊。
