(function () {
  const PAGE_SIZE = 6;

  const state = {
    allItems: [],
    filtered: [],
    page: 1,
    query: "",
    tag: "",
    category: "",
    sort: "newest",
  };

  const el = {
    search: document.getElementById("searchInput"),
    tag: document.getElementById("tagFilter"),
    category: document.getElementById("categoryFilter"),
    sort: document.getElementById("sortFilter"),
    clear: document.getElementById("clearFilters"),
    summary: document.getElementById("resultsSummary"),
    results: document.getElementById("postResults"),
    empty: document.getElementById("emptyState"),
    prev: document.getElementById("prevPage"),
    next: document.getElementById("nextPage"),
    pageInfo: document.getElementById("pageInfo"),
  };

  function escapeHtml(value) {
    return String(value)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll('"', "&quot;")
      .replaceAll("'", "&#39;");
  }

  function normalize(value) {
    return String(value || "").trim().toLowerCase();
  }

  function csvToList(value) {
    return String(value || "")
      .split(",")
      .map((part) => part.trim())
      .filter(Boolean);
  }

  function parseIntOrNull(value) {
    const raw = String(value || "").trim();
    if (!raw) {
      return null;
    }
    const parsed = Number.parseInt(raw, 10);
    return Number.isFinite(parsed) ? parsed : null;
  }

  function normalizeRecord(record) {
    const meta = record && typeof record.meta === "object" ? record.meta : {};
    return {
      kind: normalize(meta.kind),
      url: String(record?.url || ""),
      title: String(meta.title || ""),
      summary: String(meta.summary || ""),
      image: String(meta.image || ""),
      tags: csvToList(meta.tags),
      category: String(meta.category || ""),
      time_min: parseIntOrNull(meta["time-min"]),
      published: String(meta.published || ""),
    };
  }

  function parseQueryParams() {
    const params = new URLSearchParams(window.location.search);
    state.query = params.get("q") || "";
    state.tag = params.get("tag") || "";
    state.category = params.get("category") || "";
    state.sort = params.get("sort") || "newest";
    state.page = Math.max(parseInt(params.get("page") || "1", 10) || 1, 1);

    el.search.value = state.query;
    el.sort.value = state.sort;
  }

  function updateQueryParams() {
    const params = new URLSearchParams();
    if (state.query) {
      params.set("q", state.query);
    }
    if (state.tag) {
      params.set("tag", state.tag);
    }
    if (state.category) {
      params.set("category", state.category);
    }
    if (state.sort && state.sort !== "newest") {
      params.set("sort", state.sort);
    }
    if (state.page > 1) {
      params.set("page", String(state.page));
    }

    const query = params.toString();
    const nextUrl = query ? `?${query}` : window.location.pathname;
    window.history.replaceState({}, "", nextUrl);
  }

  function uniqueSorted(values) {
    return Array.from(new Set(values.filter(Boolean))).sort((a, b) => a.localeCompare(b));
  }

  function populateFacets() {
    const tags = uniqueSorted(state.allItems.flatMap((item) => item.tags || []));
    const categories = uniqueSorted(state.allItems.map((item) => item.category || ""));

    tags.forEach((tag) => {
      const option = document.createElement("option");
      option.value = tag;
      option.textContent = tag;
      el.tag.appendChild(option);
    });

    categories.forEach((category) => {
      const option = document.createElement("option");
      option.value = category;
      option.textContent = category;
      el.category.appendChild(option);
    });

    el.tag.value = state.tag;
    el.category.value = state.category;
  }

  function applyFilters() {
    const query = normalize(state.query);
    const tag = normalize(state.tag);
    const category = normalize(state.category);

    let items = state.allItems.filter((item) => {
      if (tag && !(item.tags || []).map(normalize).includes(tag)) {
        return false;
      }
      if (category && normalize(item.category) !== category) {
        return false;
      }
      if (!query) {
        return true;
      }

      const haystack = [
        item.title,
        item.summary,
        item.category,
        (item.tags || []).join(" "),
      ]
        .join(" ")
        .toLowerCase();

      return haystack.includes(query);
    });

    items.sort((a, b) => {
      if (state.sort === "time_asc") {
        return (a.time_min ?? Number.MAX_SAFE_INTEGER) - (b.time_min ?? Number.MAX_SAFE_INTEGER);
      }
      if (state.sort === "time_desc") {
        return (b.time_min ?? -1) - (a.time_min ?? -1);
      }
      if (state.sort === "title_asc") {
        return (a.title || "").localeCompare(b.title || "");
      }
      return (b.published || "").localeCompare(a.published || "");
    });

    state.filtered = items;
  }

  function postCardHtml(item) {
    const tags = (item.tags || [])
      .map((tag) => `<span class="inline-flex border border-[#327f42] bg-[#081108] px-2 py-1 text-[11px] font-semibold uppercase tracking-[0.08em] text-[#9df8ad]">${escapeHtml(tag)}</span>`)
      .join(" ");

    const readLabel = item.time_min !== null && item.time_min !== undefined
      ? `${escapeHtml(item.time_min)} min read`
      : "Quick read";

    const imageHtml = item.image
      ? `<img src="${escapeHtml(item.image)}" alt="${escapeHtml(item.title || "Post image")}" class="h-44 w-full object-cover grayscale contrast-125">`
      : `<div class="h-44 w-full bg-[#0b180b]"></div>`;

    return `
      <article class="overflow-hidden border border-[#2a6d39] bg-[#061006] shadow-[0_0_0_1px_rgba(56,142,78,0.25)]">
        ${imageHtml}
        <div class="p-5">
          <p class="text-[11px] uppercase tracking-[0.16em] text-[#84da95]">${escapeHtml(item.published || "")}${item.category ? ` · ${escapeHtml(item.category)}` : ""}</p>
          <h2 class="mt-2 font-['Space_Grotesk'] text-2xl font-bold uppercase leading-tight text-[#dbffe2]">${escapeHtml(item.title || "Untitled")}</h2>
          <p class="mt-3 text-sm leading-7 text-[#a7eeb4]">${escapeHtml(item.summary || "")}</p>
          <p class="mt-3 text-xs uppercase tracking-[0.14em] text-[#70cd83]">${readLabel}</p>
          <div class="mt-4 flex flex-wrap gap-2">${tags}</div>
          <a href="${escapeHtml(item.url || "#")}" class="mt-5 inline-flex border border-[#3b954f] bg-[#081408] px-3 py-2 text-xs font-semibold uppercase tracking-[0.1em] text-[#bfffc9] hover:bg-[#0c1d0c]">Open post</a>
        </div>
      </article>
    `;
  }

  function render() {
    const total = state.filtered.length;
    const totalPages = Math.max(Math.ceil(total / PAGE_SIZE), 1);
    if (state.page > totalPages) {
      state.page = totalPages;
    }

    const start = (state.page - 1) * PAGE_SIZE;
    const pageItems = state.filtered.slice(start, start + PAGE_SIZE);

    el.results.innerHTML = pageItems.map(postCardHtml).join("\n");
    el.empty.classList.toggle("hidden", pageItems.length > 0);

    el.summary.textContent = `Showing ${pageItems.length ? start + 1 : 0}-${start + pageItems.length} of ${total} post${total === 1 ? "" : "s"}`;
    el.pageInfo.textContent = `Page ${state.page} of ${totalPages}`;

    el.prev.disabled = state.page <= 1;
    el.next.disabled = state.page >= totalPages;
    el.prev.classList.toggle("opacity-50", el.prev.disabled);
    el.next.classList.toggle("opacity-50", el.next.disabled);

    updateQueryParams();
  }

  function onFilterChange() {
    state.query = el.search.value.trim();
    state.tag = el.tag.value;
    state.category = el.category.value;
    state.sort = el.sort.value;
    state.page = 1;
    applyFilters();
    render();
  }

  async function init() {
    parseQueryParams();

    let records = [];
    try {
      const response = await fetch("search-index.json", { cache: "no-store" });
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }
      const payload = await response.json();
      records = Array.isArray(payload) ? payload : [];
    } catch (error) {
      el.summary.textContent = "Failed to load post index.";
      el.results.innerHTML = "";
      el.empty.classList.remove("hidden");
      el.empty.textContent = "Post index is unavailable. Run the build step to regenerate search-index.json.";
      return;
    }

    state.allItems = records
      .map(normalizeRecord)
      .filter((item) => item.kind === "post");

    populateFacets();
    applyFilters();
    render();

    el.search.addEventListener("input", onFilterChange);
    el.tag.addEventListener("change", onFilterChange);
    el.category.addEventListener("change", onFilterChange);
    el.sort.addEventListener("change", onFilterChange);

    el.clear.addEventListener("click", () => {
      el.search.value = "";
      el.tag.value = "";
      el.category.value = "";
      el.sort.value = "newest";
      onFilterChange();
    });

    el.prev.addEventListener("click", () => {
      if (state.page > 1) {
        state.page -= 1;
        render();
      }
    });

    el.next.addEventListener("click", () => {
      const totalPages = Math.max(Math.ceil(state.filtered.length / PAGE_SIZE), 1);
      if (state.page < totalPages) {
        state.page += 1;
        render();
      }
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
