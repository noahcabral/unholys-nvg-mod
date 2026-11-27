from argparse import ArgumentParser
from collections import Counter
from csv import DictReader
from pathlib import Path
import re
from zipfile import ZIP_DEFLATED, ZipFile

from catalog import build_catalog, emit_catalog, read, read_config, resolved_files, roots


HERE = Path(__file__).resolve().parent
PROFILE = "ROMEO - 2026-11-09-001"


def replace_once(source, original, replacement):
    if source.count(original) != 1:
        raise ValueError(f"Expected one match for {original!r}, found {source.count(original)}")
    return source.replace(original, replacement, 1)


LANES = """
#if defined(SHADER_BUS) && SHADER_BUS >= 7
uniform float4 bus_romeo_pip;
uniform float4 bus_romeo_pip_zoom;
uniform float4 m_blender_mode;
Texture2D s_romeo_viewport;

bool romeo_pip_active()
{
    return bus_romeo_pip.x > 0.5 && m_blender_mode.z > 0.5 && m_hud_params.w < 0.5;
}
#endif
"""


def lens_shader(source):
    source = replace_once(source, "float4 ogse_c_screen;", "float4 ogse_c_screen;\n" + LANES)
    source = replace_once(
        source, "float max_zoom)", "float max_zoom, float optical_fov)",
    )
    source = replace_once(
        source, "smoothstep(min_zoom, max_zoom, ogse_c_screen.x)",
        "smoothstep(min_zoom, max_zoom, optical_fov)",
    )
    source = replace_once(
        source, "float MAX_ZOOM_FOV = m_hud_fov_params.x * 0.75;",
        """float MAX_ZOOM_FOV = m_hud_fov_params.x * 0.75;
    float optical_fov = ogse_c_screen.x;
    bool pip_active = false;
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    pip_active = romeo_pip_active();
    if (pip_active) {
        optical_fov = bus_romeo_pip.z;
        MIN_ZOOM_FOV = bus_romeo_pip_zoom.x;
        MAX_ZOOM_FOV = bus_romeo_pip_zoom.y;
    }
#endif""",
    )
    source = replace_once(
        source, "float current_zoom = max(MIN_ZOOM_FOV / ogse_c_screen.x, 1);",
        "float current_zoom = max(MIN_ZOOM_FOV / optical_fov, 1);",
    )
    source = replace_once(
        source, "(ogse_c_screen.x - MIN_ZOOM_FOV)", "(optical_fov - MIN_ZOOM_FOV)",
    )
    source = replace_once(
        source, "sample_zoom_switch_shadow(reticle_lens_tc, MIN_ZOOM_FOV, MAX_ZOOM_FOV)",
        "sample_zoom_switch_shadow(reticle_lens_tc, MIN_ZOOM_FOV, MAX_ZOOM_FOV, optical_fov)",
    )
    source = replace_once(
        source, "\tfloat3 back;\n\tif ((IMAGE_TYPE",
        """\tfloat3 back;
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    if (pip_active) {
        float2 pip_tc = saturate(0.5 + (scope_tc - 0.5) * bus_romeo_pip.y);
        bool thermal = (IMAGE_TYPE == IT_THERMAL || IMAGE_TYPE == IT_THERMAL_COLOR)
            && markswitch_current.x < 2;
        if (thermal && SETTING(SETTINGS, ST_THERMAL_PIXELATION)) {
            float pixelate = max(1, floor(current_zoom));
            pip_tc = (floor(pip_tc * screen_res.xy / pixelate) * pixelate + 0.5) / screen_res.xy;
        }
        back = s_romeo_viewport.Sample(smp_base, saturate(pip_tc)).rgb;
        if (thermal) {
            if (markswitch_current.x == 1) back = 1 - back;
            if (IMAGE_TYPE == IT_THERMAL_COLOR)
                back = s_heat_map.Sample(smp_base, float2(back.x, 0.5));
        } else if (IMAGE_TYPE != IT_NV || markswitch_current.x != 0) {
            back *= LENS_COLOR;
        }
    } else
#endif
    if ((IMAGE_TYPE""",
    )
    source = replace_once(
        source, """if (IMAGE_TYPE == IT_NV && markswitch_current.x == 0) {
\t\tback = apply_nvg(scope_tc, back);
\t}
\tback = apply_white_fusion(scope_tc, back);""",
        """if (!pip_active) {
        if (IMAGE_TYPE == IT_NV && markswitch_current.x == 0)
            back = apply_nvg(scope_tc, back);
        back = apply_white_fusion(scope_tc, back);
    }""",
    )
    return source


