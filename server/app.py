#!/usr/bin/env python3
"""
IRL Hunts - Game Server v4 (Complete Game Modes + Emergency System)
Real-world predator vs prey game management system

Features:
- 6 Game Modes: Classic Hunt, Quick Match, Endurance, Team Competition, Infection, Photo Safari
- Emergency System with full player tracking
- Mode-specific scoring and mechanics
- Team management for Team Competition
- Infection mode role conversion
- Photo Safari capture requirements

Run with: python app.py
Access at: http://YOUR_IP:5000
"""

from flask import Flask, render_template, request, jsonify, session, send_from_directory
from flask_socketio import SocketIO, emit, join_room
from werkzeug.utils import secure_filename
from datetime import datetime, timedelta
from functools import wraps
import secrets
import time
import threading
import os
import uuid
import random
import sys

# =============================================================================
# LOAD CONFIGURATION
# =============================================================================

# Try to load local config first, fall back to default
try:
    from config_local import *
    print("✅ Loaded local configuration (config_local.py)")
except ImportError:
    try:
        from config import *
        print("ℹ️  Using default configuration (config.py)")
        print("   Create config_local.py for custom settings")
    except ImportError:
        print("⚠️  No config file found, using hardcoded defaults")
        # Hardcoded fallbacks
        ADMIN_PASSWORD = "hunt2024!"
        SECRET_KEY = None
        HOST = "0.0.0.0"
        PORT = 5000
        DEBUG = True
        SESSION_LIFETIME_HOURS = 12
        MAX_UPLOAD_SIZE = 16 * 1024 * 1024
        DEFAULT_CAPTURE_RSSI = -70
        DEFAULT_SAFEZONE_RSSI = -75
        SIGHTING_POINTS = 25
        SIGHTING_COOLDOWN = 30
        CAPTURE_COOLDOWN = 10
        PLAYER_TIMEOUT = 30
        MAX_PLAYERS = 100
        MAX_EVENTS = 1000
        MAX_MESSAGES = 500
        UPLOAD_FOLDER = "uploads"
        SOCKETIO_ASYNC_MODE = "threading"
        ALLOW_UNSAFE_WERKZEUG = True

# =============================================================================
# APP SETUP
# =============================================================================

app = Flask(__name__)
app.secret_key = os.environ.get('SECRET_KEY', secrets.token_hex(32))
app.config['MAX_CONTENT_LENGTH'] = 16 * 1024 * 1024
app.config['PERMANENT_SESSION_LIFETIME'] = timedelta(hours=12)

# Custom error handler for file too large
@app.errorhandler(413)
def file_too_large(e):
    return jsonify({"error": "File too large. Maximum size is 16MB."}), 413

UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'uploads')
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

# =============================================================================
# CONFIGURATION (loaded from config.py or config_local.py)
# =============================================================================

# These can be overridden by environment variables for extra security
ADMIN_PASSWORD = os.environ.get("IRLHUNTS_ADMIN_PASSWORD", ADMIN_PASSWORD if 'ADMIN_PASSWORD' in dir() else "hunt2024!")

# Use config values or defaults
if 'DEFAULT_CAPTURE_RSSI' not in dir():
    DEFAULT_CAPTURE_RSSI = -70
if 'DEFAULT_SAFEZONE_RSSI' not in dir():
    DEFAULT_SAFEZONE_RSSI = -75
if 'SIGHTING_POINTS' not in dir():
    SIGHTING_POINTS = 25
if 'SIGHTING_COOLDOWN' not in dir():
    SIGHTING_COOLDOWN = 30
if 'CAPTURE_COOLDOWN' not in dir():
    CAPTURE_COOLDOWN = 10
if 'PLAYER_TIMEOUT' not in dir():
    PLAYER_TIMEOUT = 30
if 'MAX_PLAYERS' not in dir():
    MAX_PLAYERS = 100
if 'MAX_EVENTS' not in dir():
    MAX_EVENTS = 1000
if 'MAX_MESSAGES' not in dir():
    MAX_MESSAGES = 500

# =============================================================================
# GAME MODES CONFIGURATION
# =============================================================================

GAME_MODES = {
    "classic": {
        "name": "Classic Hunt",
        "description": "Standard 30-minute hunt with balanced scoring",
        "duration": 30,
        "capture_points": 100,
        "escape_points": 75,
        "sighting_points": 25,
        "survival_bonus": 200,
        "capture_bonus_3": 50,
        "capture_bonus_5": 100,
        "infection": False,
        "photo_required": False,
        "team_mode": False
    },
    "quick": {
        "name": "Quick Match",
        "description": "Fast-paced 15-minute game",
        "duration": 15,
        "capture_points": 100,
        "escape_points": 75,
        "sighting_points": 25,
        "survival_bonus": 150,
        "capture_bonus_3": 50,
        "capture_bonus_5": 100,
        "infection": False,
        "photo_required": False,
        "team_mode": False
    },
    "endurance": {
        "name": "Endurance",
        "description": "60-minute marathon with high survival bonus",
        "duration": 60,
        "capture_points": 100,
        "escape_points": 75,
        "sighting_points": 25,
        "survival_bonus": 500,
        "capture_bonus_3": 50,
        "capture_bonus_5": 100,
        "infection": False,
        "photo_required": False,
        "team_mode": False
    },
    "team": {
        "name": "Team Competition",
        "description": "Predators compete in teams for most captures",
        "duration": 30,
        "capture_points": 100,
        "escape_points": 75,
        "sighting_points": 25,
        "survival_bonus": 200,
        "capture_bonus_3": 50,
        "capture_bonus_5": 100,
        "infection": False,
        "photo_required": False,
        "team_mode": True,
        "teams": ["Alpha", "Beta", "Gamma", "Delta"]
    },
    "infection": {
        "name": "Infection Mode",
        "description": "Captured prey become predators! Last survivor wins big",
        "duration": 30,
        "capture_points": 50,
        "escape_points": 0,
        "sighting_points": 25,
        "survival_bonus": 1000,
        "capture_bonus_3": 25,
        "capture_bonus_5": 50,
        "infection": True,
        "photo_required": False,
        "team_mode": False
    },
    "photo_safari": {
        "name": "Photo Safari",
        "description": "Must photograph prey before capture! Double photo points",
        "duration": 30,
        "capture_points": 100,
        "escape_points": 75,
        "sighting_points": 50,
        "survival_bonus": 200,
        "capture_bonus_3": 50,
        "capture_bonus_5": 100,
        "infection": False,
        "photo_required": True,
        "team_mode": False
    }
}

# =============================================================================
# GAME STATE
# =============================================================================

game = {
    "phase": "lobby",
    "mode": "classic",
    "start_time": None,
    "end_time": None,
    "duration": 30,
    "countdown": 10,
    "emergency": False,
    "emergency_by": None,
    "emergency_reason": "",
    "emergency_time": None,
    "settings": {
        "honor_system": False,
        "capture_rssi": DEFAULT_CAPTURE_RSSI,
        "safezone_rssi": DEFAULT_SAFEZONE_RSSI,
        "allow_role_change": True,
    }
}

players = {}
beacons = {}
events = []
messages = []
moderators = set()
bounties = {}
capture_cooldowns = {}
sighting_cooldowns = {}  # Track sighting cooldowns per player-target pair
team_scores = {"Alpha": 0, "Beta": 0, "Gamma": 0, "Delta": 0}
event_counter = 0
background_thread_stop = False

# =============================================================================
# UTILITY FUNCTIONS
# =============================================================================

def now():
    return datetime.now().isoformat()

def now_str():
    return datetime.now().strftime("%H:%M:%S")


def validate_game_action(required_phase=None, block_on_emergency=True):
    """Helper to validate common game action preconditions"""
    if required_phase and game["phase"] != required_phase:
        return False, f"Game must be in {required_phase} phase"
    if block_on_emergency and game["emergency"]:
        return False, "Action blocked: Emergency active"
    return True, "OK"

def get_mode_config():
    return GAME_MODES.get(game["mode"], GAME_MODES["classic"])

