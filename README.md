# ROS 2 3DS Controller

A native, high-performance **ROS 2 handheld remote controller** for robots, rovers, drones, and autonomous systems running on the Nintendo 3DS (3DS, 3DS XL, 2DS, New 3DS, New 3DS XL, New 2DS XL).

The 3DS connects directly to your ROS 2 network over Wi-Fi via native DDS (Cyclone DDS), requiring **no external bridge, middleman server, or companion PC**.

---

## Key Features

- **🎮 100% Physical Button Isolation:**
  - All hardware buttons (A, B, X, Y, L, R, ZL, ZR, Start, Select, D-Pad) and analog sticks (Circle Pad, C-Stick) are **strictly reserved for robot teleoperation** over standard ROS 2 `sensor_msgs/msg/Joy`.
  - Physical buttons **never** navigate menus or trigger app exits, preventing accidental commands or interrupted control during teleop.
- **📱 100% Touch Screen Management:**
  - Everything is managed from the bottom touch screen: typing commands, toggling streams, live configuration, and quitting the app (`[ EXIT ]`).
- **📺 Hardware-Accelerated Top-Screen Video:**
  - Subscribes to `sensor_msgs/msg/CompressedImage` JPEG streams from the robot.
  - Decompressed in real-time via **TurboJPEG** and tiled into PICA200 Morton 8x8 textures rendered at up to 60 FPS using Citro3D/Citro2D with aspect-ratio-preserving scaling.
  - Built-in standby HUD when waiting for camera streams.
- **⌨ Native Software Keyboard Commands:**
  - Tap `[ ⌨ Send Cmd ]` to summon the 3DS native touch keyboard (`swkbd`).
  - Publishes typed strings directly over ROS 2 `std_msgs/msg/String`.
- **⚙ Live On-Device Configuration (Touch-Driven):**
  - **Camera Topic:** Change the subscribed camera topic at runtime (re-subscribes reader on the fly).
  - **ROS Domain ID:** Switch ROS Domain ID (0–232) via on-screen numeric keypad (cleanly restarts DDS participant).
  - **ROS 2 Namespace:** Configure controller namespace (e.g., `/turtlebot4`, `/my_robot`).
  - **SD Persistence:** Settings persist across reboots in `sdmc:/3ds/ros2_3ds_controller/config.ini`.
- **🔍 ROS 2 Graph Integration:**
  - Full node and entity discovery announcement via `/ros_discovery_info` (`rmw_dds_common/msg/ParticipantEntitiesInfo`). Visible to standard CLI tools (`ros2 node list`, `ros2 topic list`).

---

## Screen Layout

```text
+------------------------------------------+
|               TOP SCREEN                 |
|               (400 x 240)                |
|                                          |
|      Live Robot Video Stream (JPEG)      |
|    or Standby HUD (Target Topic & ID)    |
|                                          |
+------------------------------------------+

+------------------------------------+
|           BOTTOM SCREEN            |
|            (320 x 240)             |
|                                    |
| [ ⌨ Cmd ]   [ ⚙ Config ]  [ EXIT ] |
| [ Joy: ON/OFF ]   [ Cam: ON/OFF ]  |
|                                    |
| (•) Circle Pad    Badges   (•) C-Stick
|     Monitor     (A,B,X,Y)     Monitor
|                                    |
| Status / Telemetry HUD             |
+------------------------------------+
```

---

## ROS 2 Topic & Interface Specifications

| Role | Default ROS 2 Topic | Wire DDS Topic | Message Type | QoS Profile |
| :--- | :--- | :--- | :--- | :--- |
| **Teleop Joy** | `<namespace>/joy` | `rt/<namespace>/joy` | `sensor_msgs/msg/Joy` | Best Effort, Volatile, Depth 1 |
| **Camera Video** | `<cam_topic>` (configurable) | `rt/<cam_topic>` | `sensor_msgs/msg/CompressedImage` | Best Effort, Volatile, Depth 1 |
| **Command String** | `<namespace>/command` | `rt/<namespace>/command` | `std_msgs/msg/String` | Reliable, Transient Local, Depth 10 |
| **ROS 2 Graph** | `/ros_discovery_info` | `ros_discovery_info` | `rmw_dds_common/msg/ParticipantEntitiesInfo` | Transient Local, Depth 1 |

---

## Standard Joy Message Specification (`sensor_msgs/msg/Joy`)

### Axes (`sequence<float> axes`, 8 axes)

| Index | Input Hardware | Direction / Range | Standard ROS 2 Semantic |
| :---: | :--- | :--- | :--- |
| **`axes[0]`** | Circle Pad X | Left: `+1.0` / Right: `-1.0` | Angular yaw / steering |
| **`axes[1]`** | Circle Pad Y | Up: `+1.0` / Down: `-1.0` | Linear forward / backward |
| **`axes[2]`** | C-Stick X (New 3DS / CPP) | Left: `+1.0` / Right: `-1.0` | Camera pan / lateral move |
| **`axes[3]`** | C-Stick Y (New 3DS / CPP) | Up: `+1.0` / Down: `-1.0` | Camera tilt / elevation |
| **`axes[4]`** | D-Pad Horizontal | Left: `+1.0` / Right: `-1.0` / Center: `0.0` | Discrete steering / trims |
| **`axes[5]`** | D-Pad Vertical | Up: `+1.0` / Down: `-1.0` / Center: `0.0` | Discrete speed / gears |
| **`axes[6]`** | Touch Screen X | Left: `-1.0` / Right: `+1.0` (`0.0` if untouched) | Touch analog slider X |
| **`axes[7]`** | Touch Screen Y | Bottom: `-1.0` / Top: `+1.0` (`0.0` if untouched) | Touch analog slider Y |

