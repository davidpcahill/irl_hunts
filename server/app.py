#!/usr/bin/env python3
"""
IRL Hunts - Game Server v2 (Fixed)
Real-world predator vs prey game management system

Fixes:
- Admin announcements now broadcast to all player dongles
- Message system properly integrated
- Better notification routing

Run with: python app.py
Access at: http://YOUR_IP:5000
"""

from flask import Flask, render_template, request, jsonify, session, redirect, url_for, send_from_directory
from flask_socketio import SocketIO, emit, join_room
from datetime import datetime, timedelta
import secrets
import time
import threading
import os
import uuid

# =============================================================================
# APP SETUP
# =============================================================================

app = Flask(__name__)
app.secret_key = secrets.token_hex(32)
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024  # 16MB max upload

# Create uploads directory
UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'uploads')
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

# WebSocket setup
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# =============================================================================
# CONFIGURATION - CHANGE THESE!
# =============================================================================

ADMIN_PASSWORD = "hunt2024!"           # ⚠️ CHANGE THIS!
DEFAULT_CAPTURE_RSSI = -70             # Signal strength needed to capture
DEFAULT_SAFEZONE_RSSI = -75            # Signal strength to detect safe zone
PROXIMITY_ALERT_RSSI = -80             # Alert distance for prey
SIGHTING_POINTS = 25                   # Points per photo sighting
CAPTURE_PHOTO_BONUS = 50               # Bonus points for photo with capture

# =============================================================================
# GAME STATE
# =============================================================================

game = {
    "phase": "lobby",                  # lobby, countdown, running, paused, ended
    "start_time": None,
    "end_time": None,
    "duration": 30,                    # minutes
    "countdown": 10,                   # seconds
    "settings": {
        "honor_system": False,         # Allow manual safe zone toggle
        "capture_rssi": DEFAULT_CAPTURE_RSSI,
        "safezone_rssi": DEFAULT_SAFEZONE_RSSI,
        "allow_role_change": True,     # Allow role change in safe zone
    }
}

players = {}                           # {device_id: player_data}
beacons = {}                          # {beacon_id: beacon_data}
events = []                           # [{id, type, data, timestamp}]
messages = []                         # [{from, to, message, timestamp}]
moderators = set()                    # Set of device_ids
bounties = {}                         # {target_id: {points, reason}}

event_counter = 0

# =============================================================================
# UTILITY FUNCTIONS
# =============================================================================

def now():
    """Get current timestamp string"""
    return datetime.now().isoformat()

def now_str():
    """Get formatted time string"""
    return datetime.now().strftime("%H:%M:%S")

def log_event(event_type, data, broadcast=True):
    """Log a game event"""
    global event_counter
    event_counter += 1
    
    event = {
        "id": event_counter,
        "type": event_type,
        "data": data,
        "timestamp": now(),
        "time": now_str()
    }
    
    events.append(event)
    if len(events) > 500:
        events.pop(0)
    
    if broadcast:
        socketio.emit("event", event, room="all")
    
    print(f"[{now_str()}] {event_type}: {data}")
    return event

def get_player(device_id):
    """Get or create player"""
    if device_id not in players:
        players[device_id] = {
            "device_id": device_id,
            "nickname": "",
            "name": f"Player_{device_id[-4:]}",
            "profile_pic": None,
            "role": "unassigned",
            "status": "offline",
            "online": False,
            "in_safe_zone": False,
            "safe_zone_beacon": None,
            "captures": 0,
            "escapes": 0,
            "times_captured": 0,
            "sightings": 0,
            "points": 0,
            "last_seen": now(),
            "last_rssi": {},
            "notifications": []
        }
        log_event("player_join", {"id": device_id})
    return players[device_id]

def notify_player(device_id, message, msg_type="info"):
    """Send notification to a single player"""
    if device_id in players:
        notif = {"message": message, "type": msg_type, "time": now_str()}
        players[device_id]["notifications"].append(notif)
        if len(players[device_id]["notifications"]) > 20:
            players[device_id]["notifications"].pop(0)
        socketio.emit("notification", notif, room=device_id)

def notify_all_players(message, msg_type="info"):
    """Send notification to ALL players (for admin broadcasts)"""
    notif = {"message": message, "type": msg_type, "time": now_str()}
    for device_id in players:
        players[device_id]["notifications"].append(notif)
        if len(players[device_id]["notifications"]) > 20:
            players[device_id]["notifications"].pop(0)
    socketio.emit("notification", notif, room="all")
    print(f"[BROADCAST] {msg_type}: {message}")

