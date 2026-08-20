#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import posixpath
import re
import shutil
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path
from urllib.parse import quote


REQUIRED_IDENTITY_KEYS = (
    "PRODUCT_DISPLAY_NAME",
    "PRODUCT_ID",
    "PRODUCT_RESOURCE_DIRECTORY",
    "PRODUCT_PLUGIN_FILENAME",
    "PRODUCT_SCRIPT_DIRECTORY",
    "PRODUCT_PACKAGE_PREFIX",
    "PRODUCT_REPOSITORY_URL",
    "PRODUCT_OSK_SCRIPT_FILENAME",
    "PRODUCT_OSD_SCRIPT_FILENAME",
    "PRODUCT_NOTIFICATIONS_SCRIPT_FILENAME",
    "PRODUCT_CONTROL_PANEL_SCRIPT_FILENAME",
    "PRODUCT_CONTROL_PANEL_ACTION_ID",
)

PLUGIN_ASSETS = (
    ("darwin64", "darwin-x86_64", ".dylib"),
    ("darwin-arm64", "darwin-arm64", ".dylib"),
    ("win64", "win64", ".dll"),
    ("linux64", "linux-x86_64", ".so"),
)

STABLE_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_-]*$")
ACTION_ID_PATTERN = re.compile(r"^[A-Z][A-Z0-9_]*$")
VERSION_PATTERN = re.compile(r"^\d")
GITHUB_REPOSITORY_PATTERN = re.compile(r"^https://github\.com/([^/]+)/([^/]+?)(?:\.git)?$")


def repository_root():
    return Path(__file__).resolve().parents[2]


def fail(message):
    raise RuntimeError(message)


def read_identity(root):
    identity_path = root / "Scripts" / "product_identity.conf"
    identity = {}
    for line_number, raw_line in enumerate(identity_path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            fail(f"Invalid product identity line {line_number}: {line}")
        key, value = line.split("=", 1)
        if key in identity:
            fail(f"Duplicate product identity key: {key}")
        identity[key] = value

    for key in REQUIRED_IDENTITY_KEYS:
        if not identity.get(key):
            fail(f"Missing product identity value: {key}")
    return identity


def validate_identity(identity):
    if not STABLE_ID_PATTERN.fullmatch(identity["PRODUCT_ID"]):
        fail("PRODUCT_ID must be a stable lowercase ASCII ID")
    if not ACTION_ID_PATTERN.fullmatch(identity["PRODUCT_CONTROL_PANEL_ACTION_ID"]):
        fail("PRODUCT_CONTROL_PANEL_ACTION_ID must contain uppercase ASCII letters, digits, and underscores, without a leading underscore")
    for key in ("PRODUCT_RESOURCE_DIRECTORY", "PRODUCT_SCRIPT_DIRECTORY", "PRODUCT_PACKAGE_PREFIX"):
        if "/" in identity[key] or "\\" in identity[key]:
            fail(f"{key} must be one path component")
    repository_match = GITHUB_REPOSITORY_PATTERN.fullmatch(identity["PRODUCT_REPOSITORY_URL"])
    if not repository_match:
        fail("PRODUCT_REPOSITORY_URL must be a GitHub repository URL")
    return repository_match.group(1), repository_match.group(2)


def resolve_inside(root, value, label):
    resolved = (root / value).resolve() if not Path(value).is_absolute() else Path(value).resolve()
    try:
        resolved.relative_to(root.resolve())
    except ValueError:
        fail(f"{label} must be inside the repository: {resolved}")
    return resolved


def require_regular_file(path, label):
    if not path.is_file() or path.is_symlink():
        fail(f"Missing or unsupported {label}: {path}")


def verify_vendor_install_paths(records, resource_directory):
    surface_source_prefix = "resources/Surfaces/Vendor/"
    surface_install_prefix = f"Data/{resource_directory}/Surfaces/Vendor/"
    zone_source_prefix = "resources/Zones/Vendor/"
    zone_install_prefix = f"Data/{resource_directory}/Zones/Vendor/"
    for record in records:
        if record["localPath"].startswith(surface_source_prefix) and not record["installPath"].startswith(surface_install_prefix):
            fail(f"Vendor surface has an invalid install path: {record['installPath']}")
        if record["localPath"].startswith(zone_source_prefix) and not record["installPath"].startswith(zone_install_prefix):
            fail(f"Vendor zone has an invalid install path: {record['installPath']}")


def raw_url(owner, repository, tag, relative_path):
    encoded_tag = quote(tag, safe="")
    encoded_path = quote(relative_path.as_posix(), safe="/")
    return f"https://raw.githubusercontent.com/{owner}/{repository}/{encoded_tag}/{encoded_path}"


def release_url(repository_url, tag, asset_name):
    return f"{repository_url}/releases/download/{quote(tag, safe='')}/{quote(asset_name, safe='')}"


def write_manifest(path, description, author, version, tag, repository_url, provides):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"@description {description}",
        f"@author {author}",
        f"@version {version}",
        f"@changelog Preview package generated for {tag}.",
        f"@link {repository_url}",
        "@provides",
    ]
    lines.extend(f"  {provide}" for provide in provides)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def add_source(records, url, local_path, source_file, install_path, platform=None, source_type=None, main=None):
    normalized_install_path = install_path.as_posix()
    lowered_install_path = normalized_install_path.lower()
    forbidden_fragments = ("/surfaces/user/", "/zones/user/", "/snippets/user/", "/backups/", "/generated/")
    if any(fragment in f"/{lowered_install_path}/" for fragment in forbidden_fragments):
        fail(f"ReaPack source targets a user-owned path: {normalized_install_path}")
    records.append({
        "url": url,
        "localPath": local_path.as_posix(),
        "sourceFile": source_file,
        "installPath": normalized_install_path,
        "platform": platform,
        "type": source_type,
        "main": main,
    })


