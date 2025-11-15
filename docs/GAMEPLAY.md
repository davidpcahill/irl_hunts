# 🎮 IRL Hunts - Gameplay Guide

Master the hunt with this comprehensive guide to rules, strategies, and scoring.

---

## 🌟 Overview

IRL Hunts is a real-world game of cat and mouse. **Predators** hunt **Prey** using proximity-based tracking, while Prey use strategy, safe zones, and teamwork to survive and escape.

---

## 🦊 Playing as Predator

### Your Mission
Track down and capture prey by getting physically close enough to trigger a capture.

### How Captures Work
1. Your tracker detects nearby prey via LoRa signal
2. OLED shows signal strength (RSSI) and player ID
3. Get close enough (RSSI meets threshold)
4. Press button to attempt capture
5. Server validates and confirms capture

### RSSI Signal Guide
```
-30 to -50: VERY CLOSE (1-2m) ✅ Easy capture
-50 to -60: CLOSE (2-5m) ✅ Good capture range
-60 to -70: MEDIUM (5-10m) ⚠️ Might work
-70 to -80: FAR (10-20m) ❌ Too far
-80 to -90: VERY FAR (20m+) ❌ Out of range
```

**Note**: Obstacles (walls, trees) weaken signals significantly.

### Scoring
| Action | Points |
|--------|--------|
| Each capture | +100 |
| 3+ captures bonus | +50 |
| 5+ captures bonus | +100 |
| Photo sighting | +25 |
| Bounty capture | Variable |

### Predator Strategies

**Lone Wolf**
- Hunt solo for maximum captures
- Move quickly between areas
- Surprise prey in hiding spots

**Pack Hunter**
- Coordinate with other predators
- Flush prey toward teammates
- Cover more ground

**Stalker**
- Follow prey from distance
- Wait for them to leave safe zones
- Strike when vulnerable

**Trap Setter**
- Position between prey and safe zones
- Predict prey movements
- Cut off escape routes

### Photo Sightings
- Take photos of prey for +25 points
- Upload via web dashboard
- Proves you found them
- Useful for near-misses

### Tips for Predators
- Watch for prey leaving safe zones
- Communicate prey locations with team
- Don't tunnel vision on one target
- Check all hiding spots
- Use terrain to your advantage
- Photos are backup points if capture fails

---

## 🐰 Playing as Prey

### Your Mission
Survive the game without being captured, and escape when you are caught.

### How to Avoid Capture
- Monitor tracker for predator warnings
- Stay mobile - don't camp
- Use safe zones strategically
- Work with other prey
- Watch RSSI signals for approaching preds

### Safe Zones
Physical locations with beacon devices:
- Auto-detected by your tracker
- Can't be captured while inside
- Captured prey auto-escape on entry
- Limited time recommended (avoid camping)

**Safe Zone Notifications:**
- "Entered safe zone: [Name]" ✅
- "Left safe zone - you can be captured!" ⚠️
- "YOU ESCAPED!" (if captured) 🎉

### Scoring
| Action | Points |
|--------|--------|
| Each escape | +75 |
| Photo sighting | +25 |
| Survival bonus (never caught) | +200 |

### Prey Strategies

**Runner**
- Constant movement
- Use speed to stay ahead
- Never stay in one place

**Hider**
- Find obscure hiding spots
- Minimize LoRa transmission distance
- Stay quiet and hidden

**Safe Zone Hopper**
- Move between safe zones
- Never stray far from safety
- Quick dashes between zones

**Counter-Intel**
- Photograph predators approaching
- Warn other prey via chat
- Share predator locations

**Decoy**
- Distract predators
- Lead them away from other prey
- Sacrifice for team points

### Escaping After Capture
1. Status shows "CAPTURED"
2. Head to nearest safe zone beacon
3. Enter beacon range
4. Auto-escape triggers
5. You're free again!

### Tips for Prey
- Know all safe zone locations before game
- Don't panic when captured - escape is possible
- Team chat for pred locations
- Photos of preds help your team
- Mix up your patterns
- Walls/buildings block LoRa signals
- Stay unpredictable

---

## 🏠 Safe Zone Mechanics

### Detection
- Beacon broadcasts LoRa signal
- Your tracker detects signal strength
- Exceeds RSSI threshold = you're in
- Automatic - no button press needed

### Rules
✅ **Allowed:**
- Enter/exit freely
- Change roles (if enabled)
- Chat with team
- Take photos

❌ **Not Allowed:**
- Being captured
- Camping indefinitely (frowned upon)
- Blocking safe zone entrance

### Strategic Use
- Quick refuge when predator close
- Escape after capture
- Role change location
- Rally point for prey team

---

## 📸 Photo Sightings

### How It Works
1. Open dashboard on phone
2. Click 📷 floating button
3. Select target player from list
4. Take/upload photo
5. Submit for points

### Rules
- Predators photograph prey only
- Prey photograph predators only
- Photo should show the target
- Game must be running
- +25 points per sighting

