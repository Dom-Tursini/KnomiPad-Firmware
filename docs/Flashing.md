# Flashing with VS Code & PlatformIO

This is the most reliable way to get KnomiPad onto your **BIGTREETECH Knomi V2 (ESP32‑S3)**.

---

## You’ll need

- **Visual Studio Code** (free)  
- **PlatformIO** extension for VS Code  
- A **USB‑C** cable
- This repository opened in VS Code

> On Windows, allow any prompted USB driver installs. If upload fails the first time, hold **BOOT** while connecting, or hold **BOOT** and tap **RESET** once.

---

## Steps

1. **Open the folder** (contains `platformio.ini`) in VS Code.  
2. Click the **PlatformIO** icon → open your project.  
3. In the bottom **status bar**, pick the environment (e.g. `knomiv2`).  
4. Click **Build** (check icon). Wait for success.  
5. Click **Upload** (right‑arrow icon). Wait for “successfully uploaded”.  
6. Open **Monitor** (plug icon) at **115200 baud**. You should see boot logs and the **Home clock**.

### If upload fails
- Try a different USB‑C cable/port.  
- Use the **BOOT** button as described above.  
- In PlatformIO: **Clean** then **Build** again.  
- Select the correct **COM port** from PlatformIO’s device list.
