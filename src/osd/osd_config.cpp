#include <ares/osd_config.h>
#include <ares/processing_config.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

namespace ares {

// Get primary IP address of the system
static std::string getIPAddress() {
    struct ifaddrs* ifaddr;
    struct ifaddrs* ifa;
    std::string ip_address = "Not connected";

    if (getifaddrs(&ifaddr) == -1) {
        return ip_address;
    }

    // Iterate through interfaces
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        // Check for IPv4 address
        if (ifa->ifa_addr->sa_family == AF_INET) {
            // Skip loopback
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;

            // Get IP address
            void* addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char address_buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, addr, address_buffer, INET_ADDRSTRLEN);

            // Prefer ethernet (eth0, enp*) over wifi (wlan0, wlp*)
            if (strncmp(ifa->ifa_name, "eth", 3) == 0 ||
                strncmp(ifa->ifa_name, "enp", 3) == 0) {
                ip_address = std::string(address_buffer) + " (" + ifa->ifa_name + ")";
                break;
            } else if (strncmp(ifa->ifa_name, "wlan", 4) == 0 ||
                       strncmp(ifa->ifa_name, "wlp", 3) == 0) {
                ip_address = std::string(address_buffer) + " (" + ifa->ifa_name + ")";
                // Don't break, keep looking for ethernet
            }
        }
    }

    freeifaddrs(ifaddr);
    return ip_address;
}

