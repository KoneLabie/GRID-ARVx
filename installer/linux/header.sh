#!/usr/bin/env bash
set -euo pipefail
payload_bytes=@PAYLOAD_BYTES@
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT
if [[ "${1:-}" == --help ]]; then
  echo 'GRID-ARVx installer: --all, --standalone, --vst3, --clap (options may be combined).'
  exit 0
fi
tail -c "$payload_bytes" "$0" | tar -xz -C "$work_dir"
if [[ $# == 0 ]]; then
  if [[ ! -t 0 ]]; then echo 'Choose --all, --standalone, --vst3 or --clap.' >&2; exit 2; fi
  echo 'GRID-ARVx: 1 Standalone, 2 VST3, 3 CLAP, 4 All'
  read -r -p 'Selection [4]: ' choice
  case "${choice:-4}" in
    1) set -- --standalone ;; 2) set -- --vst3 ;; 3) set -- --clap ;; 4) set -- --all ;;
    *) echo 'No installation performed.'; exit 2 ;;
  esac
fi
# Validate the entire selection before writing anything.
for option in "$@"; do
  case "$option" in --all|--standalone|--vst3|--clap) ;; *) echo "Unknown option: $option" >&2; exit 2 ;; esac
done
notices_dir="$HOME/.local/share/KoneLabie/GRID-ARVx"
mkdir -p "$notices_dir"
cp "$work_dir/LICENSE-NOTICE.md" "$notices_dir/"
for folder in docs ThirdPartyNotices; do
  if [[ -d "$work_dir/$folder" ]]; then cp -R "$work_dir/$folder" "$notices_dir/"; fi
done
for option in "$@"; do
  case "$option" in
    --all) selected=(Standalone VST3 CLAP) ;;
    --standalone) selected=(Standalone) ;; --vst3) selected=(VST3) ;; --clap) selected=(CLAP) ;;
  esac
  for component in "${selected[@]}"; do
    case "$component" in
      VST3) target="$HOME/.vst3" ;; CLAP) target="$HOME/.clap" ;;
      Standalone) target="$HOME/.local/opt/KoneLabie/GRID-ARVx" ;;
    esac
    mkdir -p "$target"; cp -R "$work_dir/$component/." "$target/"
    if [[ "$component" == Standalone ]]; then
      chmod +x "$target/GRID-ARVx"
      mkdir -p "$HOME/.local/share/applications"
      cat > "$HOME/.local/share/applications/konelabie-grid-arvx.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=GRID-ARVx
Exec="$target/GRID-ARVx"
Terminal=false
Categories=AudioVideo;
DESKTOP
    fi
    echo "Installed $component to $target"
  done
done
echo 'GRID-ARVx is installed. Rescan your DAW plugins.'
exit 0
