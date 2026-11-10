# Builds the browser version of Jack Volt.
#
#   python web/build.py [path to zig]
#
# It compiles the game to WebAssembly with Zig's C++ compiler (zig is free:
# https://ziglang.org, or "pip install ziglang"), then writes:
#   web/dist/            the installable web app: put these files on a web
#                        server (GitHub Pages serves docs/play/ from this repo)
#   web/JackVolt.html    one self-contained page, with the game inside it
import base64, os, shutil, subprocess, sys

here = os.path.dirname(os.path.abspath(__file__))
top = os.path.dirname(here)
zig = sys.argv[1] if len(sys.argv) > 1 else "zig"
if zig == "zig":
    try:
        import ziglang                                  # pip install ziglang
        zig = os.path.join(os.path.dirname(ziglang.__file__), "zig")
    except ImportError:
        pass

sources = ["web/platform.cpp", "game.cpp", "draw.cpp", "editor.cpp", "designer.cpp", "checker.cpp",
           "levels.cpp", "defaults.cpp", "art.cpp", "sound.cpp", "scores.cpp"]
dist = os.path.join(here, "dist")
os.makedirs(dist, exist_ok=True)
wasm = os.path.join(dist, "volt.wasm")
subprocess.check_call([zig, "c++", "-target", "wasm32-wasi", "-mexec-model=reactor", "-O2", "-s",
                       "-Iweb", "-fno-exceptions", "-o", wasm] + sources, cwd=top)

page = open(os.path.join(here, "index.html"), encoding="utf-8").read()
head = ('<!doctype html>\n<html lang="en">\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
        '<style>:root { padding-top: env(safe-area-inset-top, 0px); padding-bottom: env(safe-area-inset-bottom, 0px); } '
        'body { margin: 0; } [hidden] { display: none !important; }</style>\n')

# The installable app: the page fetches volt.wasm, and a service worker keeps
# everything so it works offline
app = head + page.replace("<!--PWA-->",
    '<link rel="manifest" href="manifest.webmanifest">\n<link rel="icon" href="icon-192.png">\n'
    '<link rel="apple-touch-icon" href="icon-192.png">\n<meta name="mobile-web-app-capable" content="yes">') \
  .replace("<!--SW-->",
    '<script>if ("serviceWorker" in navigator && location.protocol === "https:") '
    'navigator.serviceWorker.register("sw.js").catch(function () {});</script>') \
  .replace("/*WASM_BASE64*/", "")
open(os.path.join(dist, "index.html"), "w", encoding="utf-8").write(app)
for f in ("manifest.webmanifest", "sw.js", "icon-192.png", "icon-512.png"):
    shutil.copy(os.path.join(here, f), os.path.join(dist, f))

# The single page: the game goes inside it, as text
b64 = base64.b64encode(open(wasm, "rb").read()).decode()
single = page.replace("<!--PWA-->", "").replace("<!--SW-->", "") \
             .replace("/*WASM_BASE64*/", 'window.VOLT_WASM = "%s";' % b64)
open(os.path.join(here, "JackVolt.html"), "w", encoding="utf-8").write(single)
print("built: web/dist/ (%d KB of game) and web/JackVolt.html" % (os.path.getsize(wasm) // 1024))
