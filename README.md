# Hansel & Cookie — Technical Portfolio

Unreal Engine 5 기반 2인 협동 멀티플레이 게임 **Hansel & Cookie**의
핵심 시스템 구현을 정리한 기술 포트폴리오입니다.

## Tech Stack

- Unreal Engine 5
- C++
- Blueprint
- Steam Online Subsystem
- Advanced Sessions
- Listen Server
- RPC / Replication / RepNotify
- Seamless Travel

## Core Systems

### Multiplayer Networking
- Steam Session Create / Find / Join
- Lobby Player Synchronization
- Server RPC / Client RPC
- PlayerState RepNotify
- Character Selection
- Kick / Disconnect Handling

### Seamless Travel & Loading
- Pre-Travel Cinematic
- Transition Map
- GameInstance Loading State
- World Lifecycle Detection
- Loading Widget Recreation
- 2-Player Arrival Synchronization
- Character Spawn / Possess

### Co-op Gameplay & Physics
- Networked Boat System
- Rope / Tether System
- Moving Platforms
- Cooperative Interaction

### Gameplay Systems
- Checkpoint / Respawn
- Interaction
- Combat / Punch

---

## Current Source

### Loading System

`Source/LoadingSystem`

- `GI_Steam.h / .cpp`
- `LoadingPlayerControllerBase.h / .cpp`
- `LoadingOverlayWidgetBase.h / .cpp`
- `LoadingScreenConfigAsset.h / .cpp`

---

## Project

**Hansel & Cookie**

2-Player Cooperative Steam Multiplayer Game
