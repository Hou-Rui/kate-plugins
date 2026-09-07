# AGENT.md

This file provides guidance to coding agents when working with code in this repository.

## Overview

A collection of KDE Frameworks 6 (KF6) plugins for the Kate text editor, built on the `KTextEditor::Plugin` API. Each plugin lives in its own subdirectory under `src/` and is compiled into a separate loadable `.so`:

- **`bookmarks_tree`** — shows all Kate bookmarks across open documents as a tree in a sidebar tool view.
- **`ripgrep_search`** — find & replace across files/project using the `rg` (ripgrep) CLI.
- **`cmark_preview`** — renders the active Markdown document live in a sidebar tool view via the `cmark` CLI (falling back to `cmark-gfm`).

Each subdirectory sets `plugin_name` in its `CMakeLists.txt` and installs a `.so` of that name: `kate_bookmarks_tree`, `kate_ripgrep_search`, `kate_cmark_preview`. (The `bookmarks_tree` metadata file is `bookmarks_tree.json`; the other two are `kate_<plugin>.json`.)

## Build & Install

```sh
cmake -B build          # configure (cmake-init target)
cmake --build build     # build (cmake-build target, the .kateproject default)
sudo cmake --install build
```

Plugins install into the `kf6/ktexteditor` namespace and are loaded by Kate's plugin manager. There is no test suite. `compile_commands.json` is exported into `build/` for clangd/tooling.

Requires Qt ≥ 6.5 (`Concurrent`, `Core`, `DBus`, `Widgets`) and KF6 ≥ 6.0 (`TextEditor`, `KIO`, `I18n`); the ripgrep plugin needs an `rg` binary on PATH and optionally `kio-fuse` to search opened `sftp://` documents, while the Markdown plugin needs `cmark` or `cmark-gfm` on PATH. To rebuild after editing, just re-run `cmake --build build`.

## Code Style

`.clang-format` is present (WebKit base, 4-space indent, 160 column limit, Linux braces). Run `clang-format` before committing. Note the deliberate `// clang-format off` / `on` guards around multi-line literal blocks (tooltips, `createToolView` calls) — preserve them.

## Architecture

### Plugin lifecycle (all three plugins follow this)
`K_PLUGIN_CLASS_WITH_JSON(<Plugin>, "<plugin>.json")` registers the plugin. The `<plugin>.json` metadata file declares the name/icon/category Kate shows in its plugin manager. `Plugin::createView(MainWindow*)` is called once per Kate main window and returns a per-window view object; the plugin keeps a list of its views and `deleteLater()`s them on destruction. Each view builds its UI via `mainWindow->createToolView(...)` to get a sidebar dock.

### Two coding conventions coexist — match the file you're editing
- **`bookmarks_tree`** is single-translation-unit, uses `m_`-prefixed members directly on the view class, and `using namespace Qt::Literals::StringLiterals` (`u"..."_s`).
- **`ripgrep_search`** and **`cmark_preview`** use the **d-pointer (PIMPL) pattern**: each class has a `class FooPrivate;` forward declaration, a `QScopedPointer<FooPrivate> d` member, and all state/private slots live in the `FooPrivate` struct (with a back-pointer `q`). The private struct is `QObject`-derived when it needs slots (see `RipgrepSearchViewPrivate`, `CmarkPreviewViewPrivate`). New code in these plugins should follow the d-pointer style.

### ripgrep_search internals
Four collaborating classes wired together in `RipgrepSearchView::setupUi`/`setupRipgrepProcess`:

- **`RipgrepCommand`** — wraps a `QProcess` running `rg --json`. Setters (`setWholeWord`, etc.) mutate `SearchOptions` and emit `searchOptionsChanged()` to trigger a re-search. JSON output is parsed line-by-line into `matchFoundInFile` / `matchFound` / `searchFinished` signals. **Critical detail:** ripgrep reports submatch offsets as UTF-8 *byte* offsets, but `KTextEditor` cursors use UTF-16 *character* offsets — `byteOffsetToCharOffset()` converts between them, which matters for any line with multi-byte (e.g. CJK) characters. Don't reintroduce raw byte offsets when touching match/replace code.
- **`SearchResultsModel`** (`QStandardItemModel`) — two-level tree: file items (parents) → result-line items (children). Both are checkable for the replace workflow; `onItemChanged` propagates a file's check state down to its lines and refreshes the file's tri-state from its children. `checkedResults()` returns the `ReplacementTarget`s selected for replacement.
- **`SearchResultsView`** (`QTreeView`) — custom `SearchResultDelegate` paints match highlighting via `QTextLayout` and draws the check indicator only when `showCheckboxes()` is on (toggled with the replace options). Emits `jumpToFile` / `jumpToResult`, handled in the view to open the document and position the cursor/selection.
- **`RipgrepSearchView`** — owns the toolbar UI, actions, and orchestration. The tool view holds a `QStackedWidget` with two pages: the search UI (`searchPage`, an explicit zero-margin `QVBoxLayout`) and a centered placeholder shown when `rg` is missing (see below). Actions are registered through `KXMLGUIClient` (`actions.rc` defines the menu layout; `setupActions` creates them and adds the client to the GUI factory). Replace operates per-file, applying matches **bottom-up** (sorted by descending line/column) so earlier edits don't shift later positions.

