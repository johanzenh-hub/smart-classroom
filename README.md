# Smart Classroom: Render cloud dashboard

This project keeps the existing local ESP32 dashboard and adds a separate, password-protected Render dashboard. The ESP32 makes outbound HTTPS requests to Render, so no router port forwarding is needed.

## Create the Render service

1. Push this repository to GitHub and sign in to [Render](https://dashboard.render.com/).
2. Select **New + → Blueprint**, then connect the `smart-classroom` GitHub repository. Render reads `render.yaml`.
3. Before deploying, set the secret environment variables:
   - `DASHBOARD_PASSWORD`: a unique password of at least 16 characters.
   - `DEVICE_TOKEN`: a random, private value of at least 32 characters. Generate one with `node -e "console.log(require('node:crypto').randomBytes(32).toString('hex'))"`.
4. Deploy the service. Open its `https://...onrender.com` URL and sign in with username `admin` and the dashboard password.

The Render service keeps device status and pending commands in memory. A Render restart clears the latest status and any queued command. The ESP32 will publish status again after reconnecting.

## Configure and upload the ESP32

1. In Render, open the service's **Environment** page and copy the exact `DEVICE_TOKEN` value. Keep it private.
2. Copy `device_config.example.h` to `device_config.h`. This file is ignored by Git.
3. Set `WIFI_SSID`, `WIFI_PASSWORD`, `CLOUD_BASE_URL` (your Render service URL, without a trailing slash), and `DEVICE_TOKEN` in `device_config.h`.
4. Install the **WebSockets** library by Markus Sattler through Arduino IDE's Library Manager.
5. Select your ESP32 board and upload `smart_classroom.ino`. The firmware uses TLS certificate verification for cloud requests.
6. After the board connects to Wi-Fi and Render, refresh the dashboard. It should report the ESP32 online and accept automation and bulb controls.

The local dashboard remains available at the ESP32's local IP on port 80. The cloud dashboard can be reached from outside the local network.

## Security notes

- Never commit `device_config.h`, Wi-Fi credentials, the Render dashboard password, or `DEVICE_TOKEN`.
- The Render dashboard uses HTTP Basic authentication. Use a unique strong password and share the URL/password only with trusted users.
- Keep the Render service HTTPS URL. Do not use plain HTTP for cloud control.
- Cloud commands are acknowledged by the device and are idempotent. If the ESP32 is offline, the most recent requested command remains queued until the device reconnects.
- The existing direct local dashboard has no authentication and is intended only for trusted devices on the local network.
