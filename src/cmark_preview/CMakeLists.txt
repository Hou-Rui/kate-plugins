set(plugin_name kate_cmark_preview)

kcoreaddons_add_plugin(${plugin_name}
    INSTALL_NAMESPACE "kf6/ktexteditor")

target_sources(${plugin_name} PRIVATE
    CmarkCommand.cpp
    CmarkPreviewPlugin.cpp
    CmarkPreviewView.cpp
)

target_link_libraries(${plugin_name}
    KF6::TextEditor
    Qt6::Core
    Qt6::Widgets
)