def log_event(event_type, data, broadcast=True):
    global event_counter
    event_counter += 1
    event = {
        "id": event_counter,
        "type": event_type,
        "data": data,
        "timestamp": now(),
        "time_str": now_str()
    }
    events.append(event)
    if len(events) > MAX_EVENTS:
        events.pop(0)
    if broadcast:
        socketio.emit("event", event, room="all")
    
    # Enhanced logging for emergencies
    if event_type == "emergency_triggered":
        print(f"[{now_str()}] *** EMERGENCY ***")
        print(f"  Player: {data.get('player_name', 'Unknown')}")
        print(f"  Device: {data.get('device_id', 'Unknown')}")
        print(f"  Reason: {data.get('reason', 'Not specified')}")
        print(f"  Location: {data.get('location_hint', 'Unknown')}")
        nearest = data.get('nearest_to_emergency', [])
        if nearest:
            print(f"  Nearest players: {', '.join([f"{p['name']} ({p['rssi']}dB)" for p in nearest[:5]])}")
    else:
        print(f"[{now_str()}] {event_type}: {data}")
    return event

MAX_PLAYERS = 100  # Reasonable limit for memory/performance

def get_player(device_id):
    if device_id not in players:
        if len(players) >= MAX_PLAYERS:
            # Don't create new player if at limit
            return None
        players[device_id] = {
            "device_id": device_id,
            "nickname": "",
            "name": f"Player_{device_id[-4:]}",
            "profile_pic": None,
            "role": "unassigned",
            "original_role": "unassigned",
            "status": "offline",
            "online": False,
            "in_safe_zone": False,
            "safe_zone_beacon": None,
            "team": None,
            "captures": 0,
            "escapes": 0,
            "times_captured": 0,
            "sightings": 0,
            "points": 0,
            "last_seen": now(),
            "last_ping": time.time(),
            "last_rssi": {},
            "last_location_hint": "",
            "nearby_players": [],
            "notifications": [],
            "captured_by": None,
            "has_photo_of": [],
            "infections": 0,
            "consent_flags": {
                "physical_tag": False,  # Allow physical tagging (tapping shoulder)
                "photo_visible": True,  # Allow photos to be taken
                "location_share": True  # Share location hints
            },
            "ready": False,
            "phone_number": ""  # Emergency contact number (optional)
        }
        log_event("player_join", {"id": device_id, "name": f"Player_{device_id[-4:]}"})
    return players[device_id]

def notify_player(device_id, message, msg_type="info"):
    if device_id in players:
        notif = {"message": message, "type": msg_type, "time_str": now_str()}
        players[device_id]["notifications"].append(notif)
        if len(players[device_id]["notifications"]) > 20:
            players[device_id]["notifications"].pop(0)
        socketio.emit("notification", notif, room=device_id)

def notify_all_players(message, msg_type="info"):
    notif = {"message": message, "type": msg_type, "time_str": now_str()}
    for device_id in players:
        players[device_id]["notifications"].append(notif)
        if len(players[device_id]["notifications"]) > 20:
            players[device_id]["notifications"].pop(0)
    socketio.emit("notification", notif, room="all")

def calculate_points(player):
    mode_config = get_mode_config()
    pts = 0
    
    if player["role"] == "pred":
        pts += player["captures"] * mode_config["capture_points"]
        if player["captures"] >= 3:
            pts += mode_config["capture_bonus_3"]
        if player["captures"] >= 5:
            pts += mode_config["capture_bonus_5"]
        pts += player["sightings"] * mode_config["sighting_points"]
        if mode_config["infection"]:
            pts += player.get("infections", 0) * 25
    elif player["role"] == "prey":
        pts += player["escapes"] * mode_config["escape_points"]
        pts += player["sightings"] * mode_config["sighting_points"]
        if player["times_captured"] == 0 and game["phase"] == "ended":
            pts += mode_config["survival_bonus"]
        if mode_config["infection"] and game["phase"] == "ended":
            prey_count = len([p for p in players.values() if p["role"] == "prey"])
            if prey_count == 1 and player["role"] == "prey":
                pts += mode_config["survival_bonus"]
    
    # Team winning bonus (calculated at game end)
    if mode_config["team_mode"] and game["phase"] == "ended" and player.get("team"):
        if team_scores:
            winning_team = max(team_scores, key=team_scores.get)
            if player["team"] == winning_team and team_scores[winning_team] > 0:
                pts += 300  # Winning team bonus per member
    
    player["points"] = pts
    return pts

def get_leaderboard():
    preds, prey = [], []
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
            "in_safe_zone": p["in_safe_zone"],
            "team": p.get("team"),
            "infections": p.get("infections", 0),
            "consent_physical": p.get("consent_physical", False),
            "consent_photo": p.get("consent_photo", True)
        }
        if p["role"] == "pred":
            preds.append(entry)
        else:
            prey.append(entry)
    preds.sort(key=lambda x: x["points"], reverse=True)
    prey.sort(key=lambda x: x["points"], reverse=True)
    
    teams = {}
    if get_mode_config()["team_mode"]:
        teams = dict(team_scores)
    
    return {"preds": preds, "prey": prey, "teams": teams}

def assign_team(player):
    mode_config = get_mode_config()
    if not mode_config["team_mode"] or player["role"] != "pred":
        return
    
    team_sizes = {team: 0 for team in mode_config["teams"]}
    for p in players.values():
        if p["role"] == "pred" and p.get("team") in team_sizes:
            team_sizes[p["team"]] += 1
    
    smallest_team = min(team_sizes, key=team_sizes.get)
    player["team"] = smallest_team
    notify_player(player["device_id"], f"You've been assigned to Team {smallest_team}!", "info")

def process_capture(pred_id, prey_id, rssi):
    mode_config = get_mode_config()
    pred = players.get(pred_id)
    prey_p = players.get(prey_id)
    
    if not pred or not prey_p:
        return False, "Invalid players"
    if pred_id == prey_id:
        return False, "Cannot capture yourself"
    if pred["role"] != "pred":
        return False, "Not a predator"
    if prey_p["role"] != "prey":
        return False, "Target is not prey"
    if prey_p["status"] == "captured" and not mode_config["infection"]:
        return False, "Already captured"
    if prey_p["in_safe_zone"] and not mode_config["infection"]:
        return False, "Prey is in safe zone"
    if game["phase"] != "running":
        return False, "Game not running"
    if game["emergency"]:
        return False, "Game paused for emergency"
    if rssi < game["settings"]["capture_rssi"]:
        return False, f"Too far (RSSI: {rssi}, need > {game['settings']['capture_rssi']})"
    
    if mode_config["photo_required"]:
        if prey_id not in pred.get("has_photo_of", []):
            return False, "Must photograph prey first! Take a sighting photo before capture."
    
    current_time = time.time()
    if pred_id in capture_cooldowns:
        time_since_last = current_time - capture_cooldowns[pred_id]
        if time_since_last < CAPTURE_COOLDOWN:
            return False, f"Wait {int(CAPTURE_COOLDOWN - time_since_last)}s"
    
    capture_cooldowns[pred_id] = current_time
    
    if mode_config["infection"]:
        prey_p["role"] = "pred"
        prey_p["status"] = "active"
        prey_p["times_captured"] += 1
        pred["captures"] += 1
        pred["infections"] = pred.get("infections", 0) + 1
        
        log_event("infection", {"pred": pred["name"], "prey": prey_p["name"], "rssi": rssi})
        notify_player(prey_id, f"INFECTED by {pred['name']}! You are now a PREDATOR!", "danger")
        notify_player(pred_id, f"You infected {prey_p['name']}! They're now hunting!", "success")
        notify_all_players(f"🦠 {prey_p['name']} has been INFECTED!", "warning")
        socketio.emit("infection", {"pred": pred["name"], "prey": prey_p["name"]}, room="all")
        
        prey_left = len([p for p in players.values() if p["role"] == "prey"])
        if prey_left == 0:
            game["phase"] = "ended"
            game["end_time"] = now()
            # Find top infector
            top_infector = max(players.values(), key=lambda p: p.get("infections", 0), default=None)
            top_name = top_infector["name"] if top_infector else "Unknown"
            log_event("game_auto_end", {"reason": "All prey infected!", "top_infector": top_name})
            notify_all_players(f"🦠 ALL PREY INFECTED! {top_name} was top infector!", "success")
            socketio.emit("game_ended", get_leaderboard(), room="all")
        
        return True, f"Infected {prey_p['name']}! They're now a predator!"
    
    prey_p["status"] = "captured"
    prey_p["times_captured"] += 1
    prey_p["captured_by"] = pred["name"]
    pred["captures"] += 1
    
    if mode_config["team_mode"] and pred.get("team"):
        team_scores[pred["team"]] = team_scores.get(pred["team"], 0) + 1
    
    log_event("capture", {"pred": pred["name"], "prey": prey_p["name"], "rssi": rssi, "team": pred.get("team")})
    notify_player(prey_id, f"CAPTURED by {pred['name']}!", "danger")
    notify_player(pred_id, f"You captured {prey_p['name']}!", "success")
    socketio.emit("capture", {"pred": pred["name"], "prey": prey_p["name"], "team": pred.get("team")}, room="all")
    
    # Check for bounty collection
    if prey_id in bounties:
        bounty_points = bounties[prey_id]["points"]
        pred["points"] += bounty_points
        log_event("bounty_collected", {"pred": pred["name"], "prey": prey_p["name"], "points": bounty_points})
        notify_player(pred_id, f"💰 BOUNTY COLLECTED! +{bounty_points} bonus points!", "success")
        notify_all_players(f"💰 {pred['name']} collected the bounty on {prey_p['name']}!", "info")
        del bounties[prey_id]
    
    return True, f"Captured {prey_p['name']}!"

