# Sourced by the hooks that need the tools of mise.toml (pre-commit, pre-push): runs the hook again inside mise, also
# for a Git client started outside a mise shell. $0 and "$@" are the hook's.
if [ -z "$LILPET_HOOK_IN_MISE" ]; then
  if ! command -v mise >/dev/null 2>&1; then
    echo "${0##*/}: mise is needed, it provides the tools of mise.toml (https://mise.jdx.dev)" >&2
    exit 1
  fi
  LILPET_HOOK_IN_MISE=1 exec mise exec -- sh -e "$0" "$@"
fi
