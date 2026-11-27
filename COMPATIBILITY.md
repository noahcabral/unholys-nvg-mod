# Compatibility and feature status

## Romeo

- In-game DX11 captures cover Gen 1, Gen 2, Gen 3, monochrome digital, color digital and the Gen 3 quad. Other capture runs cover reflex ADS, helmet fits and local third-person eye glow.
- The deployed viewports sample the current main view. The analog viewports use distance-based focus and a short defocus transition. Digital uses camera-motion blur.
- ECOTI is a thermal-outline overlay using the renderer heat and position buffers. Both outlines and eyepiece skin lighting depend on the integrated final pass. The rifle and optic can still occlude the outline. The native rear sight may remain visible below the device.
- Canted ADS and remote-player visuals require runtime verification. The module guards xrRazom save hooks and keeps the spawn roll host-owned. Solo and co-op are separate verification targets.
- Helmets and actor visuals have individual fit offsets. The GP5 uses a different mount.

## Porting to GAMMA and standalone Anomaly

The engine API baseline is DX11, Shader Bus 7, script attachments for the camera and world, and a renderer heat target. The Monolith source exposes those primitives. Full parity also requires the effective active shader stack and gameplay hooks. [Monolith engine source](https://github.com/themrdemonized/xray-monolith)

- Port the postprocess bindings using the target stack’s color, position and heat buffers. The stock Anomaly postprocess predates the exact Romeo HDR and PiP code paths.
- On profiles without 3D Shader Scopes, add an equivalent reflex reticle pass for right-tube ADS. A GAMMA 3DSS donor alone does not supply the full Romeo integration.
- Replace the xrRazom-specific trade-window rewrites with the equivalent hooks in the target profile. Then test prices, both supply tiers and Gen 3 restrictions through checkout.
- Verify the target profile’s NPC spawn and death-loot path with the ranks and item catalog actually installed. Confirm existing inventories, fixed equipment and quest rewards are retained.
- Verify the wearer fits and local eye glow against the target profile’s player and helmet models. Camera and world attachments have different model and bone requirements.

Compatibility depends on those last-mile integrations. The local build recipe targets the exact Romeo revisions listed in the [build instructions](BUILD.md).