### Buttons (`sequence<int32> buttons`, 15 buttons)

| Index | Hardware Button | Value Pressed | Value Released |
| :---: | :--- | :---: | :---: |
| **`buttons[0]`** | **A** | `1` | `0` |
| **`buttons[1]`** | **B** | `1` | `0` |
| **`buttons[2]`** | **X** | `1` | `0` |
| **`buttons[3]`** | **Y** | `1` | `0` |
| **`buttons[4]`** | **L** (Shoulder) | `1` | `0` |
| **`buttons[5]`** | **R** (Shoulder) | `1` | `0` |
| **`buttons[6]`** | **ZL** (Trigger, N3DS / CPP) | `1` | `0` |
| **`buttons[7]`** | **ZR** (Trigger, N3DS / CPP) | `1` | `0` |
| **`buttons[8]`** | **Select** | `1` | `0` |
| **`buttons[9]`** | **Start** | `1` | `0` |
| **`buttons[10]`** | **Touch Screen** | `1` | `0` |
| **`buttons[11]`** | **D-Pad Up** | `1` | `0` |
| **`buttons[12]`** | **D-Pad Down** | `1` | `0` |
| **`buttons[13]`** | **D-Pad Left** | `1` | `0` |
| **`buttons[14]`** | **D-Pad Right** | `1` | `0` |

---

## Configuration & Storage

Settings can be changed dynamically on the 3DS touch screen via `[ ⚙ Config ]`, and are stored in standard INI format at:
```text
sdmc:/3ds/ros2_3ds_controller/config.ini
```

### Example `config.ini`:
```ini
# ROS 2 3DS Controller Configuration
domain_id=0
ros_namespace=/nintendo_3ds
camera_topic=/camera/image_raw/compressed
joy_publish_hz=30
joy_deadzone=0.080
peer_ip=
broadcast_ip=
joy_enabled=1
camera_enabled=1
```

- If `sdmc:/3ds/ros2_3ds_controller/config.ini` is not found, built-in defaults from `romfs:/config.ini` are loaded.
- When tapping `💾 Save Configuration to SD` in the Settings screen, changes are automatically written to the SD card.

---

## Robot Teleoperation Quickstart

### 1. Verify 3DS Topics on PC / Robot
Ensure your computer is on the same Wi-Fi network and has the same `ROS_DOMAIN_ID`:
```bash
export ROS_DOMAIN_ID=0

# Check nodes
ros2 node list
# Output: /nintendo_3ds/ros2_3ds_controller

# Echo joy inputs
ros2 topic echo /nintendo_3ds/joy

# Echo keyboard commands
ros2 topic echo /nintendo_3ds/command
```

### 2. Teleoperate a Robot (`teleop_twist_joy`)
You can map the 3DS Joy message to robot velocity (`geometry_msgs/msg/Twist`) using standard `teleop_twist_joy`:

Create a mapping config `3ds_teleop.yaml`:
```yaml
teleop_twist_joy_node:
  ros__parameters:
    axis_linear:
      x: 1                # Circle Pad Y -> forward / back
    scale_linear:
      x: 0.5              # Max speed 0.5 m/s
    axis_angular:
      yaw: 0              # Circle Pad X -> steering
    scale_angular:
      yaw: 1.0            # Max steering 1.0 rad/s
    enable_button: 4      # L button deadman switch
```

Launch teleoperation:
```bash
ros2 run teleop_twist_joy teleop_twist_joy_node \
  --ros-args -r joy:=/nintendo_3ds/joy -r cmd_vel:=/cmd_vel \
  --params-file 3ds_teleop.yaml
```

### 3. Stream Robot Camera to 3DS Top Screen
Publish compressed JPEG images to the camera topic (e.g. `/camera/image_raw/compressed`):
```bash
# Using standard image_transport compressed publisher
ros2 run image_transport republish raw compressed \
  --ros-args -r in:=/camera/image_raw -r out/compressed:=/camera/image_raw/compressed
```
The video will immediately appear on the 3DS top screen with live resolution and FPS overlay.

---

## Building from Source

### Prerequisites
- [devkitPro](https://devkitpro.org/) with `devkitARM`, `libctru`, `citro3d`, `citro2d`, `libturbojpeg`.
- CycloneDDS for 3DS (`cyclonedds_3ds` build directory adjacent to this repo).
- Host `idlc` compiler.

### Build Command
```bash
source /etc/profile.d/devkit-env.sh
make -j$(nproc)
```

Outputs:
- `ros2_3ds_controller.3dsx` (Homebrew Launcher executable)
- `ros2_3ds_controller.smdh` (3DS title & icon metadata)

To install on your 3DS:
Copy `ros2_3ds_controller.3dsx` and `ros2_3ds_controller.smdh` to `/3ds/ros2_3ds_controller/` on your SD card.

---

## License

Apache-2.0. Open-source contribution by SinfonIA & the robotics community.
