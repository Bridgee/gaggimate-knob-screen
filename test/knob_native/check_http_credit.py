"""Exercise the pinned server's actual optional gate, not a rewritten algorithm.

A final ACK can leave zero credits while the send window is temporarily small;
a later poll with a recovered window still cannot advance. This host scenario
explains the selected workaround; it does not prove a device's network cause.
"""
from pathlib import Path
import configparser
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / '.pio/libdeps/display-knob/ESPAsyncWebServer/src/WebResponses.cpp').read_text()
start = source.index('size_t AsyncAbstractResponse::write_send_buffs(')
match = re.search(r'#if ASYNCWEBSERVER_USE_CHUNK_INFLIGHT\n(.*?)\n#endif', source[start:], re.S)
if not match:
    raise RuntimeError('HTTP gate changed; review target transport configuration')
config = configparser.ConfigParser(interpolation=None)
config.read(ROOT / 'platformio.ini')
assert '-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT=0' in config['env:display-knob']['build_flags']
build = ROOT / 'dist/http-credit-check'
build.mkdir(parents=True, exist_ok=True)
cpp = '''#include <algorithm>
#include <cassert>
#include <cstddef>
#define CONFIG_LWIP_TCP_WND_DEFAULT 5760
#define ASYNC_RESPONCE_BUFF_SIZE 2920
struct Client { size_t window; size_t space() { return window; } };
struct Request { Client value; Client *client() { return &value; } };
struct Gate {
    unsigned _in_flight_credit=0;
    size_t _in_flight=100, _sentLength=6000;
    bool _chunked=false, _sendContentLength=true;
    bool advance(size_t len,size_t window) {
        (void)len; Request object{{window}}; auto *request=&object; (void)request;
#if ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
''' + match[1] + '''
#endif
        return true;
    }
};
int main() {
    Gate gate;
#if ASYNCWEBSERVER_USE_CHUNK_INFLIGHT
    assert(!gate.advance(100,ASYNC_RESPONCE_BUFF_SIZE-1));
    assert(gate._in_flight==0 && gate._in_flight_credit==0);
    for(int i=0;i<10;++i) assert(!gate.advance(0,ASYNC_RESPONCE_BUFF_SIZE*2));
#else
    assert(gate.advance(100,ASYNC_RESPONCE_BUFF_SIZE-1));
    for(int i=0;i<10;++i) assert(gate.advance(0,ASYNC_RESPONCE_BUFF_SIZE*2));
#endif
}
'''
path = build / 'gate.cpp'
path.write_text(cpp)
for enabled in (1,0):
    binary = build / ('gate-' + str(enabled))
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-DASYNCWEBSERVER_USE_CHUNK_INFLIGHT='+str(enabled),str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS: actual pinned gate starvation reproduced; K718 disables that optional gate. Device transfer qualification remains separate.')
