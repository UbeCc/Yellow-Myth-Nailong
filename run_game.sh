#!/bin/bash
set -e

# Launch Yellow Myth: Nailong on the shouyun1 NVIDIA X display (:2).
# The x11vnc session on port 5902 shares this display so you can VNC in.

export DISPLAY=:2

# A window manager is needed for focus / input handling.
if ! pgrep -x openbox >/dev/null; then
    nohup openbox --sm-disable >/tmp/openbox.log 2>&1 &
    sleep 2
fi

# O3DE warns when the open-file limit is below 65536.
ulimit -n 65536

cd /root/Yellow-Myth-Nailong/YellowMythNailong/build/linux/bin/profile

# If a dummy audio driver is available, use it to silence ALSA startup noise.
if [ -z "$SDL_AUDIODRIVER" ]; then
    export SDL_AUDIODRIVER=dummy 2>/dev/null || true
fi

# Background watcher: auto-dismiss the "Startup Errors" dialog (ALSA audio noise).
(
  for i in $(seq 1 40); do
    sleep 1
    if xdotool search --name "Startup Errors" >/dev/null 2>&1; then
      xdotool search --name "Startup Errors" key --clearmodifiers Escape 2>/dev/null || true
      sleep 1
      xdotool search --name "Startup Errors" key --clearmodifiers Return 2>/dev/null || true
      sleep 1
      xdotool search --name "Startup Errors" windowclose 2>/dev/null || true
    fi
  done
) &

exec ./YellowMythNailong.GameLauncher \
    --project-path=/root/Yellow-Myth-Nailong/YellowMythNailong \
    --rhi=Vulkan \
    "$@"