def notify_team(role, message, msg_type="info"):
    """Send notification to all players of a specific role"""
    notif = {"message": message, "type": msg_type, "time": now_str()}
    for device_id, player in players.items():
        if player["role"] == role:
            player["notifications"].append(notif)
            if len(player["notifications"]) > 20:
                player["notifications"].pop(0)
            socketio.emit("notification", notif, room=device_id)

def calculate_points(player):
    """Calculate player's total points"""
    pts = 0
    
    if player["role"] == "pred":
        pts += player["captures"] * 100
        if player["captures"] >= 3:
            pts += 50
        if player["captures"] >= 5:
            pts += 100
        pts += player["sightings"] * SIGHTING_POINTS
    elif player["role"] == "prey":
        pts += player["escapes"] * 75
        pts += player["sightings"] * SIGHTING_POINTS
        if player["times_captured"] == 0 and game["phase"] == "ended":
            pts += 200
    
    if player["device_id"] in bounties:
        pts += bounties[player["device_id"]].get("points", 0)
    
    player["points"] = pts
    return pts

def get_leaderboard():
    """Get sorted leaderboard"""
    preds = []
    prey = []
    
    for p in players.values():
        if p["role"] == "unassigned":
            continue
        
        calculate_points(p)
        entry = {
            "device_id": p["device_id"],
            "name": p["name"],
            "profile_pic": p["profile_pic"],
            "points": p["points"],
            "captures": p["captures"],
            "escapes": p["escapes"],
            "times_captured": p["times_captured"],
            "sightings": p["sightings"],
            "status": p["status"],
            "online": p["online"],
            "in_safe_zone": p["in_safe_zone"]
        }
        
        if p["role"] == "pred":
            preds.append(entry)
        else:
            prey.append(entry)
    
    preds.sort(key=lambda x: x["points"], reverse=True)
    prey.sort(key=lambda x: x["points"], reverse=True)
    
    return {"preds": preds, "prey": prey}

def process_capture(pred_id, prey_id, rssi):
    """Process a capture attempt"""
    pred = players.get(pred_id)
    prey_p = players.get(prey_id)
    
    if not pred or not prey_p:
        return False, "Invalid players"
    if pred["role"] != "pred":
        return False, "Not a predator"
    if prey_p["role"] != "prey":
        return False, "Target is not prey"
    if prey_p["status"] == "captured":
        return False, "Already captured"
    if prey_p["in_safe_zone"]:
        return False, "Prey is in safe zone"
    if game["phase"] != "running":
        return False, "Game not running"
    if rssi < game["settings"]["capture_rssi"]:
        return False, f"Too far (RSSI: {rssi}, need > {game['settings']['capture_rssi']})"
    
    # Success!
    prey_p["status"] = "captured"
    prey_p["times_captured"] += 1
    pred["captures"] += 1
    
    log_event("capture", {
        "pred": pred["name"],
        "prey": prey_p["name"],
        "rssi": rssi
    })
    
    notify_player(prey_id, f"CAPTURED by {pred['name']}!", "danger")
    notify_player(pred_id, f"You captured {prey_p['name']}!", "success")
    
    socketio.emit("capture", {"pred": pred["name"], "prey": prey_p["name"]}, room="all")
    
    return True, f"Captured {prey_p['name']}!"

def process_escape(prey_id, beacon_id=None):
    """Process prey escape"""
    prey_p = players.get(prey_id)
    if not prey_p or prey_p["status"] != "captured":
        return False
    
    prey_p["status"] = "active"
    prey_p["escapes"] += 1
    
    beacon_name = beacons.get(beacon_id, {}).get("name", beacon_id) if beacon_id else "safe zone"
    
    log_event("escape", {"prey": prey_p["name"], "beacon": beacon_name})
    notify_player(prey_id, f"You ESCAPED via {beacon_name}!", "success")
    socketio.emit("escape", {"prey": prey_p["name"]}, room="all")
    
    return True

