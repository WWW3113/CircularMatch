#!/usr/bin/env bash
# Download and unpack the ETH "Trees" dataset (Theiler et al., "Globally
# Consistent Registration of Multiple Point Clouds") into data/eth_trees/.
#
# Usage: scripts/eth_trees/download.sh <ZIP URL>
#   The URL is the official "Download Trees (ZIP, 1.2 GB)" link from
#   https://prs.igp.ethz.ch/research/completed_projects/automatic_registration_of_point_clouds.html
#   (copy it from the page; it is passed explicitly so the record shows exactly
#   what was downloaded).
#
# Writes data/eth_trees/trees.zip, data/eth_trees/raw/ (unzipped, never modified)
# and a provenance record (URL, final URL after redirects, size, SHA-256, date)
# to both data/eth_trees/DOWNLOAD.txt and results/eth_trees/download_record.txt.
# Works on Linux and WSL2 (needs curl, unzip, sha256sum).
set -euo pipefail

URL="${1:?usage: $0 <official Trees ZIP URL>}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DIR="$ROOT/data/eth_trees"
REC="$ROOT/results/eth_trees/download_record.txt"
mkdir -p "$DIR" "$(dirname "$REC")"

# ~1.2 GB zip + unpacked data (size unknown in advance): require 8 GB free.
need_kb=$((8 * 1024 * 1024))
avail_kb=$(df -Pk "$DIR" | awk 'NR==2 {print $4}')
if [ "$avail_kb" -lt "$need_kb" ]; then
  echo "ERROR: need >= 8 GB free in $DIR, have $((avail_kb / 1024)) MB" >&2
  exit 1
fi
echo "free space: $((avail_kb / 1024 / 1024)) GB"

ZIP="$DIR/trees.zip"
if [ ! -s "$ZIP" ]; then
  final_url=$(curl -fL --retry 3 --retry-delay 5 -o "$ZIP.part" -w '%{url_effective}' "$URL")
  mv "$ZIP.part" "$ZIP"
else
  echo "reusing existing $ZIP"
  final_url="(existing file, not re-downloaded)"
fi

size=$(stat -c %s "$ZIP")
sha=$(sha256sum "$ZIP" | cut -d' ' -f1)
unzip -tq "$ZIP" >/dev/null   # integrity check of every member (CRC)

rm -rf "$DIR/raw"
mkdir -p "$DIR/raw"
unzip -q "$ZIP" -d "$DIR/raw"
n_files=$(find "$DIR/raw" -type f | wc -l)
raw_bytes=$(du -sb "$DIR/raw" | cut -f1)

{
  echo "dataset=ETH Trees (Theiler et al., Globally Consistent Registration of Multiple Point Clouds)"
  echo "page=https://prs.igp.ethz.ch/research/completed_projects/automatic_registration_of_point_clouds.html"
  echo "url=$URL"
  echo "final_url=$final_url"
  echo "zip_bytes=$size"
  echo "zip_sha256=$sha"
  echo "unzipped_files=$n_files"
  echo "unzipped_bytes=$raw_bytes"
  echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} | tee "$DIR/DOWNLOAD.txt" > "$REC"
cat "$REC"
