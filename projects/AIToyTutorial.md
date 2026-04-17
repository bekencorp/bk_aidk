## Build Your Own AI Talking Toy
### Step 1: Open BK AIDK in Codespaces

Go to: https://github.com/bekencorp/bk_aidk

Make sure you are on branch: ai_server/v2.0.1

Login with your guihub account

Then click plus sign to Create Codespace

<img width="838" height="535" alt="image" src="https://github.com/user-attachments/assets/d8d41ec4-cf46-43d0-8dd7-0d82369d12d9" />

✅ What you should see after created
- A terminal opens
- Your path is: /workspaces/bk_aidk
<img width="3024" height="1658" alt="image" src="https://github.com/user-attachments/assets/0fc56ddf-a154-4987-b5f6-b0d384f9000f" />

### Step 2: Download project dependencies
Run:
```bash
git submodule init
git submodule update
```
Then:
```bash
cd bk_avdk
git submodule init
git submodule update
cd -
```
What this does
- This downloads all required project files.
- This may take a few minutes.

Expected output
- Multiple repositories being downloaded
- No error messages

Common mistakes
- Forgetting second bk_avdk step
- Cancelling early (downloads incomplete)

### Step 3: Switch to the correct version
```bash
git checkout ai_release/v2.0.1.8
git submodule update --recursive
```
Why this matters
- Ensures you are using the correct firmware version.

Expected output
- Branch changes successfully
- More files downloaded

Common mistakes
- Not switching branch → build fails later

### Step 4: Install ARM compiler
```bash
cd /opt
wget https://download.agora.io/rtsasdk/release/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
bzip2 -d gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
tar -xvf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar
```
What this does
- This installs the compiler used to build firmware for the device.

Expected output
- File downloaded (~100MB)
- Folder extracted in /opt

Common mistakes
- No permission → use Codespaces (not local)
- Download interrupted

### Step 5: Clone AI sample project
```bash
cd /workspaces/
git clone https://github.com/AgoraIO-Community/Conversational-AI-IOT-Sample.git
cd Conversational-AI-IOT-Sample
git checkout bk7258/ten_server
```
Expected output
- New folder appears
- Branch switched successfully

Common mistakes
- Wrong branch → missing files later

### Step 6: Replace default project 
```bash
cd ../bk_aidk
rm -rf ./projects/
cp -r ../Conversational-AI-IOT-Sample/device/projects .
```

What this does
- You are replacing the default firmware with the AI voice demo.

Expected output
- New projects/ folder created

Common mistakes
- Missing space before . in cp command
- Copy path wrong

### Step 7: Add your Wi-Fi

Open: projects/common_components/network_transfer/agora_rtc/agora_config.h

#### Update:

#define WIFI_SSID "YOUR_WIFI_NAME"

#define WIFI_PWD  "YOUR_PASSWORD"
<img width="3024" height="1650" alt="image" src="https://github.com/user-attachments/assets/04ff0b6f-6c57-4db5-8ded-98a53151d3c0" />

#### Important

Make sure:
- no extra spaces
- correct Wi-Fi name and passwords

### Step 8: Compile
```bash
pip install click future click_option_group cryptography pycryptodome
make bk7258 PROJECT=beken_genie
```
#### It might take  ~10 minutes, be patienc

Output
- After build, you should see:
bk_aidk > build > beken_genie > bk7258 > all-app.bin
<img width="592" height="620" alt="image" src="https://github.com/user-attachments/assets/530f8b13-513d-4214-9fa5-ebb63aa5b7a1" />

Common mistakes
- Build stops early → dependency issue
- Not running pip install

### Step 9: Download .bin file
In file explorer:
- find all-app.bin
- right-click → Download

Expected output
- File downloaded to your computer and exists locally

### Step 10: Connect device

Use USB cable

Plug into USB-UART port

### Step 11: Flash firmware

Use BKFIL tool:
- select .bin file
- click Flash

Expected output
- Progress bar completes
- Flash success message

Common mistakes
- Wrong port selected
- Cable not data-capable

If flashing fails
- Click Flash again
- Immediately press Reset button on board after click the download

You can download the BKFIL app through: 
​
https://dl.bekencorp.com/tools/flash for Windows

(choose BEKEN_BKFIL_V2.1.11.15_20241114)
​
https://dl.bekencorp.com/tools/bkfil/v4/gui/macos for Mac

(choose BKFIL_macos_4.0.1.25123002.zip)
<img width="2396" height="1616" alt="image" src="https://github.com/user-attachments/assets/01bd2671-85ae-44cd-bde2-57b5ace048cc" />

### Step 12: Test

press S2 button

speak to the device

Expected result
- device connects to server
- Response plays within ~2–3 seconds

Common mistakes
- No Wi-Fi connection
- Wrong credentials

### Step 13: Open TEN framework
Go to:
​
https://github.com/TEN-framework/ten-framework​

Create Codespace

### Step 14: Setup environment
```bash
cp .env.example .env
```

### Step 15: Add your keys

Edit .env:

AGORA_APP_ID=your_id

OPENAI_API_KEY=your_key

AZURE_STT_KEY=your_key

MINIMAX_TTS_API_KEY=your_key

### Step 16: Run server
```bash
cd agents/examples/voice-assistant
task install
task run
```
What you should see
- Server running logs

### Step 17: Open UI

Port 49483 → Graph

Port 3000 → Web UI

### Step 18: Test AI

Select graph: voice_assistant

Click connect

Speak

### Step 19: Get server URL

make port public

copy URL

### Step 20: Update device config

Edit:
CONFIG_AGENT_SERVER_URL
Replace with your URL.
Step 21: Rebuild + Flash again
Repeat:
make bk7258 PROJECT=beken_genie
Flash again.
