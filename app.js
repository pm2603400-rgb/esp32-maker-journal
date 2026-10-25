/* =====================================================================
   ESP32 自造筆記 — 網站程式
   頁面：首頁、學習地圖、作品展示、學習日誌、問題排除、零件清單、下一步
   自己新增的日誌、勾選進度存在瀏覽器 localStorage；可匯出成網站檔案永久保存
   ===================================================================== */
(function () {
  "use strict";

  const app = document.getElementById("app");
  const KEY_JOURNAL = "esp32notes.journal";
  const KEY_LESSON = "esp32notes.lessons";
  const KEY_ROAD = "esp32notes.roadmap";
  const KEY_THEME = "esp32notes.theme";

  // ---------- 小工具 ----------
  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
  const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  const today = () => {
    const d = new Date();
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
  };
  function load(key, fallback) {
    try { const v = localStorage.getItem(key); return v ? JSON.parse(v) : fallback; } catch (e) { return fallback; }
  }
  function save(key, val) {
    try { localStorage.setItem(key, JSON.stringify(val)); return true; }
    catch (e) { toast("瀏覽器空間不足或無法儲存，請先匯出備份"); return false; }
  }
  function toast(msg) {
    const t = document.createElement("div");
    t.className = "toast"; t.textContent = msg;
    document.body.appendChild(t);
    setTimeout(() => t.remove(), 2400);
  }
  function download(name, text, type = "application/json") {
    const blob = new Blob([text], { type });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob); a.download = name;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  }
  // 簡易 Markdown：**粗體**、`程式碼`、換行、空行分段
  function md(text) {
    return esc(text)
      .split(/\n{2,}/)
      .map((p) => "<p>" + p
        .replace(/\*\*(.+?)\*\*/g, "<b>$1</b>")
        .replace(/`(.+?)`/g, "<code>$1</code>")
        .replace(/\n/g, "<br>") + "</p>")
      .join("");
  }

  // ---------- 主題 ----------
  const root = document.documentElement;
  const savedTheme = load(KEY_THEME, null);
  if (savedTheme) root.dataset.theme = savedTheme;
  $("#themeBtn").addEventListener("click", () => {
    const dark = root.dataset.theme ? root.dataset.theme === "dark" : matchMedia("(prefers-color-scheme: dark)").matches;
    root.dataset.theme = dark ? "light" : "dark";
    save(KEY_THEME, root.dataset.theme);
  });
  $("#menuBtn").addEventListener("click", () => {
    const nav = $("#nav"); nav.classList.toggle("open");
    $("#menuBtn").setAttribute("aria-expanded", nav.classList.contains("open"));
  });

  // ---------- 彈出視窗 ----------
  const modal = $("#modal");
  function openModal(html) { $("#modalBody").innerHTML = html; modal.hidden = false; document.body.style.overflow = "hidden"; }
  function closeModal() { modal.hidden = true; document.body.style.overflow = ""; }
  modal.addEventListener("click", (e) => { if (e.target.hasAttribute("data-close")) closeModal(); });
  document.addEventListener("keydown", (e) => { if (e.key === "Escape") closeModal(); });
  // 點圖片放大
  document.addEventListener("click", (e) => {
    const img = e.target.closest(".figs img, .entry img, .zoomable");
    if (img && !modal.contains(img)) openModal(`<div class="zoom"><img src="${img.getAttribute("src")}" alt=""></div>`);
  });

  // ---------- 資料 ----------
  const SITE = window.SITE || {};
  const WORKS = window.WORKS || [];
  const LESSONS = window.LESSONS || [];
  const FIXES = window.FIXES || [];
  const PARTS = window.PARTS || [];
  const ROADMAP = window.ROADMAP || [];
  const BASE = (window.BASE_JOURNAL || []).map((e, i) => ({ ...e, order: i, source: "base" }));
  const PUBLISHED = (window.MY_JOURNAL || []).map((e) => ({ ...e, source: "published" }));
  let mine = load(KEY_JOURNAL, []);

  function allEntries() {
    // 同一個 id 以「瀏覽器裡的版本」為準（方便修改已發布的紀錄）
    const map = new Map();
    [...BASE, ...PUBLISHED].forEach((e) => map.set(e.id, e));
    mine.forEach((e) => map.set(e.id, { ...e, source: "mine" }));
    return [...map.values()].sort((a, b) => (b.date + (b.createdAt || "")).localeCompare(a.date + (a.createdAt || "")) || (b.order || 0) - (a.order || 0));
  }

  $("#footText").textContent = `© ${new Date().getFullYear()} ${SITE.author || ""} · ${SITE.title || ""}`;

  // =====================================================================
  //  頁面
  // =====================================================================
  const pages = {
    home() {
      const lessonDone = load(KEY_LESSON, {});
      const doneCount = LESSONS.filter((l) => lessonDone[l.id]).length;
      const feat = WORKS.find((w) => w.featured) || WORKS[0];
      const days = Math.max(1, Math.floor((new Date(today() + "T00:00:00") - new Date(SITE.startDate + "T00:00:00")) / 86400000) + 1);
      return `
<section class="hero">
  <div>
    <h1>${esc(SITE.title).replace("ESP32", "<span>ESP32</span>")}</h1>
    <p>${esc(SITE.subtitle)}。從買錯擴展板、裝驅動、第一次讓燈閃爍，到用 AI 繪圖做出會撿球的牧羊犬，這裡記錄每一步。</p>
    <div class="hero-actions">
      <a class="btn primary" href="#journal">✎ 寫今天的學習日誌</a>
      <a class="btn" href="#works">看作品</a>
      <a class="btn" href="#learn">從頭學起</a>
    </div>
  </div>
  <div class="oled-frame">
    <img src="${esc(feat.cover)}" alt="${esc(feat.title)} 動畫">
    <div class="pins"><i></i><i></i><i></i><i></i></div>
  </div>
</section>
<section class="stats">
  <div class="stat"><b>${days}</b><span>天（從 ${esc(SITE.startDate)} 開始）</span></div>
  <div class="stat"><b>${WORKS.length}</b><span>件作品</span></div>
  <div class="stat"><b>${doneCount}/${LESSONS.length}</b><span>學習單元完成</span></div>
  <div class="stat"><b>${allEntries().length}</b><span>篇學習日誌</span></div>
</section>
<h2 class="sec">最新作品 <small>點圖片看詳細</small></h2>
<div class="grid">${WORKS.slice(0, 3).map(workCard).join("")}</div>
<h2 class="sec" style="margin-top:36px">最近的日誌</h2>
<div class="timeline">${allEntries().slice(0, 3).map(entryHTML).join("")}</div>
<p><a href="#journal">看全部日誌 →</a></p>`;
    },

    learn() {
      const done = load(KEY_LESSON, {});
      const n = LESSONS.filter((l) => done[l.id]).length;
      return `
<h2 class="sec">學習地圖 <small>${n}/${LESSONS.length} 完成</small></h2>
<div class="progress"><i style="width:${(n / LESSONS.length) * 100}%"></i></div>
<div class="lessons">${LESSONS.map((l) => `
  <details class="lesson" ${location.hash.includes(l.id) ? "open" : ""}>
    <summary>
      <span class="no">${esc(l.no)}</span>
      <span class="ttl">${esc(l.title)}</span>
      <span class="tag">${esc(l.level)}</span>
      <label class="done-box" onclick="event.stopPropagation()"><input type="checkbox" data-lesson="${esc(l.id)}" ${done[l.id] ? "checked" : ""}> 學會了</label>
    </summary>
    <div class="content">${l.body}</div>
  </details>`).join("")}
</div>`;
    },

    works() {
      const tags = [...new Set(WORKS.flatMap((w) => w.tags))];
      return `
<h2 class="sec">作品展示 <small>${WORKS.length} 件，失敗作也是紀錄</small></h2>
<div class="chips" id="workChips"><button class="chip on" data-tag="">全部</button>${tags.map((t) => `<button class="chip" data-tag="${esc(t)}">${esc(t)}</button>`).join("")}</div>
<div class="grid" id="workGrid">${WORKS.map((w, i) => workCard(w, i === 0 && w.featured)).join("")}</div>`;
    },

    journal() {
      return `
<h2 class="sec">學習日誌 <small>記錄心路歷程</small></h2>
<p class="lead">新增的紀錄會存在這台電腦的瀏覽器。想讓所有裝置都看得到，請用「匯出成網站檔案」後重新部署。</p>
<div class="journal-layout">
  <div class="panel form" id="jForm">
    <h3 id="formTitle">新增一篇</h3>
    <input type="hidden" id="jId">
    <div class="row">
      <div><label for="jDate">日期</label><input type="date" id="jDate" value="${today()}"></div>
      <div><label for="jMood">心情</label>
        <select id="jMood"><option>成就感</option><option>好玩</option><option>學到了</option><option>卡關</option><option>反覆修改</option><option>期待</option><option>累了</option></select>
      </div>
    </div>
    <label for="jTitle">標題</label><input id="jTitle" placeholder="例如：OTA 終於成功了">
    <label for="jTags">標籤（用逗號分隔）</label><input id="jTags" placeholder="WiFi, 里程碑">
    <label for="jBody">內容（支援 **粗體**、\`程式碼\`）</label>
    <textarea id="jBody" placeholder="今天做了什麼？卡在哪裡？怎麼解決？下一步？"></textarea>
    <label for="jImg">附圖（選填，會自動壓縮）</label><input type="file" id="jImg" accept="image/*">
    <img id="jPreview" class="preview-img" hidden alt="">
    <div class="actions">
      <button class="btn primary" id="jSave">儲存</button>
      <button class="btn" id="jCancel" hidden>取消編輯</button>
    </div>
    <div class="tools">
      <button class="btn small" id="jExportSite" title="下載 my-journal.js，取代 data 資料夾裡的同名檔案後重新部署">⬇ 匯出成網站檔案</button>
      <button class="btn small" id="jExport">⬇ 備份 JSON</button>
      <label class="btn small" for="jImport">⬆ 匯入 JSON</label><input type="file" id="jImport" accept=".json,application/json" hidden>
    </div>
  </div>
  <div>
    <input class="search" id="jSearch" placeholder="搜尋標題或內容…" style="margin-bottom:12px">
    <div class="chips" id="jChips"></div>
    <div class="timeline" id="jList"></div>
  </div>
</div>`;
    },

    fix() {
      return `
<h2 class="sec">問題排除 <small>我踩過的坑</small></h2>
${FIXES.map((f) => `
<details class="fix"><summary>${esc(f.q)}</summary>
  <div class="fx"><div><b>原因</b>${esc(f.cause)}</div><div><b>解法</b>${esc(f.fix)}</div></div>
</details>`).join("")}`;
    },

    parts() {
      const total = PARTS.filter((p) => p.price && p.status !== "規劃中").reduce((s, p) => s + p.price, 0);
      return `
<h2 class="sec">零件清單 <small>已記錄花費 NT$${total}</small></h2>
<div class="figs" style="max-width:460px"><figure><img src="assets/img/labels.jpg" alt="商品標籤"><figcaption>購買時的標籤：擴展板 38PIN vs 開發板 30P</figcaption></figure></div>
<div class="table-wrap"><table class="parts">
  <thead><tr><th>品項</th><th>規格</th><th>價格</th><th>狀態</th></tr></thead>
  <tbody>${PARTS.map((p) => `<tr><td>${esc(p.name)}</td><td class="muted">${esc(p.spec)}</td><td>${p.price ? "NT$" + p.price : "—"}</td><td><span class="pill ${esc(p.status)}">${esc(p.status)}</span></td></tr>`).join("")}</tbody>
</table></div>`;
    },

    next() {
      const saved = load(KEY_ROAD, {});
      const done = {};
      ROADMAP.forEach((r) => { done[r.id] = r.id in saved ? saved[r.id] : !!r.done; });
      const groups = [...new Set(ROADMAP.map((r) => r.group))];
      const n = ROADMAP.filter((r) => done[r.id]).length;
      return `
<h2 class="sec">下一步 <small>${n}/${ROADMAP.length} 完成</small></h2>
<div class="progress"><i style="width:${(n / ROADMAP.length) * 100}%"></i></div>
<div class="road">${groups.map((g) => `
  <div class="panel"><h3>${esc(g)}</h3>
    ${ROADMAP.filter((r) => r.group === g).map((r) => `
      <label class="check ${done[r.id] ? "done" : ""}"><input type="checkbox" data-road="${esc(r.id)}" ${done[r.id] ? "checked" : ""}><span>${esc(r.text)}</span></label>`).join("")}
  </div>`).join("")}
</div>`;
    },
  };

  // ---------- 元件 ----------
  function workCard(w, feature) {
    return `
<article class="card ${feature ? "feature" : ""}" data-tags="${esc(w.tags.join("|"))}">
  <div class="shot" data-work="${esc(w.id)}"><img src="${esc(w.cover)}" alt="${esc(w.title)}" loading="lazy"></div>
  <div class="body">
    <h3>${esc(w.title)} <span class="status ${esc(w.status)}">${esc(w.status)}</span></h3>
    <div>${w.tags.map((t) => `<span class="tag">${esc(t)}</span>`).join("")}</div>
    <p>${esc(w.summary)}</p>
    <div class="foot">
      <button class="btn small" data-work="${esc(w.id)}">詳細與心得</button>
      <a class="btn small" href="${esc(w.code)}" download>⬇ 程式碼</a>
    </div>
  </div>
</article>`;
  }

  function workDetail(w) {
    return `
<h2>${esc(w.title)} <span class="status ${esc(w.status)}">${esc(w.status)}</span></h2>
<div class="muted">${esc(w.date)} · ${w.tags.map(esc).join("、")}</div>
<div class="bigshot"><img src="${esc(w.cover)}" alt=""></div>
<p>${esc(w.summary)}</p>
<div class="two-col">
  <div><h3>做了什麼</h3><ul>${w.highlights.map((h) => `<li>${esc(h)}</li>`).join("")}</ul></div>
  <div><h3>學到什麼</h3><ul>${w.lessons.map((h) => `<li>${esc(h)}</li>`).join("")}</ul></div>
</div>
${w.gallery && w.gallery.length ? `<h3>相關圖片</h3><div class="figs">${w.gallery.map((g) => `<figure><img src="${esc(g)}" alt=""></figure>`).join("")}</div>` : ""}
<div class="foot" style="display:flex;gap:8px;flex-wrap:wrap;margin-top:10px">
  <a class="btn primary" href="${esc(w.code)}" download>⬇ 下載程式碼</a>
  ${(w.extraCode || []).map((c) => `<a class="btn" href="${esc(c.href)}" download>⬇ ${esc(c.label)}</a>`).join("")}
</div>`;
  }

  function entryHTML(e) {
    const editable = e.source === "mine";
    return `
<article class="entry ${editable ? "mine" : ""}" data-id="${esc(e.id)}">
  <div class="meta"><span>${esc(e.date)}</span>${e.mood ? `<span class="mood">${esc(e.mood)}</span>` : ""}${(e.tags || []).map((t) => `<span class="tag">${esc(t)}</span>`).join("")}</div>
  <h4>${esc(e.title)}</h4>
  <div class="text">${md(e.body)}</div>
  ${(e.images && e.images.length ? e.images : (e.image ? [e.image] : [])).map((src) => `<img src="${esc(src)}" alt="" loading="lazy">`).join("")}
  ${(e.videos || []).map((v) => `<p><a href="${esc(v)}" target="_blank" rel="noopener">▶ 觀看影片</a></p>`).join("")}
  ${e.issue ? `<div class="meta" style="margin-top:6px"><a href="${esc(e.issue)}" target="_blank" rel="noopener">來自 GitHub Issue</a></div>` : ""}
  ${editable || e.source === "base" || e.source === "published" ? `<div class="ops">
    <button class="btn small" data-edit="${esc(e.id)}">編輯</button>
    ${editable ? `<button class="btn small danger" data-del="${esc(e.id)}">刪除</button>` : ""}
  </div>` : ""}
</article>`;
  }

  // =====================================================================
  //  路由與事件
  // =====================================================================
  function route() {
    const name = (location.hash.replace("#", "").split("/")[0]) || "home";
    const page = pages[name] ? name : "home";
    app.innerHTML = pages[page]();
    $$(".nav a").forEach((a) => a.classList.toggle("active", a.getAttribute("href") === "#" + page));
    $("#nav").classList.remove("open");
    if (page === "journal") initJournal();
    window.scrollTo(0, 0);
  }

  app.addEventListener("click", (e) => {
    const w = e.target.closest("[data-work]");
    if (w) { const work = WORKS.find((x) => x.id === w.dataset.work); if (work) openModal(workDetail(work)); return; }
    const chip = e.target.closest("#workChips .chip");
    if (chip) {
      $$("#workChips .chip").forEach((c) => c.classList.toggle("on", c === chip));
      const t = chip.dataset.tag;
      $$("#workGrid .card").forEach((c) => { c.style.display = !t || c.dataset.tags.split("|").includes(t) ? "" : "none"; });
    }
  });

  app.addEventListener("change", (e) => {
    if (e.target.matches("[data-lesson]")) {
      const done = load(KEY_LESSON, {}); done[e.target.dataset.lesson] = e.target.checked; save(KEY_LESSON, done);
      const n = LESSONS.filter((l) => done[l.id]).length;
      $(".sec small").textContent = `${n}/${LESSONS.length} 完成`;
      $(".progress i").style.width = (n / LESSONS.length) * 100 + "%";
    }
    if (e.target.matches("[data-road]")) {
      const done = load(KEY_ROAD, {}); done[e.target.dataset.road] = e.target.checked; save(KEY_ROAD, done);
      e.target.closest(".check").classList.toggle("done", e.target.checked);
      const n = ROADMAP.filter((r) => done[r.id]).length;
      $(".sec small").textContent = `${n}/${ROADMAP.length} 完成`;
      $(".progress i").style.width = (n / ROADMAP.length) * 100 + "%";
    }
  });

  // ---------- 學習日誌 ----------
  let filterTag = "";
  let pendingImage = null;

  function renderJournal() {
    const q = ($("#jSearch").value || "").trim().toLowerCase();
    const list = allEntries();
    const tags = [...new Set(list.flatMap((e) => e.tags || []))];
    $("#jChips").innerHTML = `<button class="chip ${filterTag ? "" : "on"}" data-jtag="">全部</button>` +
      tags.map((t) => `<button class="chip ${filterTag === t ? "on" : ""}" data-jtag="${esc(t)}">${esc(t)}</button>`).join("");
    const shown = list.filter((e) => (!filterTag || (e.tags || []).includes(filterTag)) &&
      (!q || (e.title + " " + e.body).toLowerCase().includes(q)));
    $("#jList").innerHTML = shown.length ? shown.map(entryHTML).join("") : `<div class="empty">沒有符合的紀錄</div>`;
  }

  function resetForm() {
    $("#jId").value = ""; $("#jDate").value = today(); $("#jTitle").value = ""; $("#jTags").value = "";
    $("#jBody").value = ""; $("#jImg").value = ""; $("#jPreview").hidden = true; pendingImage = null;
    $("#formTitle").textContent = "新增一篇"; $("#jCancel").hidden = true;
  }

  function compressImage(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => {
        const img = new Image();
        img.onload = () => {
          const max = 1000, scale = Math.min(1, max / Math.max(img.width, img.height));
          const c = document.createElement("canvas");
          c.width = Math.round(img.width * scale); c.height = Math.round(img.height * scale);
          c.getContext("2d").drawImage(img, 0, 0, c.width, c.height);
          resolve(c.toDataURL("image/jpeg", 0.72));
        };
        img.onerror = reject; img.src = reader.result;
      };
      reader.onerror = reject; reader.readAsDataURL(file);
    });
  }

  function initJournal() {
    filterTag = "";
    renderJournal();
    $("#jSearch").addEventListener("input", renderJournal);
    $("#jChips").addEventListener("click", (e) => {
      const c = e.target.closest("[data-jtag]"); if (!c) return;
      filterTag = c.dataset.jtag; renderJournal();
    });
    $("#jImg").addEventListener("change", async (e) => {
      const f = e.target.files[0]; if (!f) return;
      try { pendingImage = await compressImage(f); $("#jPreview").src = pendingImage; $("#jPreview").hidden = false; }
      catch (err) { toast("圖片讀取失敗"); }
    });
    $("#jSave").addEventListener("click", () => {
      const title = $("#jTitle").value.trim(), body = $("#jBody").value.trim();
      if (!title || !body) { toast("請填寫標題和內容"); return; }
      const id = $("#jId").value || "m" + Date.now();
      const old = allEntries().find((x) => x.id === id);
      const entry = {
        id, date: $("#jDate").value || today(), title, body, mood: $("#jMood").value,
        tags: $("#jTags").value.split(/[,，、]/).map((s) => s.trim()).filter(Boolean),
        image: pendingImage || (old && old.image) || "",
        createdAt: (old && old.createdAt) || new Date().toISOString(),
      };
      const next = mine.filter((x) => x.id !== id); next.push(entry);
      if (save(KEY_JOURNAL, next)) { mine = next; resetForm(); renderJournal(); toast("已儲存"); }
    });
    $("#jCancel").addEventListener("click", resetForm);
    $("#jList").addEventListener("click", (e) => {
      const ed = e.target.closest("[data-edit]");
      if (ed) {
        const en = allEntries().find((x) => x.id === ed.dataset.edit); if (!en) return;
        $("#jId").value = en.id; $("#jDate").value = en.date; $("#jTitle").value = en.title;
        $("#jTags").value = (en.tags || []).join(", "); $("#jBody").value = en.body; $("#jMood").value = en.mood || "成就感";
        pendingImage = null; $("#jPreview").hidden = !en.image; if (en.image) $("#jPreview").src = en.image;
        $("#formTitle").textContent = "編輯紀錄"; $("#jCancel").hidden = false;
        $("#jForm").scrollIntoView({ behavior: "smooth", block: "start" });
      }
      const del = e.target.closest("[data-del]");
      if (del && confirm("確定刪除這篇紀錄？")) {
        mine = mine.filter((x) => x.id !== del.dataset.del); save(KEY_JOURNAL, mine); renderJournal(); toast("已刪除");
      }
    });
    $("#jExport").addEventListener("click", () => download(`esp32-journal-backup-${today()}.json`, JSON.stringify(mine, null, 2)));
    $("#jExportSite").addEventListener("click", () => {
      // 已發布的 + 瀏覽器裡的，合併成新的 my-journal.js
      const map = new Map();
      PUBLISHED.forEach((e) => { const { source, ...rest } = e; map.set(e.id, rest); });
      mine.forEach((e) => map.set(e.id, e));
      const text = "/* 你的學習日誌（由網站匯出，取代 data/my-journal.js 後重新部署） */\nwindow.MY_JOURNAL = " +
        JSON.stringify([...map.values()], null, 2) + ";\n";
      download("my-journal.js", text, "text/javascript");
      toast("已下載 my-journal.js");
    });
    $("#jImport").addEventListener("change", async (e) => {
      const f = e.target.files[0]; if (!f) return;
      try {
        const data = JSON.parse(await f.text());
        if (!Array.isArray(data)) throw new Error("格式不對");
        const map = new Map(mine.map((x) => [x.id, x]));
        data.forEach((x) => { if (x && x.id && x.title) map.set(x.id, x); });
        mine = [...map.values()]; save(KEY_JOURNAL, mine); renderJournal(); toast(`已匯入 ${data.length} 篇`);
      } catch (err) { toast("匯入失敗：檔案格式不正確"); }
      e.target.value = "";
    });
  }

  window.addEventListener("hashchange", route);
  route();
})();
