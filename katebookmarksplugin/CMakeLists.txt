kcoreaddons_add_plugin(katebookmarksplugin
    INSTALL_NAMESPACE "kf6/ktexteditor")

target_sources(katebookmarksplugin PRIVATE
    KateBookmarksPlugin.cpp
)

target_link_libraries(katebookmarksplugin
    KF6::TextEditor
    Qt6::Core
    Qt6::Widgets
)