dpf_add_plugin(sideous
  TARGETS lv2 vst3 clap jack
  UI_TYPE cairo
  FILES_DSP
      SideousPlugin.cpp
  FILES_UI
      SideousUI.cpp)

target_include_directories(sideous PUBLIC ".")
