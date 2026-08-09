#!/bin/sh

script_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
identity_file="$script_directory/build/generated/product_identity.env"

if [ ! -f "$identity_file" ]; then
  echo "Configure CMake first. Missing $identity_file" >&2
  exit 1
fi

plugin_filename=$(sed -n 's/^PRODUCT_PLUGIN_FILENAME=//p' "$identity_file")
if [ -z "$plugin_filename" ]; then
  echo "PRODUCT_PLUGIN_FILENAME is missing in $identity_file" >&2
  exit 1
fi

xattr -d com.apple.quarantine "$HOME/Library/Application Support/REAPER/UserPlugins/$plugin_filename.dylib"