def process_escape(prey_id, beacon_id=None):
    mode_config = get_mode_config()
    
    if mode_config["infection"]:
        return False
    
    prey_p = players.get(prey_id)
    if not prey_p or prey_p["status"] != "captured":
        return False
    
    prey_p["status"] = "active"
    prey_p["escapes"] += 1
    prey_p["captured_by"] = None
    
    beacon_name = beacons.get(beacon_id, {}).get("name", beacon_id) if beacon_id else "safe zone"
    log_event("escape", {"prey": prey_p["name"], "beacon": beacon_name})
    notify_player(prey_id, f"You ESCAPED via {beacon_name}!", "success")
    socketio.emit("escape", {"prey": prey_p["name"]}, room="all")
    return True

def update_safe_zone(device_id, beacon_rssi):
    player = players.get(device_id)
    if not player:
        return
    
    was_safe = player["in_safe_zone"]
    is_safe = False
    nearest_beacon = None
    best_rssi = -999
    
    for beacon_id, rssi in beacon_rssi.items():
        if beacon_id in beacons and beacons[beacon_id].get("active", True):
            threshold = beacons[beacon_id].get("rssi", DEFAULT_SAFEZONE_RSSI)
            if rssi >= threshold and rssi > best_rssi:
                is_safe = True
                nearest_beacon = beacon_id
                best_rssi = rssi
    
    if is_safe and not was_safe:
        player["in_safe_zone"] = True
        player["safe_zone_beacon"] = nearest_beacon
        beacon_name = beacons[nearest_beacon].get("name", nearest_beacon)
        player["last_location_hint"] = f"Near {beacon_name}"
        log_event("enter_safezone", {"player": player["name"], "beacon": beacon_name})
        notify_player(device_id, f"Entered safe zone: {beacon_name}", "success")
        if player["status"] == "captured":
            process_escape(device_id, nearest_beacon)
    elif not is_safe and was_safe:
        player["in_safe_zone"] = False
        player["safe_zone_beacon"] = None
        log_event("leave_safezone", {"player": player["name"]})
        notify_player(device_id, "Left safe zone - you can be captured!", "warning")

def trigger_emergency(device_id, reason=""):
    player = players.get(device_id)
    if not player:
        return False, "Player not found"
    
    game["emergency"] = True
    game["emergency_by"] = player["name"]
    game["emergency_reason"] = reason
    game["emergency_time"] = now()
    
    if game["phase"] == "running":
        game["phase"] = "paused"
    
    nearest_players = []
    for pid, p in players.items():
        if pid != device_id and p["online"]:
            if device_id in p.get("last_rssi", {}):
                rssi = p["last_rssi"][device_id]
                nearest_players.append({"name": p["name"], "device_id": pid, "rssi": rssi})
    
    nearest_players.sort(key=lambda x: x["rssi"], reverse=True)
    
    emergency_data = {
        "player_name": player["name"],
        "device_id": device_id,
        "reason": reason,
        "location_hint": player.get("last_location_hint", "Unknown"),
        "nearby_players": player.get("nearby_players", []),
        "nearest_to_emergency": nearest_players[:5],
        "time": now_str(),
        "phone_number": player.get("phone_number", "")  # For emergency contact
    }
    
    log_event("emergency_triggered", emergency_data)
    
    emergency_msg = f"🚨 EMERGENCY! {player['name']} needs help! Location: {player.get('last_location_hint', 'Unknown')}"
    notify_all_players(emergency_msg, "danger")
    
    socketio.emit("emergency_alert", emergency_data, room="all")
    
    return True, "Emergency triggered"

def clear_emergency():
    game["emergency"] = False
    old_by = game["emergency_by"]
    game["emergency_by"] = None
    game["emergency_reason"] = ""
    game["emergency_time"] = None
    
    log_event("emergency_cleared", {"was_by": old_by})
    notify_all_players("✅ Emergency cleared. Game can resume.", "success")
    socketio.emit("emergency_cleared", {}, room="all")
    
    return True

def check_player_timeouts():
    current_time = time.time()
    for device_id, player in players.items():
        if player["online"]:
            time_since_ping = current_time - player.get("last_ping", 0)
            if time_since_ping > PLAYER_TIMEOUT:
                player["online"] = False
                if game["phase"] == "running":
                    log_event("player_timeout", {"player": player["name"], "device_id": device_id})

def check_game_end():
    if game["phase"] == "running" and game["end_time"] and not game["emergency"]:
        end_dt = datetime.fromisoformat(game["end_time"])
        if datetime.now() >= end_dt:
            game["phase"] = "ended"
            lb = get_leaderboard()
            log_event("game_auto_end", {"reason": "Timer expired"})
            notify_all_players("TIME'S UP! Game ended!", "info")
            socketio.emit("game_ended", lb, room="all")

def cleanup_old_cooldowns():
    """Clean up old cooldown entries to prevent memory leak"""
    current_time = time.time()
    # Clean capture cooldowns older than 5 minutes (thread-safe copy)
    try:
        expired_captures = [k for k, v in list(capture_cooldowns.items()) if current_time - v > 300]
        for k in expired_captures:
            capture_cooldowns.pop(k, None)
        # Clean sighting cooldowns older than 10 minutes
        expired_sightings = [k for k, v in list(sighting_cooldowns.items()) if current_time - v > 600]
        for k in expired_sightings:
            sighting_cooldowns.pop(k, None)
    except RuntimeError:
        # Dictionary changed size during iteration - will clean next cycle
        pass

def background_tasks():
    global background_thread_stop
    cleanup_counter = 0
    while not background_thread_stop:
        try:
            check_player_timeouts()
            check_game_end()
            cleanup_counter += 1
            if cleanup_counter >= 12:  # Every minute (12 * 5 seconds)
                cleanup_old_cooldowns()
                cleanup_counter = 0
        except Exception as e:
            print(f"[ERROR] Background task: {e}")
        time.sleep(5)

def start_background_thread():
    global background_thread_stop
    background_thread_stop = False
    thread = threading.Thread(target=background_tasks, daemon=True)
    thread.start()

# =============================================================================
# AUTH DECORATORS
# =============================================================================

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
    return render_template("login.html")

@app.route("/dashboard")
@login_required
def dashboard():
    device_id = session["device_id"]
    player = get_player(device_id)
    is_mod = device_id in moderators or session.get("is_admin", False)
    mode_config = get_mode_config()
    return render_template("dashboard.html", player=player, is_mod=is_mod, 
                          mode_config=mode_config, game_mode=game["mode"])

@app.route("/admin")
@admin_required
def admin():
    return render_template("admin.html", game_modes=GAME_MODES)

@app.route("/uploads/<filename>")
def uploaded_file(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename)

# =============================================================================
# AUTH API
# =============================================================================

@app.route("/api/login", methods=["POST"])
def api_login():
    data = request.json or {}
    device_id = data.get("device_id", "").strip().upper()
    if not device_id or len(device_id) < 4:
        return jsonify({"error": "Invalid device ID"}), 400
    if len(device_id) > 10:
        return jsonify({"error": "Device ID too long"}), 400
    
    session["device_id"] = device_id
    session.permanent = True
    player = get_player(device_id)
    
    if player is None:
        return jsonify({"error": "Server at capacity (max players reached)"}), 503
    player["online"] = True
    player["last_ping"] = time.time()
    if player["status"] == "offline":
        player["status"] = "lobby"
    
    log_event("web_login", {"player": player["name"]})
    return jsonify({"success": True, "player": player})

