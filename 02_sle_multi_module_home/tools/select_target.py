#!/usr/bin/env python3
"""Select one firmware target for the four-module smart-home example."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


EXAMPLE_DIR = Path(__file__).resolve().parents[1]
VENDOR_DIR = EXAMPLE_DIR.parents[1]
PROJECT_ROOT = VENDOR_DIR.parents[2]

TARGETS = {
    "hub": ("hub", 1, True),
    "sht30": ("module", 1, False),
    "fan": ("module", 3, False),
    "alarm": ("module", 6, False),
    "light": ("module", 7, False),
}


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_if_changed(path: Path, content: str) -> None:
    if read(path) != content:
        path.write_text(content, encoding="utf-8")


def set_example_target(role: str, module_id: int) -> None:
    path = EXAMPLE_DIR / "BUILD.gn"
    content = read(path)
    content = re.sub(
        r'xh_sle_role\s*=\s*"[^"]+"',
        f'xh_sle_role = "{role}"',
        content,
        count=1,
    )
    content = re.sub(
        r"xh_module_id\s*=\s*\d+",
        f"xh_module_id = {module_id}",
        content,
        count=1,
    )
    write_if_changed(path, content)


def set_vendor_root(include_xiaohong: bool) -> None:
    path = VENDOR_DIR / "BUILD.gn"
    content = read(path)
    if "xh_include_xiaohong_app" not in content:
        content = content.replace(
            'group("xiaohong-se") {\n  deps = []\n\n'
            '  deps += [ "xiaohong:xiaohong" ]',
            'declare_args() {\n'
            '  xh_include_xiaohong_app = true\n'
            '}\n\n'
            'group("xiaohong-se") {\n'
            '  deps = []\n\n'
            '  if (xh_include_xiaohong_app) {\n'
            '    deps += [ "xiaohong:xiaohong" ]\n'
            '  }',
        )
    content = re.sub(
        r"xh_include_xiaohong_app\s*=\s*(true|false)",
        f"xh_include_xiaohong_app = {'true' if include_xiaohong else 'false'}",
        content,
        count=1,
    )
    write_if_changed(path, content)


def set_examples_component() -> None:
    path = VENDOR_DIR / "examples" / "BUILD.gn"
    content = read(path)
    content = re.sub(
        r"features\s*=\s*\[[\s\S]*?\]",
        'features = [\n'
        '    "02_sle_multi_module_home:sle_multi_module_home",\n'
        "  ]",
        content,
        count=1,
    )
    write_if_changed(path, content)


def remove_component(content: str, name: str) -> str:
    content = re.sub(rf'"{re.escape(name)}",?', "", content)
    content = re.sub(rf"'{re.escape(name)}',?", "", content)
    return content


def add_to_app_cmake(content: str, component: str) -> str:
    marker = 'elseif(${TARGET_COMMAND} MATCHES "ws63-liteos-app")'
    start = content.find(marker)
    if start < 0:
        raise RuntimeError("ws63-liteos-app component list not found")
    end = content.find(")\nendif()", start)
    if end < 0:
        raise RuntimeError("ws63-liteos-app component list end not found")
    return content[:end] + f'        "{component}"\n' + content[end:]


def set_one_sdk_components(sdk_dir: Path, include_xiaohong: bool) -> None:
    cmake = sdk_dir / "libs_url/ws63/cmake/ohos.cmake"
    config = sdk_dir / "build/config/target_config/ws63/config.py"
    if not cmake.exists() or not config.exists():
        return

    old_examples = (
        "p4_uart_alarm_gateway_demo",
        "ai_text_control_alarm",
        "sle_th_phone_oled",
        "sle_th_fan_1v1",
    )

    content = read(cmake)
    for name in (*old_examples, "sle_multi_module_home", "xiaohong"):
        content = remove_component(content, name)
    content = add_to_app_cmake(content, "sle_multi_module_home")
    if include_xiaohong:
        content = add_to_app_cmake(content, "xiaohong")
    write_if_changed(cmake, content)

    content = read(config)
    for name in (*old_examples, "sle_multi_module_home", "xiaohong"):
        content = remove_component(content, name)
    content = content.replace(
        "'samples',",
        "'samples',\n            'sle_multi_module_home',",
        1,
    )
    if include_xiaohong:
        content = content.replace(
            '"wifiservice",',
            '\'xiaohong\',\n            "wifiservice",',
            1,
        )
    write_if_changed(config, content)


def set_sdk_components(include_xiaohong: bool) -> None:
    soc_dir = PROJECT_ROOT / "device/soc/hisilicon/ws63v100"
    set_one_sdk_components(soc_dir / "sdk", include_xiaohong)
    set_one_sdk_components(soc_dir / "sdkv106", include_xiaohong)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("target", choices=TARGETS)
    args = parser.parse_args()

    role, module_id, include_xiaohong = TARGETS[args.target]
    set_vendor_root(include_xiaohong)
    set_examples_component()
    set_example_target(role, module_id)
    set_sdk_components(include_xiaohong)

    print(
        f"selected target={args.target} role={role} "
        f"module_id={module_id} include_xiaohong={include_xiaohong}"
    )


if __name__ == "__main__":
    main()
