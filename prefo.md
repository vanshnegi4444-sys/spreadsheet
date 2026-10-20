# prefo.md — Making x-spreadsheet Faster, Smoother, and Better for Users

This document looks at the codebase **from the user's side**. It covers:

1. The Excel complaints people post about online (Reddit r/excel, Microsoft forums, data-science write-ups)
2. Whether **this** app has the same problem, based on reading the source
3. How to fix each one
4. How this document compares to [performance.md](performance.md)

> [!NOTE]
> Every finding below links to the line it came from. Speed-up numbers marked *"est."* are estimates and haven't been measured. Profile before and after each change.

---

## Part 1 — Excel Pain Points vs. This Codebase

### Scorecard

| # | Excel complaint (from online sources) | Does x-spreadsheet have it? | Severity here |
|---|---|---|---|
| 1 | Auto-conversion wrecks data (gene names → dates, lost leading zeros, 15-digit IDs → `1.23E+15`) | **No, mostly avoided.** Cells keep raw text | Low |
| 2 | Floating-point surprises (`0.1+0.2≠0.3`) | **Yes, and worse.** Every formula result goes through `std::to_string` (6 decimals) | **High** |
| 3 | Freezes and slow recalculation on big sheets | **Yes.** Formulas re-run on every rendered frame, with no caching and no manual-calc mode | **High** |
| 4 | Whole-column / bloated-range formatting makes files huge and slow | **Yes, and worse.** Formatting a selection creates one `Cell` object for every cell | **Critical** |
| 5 | Lost work after crashes; AutoSave you can't rely on | **Yes.** The AutoSave button does nothing, saves aren't atomic, and load errors are silent | **Critical** |
| 6 | Limited undo; undo history gets wiped | **Partly.** 100 steps, but each step is a full JSON snapshot | High |
| 7 | Unhelpful circular-reference messages | **Yes, and worse.** A circular reference shows `#REF!`, which is the wrong error | Medium |
| 8 | Cryptic or missing errors (`#VALUE!`) | **Yes.** Text in arithmetic silently turns into `0` | Medium |
| 9 | Opening a CSV mangles it | **No.** Import keeps text as-is. But export allows CSV injection | Medium |
| 10 | Slow startup from COM add-ins | **No.** Native binary with no plug-ins | Advantage |
| 11 | Keyboard shortcuts behave oddly | **Yes.** Shortcuts are *shown* in tooltips but never wired up | **High (UX)** |

---

### 1. Data auto-conversion — mostly avoided ✅

**Excel:** Typing `SEPT9` gives `Sep-9`, `00123` gives `123`, and a 16-digit card number loses its last digits for good. This was common enough that HUGO renamed human genes because of it.

