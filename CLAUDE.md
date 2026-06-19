# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A collection of KDE Frameworks 6 (KF6) plugins for the Kate text editor, built on the `KTextEditor::Plugin` API. Each plugin lives in its own subdirectory under `src/` and is compiled into a separate loadable `.so`:

- **`bookmarks_tree`** — shows all Kate bookmarks across open documents as a tree in a sidebar tool view.
- **`ripgrep_search`** — find & replace across files/project using the `rg` (ripgrep) CLI.

## Build & Install

```sh
cmake -B build          # configure (cmake-init target)
cmake --build build     # build (cmake-build target, the .kateproject default)
sudo cmake --install build
```

Plugins install into the `kf6/ktexteditor` namespace and are loaded by Kate's plugin manager. There is no test suite. `compile_commands.json` is exported into `build/` for clangd/tooling.

Requires Qt ≥ 6.5, KF6 ≥ 6.0 (`TextEditor`, `KIO`, `I18n`), and a working `rg` binary on PATH for the ripgrep plugin. To rebuild after editing, just re-run `cmake --build build`.

## Code Style

`.clang-format` is present (WebKit base, 4-space indent, 160 column limit, Linux braces). Run `clang-format` before committing. Note the deliberate `// clang-format off` / `on` guards around multi-line literal blocks (tooltips, `createToolView` calls) — preserve them.

## Architecture

### Plugin lifecycle (both plugins follow this)
`K_PLUGIN_CLASS_WITH_JSON(<Plugin>, "<plugin>.json")` registers the plugin. The `<plugin>.json` metadata file declares the name/icon/category Kate shows in its plugin manager. `Plugin::createView(MainWindow*)` is called once per Kate main window and returns a per-window view object; the plugin keeps a list of its views and `deleteLater()`s them on destruction. Each view builds its UI via `mainWindow->createToolView(...)` to get a sidebar dock.

### Two coding conventions coexist — match the file you're editing
- **`bookmarks_tree`** is single-translation-unit, uses `m_`-prefixed members directly on the view class, and `using namespace Qt::Literals::StringLiterals` (`u"..."_s`).
- **`ripgrep_search`** uses the **d-pointer (PIMPL) pattern**: each class has a `class FooPrivate;` forward declaration, a `QScopedPointer<FooPrivate> d` member, and all state/private slots live in the `FooPrivate` struct (with a back-pointer `q`). The private struct is `QObject`-derived when it needs slots (see `RipgrepSearchViewPrivate`). New code in this plugin should follow the d-pointer style.

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
`startSearch()` clears the model, then searches the **project base dir** (queried from the `kateprojectplugin` via `pluginView(...)->property("projectBaseDir")`) if available, otherwise the set of currently-open local files. Results stream in as `rg` emits them; the view auto-expands each file as its rows are inserted.

### Adding actions / menus
Menu/toolbar actions go through `KActionCollection` and the `actions.rc` XMLGUI file (entries must match action names). Context-menu-only actions are created as `QAction`s, registered on the widget with `Qt::WidgetShortcut` context so their shortcuts fire when the view has focus, and assembled into a `QMenu` in `contextMenuEvent`.