// Create default OSD menu structure (madVR Envy style)
OSDMenuStructure createDefaultOSDMenu() {
    OSDMenuStructure menu;

    // Tab 1: Processing
    Menu processing_tab;
    processing_tab.id = "processing";
    processing_tab.title = "Processing";

    MenuItem black_bars_enable;
    black_bars_enable.id = "black_bars_enable";
    black_bars_enable.label = "Black Bar Detection";
    black_bars_enable.type = MenuItemType::TOGGLE;
    black_bars_enable.tooltip = "Automatically detect and crop black bars";
    processing_tab.items.push_back(black_bars_enable);

    MenuItem black_bars_crop;
    black_bars_crop.id = "black_bars_crop";
    black_bars_crop.label = "  Auto Crop";
    black_bars_crop.type = MenuItemType::TOGGLE;
    black_bars_crop.tooltip = "Automatically crop detected black bars";
    processing_tab.items.push_back(black_bars_crop);

    menu.tabs.push_back(processing_tab);

    // Tab 2: NLS (Non-Linear Stretch)
    Menu nls_tab;
    nls_tab.id = "nls";
    nls_tab.title = "NLS";

    MenuItem nls_enable;
    nls_enable.id = "nls_enable";
    nls_enable.label = "Enable NLS";
    nls_enable.type = MenuItemType::TOGGLE;
    nls_enable.tooltip = "Enable non-linear stretch for cinemascope screens";
    nls_tab.items.push_back(nls_enable);

    MenuItem nls_target_aspect;
    nls_target_aspect.id = "nls_target_aspect";
    nls_target_aspect.label = "Target Aspect Ratio";
    nls_target_aspect.type = MenuItemType::ENUM;
    nls_target_aspect.enum_options = {"2.35:1", "2.40:1", "2.55:1", "Custom"};
    nls_target_aspect.tooltip = "Target aspect ratio for stretch";
    nls_tab.items.push_back(nls_target_aspect);

    MenuItem nls_h_stretch;
    nls_h_stretch.id = "nls_h_stretch";
    nls_h_stretch.label = "Horizontal Stretch";
    nls_h_stretch.type = MenuItemType::SLIDER;
    nls_h_stretch.min_value = 0.0f;
    nls_h_stretch.max_value = 1.0f;
    nls_h_stretch.step = 0.05f;
    nls_h_stretch.tooltip = "Horizontal stretch amount (0.0-1.0)";
    nls_tab.items.push_back(nls_h_stretch);

    MenuItem nls_v_stretch;
    nls_v_stretch.id = "nls_v_stretch";
    nls_v_stretch.label = "Vertical Stretch";
    nls_v_stretch.type = MenuItemType::SLIDER;
    nls_v_stretch.min_value = 0.0f;
    nls_v_stretch.max_value = 1.0f;
    nls_v_stretch.step = 0.05f;
    nls_v_stretch.tooltip = "Vertical stretch amount (0.0-1.0)";
    nls_tab.items.push_back(nls_v_stretch);

    MenuItem nls_center_protect;
    nls_center_protect.id = "nls_center_protect";
    nls_center_protect.label = "Center Protection";
    nls_center_protect.type = MenuItemType::SLIDER;
    nls_center_protect.min_value = 0.1f;
    nls_center_protect.max_value = 6.0f;
    nls_center_protect.step = 0.1f;
    nls_center_protect.tooltip = "Power curve: higher = more stretch at edges, less in center";
    nls_tab.items.push_back(nls_center_protect);

    MenuItem nls_crop_amount;
    nls_crop_amount.id = "nls_crop_amount";
    nls_crop_amount.label = "Crop Amount";
    nls_crop_amount.type = MenuItemType::SLIDER;
    nls_crop_amount.min_value = 0.0f;
    nls_crop_amount.max_value = 1.0f;
    nls_crop_amount.step = 0.05f;
    nls_crop_amount.tooltip = "Crop edges before stretch (reduces distortion)";
    nls_tab.items.push_back(nls_crop_amount);

    MenuItem nls_bars_amount;
    nls_bars_amount.id = "nls_bars_amount";
    nls_bars_amount.label = "Black Bars Amount";
    nls_bars_amount.type = MenuItemType::SLIDER;
    nls_bars_amount.min_value = 0.0f;
    nls_bars_amount.max_value = 1.0f;
    nls_bars_amount.step = 0.05f;
    nls_bars_amount.tooltip = "Add black bars/padding (reduces distortion)";
    nls_tab.items.push_back(nls_bars_amount);

    MenuItem nls_interpolation;
    nls_interpolation.id = "nls_interpolation";
    nls_interpolation.label = "Interpolation Quality";
    nls_interpolation.type = MenuItemType::ENUM;
    nls_interpolation.enum_options = {"Bilinear", "Bicubic", "Lanczos"};
    nls_interpolation.tooltip = "Interpolation quality (higher = better but slower)";
    nls_tab.items.push_back(nls_interpolation);

    menu.tabs.push_back(nls_tab);

    // Tab 3: Tone Mapping
    Menu tone_mapping_tab;
    tone_mapping_tab.id = "tone_mapping";
    tone_mapping_tab.title = "Tone Map";

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

    MenuItem ip_address;
    ip_address.id = "ip_address";
    ip_address.label = "IP Address";
    ip_address.type = MenuItemType::INFO;
    ip_address.info_text = getIPAddress();
    ip_address.tooltip = "Use this IP to SSH into the system";
    info_tab.items.push_back(ip_address);

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

    MenuItem gpu_info;
    gpu_info.id = "gpu_info";
    gpu_info.label = "GPU";
    gpu_info.type = MenuItemType::INFO;
    gpu_info.info_text = "NVIDIA GPU";
    info_tab.items.push_back(gpu_info);

    MenuItem hdr_status;
    hdr_status.id = "hdr_status";
    hdr_status.label = "HDR Status";
    hdr_status.type = MenuItemType::INFO;
    hdr_status.info_text = "Detecting...";
    hdr_status.tooltip = "Current HDR mode (SDR/HDR10/HLG/DV)";
    info_tab.items.push_back(hdr_status);

    MenuItem color_space;
    color_space.id = "color_space";
    color_space.label = "Color Space";
    color_space.type = MenuItemType::INFO;
    color_space.info_text = "Auto (BT.709/BT.2020)";
    color_space.tooltip = "Automatically selected based on HDR metadata";
    info_tab.items.push_back(color_space);

    menu.tabs.push_back(info_tab);

    return menu;
}

} // namespace ares