**Here:** [`Rows::setCellText`](cpp/src/core/row_col.cpp) stores the raw string, and [`Formatter::format`](cpp/src/core/format.cpp#L31-L68) only reformats when the user picks a number format. **Nothing gets rewritten on input.** That's an advantage worth keeping.

**Watch out:** Date, time, and duration formats aren't implemented. They fall through to `return rawText` at [format.cpp:L66-L67](cpp/src/core/format.cpp#L66-L67). When you add them, **don't** auto-detect dates on input. Make conversion opt-in, the way Excel 365's *File › Options › Data › Automatic Data Conversion* toggles now work.

---

### 2. Number precision — worse than Excel ❌

Excel keeps 15 significant digits. This engine keeps **6 decimal places**, and it re-parses that string at every step of a formula chain.

**Where:** Every arithmetic result in [formula.cpp:L314-L333](cpp/src/core/formula.cpp#L314-L333):

```cpp
stack.push_back(std::fmod(res, 1.0) == 0.0
    ? std::to_string(static_cast<long long>(res))   // UB if |res| > 9.2e18
    : std::to_string(res));                          // "%f" → 6 decimals
```

| Input | Excel shows | x-spreadsheet shows |
|---|---|---|
| `=1/3` | `0.333333333333333` | `0.333333` |
| `=0.1+0.2` | `0.3` | `0.300000` |
| `=1/3*3` (B1 refers to A1) | `1` | `0.999999` (error carried through the chain) |
| `=10^20` sized result | `1E+20` | Undefined behaviour (cast overflow) |

Also, the tokenizer at [formula.cpp:L119-L126](cpp/src/core/formula.cpp#L119-L126) doesn't accept scientific notation (`1e5`).

**Fix:**
1. Pass values through the evaluator as a `Value` variant (`double | string | bool | Error`), not as strings. Only convert to text at display time.
2. Format with `std::format("{:.15g}", v)` (C++20, already enabled in CMake) or `std::to_chars`. That gives 15 significant digits and no trailing zeros.
3. Drop the `long long` cast. `{:.15g}` already prints whole numbers cleanly.

---

### 3. Freezing and slow recalculation ❌

**Excel:** People complain about freezes with volatile functions and big `SUMIFS`. The usual workaround is *Calculation › Manual*.

**Here it's structural:** [`getCellDisplayValue`](cpp/src/core/sheet_data.cpp#L163-L178) runs `FormulaEngine::eval` → tokenize → RPN → evaluate **for every visible formula cell on every frame**. Each referenced cell is evaluated **recursively** ([formula.cpp:L279-L290](cpp/src/core/formula.cpp#L279-L290)). So:

- A chain `A2=A1+1 … A1000=A999+1` costs about 1000 evaluations for A1000 alone. With ~30 of those cells on screen, a single frame does about 30,000 tokenize/parse passes.
- A **long enough chain overflows the C++ call stack** and crashes. Excel would just compute slowly.
- `SUM(A1:A1000)` builds 1000 `"A1"…"A1000"` strings ([formula.cpp:L203-L210](cpp/src/core/formula.cpp#L203-L210)) and then parses each one back with `expr2xy`.
- `IF` evaluates **both** branches before choosing one ([formula.cpp:L408-L418](cpp/src/core/formula.cpp#L408-L418)).

**Fix, in priority order:**
1. **Cache results:** add `cachedValue` and `dirty` to `Cell`. Re-evaluate only when an input changes. This removes almost all per-frame cost.
2. **Iterative evaluation:** walk dependencies with an explicit stack in topological order, not recursion. That ends the stack-overflow crash.
3. **Range as a first-class token:** iterate `(r, c)` directly instead of generating strings.
4. **Manual calc toggle + F9:** give users the escape hatch Excel users rely on.
5. A **"Calculating… (n%)"** status-bar indicator, so long recalcs look like progress, not a hang.

---

### 4. Formatting blow-up — the biggest problem found ❌❌

**Excel:** Formatting applied down to row 1,048,576 bloats the file and slows everything. Reddit's usual fix is "copy to a new workbook".

**Here it's worse**, and the recent change to a **1000 × 1000** default grid makes it reachable with one click:

1. Click the top-left corner. That selects all **1,000,000** cells ([spreadsheet_ui.cpp](cpp/src/ui/spreadsheet_ui.cpp), corner-click handler).
2. Click **Bold**. [`setSelectedCellAttr`](cpp/src/core/sheet_data.cpp#L62-L92) then:
   - calls `rows.getCellOrNew` **1,000,000 times**, creating 1M `Cell` objects (two `std::string`s each, plus hash-map nodes, roughly 100+ MB, est.)
   - calls `addStyle`, which **linearly scans** the whole style table ([sheet_data.cpp:L19-L25](cpp/src/core/sheet_data.cpp#L19-L25)), once per cell
   - **before all that**, `pushHistoryState()` serializes the sheet to JSON. The *next* edit then snapshots all 1M cells, and up to 100 of those snapshots can sit in the undo stack.
3. On top of that, [`calculateSelectionStats`](cpp/src/ui/spreadsheet_ui.cpp#L2198-L2220) walks **all 1M cells, copying a `std::string` each time, on every rendered frame** while the selection is active. Even a mouse move redraws.

**Fix:**
- **Row/column-level styles:** `RowData::style` and `ColData::style` already exist in [row_col.h](cpp/src/core/row_col.h). Use them. A whole-column or whole-row selection should set **one** column/row style, not N cell styles. Lookup order: cell, then row, then column, then default.
- **Range styles:** store `{CellRange, styleId}` entries for rectangular selections (Google Sheets and LibreOffice work this way).
- **Style interning:** use `std::unordered_map<Style, int>` (hash of fields) in place of the linear scan.
- **Stats:** iterate only cells that **exist** in the sparse map and fall inside the selection, and recompute only when the selection or data changes, not every frame.

---

### 5. Lost work — AutoSave is fake ❌❌

**Excel:** "Excel crashed and AutoRecover didn't save my file" is one of the most common complaints.

**Here:**

| Problem | Location |
|---|---|
| The **AutoSave button only flips a bool**. Nothing ever saves on a timer. The label always says "AutoSave: On", even when it's off. | [spreadsheet_ui.cpp:L169-L178](cpp/src/ui/spreadsheet_ui.cpp#L169-L178) |
| Save writes **straight over the target file**. A crash or full disk mid-write leaves a truncated, unreadable JSON file. | [spreadsheet_book.cpp:L84-L89](cpp/src/core/spreadsheet_book.cpp#L84-L89) |
| The `saveToFile` return value is ignored, so the user never hears that a save failed. | [spreadsheet_ui.cpp:L185-L187](cpp/src/ui/spreadsheet_ui.cpp#L185-L187) |
| Load swallows every exception with `catch (...) { return false; }`. A corrupt file just fails silently. | [spreadsheet_book.cpp:L94-L101](cpp/src/core/spreadsheet_book.cpp#L94-L101) |
| No "unsaved changes" prompt when closing the window. | [main.cpp](cpp/src/main.cpp) |

**Fix:**
1. **Atomic save:** write to `file.json.tmp`, flush, then `MoveFileExW(tmp, file, MOVEFILE_REPLACE_EXISTING)` (or `std::filesystem::rename`). The old file stays intact until the new one is complete.
2. **Real AutoSave:** keep a dirty flag, and once it's been dirty for 30 s, save on a **background thread** to `%LOCALAPPDATA%\xspreadsheet\recovery\`. On startup, offer to restore.
3. **Visible errors:** show a status-bar or toast message like "Save failed: disk full" or "File is not valid JSON at line 42" (nlohmann's `parse_error` includes the byte position).
4. **Close guard:** set `glfwSetWindowCloseCallback`, check the dirty flag, and ask Save / Discard / Cancel.
5. Fix the AutoSave button label so it reflects the actual state.

---

### 6. Undo ⚠️

**Excel:** Some actions wipe undo history, and the step limit gets hit.

**Here:** [`History`](cpp/src/core/history.h) stores **100 full JSON snapshots**, and `undoItems.erase(begin())` is O(n). Undo is per-sheet, and deleting a sheet ([spreadsheet_book.cpp:L38-L47](cpp/src/core/spreadsheet_book.cpp#L38-L47)) can't be undone. One upside over Excel: nothing *clears* history.

**Fix:** use command/delta undo (see performance.md §4) and a `std::deque` ring buffer. Once each step is cheap, raise the limit to 1000+. Put sheet add/delete/rename on a **workbook-level** undo stack.

---

### 7 & 8. Error messages ⚠️

| Situation | Excel | x-spreadsheet | Should be |
|---|---|---|---|
| `A1: =A1+1` (circular) | Warning dialog + `0` | `#REF!` ([formula.cpp:L282](cpp/src/core/formula.cpp#L282)), which wrongly suggests a deleted reference | `#CIRC!` plus a status-bar note naming the loop: "Circular: A1 → B3 → A1" |
| `="abc"-1` | `#VALUE!` | `-1` (text quietly becomes 0) | `#VALUE!` |
| `=FOO(1)` | `#NAME?` | `#NAME?` ✅ | Add a tooltip: "Unknown function FOO. Did you mean FLOOR?" |
| Typed text longer than 1023 bytes | Allowed (32,767 limit) | **Silently cut off.** `cellEditBuf[1024]` in [spreadsheet_ui.h](cpp/src/ui/spreadsheet_ui.h) | Use `std::string` with `ImGuiInputTextFlags_CallbackResize` |

Since the evaluator already tracks `visited`, it can report the full cycle path cheaply. Use that to do better than Excel here.

---

### 9. CSV ⚠️

- ✅ Import keeps text exactly as written (no Excel-style mangling).
- ❌ **Import doesn't grow the grid.** [`importCsv`](cpp/src/core/spreadsheet_book.cpp#L135-L184) writes cells but never extends `rows.len` / `cols.len`, so rows past 1000 are stored but **can't be seen**.
- ❌ **CSV/formula injection on export:** cells starting with `=`, `+`, `-`, `@` are written raw ([spreadsheet_book.cpp:L114-L126](cpp/src/core/spreadsheet_book.cpp#L114-L126)). If someone opens the export in Excel, those run as formulas. That's a known security issue (OWASP "CSV Injection"). Offer an "escape formulas" option that prefixes `'`.
- ❌ No UTF-8 BOM handling on import (a BOM ends up inside cell A1).

---

### 11. Keyboard UX — shortcuts advertised but not wired ❌

The tooltips and help panel list **Ctrl+Z, Ctrl+Y, Ctrl+C, Ctrl+X, Ctrl+V, Ctrl+S, Ctrl+B, Ctrl+I, Ctrl+U, Ctrl+A** ([spreadsheet_ui.cpp:L1057-L1062](cpp/src/ui/spreadsheet_ui.cpp#L1057-L1062)). The only Ctrl handling in the whole UI is **Ctrl+Home** ([spreadsheet_ui.cpp:L1820](cpp/src/ui/spreadsheet_ui.cpp#L1820)). Excel users will try these keys right away and assume the app is broken.

Also, **typing a character on a selected cell doesn't start editing**. Only Enter, F2, or double-click do ([spreadsheet_ui.cpp:L1763-L1764](cpp/src/ui/spreadsheet_ui.cpp#L1763-L1764)). In Excel you just start typing, and that's the most common way people enter data.

**Fix:** add a shortcut table using `ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)` (ImGui ≥1.90), checked once per frame when no text field is active. Read `io.InputQueueCharacters` while a cell is selected, and if it's non-empty, call `startCellEditing(ri, ci, typedChars)`.

---

## Part 2 — Smoothness (what users *feel*)

### The current "hybrid" render loop — how it really works

[main.cpp](cpp/src/main.cpp) now sleeps with `glfwWaitEvents()` when idle, runs at 60 FPS while scrolling or dragging, and polls every 350 ms while a cell is being edited. Idle CPU is now about zero, which is a real gain.

> [!IMPORTANT]
> This is **event-gating (Option 1)**, not the FBO/scissor partial redraw (Option 2) that was requested. Each frame still redraws the **whole window**. The status-bar text "Event-Driven (Pixel updates)" overstates this and should be reworded. A true Option 2 (cache the grid in an offscreen FBO and redraw only dirty cell rectangles with `glScissor`) hasn't been built yet. See the plan below.

Known side effects of the current gating:
- **Mouse movement wakes the loop.** Hovering over the window renders a full frame for every cursor event. That's fine for idle power, but hovering isn't free.
- Dear ImGui sometimes needs **1–2 extra frames** to settle after an input (hover highlights, popups opening). With pure `glfwWaitEvents()`, a popup can look one frame late. Common fix: after any input, render 2–3 more frames before sleeping again.

### Real partial redraw (Option 2) — implementation plan

1. Render the grid area (cells, gridlines, headers) into an **FBO texture** when it's marked dirty.
2. Each frame, draw that texture with `ImGui::Image` and let ImGui draw only the *overlays* on top: selection box, editor, ribbon, menus.
3. On a cell edit, mark **only that cell's rect** dirty, bind the FBO, call `glScissor(rect)`, and redraw just that cell into the texture.
4. On scroll, either redraw the FBO fully, or (better) **blit-shift** the existing texture by the scroll delta and redraw only the newly exposed strip. Browsers scroll this way.
5. Mark everything dirty on zoom, resize, or style change.

That gives truly pixel-local updates when typing, and cheap scrolling.

### Other smoothness items

| Issue | Fix |
|---|---|
| Wheel scroll jumps 40 px per notch ([spreadsheet_ui.cpp](cpp/src/ui/spreadsheet_ui.cpp), `renderGridCanvas`) | Smooth scrolling: lerp toward a target scroll value over ~100 ms, and keep `scrollActivityTimer` alive until it arrives |
| `parseHexColor` builds a `std::stringstream` for every cell, every frame ([spreadsheet_ui.cpp:L26-L55](cpp/src/ui/spreadsheet_ui.cpp#L26-L55)) | Parse once when the style is created and store `ImU32` in `Style` |
| `getCellStyle` returns `Style` **by value** (strings, borders) for each visible cell | Return `const Style&` |
| `ImGui::CalcTextSize` runs on every visible cell, every frame | Cache the measured width alongside the cached display string |
| Long text doesn't spill into the empty neighbouring cell the way Excel's does | Clip to the cell rect, or extend into empty neighbours (and keep that consistent with the FBO dirty rects) |

---

## Part 3 — Customer-Focused Roadmap

Ordered by **user pain per hour of work**:

| Priority | Item | Why users care |
|---|---|---|
| **P0** | Atomic save + real AutoSave + close prompt | Losing work is the worst thing a spreadsheet can do |
| **P0** | Wire up Ctrl+Z/C/V/X/S/A/B/I/U + type-to-edit | Without these, it feels broken in the first minute |
| **P0** | Row/column-level styles; stats only over existing cells | Stops the 1M-cell freeze from select-all + Bold |
| **P1** | Formula result cache + iterative evaluation | Smooth scrolling over formulas; no stack-overflow crash |
| **P1** | `Value` type + `{:.15g}` formatting | Correct numbers (trust) |
| **P1** | Correct errors: `#CIRC!` with path, `#VALUE!` | Users can fix their own mistakes |
| **P2** | CSV: grow grid on import, BOM, injection-safe export | Data round-trips safely |
| **P2** | True FBO partial redraw + smooth scroll | Premium feel, lower GPU use |
| **P3** | Delta undo, larger history, workbook-level undo | Confidence to experiment |
| **P3** | Manual-calc mode + "Calculating…" progress | Power users with heavy sheets |

---

## Part 4 — Comparison with `performance.md`

### What each document covers

| Topic | performance.md | prefo.md (this) |
|---|---|---|
| Per-frame `rowY`/`colX` layout cost | ✅ Detailed (O(1) + Fenwick tree) | Referenced only |
| Formula caching / DAG | ✅ Architecture | ✅ Plus **the stack-overflow crash** and the `IF` both-branches issue |
| Bytecode / SIMD | ✅ | — (not needed until caching is done) |
| Delta undo | ✅ | ✅ Plus workbook-level undo and the deque fix |
| Merge spatial index | ✅ | — |
| Chunked storage | ✅ | — |
| Sort pre-parsing | ✅ | — |
| **Formatting blow-up (1M cells)** | ❌ Missed | ✅ **Critical, top finding** |
| **Selection stats over 1M cells per frame** | ❌ Missed | ✅ |
| **Numeric precision (`to_string`, 6 dp, UB cast)** | ❌ | ✅ |
| **Data safety (AutoSave, atomic save, silent load)** | ❌ | ✅ |
| **Keyboard shortcuts / type-to-edit** | ❌ | ✅ |
| **Error correctness (`#REF!` vs circular, `#VALUE!`)** | ❌ | ✅ |
| **CSV injection / grid not growing on import** | ❌ | ✅ |
| Render loop / FPS / partial redraw | ❌ (written before the hybrid loop) | ✅ Including the Option 1 vs Option 2 correction |
| Per-cell `parseHexColor` / `Style` copies | ❌ | ✅ |

### Main differences

1. **Scale vs. everyday use.** performance.md asks how to reach 1M rows. prefo.md asks what goes wrong for someone using it today at 1000 × 1000. The most serious *practical* problem (formatting blow-up) only shows up from the second angle.
2. **Speed vs. trust.** performance.md is purely about speed. Several items here (precision, fake AutoSave, wrong error codes, unwired shortcuts) aren't performance problems at all, but they're what Excel users actually complain about.
3. **Corrections to performance.md:**
   - Its speed-up figures ("450 ms → 6 ms", "50×–200×") are **estimates, not measurements**. Treat them as orders of magnitude.
   - Its Phase 1 should add **row/column styles** and **stats over existing cells**. Those block the worst freeze and are cheaper than anything currently in Phase 1.
   - The earlier claim that a 1000 × 1000 grid "works fine" holds for **viewing and scrolling**. It doesn't hold for **select-all operations**, which become a 1M-cell loop.

### Combined order of work

```
P0  Data safety  ─┐
P0  Shortcuts     ├─ prefo.md (user trust)
P0  Row/col styles┘
P1  Formula cache + iterative eval ── both docs agree
P1  Value type / precision ────────── prefo.md
P2  O(1) layout (performance.md §1) + FBO partial redraw (prefo.md)
P3  Delta undo, merge index, sort keys, chunked storage, bytecode/SIMD ── performance.md
```

---

### Sources consulted (online)
- Reddit r/excel threads on slowdowns, COM add-ins, volatile functions, file bloat
- Microsoft Support: circular references, excessive formatting cleanup, Automatic Data Conversion options
- NIH / HGNC reporting on gene names being converted to dates by Excel
- Exceljet: circular reference behaviour
- OWASP: CSV Injection
