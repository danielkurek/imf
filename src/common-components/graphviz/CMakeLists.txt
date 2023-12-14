idf_component_register()

set(enable_ltdl OFF)
set(with_expat OFF)
set(with_digcola OFF)
set(with_gvedit OFF)
set(with_ortho OFF)
set(with_sfdp OFF)
set(with_zlib OFF)

add_subdirectory(graphviz)

target_link_libraries(${COMPONENT_LIB} INTERFACE common gvc neatogen cgraph gvplugin_core)

target_sources(${COMPONENT_LIB} INTERFACE "neato_esp/neato_esp.c")
target_include_directories(${COMPONENT_LIB} INTERFACE "neato_esp")