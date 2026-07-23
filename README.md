# IT8951 E-Paper Display Driver (C) — Orange Pi Zero 2W

C port of the [Python IT8951 driver](https://github.com/sneakyjoeru/it8951-epaper)
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
scp it8951 orangepi@192.168.0.199:~/
ssh orangepi@192.168.0.199

# Run setup (configures boot overlay + installs packages)
sudo ./it8951 --setup
# Reboot if prompted

# After reboot:
sudo ./it8951 --info
sudo ./it8951 --clear
sudo ./it8951 --text "Hello, World!"
```

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
sudo ./it8951 --image photo.jpg
sudo ./it8951 --gradient
sudo ./it8951 --cross 9
sudo ./it8951 --checker 50
sudo ./it8951 --quarter
```

## OS Setup

The `--setup` command automatically:
1. Installs required packages (libgpiod-dev, libfreetype-dev, gcc, make)
2. Configures the boot overlay to `spi1-cs1-spidev` (frees GPIO 229 for manual CS)
3. Checks if `/dev/spidev1.1` exists (prompts reboot if not)

```bash
sudo ./it8951 --setup
```

If the overlay was changed, reboot:
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

See the [Python repo README](https://github.com/sneakyjoeru/it8951-epaper#os-configuration--bring-os-to-working-state)
for detailed OS setup instructions. The `--setup` command automates most of it.

GPIO pins: RST=226, CS=229, BUSY=228 (H616 port PH)
Boot overlay: `spi1-cs1-spidev` (frees GPIO 229 for manual CS control)

## Credits

- Based on [Waveshare IT8951-ePaper](https://github.com/waveshare/IT8951-ePaper) C code
- Python driver: [sneakyjoeru/it8951-epaper](https://github.com/sneakyjoeru/it8951-epaper)
- stb_image by Sean Barrett (public domain)

## License

MIT