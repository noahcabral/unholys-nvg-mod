"""Assemble the Romeo release from repository source and local game assets."""

from argparse import ArgumentParser
from hashlib import sha256
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile
import json


ROOT = Path(__file__).resolve().parent


def checked(data, expected, description):
    if sha256(data).hexdigest() != expected:
        raise ValueError(f"Unexpected revision of {description}. Use the matching donor files.")
    return data


def apply_shader_edits(raw, spec, name):
    source = raw.decode("utf-8-sig").replace("\r\n", "\n").encode("utf-8")
    lines = checked(source, spec["input_sha256"], name).decode("utf-8").splitlines(keepends=True)
    output = []
    cursor = 0
    for start, stop, replacement in spec["edits"]:
        if not cursor <= start <= stop <= len(lines):
            raise ValueError(f"Invalid shader edit in {name}")
        output.extend(lines[cursor:start])
        output.append(replacement)
        cursor = stop
    output.extend(lines[cursor:])
    return checked("".join(output).encode("utf-8"), spec["output_sha256"], f"generated {name}")


def build(pip, scopes, ssfx, assets, output, merge_renderer=False):
    manifest = json.loads((ROOT / "build-inputs.json").read_text(encoding="utf-8"))
    files = {
        p.relative_to(ROOT / "src").as_posix(): p.read_bytes()
        for p in (ROOT / "src").rglob("*") if p.is_file()
    }
    with ZipFile(pip) as pip_zip, ZipFile(assets) as asset_zip:
        if merge_renderer:
            renderer_files = {name: pip_zip.read(name) for name in pip_zip.namelist() if name.startswith("gamedata/")}
            files = renderer_files | files
        for name, spec in manifest["donors"].items():
            source_name = spec.get("source", name)
            if spec["donor"] == "pip":
                raw = pip_zip.read(source_name)
            elif spec["donor"] == "3dss":
                raw = (scopes / source_name).read_bytes()
            elif spec["donor"] == "ssfx":
                raw = (ssfx / source_name).read_bytes()
            else:
                raise ValueError(f"Unknown donor for {name}")
            files[name] = apply_shader_edits(raw, spec, name)
        for name, sha256sum in manifest["local_assets"].items():
            files[name] = checked(asset_zip.read(name), sha256sum, name)

    install_order = "Screen Space Shaders" if merge_renderer else "Screen Space Shaders and Unholy's PiP Optics Mod"
    files["README.txt"] = (
        "Unholy's nvg mod\n\n"
        "Install the ZIP in Mod Organizer 2 and enable it after 3D Shader Scopes, "
        f"{install_order} in the Romeo profile.\n"
        "Use the model assets for noncommercial purposes and keep the attached credits.\n"
        "Use xrRazom DX11 with Shader Bus 7, disable MSAA, and use r__3Dfakescope 1.\n"
        "Equip a PVS-69 in the headlamp slot and use your night-vision binding (N by default).\n"
    ).encode("utf-8")
    files["ATTRIBUTION.md"] = (ROOT / "ATTRIBUTION.md").read_bytes()
    with ZipFile(output, "x", ZIP_DEFLATED, compresslevel=6) as built:
        for name, data in sorted(files.items()):
            built.writestr(name, data)
    with ZipFile(output) as built:
        if built.testzip() is not None or set(built.namelist()) != files.keys():
            raise ValueError("The generated archive failed verification.")
    print(f"Built and verified {len(files)} files in {output}.")


def main():
    parser = ArgumentParser(description=__doc__)
    render_input = parser.add_mutually_exclusive_group(required=True)
    render_input.add_argument("--pip", type=Path, help="Matching Romeo PiP Optics overlay ZIP or engine bundle")
    render_input.add_argument("--romeo", type=Path, help="Installation folder containing Anomaly and GAMMA")
    parser.add_argument("--base-gamedata", type=Path, help="Unpacked Anomaly configs and scripts for a local Romeo build")
    parser.add_argument("--base-shaders", type=Path, help="Unpacked stock shaders, if outside base gamedata")
    parser.add_argument("--profile", default="ROMEO - 2026-11-09-001", help="MO2 profile to use when building from Romeo")
    parser.add_argument("--scopes", type=Path, required=True, help="3D Shader Scopes for GAMMA mod directory")
    parser.add_argument("--ssfx", type=Path, required=True, help="Screen Space Shaders Update 23 mod directory")
    parser.add_argument("--assets", type=Path, required=True, help="Authorized local ZIP containing the compiled NVG meshes and textures")
    parser.add_argument("--output", type=Path, required=True, help="New MO2 ZIP path")
    options = parser.parse_args()
    try:
        if options.pip:
            build(options.pip, options.scopes, options.ssfx, options.assets, options.output)
        else:
            if not options.base_gamedata:
                parser.error("--romeo requires --base-gamedata")
            with TemporaryDirectory(prefix="pvs69-build-", dir=options.output.resolve().parent) as directory:
                renderer = Path(directory) / "romeo-renderer.zip"
                command = [sys.executable, str(ROOT / "tools/build_3dss_overlay.py"), str(options.romeo),
                           str(options.base_gamedata), str(renderer), "--profile", options.profile,
                           "--exclude-mod", "Romeo - PVS-69", "--exclude-mod", "Unholy's PVS-69 Night Vision Mod", "--exclude-mod", "Unholy's nvg mod"]
                if options.base_shaders:
                    command.extend(("--base-shaders", str(options.base_shaders)))
                subprocess.run(command, check=True)
                build(renderer, options.scopes, options.ssfx, options.assets, options.output, merge_renderer=True)
    except (FileNotFoundError, FileExistsError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