def update_safe_zone(device_id, beacon_rssi):
    """Update player's safe zone status based on beacon proximity"""
    player = players.get(device_id)
    if not player:
        return
    
    was_safe = player["in_safe_zone"]
    is_safe = False
    nearest_beacon = None
    
    for beacon_id, rssi in beacon_rssi.items():
        if beacon_id in beacons and beacons[beacon_id].get("active", True):
            threshold = beacons[beacon_id].get("rssi", DEFAULT_SAFEZONE_RSSI)
            if rssi >= threshold:
                is_safe = True
                nearest_beacon = beacon_id
                break
    
    if is_safe and not was_safe:
        player["in_safe_zone"] = True
        player["safe_zone_beacon"] = nearest_beacon
        beacon_name = beacons[nearest_beacon].get("name", nearest_beacon)
        
        log_event("enter_safezone", {"player": player["name"], "beacon": beacon_name})
        notify_player(device_id, f"Entered safe zone: {beacon_name}", "success")
        
        if player["status"] == "captured":
            process_escape(device_id, nearest_beacon)
    
    elif not is_safe and was_safe:
        player["in_safe_zone"] = False
        player["safe_zone_beacon"] = None
        
        log_event("leave_safezone", {"player": player["name"]})
        notify_player(device_id, "Left safe zone - you can be captured!", "warning")

# =============================================================================
# AUTHENTICATION DECORATORS
# =============================================================================

from functools import wraps

def login_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if "device_id" not in session:
            return jsonify({"error": "Not logged in"}), 401
        return f(*args, **kwargs)
    return decorated

def admin_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if not session.get("is_admin"):
            return jsonify({"error": "Admin required"}), 403
        return f(*args, **kwargs)
    return decorated

def mod_required(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        if not session.get("is_admin") and session.get("device_id") not in moderators:
            return jsonify({"error": "Moderator required"}), 403
        return f(*args, **kwargs)
    return decorated

# =============================================================================
# WEB ROUTES
# =============================================================================

@app.route("/")
def index():
    """Login page"""
    return render_template("login.html")

@app.route("/dashboard")
@login_required
def dashboard():
    """Player dashboard"""
    device_id = session["device_id"]
    player = get_player(device_id)
    player["web_connected"] = True
    is_mod = device_id in moderators or session.get("is_admin", False)
    return render_template("dashboard.html", player=player, is_mod=is_mod)

@app.route("/admin")
@admin_required
def admin():
    """Admin control panel"""
    return render_template("admin.html")

@app.route("/uploads/<filename>")
def uploaded_file(filename):
    """Serve uploaded files"""
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)

# =============================================================================
# AUTH API
# =============================================================================

@app.route("/api/login", methods=["POST"])
def api_login():
    """Player login"""
    data = request.json or {}
    device_id = data.get("device_id", "").strip().upper()
    
    if not device_id or len(device_id) < 4:
        return jsonify({"error": "Invalid device ID"}), 400
    
    session["device_id"] = device_id
    player = get_player(device_id)
    player["online"] = True
    if player["status"] == "offline":
        player["status"] = "lobby"
    
    log_event("web_login", {"player": player["name"]})
    return jsonify({"success": True, "player": player})

@app.route("/api/admin_login", methods=["POST"])
def api_admin_login():
    """Admin login"""
    data = request.json or {}
    if data.get("password") == ADMIN_PASSWORD:
        session["is_admin"] = True
        session["device_id"] = "ADMIN"
        log_event("admin_login", {})
        return jsonify({"success": True})
    return jsonify({"error": "Invalid password"}), 401

@app.route("/api/logout", methods=["POST"])
def api_logout():
    """Logout"""
    device_id = session.get("device_id")
    if device_id and device_id in players:
        players[device_id]["online"] = False
    session.clear()
    return jsonify({"success": True})

# =============================================================================
# PLAYER API
# =============================================================================

@app.route("/api/player", methods=["GET"])
@login_required
def api_get_player():
    """Get current player"""
    return jsonify(get_player(session["device_id"]))

