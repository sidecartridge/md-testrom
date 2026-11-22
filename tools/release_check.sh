#!/usr/bin/env bash
#
# release_check.sh: what must hold before a version is tagged and released.
# Run by `make tag` before it tags, and by the release workflow before it
# publishes.
#
#   - nothing committed names the local planning backlog;
#   - the changelog's top section is version.txt's version (its notes become
#     the release's);
#   - with a tag given, the tag is that version.
#
# Usage:  tools/release_check.sh [tag]

set -Eeuo pipefail
trap 'echo "ERROR: release_check.sh: failed at line $LINENO" >&2' ERR
cd "$(dirname "$0")/.."

fail=0
version="$(tr -d '[:space:]' < version.txt)"

backlog="$(git grep --cached -IinE "EPIC-|STORY-|\bepics?\b|\bstor(y|ies)\b|docs/|\b[CDF]-[0-9]{2}\b" \
  -- . ':!CLAUDE.md' ':!.gitignore' ':!tools/release_check.sh' || true)"
if [[ -n "$backlog" ]]; then
  echo "release_check: committed files name the planning backlog:" >&2
  echo "$backlog" >&2
  fail=1
fi

top="$(grep -m1 '^## ' CHANGELOG.md || true)"
if [[ "$top" != "## $version "* ]]; then
  echo "release_check: CHANGELOG.md's top section is '$top', not $version" >&2
  fail=1
fi

if [[ $# -ge 1 && "$1" != "$version" ]]; then
  echo "release_check: the tag is $1, version.txt says $version" >&2
  fail=1
fi

[[ $fail -eq 0 ]] || exit 1
echo "release_check: $version is ready to release"
