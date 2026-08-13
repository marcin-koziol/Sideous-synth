#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Sideous"
#define DISTRHO_PLUGIN_NAME    "Sideous"
#define DISTRHO_PLUGIN_URI     "https://github.com/sideous/sideous"
#define DISTRHO_PLUGIN_CLAP_ID "sideous.sideous"

#define DISTRHO_PLUGIN_BRAND_ID  Side
#define DISTRHO_PLUGIN_UNIQUE_ID Sid1

#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_IS_SYNTH         1
#define DISTRHO_PLUGIN_NUM_INPUTS       0
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_STATE       0
#define DISTRHO_PLUGIN_WANT_PROGRAMS    0
#define DISTRHO_PLUGIN_WANT_TIMEPOS     1

#define DISTRHO_UI_USE_CAIRO             1
#define DISTRHO_UI_DEFAULT_WIDTH         760
#define DISTRHO_UI_DEFAULT_HEIGHT        1104
// must match the height ui/UIPainter.hpp's buildLayout() actually lays out to
// (verified via the offline PNG renderer) - keep these two in sync by hand.

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