@app.route("/api/player", methods=["PUT"])
@login_required
def api_update_player():
    """Update player info"""
    device_id = session["device_id"]
    player = get_player(device_id)
    data = request.json or {}
    
    # Nickname
    if "nickname" in data:
        nickname = str(data["nickname"]).strip()[:30]
        player["nickname"] = nickname
        old_name = player["name"]
        player["name"] = nickname if nickname else f"Player_{device_id[-4:]}"
        if old_name != player["name"]:
            log_event("name_change", {"old": old_name, "new": player["name"]})
    
    # Role
    if "role" in data and data["role"] in ["prey", "pred"]:
        can_change = (
            game["phase"] == "lobby" or
            (game["settings"]["allow_role_change"] and player["in_safe_zone"]) or
            session.get("is_admin") or
            device_id in moderators
        )
        if can_change:
            old_role = player["role"]
            player["role"] = data["role"]
            if old_role != data["role"]:
                log_event("role_change", {"player": player["name"], "from": old_role, "to": data["role"]})
        else:
            return jsonify({"error": "Must be in safe zone to change role"}), 400
    
    # Status
    if "status" in data and data["status"] in ["ready", "lobby", "dnd"]:
        player["status"] = data["status"]
    
    # Honor system safe zone
    if "in_safe_zone" in data and game["settings"]["honor_system"]:
        if data["in_safe_zone"] and not player["in_safe_zone"]:
            player["in_safe_zone"] = True
            log_event("enter_safezone_manual", {"player": player["name"]})
            if player["status"] == "captured":
                process_escape(device_id)
        elif not data["in_safe_zone"] and player["in_safe_zone"]:
            player["in_safe_zone"] = False
            log_event("leave_safezone_manual", {"player": player["name"]})
    
    player["last_seen"] = now()
    socketio.emit("player_update", player, room="all")
    return jsonify(player)

@app.route("/api/player/photo", methods=["POST"])
@login_required
def api_upload_photo():
    """Upload profile picture"""
    device_id = session["device_id"]
    player = get_player(device_id)
    
    if 'file' not in request.files:
        return jsonify({"error": "No file"}), 400
    
    file = request.files['file']
    if not file.filename:
        return jsonify({"error": "No file selected"}), 400
    
    ext = file.filename.rsplit('.', 1)[-1].lower() if '.' in file.filename else 'jpg'
    if ext not in ['jpg', 'jpeg', 'png', 'gif', 'webp']:
        return jsonify({"error": "Invalid file type"}), 400
    
    filename = f"profile_{device_id}_{uuid.uuid4().hex[:8]}.{ext}"
    file.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))
    
    player["profile_pic"] = f"/uploads/{filename}"
    log_event("photo_upload", {"player": player["name"], "type": "profile"})
    
    return jsonify({"success": True, "url": player["profile_pic"]})

@app.route("/api/players", methods=["GET"])
def api_get_players():
    """Get all players"""
    return jsonify(players)

@app.route("/api/player/notifications", methods=["GET"])
@login_required
def api_get_notifications():
    """Get and clear notifications"""
    device_id = session["device_id"]
    player = get_player(device_id)
    notifs = player.get("notifications", [])
    player["notifications"] = []
    return jsonify(notifs)

# =============================================================================
# SIGHTING API
# =============================================================================

@app.route("/api/sighting", methods=["POST"])
@login_required
def api_upload_sighting():
    """Upload photo sighting"""
    device_id = session["device_id"]
    player = get_player(device_id)
    
    if game["phase"] != "running":
        return jsonify({"error": "Game not running"}), 400
    
    target_id = request.form.get("target_id", "").upper()
    if target_id not in players:
        return jsonify({"error": "Target not found"}), 400
    
    target = players[target_id]
    
    # Validate teams
    if player["role"] == "pred" and target["role"] != "prey":
        return jsonify({"error": "Preds can only spot prey"}), 400
    if player["role"] == "prey" and target["role"] != "pred":
        return jsonify({"error": "Prey can only spot preds"}), 400
    
    if 'photo' not in request.files:
        return jsonify({"error": "No photo"}), 400
    
    photo = request.files['photo']
    if not photo.filename:
        return jsonify({"error": "No photo selected"}), 400
    
    ext = photo.filename.rsplit('.', 1)[-1].lower() if '.' in photo.filename else 'jpg'
    if ext not in ['jpg', 'jpeg', 'png', 'gif', 'webp']:
        return jsonify({"error": "Invalid file type"}), 400
    
    filename = f"sighting_{device_id}_{target_id}_{uuid.uuid4().hex[:8]}.{ext}"
    photo.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))
    
    player["sightings"] += 1
    
    log_event("sighting", {
        "spotter": player["name"],
        "target": target["name"],
        "photo": f"/uploads/{filename}"
    })
    
    notify_player(device_id, f"Sighting recorded! +{SIGHTING_POINTS} pts", "success")
    notify_player(target_id, f"You were spotted by {player['name']}!", "warning")
    
    socketio.emit("sighting", {
        "spotter": player["name"],
        "target": target["name"],
        "photo": f"/uploads/{filename}"
    }, room="all")
    
    return jsonify({"success": True, "points": SIGHTING_POINTS})

