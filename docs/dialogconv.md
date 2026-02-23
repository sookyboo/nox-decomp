# Converting dialog audio files
The audio dialog files need to be converted in order for them to work.

The releases have start scripts that try to convert and extract the gog installer for you.

```bash
  # example can be found here: https://github.com/sookyboo/PortMaster-New/blob/sookyboo_nox_decomp/ports/nox-decomp/Nox-Decomp.sh
  export DIALOG_DIR="Dialog" # Put a full path to your extracted Dialog directory
  export FFMPEG_BIN="ffmpeg"
  shopt -s nullglob nocaseglob
  wav_files=("$DIALOG_DIR"/*.wav)
  total="${#wav_files[@]}"

  if [ "$total" -eq 0 ]; then
    echo "No dialog WAV files found, skipping conversion"
    return 0
  fi

  echo "Converting dialog audio ($total files)"
  sleep 1

  i=0

  # -------------------------------------------------
  # Convert with progress updates
  # -------------------------------------------------
  for wav in "${wav_files[@]}"; do
    tmp="${wav}.tmp"

    if "$FFMPEG_BIN" -y \
        -loglevel error \
        -i "$wav" \
        -ac 1 \
        -ar 22050 \
        -c:a pcm_s16le \
        -f wav \
        "$tmp"; then
      mv "$tmp" "$wav"
    else
      rm -f "$tmp"
      echo "ERROR converting $(basename "$wav")"
      return 1
    fi

    i=$((i + 1))
  done

```