def prepare(args):
    root = repository_root()
    identity = read_identity(root)
    owner, repository = validate_identity(identity)
    github_repository = os.environ.get("GITHUB_REPOSITORY")
    if github_repository and github_repository != f"{owner}/{repository}":
        fail(f"PRODUCT_REPOSITORY_URL does not match GITHUB_REPOSITORY: {github_repository}")
    if not VERSION_PATTERN.match(args.version):
        fail("ReaPack version must start with a digit")
    if not args.tag:
        fail("Release tag must not be empty")

    stage_root = resolve_inside(root, args.stage_dir, "Stage directory")
    asset_root = resolve_inside(root, args.asset_dir, "Asset directory")
    if stage_root.exists():
        fail(f"Stage directory already exists: {stage_root}")
    stage_root.mkdir(parents=True)
    shutil.copyfile(root / ".reapack-index.conf", stage_root / ".reapack-index.conf")

    display_name = identity["PRODUCT_DISPLAY_NAME"]
    resource_directory = identity["PRODUCT_RESOURCE_DIRECTORY"]
    script_directory = identity["PRODUCT_SCRIPT_DIRECTORY"]
    package_prefix = identity["PRODUCT_PACKAGE_PREFIX"]
    repository_url = identity["PRODUCT_REPOSITORY_URL"]
    plugin_filename = identity["PRODUCT_PLUGIN_FILENAME"]
    source_records = []

    core_provides = []
    for platform, asset_suffix, extension in PLUGIN_ASSETS:
        asset_name = f"{plugin_filename}-{asset_suffix}{extension}"
        target_name = f"{plugin_filename}{extension}"
        local_path = asset_root / asset_name
        require_regular_file(local_path, f"{platform} extension asset")
        url = release_url(repository_url, args.tag, asset_name)
        core_provides.append(f"[{platform}] {target_name} {url}")
        add_source(source_records, url, local_path.relative_to(root), target_name, Path("UserPlugins") / target_name, platform=platform)

    main_scripts = {identity["PRODUCT_OSK_SCRIPT_FILENAME"], identity["PRODUCT_OSD_SCRIPT_FILENAME"], identity["PRODUCT_NOTIFICATIONS_SCRIPT_FILENAME"], identity["PRODUCT_CONTROL_PANEL_SCRIPT_FILENAME"]}
    script_paths = sorted((root / "Scripts").glob("*.lua"))
    script_paths.append(root / "Scripts" / "product_identity.conf")
    script_paths.append(root / "Scripts" / "settings_schema.conf")
    for script_path in script_paths:
        require_regular_file(script_path, "shared script source")
        source_file = script_path.name
        url = raw_url(owner, repository, args.tag, script_path.relative_to(root))
        main = "main" if source_file in main_scripts else None
        option = "[script main]" if main else "[script nomain]"
        target_file = (Path("..") / source_file).as_posix()
        core_provides.append(f"{option} {target_file} {url}")
        install_path = Path("Scripts") / script_directory / source_file
        add_source(source_records, url, script_path.relative_to(root), target_file, install_path, source_type="script", main=main)

    core_manifest = stage_root / script_directory / f"{package_prefix} Core.ext"
    write_manifest(core_manifest, f"{display_name} core extension and shared scripts", owner, args.version, args.tag, repository_url, core_provides)

    vendor_surfaces_root = root / "resources" / "Surfaces" / "Vendor"
    for surface_path in sorted(vendor_surfaces_root.glob("*.txt")):
        surface_id = surface_path.stem
        if not STABLE_ID_PATTERN.fullmatch(surface_id):
            fail(f"Invalid vendor surface ID: {surface_id}")
        require_regular_file(surface_path, "vendor surface")
        url = raw_url(owner, repository, args.tag, surface_path.relative_to(root))
        manifest_path = stage_root / resource_directory / "Surfaces" / "Vendor" / f"{package_prefix} Surface {surface_id}.data"
        target_file = (Path(resource_directory) / "Surfaces" / "Vendor" / surface_path.name).as_posix()
        write_manifest(manifest_path, f"{display_name} vendor surface: {surface_id}", owner, args.version, args.tag, repository_url, [f"{target_file} {url}"])
        install_path = Path("Data") / resource_directory / "Surfaces" / "Vendor" / surface_path.name
        add_source(source_records, url, surface_path.relative_to(root), target_file, install_path)

    vendor_zones_root = root / "resources" / "Zones" / "Vendor"
    for profile_path in sorted(path for path in vendor_zones_root.iterdir() if path.is_dir()):
        profile_id = profile_path.name
        if profile_path.is_symlink():
            fail(f"Vendor zone profile must not be a symlink: {profile_id}")
        if not STABLE_ID_PATTERN.fullmatch(profile_id):
            fail(f"Invalid vendor zone profile ID: {profile_id}")
        provides = []
        for zone_path in sorted(path for path in profile_path.rglob("*") if path.is_file()):
            if zone_path.name == ".gitkeep":
                continue
            require_regular_file(zone_path, "vendor zone source")
            source_file = zone_path.relative_to(profile_path).as_posix()
            url = raw_url(owner, repository, args.tag, zone_path.relative_to(root))
            target_file = (Path(resource_directory) / "Zones" / "Vendor" / profile_id / source_file).as_posix()
            provides.append(f"{target_file} {url}")
            install_path = Path("Data") / resource_directory / "Zones" / "Vendor" / profile_id / source_file
            add_source(source_records, url, zone_path.relative_to(root), target_file, install_path)
        if not provides:
            fail(f"Vendor zone profile is empty: {profile_id}")
        manifest_path = stage_root / resource_directory / "Zones" / "Vendor" / profile_id / f"{package_prefix} Zones {profile_id}.data"
        write_manifest(manifest_path, f"{display_name} vendor zone profile: {profile_id}", owner, args.version, args.tag, repository_url, provides)

    built_in_snippets_root = root / "resources" / "Snippets" / "BuiltIn"
    for snippet_path in sorted(built_in_snippets_root.glob("*.snippet")):
        require_regular_file(snippet_path, "built-in snippet")
        url = raw_url(owner, repository, args.tag, snippet_path.relative_to(root))
        manifest_path = stage_root / resource_directory / "Snippets" / "BuiltIn" / f"{package_prefix} Snippet {snippet_path.stem}.data"
        target_file = (Path(resource_directory) / "Snippets" / "BuiltIn" / snippet_path.name).as_posix()
        write_manifest(manifest_path, f"{display_name} built-in snippet: {snippet_path.stem}", owner, args.version, args.tag, repository_url, [f"{target_file} {url}"])
        install_path = Path("Data") / resource_directory / "Snippets" / "BuiltIn" / snippet_path.name
        add_source(source_records, url, snippet_path.relative_to(root), target_file, install_path)

    verify_vendor_install_paths(source_records, resource_directory)
    source_map_path = stage_root.parent / "reapack-source-map.json"
    source_map_path.write_text(json.dumps({"sources": source_records}, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(source_records)} ReaPack sources in {stage_root}")


def source_elements(root_element):
    return [element for element in root_element.iter() if element.tag.rsplit("}", 1)[-1] == "source"]


def source_contexts(root_element):
    contexts = []
    repository_name = root_element.attrib.get("name")
    if not repository_name:
        fail("Generated ReaPack index has no repository name")
    for category_element in root_element:
        if category_element.tag.rsplit("}", 1)[-1] != "category":
            continue
        category_name = category_element.attrib.get("name")
        if not category_name:
            fail("Generated ReaPack index has an unnamed category")
        for package_element in category_element:
            if package_element.tag.rsplit("}", 1)[-1] != "reapack":
                continue
            package_type = package_element.attrib.get("type")
            for element in source_elements(package_element):
                contexts.append((element, repository_name, category_name, package_type))
    return contexts


def reapack_install_path(element, repository_name, category_name, package_type):
    source_file = element.attrib.get("file")
    if not source_file:
        fail("Generated ReaPack source has no target file")
    source_type = element.attrib.get("type") or package_type
    if source_type == "script":
        install_path = posixpath.normpath(posixpath.join("Scripts", repository_name, category_name, source_file))
    elif source_type == "extension":
        install_path = posixpath.normpath(posixpath.join("UserPlugins", source_file))
    elif source_type == "data":
        install_path = posixpath.normpath(posixpath.join("Data", source_file))
    else:
        fail(f"Generated ReaPack source has an unsupported type: {source_type}")
    if install_path.startswith("../") or install_path.startswith("/"):
        fail(f"Generated ReaPack source escapes the REAPER resource directory: {install_path}")
    return install_path


def verify_attribute(element, attribute, expected, source_url):
    actual = element.attrib.get(attribute)
    if actual != expected:
        fail(f"Unexpected {attribute} for {source_url}: expected {expected!r}, got {actual!r}")


def finalize(args):
    root = repository_root()
    stage_root = resolve_inside(root, args.stage_dir, "Stage directory")
    output_root = resolve_inside(root, args.output_dir, "Output directory")
    index_path = stage_root / "index.xml"
    source_map_path = stage_root.parent / "reapack-source-map.json"
    require_regular_file(index_path, "generated ReaPack index")
    require_regular_file(source_map_path, "ReaPack source map")

    source_map = json.loads(source_map_path.read_text(encoding="utf-8"))
    records_by_url = {}
    for record in source_map["sources"]:
        if record["url"] in records_by_url:
            fail(f"Duplicate ReaPack source URL: {record['url']}")
        records_by_url[record["url"]] = record

    xml_tree = ElementTree.parse(index_path)
    seen_urls = set()
    for element, repository_name, category_name, package_type in source_contexts(xml_tree.getroot()):
        source_url = (element.text or "").strip()
        record = records_by_url.get(source_url)
        if not record:
            fail(f"Generated index contains an unknown source URL: {source_url}")
        if source_url in seen_urls:
            fail(f"Generated index contains a duplicate source URL: {source_url}")
        seen_urls.add(source_url)
        verify_attribute(element, "file", record["sourceFile"], source_url)
        verify_attribute(element, "platform", record["platform"], source_url)
        verify_attribute(element, "type", record["type"], source_url)
        verify_attribute(element, "main", record["main"], source_url)
        actual_install_path = reapack_install_path(element, repository_name, category_name, package_type)
        if actual_install_path != record["installPath"]:
            fail(f"Unexpected install path for {source_url}: expected {record['installPath']!r}, got {actual_install_path!r}")
        local_path = root / record["localPath"]
        require_regular_file(local_path, "source used for index checksum")
        digest = hashlib.sha256(local_path.read_bytes()).hexdigest()
        element.set("hash", f"1220{digest}")

    missing_urls = sorted(set(records_by_url) - seen_urls)
    if missing_urls:
        fail(f"Generated index is missing {len(missing_urls)} expected source URLs")

    ElementTree.indent(xml_tree, space="  ")
    xml_tree.write(index_path, encoding="utf-8", xml_declaration=True)
    with index_path.open("ab") as index_file:
        index_file.write(b"\n")

    output_root.mkdir(parents=True, exist_ok=True)
    output_index = output_root / "index.xml"
    shutil.copyfile(index_path, output_index)
    index_digest = hashlib.sha256(output_index.read_bytes()).hexdigest()
    (output_root / "index.xml.sha256").write_text(f"{index_digest}  index.xml\n", encoding="utf-8")
    print(f"Finalized {len(seen_urls)} hashed ReaPack sources in {output_index}")


def parse_arguments():
    parser = argparse.ArgumentParser(description="Prepare and finalize the preview ReaPack repository")
    subparsers = parser.add_subparsers(dest="command", required=True)

    prepare_parser = subparsers.add_parser("prepare", help="Create a temporary metadata repository")
    prepare_parser.add_argument("--version", required=True)
    prepare_parser.add_argument("--tag", required=True)
    prepare_parser.add_argument("--asset-dir", required=True)
    prepare_parser.add_argument("--stage-dir", default=".reapack-build/repository")
    prepare_parser.set_defaults(handler=prepare)

    finalize_parser = subparsers.add_parser("finalize", help="Add hashes and verify the generated index")
    finalize_parser.add_argument("--stage-dir", default=".reapack-build/repository")
    finalize_parser.add_argument("--output-dir", required=True)
    finalize_parser.set_defaults(handler=finalize)
    return parser.parse_args()


def main():
    args = parse_arguments()
    try:
        args.handler(args)
    except (OSError, RuntimeError, ValueError, ElementTree.ParseError, KeyError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