@app.route("/api/sightings", methods=["GET"])
def api_get_sightings():
    """Get all sighting photos"""
    sightings = []
    for event in events:
        if event["type"] == "sighting" and "photo" in event["data"]:
            sightings.append({
                "spotter": event["data"]["spotter"],
                "target": event["data"]["target"],
                "photo": event["data"]["photo"],
                "time": event["time"]
            })
    return jsonify(list(reversed(sightings)))

# =============================================================================
# TRACKER API
# =============================================================================

@app.route("/api/tracker/ping", methods=["POST"])
def api_tracker_ping():
    """Tracker heartbeat and status update"""
    data = request.json or {}
    device_id = data.get("device_id", "").strip().upper()
    
    if not device_id:
        return jsonify({"error": "device_id required"}), 400
    
    player = get_player(device_id)
    player["online"] = True
    player["last_seen"] = now()
    
    if player["status"] == "offline":
        player["status"] = "lobby"
        log_event("tracker_connect", {"player": player["name"]})
    
    # Update RSSI readings
    if "player_rssi" in data:
        player["last_rssi"] = data["player_rssi"]
    
    # Check safe zone beacons
    if "beacon_rssi" in data:
        update_safe_zone(device_id, data["beacon_rssi"])
    
    # Response - include pending notifications
    notifs = player.get("notifications", [])[-5:]
    player["notifications"] = []
    
    return jsonify({
        "phase": game["phase"],
        "status": player["status"],
        "role": player["role"],
        "in_safe_zone": player["in_safe_zone"],
        "notifications": notifs,
        "settings": game["settings"],
        "beacons": list(beacons.keys())
    })

@app.route("/api/tracker/capture", methods=["POST"])
def api_tracker_capture():
    """Capture attempt from tracker"""
    data = request.json or {}
    pred_id = data.get("pred_id", "").upper()
    prey_id = data.get("prey_id", "").upper()
    rssi = data.get("rssi", -100)
    
    success, msg = process_capture(pred_id, prey_id, rssi)
    return jsonify({"success": success, "message": msg})

# =============================================================================
# SAFE ZONE API
# =============================================================================

@app.route("/api/beacons", methods=["GET"])
def api_get_beacons():
    """Get all safe zone beacons"""
    return jsonify([
        {
            "id": bid,
            "name": b.get("name", bid),
            "rssi": b.get("rssi", DEFAULT_SAFEZONE_RSSI),
            "active": b.get("active", True)
        }
        for bid, b in beacons.items()
    ])

@app.route("/api/beacons", methods=["POST"])
@admin_required
def api_add_beacon():
    """Add safe zone beacon"""
    data = request.json or {}
    beacon_id = data.get("id", "").strip().upper()
    name = data.get("name", beacon_id)
    rssi = data.get("rssi", DEFAULT_SAFEZONE_RSSI)
    
    if not beacon_id:
        return jsonify({"error": "ID required"}), 400
    
    beacons[beacon_id] = {"name": name, "rssi": rssi, "active": True}
    log_event("beacon_add", {"id": beacon_id, "name": name, "rssi": rssi})
    
    return jsonify({"success": True})

@app.route("/api/beacons/<beacon_id>", methods=["PUT"])
@admin_required
def api_update_beacon(beacon_id):
    """Update beacon settings"""
    if beacon_id not in beacons:
        return jsonify({"error": "Not found"}), 404
    
    data = request.json or {}
    if "name" in data:
        beacons[beacon_id]["name"] = data["name"]
    if "rssi" in data:
        beacons[beacon_id]["rssi"] = data["rssi"]
    if "active" in data:
        beacons[beacon_id]["active"] = data["active"]
        log_event("beacon_toggle", {"id": beacon_id, "active": data["active"]})
    
    return jsonify({"success": True})

@app.route("/api/beacons/<beacon_id>", methods=["DELETE"])
@admin_required
def api_delete_beacon(beacon_id):
    """Delete beacon"""
    if beacon_id in beacons:
        del beacons[beacon_id]
        log_event("beacon_delete", {"id": beacon_id})
    return jsonify({"success": True})

# =============================================================================
# GAME API
# =============================================================================

