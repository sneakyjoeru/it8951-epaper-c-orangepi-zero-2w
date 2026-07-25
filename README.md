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
│   ├── stb_image.h         # Vendored header-only image library
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