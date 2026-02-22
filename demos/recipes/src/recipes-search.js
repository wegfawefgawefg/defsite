(function () {
  const PAGE_SIZE = 6;

  const state = {
    allItems: [],
    filtered: [],
    page: 1,
    query: "",
    diet: "",
    method: "",
    sort: "newest",
  };

  const el = {
    search: document.getElementById("searchInput"),
    diet: document.getElementById("dietFilter"),
    method: document.getElementById("methodFilter"),
    sort: document.getElementById("sortFilter"),
    clear: document.getElementById("clearFilters"),
    summary: document.getElementById("resultsSummary"),
    results: document.getElementById("recipeResults"),
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

  function parseQueryParams() {
    const params = new URLSearchParams(window.location.search);
    state.query = params.get("q") || "";
    state.diet = params.get("diet") || "";
    state.method = params.get("method") || "";
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
    if (state.diet) {
      params.set("diet", state.diet);
    }
    if (state.method) {
      params.set("method", state.method);
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
    const diets = uniqueSorted(state.allItems.flatMap((item) => item.diets || []));
    const methods = uniqueSorted(state.allItems.map((item) => item.method || ""));

    diets.forEach((diet) => {
      const option = document.createElement("option");
      option.value = diet;
      option.textContent = diet;
      el.diet.appendChild(option);
    });

    methods.forEach((method) => {
      const option = document.createElement("option");
      option.value = method;
      option.textContent = method;
      el.method.appendChild(option);
    });

    el.diet.value = state.diet;
    el.method.value = state.method;
  }

  function applyFilters() {
    const query = normalize(state.query);
    const diet = normalize(state.diet);
    const method = normalize(state.method);

    let items = state.allItems.filter((item) => {
      if (diet && !(item.diets || []).map(normalize).includes(diet)) {
        return false;
      }
      if (method && normalize(item.method) !== method) {
        return false;
      }
      if (!query) {
        return true;
      }

      const haystack = [
        item.title,
        item.summary,
        item.method,
        (item.diets || []).join(" "),
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

  function recipeCardHtml(item) {
    const chips = (item.diets || [])
      .map((diet) => `<span class="inline-flex rounded-full bg-stone-200 px-3 py-1 text-xs font-semibold text-stone-700">${escapeHtml(diet)}</span>`)
      .join(" ");

    const metaBits = [];
    if (item.time_min !== null && item.time_min !== undefined) {
      metaBits.push(`${escapeHtml(item.time_min)} min`);
    }
    if (item.difficulty) {
      metaBits.push(escapeHtml(item.difficulty));
    }
    if (item.serves) {
      metaBits.push(`serves ${escapeHtml(item.serves)}`);
    }

    const imageHtml = item.image
      ? `<img src="${escapeHtml(item.image)}" alt="${escapeHtml(item.title || "Recipe image")}" class="h-44 w-full object-cover">`
      : `<div class="h-44 w-full bg-stone-200"></div>`;

    return `
      <article class="overflow-hidden rounded-2xl border border-stone-300/60 bg-white shadow-sm">
        ${imageHtml}
        <div class="p-5">
          <p class="text-xs uppercase tracking-[0.17em] text-stone-500">${metaBits.join(" · ")}</p>
          <h2 class="mt-2 font-['Noto_Serif_JP'] text-2xl font-black leading-tight">${escapeHtml(item.title || "Untitled")}</h2>
          <p class="mt-2 text-sm text-stone-600">${escapeHtml(item.summary || "")}</p>
          <div class="mt-4 flex flex-wrap gap-2">${chips}</div>
          <a href="${escapeHtml(item.url || "#")}" class="mt-5 inline-flex w-max rounded-lg bg-stone-900 px-3 py-2 text-sm font-semibold text-stone-50 hover:bg-stone-700">Open recipe</a>
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

    el.results.innerHTML = pageItems.map(recipeCardHtml).join("\n");
    el.empty.classList.toggle("hidden", pageItems.length > 0);

    el.summary.textContent = `Showing ${pageItems.length ? start + 1 : 0}-${start + pageItems.length} of ${total} recipe${total === 1 ? "" : "s"}`;
    el.pageInfo.textContent = `Page ${state.page} of ${totalPages}`;

    el.prev.disabled = state.page <= 1;
    el.next.disabled = state.page >= totalPages;
    el.prev.classList.toggle("opacity-50", el.prev.disabled);
    el.next.classList.toggle("opacity-50", el.next.disabled);

    updateQueryParams();
  }

  function onFilterChange() {
    state.query = el.search.value.trim();
    state.diet = el.diet.value;
    state.method = el.method.value;
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
      records = await response.json();
    } catch (error) {
      el.summary.textContent = `Failed to load search index: ${error}`;
      el.empty.classList.remove("hidden");
      return;
    }

    state.allItems = Array.isArray(records) ? records : [];
    populateFacets();
    applyFilters();
    render();

    el.search.addEventListener("input", onFilterChange);
    el.diet.addEventListener("change", onFilterChange);
    el.method.addEventListener("change", onFilterChange);
    el.sort.addEventListener("change", onFilterChange);

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

    el.clear.addEventListener("click", () => {
      state.query = "";
      state.diet = "";
      state.method = "";
      state.sort = "newest";
      state.page = 1;

      el.search.value = "";
      el.diet.value = "";
      el.method.value = "";
      el.sort.value = "newest";

      applyFilters();
      render();
    });
  }

  init();
})();
