# Troubleshooting

### Bluetooth pairs but doesn’t type
- Forget/remove **KnomiPad** in OS Bluetooth settings.  
- Reboot the device, then pair again.  
- Test in a plain text field.

### Keeps connecting/disconnecting (Windows)
- Forget/remove the device, reboot KnomiPad, and pair again.

### Can’t reach `http://knomipad.local`
- Ensure your phone/computer is on the **same Wi‑Fi** the device joined.  
- Try the device IP from the serial monitor (PlatformIO → Monitor at 115200).  
- If Wi‑Fi credentials are wrong, you’ll see a **Wi‑Fi error** prompt at boot; retry or **Firmware Reset**.

### Icons don’t show
- Use **PNG** (200–240 px).  
- Re‑upload from the editor; the **Preview** updates immediately.  
- Click **Save** on the card.

### Factory reset
- In the editor, click **Firmware Reset**.  
- Clears **macros, icons, and Wi‑Fi settings**, then reboots to the AP (`KnomiPad`).

### Getting logs
- Connect USB and open a serial monitor at **115200** baud.  
- Useful tags: `[BOOT]`, `[HB]`, `[GAP]`, `[BLE]`, `[MACRO]`, `[UI]`.
