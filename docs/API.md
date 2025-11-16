# IRL Hunts API Reference

This document describes all available API endpoints.

## Authentication

Most endpoints require authentication via session cookie. Login first, then include cookies in subsequent requests.

---

## Public Endpoints (No Auth Required)

### GET `/api/game`
Get current game state.

**Response:**
```json
{
  "phase": "lobby|countdown|running|paused|ended",
  "mode": "classic",
  "mode_name": "Classic Hunt",
  "duration": 30,
  "time_remaining": 1800,
  "player_count": 10,
  "pred_count": 4,
  "prey_count": 6,
  "ready_count": 8,
  "online_count": 10,
  "captured_count": 2,
  "emergency": false,
  "settings": {...}
}
```

### GET `/api/players`
Get all registered players.

### GET `/api/leaderboard`
Get current leaderboard rankings.

**Response:**
```json
{
  "preds": [
    {
      "device_id": "T9EF0",
      "name": "ShadowWolf",
      "points": 350,
      "captures": 3,
      "online": true
    }
  ],
  "prey": [...],
  "teams": {"Alpha": 5, "Beta": 3}
}
```

### GET `/api/beacons`
Get registered safe zone beacons.

### GET `/api/sightings`
Get photo sighting gallery.

### GET `/api/events?limit=100`
Get recent events log.

### GET `/api/stats`
Get comprehensive game statistics.

### GET `/api/achievements`
Get list of all available achievements.

### GET `/api/stats/global`
Get server-wide statistics (total games, captures, etc.)

### GET `/api/game/modes`
Get all available game modes and their configurations.

### GET `/api/game/presets`
Get predefined game setup presets.

---

## Player Endpoints (Login Required)

### POST `/api/login`
Login as a player.

**Request:**
```json
{
  "device_id": "T9EF0"
}
```

**Response:**
```json
{
  "success": true,
  "player": {...},
  "session_token": "abc123"
}
```

### POST `/api/logout`
Logout current session.

### GET `/api/player`
Get current player's data.

### PUT `/api/player`
Update player data.

**Request:**
```json
{
  "nickname": "NewName",
  "role": "prey|pred",
  "status": "ready|lobby|dnd",
  "phone_number": "555-1234",
  "consent_flags": {
    "physical_tag": false,
    "photo_visible": true,
    "location_share": true
  }
}
```

### POST `/api/player/photo`
Upload profile picture.

**Request:** multipart/form-data with `file` field

### GET `/api/player/notifications`
Get and clear pending notifications.

### GET `/api/player/achievements`
Get current player's earned achievements.

### GET `/api/player/photos`
Get list of prey this predator has photographed (Photo Safari mode).

### GET `/api/player/status`
Get full player status with game context.

### GET `/api/player/consent`
Get current consent settings.

### PUT `/api/player/consent`
Update consent settings.

### POST `/api/sighting`
Upload a photo sighting.

**Request:** multipart/form-data with:
- `target_id`: Device ID of target
- `photo`: Image file

### GET `/api/messages`
Get chat messages (filtered by role/team visibility).

### POST `/api/messages`
Send a chat message.

**Request:**
```json
{
  "to": "all|team",
  "message": "Hello everyone!"
}
```

### POST `/api/emergency`
Trigger emergency alert for yourself.

**Request:**
```json
{
  "reason": "I need help!"
}
```

### POST `/api/report`
Report a player for inappropriate behavior.

**Request:**
```json
{
  "target_id": "T1234",
  "reason": "Description of issue",
  "category": "harassment|cheating|unsafe_behavior|consent_violation|other"
}
```

---

## Tracker Endpoints (Device Communication)

### POST `/api/tracker/ping`
Tracker heartbeat and status update.

**Request:**
```json
{
  "device_id": "T9EF0",
  "player_rssi": {"T1234": -65, "T5678": -72},
  "beacon_rssi": {"SZ1A2B": -70}
}
```

**Response:**
```json
{
  "phase": "running",
  "status": "active",
  "role": "prey",
  "name": "PlayerName",
  "in_safe_zone": false,
  "team": null,
  "notifications": [...],
  "settings": {...},
  "beacons": ["SZ1A2B", "SZ3C4D"],
  "game_mode": "classic",
  "emergency": false
}
```

