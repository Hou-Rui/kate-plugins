# Kate Plugins

A collection of plugins for Kate and KDE Frameworks 6:

- `bookmarks_tree` — displays and navigates bookmarks from all open documents in a sidebar tree.
- `ripgrep_search` — searches and replaces text across a project or the open files using `rg`.
- `cmark_preview` — renders the active Markdown document live using `cmark`, falling back to `cmark-gfm`.

## Requirements

- Qt 6.5 or newer
- KDE Frameworks 6.0 or newer
- Kate built with KF6
- `rg` for `ripgrep_search`
- `cmark` or `cmark-gfm` on `PATH` for `cmark_preview`

## Build and install

```sh
cmake -B build
cmake --build build
sudo cmake --install build
```

After installation, enable the desired plugins in Kate's plugin manager. The bookmark and ripgrep plugins provide sidebar tool views; the Markdown preview follows the active editor document.
