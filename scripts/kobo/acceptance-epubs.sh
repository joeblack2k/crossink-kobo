#!/usr/bin/env bash
# Copy and verify the user-supplied EPUB acceptance corpus without ever adding
# book contents to the source tree or a release image.
set -euo pipefail

repo_dir=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
books_dir=${CROSSINK_ACCEPTANCE_BOOKS_DIR:-"$repo_dir/../books"}
target=${CROSSINK_ACCEPTANCE_TARGET:-crossink-n437}
target_dir=${CROSSINK_ACCEPTANCE_TARGET_DIR:-/data/Books/Acceptance-Local}
evidence_dir=${CROSSINK_ACCEPTANCE_EVIDENCE_DIR:-"$repo_dir/artifacts/kobo/hardware/reader"}
manifest="$evidence_dir/local-books-manifest.tsv"
source_validation="$evidence_dir/local-books-source-validation.tsv"

usage() {
	cat >&2 <<EOF
Usage: $(basename "$0") copy|verify|manifest|validate-source

Environment:
  CROSSINK_ACCEPTANCE_BOOKS_DIR     local EPUB directory (default: ../books)
  CROSSINK_ACCEPTANCE_TARGET        SSH target (default: crossink-n437)
  CROSSINK_ACCEPTANCE_TARGET_DIR    Kobo directory (default: /data/Books/Acceptance-Local)
EOF
	exit 2
}

require_local_corpus() {
	[ -d "$books_dir" ] || { echo "Missing local books directory: $books_dir" >&2; exit 1; }
	# Once the user-approved twelve-book manifest exists, it is the corpus
	# contract. Extra files can legitimately appear here (for example a
	# repackaged Apple Books directory used for upload testing) and must not
	# silently change or invalidate the reproducible acceptance set.
	if [ -f "$manifest" ]; then
		count=$(awk 'END { print NR - 1 }' "$manifest")
		[ "$count" -eq 12 ] || {
			echo "Expected 12 manifest books, found $count in $manifest" >&2
			exit 1
		}
		missing=0
		awk -F '\t' 'NR > 1 { print $1 }' "$manifest" |
			while IFS= read -r name; do
				[ -f "$books_dir/$name" ] || { echo "Missing manifest book: $name" >&2; missing=1; }
			done
		# The loop above is a pipeline subshell on macOS; independently test
		# the manifest rows in awk's exit status below.
		awk -F '\t' -v books_dir="$books_dir" 'NR > 1 {
			path = books_dir "/" $1
			cmd = "test -f \"" path "\""
			if (system(cmd) != 0) exit 1
		}' "$manifest" || exit 1
		return
	fi
	count=$(find "$books_dir" -maxdepth 1 -type f -iname '*.epub' | wc -l | tr -d ' ')
	[ "$count" -eq 12 ] || {
		echo "Expected exactly 12 top-level EPUB files, found $count in $books_dir" >&2
		exit 1
	}
}

write_manifest() {
	if [ -f "$manifest" ] && [ "${CROSSINK_ACCEPTANCE_REFRESH_MANIFEST:-0}" != 1 ]; then
		require_local_corpus
		echo "Preserved existing 12-book manifest: $manifest"
		return
	fi
	require_local_corpus
	mkdir -p "$evidence_dir"
	{
		printf 'basename\tbytes\tsha256\n'
		# HFS/APFS directory order is stable for an unchanged corpus.  Avoid
		# GNU-only `sort -z` here because this harness is run from macOS.
		find "$books_dir" -maxdepth 1 -type f -iname '*.epub' -print0 |
			while IFS= read -r -d '' book; do
				name=$(basename "$book")
				bytes=$(stat -f '%z' "$book")
				hash=$(shasum -a 256 "$book" | awk '{print $1}')
				printf '%s\t%s\t%s\n' "$name" "$bytes" "$hash"
			done
	} >"$manifest"
	echo "Wrote $manifest"
}

copy_corpus() {
	require_local_corpus
	[ -f "$manifest" ] || write_manifest
	# This is a dedicated, disposable acceptance directory.  Remove only stale
	# AppleDouble sidecars left by an older macOS transfer; never touch user
	# books outside this directory.
	ssh "$target" "mkdir -p '$target_dir'; find '$target_dir' -maxdepth 1 -type f -name '._*.epub' -exec rm -f {} \\;"
	# Basenames are checked locally before this command. The archive preserves
	# binary EPUB contents and does not include parent paths.  macOS otherwise
	# adds `._*` AppleDouble files to archives, which a reader must never see.
	(
		cd "$books_dir"
		awk -F '\t' 'NR > 1 { printf "%s%c", $1, 0 }' "$manifest" |
			COPYFILE_DISABLE=1 bsdtar --no-mac-metadata --null -T - -cf -
	) | ssh "$target" "tar -C '$target_dir' -xf -"
}

validate_source() {
	require_local_corpus
	command -v unzip >/dev/null 2>&1 || {
		echo 'Missing required host tool: unzip' >&2
		exit 1
	}
	mkdir -p "$evidence_dir"
	{
		printf 'basename\tzip_integrity\tcontainer_xml\n'
		find "$books_dir" -maxdepth 1 -type f -iname '*.epub' -print0 |
			while IFS= read -r -d '' book; do
				name=$(basename "$book")
				zip_status=PASS
				container_status=PASS
				unzip -tqq "$book" >/dev/null 2>&1 || zip_status=FAIL
				unzip -Z1 "$book" META-INF/container.xml 2>/dev/null | grep -qx 'META-INF/container.xml' ||
					container_status=FAIL
				printf '%s\t%s\t%s\n' "$name" "$zip_status" "$container_status"
			done
		# Shell pipelines run in a subshell on macOS, so independently derive
		# the final status from the generated rows below.
	} >"$source_validation"
	if awk -F '\t' 'NR > 1 && ($2 != "PASS" || $3 != "PASS") { exit 1 }' "$source_validation"; then
		echo "Source EPUB validation passed: $source_validation"
	else
		echo "Source EPUB validation failed: $source_validation" >&2
		exit 1
	fi
}

verify_corpus() {
	[ -f "$manifest" ] || write_manifest
	# `IFS='\\t'` means a literal backslash and letter t in POSIX shells, not
	# a tab.  Use a delimiter that cannot occur in validated corpus names.
	awk -F '\t' 'NR > 1 { print $1 "|" $2 "|" $3 }' "$manifest" |
		ssh "$target" "
			set -eu
			cd '$target_dir'
			status=0
			while IFS='|' read -r name bytes hash; do
				[ -n \"\$name\" ] || continue
				if [ ! -f \"\$name\" ]; then echo \"MISSING\t\$name\" >&2; status=1; continue; fi
				actual_bytes=\$(wc -c <\"\$name\" | tr -d ' ')
				actual_hash=\$(sha256sum \"\$name\" | awk '{print \$1}')
				if [ \"\$actual_bytes\" != \"\$bytes\" ] || [ \"\$actual_hash\" != \"\$hash\" ]; then
					echo \"MISMATCH\t\$name\" >&2; status=1
				else
					echo \"OK\t\$name\"
				fi
			done
			exit \"\$status\"
		"
}

case ${1:-} in
	manifest) write_manifest ;;
	copy) copy_corpus ;;
	verify) verify_corpus ;;
	validate-source) validate_source ;;
	*) usage ;;
esac
