"""Copy xiaozhi icons, rename symbols to match our project naming."""
import os

SRC = r"d:\CCWorkspace\xiaozhi-esp32\main\boards\waveshare-s3-rlcd-4.2\assets\icons"
DST = r"d:\CCWorkspace\ESP32-S3-RLCD-Monitor\firmware\components\ui_app"

# xiaozhi symbol -> our symbol
rename_map = {
    "ui_img_wifi": "icon_wifi",
    "ui_img_wifi_low": "icon_wifi_low",
    "ui_img_wifi_off": "icon_wifi_off",
    "ui_img_battery_full": "icon_bat_full",
    "ui_img_battery_medium": "icon_bat_med",
    "ui_img_battery_low": "icon_bat_low",
    "ui_img_battery_charging": "icon_bat_chg",
}

files = list(rename_map.keys())

for old_sym in files:
    src_path = os.path.join(SRC, old_sym + ".c")
    new_sym = rename_map[old_sym]
    dst_path = os.path.join(DST, new_sym + ".c")

    with open(src_path, "r") as f:
        content = f.read()

    # Rename all occurrences
    content = content.replace(old_sym + "_map", new_sym + "_map")
    content = content.replace(old_sym, new_sym)

    with open(dst_path, "w") as f:
        f.write(content)

    print(f"  {old_sym:40s} -> {new_sym}")

print(f"\nDone! Copied {len(files)} files to {DST}")
