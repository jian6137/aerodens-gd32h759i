# AeroDENS Troubleshooting Guide

This guide covers common hardware jumper setups, flashing issues, MicroSD card diagnostics, and telemetry connection troubleshooting for the AeroDENS avionics system running on the GigaDevice GD32H759I-EVAL board.

---

## 1. Hardware & Jumper Configurations

The GD32H759I-EVAL development board features multiple jumper headers that configure multiplexed pin routing between on-board peripherals. Incorrect jumper positions can disable the display, prevent MicroSD detection, or cause touch sensor failures.

### LCD Display Stays Black or Backlight Fails (JP66)
- **Symptom**: The board powers on (LEDPWR lights up), but the 4.3-inch TFT LCD remains completely black or backlight is off.
- **Cause**: Jumper **JP66** controls pin PK5 routing between the LCD backlight/display control circuitry and an on-board user LED.
- **Solution**: Set jumper **JP66** to the **LCD** position. If JP66 is jumpered to the LED position, the display panel receives no backlight enable signal.

### MicroSD Card Not Detected / CMD_RESP_TIMEOUT (JP52, JP62, JP65)
- **Symptom**: The UI indicates "Insert SD Card", or internal diagnostics return `CMD_RESP_TIMEOUT (19)`.
- **Cause**: High-speed SDIO signals on the GD32H759 chip are multiplexed with other on-board interfaces (such as NAND Flash, Camera, or expansion headers). Jumpers **JP52**, **JP62**, and **JP65** route SDIO lines (PD2 CMD, PB13 D0, PC12 CLK) to the MicroSD slot.
- **Solution**:
  1. Verify that jumpers **JP52**, **JP62**, and **JP65** are all placed in the **SDIO** position.
  2. Ensure your MicroSD card is formatted as **FAT32** with standard 32 KB or 64 KB cluster allocation and an MBR partition table.
  3. Ensure the MicroSD card is firmly pushed into the push-push slot until it clicks.

### Goodix GT911 Capacitive Touch Sensor Not Responding
- **Symptom**: The display shows the UI, but tapping buttons produces no response.
- **Cause**: The capacitive touch panel uses a software I2C interface (SCL on PH6, SDA on PI11) with an active-low interrupt line and hardware reset line. A misaligned or loose FPC ribbon cable will prevent I2C initialization.
- **Solution**:
  1. Power off the board completely by disconnecting all USB cables.
  2. Carefully flip up the black retaining latch on the LCD ribbon connector.
  3. Re-seat the gold contacts of the flexible ribbon cable squarely into the connector and firmly lock the latch down.
  4. Power the board back on.

---

## 2. Firmware Flashing & OpenOCD Issues

### Port Confusion: GD-Link vs. USART
- **Symptom**: The flasher tool or OpenOCD reports "No CMSIS-DAP device found."
- **Cause**: The GD32H759I-EVAL board has two separate USB interfaces:
  1. **GD-Link / CMSIS-DAP Port**: Used for flashing, debugging, and OpenOCD programming.
  2. **USART Port**: Used for serial telemetry communication with the PC or flight simulator.
- **Solution**: Ensure your programming USB cable is connected directly to the **GD-Link / CMSIS-DAP** port when using the AeroDENS Flasher or `pio run -t upload`.

### Linux USB Permissions (Udev Rules)
- **Symptom**: On Linux, OpenOCD fails with `LIBUSB_ERROR_ACCESS` or `Error: unable to open CMSIS-DAP device 0x28e9:0x...`.
- **Cause**: The current user does not have read/write access to the GigaDevice USB vendor ID (`28e9`).
- **Solution**: Create a persistent udev rule:
  ```bash
  sudo tee /etc/udev/rules.d/99-gd32.rules << 'EOF'
  SUBSYSTEM=="usb", ATTR{idVendor}=="28e9", MODE="0666", GROUP="plugdev"
  EOF
  sudo udevadm control --reload-rules
  sudo udevadm trigger
  ```
  Ensure your user belongs to the `plugdev` group (`sudo usermod -aG plugdev $USER`).

### Target MCU Locked / Flash Sector Erase Timeout
- **Symptom**: OpenOCD stalls on `GD32H7 FMC: Erasing sector...` or reports target halt failure.
- **Solution**:
  1. Press and hold the physical **RESET** push button on the board.
  2. Start the flashing command in terminal or click **START** in the flasher.
  3. Release the **RESET** button 0.5 seconds after the command begins.
  4. If the target remains unresponsive, cycle power completely by unplugging and re-attaching the USB cable.

### Flasher Tool GUI Does Not Open (Tkinter Missing)
- **Symptom**: Running `python3 tools/flasher/flasher.py` produces `ModuleNotFoundError: No module named 'tkinter'`.
- **Cause**: Minimal Linux installations package Tkinter separately from the main Python interpreter.
- **Solution**:
  - Ubuntu / Debian: `sudo apt-get install python3-tk`
  - Fedora / RHEL: `sudo dnf install python3-tkinter`
  - Arch Linux: `sudo pacman -S tk`
  - Alternatively, use the CLI flasher: `python3 tools/flasher/flasher.py --cli -l`

---

## 3. Flight Telemetry & Simulator Binding Issues

### Simulator Binder Cannot Find Connected GD32 Board
- **Symptom**: `python3 tools/tester/simulator_binder.py` reports no GD32 serial device detected.
- **Cause**: The simulator binder communicates over the board's **USB USART** port, not the GD-Link port.
- **Solution**:
  1. Connect a USB cable from your computer to the board's **USART** port.
  2. On the AeroDENS display, select a flight and proceed to the telemetry monitoring screen (showing the "Waiting" status).
  3. Re-run `python3 tools/tester/simulator_binder.py`.

### Simulator Binder Cannot Detect Infinite Flight Sessions
- **Symptom**: The binder script listens on UDP port `15000` but never discovers an active flight session.
- **Cause**: The computer running the script and the mobile device running Infinite Flight are on different network subnets, or Infinite Flight Connect is disabled.
- **Solution**:
  1. Open Infinite Flight on your mobile device.
  2. Go to **Settings > General** and ensure **Enable Infinite Flight Connect** is toggled ON.
  3. Ensure both the mobile phone and your computer are connected to the exact same Wi-Fi router. Disable guest network isolation or VPNs that block local UDP broadcasting.
  4. Ensure your local firewall allows incoming UDP traffic on port `15000`.

---

## 4. Verification Checklists

| Component | Correct Setting | Verification Method |
| :--- | :--- | :--- |
| **JP66 (LCD)** | Jumpered to LCD pin | LCD backlight illuminates upon power-up |
| **JP52, JP62, JP65 (SDIO)** | All set to SDIO | MicroSD card mounts without `CMD_RESP_TIMEOUT` |
| **Flashing Cable** | GD-Link USB port | `python3 tools/flasher/flasher.py --cli -c` reports device found |
| **Telemetry Cable** | USART USB port | Serial port recognized under `/dev/ttyUSB*` or `COM*` |
| **IF Connect API** | Enabled on port 15000 | `simulator_binder.py` logs live speed, altitude, and OAT |