@app.route("/api/game", methods=["GET"])
def api_get_game():
    """Get game state"""
    state = game.copy()
    state["player_count"] = len([p for p in players.values() if p["role"] != "unassigned"])
    state["pred_count"] = len([p for p in players.values() if p["role"] == "pred"])
    state["prey_count"] = len([p for p in players.values() if p["role"] == "prey"])
    state["ready_count"] = len([p for p in players.values() if p["status"] == "ready"])
    state["online_count"] = len([p for p in players.values() if p["online"]])
    
    if state["phase"] == "running" and state["end_time"]:
        remaining = (datetime.fromisoformat(state["end_time"]) - datetime.now()).total_seconds()
        state["time_remaining"] = max(0, int(remaining))
    else:
        state["time_remaining"] = 0
    
    return jsonify(state)

@app.route("/api/game/settings", methods=["PUT"])
@admin_required
def api_update_settings():
    """Update game settings"""
    data = request.json or {}
    for key in ["honor_system", "capture_rssi", "safezone_rssi", "allow_role_change"]:
        if key in data:
            game["settings"][key] = data[key]
    log_event("settings_update", game["settings"])
    return jsonify(game["settings"])

@app.route("/api/game/start", methods=["POST"])
@mod_required
def api_start_game():
    """Start the game"""
    if game["phase"] != "lobby":
        return jsonify({"error": "Not in lobby"}), 400
    
    data = request.json or {}
    game["duration"] = data.get("duration", 30)
    game["countdown"] = data.get("countdown", 10)
    game["phase"] = "countdown"
    
    # Activate ready players
    for p in players.values():
        if p["status"] == "ready" and p["role"] != "unassigned":
            p["status"] = "active"
    
    log_event("game_start", {"duration": game["duration"], "countdown": game["countdown"]})
    
    # Notify ALL players including dongles
    notify_all_players(f"Game starting in {game['countdown']} seconds!", "warning")
    socketio.emit("game_starting", {"countdown": game["countdown"]}, room="all")
    
    # Countdown thread
    def start_after_countdown():
        time.sleep(game["countdown"])
        game["phase"] = "running"
        game["start_time"] = now()
        game["end_time"] = (datetime.now() + timedelta(minutes=game["duration"])).isoformat()
        log_event("game_running", {"duration": game["duration"]})
        notify_all_players("GAME STARTED! Good luck!", "success")
        socketio.emit("game_started", {}, room="all")
    
    threading.Thread(target=start_after_countdown, daemon=True).start()
    return jsonify({"success": True})

@app.route("/api/game/pause", methods=["POST"])
@mod_required
def api_pause_game():
    """Pause game"""
    if game["phase"] == "running":
        game["phase"] = "paused"
        log_event("game_pause", {})
        notify_all_players("Game PAUSED", "warning")
        socketio.emit("game_paused", {}, room="all")
    return jsonify({"success": True})

@app.route("/api/game/resume", methods=["POST"])
@mod_required
def api_resume_game():
    """Resume game"""
    if game["phase"] == "paused":
        game["phase"] = "running"
        log_event("game_resume", {})
        notify_all_players("Game RESUMED!", "success")
        socketio.emit("game_resumed", {}, room="all")
    return jsonify({"success": True})

@app.route("/api/game/end", methods=["POST"])
@mod_required
def api_end_game():
    """End game"""
    game["phase"] = "ended"
    game["end_time"] = now()
    lb = get_leaderboard()
    log_event("game_end", {"leaderboard": lb})
    notify_all_players("GAME OVER!", "info")
    socketio.emit("game_ended", lb, room="all")
    return jsonify({"success": True, "leaderboard": lb})

@app.route("/api/game/reset", methods=["POST"])
@admin_required
def api_reset_game():
    """Reset game to lobby"""
    game["phase"] = "lobby"
    game["start_time"] = None
    game["end_time"] = None
    
    for p in players.values():
        p["status"] = "lobby" if p["online"] else "offline"
        p["captures"] = 0
        p["escapes"] = 0
        p["times_captured"] = 0
        p["sightings"] = 0
        p["points"] = 0
        p["in_safe_zone"] = False
    
    log_event("game_reset", {})
    notify_all_players("Game reset to lobby", "info")
    socketio.emit("game_reset", {}, room="all")
    return jsonify({"success": True})

# =============================================================================
# MESSAGING API - FIXED
# =============================================================================

@app.route("/api/events", methods=["GET"])
def api_get_events():
    """Get event log"""
    limit = request.args.get("limit", 100, type=int)
    return jsonify(events[-limit:])

