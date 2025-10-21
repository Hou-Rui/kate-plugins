set(plugin_name kate_ripgrep_search)

qt_add_resources(plugin_resources_qrc plugin.qrc)

kcoreaddons_add_plugin(${plugin_name}
    INSTALL_NAMESPACE "kf6/ktexteditor")    

target_sources(${plugin_name} PRIVATE
    RipgrepCommand.cpp
    RipgrepSearchPlugin.cpp
    RipgrepSearchView.cpp
    SearchResultsView.cpp
    ${plugin_resources_qrc}
)

target_link_libraries(${plugin_name}
    KF6::KIOCore
    KF6::TextEditor
    Qt6::Widgets
)