@app.route("/api/admin_login", methods=["POST"])
def api_admin_login():
    data = request.json or {}
    if data.get("password") == ADMIN_PASSWORD:
        session["is_admin"] = True
        session["device_id"] = "ADMIN"
        session.permanent = True
        log_event("admin_login", {})
        return jsonify({"success": True})
    return jsonify({"error": "Invalid password"}), 401

@app.route("/api/logout", methods=["POST"])
def api_logout():
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
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin account"}), 400
    return jsonify(get_player(device_id))

@app.route("/api/player", methods=["PUT"])
@login_required
def api_update_player():
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin cannot be a player"}), 400
    
    player = get_player(device_id)
    data = request.json or {}
    
    if "nickname" in data:
        nickname = str(data["nickname"]).strip()[:30]
        # Allow alphanumeric, spaces, underscores, hyphens, and common punctuation
        nickname = "".join(c for c in nickname if c.isalnum() or c in " _-.'!?")
        nickname = nickname.strip()
        if len(nickname) < 2:
            nickname = ""
        if len(nickname) > 30:
            return jsonify({"error": "Nickname too long (max 30 characters)"}), 400
        player["nickname"] = nickname
        old_name = player["name"]
        player["name"] = nickname if nickname else f"Player_{device_id[-4:]}"
        if old_name != player["name"]:
            log_event("name_change", {"old": old_name, "new": player["name"]})
    
    if "role" in data and data["role"] in ["prey", "pred"]:
        mode_config = get_mode_config()
        can_change = (
            game["phase"] == "lobby" or
            (game["settings"]["allow_role_change"] and player["in_safe_zone"] and not mode_config["infection"]) or
            session.get("is_admin") or
            device_id in moderators
        )
        
        # In infection mode during game, no manual role changes
        if mode_config["infection"] and game["phase"] == "running" and not session.get("is_admin"):
            return jsonify({"error": "Cannot change role in Infection mode"}), 400
        if can_change:
            old_role = player["role"]
            player["role"] = data["role"]
            player["original_role"] = data["role"]
            if old_role != data["role"]:
                log_event("role_change", {"player": player["name"], "from": old_role, "to": data["role"]})
                assign_team(player)
        else:
            return jsonify({"error": "Must be in safe zone to change role"}), 400
    
    if "status" in data and data["status"] in ["ready", "lobby", "dnd"]:
        old_status = player["status"]
        player["status"] = data["status"]
        # Track ready state separately for toggle functionality
        if data["status"] == "ready":
            player["ready"] = True
        elif data["status"] == "lobby":
            player["ready"] = False
        # Log ready state changes
        if old_status != data["status"]:
            if data["status"] == "ready":
                log_event("player_ready", {"player": player["name"]})
            elif old_status == "ready" and data["status"] == "lobby":
                log_event("player_unready", {"player": player["name"]})
    
    # Update consent flags
    if "consent_physical" in data:
        player["consent_physical"] = bool(data["consent_physical"])
    if "consent_photo" in data:
        player["consent_photo"] = bool(data["consent_photo"])
    if "consent_chat" in data:
        player["consent_chat"] = bool(data["consent_chat"])
    
    if "in_safe_zone" in data and game["settings"]["honor_system"]:
        if data["in_safe_zone"] and not player["in_safe_zone"]:
            player["in_safe_zone"] = True
            if player["status"] == "captured":
                process_escape(device_id)
        elif not data["in_safe_zone"] and player["in_safe_zone"]:
            player["in_safe_zone"] = False
    
    # Update consent flags if provided
    if "consent_flags" in data and isinstance(data["consent_flags"], dict):
        for key in ["physical_tag", "photo_visible", "location_share"]:
            if key in data["consent_flags"]:
                player["consent_flags"][key] = bool(data["consent_flags"][key])
        log_event("consent_update", {"player": player["name"], "flags": player["consent_flags"]})
    
    # Update phone number if provided (sanitize input)
    if "phone_number" in data:
        phone = str(data["phone_number"]).strip()
        # Only allow digits, spaces, dashes, parentheses, plus sign
        phone = "".join(c for c in phone if c.isdigit() or c in " -+().")
        phone = phone[:20]  # Max 20 characters
        player["phone_number"] = phone
        if phone:
            log_event("phone_update", {"player": player["name"], "has_phone": True})
    
    player["last_seen"] = now()
    player["last_ping"] = time.time()
    socketio.emit("player_update", player, room="all")
    return jsonify(player)

@app.route("/api/player/photo", methods=["POST"])
@login_required
def api_upload_photo():
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin cannot upload photo"}), 400
    
    player = get_player(device_id)
    if 'file' not in request.files:
        return jsonify({"error": "No file"}), 400
    
    file = request.files['file']
    if not file.filename:
        return jsonify({"error": "No file selected"}), 400
    
    ext = file.filename.rsplit('.', 1)[-1].lower() if '.' in file.filename else 'jpg'
    if ext not in ['jpg', 'jpeg', 'png', 'gif', 'webp']:
        return jsonify({"error": "Invalid file type"}), 400
    
    base_filename = f"profile_{device_id}_{uuid.uuid4().hex[:8]}.{ext}"
    filename = secure_filename(base_filename)
    file.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))
    
    player["profile_pic"] = f"/uploads/{filename}"
    log_event("photo_upload", {"player": player["name"], "type": "profile"})
    return jsonify({"success": True, "url": player["profile_pic"]})

@app.route("/api/players", methods=["GET"])
def api_get_players():
    return jsonify(players)

@app.route("/api/player/notifications", methods=["GET"])
@login_required
def api_get_notifications():
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify([])
    player = get_player(device_id)
    notifs = player.get("notifications", [])
    player["notifications"] = []
    return jsonify(notifs)

# =============================================================================
# EMERGENCY API
# =============================================================================

@app.route("/api/emergency", methods=["POST"])
@login_required
def api_trigger_emergency():
    """Player triggers emergency for themselves (so they can be found)"""
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin cannot trigger emergency - use /api/emergency/admin instead"}), 400
    
    data = request.json or {}
    reason = str(data.get("reason", "Emergency button pressed"))[:200]
    
    success, msg = trigger_emergency(device_id, reason)
    if success:
        return jsonify({"success": True, "message": msg})
    return jsonify({"error": msg}), 400

@app.route("/api/emergency/admin", methods=["POST"])
@mod_required
def api_admin_trigger_emergency():
    """Admin/mod can trigger emergency for a specific player or as system alert"""
    data = request.json or {}
    target_id = data.get("target_id", "").strip().upper()
    reason = str(data.get("reason", "Admin triggered emergency"))[:200]
    
    # If target_id specified, trigger for that player
    if target_id and target_id in players:
        success, msg = trigger_emergency(target_id, reason)
        return jsonify({"success": success, "message": msg})
    
    # Otherwise, trigger as system-wide emergency
    game["emergency"] = True
    game["emergency_by"] = "SYSTEM (Admin)"
    game["emergency_reason"] = reason
    game["emergency_time"] = now()
    
    if game["phase"] == "running":
        game["phase"] = "paused"
    
    emergency_data = {
        "player_name": "SYSTEM (Admin)",
        "device_id": "ADMIN",
        "reason": reason,
        "location_hint": "Admin triggered - check all areas",
        "nearby_players": [],
        "nearest_to_emergency": [],
        "time": now_str()
    }
    
    log_event("emergency_triggered", emergency_data)
    notify_all_players(f"🚨 EMERGENCY! {reason}", "danger")
    socketio.emit("emergency_alert", emergency_data, room="all")
    
    return jsonify({"success": True, "message": "Emergency triggered by admin"})

@app.route("/api/emergency/clear", methods=["POST"])
@mod_required
def api_clear_emergency():
    clear_emergency()
    return jsonify({"success": True})