@app.route("/api/leaderboard", methods=["GET"])
def api_get_leaderboard():
    """Get leaderboard"""
    return jsonify(get_leaderboard())

@app.route("/api/messages", methods=["GET"])
@login_required
def api_get_messages():
    """Get messages visible to player"""
    device_id = session["device_id"]
    player = get_player(device_id) if device_id != "ADMIN" else None
    
    visible = []
    for msg in messages[-100:]:
        if msg["to"] == "all":
            visible.append(msg)
        elif msg["to"] == "team" and player and msg.get("team") == player["role"]:
            visible.append(msg)
        elif msg["to"] == device_id or msg["from_id"] == device_id:
            visible.append(msg)
        elif session.get("is_admin"):
            visible.append(msg)
    
    return jsonify(visible)

@app.route("/api/messages", methods=["POST"])
@login_required
def api_send_message():
    """Send message"""
    device_id = session["device_id"]
    data = request.json or {}
    content = str(data.get("message", ""))[:500]
    to = data.get("to", "all")
    
    if not content:
        return jsonify({"error": "Empty message"}), 400
    
    if device_id == "ADMIN":
        from_name = "ADMIN"
        team = None
    else:
        player = get_player(device_id)
        from_name = player["name"]
        team = player["role"]
    
    msg = {
        "from_id": device_id,
        "from_name": from_name,
        "to": to,
        "team": team,
        "message": content,
        "time_str": now_str()
    }
    
    messages.append(msg)
    if len(messages) > 500:
        messages.pop(0)
    
    # Broadcast to web clients
    socketio.emit("message", msg, room="all")
    
    # If this is an admin announcement to all, also push to dongle notification queues
    if device_id == "ADMIN" and to == "all":
        # Extract the actual message without the prefix if it has one
        notify_all_players(content, "info")
    
    return jsonify({"success": True})

@app.route("/api/bounties", methods=["GET"])
def api_get_bounties():
    """Get active bounties"""
    return jsonify([
        {
            "target_id": tid,
            "name": players[tid]["name"] if tid in players else tid,
            "points": b["points"],
            "reason": b.get("reason", "")
        }
        for tid, b in bounties.items()
    ])

@app.route("/api/bounties", methods=["POST"])
@mod_required
def api_set_bounty():
    """Set bounty on player"""
    data = request.json or {}
    target_id = data.get("target_id", "").upper()
    points = data.get("points", 50)
    reason = data.get("reason", "")
    
    if target_id not in players:
        return jsonify({"error": "Player not found"}), 404
    
    bounties[target_id] = {"points": points, "reason": reason}
    log_event("bounty_set", {"target": players[target_id]["name"], "points": points})
    notify_player(target_id, f"Bounty of {points} points placed on you!", "warning")
    
    return jsonify({"success": True})

@app.route("/api/mod/add", methods=["POST"])
@admin_required
def api_add_mod():
    """Add moderator"""
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in players:
        moderators.add(device_id)
        log_event("mod_add", {"player": players[device_id]["name"]})
        notify_player(device_id, "You are now a moderator!", "success")
    return jsonify({"success": True})

@app.route("/api/mod/release", methods=["POST"])
@mod_required
def api_release_player():
    """Release captured player"""
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in players and players[device_id]["status"] == "captured":
        players[device_id]["status"] = "active"
        log_event("mod_release", {"player": players[device_id]["name"]})
        notify_player(device_id, "Moderator released you!", "success")
    return jsonify({"success": True})

@app.route("/api/mod/kick", methods=["POST"])
@mod_required
def api_kick_player():
    """Kick player"""
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in players:
        name = players[device_id]["name"]
        del players[device_id]
        log_event("player_kick", {"player": name})
    return jsonify({"success": True})

# =============================================================================
# WEBSOCKET
# =============================================================================

@socketio.on("connect")
def ws_connect():
    """WebSocket connection"""
    join_room("all")
    if "device_id" in session:
        join_room(session["device_id"])

# =============================================================================
# MAIN
# =============================================================================

if __name__ == "__main__":
    print("=" * 60)
    print("  🎯 IRL Hunts Game Server v2 (Fixed)")
    print("=" * 60)
    print(f"  Admin Password: {ADMIN_PASSWORD}")
    print(f"  Server URL: http://0.0.0.0:5000")
    print("=" * 60)
    print()
    
    socketio.run(app, host="0.0.0.0", port=5000, debug=True, allow_unsafe_werkzeug=True)
