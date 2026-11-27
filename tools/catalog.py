from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from fnmatch import fnmatchcase
from functools import cache
from pathlib import Path, PurePosixPath
import math
import os
import re


SECTION = re.compile(r"^([!@-]?)\[([^\]]+)\]\s*(?::\s*(.*))?$")
INCLUDE = re.compile(r'^#include\s+"([^"]+)"', re.I)
LIST_UPDATE = re.compile(r"^([<>])\s*(.*?)\s*=\s*(.*)$")


def read(path):
    if path is None:
        return ""
    raw = path.read_bytes()
    try:
        return raw.decode("utf-8-sig").replace("\r\n", "\n")
    except UnicodeDecodeError:
        return raw.decode("cp1251", errors="replace").replace("\r\n", "\n")


def roots(install, vanilla, profile, excluded=()):
    gamma = install / "GAMMA"
    enabled = [
        line[1:] for line in read(gamma / "profiles" / profile / "modlist.txt").splitlines()
        if line.startswith("+") and not line.endswith("_separator") and line[1:] not in excluded
    ]
    return [vanilla, install / "Anomaly/gamedata"] + [
        gamma / "mods" / name / "gamedata" for name in reversed(enabled)
    ] + [gamma / "overwrite/gamedata"]


def find_file(dirs, relative):
    return resolved_files(tuple(dirs), (relative,)).get(relative)


@cache
def resolved_files(dirs, relatives):
    def scan(root):
        return {key: root / key for key in relatives if (root / key).is_file()}

    result = {}
    with ThreadPoolExecutor(max_workers=16) as executor:
        for files in executor.map(scan, dirs):
            result.update(files)
    return result


def tokens(value):
    return [part.strip() for part in value.split(",") if part.strip()]


def enabled(value):
    return value.lower() in {"true", "on", "1", "yes"}


def number(value, default=0.0):
    try:
        value = float(value)
        if math.isfinite(value):
            return value
    except (TypeError, ValueError):
        pass
    return default


def read_config(dirs, section_origins=None):
    def scan(root):
        config = root / "configs"
        found = {}
        for folder, _, names in os.walk(config):
            for name in names:
                if name.casefold().endswith(".ltx"):
                    path = Path(folder) / name
                    found[path.relative_to(config).as_posix().lower()] = path
        return found

    paths = {}
    with ThreadPoolExecutor(max_workers=16) as executor:
        for found in executor.map(scan, dirs):
            paths.update(found)

    def includes(key, target):
        target = target.replace("\\", "/").lower()
        relative = (PurePosixPath(key).parent / target).as_posix()
        if "*" in relative or "?" in relative:
            return sorted(p for p in paths if fnmatchcase(p, relative))
        if relative in paths:
            return [relative]
        return [target] if target in paths else []

    top_level = sorted(
        key for key in paths if (key.startswith("mod_system_") and "/" not in key)
        or fnmatchcase(key, "items/settings/*magnifier_switch_mappings*.ltx")
    )
    pending = set(top_level) | {"system.ltx"}
    contents = {}
    with ThreadPoolExecutor(max_workers=16) as executor:
        while pending:
            batch = sorted(pending)
            pending.clear()
            for key, source in zip(batch, executor.map(lambda key: read(paths[key]), batch)):
                contents[key] = source
                for line in source.splitlines():
                    if inc := INCLUDE.match(line.strip()):
                        pending.update(includes(key, inc[1]))
            pending.difference_update(contents)

    local, parents, list_updates, defined, seen = {}, {}, {}, set(), set()

    def visit(key):
        if key in seen or key not in paths:
            return
        seen.add(key)
        current = None
        for raw in contents[key].splitlines():
            line = raw.partition(";")[0].partition("--")[0].strip()
            if inc := INCLUDE.match(line):
                for match in includes(key, inc[1]):
                    visit(match)
                continue
            if section := SECTION.match(line):
                op, current, inherited = section.groups()
                current = current.strip().lower()
                if op == "-":
                    local.pop(current, None)
                    parents.pop(current, None)
                    list_updates.pop(current, None)
                    defined.discard(current)
                    if section_origins is not None:
                        section_origins.pop(current, None)
                    current = None
                    continue
                local.setdefault(current, {})
                if op in {"", "@"}:
                    defined.add(current)
                if section_origins is not None:
                    section_origins.setdefault(current, []).append(paths[key])
                if inherited:
                    parents.setdefault(current, []).extend(tokens(inherited.lower()))
                continue
            if current and (update := LIST_UPDATE.match(line)):
                op, prop, value = update.groups()
                list_updates.setdefault(current, []).append((op, prop.lower(), tokens(value)))
            elif current and "=" in line:
                prop, value = line.split("=", 1)
                local[current][prop.strip().lower()] = value.strip().strip('"')
            elif current and line.startswith("!"):
                local[current][line[1:].strip().lower()] = ""

    visit("system.ltx")
    for key in top_level:
        visit(key)

    @cache
    def effective(section, chain=()):
        if section in chain:
            return {}
        result = {}
        for parent in parents.get(section, ()):
            result.update(effective(parent, chain + (section,)))
        result.update(local.get(section, {}))
        for op, prop, value in list_updates.get(section, ()):
            old = tokens(result.get(prop, ""))
            if op == ">":
                old.extend(item for item in value if item not in old)
            else:
                old = [item for item in old if item not in value]
            result[prop] = ", ".join(old)
        return result

    return {name: effective(name) for name in defined}, seen


