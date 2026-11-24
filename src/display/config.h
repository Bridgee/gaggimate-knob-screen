#ifndef CONFIG_H
#define CONFIG_H

#ifdef GAGGIMATE_KNOB
constexpr char WIFI_AP_SSID[] = "gaggimate_knob";
#else
constexpr char WIFI_AP_SSID[] = "GaggiMate";
#endif
constexpr char NTP_SERVER[] = "pool.ntp.org";

#endif
