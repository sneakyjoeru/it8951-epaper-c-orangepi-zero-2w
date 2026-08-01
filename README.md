# IT8951 E-Paper Display Driver (C) — Orange Pi Zero 2W

C port of the [Python IT8951 driver](https://github.com/sneakyjoeru/it8951-epaper-python-orangepi-zero-2w)
for the **Waveshare 7.8" E-Ink HAT** (1872×1404, IT8951) on **Orange Pi Zero 2W**.

Pre-built binary available — no compilation required on the Pi.

## Features

- 📺 Full IT8951 SPI driver (4bpp & 8bpp display)
- 📝 Antialiased text rendering (FreeType, 3× supersampling)
- 🖼️ Image display (stb_image, auto-scaled, Floyd-Steinberg dithered)
- 🎨 Test patterns (gradient, checkerboard, cross, quarter)
- 🔧 OS setup mode (`--setup` configures overlay + installs packages)
- 🌫️ Random dithering for smooth gradients
- 🔄 **Regional differential updates** — only refreshes changed areas
  (with `--hard`/`--soft` modes and configurable border dithering)

![Antialiased text](https://media.discordapp.net/attachments/337997702170279946/1529633968079634604/PXL_20260722_233256404.MP.jpg?ex=6a62a625&is=6a6154a5&hm=ed58adc604bb8715ffc913c27a1ff44491d0379b7cd2721ee919d5fd6782fc37&animated=true&width=1785&height=1344)


## Quick Start (Pre-built Binary)

```bash
# Download binary to the Pi
scp bin/it8951-aarch64 orangepi@192.168.0.199:~/it8951
ssh orangepi@192.168.0.199
chmod +x it8951

# Run setup (configures boot overlay + installs packages)
sudo ./it8951 --setup
# Reboot if prompted

# After reboot:
sudo ./it8951 --info
sudo ./it8951 --clear
sudo ./it8951 --text "Hello, World!"
```

## Performance (image render, 4096×4096 → 1872×1404)

| Version | Avg time | Notes |
|---------|---------|-------|
| C (this repo) | **~4.1s** | gamma LUT + 4bpp packing + overlapped A2 clear |
| Python | ~9.2s | baseline (PIL + numpy) |

The C port is ~2.25× faster than the Python original. The speedups ported
from the Python optimization work:

1. **Gamma LUT** — precomputed 256-entry lookup table (no per-pixel `powf`)
2. **4bpp packing** — half the SPI data vs 8bpp (1.3 MB vs 2.6 MB transfer)
3. **Overlapped A2 clear** — A2 black-flash refresh overlaps with 4bpp data
   load (`it8951_clear_then_display_4bpp`), saving ~1.5s vs sequential clear+render

## Building from Source

### Prerequisites

```bash
sudo apt install -y libgpiod-dev libfreetype-dev gcc make
```

### Build

```bash
make
```

### Test

```bash
sudo ./it8951 --info
sudo ./it8951 --clear
sudo ./it8951 --text "Hello, Orange Pi!" --font-size 96
sudo ./it8951 --image docs/test_image.jpg   # bundled sample (4096×4096)
sudo ./it8951 --image photo.jpg
sudo ./it8951 --gradient
sudo ./it8951 --cross 9
sudo ./it8951 --checker 50
sudo ./it8951 --quarter
```

## Regional Differential Updates

The driver can compare a new image against the last displayed image and only
refresh the changed region — dramatically reducing flash/flicker and update time
for small changes (e.g. time-line movement on a calendar).

### Usage

```bash
# Soft refresh — no white flash, just GC16 on changed region
sudo ./it8951 --image calendar.png --soft

# Hard refresh — white-flash the region first, then draw (cleanest result)
sudo ./it8951 --image calendar.png --hard

# Custom border (dithering zone around changed region)
sudo ./it8951 --image calendar.png --soft --border-smooth 30
```

### How it works

1. **Store last image** — After each render, the current image is saved to
   `/tmp/it8951_last.png` (8bpp grayscale PNG).
2. **Compare** — New image is compared pixel-by-pixel against the stored image.
   Pixels differing by more than a threshold (default: 5) mark the changed region.
3. **Expand** — The changed bounding box is expanded by `--border-smooth` pixels
   (default: 20) on each side, clamped to screen bounds.
4. **Dither edges** — Floyd-Steinberg dithering blends old and new pixel values
   in the border zone, creating a smooth transition with no hard edge artifacts.
5. **Send region** — Only the expanded region is loaded via SPI and refreshed:
   - `--soft`: Single GC16 pass (no blinking, may leave faint ghosting)
   - `--hard`: White-flash region (GC16 clear) then GC16 draw (cleanest, brief blink)

### When to use

| Mode | Best for | Blinking | Ghosting |
|------|----------|----------|----------|
| `--soft` | Small changes (time line, single event) | None | Possible on dark→light |
| `--hard` | Large changes, ghosting-prone content | Brief white flash | None |
| (none) | First render, full screen change | Full flash | None |

## OS Setup

The `--setup` command automatically:
1. Installs required packages (libgpiod-dev, libfreetype-dev, gcc, make + python3-pil/libgpiod/spidev/numpy)
2. Configures the boot overlay to `spi1-cs1-spidev` (frees GPIO 229 for manual CS)
3. Checks if `/dev/spidev1.1` exists (prompts reboot if not)
4. Verifies GPIO lines 226 (RST), 228 (BUSY), 229 (CS) are free for manual control — if any is kernel-claimed, prompts reboot

```bash
sudo ./it8951 --setup
```

If the overlay was changed or any GPIO is kernel-claimed, reboot:
```bash
sudo reboot
```

After reboot, verify:
```bash
sudo ./it8951 --info
```

## CLI Options

```
sudo ./it8951 --info                    Show device info + VCOM
sudo ./it8951 --clear                   Clear screen to white
sudo ./it8951 --text "Hello!"           Display text
sudo ./it8951 --text "Hi" --font-size 96
sudo ./it8951 --image photo.jpg         Display image (auto-scaled)
sudo ./it8951 --image img.jpg --brightness 1.4
sudo ./it8951 --gradient                Smooth vertical gradient
sudo ./it8951 --checker 50              50px checkerboard
sudo ./it8951 --cross 9                 9px gradient cross
sudo ./it8951 --cross 9 --cross-invert  Inverted (black bg)
sudo ./it8951 --cross 9 --cross-vertical
sudo ./it8951 --quarter                 Top-left quarter black
sudo ./it8951 --setup                   Configure OS (overlay + packages)
sudo ./it8951 --set-vcom 2510           Set VCOM to -2.51V
sudo ./it8951 --grid-test               1px grid lines every 50px (fidelity test)

# Refresh modes (use with --image)
sudo ./it8951 --image cal.png --soft            Regional GL16, no flash
sudo ./it8951 --image cal.png --hard            Regional: flash changed area, then GL16
sudo ./it8951 --image cal.png --du              Regional DU (1-bit, no flash, no ghosting)
sudo ./it8951 --image cal.png --fullscreen      Full-screen GC16 clean refresh
sudo ./it8951 --image cal.png --du-fullscreen   Full-screen DU (1-bit) clean refresh — sharp b/w
sudo ./it8951 --image cal.png --soft --border-smooth 30
```

## Architecture

```
it8951-epaper-c/
├── include/
│   └── it8951_driver.h     # Driver API header
├── src/
│   ├── it8951_driver.c     # Core SPI driver (GPIO, protocol, display)
│   ├── it8951_text.c       # Text rendering (FreeType, supersampled)
│   ├── it8951_image.c      # Image display (stb_image, dithered)
│   ├── it8951_diff.c       # Regional differential update modes
│   ├── stb_image.h         # Vendored header-only image library
│   ├── stb_image_write.h   # Vendored PNG writer (last-image cache)
│   └── main.c              # CLI entry point + patterns + OS setup
├── Makefile                # Build system
└── README.md
```

## Dependencies

| Library | Purpose | Package |
|---------|---------|---------|
| libgpiod | GPIO control | `libgpiod-dev` |
| FreeType | Text rendering | `libfreetype-dev` |
| stb_image | Image loading | Vendored (no install needed) |

## Color Conventions

- **8bpp** (text, images, checker, quarter): `0=black, 255=white` (PIL convention)
- **4bpp** (gradient, cross): `0=white, 15=black` (inverted in display_4bpp)
- Display hardware inverts colors — driver handles this automatically

## Hardware Setup

See the [Python repo README](https://github.com/sneakyjoeru/it8951-epaper-python-orangepi-zero-2w#os-configuration--bring-os-to-working-state)
for detailed OS setup instructions. The `--setup` command automates most of it.

GPIO pins: RST=226, CS=229, BUSY=228 (H616 port PH)
Boot overlay: `spi1-cs1-spidev` (frees GPIO 229 for manual CS control)

## Credits

- Based on [Waveshare IT8951-ePaper](https://github.com/waveshare/IT8951-ePaper) C code
- Python driver: [sneakyjoeru/it8951-epaper-python-orangepi-zero-2w](https://github.com/sneakyjoeru/it8951-epaper-python-orangepi-zero-2w)
- stb_image by Sean Barrett (public domain)

## License

MIT