@dataclass(frozen=True)
class Optic:
    minimum: float
    maximum: float
    steps: int
    image_type: int


def optic_for(sight, weapon, modular=False):
    if not any(key.startswith("s3ds_") for key in sight):
        return None
    powers = [
        float(n) for n in re.findall(r"\d+(?:\.\d+)?", sight.get("magnifications", ""))
    ]
    powered = int(number(sight.get("s3ds_image_type")))
    combined = weapon | sight if modular else weapon
    native_min = number(combined.get("min_scope_zoom_factor"))
    native_max = number(combined.get("scope_zoom_factor"))
    dynamic = enabled(combined.get("scope_dynamic_zoom", ""))
    if not powers:
        # Fixed-integral 3DSS rifles sometimes omit the display magnification.
        powers = [120 / native_max] if 0 < native_max < 120 else [1]
    minimum, maximum = max(1, min(powers)), max(1, max(powers))
    if dynamic and native_min > 0:
        minimum = max(1, min(maximum, 120 / native_min))
    if dynamic and native_max > 0:
        maximum = max(minimum, min(maximum, 120 / native_max))
    if not powered and maximum <= 1.01:
        return None
    if not dynamic and minimum < maximum:
        # Multi-mode magnifiers that replace the weapon section have a fixed
        # optical power in each attachment state.
        minimum = maximum if not native_max else min(maximum, max(minimum, 120 / native_max))
        maximum = minimum
    steps = max(1, int(number(combined.get("zoom_step_count"), 4))) if maximum > minimum else 0
    return Optic(minimum, maximum, steps, powered)


def build_catalog(config):
    result = {}
    excluded = Counter()
    for section, values in sorted(config.items()):
        if not section.startswith("wpn_") or not all(values.get(key) for key in ("hud", "visual", "inv_name", "class")):
            continue
        if values.get("scope_texture", "").lower() not in {"", "none"}:
            excluded["2D texture scopes"] += 1
            continue
        modular = enabled(values.get("modular_attachments", ""))
        optics = {}
        if modular:
            for key in ("scopes", "scopes_sect"):
                for sight in tokens(values.get(key, "").lower()):
                    attached = config.get(sight, {})
                    if optic := optic_for(attached, values, modular=True):
                        optics[sight] = optic
        else:
            parent = values.get("parent_section", section)
            sight = section[len(parent) + 1:] if parent != section and section.startswith(parent + "_") else section
            if optic := optic_for(config.get(sight, {}) | values, values):
                optics[section] = optic
        if optics:
            result[section] = modular, optics, camera_fov(min(o.minimum for o in optics.values()))
        else:
            excluded["unmagnified or no 3DSS data"] += 1

    # Bone-mode magnifiers change the effective 3DSS section without changing
    # the physical inventory section from which the camera FOV is loaded.
    pairs = dict(config.get("unique_mappings", {}))
    sight_pairs = config.get("magnifier_mappings", {})
    for section, values in config.items():
        parent = values.get("parent_section", "")
        if not parent or not section.startswith(parent + "_"):
            continue
        sight = section[len(parent) + 1:]
        if sight in sight_pairs:
            pairs.setdefault(section, parent + "_" + sight_pairs[sight])
    for low, high in pairs.items():
        if high not in result or low not in config:
            continue
        high_modular, high_optics, _ = result[high]
        low_modular, low_optics, _ = result.get(low, (False, {}, 0))
        if high_modular or low_modular:
            continue
        on = high_optics.get(high)
        if on is None:
            continue
        own_low = low_optics.get(low)
        if any(enabled(config[name].get("magnifier_inherit_zoom", "")) for name in (low, high)):
            own_low = on
        elif own_low and own_low.image_type == 0:
            own_low = None
        combined = {high: on}
        if own_low:
            combined[low] = own_low
        camera = camera_fov(min(optic.minimum for optic in combined.values()))
        result[high] = False, combined, camera
        result[low] = False, combined, camera

    return dict(sorted(result.items())), excluded


def camera_fov(widest):
    # The engine permits exterior FOV up to 120 degrees. A fixed native
    # viewport at the broadest supported lens projection never needs upscaling.
    return math.degrees(2 * math.atan(math.tan(math.radians(120 / 2)) / widest))


def emit_catalog(catalog):
    overlay, info = [], []
    for section, (modular, options, camera) in catalog.items():
        overlay.extend((f"![{section}]", f"scope_lense_fov = {camera:.7f}", "zoom_dof = -1, -1, -1", ""))
        info.extend((f"[{section}]", f"camera = {camera:.7f}", f"modular = {'true' if modular else 'false'}"))
        for optic, data in sorted(options.items()):
            info.extend((
                f"{optic}_min = {data.minimum:.7f}",
                f"{optic}_max = {data.maximum:.7f}",
                f"{optic}_steps = {data.steps}",
            ))
        info.append("")
    return "\n".join(overlay), "\n".join(info)
