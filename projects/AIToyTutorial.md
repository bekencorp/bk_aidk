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

