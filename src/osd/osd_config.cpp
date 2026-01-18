#include <ares/osd_config.h>
#include <ares/processing_config.h>

namespace ares {

// Create default OSD menu structure (madVR Envy style)
OSDMenuStructure createDefaultOSDMenu() {
    OSDMenuStructure menu;

    // Tab 1: Processing
    Menu processing_tab;
    processing_tab.id = "processing";
    processing_tab.title = "Processing";

    MenuItem nls_enable;
    nls_enable.id = "nls_enable";
    nls_enable.label = "NLS (Aspect Ratio Warping)";
    nls_enable.type = MenuItemType::TOGGLE;
    nls_enable.tooltip = "Enable non-linear stretch for cinemascope screens";
    // TODO: Link to actual config
    processing_tab.items.push_back(nls_enable);

    MenuItem black_bars_enable;
    black_bars_enable.id = "black_bars_enable";
    black_bars_enable.label = "Black Bar Detection";
    black_bars_enable.type = MenuItemType::TOGGLE;
    black_bars_enable.tooltip = "Automatically detect and crop black bars";
    processing_tab.items.push_back(black_bars_enable);

    menu.tabs.push_back(processing_tab);

    // Tab 2: Tone Mapping
    Menu tone_mapping_tab;
    tone_mapping_tab.id = "tone_mapping";
    tone_mapping_tab.title = "Tone Mapping";

    MenuItem tone_algorithm;
    tone_algorithm.id = "tone_algorithm";
    tone_algorithm.label = "Algorithm";
    tone_algorithm.type = MenuItemType::ENUM;
    tone_algorithm.enum_options = {"BT.2390", "Reinhard", "Hable", "Mobius", "Clip"};
    tone_algorithm.tooltip = "HDR tone mapping algorithm";
    tone_mapping_tab.items.push_back(tone_algorithm);

    MenuItem target_nits;
    target_nits.id = "target_nits";
    target_nits.label = "Target Brightness";
    target_nits.type = MenuItemType::SLIDER;
    target_nits.min_value = 50.0f;
    target_nits.max_value = 500.0f;
    target_nits.step = 10.0f;
    target_nits.unit = " nits";
    target_nits.tooltip = "Target display peak brightness";
    tone_mapping_tab.items.push_back(target_nits);

    menu.tabs.push_back(tone_mapping_tab);

    // Tab 3: Color
    Menu color_tab;
    color_tab.id = "color";
    color_tab.title = "Color";

    MenuItem saturation;
    saturation.id = "saturation";
    saturation.label = "Saturation";
    saturation.type = MenuItemType::SLIDER;
    saturation.min_value = 0.5f;
    saturation.max_value = 2.0f;
    saturation.step = 0.1f;
    saturation.tooltip = "Color saturation adjustment";
    color_tab.items.push_back(saturation);

    MenuItem contrast;
    contrast.id = "contrast";
    contrast.label = "Contrast";
    contrast.type = MenuItemType::SLIDER;
    contrast.min_value = 0.5f;
    contrast.max_value = 2.0f;
    contrast.step = 0.1f;
    contrast.tooltip = "Contrast adjustment";
    color_tab.items.push_back(contrast);

    menu.tabs.push_back(color_tab);

    // Tab 4: Display
    Menu display_tab;
    display_tab.id = "display";
    display_tab.title = "Display";

    MenuItem refresh_rate;
    refresh_rate.id = "refresh_rate";
    refresh_rate.label = "Refresh Rate";
    refresh_rate.type = MenuItemType::ENUM;
    refresh_rate.enum_options = {"23.976", "24", "25", "29.97", "30", "50", "59.94", "60"};
    refresh_rate.tooltip = "Display refresh rate";
    display_tab.items.push_back(refresh_rate);

    menu.tabs.push_back(display_tab);

    // Tab 5: Info
    Menu info_tab;
    info_tab.id = "info";
    info_tab.title = "Info";

    MenuItem version;
    version.id = "version";
    version.label = "Version";
    version.type = MenuItemType::INFO;
    version.info_text = "Ares HDR Video Processor v1.0";
    info_tab.items.push_back(version);

    MenuItem input_info;
    input_info.id = "input_info";
    input_info.label = "Input";
    input_info.type = MenuItemType::INFO;
    input_info.info_text = "No input";
    info_tab.items.push_back(input_info);

    MenuItem output_info;
    output_info.id = "output_info";
    output_info.label = "Output";
    output_info.type = MenuItemType::INFO;
    output_info.info_text = "No output";
    info_tab.items.push_back(output_info);

    menu.tabs.push_back(info_tab);

    return menu;
}

} // namespace ares