@app.route("/api/emergency/status", methods=["GET"])
def api_emergency_status():
    if not game["emergency"]:
        return jsonify({"emergency": False})
    
    emergency_player = None
    for p in players.values():
        if p["name"] == game["emergency_by"]:
            emergency_player = p
            break
    
    nearest = []
    if emergency_player:
        for pid, p in players.items():
            if p["device_id"] != emergency_player["device_id"] and p["online"]:
                if emergency_player["device_id"] in p.get("last_rssi", {}):
                    rssi = p["last_rssi"][emergency_player["device_id"]]
                    nearest.append({"name": p["name"], "rssi": rssi})
        nearest.sort(key=lambda x: x["rssi"], reverse=True)
    
    return jsonify({
        "emergency": True,
        "by": game["emergency_by"],
        "reason": game["emergency_reason"],
        "time": game["emergency_time"],
        "location_hint": emergency_player.get("last_location_hint", "Unknown") if emergency_player else "Unknown",
        "nearest_players": nearest[:10]
    })

# =============================================================================
# SIGHTING API
# =============================================================================

@app.route("/api/sighting", methods=["POST"])
@login_required
def api_upload_sighting():
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin cannot upload sightings"}), 400
    
    player = get_player(device_id)
    mode_config = get_mode_config()
    
    if game["phase"] != "running":
        return jsonify({"error": "Game not running"}), 400
    
    if game["emergency"]:
        return jsonify({"error": "Game paused for emergency"}), 400
    
    target_id = request.form.get("target_id", "").upper()
    if target_id not in players:
        return jsonify({"error": "Target not found"}), 400
    
    target = players[target_id]
    if player["role"] == "pred" and target["role"] != "prey":
        return jsonify({"error": "Preds can only spot prey"}), 400
    if player["role"] == "prey" and target["role"] != "pred":
        return jsonify({"error": "Prey can only spot preds"}), 400
    
    # Check consent for photography
    if not target.get("consent_flags", {}).get("photo_visible", True):
        return jsonify({"error": f"{target['name']} has disabled photo visibility"}), 403
    
    # Check sighting cooldown for this specific target
    cooldown_key = f"{device_id}_{target_id}"
    current_time = time.time()
    if cooldown_key in sighting_cooldowns:
        time_since_last = current_time - sighting_cooldowns[cooldown_key]
        if time_since_last < SIGHTING_COOLDOWN:
            remaining = int(SIGHTING_COOLDOWN - time_since_last)
            return jsonify({"error": f"Wait {remaining}s before spotting {target['name']} again"}), 400
    sighting_cooldowns[cooldown_key] = current_time
    
    if 'photo' not in request.files:
        return jsonify({"error": "No photo"}), 400
    
    photo = request.files['photo']
    if not photo.filename:
        return jsonify({"error": "No photo selected"}), 400
    
    ext = photo.filename.rsplit('.', 1)[-1].lower() if '.' in photo.filename else 'jpg'
    if ext not in ['jpg', 'jpeg', 'png', 'gif', 'webp']:
        return jsonify({"error": "Invalid file type"}), 400
    
    base_filename = f"sighting_{device_id}_{target_id}_{uuid.uuid4().hex[:8]}.{ext}"
    filename = secure_filename(base_filename)
    filepath = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    
    # Check file doesn't already exist (very unlikely with UUID but safe)
    if os.path.exists(filepath):
        return jsonify({"error": "Please try again"}), 400
    
    photo.save(filepath)
    player["sightings"] += 1
    
    if mode_config["photo_required"] and player["role"] == "pred":
        if target_id not in player.get("has_photo_of", []):
            player["has_photo_of"].append(target_id)
            notify_player(device_id, f"✅ You can now capture {target['name']}!", "success")
    
    sighting_points = mode_config["sighting_points"]
    
    log_event("sighting", {
        "spotter": player["name"],
        "target": target["name"],
        "photo": f"/uploads/{filename}"
    })
    
    notify_player(device_id, f"Sighting recorded! +{sighting_points} pts", "success")
    notify_player(target_id, f"You were spotted by {player['name']}!", "warning")
    socketio.emit("sighting", {
        "spotter": player["name"],
        "target": target["name"],
        "photo": f"/uploads/{filename}"
    }, room="all")
    
    return jsonify({"success": True, "points": sighting_points})

@app.route("/api/sightings", methods=["GET"])
def api_get_sightings():
    sightings = []
    for event in events:
        if event["type"] == "sighting" and "photo" in event["data"]:
            sightings.append({
                "spotter": event["data"]["spotter"],
                "target": event["data"]["target"],
                "photo": event["data"]["photo"],
                "time_str": event["time_str"]
            })
    return jsonify(list(reversed(sightings)))

# =============================================================================
# TRACKER API
# =============================================================================

@app.route("/api/tracker/ping", methods=["POST"])
def api_tracker_ping():
    data = request.json or {}
    device_id = data.get("device_id", "").strip().upper()
    if not device_id:
        return jsonify({"error": "device_id required"}), 400
    
    player = get_player(device_id)
    player["online"] = True
    player["last_seen"] = now()
    player["last_ping"] = time.time()
    
    if player["status"] == "offline":
        player["status"] = "lobby"
        log_event("tracker_connect", {"player": player["name"]})
    
    if "player_rssi" in data:
        player["last_rssi"] = data["player_rssi"]
        nearby = []
        for pid, rssi in data["player_rssi"].items():
            if pid in players:
                nearby.append({"id": pid, "name": players[pid]["name"], "rssi": rssi})
        nearby.sort(key=lambda x: x["rssi"], reverse=True)
        player["nearby_players"] = nearby[:5]
        
        if nearby:
            # Respect location sharing consent
            if player.get("consent_flags", {}).get("location_share", True):
                player["last_location_hint"] = f"Near {nearby[0]['name']} ({nearby[0]['rssi']}dB)"
            else:
                player["last_location_hint"] = "Location hidden by player preference"
    
    if "beacon_rssi" in data:
        update_safe_zone(device_id, data["beacon_rssi"])
    
    notifs = player.get("notifications", [])[-5:]
    player["notifications"] = []
    
    mode_config = get_mode_config()
    
    # Calculate consent badge for tracker display
    consent_flags = player.get("consent_flags", {})
    badge_parts = []
    if consent_flags.get("physical_tag", False):
        badge_parts.append("T")
    if not consent_flags.get("photo_visible", True):
        badge_parts.append("NP")
    if not consent_flags.get("location_share", True):
        badge_parts.append("NL")
    consent_badge = "".join(badge_parts) if badge_parts else "STD"
    
    # Return None for empty team (avoids "null" string in ArduinoJson)
    team_value = player.get("team") if player.get("team") else None
    
    return jsonify({
        "phase": game["phase"],
        "status": player["status"],
        "role": player["role"],
        "name": player["name"],
        "in_safe_zone": player["in_safe_zone"],
        "team": team_value,
        "notifications": notifs,
        "settings": game["settings"],
        "beacons": list(beacons.keys()),
        "game_mode": game["mode"],
        "game_mode_name": mode_config["name"],
        "emergency": game["emergency"],
        "emergency_by": game["emergency_by"],
        "infection_mode": mode_config["infection"],
        "photo_required": mode_config["photo_required"],
        "has_photo_of": player.get("has_photo_of", []),
        "consent_flags": consent_flags,
        "consent_badge": consent_badge,
        "consent_physical": consent_flags.get("physical_tag", False),
        "consent_photo": consent_flags.get("photo_visible", True),
        "ready": player.get("ready", False)
    })

@app.route("/api/tracker/capture", methods=["POST"])
def api_tracker_capture():
    data = request.json or {}
    pred_id = data.get("pred_id", "").upper()
    prey_id = data.get("prey_id", "").upper()
    rssi = data.get("rssi", -100)
    success, msg = process_capture(pred_id, prey_id, rssi)
    return jsonify({"success": success, "message": msg})

@app.route("/api/tracker/emergency", methods=["POST"])
def api_tracker_emergency():
    data = request.json or {}
    device_id = data.get("device_id", "").strip().upper()
    reason = data.get("reason", "Emergency triggered from tracker")
    
    if not device_id or device_id not in players:
        return jsonify({"error": "Invalid device"}), 400
    
    success, msg = trigger_emergency(device_id, reason)
    return jsonify({"success": success, "message": msg})

# =============================================================================
# BEACON API
# =============================================================================

@app.route("/api/beacons", methods=["GET"])
@app.route("/api/safezones", methods=["GET"])
def api_get_beacons():
    return jsonify([
        {
            "id": bid,
            "beacon_id": bid,
            "name": b.get("name", bid),
            "rssi": b.get("rssi", DEFAULT_SAFEZONE_RSSI),
            "rssi_threshold": b.get("rssi", DEFAULT_SAFEZONE_RSSI),
            "active": b.get("active", True)
        }
        for bid, b in beacons.items()
    ])

