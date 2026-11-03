#!/usr/bin/env bash
set -euo pipefail

# Install/upgrade the KART development SDK packages needed by KylinBot.
# The script is intentionally apt-based because these SDK packages are
# distributed as Debian packages on Kylin systems.

SUBSYSTEM_PACKAGE="kylin-ai-subsystem"
KART_DEV_PACKAGES=(
  "libkysdk-genai-nlp-dev"
  "libkysdk-genai-vision-dev"
  "libkylin-coreai-embedding-dev"
  "libkysdk-coreai-speech-dev"
  "libkysdk-coreai-vision-dev"
)

DRY_RUN=false
ASSUME_YES=false
RUN_APT_UPDATE=true
ALLOW_REMOVALS=false

if [ -t 1 ]; then
  BOLD=$'\033[1m'
  GREEN=$'\033[32m'
  YELLOW=$'\033[33m'
  RED=$'\033[31m'
  RESET=$'\033[0m'
else
  BOLD=""
  GREEN=""
  YELLOW=""
  RED=""
  RESET=""
fi

info() { printf "  %s✓%s %s\n" "$GREEN" "$RESET" "$*"; }
warn() { printf "  %s⚠%s %s\n" "$YELLOW" "$RESET" "$*" >&2; }
die() {
  printf "  %s✗%s %s\n" "$RED" "$RESET" "$*" >&2
  exit 1
}

print_help() {
  cat <<'EOF'
KylinBot KART development environment setup

Usage: ./dev/setup-kart-dev-env.sh [options]

Options:
  -y, --yes      Pass -y to apt-get install
  --dry-run      Print required package actions without installing
  --no-update    Skip apt-get update before version checks
  --allow-removals
                 Allow apt to remove packages during the install/upgrade
  -h, --help     Show this help

Packages:
  kylin-ai-subsystem
  libkysdk-genai-nlp-dev
  libkysdk-genai-vision-dev
  libkylin-coreai-embedding-dev
  libkysdk-coreai-speech-dev
  libkysdk-coreai-vision-dev
EOF
}

parse_args() {
  while [ $# -gt 0 ]; do
    case "$1" in
      -y|--yes)
        ASSUME_YES=true
        ;;
      --dry-run)
        DRY_RUN=true
        ;;
      --no-update)
        RUN_APT_UPDATE=false
        ;;
      --allow-removals)
        ALLOW_REMOVALS=true
        ;;
      -h|--help)
        print_help
        exit 0
        ;;
      *)
        die "Unknown option: $1"
        ;;
    esac
    shift
  done
}

require_commands() {
  local missing=()
  local cmd

  for cmd in apt-get apt-cache dpkg-query dpkg; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      missing+=("$cmd")
    fi
  done

  if [ "${#missing[@]}" -gt 0 ]; then
    die "Missing required command(s): ${missing[*]}"
  fi
}

sudo_cmd() {
  if [ "$(id -u)" -eq 0 ]; then
    "$@"
  else
    if ! command -v sudo >/dev/null 2>&1; then
      die "sudo is required when running as a non-root user"
    fi
    sudo "$@"
  fi
}

installed_version() {
  local package="$1"
  dpkg-query -W -f='${Version}' "$package" 2>/dev/null || true
}

candidate_version() {
  local package="$1"
  LC_ALL=C apt-cache policy "$package" | awk '/Candidate:/ { print $2; exit }'
}

latest_available_version_and_release() {
  local package="$1"
  local version release latest latest_release
  latest=""
  latest_release=""

  while IFS='|' read -r version release; do
    if [ -z "$version" ]; then
      continue
    fi
    if [ -z "$latest" ] || dpkg --compare-versions "$version" gt "$latest"; then
      latest="$version"
      latest_release="$release"
    fi
  done < <(
    LC_ALL=C apt-cache madison "$package" \
      | awk -F'|' '{
          gsub(/^[ \t]+|[ \t]+$/, "", $2);
          gsub(/^[ \t]+|[ \t]+$/, "", $3);
          split($3, repo, " ");
          suite = repo[2];
          sub(/\/.*/, "", suite);
          print $2 "|" suite
        }'
  )

  if [ -n "$latest" ]; then
    printf "%s|%s\n" "$latest" "$latest_release"
    return
  fi

  printf "%s|\n" "$(candidate_version "$package")"
}

package_status() {
  local package="$1"
  local installed target

  installed="$(installed_version "$package")"
  local target_line target_release
  target_line="$(latest_available_version_and_release "$package")"
  target="${target_line%%|*}"
  target_release="${target_line#*|}"

  if [ -z "$target" ] || [ "$target" = "(none)" ]; then
    printf "unavailable|%s|%s|%s\n" "${installed:-none}" "${target:-none}" "$target_release"
    return
  fi

  if [ -z "$installed" ]; then
    printf "missing|none|%s|%s\n" "$target" "$target_release"
    return
  fi

  if dpkg --compare-versions "$installed" ge "$target"; then
    printf "latest|%s|%s|%s\n" "$installed" "$target" "$target_release"
  else
    printf "outdated|%s|%s|%s\n" "$installed" "$target" "$target_release"
  fi
}