### ripgrep availability
`RipgrepSearchViewPrivate::ripgrepAvailable()` checks for `rg` on PATH via `QStandardPaths::findExecutable("rg")`. The result is computed once in `setupActions` (which runs before `setupUi`) and cached in the `rgAvailable` member. When `rg` is absent:
- `setupActions` disables **every** action in `actionCollection()` (`setEnabled(false)`), which also disables their shortcuts — the plugin is inert rather than spawning a failing process.
- `setupUi` switches the `QStackedWidget` to the placeholder page (`createPlaceholder()`): a centered `dialog-warning` icon + rich-text label telling the user to install ripgrep.

The Kate sidebar tool-view button is owned by Kate (not our `actionCollection`), so it stays clickable and the user can still open the dock to read the placeholder. The check is one-shot at view construction — there's no live re-check if `rg` is installed afterwards.

### Search flow
`startSearch()` clears the model, then searches the **project base dir** (queried from the `kateprojectplugin` via `pluginView(...)->property("projectBaseDir")`) if available, otherwise the set of currently-open local and SFTP files. Opened `sftp://` documents are resolved asynchronously through `org.kde.KIOFuse.VFS.mountUrl`; `rg` receives the resulting local FUSE path, while `sourceUrlBySearchPath` retains the original document URL for navigation and replacement. Results stream in as `rg` emits them; the view auto-expands each file when its first result is inserted.

### cmark_preview internals
Three collaborating classes, all d-pointer style, wired together in `CmarkPreviewView`:

- **`CmarkCommand`** — wraps a `QProcess` that pipes the document text to `cmark --to html` on stdin and reads the HTML from stdout. `resolveExecutablePath()` honours an optionally-configured executable, otherwise prefers `cmark` and falls back to `cmark-gfm`; `isAvailable()` reflects whether either was found. Each `render()` bumps a `renderSerial` and kills any in-flight process so only the newest render's `finished` handler is honoured — stale output is dropped. Emits `rendered(html)` / `renderFailed(message)`. `executablePath()`/`setExecutablePath()` are exposed ahead of any config page so the renderer API needn't change when one is added.
- **`CmarkPreviewStyle`** — a free-function namespace (no QObject) plus a `CmarkPreviewStyleOptions` struct (document margin, line height, font, CSS style sheet). `defaultOptions(font)` seeds the options, `prepareDocument()` applies margin/font/style sheet to a `QTextDocument`, and `applyLineHeight()` walks the blocks to set proportional line height (which the CSS style sheet cannot express). `CmarkPreviewView::setStyleOptions()` re-applies and re-renders.
- **`CmarkPreviewView`** — owns the tool view (a `QLabel` status line + a read-only `QTextBrowser` with external links enabled). It tracks the active document via `MainWindow::viewChanged`, re-connecting to the document's `textChanged`. Edits are debounced through a 100 ms single-shot `QTimer` (`scheduleRender` → `renderDocument`); switching documents renders **immediately** (bypassing the timer) so stale output can't linger. The `rendered` handler bails if a newer edit is already pending (`renderTimer->isActive()`), sets the browser base URL to the document URL (for relative links/images), then applies HTML and line height.

### cmark availability
Unlike ripgrep, there is no placeholder page or action-disabling: availability is re-checked live on each render. `CmarkCommand::isAvailable()` is consulted in `setupUi` (initial status text), `scheduleRender`, and `renderDocument`; when neither executable is present the status label reads "cmark is not available" and the preview shows an install hint via `showMessage()` (which uses `setPlainText`). Because the check runs per-render, installing `cmark` afterwards works without reopening the view.

### Adding actions / menus
Menu/toolbar actions go through `KActionCollection` and the `actions.rc` XMLGUI file (entries must match action names). Context-menu-only actions are created as `QAction`s, registered on the widget with `Qt::WidgetShortcut` context so their shortcuts fire when the view has focus, and assembled into a `QMenu` in `contextMenuEvent`.