### Why Bother?
- Backup points if capture fails
- Proof you found someone
- Teamwide intel (gallery visible)
- Fun memory of the game

---

## 💬 Team Communication

### Chat Types
- **All**: Everyone sees
- **Team**: Only your role sees

### Strategic Uses
- Share predator locations (prey)
- Coordinate captures (pred)
- Warn of approaching threats
- Plan group strategies
- Celebrate captures/escapes

### Sample Calls
**Prey:**
```
"Pred near Building A, heading east"
"Safe zone Oak Tree clear, come here"
"I'm captured, going for escape at fountain"
```

**Predators:**
```
"Two prey near parking lot"
"I'll flank left, you go right"
"Target escaping toward safe zone"
```

---

## 🏆 Scoring System

### Predator Total
```
Base captures × 100
+ Capture bonus (3+) × 50
+ Capture bonus (5+) × 100
+ Photo sightings × 25
+ Bounty bonuses
= Total Points
```

### Prey Total
```
Escapes × 75
+ Photo sightings × 25
+ Survival bonus (if uncaught) × 200
+ Bounty bonuses
= Total Points
```

### Leaderboards
- Separate for Preds and Prey
- Real-time updates
- Shows profile pics
- Displays online status
- Safe zone indicator

---

## 🎯 Special Mechanics

### Bounties
- Admin/mods set bounties on players
- Extra points for that target
- Creates focus on specific player
- Notification to target ("You have a bounty!")

### Emergency Button
- Press if real emergency
- Alerts all moderators
- For actual safety concerns
- Not for game purposes

### Moderator Powers
- Start/pause/end game
- Release captured players
- Kick problematic players
- Set bounties
- Send announcements

---

## 🎲 Game Variations

### Classic Hunt (Recommended)
- 1-3 predators
- 5-10 prey
- 30 minutes
- Most points wins

### Quick Match
- 10-15 minutes
- Fewer players
- Fast-paced action

### Endurance
- 60+ minutes
- Tests stamina
- Strategic planning

### Team Competition
- Teams of preds compete
- Most captures for team wins
- Coordination crucial

### Infection Mode
- Start with 1-2 preds
- Captured prey become preds
- Last prey standing wins big

### Photo Safari
- Double sighting points (50)
- Photos required for captures
- Document everything

---

## ⚠️ Safety Rules

### Physical Safety
- Watch where you're going
- Don't run into traffic
- Be aware of obstacles
- Stay hydrated
- Respect boundaries

### Game Boundaries
- Define play area before game
- No going into unsafe areas
- Out of bounds = DQ
- Respect property

### Social Safety
- Share game plans with non-players
- Have emergency contacts
- Use emergency button if needed
- Buddy system recommended

### Fair Play
- No interfering with equipment
- No blocking safe zones
- No physical contact
- Respect other players
- Report cheating to mods

---

## 💡 Pro Tips

### Tracker Management
- Check battery before game
- Understand your display
- Know button functions
- Watch for notifications

### Web Dashboard
- Keep open on phone
- Use chat actively
- Upload photos when safe
- Check leaderboard for motivation

### Environmental Awareness
- Use terrain (hills, buildings)
- Know signal blocking areas
- Weather considerations
- Lighting for photo sightings

### Mental Game
- Stay calm when captured
- Don't give up - escape is possible
- Adapt strategies mid-game
- Have fun regardless of score

---

## 🎮 Sample Game Timeline

**Pre-Game (10 min)**
- Players arrive, get trackers
- Log into web dashboard
- Set nicknames, upload photos
- Choose roles
- Safety briefing

**Countdown (10 sec)**
- Admin starts game
- Players take positions
- Tension builds

**Early Game (0-10 min)**
- Prey scatter and hide
- Preds begin searching
- First captures likely
- Establishing patterns

**Mid Game (10-20 min)**
- Prey learning pred patterns
- Preds communicating intel
- Photo sightings increasing
- Escapes happening

**Late Game (20-30 min)**
- Intensity peaks
- Strategic plays
- Leaderboard races
- Final captures/escapes

**End Game**
- Timer expires
- Final scores calculated
- Winners announced
- Stories shared

---

## 🏅 Victory Conditions

### Individual Glory
- Highest points in your role
- Most captures (pred)
- Most escapes (prey)
- Never caught (prey)

### Team Pride
- Total team points
- Coordinated strategies
- Helping teammates

### Achievement Hunting
- First capture
- First escape
- Most sightings
- Longest survival

---

## 🎉 Making It Fun

### For Organizers
- Clear rules explanation
- Appropriate difficulty
- Balance teams
- Monitor fun factor
- Adjust mid-game if needed

### For Players
- Embrace the role
- Communicate with team
- Celebrate successes
- Learn from captures
- Good sportsmanship

### After Game
- Share best moments
- Review photo gallery
- Discuss strategies
- Plan next game
- Thank the organizers

---

**Remember: The goal is FUN! Points are great, but the memories and experiences are what matter. Play fair, stay safe, and enjoy the hunt!**

🎯🦊🐰 **Happy Hunting!** 🐰🦊🎯
