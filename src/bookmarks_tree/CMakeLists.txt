set(plugin_name bookmarks_tree)

kcoreaddons_add_plugin(${plugin_name}
    INSTALL_NAMESPACE "kf6/ktexteditor")

target_sources(${plugin_name} PRIVATE
    BookmarksModel.cpp
    BookmarksTreePlugin.cpp
    BookmarksTreeView.cpp
)

target_link_libraries(${plugin_name}
    KF6::TextEditor
    Qt6::Core
    Qt6::Widgets
)