#!/bin/sh
# Animal Crossing (OpenCrossing) — Miyoo A30 launcher for spruceOS.
# Copy this file to /mnt/SDCARD/Roms/A30PORTS/ and the ac-gc/ folder next to it.
# spruceOS runs it with /bin/sh from Roms/A30PORTS (no PortMaster on the A30).
#
# A30 specifics (see kb/device.md "Miyoo A30"):
#  - Mali-400 = GLES 2.0 only → PC_GLES2=1 (skip the doomed GLES 3.1 attempt)
#  - 480x640 portrait panel → the game rotates its own output (auto-detected
#    from the drawable size; if the picture comes out sideways or upside
#    down, uncomment PC_ROTATE below and try 90 / 270)
#  - buttons arrive as keyboard keys → keybindings.ini written on first run

GAMEDIR="$(cd "$(dirname "$0")" && pwd)/ac-gc"
cd "$GAMEDIR" || exit 1
mkdir -p "$GAMEDIR/rom" "$GAMEDIR/conf"

: > "$GAMEDIR/log.txt"
exec >> "$GAMEDIR/log.txt" 2>&1

# First-run settings for the A30 (Cortex-A7 + Mali-400): 75% render scale
# (480x360, same as spruce's N64 setup), dynamic FPS, lighter effects, FPS
# counter + verbose log on while the A30 port is being tested.
if [ ! -f "$GAMEDIR/settings.ini" ]; then
cat > "$GAMEDIR/settings.ini" <<'EOF'
[Graphics]
fullscreen = 1
vsync = 1
msaa = 0
show_fps = 1
[Performance]
fps_target = 6
render_scale = 75
[Debug]
verbose = 1
[LowSpec]
frustum_cull = 1
shadow_quality = 1
particle_quality = 2
EOF
fi

# A30 buttons → keyboard keys (spruce's spruce/scripts/platform/A30.cfg —
# NOT the Miyoo Mini layout): A=Space B=LCtrl X=LShift Y=LAlt
# L1=Tab L2=E R1=Backspace R2=T Start=Return Select=RCtrl Menu=Escape,
# d-pad = arrow keys.
# GC mapping: L1=L, R1=R, R2=Z, L2 = OpenCrossing settings menu,
# Menu (Escape) = quit (press twice).
# Files from before the "Menu" binding existed (L1 opened the settings
# menu, shoulders mis-bound) are replaced once; the old copy is kept.
if [ -f "$GAMEDIR/keybindings.ini" ] && ! grep -q '^Menu' "$GAMEDIR/keybindings.ini"; then
  mv "$GAMEDIR/keybindings.ini" "$GAMEDIR/keybindings.ini.old"
fi
if [ ! -f "$GAMEDIR/keybindings.ini" ]; then
cat > "$GAMEDIR/keybindings.ini" <<'EOF'
[Keyboard]
A = Space
B = Left Ctrl
X = Left Shift
Y = Left Alt
Start = Return
Z = T
L = Tab
R = Backspace

Stick_Up = Up
Stick_Down = Down
Stick_Left = Left
Stick_Right = Right

CStick_Up = I
CStick_Down = K
CStick_Left = J
CStick_Right = L

DPad_Up = W
DPad_Down = S
DPad_Left = A
DPad_Right = D

Menu = E
EOF
fi

export HOME="$GAMEDIR/conf"
export XDG_DATA_HOME="$GAMEDIR/conf"
export LD_LIBRARY_PATH="$GAMEDIR/libs:/usr/miyoo/lib:/usr/lib:$LD_LIBRARY_PATH"
export PC_GLES2=1
# Buttons come through twice (keyboard keys + spruce's virtual Xbox pad);
# use the keyboard path only so each press is one GC button.
export PC_NO_CONTROLLER=1
# No physical keyboard: typing mode would turn buttons into letters.
export PC_NO_TYPING=1
# export PC_ROTATE=270

# Mali-400 driver libs ship unversioned (libGLESv2.so / libEGL.so); point
# SDL at them in case its defaults (libGLESv2.so.2 / libEGL.so.1) are absent.
for d in /usr/lib /usr/miyoo/lib; do
  [ -z "$SDL_VIDEO_GL_DRIVER" ] && [ -e "$d/libGLESv2.so" ] && export SDL_VIDEO_GL_DRIVER="$d/libGLESv2.so"
  [ -z "$SDL_VIDEO_EGL_DRIVER" ] && [ -e "$d/libEGL.so" ] && export SDL_VIDEO_EGL_DRIVER="$d/libEGL.so"
done

# Audio: spruce's ALSA setup (as its N64 launcher does), OSS as fallback.
[ -x /mnt/SDCARD/spruce/scripts/asound-setup.sh ] && /mnt/SDCARD/spruce/scripts/asound-setup.sh "$HOME"
export SDL_AUDIODRIVER=alsa,dsp

# The game reads the stick through the arrow keys (the A30 has no SDL game
# controller), so put spruce's joystickinput daemon in d-pad mode for the run.
killall -q -USR2 joystickinput

echo "--- A30 diag ---"
uname -a
cat /proc/meminfo | head -3
ls -l /usr/lib/libGLESv2* /usr/lib/libEGL* /usr/lib/libMali* /usr/miyoo/lib/libSDL2* 2>/dev/null
echo "SDL_VIDEO_GL_DRIVER=$SDL_VIDEO_GL_DRIVER SDL_VIDEO_EGL_DRIVER=$SDL_VIDEO_EGL_DRIVER"
echo "--- end diag ---"

chmod +x "$GAMEDIR/AnimalCrossing"
./AnimalCrossing

# Stick back to analog mode for the rest of spruceOS
killall -q -USR1 joystickinput
sync
