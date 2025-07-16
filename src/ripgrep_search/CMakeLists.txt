set(plugin_name ripgrep_search)

kcoreaddons_add_plugin(${plugin_name}
    INSTALL_NAMESPACE "kf6/ktexteditor")

target_sources(${plugin_name} PRIVATE
    RipgrepCommand.cpp
    RipgrepSearchPlugin.cpp
)

target_link_libraries(${plugin_name}
    KF6::KIOCore
    KF6::TextEditor
    Qt6::Core
    Qt6::Widgets
)