@app.route("/api/beacons", methods=["POST"])
@app.route("/api/safezones", methods=["POST"])
@admin_required
def api_add_beacon():
    data = request.json or {}
    beacon_id = data.get("id", data.get("beacon_id", "")).strip().upper()
    name = data.get("name", beacon_id)
    rssi = data.get("rssi", data.get("rssi_threshold", DEFAULT_SAFEZONE_RSSI))
    
    if not beacon_id:
        return jsonify({"error": "ID required"}), 400
    
    if len(beacon_id) < 4 or len(beacon_id) > 10:
        return jsonify({"error": "Beacon ID must be 4-10 characters"}), 400
    
    try:
        rssi = int(rssi)
    except (ValueError, TypeError):
        return jsonify({"error": "RSSI must be a valid integer"}), 400
    
    if rssi > -30 or rssi < -100:
        return jsonify({"error": "RSSI threshold must be between -100 and -30 (e.g., -75)"}), 400
    
    beacons[beacon_id] = {"name": name, "rssi": rssi, "active": True}
    log_event("beacon_add", {"id": beacon_id, "name": name, "rssi": rssi})
    return jsonify({"success": True})

@app.route("/api/beacons/<beacon_id>", methods=["PUT"])
@app.route("/api/safezones/<beacon_id>", methods=["PUT"])
@admin_required
def api_update_beacon(beacon_id):
    beacon_id = beacon_id.upper()
    if beacon_id not in beacons:
        return jsonify({"error": "Not found"}), 404
    
    data = request.json or {}
    if "name" in data:
        beacons[beacon_id]["name"] = data["name"]
    if "rssi" in data:
        beacons[beacon_id]["rssi"] = data["rssi"]
    if "rssi_threshold" in data:
        beacons[beacon_id]["rssi"] = data["rssi_threshold"]
    if "active" in data:
        beacons[beacon_id]["active"] = data["active"]
    
    return jsonify({"success": True})

@app.route("/api/beacons/<beacon_id>", methods=["DELETE"])
@app.route("/api/safezones/<beacon_id>", methods=["DELETE"])
@admin_required
def api_delete_beacon(beacon_id):
    beacon_id = beacon_id.upper()
    if beacon_id in beacons:
        del beacons[beacon_id]
        log_event("beacon_delete", {"id": beacon_id})
    return jsonify({"success": True})

# =============================================================================
# GAME API
# =============================================================================

@app.route("/api/game", methods=["GET"])
def api_get_game():
    mode_config = get_mode_config()
    state = game.copy()
    state["mode_config"] = mode_config
    state["mode_name"] = mode_config["name"]
    state["player_count"] = len([p for p in players.values() if p["role"] != "unassigned"])
    state["pred_count"] = len([p for p in players.values() if p["role"] == "pred"])
    state["prey_count"] = len([p for p in players.values() if p["role"] == "prey"])
    state["ready_count"] = len([p for p in players.values() if p["status"] == "ready"])
    state["online_count"] = len([p for p in players.values() if p["online"]])
    state["captured_count"] = len([p for p in players.values() if p["status"] == "captured"])
    state["team_scores"] = team_scores
    
    if state["phase"] == "running" and state["end_time"] and not state["emergency"]:
        remaining = (datetime.fromisoformat(state["end_time"]) - datetime.now()).total_seconds()
        state["time_remaining"] = max(0, int(remaining))
    else:
        state["time_remaining"] = 0
    
    return jsonify(state)

@app.route("/api/game/modes", methods=["GET"])
def api_get_game_modes():
    return jsonify(GAME_MODES)

@app.route("/api/game/mode", methods=["PUT"])
@admin_required
def api_set_game_mode():
    if game["phase"] != "lobby":
        return jsonify({"error": "Can only change mode in lobby"}), 400
    
    data = request.json or {}
    mode = data.get("mode", "classic")
    
    if mode not in GAME_MODES:
        return jsonify({"error": "Invalid game mode"}), 400
    
    game["mode"] = mode
    mode_config = GAME_MODES[mode]
    game["duration"] = mode_config["duration"]
    
    if mode_config["team_mode"]:
        for p in players.values():
            if p["role"] == "pred":
                assign_team(p)
    else:
        for p in players.values():
            p["team"] = None
    
    for team in team_scores:
        team_scores[team] = 0
    
    log_event("mode_change", {"mode": mode, "name": mode_config["name"]})
    notify_all_players(f"Game mode set to: {mode_config['name']}", "info")
    socketio.emit("mode_change", {"mode": mode, "config": mode_config}, room="all")
    
    return jsonify({"success": True, "mode": mode, "config": mode_config})

@app.route("/api/game/settings", methods=["PUT"])
@admin_required
def api_update_settings():
    data = request.json or {}
    for key in ["honor_system", "capture_rssi", "safezone_rssi", "allow_role_change"]:
        if key in data:
            game["settings"][key] = data[key]
    log_event("settings_update", game["settings"])
    return jsonify(game["settings"])

@app.route("/api/game/ready_all", methods=["POST"])
@admin_required
def api_ready_all():
    """Set all assigned players to ready status"""
    count = 0
    for p in players.values():
        if p["role"] != "unassigned" and p["online"] and p["status"] == "lobby":
            p["status"] = "ready"
            count += 1
    log_event("bulk_ready", {"count": count})
    notify_all_players(f"Admin readied {count} players", "info")
    return jsonify({"success": True, "count": count})

@app.route("/api/game/start", methods=["POST"])
@mod_required
def api_start_game():
    if game["phase"] != "lobby":
        return jsonify({"error": "Not in lobby"}), 400
    
    if game["emergency"]:
        return jsonify({"error": "Cannot start game during emergency"}), 400
    
    pred_count = len([p for p in players.values() if p["role"] == "pred" and p["status"] == "ready"])
    prey_count = len([p for p in players.values() if p["role"] == "prey" and p["status"] == "ready"])
    online_count = len([p for p in players.values() if p["online"]])
    
    if pred_count == 0:
        return jsonify({"error": "Need at least one ready predator"}), 400
    if prey_count == 0:
        return jsonify({"error": "Need at least one ready prey"}), 400
    if online_count < 2:
        return jsonify({"error": "Need at least 2 online players"}), 400
    
    # Warn about imbalanced games
    if pred_count > prey_count * 2:
        log_event("game_imbalance", {"preds": pred_count, "prey": prey_count, "warning": "Too many predators"})
    
    mode_config = get_mode_config()
    data = request.json or {}
    game["duration"] = min(max(data.get("duration", mode_config["duration"]), 5), 180)
    game["countdown"] = min(max(data.get("countdown", 10), 5), 60)
    game["phase"] = "countdown"
    
    if mode_config["photo_required"]:
        for p in players.values():
            p["has_photo_of"] = []
    
    for p in players.values():
        if p["status"] == "ready" and p["role"] != "unassigned":
            p["status"] = "active"
    
    log_event("game_start", {
        "duration": game["duration"], 
        "countdown": game["countdown"],
        "mode": game["mode"],
        "mode_name": mode_config["name"]
    })
    notify_all_players(f"{mode_config['name']} starting in {game['countdown']} seconds!", "warning")
    socketio.emit("game_starting", {"countdown": game["countdown"], "mode": game["mode"]}, room="all")
    
    def start_after_countdown():
        time.sleep(game["countdown"])
        if game["phase"] == "countdown":
            game["phase"] = "running"
            game["start_time"] = now()
            game["end_time"] = (datetime.now() + timedelta(minutes=game["duration"])).isoformat()
            log_event("game_running", {"duration": game["duration"], "mode": game["mode"]})
            notify_all_players(f"🎮 {mode_config['name']} STARTED! Good luck!", "success")
            socketio.emit("game_started", {"mode": game["mode"]}, room="all")
    
    threading.Thread(target=start_after_countdown, daemon=True).start()
    return jsonify({"success": True})

@app.route("/api/game/pause", methods=["POST"])
@mod_required
def api_pause_game():
    if game["phase"] == "running":
        game["phase"] = "paused"
        # Store remaining time so we can restore it on resume
        if game.get("end_time"):
            remaining = (datetime.fromisoformat(game["end_time"]) - datetime.now()).total_seconds()
            game["paused_remaining"] = max(0, remaining)
        log_event("game_pause", {})
        notify_all_players("Game PAUSED", "warning")
        socketio.emit("game_paused", {}, room="all")
    elif game["phase"] == "countdown":
        game["phase"] = "lobby"
        log_event("countdown_cancel", {})
        notify_all_players("Countdown CANCELLED", "warning")
        socketio.emit("countdown_cancelled", {}, room="all")
    return jsonify({"success": True})

