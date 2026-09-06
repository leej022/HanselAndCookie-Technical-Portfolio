# Two-Player Pulley Lift System

UE5 C++ based cooperative pulley platform system.

Two players independently control the left and right rope handles.
Each player's movement is converted into a normalized pull value,
which is then combined into a single platform position and rotation.

## Core Features

- Server-authoritative rope grab / release
- Independent Left / Right holder state
- Distance-based pull calculation
- Local-space mechanism calculations
- Pull Alpha normalization
- Endpoint height control
- Maximum platform tilt constraint
- Kinematic platform controller
- Character rope-distance constraint
- Client soft limit / Server hard limit
- Custom network transform smoothing

## Gameplay Flow

Player Input
→ Rope Grab
→ Pull Distance
→ Pull Alpha
→ Left / Right Endpoint Heights
→ Platform Center + Pitch
→ Server Transform
→ Replicated Visual State
→ Client Smoothing

## Main Source

- `TwoDRopeLiftBar.h`
- `TwoDRopeLiftBar.cpp`

Character-side integration:
- `SteamDevelopmentCharacter.cpp`
