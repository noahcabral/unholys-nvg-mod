# Build and install for Romeo

Download the **Romeo Build Kit** and **Noncommercial Model Assets** from [Releases](https://github.com/noahcabral/unholys-nvg-mod/releases). Extract the build kit. Keep the model assets in their ZIP for the builder. Python 3.10 or newer is sufficient.

| Input | What it supplies |
| --- | --- |
| Romeo installation containing `Anomaly` and `GAMMA` | The selected Romeo profile and matching renderer input files. |
| Unpacked Anomaly configs, scripts, and shaders | Use the unpacker supplied in `Anomaly/tools`. If shaders are separate, pass their unpacked directory with `--base-shaders`. |
| 3D Shader Scopes for GAMMA mod directory | The four matching reflex-reticle shader pairs under gamedata/shaders/r3. |
| Screen Space Shaders Update 23 mod directory | The glass vertex shader that supplies the fitted housing and viewport variations. |
| Noncommercial Model Assets ZIP | Compiled, rigged `.ogf` meshes and `.dds` textures for all 11 devices. |

Run `db_unpacker.bat` in `Anomaly/tools` for the configs and scripts. In the same directory, unpack the stock shaders into a separate folder. For example, from PowerShell run `./converter.exe -unpack -xdb ../db/shaders.db0 -dir D:/Romeo/unpacked-shaders`.

```powershell
python build_romeo.py --romeo "D:\Romeo" --base-gamedata "D:\Romeo\Anomaly\tools\_unpacked" --base-shaders "D:\Romeo\unpacked-shaders" --scopes "D:\Romeo\GAMMA\mods\410- 3DSS for GAMMA" --ssfx "D:\Romeo\GAMMA\mods\190- Screen Space Shaders 23" --assets "Unholys nvg mod - Noncommercial Model Assets.zip" --output "Unholys nvg mod - Romeo.zip"
```

Use the exact mod directory names from your install. The builder defaults to `ROMEO - 2026-11-09-001`. For another MO2 profile, pass its exact name with `--profile`. To use an existing matching Romeo PiP ZIP, replace `--romeo`, `--base-gamedata`, and `--base-shaders` with `--pip "Romeo PiP Optics.zip"` and keep that separate PiP overlay enabled.

1. Install the output as one MO2 mod. Set its priority after 3D Shader Scopes and Screen Space Shaders. If you selected `--pip`, place the output after the existing PiP overlay.
2. Disable any previous PVS-69 or PVS-69 Third-Person Preview entries. With `--romeo`, also disable earlier PiP Optics entries because the generated ZIP includes the matching scope overlay. Use the matching xrRazom DX11 executable and keep MSAA off for the ECOTI postprocess.
3. Set `r__3Dfakescope 1` for the 3DSS reflex pass. Equip a device in the headlamp slot and use the night-vision binding, which defaults to **N**.
4. Use the same PVS-69 setup on all co-op PCs.

The builder checks local file fingerprints and writes a new archive without changing the installation. A mismatched donor points to a different shader or asset revision. Use the matching revision so the patch can preserve the renderer’s layout. The [compatibility notes](COMPATIBILITY.md) cover related prerequisites.
