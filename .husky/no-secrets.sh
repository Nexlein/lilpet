#!/bin/sh
# Blocks commits that stage .env files or secrets (gitleaks, from mise.toml: the hook runs inside mise, and refuses the
# commit if gitleaks is still missing rather than look for secrets less well).
status=0
# One path per line, so names with spaces stay whole, and not quoted, so that a path with accents ends in its name.
staged=$(git -c core.quotePath=false diff --cached --name-only --diff-filter=ACMR)
while IFS= read -r f; do
  case "$(basename "$f")" in
    "") ;;
    .env | .env.*) echo "no-secrets: $f must not be committed" >&2; status=1 ;;
  esac
done <<EOF
$staged
EOF
if ! command -v gitleaks >/dev/null 2>&1; then
  echo "no-secrets: gitleaks not found (run: mise install, in the repo)" >&2
  exit 1
fi
gitleaks git --pre-commit --staged --redact --no-banner --verbose || status=1
exit $status