@app.route("/api/game/resume", methods=["POST"])
@mod_required
def api_resume_game():
    if game["emergency"]:
        return jsonify({"error": "Cannot resume during emergency. Clear emergency first."}), 400
    
    if game["phase"] == "paused":
        game["phase"] = "running"
        # Restore timer from paused state
        if game.get("paused_remaining"):
            game["end_time"] = (datetime.now() + timedelta(seconds=game["paused_remaining"])).isoformat()
            del game["paused_remaining"]
        log_event("game_resume", {})
        notify_all_players("Game RESUMED!", "success")
        socketio.emit("game_resumed", {}, room="all")
    return jsonify({"success": True})

@app.route("/api/game/end", methods=["POST"])
@mod_required
def api_end_game():
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
    game["phase"] = "lobby"
    game["mode"] = "classic"
    game["start_time"] = None
    game["end_time"] = None
    game["emergency"] = False
    game["emergency_by"] = None
    game["emergency_reason"] = ""
    game["emergency_time"] = None
    
    for p in players.values():
        p["status"] = "lobby" if p["online"] else "offline"
        p["role"] = "unassigned"
        p["original_role"] = "unassigned"
        p["team"] = None
        p["captures"] = 0
        p["escapes"] = 0
        p["times_captured"] = 0
        p["sightings"] = 0
        p["points"] = 0
        p["in_safe_zone"] = False
        p["safe_zone_beacon"] = None
        p["captured_by"] = None
        p["has_photo_of"] = []
        p["infections"] = 0
        p["nearby_players"] = []
        p["last_rssi"] = {}
        # Keep nickname and profile_pic - they worked hard on those!
    
    for team in team_scores:
        team_scores[team] = 0
    
    capture_cooldowns.clear()
    sighting_cooldowns.clear()
    log_event("game_reset", {})
    notify_all_players("Game reset to lobby", "info")
    socketio.emit("game_reset", {}, room="all")
    return jsonify({"success": True})

# =============================================================================
# PHOTO TRACKING API
# =============================================================================

@app.route("/api/player/photos", methods=["GET"])
@login_required
def api_get_player_photos():
    """Get list of prey this predator has photographed (for Photo Safari mode)"""
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify([])
    
    player = get_player(device_id)
    if player["role"] != "pred":
        return jsonify([])
    
    photographed = []
    for prey_id in player.get("has_photo_of", []):
        if prey_id in players:
            photographed.append({
                "device_id": prey_id,
                "name": players[prey_id]["name"],
                "can_capture": True
            })
    
    return jsonify(photographed)

@app.route("/api/player/status", methods=["GET"])
@login_required
def api_get_my_full_status():
    """Get current player's full status for dashboard refresh"""
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin account"}), 400
    
    player = get_player(device_id)
    if not player:
        return jsonify({"error": "Player not found"}), 404
    
    # Update last seen
    player["last_seen"] = now()
    player["last_ping"] = time.time()
    player["online"] = True
    
    # Calculate current points
    calculate_points(player)
    
    mode_config = get_mode_config()
    
    return jsonify({
        "player": player,
        "game_phase": game["phase"],
        "game_mode": game["mode"],
        "mode_config": mode_config,
        "emergency": game["emergency"],
        "emergency_by": game.get("emergency_by", "")
    })

# =============================================================================
# GAME STATS API
# =============================================================================

@app.route("/api/stats", methods=["GET"])
def api_get_stats():
    """Get comprehensive game statistics"""
    mode_config = get_mode_config()
    
    total_captures = sum(p["captures"] for p in players.values())
    total_escapes = sum(p["escapes"] for p in players.values())
    total_sightings = sum(p["sightings"] for p in players.values())
    
    # Top performers
    top_pred = max((p for p in players.values() if p["role"] == "pred"), 
                   key=lambda x: x["captures"], default=None)
    top_prey = max((p for p in players.values() if p["role"] == "prey"), 
                   key=lambda x: x["escapes"], default=None)
    never_caught = [p["name"] for p in players.values() 
                    if p["role"] == "prey" and p["times_captured"] == 0]
    
    stats = {
        "game_mode": game["mode"],
        "game_mode_name": mode_config["name"],
        "phase": game["phase"],
        "total_players": len([p for p in players.values() if p["role"] != "unassigned"]),
        "online_players": len([p for p in players.values() if p["online"]]),
        "total_captures": total_captures,
        "total_escapes": total_escapes,
        "total_sightings": total_sightings,
        "active_bounties": len(bounties),
        "safe_zones": len([b for b in beacons.values() if b.get("active", True)]),
        "top_predator": {"name": top_pred["name"], "captures": top_pred["captures"]} if top_pred else None,
        "top_prey": {"name": top_prey["name"], "escapes": top_prey["escapes"]} if top_prey else None,
        "never_caught": never_caught,
        "emergency_active": game["emergency"],
    }
    
    if mode_config["infection"]:
        stats["prey_remaining"] = len([p for p in players.values() if p["role"] == "prey"])
        stats["total_infections"] = sum(p.get("infections", 0) for p in players.values())
    
    if mode_config["team_mode"]:
        stats["team_scores"] = dict(team_scores)
    
    return jsonify(stats)

@app.route("/api/history", methods=["GET"])
def api_get_game_history():
    """Get event history for analysis"""
    event_type = request.args.get("type", None)
    limit = min(request.args.get("limit", 100, type=int), 500)
    
    if event_type:
        filtered = [e for e in events if e["type"] == event_type]
        return jsonify(filtered[-limit:])
    
    return jsonify(events[-limit:])

# =============================================================================
# MESSAGING API
# =============================================================================

@app.route("/api/events", methods=["GET"])
def api_get_events():
    limit = request.args.get("limit", 100, type=int)
    return jsonify(events[-limit:])

@app.route("/api/leaderboard", methods=["GET"])
def api_get_leaderboard():
    return jsonify(get_leaderboard())

@app.route("/api/messages", methods=["GET"])
@login_required
def api_get_messages():
    device_id = session["device_id"]
    
    if session.get("is_admin"):
        return jsonify(messages[-100:])
    
    player = get_player(device_id)
    visible = []
    mode_config = get_mode_config()
    
    for msg in messages[-100:]:
        if msg["to"] == "all":
            visible.append(msg)
        elif msg["to"].startswith("team_") and mode_config["team_mode"]:
            # Team mode: team_Alpha, team_Beta, etc.
            if msg["to"] == f"team_{player.get('team')}":
                visible.append(msg)
        elif msg["to"].startswith("role_"):
            # Role-based: role_pred, role_prey
            if msg["to"] == f"role_{player['role']}":
                visible.append(msg)
        elif msg["to"] == "team" and msg.get("team") == player["role"]:
            # Legacy support
            visible.append(msg)
        elif msg["to"] == device_id or msg["from_id"] == device_id:
            visible.append(msg)
    
    return jsonify(visible)

@app.route("/api/messages", methods=["POST"])
@login_required
def api_send_message():
    device_id = session["device_id"]
    data = request.json or {}
    msg_content = str(data.get("message", ""))[:500].strip()
    to = data.get("to", "all")
    
    if not msg_content:
        return jsonify({"error": "Empty message"}), 400
    
    if device_id == "ADMIN":
        from_name = "📢 ADMIN"
        team = None
        role = None
    else:
        player = get_player(device_id)
        from_name = player["name"]
        team = player.get("team")  # Actual team name (Alpha, Beta, etc.)
        role = player["role"]
    
    # Route team chat properly
    if to == "team":
        mode_config = get_mode_config()
        if device_id == "ADMIN":
            return jsonify({"error": "Admin cannot send team messages"}), 400
        if mode_config["team_mode"] and team:
            to = f"team_{team}"  # Route to specific team
        else:
            to = f"role_{role}"  # Route to role (pred/prey)
    
    msg = {
        "from_id": device_id,
        "from_name": from_name,
        "to": to,
        "team": team,
        "role": role,
        "message": msg_content,
        "time_str": now_str()
    }
    
    messages.append(msg)
    if len(messages) > MAX_MESSAGES:
        messages.pop(0)
    
    socketio.emit("message", msg, room="all")
    
    if device_id == "ADMIN" and to == "all":
        notify_all_players(msg_content, "info")
    
    return jsonify({"success": True})

