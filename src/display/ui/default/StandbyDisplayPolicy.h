#pragma once
#include <cstddef>
#include <cstring>
namespace gaggimate_ui {
enum class StandbyContent : unsigned char { Logo, Text, Clock };
constexpr std::size_t StandbyTextLimit=24;
struct StandbyAppearance {
    StandbyContent content=StandbyContent::Logo;
    char text[StandbyTextLimit+1]="GaggiMate";
};
inline const char *standbyContentName(StandbyContent content) {
    return content==StandbyContent::Text?"text":content==StandbyContent::Clock?"clock":"logo";
}
inline bool validStandbyContent(const char *text) {
    return text && (!std::strcmp(text,"logo") || !std::strcmp(text,"text") || !std::strcmp(text,"clock"));
}
inline bool validStandbyText(const char *text) {
    if(!text)return false;
    std::size_t length=0;
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(text);*p;++p) {
        if(*p<32 || *p>126 || ++length>StandbyTextLimit)return false;
    }
    return true;
}
} // namespace gaggimate_ui
