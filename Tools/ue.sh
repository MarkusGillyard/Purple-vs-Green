#!/usr/bin/env bash
set -euo pipefail

: "${PVG_ENGINE:?Set PVG_ENGINE to the UE 5.8.4 install root}"
: "${PVG_STORAGE:?Set PVG_STORAGE to a writable project-owned directory for UE outputs}"
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
project="$repo/PurpleVsGreen.uproject"
engine=$(realpath -e -- "$PVG_ENGINE")
storage=$(realpath -e -- "$PVG_STORAGE")
action=${1:-}
[[ -n "$action" ]] || { echo 'Usage: Tools/ue.sh build | run | package [configuration] | smoke <package root>' >&2; exit 2; }
shift

if [[ -n "${PVG_STORAGE_UUID:-}" && "$(findmnt -n -o UUID -T "$storage")" != "$PVG_STORAGE_UUID" ]]; then
  echo "Configured storage is not on the expected device: $storage" >&2
  exit 1
fi

version=$(python3 -c 'import json,sys; v=json.load(open(sys.argv[1])); print("{MajorVersion}.{MinorVersion}.{PatchVersion}".format(**v))' "$engine/Engine/Build/Build.version")
[[ "$version" == 5.8.4 ]] || { echo "Expected UE 5.8.4, found $version" >&2; exit 1; }
mkdir -p "$storage/cache" "$storage/logs"
ue=(env "UE-LocalDataCachePath=$storage/cache")

case "$action" in
  build)
    bash "$engine/Engine/Build/BatchFiles/Linux/Build.sh" PurpleVsGreenEditor Linux Development \
      "-Project=$project" -WaitMutex -NoUBA -NoUbaLocal "-MaxParallelActions=${PVG_BUILD_JOBS:-4}" "$@"
    ;;
  run)
    "${ue[@]}" "$engine/Engine/Binaries/Linux/UnrealEditor" "$project" -game -nosplash "$@"
    ;;
  package)
    configuration=${1:-Development}
    if [[ "$configuration" != Development && "$configuration" != Shipping ]]; then
      echo 'Package configuration must be Development or Shipping.' >&2
      exit 2
    fi
    if [[ -n "$(git -C "$repo" status --porcelain)" ]]; then
      echo 'Commit the candidate source before packaging.' >&2
      exit 1
    fi
    revision=$(git -C "$repo" rev-parse HEAD)
    available=$(df -B1 --output=avail "$storage" | tail -1)
    if (( available < 7 * 1024 * 1024 * 1024 )); then
      echo 'Packaging requires 7 GiB free on the configured storage.' >&2
      exit 1
    fi
    mkdir -p "$storage/builds"
    output=$(mktemp -d "$storage/builds/$revision-$configuration.XXXXXX")
    "${ue[@]}" bash "$engine/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
      "-project=$project" -platform=Linux "-clientconfig=$configuration" \
      -build -cook -stage -pak -iostore -package -archive -nodebuginfo "-archivedirectory=$output" \
      -nop4 -unattended -utf8output -nocompileuat "-ubtargs=-NoUBA -NoUbaLocal -MaxParallelActions=${PVG_BUILD_JOBS:-4}" \
      2>&1 | tee "$output/build.log"
    if [[ "$(git -C "$repo" rev-parse HEAD)" != "$revision" || -n "$(git -C "$repo" status --porcelain)" ]]; then
      echo 'Source changed while packaging. Package identity was not recorded.' >&2
      exit 1
    fi
    printf '%s %s\n' "$revision" "$configuration" > "$output/Linux/REVISION.txt"
    mkdir -p "$output/Linux/ThirdPartyNotices"
    cp -R "$engine/Engine/Source/ThirdParty/Licenses" "$output/Linux/ThirdPartyNotices/Unreal"
    cp "$repo/Docs/Assets.md" "$output/Linux/ThirdPartyNotices/AssetLedger.md"
    (
      cd "$output/Linux"
      find . -type f -print0 | sort -z | xargs -0 sha256sum
    ) > "$output/SHA256SUMS"
    printf 'Package at %s\n' "$output/Linux"
    ;;
  smoke)
    package=$(realpath -e -- "${1:?Supply the Linux package directory}")
    if ! grep -Eq '^[a-f0-9]{40} (Development|Shipping)$' "$package/REVISION.txt"; then
      echo 'Smoke requires a manifest-bearing Development or Shipping package.' >&2
      exit 2
    fi
    profile=$(mktemp -d "$storage/smoke.XXXXXX")
    (cd "$package" && sha256sum --strict -c ../SHA256SUMS) > "$profile/hash-readback.log"
    timeout 220 "$package/PurpleVsGreen.sh" -PvGSmoke "-userdir=$profile" \
      -windowed -ResX=1920 -ResY=1080 -NoVSync -unattended -NoCrashDialog -notraceserver -nosplash -stdout -FullStdOutLogOutput \
      2>&1 | tee "$profile/smoke.log"
    grep -qx 'PvG smoke passed' "$profile/Saved/PvGSmoke.txt"
    cat "$profile/Saved/PvGSmoke.txt"
    printf 'Smoke evidence at %s\n' "$profile"
    ;;
  *)
    echo "Unknown command: $action" >&2
    exit 2
    ;;
esac
