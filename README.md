# Kate Bookmark Plugin

## Overview

A simple plugin to display all Kate bookmarks as a tree in a tool view, available via a sidebar button. KF6 only.

![Screenshot_20250703_230637](https://github.com/user-attachments/assets/66796f77-20e2-497a-975e-ed82e9b71167)

## Dependencies

- Qt 6
- KDE Framework 6.x
- Kate Editor built with KF6

## Installation

```sh
cmake . -B build
cmake --build build
sudo cmake --install build
```

`katebookmarksplugin.so` should now be installed in KTextEditor plugin directory.

## Usage

Enable "Bookmarks" plugin in Kate's plugin manager. In menu, enable "View > Sidebar Buttons > Show Bookmarks Button". Now bookmarks button should appear on the left side, click it to show bookmark tool view.Double click on any bookmark to jump to its location.