def pip_postprocess_helpers(lens):
    # Reuse Romeo's tube, fusion and battery functions against the current
    # render's heat and position targets instead of the main view's targets.
    settings = lens[lens.index("#define IT_NONE"):lens.index("Texture2D s_prev_frame;")]
    charge = lens[lens.index("float thermal_battery_charge()"):lens.index("float thermal_power_signal")]
    powered = lens[lens.index("float scope_nvg_heat("):lens.index("float3 lcd_effect(")]
    powered = powered.replace("s_prev_frame.SampleLevel(smp_base,", "s_base0.SampleLevel(smp_rtlinear,")
    return """#ifndef ROMEO_PIP_SENSOR_H
#define ROMEO_PIP_SENSOR_H
#if defined(SHADER_BUS) && SHADER_BUS >= 7
#include "thermal_utils.h"
uniform float4 bus_romeo_pip;
uniform float4 ssfx_issvp;
uniform float4 s3ds_param_2;
uniform float4 s3ds_param_3;
uniform float4 s3ds_param_4;
uniform float4 markswitch_current;
""" + settings + charge + powered + """
float3 romeo_pip_process_sensor(float2 pixel, float3 image)
{
    float2 tc = pixel * screen_res.zw;
    int image_type = s3ds_param_3.x;
    if ((image_type == IT_THERMAL || image_type == IT_THERMAL_COLOR) && markswitch_current.x < 2) {
        gbuffer_data data = gbuffer_load_data(tc, pixel, 0);
        return infrared(data, pixel, tc);
    }
    if (image_type == IT_NV) {
        if (markswitch_current.x == 0)
            image = apply_nvg(tc, image * s3ds_param_4.xyz);
        return apply_white_fusion(tc, image);
    }
    return image;
}
#endif
#endif
"""


def postprocess_shader(source):
    signature = re.search(r"float4\s+main\s*\(\s*p_postpr\s+(\w+)\s*\)\s*:\s*SV_Target", source)
    if not signature:
        raise ValueError("Unrecognized Romeo postprocess entry point")
    entry = signature[0].replace(")", ", float4 pip_pixel : SV_Position )", 1)
    source = source[:signature.start()] + '#include "romeo_pip_sensor.h"\n\n' + entry + source[signature.end():]
    outputs = list(re.finditer(r"^\s*image\s*=\s*HDR10_ToDisplay_World\(image\);", source, flags=re.M))
    if len(outputs) != 1:
        raise ValueError("Unrecognized Romeo display color transform")
    index = outputs[0].start()
    return source[:index] + """
#if defined(SHADER_BUS) && SHADER_BUS >= 7
    // The lens is composited before the primary view's display transform.
    if (ssfx_issvp.x > 0.5 && bus_romeo_pip.x > 0.5)
        return float4(romeo_pip_process_sensor(pip_pixel.xy, image), 1);
#endif
""" + source[index:]


def keep_primary_exposure(source):
    source = replace_once(source, "float4\tMiddleGray;", """float4\tMiddleGray;
#if defined(SHADER_BUS) && SHADER_BUS >= 7
uniform float4 bus_romeo_pip;
uniform float4 ssfx_issvp;
#endif""")
    return replace_once(source, "\t// sample", """#if defined(SHADER_BUS) && SHADER_BUS >= 7
    if (ssfx_issvp.x > 0.5 && bus_romeo_pip.x > 0.5)
        return s_tonemap.SampleLevel(smp_nofilter, I.Tex0, 0).xxxx;
#endif
\t// sample""")


