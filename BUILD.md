# Build and install for Romeo

The source can rebuild the existing 11-device Romeo add-on with local copies of the inputs below. Python 3.10 or newer is sufficient.

| Input | What it supplies |
| --- | --- |
| Romeo PiP Optics ZIP or the Romeo DX11 bundle | The three postprocess shader donors and their PiP sensor include. |
| 3D Shader Scopes for GAMMA mod directory | The four matching reflex-reticle shader pairs under gamedata/shaders/r3. |
| Screen Space Shaders Update 23 mod directory | The glass vertex shader that supplies the fitted housing and viewport variations. |
| Authorized local NVG asset ZIP | The compiled, rigged .ogf meshes and .dds textures under gamedata. The private Romeo PVS-69 archive is one valid input. |

```powershell
python build_romeo.py --pip "Romeo PiP Optics.zip" --scopes "410- 3DSS for GAMMA" --ssfx "190- Screen Space Shaders 23" --assets "Romeo PVS-69.zip" --output "Unholys PVS-69 Night Vision Mod - Romeo.zip"
```

1. Install the output as one MO2 mod. Set its priority after 3D Shader Scopes, Screen Space Shaders and PiP Optics so the combined shader patches win.
2. Disable any previous PVS-69 or PVS-69 Third-Person Preview entries. Use the matching xrRazom DX11 executable and keep MSAA off for the ECOTI postprocess.
3. Set `r__3Dfakescope 1` for the 3DSS reflex pass. Equip a device in the headlamp slot and use the night-vision binding, which defaults to **N**.
4. Use the same PVS-69 setup on all co-op PCs.

The builder checks local file fingerprints and writes a new archive without changing the installation. A mismatched donor points to a different shader or asset revision. Use the matching revision so the patch can preserve the renderer’s layout. The [compatibility notes](COMPATIBILITY.md) cover related prerequisites.