@app.route("/api/bounties", methods=["GET"])
def api_get_bounties():
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
    data = request.json or {}
    target_id = data.get("target_id", "").upper()
    points = min(max(data.get("points", 50), 10), 500)
    reason = str(data.get("reason", ""))[:100]
    
    if target_id not in players:
        return jsonify({"error": "Player not found"}), 404
    
    bounties[target_id] = {"points": points, "reason": reason}
    log_event("bounty_set", {"target": players[target_id]["name"], "points": points})
    notify_player(target_id, f"Bounty of {points} points placed on you!", "warning")
    return jsonify({"success": True})

@app.route("/api/mod/add", methods=["POST"])
@admin_required
def api_add_mod():
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in players:
        moderators.add(device_id)
        log_event("mod_add", {"player": players[device_id]["name"]})
        notify_player(device_id, "You are now a moderator!", "success")
    return jsonify({"success": True})

@app.route("/api/mod/remove", methods=["POST"])
@admin_required
def api_remove_mod():
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in moderators:
        moderators.remove(device_id)
        if device_id in players:
            log_event("mod_remove", {"player": players[device_id]["name"]})
            notify_player(device_id, "Moderator status removed", "info")
    return jsonify({"success": True})

@app.route("/api/mod/force_role", methods=["POST"])
@mod_required
def api_force_role():
    """Force a player's role (admin override)"""
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    new_role = data.get("role", "")
    
    if device_id not in players:
        return jsonify({"error": "Player not found"}), 404
    if new_role not in ["prey", "pred", "unassigned"]:
        return jsonify({"error": "Invalid role"}), 400
    
    player = players[device_id]
    old_role = player["role"]
    player["role"] = new_role
    player["original_role"] = new_role
    
    log_event("force_role", {"player": player["name"], "from": old_role, "to": new_role})
    notify_player(device_id, f"Your role was changed to {new_role} by moderator", "warning")
    
    # Assign team if needed
    if get_mode_config()["team_mode"] and new_role == "pred":
        assign_team(player)
    elif new_role != "pred":
        player["team"] = None
    
    return jsonify({"success": True})

@app.route("/api/mod/release", methods=["POST"])
@mod_required
def api_release_player():
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in players and players[device_id]["status"] == "captured":
        players[device_id]["status"] = "active"
        players[device_id]["captured_by"] = None
        log_event("mod_release", {"player": players[device_id]["name"]})
        notify_player(device_id, "Moderator released you!", "success")
    return jsonify({"success": True})

@app.route("/api/mod/kick", methods=["POST"])
@mod_required
def api_kick_player():
    data = request.json or {}
    device_id = data.get("device_id", "").upper()
    if device_id in players:
        name = players[device_id]["name"]
        
        # Cleanup associated data
        if device_id in bounties:
            del bounties[device_id]
        if device_id in capture_cooldowns:
            del capture_cooldowns[device_id]
        # Clean sighting cooldowns involving this player
        to_remove = [k for k in sighting_cooldowns if device_id in k]
        for k in to_remove:
            del sighting_cooldowns[k]
        if device_id in moderators:
            moderators.remove(device_id)
        
        del players[device_id]
        log_event("player_kick", {"player": name})
        notify_all_players(f"{name} was removed from the game", "info")
    return jsonify({"success": True})


# =============================================================================
# CONSENT API
# =============================================================================

@app.route("/api/player/consent", methods=["GET"])
@login_required
def api_get_consent():
    """Get current player's consent settings"""
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin account"}), 400
    
    player = get_player(device_id)
    return jsonify(player.get("consent_flags", {
        "physical_tag": False,
        "photo_visible": True,
        "location_share": True
    }))

@app.route("/api/player/consent", methods=["PUT"])
@login_required
def api_update_consent():
    """Update player's consent settings"""
    device_id = session["device_id"]
    if device_id == "ADMIN":
        return jsonify({"error": "Admin account"}), 400
    
    player = get_player(device_id)
    data = request.json or {}
    
    if "consent_flags" not in player:
        player["consent_flags"] = {
            "physical_tag": False,
            "photo_visible": True,
            "location_share": True
        }
    
    for key in ["physical_tag", "photo_visible", "location_share"]:
        if key in data:
            player["consent_flags"][key] = bool(data[key])
    
    log_event("consent_update", {"player": player["name"], "flags": player["consent_flags"]})
    socketio.emit("player_update", player, room="all")
    
    return jsonify({"success": True, "consent_flags": player["consent_flags"]})

@app.route("/api/players/consent_summary", methods=["GET"])
def api_consent_summary():
    """Get summary of consent settings for all players (for badges)"""
    summary = {}
    for device_id, player in players.items():
        if player["role"] != "unassigned":
            flags = player.get("consent_flags", {})
            # Create consent indicator string for badge
            indicators = []
            if flags.get("physical_tag", False):
                indicators.append("T")  # Touch OK
            if not flags.get("photo_visible", True):
                indicators.append("NP")  # No Photo
            if not flags.get("location_share", True):
                indicators.append("NL")  # No Location
            
            summary[device_id] = {
                "name": player["name"],
                "consent_badge": "".join(indicators) if indicators else "STD",  # Standard
                "flags": flags
            }
    return jsonify(summary)

# =============================================================================
# WEBSOCKET
# =============================================================================

@socketio.on("connect")
def ws_connect():
    join_room("all")
    if "device_id" in session:
        join_room(session["device_id"])
        if session["device_id"] in players:
            players[session["device_id"]]["online"] = True
            players[session["device_id"]]["last_ping"] = time.time()

@socketio.on("heartbeat")
def ws_heartbeat():
    if "device_id" in session and session["device_id"] in players:
        players[session["device_id"]]["last_ping"] = time.time()
        players[session["device_id"]]["online"] = True
    emit("heartbeat_ack", {"server_time": now_str()})

@app.route("/api/admin/clear_uploads", methods=["POST"])
@admin_required
def api_clear_uploads():
    """Clear all uploaded photos (use after game)"""
    import glob
    upload_dir = app.config['UPLOAD_FOLDER']
    files = glob.glob(os.path.join(upload_dir, "*"))
    count = 0
    for f in files:
        try:
            os.remove(f)
            count += 1
        except:
            pass
    log_event("uploads_cleared", {"count": count})
    return jsonify({"success": True, "files_removed": count})

@app.route("/api/admin/server_info", methods=["GET"])
@admin_required
def api_server_info():
    """Get server health information"""
    import glob
    upload_count = len(glob.glob(os.path.join(app.config['UPLOAD_FOLDER'], "*")))
    
    return jsonify({
        "uptime_seconds": time.time() - server_start_time,
        "total_events": len(events),
        "total_messages": len(messages),
        "total_players": len(players),
        "active_bounties": len(bounties),
        "upload_count": upload_count,
        "moderators": list(moderators),
        "python_version": sys.version,
    })

# Track server start time
server_start_time = time.time()

# =============================================================================
# MAIN
# =============================================================================

if __name__ == "__main__":
    # Get config values with defaults
    host = HOST if 'HOST' in dir() else "0.0.0.0"
    port = PORT if 'PORT' in dir() else 5000
    debug = DEBUG if 'DEBUG' in dir() else True
    unsafe_werkzeug = ALLOW_UNSAFE_WERKZEUG if 'ALLOW_UNSAFE_WERKZEUG' in dir() else True
    
    print("=" * 60)
    print("  🎯 IRL Hunts Game Server v4")
    print("  Complete Game Modes + Emergency System")
    print("=" * 60)
    print(f"  Admin Password: {'*' * len(ADMIN_PASSWORD)} (hidden)")
    print(f"  Server URL: http://{host}:{port}")
    print(f"  Debug Mode: {debug}")
    print(f"  Max Players: {MAX_PLAYERS}")
    print(f"  Available Modes: {', '.join(GAME_MODES.keys())}")
    print("=" * 60)
    print("  Configuration: config_local.py (if exists) or config.py")
    print("  Change settings by editing config files, not source code!")
    print("=" * 60)
    
    start_background_thread()
    socketio.run(app, host=host, port=port, debug=debug, allow_unsafe_werkzeug=unsafe_werkzeug)
