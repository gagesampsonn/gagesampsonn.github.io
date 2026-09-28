#!/usr/bin/env python3
"""Builds the self-contained browser demo.

Compiles the library, the capture reader and web/asterix_web.cpp to JavaScript
with Emscripten, generates the synthetic demo feed, and inlines both into
web/demo.html. The result is a single HTML file with no external dependencies
besides Google Fonts:

    source /path/to/emsdk/emsdk_env.sh
    python3 web/build.py            # writes build/web/asterix-demo.html
"""

import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default=str(ROOT / "build" / "web"), help="output directory")
    args = parser.parse_args()
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    compiler = shutil.which("em++")
    if compiler is None:
        sys.exit("em++ not found: install the Emscripten SDK and source emsdk_env.sh")
    sources = sorted(str(p) for p in (ROOT / "src").glob("*.cpp"))
    sources += [str(ROOT / "tools" / "capture.cpp"), str(ROOT / "web" / "asterix_web.cpp")]
    module_js = out / "asterix.js"
    subprocess.run([
        compiler, "-std=c++17", "-O2", "-Wall", "-Wextra",
        f"-I{ROOT / 'include'}", f"-I{ROOT / 'tools'}", *sources,
        "-fexceptions",
        "-sWASM=0",                  # plain JavaScript: no WebAssembly compilation needed at runtime
        "-sMODULARIZE=1", "-sEXPORT_NAME=createAsterixModule", "-sENVIRONMENT=web",
        "-sALLOW_MEMORY_GROWTH=1", "-sDYNAMIC_EXECUTION=0", "-sFILESYSTEM=0",
        "-sEXPORTED_FUNCTIONS=_asterix_web_decode,_asterix_web_count,_asterix_web_alloc,_asterix_web_free,_asterix_web_version",
        "-sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPU8",
        "-Wno-deprecated",
        "-o", str(module_js),
    ], check=True)

    feed = subprocess.run([sys.executable, str(ROOT / "web" / "make_demo_feed.py")],
                          check=True, capture_output=True, text=True).stdout
    module = module_js.read_text()
    for marker in ("</script", "<!--", "<script"):
        if marker in module.lower() or marker in feed.lower():
            sys.exit(f"cannot inline: generated code contains {marker!r}")

    page = (ROOT / "web" / "demo.html").read_text()
    for placeholder in ("/*@ASTERIX_MODULE@*/", "/*@DEMO_FEED@*/"):
        if placeholder not in page:
            sys.exit(f"placeholder {placeholder} missing from demo.html")
    page = page.replace("/*@ASTERIX_MODULE@*/", module).replace("/*@DEMO_FEED@*/", feed)
    target = out / "asterix-demo.html"
    target.write_text(page)
    print(f"wrote {target} ({len(page.encode()) / 1024:.0f} KiB)")


if __name__ == "__main__":
    main()