split_status() {
  local status_line="$1"
  STATUS="${status_line%%|*}"
  status_line="${status_line#*|}"
  INSTALLED="${status_line%%|*}"
  status_line="${status_line#*|}"
  TARGET_VERSION="${status_line%%|*}"
  TARGET_RELEASE="${status_line#*|}"
}

collect_required_packages() {
  local package status_line
  REQUIRED_PACKAGES=()
  INSTALL_TARGET_RELEASE=""

  for package in "$@"; do
    status_line="$(package_status "$package")"
    split_status "$status_line"

    case "$STATUS" in
      latest)
        info "$package is already latest ($INSTALLED)"
        ;;
      missing)
        info "$package will be installed ($TARGET_VERSION)"
        REQUIRED_PACKAGES+=("$package=$TARGET_VERSION")
        remember_target_release "$package"
        ;;
      outdated)
        info "$package will be upgraded ($INSTALLED -> $TARGET_VERSION)"
        REQUIRED_PACKAGES+=("$package=$TARGET_VERSION")
        remember_target_release "$package"
        ;;
      unavailable)
        die "$package has no apt candidate version; check apt sources"
        ;;
      *)
        die "Unexpected package status for $package: $STATUS"
        ;;
    esac
  done
}

remember_target_release() {
  local package="$1"

  if [ -z "$TARGET_RELEASE" ]; then
    return
  fi

  if [ -z "$INSTALL_TARGET_RELEASE" ]; then
    INSTALL_TARGET_RELEASE="$TARGET_RELEASE"
    return
  fi

  if [ "$INSTALL_TARGET_RELEASE" != "$TARGET_RELEASE" ]; then
    die "$package resolves to target release $TARGET_RELEASE, but earlier packages use $INSTALL_TARGET_RELEASE"
  fi
}

apt_install_command() {
  local args=("install")

  if [ -n "$INSTALL_TARGET_RELEASE" ]; then
    args+=("-t" "$INSTALL_TARGET_RELEASE")
  fi

  if [ "${1:-}" = "real" ] && [ "$ASSUME_YES" = true ]; then
    args+=("-y")
  fi

  args+=("${REQUIRED_PACKAGES[@]}")
  printf "%s\n" "${args[@]}"
}

simulate_install() {
  local args=()
  local output
  mapfile -t args < <(apt_install_command simulate)

  if ! output="$(LC_ALL=C apt-get -s "${args[@]}" 2>&1)"; then
    printf "%s\n" "$output"
    die "[dry-run] apt-get simulation failed"
  fi

  printf "%s\n" "$output"

  if printf "%s\n" "$output" | grep -Eq '^(Remv |The following packages will be REMOVED:)'; then
    if [ "$ALLOW_REMOVALS" = true ]; then
      warn "apt simulation includes package removals; continuing because --allow-removals is set"
    else
      die "apt simulation includes package removals; re-run with --allow-removals after reviewing the plan"
    fi
  fi
}

install_required_packages() {
  if [ "${#REQUIRED_PACKAGES[@]}" -eq 0 ]; then
    info "No package changes required"
    return
  fi

  local install_args=()
  mapfile -t install_args < <(apt_install_command real)

  if [ "$DRY_RUN" = true ]; then
    info "[dry-run] Would run: apt-get ${install_args[*]}"
    simulate_install
    info "[dry-run] apt-get simulation passed"
    return
  fi

  simulate_install
  sudo_cmd apt-get "${install_args[@]}"
}

main() {
  parse_args "$@"
  require_commands

  printf "%s\n" "${BOLD}KylinBot KART development environment setup${RESET}"

  if [ "$RUN_APT_UPDATE" = true ]; then
    if [ "$DRY_RUN" = true ]; then
      info "[dry-run] Would run: apt-get update"
    else
      sudo_cmd apt-get update
    fi
  else
    warn "Skipping apt-get update; candidate versions may be stale"
  fi

  printf "\n%s\n" "${BOLD}Step 1: subsystem package${RESET}"
  collect_required_packages "$SUBSYSTEM_PACKAGE"
  install_required_packages

  printf "\n%s\n" "${BOLD}Step 2: KART SDK development packages${RESET}"
  collect_required_packages "${KART_DEV_PACKAGES[@]}"
  install_required_packages

  printf "\n"
  if [ "$DRY_RUN" = true ]; then
    info "KART development environment check completed"
  else
    info "KART development environment is ready"
  fi
}

main "$@"
