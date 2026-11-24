"""Execute the actual screen shortcut guard and preview callbacks without a controller."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def method(source, name):
    match = re.search(r'(?:bool|void) DefaultUI::' + name + r'\([^)]*\)(?: const)?\s*\{', source)
    if not match:
        raise AssertionError('Missing production shortcut method: ' + name)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


class BrewShortcutTest(unittest.TestCase):
    def test_actual_guard_and_preview_callbacks(self):
        source = (ROOT / 'src/display/ui/default/DefaultUI.cpp').read_text()
        functions = '\n'.join(method(source, name) for name in (
            'canKnobBrowseProfiles', 'onKnobBrowseProfiles', 'onProfileSwitch', 'onNextProfile', 'onPreviousProfile'))
        harness = r'''
#include <algorithm>
#include <cassert>
#include <mutex>
#include <vector>
enum ScreensEnum { SCREEN_ID_BREW_SCREEN=2, SCREEN_ID_NEW_PROFILE_SCREEN=9, SCREEN_ID_MENU_SCREEN_NEW=4 };
enum class BrewScreenState { Brew, Settings };
constexpr int MODE_BREW=1, MODE_STEAM=2;
static int actualScreen=SCREEN_ID_BREW_SCREEN;
int eez_flow_get_current_screen() { return actualScreen; }
struct Client { bool connected=true; bool isConnected() { return connected; } };
struct Info { bool protocolMismatch=false; };
struct Controller {
    bool active=false, updating=false, ready=true, error=false; int mode=MODE_BREW; Client client; Info info;
    bool isActive() { return active; } bool isUpdating() { return updating; }
    bool isReady() { return ready; } bool isErrorState() { return error; } int getMode() { return mode; }
    Client *getClientController() { return &client; } Info &getSystemInfo() { return info; }
};
struct DefaultUI {
    Controller *controller; ScreensEnum currentScreen=SCREEN_ID_BREW_SCREEN, targetScreen=SCREEN_ID_BREW_SCREEN;
    BrewScreenState brewScreenState=BrewScreenState::Brew;
    bool welcomeActive=false, brewConfirmVisible=false, profileDetailsVisible=true, rerender=false;
    int currentProfileIdx=0, profileLoaded=1, transitions=0;
    std::mutex profilesMutex; std::vector<int> favoritedProfileIds{0,1,2};
    DefaultUI(Controller *c) : controller(c) {}
    void changeScreen(ScreensEnum screen) { targetScreen=screen; ++transitions; rerender=true; }
    bool canKnobBrowseProfiles() const; void onKnobBrowseProfiles(int);
    void onProfileSwitch(); void onNextProfile(); void onPreviousProfile();
};
'''
        harness += functions
        harness += r'''
int main() {
    Controller controller; DefaultUI ui(&controller);
    assert(ui.canKnobBrowseProfiles());
    ui.onKnobBrowseProfiles(0); assert(ui.transitions==0);
    ui.onKnobBrowseProfiles(1);
    assert(ui.transitions==1 && ui.targetScreen==SCREEN_ID_NEW_PROFILE_SCREEN);
    assert(ui.currentProfileIdx==1 && !ui.profileDetailsVisible && ui.rerender);
    // The callback cannot load a profile: no select API exists in this harness.
    ui.onKnobBrowseProfiles(1); assert(ui.transitions==1 && ui.currentProfileIdx==1); // transition already pending
    ui.targetScreen=SCREEN_ID_BREW_SCREEN; ui.onKnobBrowseProfiles(-1);
    assert(ui.transitions==2 && ui.currentProfileIdx==0); // first entry: no wrap
    ui.targetScreen=SCREEN_ID_BREW_SCREEN; ui.onKnobBrowseProfiles(1000);
    assert(ui.currentProfileIdx==2); // burst clamps at the list end
    ui.favoritedProfileIds={0}; ui.targetScreen=SCREEN_ID_BREW_SCREEN; ui.onKnobBrowseProfiles(1);
    assert(ui.currentProfileIdx==0); // one entry remains a preview
    ui.targetScreen=SCREEN_ID_BREW_SCREEN;
    auto blocked=[&] { int n=ui.transitions, index=ui.currentProfileIdx; assert(!ui.canKnobBrowseProfiles());
        ui.onKnobBrowseProfiles(1); assert(ui.transitions==n && ui.currentProfileIdx==index); };
    controller.active=true; blocked(); controller.active=false;
    controller.updating=true; blocked(); controller.updating=false;
    controller.error=true; blocked(); controller.error=false;
    controller.ready=false; blocked(); controller.ready=true;
    controller.client.connected=false; blocked(); controller.client.connected=true;
    controller.info.protocolMismatch=true; blocked(); controller.info.protocolMismatch=false;
    controller.mode=MODE_STEAM; blocked(); controller.mode=MODE_BREW;
    ui.welcomeActive=true; blocked(); ui.welcomeActive=false;
    ui.brewConfirmVisible=true; blocked(); ui.brewConfirmVisible=false;
    ui.profileLoaded=0; blocked(); ui.profileLoaded=1;
    ui.brewScreenState=BrewScreenState::Settings; blocked(); ui.brewScreenState=BrewScreenState::Brew;
    actualScreen=SCREEN_ID_MENU_SCREEN_NEW; blocked(); actualScreen=SCREEN_ID_BREW_SCREEN;
    ui.currentScreen=SCREEN_ID_MENU_SCREEN_NEW; blocked(); ui.currentScreen=SCREEN_ID_BREW_SCREEN;
    ui.targetScreen=SCREEN_ID_MENU_SCREEN_NEW; blocked(); ui.targetScreen=SCREEN_ID_BREW_SCREEN;
    assert(ui.canKnobBrowseProfiles());
}
'''
        with tempfile.TemporaryDirectory() as folder:
            cpp = Path(folder) / 'shortcut.cpp'
            executable = Path(folder) / 'shortcut'
            cpp.write_text(harness)
            subprocess.run(['clang++', '-std=c++17', '-fsanitize=address,undefined', '-g', str(cpp), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        self.assertIn('knobAdjustment.setProfileShortcut([this](int steps) { onKnobBrowseProfiles(steps); });', source)
        self.assertRegex(source, r'knobAdjustment\.update\([\s\S]*?canKnobBrowseProfiles\(\)\);')


if __name__ == '__main__':
    unittest.main()