### POST `/api/tracker/capture`
Attempt to capture a prey.

**Request:**
```json
{
  "pred_id": "T9EF0",
  "prey_id": "T1234",
  "rssi": -65
}
```

### POST `/api/tracker/emergency`
Trigger emergency from tracker device.

---

## Moderator Endpoints (Mod/Admin Required)

### POST `/api/game/start`
Start the game.

**Request:**
```json
{
  "duration": 30,
  "countdown": 10
}
```

### POST `/api/game/pause`
Pause the game.

### POST `/api/game/resume`
Resume paused game.

### POST `/api/game/end`
End the game immediately.

### POST `/api/mod/add`
Add a moderator.

**Request:**
```json
{
  "device_id": "T1234"
}
```

### POST `/api/mod/remove`
Remove moderator status.

### POST `/api/mod/release`
Release a captured player.

### POST `/api/mod/force_role`
Force change a player's role.

### POST `/api/mod/kick`
Remove a player from the game.

### GET `/api/reports`
Get all player reports.

### POST `/api/reports/<id>/review`
Mark a report as reviewed.

### POST `/api/emergency/admin`
Trigger emergency for a specific player or system-wide.

### POST `/api/emergency/clear`
Clear active emergency.

### POST `/api/bounties`
Set a bounty on a player.

**Request:**
```json
{
  "target_id": "T1234",
  "points": 100,
  "reason": "Most elusive prey"
}
```

---

## Admin Endpoints (Admin Only)

### POST `/api/admin_login`
Login as admin.

**Request:**
```json
{
  "password": "your_password"
}
```

### POST `/api/beacons`
Add a safe zone beacon.

**Request:**
```json
{
  "id": "SZ1A2B",
  "name": "Oak Tree",
  "rssi": -75
}
```

### PUT `/api/beacons/<id>`
Update beacon settings.

### DELETE `/api/beacons/<id>`
Remove a beacon.

### PUT `/api/game/settings`
Update game settings.

**Request:**
```json
{
  "honor_system": false,
  "capture_rssi": -70,
  "safezone_rssi": -75,
  "allow_role_change": true
}
```

### PUT `/api/game/mode`
Change game mode (lobby only).

**Request:**
```json
{
  "mode": "infection"
}
```

### POST `/api/game/preset/<name>`
Apply a game preset.

### POST `/api/game/ready_all`
Set all assigned players to ready status.

### POST `/api/game/reset`
Reset game to lobby state.

### POST `/api/admin/clear_uploads`
Delete all uploaded photos.

### GET `/api/admin/server_info`
Get server health and diagnostics.

---

## WebSocket Events

### Client → Server

- `heartbeat` - Keep connection alive
- `join_room` - Join specific room for targeted messages

### Server → Client

- `connect` - Connection established
- `disconnect` - Connection lost
- `event` - Game event occurred
- `notification` - Player notification
- `message` - Chat message
- `player_update` - Player data changed
- `capture` - Capture event
- `escape` - Escape event
- `infection` - Infection mode conversion
- `sighting` - Photo sighting uploaded
- `game_starting` - Game countdown started
- `game_started` - Game is now running
- `game_paused` - Game paused
- `game_resumed` - Game resumed
- `game_ended` - Game finished
- `game_reset` - Game reset to lobby
- `emergency_alert` - Emergency triggered
- `emergency_cleared` - Emergency resolved
- `mode_change` - Game mode changed
- `profile_update` - Player profile pic updated
- `server_shutdown` - Server shutting down

---

## Error Responses
```json
{
  "error": "Description of error"
}
```

Common HTTP status codes:
- 200: Success
- 400: Bad request (invalid input)
- 401: Unauthorized (not logged in)
- 403: Forbidden (insufficient permissions)
- 404: Not found
- 413: Payload too large (file upload)
- 429: Rate limited
- 500: Server error

---

## Rate Limiting

- Login attempts: 10 per minute per IP
- Admin login: 5 failures before 5-minute lockout
- Capture attempts: 10 second cooldown per predator
- Photo sightings: 30 second cooldown per target

---

## Notes

- All device IDs are uppercase
- Timestamps are ISO 8601 format
- RSSI values are negative integers (dBm)
- Messages are limited to 500 characters
- Profile pictures limited to 16MB
- Session cookies expire after 12 hours