def pack(dirs, overlay, optics, catalog):
    relatives = (
        "shaders/r3/models_scope_reticle.ps", "shaders/r3/models_scope_reticle.s",
        "shaders/r3/models_scope_reticle_precise.ps", "shaders/r3/models_scope_reticle_precise.s",
        "shaders/r3/postprocess.ps", "shaders/r3/postprocess_cm.ps", "shaders/r3/postprocess.s",
        "shaders/r3/bloom_luminance_3.ps",
        "scripts/zzz_mspizza_Godis_ZoomCalc.script", "scripts/magnifier_switch.script",
    )
    providers = resolved_files(tuple(dirs), relatives)

    def effective(relative):
        path = providers.get(relative)
        if path is None:
            raise FileNotFoundError(relative)
        return read(path)

    files = {
        "configs/mod_system_zzzzzzzz_romeo_pip_optics.ltx": overlay,
        "configs/romeo_pip_optics.ltx": optics,
        "scripts/romeo_pip.script": read(HERE / "romeo_pip.script"),
    }
    zoom_script = "scripts/zzz_mspizza_Godis_ZoomCalc.script"
    source = effective(zoom_script)
    source = replace_once(source, "\n/*\n", "\n--[=[\n")
    source = replace_once(source, "\n*/\n", "\n]=]\n")
    files[zoom_script] = replace_once(
        source, "function actor_on_weapon_zoom_in(obj)",
        "function actor_on_weapon_zoom_in(obj)\n\tif romeo_pip.apply_if_scoped(obj) then return end",
    )
    path = "scripts/magnifier_switch.script"
    source = effective(path)
    for factor in ("zoom_factor", "pending.zoom_factor"):
        call = f"c_wpn:SetZoomFactor({factor})"
        source = replace_once(source, call, f"if not romeo_pip.apply_if_scoped(wpn) then {call} end")
    files[path] = source
    for lens in ("models_scope_reticle", "models_scope_reticle_precise"):
        path = f"shaders/r3/{lens}"
        source = effective(path + ".ps")
        files[path + ".ps"] = lens_shader(source)
        binding = 'shader:dx10texture("s_prev_frame", "$user$generic_temp")'
        files[path + ".s"] = replace_once(
            effective(path + ".s"), binding,
            binding + '\n\tshader:dx10texture("s_romeo_viewport", "$user$viewport2")',
        )

    files["shaders/r3/romeo_pip_sensor.h"] = pip_postprocess_helpers(source)
    for shader in ("postprocess.ps", "postprocess_cm.ps"):
        path = f"shaders/r3/{shader}"
        files[path] = postprocess_shader(effective(path))
    path = "shaders/r3/bloom_luminance_3.ps"
    files[path] = keep_primary_exposure(effective(path))

    path = "shaders/r3/postprocess.s"
    binding = 'shader:dx10texture\t("s_noise", "fx\\\\fx_noise2")'
    renderer = effective(path)
    if renderer.count(binding) != 2:
        raise ValueError("Unrecognized Romeo postprocess bindings")
    files[path] = renderer.replace(binding, binding + """
    shader:dx10texture("s_heat", "$user$heat")
    shader:dx10texture("s_position", "$user$position")
""")

    covered = sum(len(options) for _, options, _ in catalog.values())
    manual = (
        "Romeo PiP Optics\n"
        "Install the ZIP through Mod Organizer 2 in ROMEO - 2026-11-09-001. "
        "Enable it after 3DSS and screen-space shaders in the left pane, below them in priority order. "
        "Disable Romeo - True PiP Scope Preview if enabled. Restart the game when enabling or disabling this add-on.\n\n"
        f"Generated coverage: {len(catalog)} weapon sections and {covered} optic configurations. "
        "Use the existing magnification controls for variable zoom. The second camera renders the widest "
        "setting and projects progressively narrower fields of view for greater magnification. "
        "Existing reticles, sight models, lens effects, power modes, and alternative sight controls still use 3DSS. "
        "The second view requires Romeo's DX11 engine with Shader Bus 7. Both players should use the same mod list.\n"
    )
    return files, manual


def main():
    parser = ArgumentParser(description="Build a Romeo profile-specific PiP add-on.")
    parser.add_argument("romeo", type=Path)
    parser.add_argument("base_gamedata", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--profile", default=PROFILE)
    parser.add_argument("--base-shaders", type=Path, help="Base shader extraction, if separate from base gamedata")
    parser.add_argument("--exclude-mod", action="append", default=[], help="Ignore an existing PiP overlay in MO2")
    args = parser.parse_args()

    excluded_mods = {"Romeo - PiP Optics", "Romeo - True PiP Scope Preview", "Unholy's PiP Optics Mod"}
    excluded_mods.update(args.exclude_mod)
    dirs = roots(args.romeo, args.base_gamedata, args.profile, excluded_mods)
    if args.base_shaders:
        dirs.insert(0, args.base_shaders)
    config, visited = read_config(dirs)
    catalog, excluded = build_catalog(config)
    with (HERE.parent / "weapon-variants.tsv").open(encoding="utf-8", newline="") as reference:
        supported = {}
        for row in DictReader(reference, delimiter="\t"):
            supported.setdefault(row["Romeo weapon section"], set()).add(row["Attachment or optic section"])
    catalog = {name: data for name, data in catalog.items() if name in supported and set(data[1]) <= supported[name]}
    if not catalog or "wpn_ak74_pso1m21" not in catalog:
        raise SystemExit("Could not resolve the installed Romeo 3DSS profile.")
    overlay, optics = emit_catalog(catalog)
    counts = Counter((optic.image_type, optic.steps > 0)
                     for _, options, _ in catalog.values() for optic in options.values())
    print(f"{len(visited)} config files. {len(catalog)} weapon sections. Optics by (image type, variable): {counts}.")
    print(f"Excluded: {excluded}. Modular weapon sections: {sum(item[0] for item in catalog.values())}.")
    files, manual = pack(dirs, overlay, optics, catalog)
    with ZipFile(args.output, "x", ZIP_DEFLATED) as archive:
        for relative, data in sorted(files.items()):
            archive.writestr("gamedata/" + relative, data.encode("utf-8"))
        archive.writestr("README.txt", manual.encode("utf-8"))
    with ZipFile(args.output) as archive:
        if corrupt := archive.testzip():
            raise SystemExit(f"Corrupt ZIP member {corrupt}")
        print(f"{len(archive.namelist())} entries written to {args.output}")


if __name__ == "__main__":
    main()
