# Maple Bay Mayhem

An original 3D open-world game for Linux, written in C with OpenGL and X11.
Walk around an island town, drive any car, fly planes and helicopters from the
airport, fight street gangs, dodge the police, run missions for the locals,
and spend your coins on vehicles, weapons, outfits and upgrades.

## Build and run

```sh
./run.sh            # builds if needed, then starts the game
# or
make && ./maplebay
```

Options: `--windowed`, `--fullscreen`. In game, **F11** toggles fullscreen and
**F12** saves a screenshot to `screenshots/`.

Requirements: a C compiler, `make`, and the X11 and OpenGL dev headers
(`libx11-dev`, `libgl-dev` / Mesa). Sound uses ALSA (`libasound.so.2`, loaded at
runtime), so the game still runs silently if ALSA is missing.

## Default controls (all rebindable in Options > Controls & Key Bindings)

| Action | Keys |
|---|---|
| Move / accelerate / brake / steer / throttle | W A S D, or the arrow keys |
| Jump / open parachute (on foot) | Space |
| Sprint (on foot) / Nitro (driving, once bought) | Left Shift |
| Enter / exit vehicle, talk, shop | E |
| Kick | F (or Fire with bare hands) |
| Fire weapon / aircraft guns | Left mouse button or V |
| Aim / scope | Right mouse button or X |
| Reload | R |
| Next / previous weapon | Q / Z, or the mouse wheel |
| Handbrake (drift) | Space or Left Ctrl |
| Horn | H or right mouse button |
| Climb / pull up (flying) | Space |
| Descend / dive (flying) | Left Ctrl |
| Fire missile (flying) | G or right mouse button |
| Call your car | C |
| Reset (unstick) vehicle | R |
| Camera | Mouse, or J L I K |
| Map | M or Tab |
| Pause | Esc (always works) or P |

Each action has a primary and a secondary key, and each binding belongs to a
context (on foot, driving, flying). The same key can do different jobs in
different contexts, so Space jumps on foot, drifts in a car and climbs in an
aircraft. Bindings that clash within a context are shown in orange. Settings
are saved in `config.ini` next to the game, and you can also edit that file by
hand.

## What's in the game

- **Three islands**, each with its own airport:
  - **Maple Bay**: the big city, 144 blocks. The original town sits in the
    middle, surrounded by new districts: Bayside Hospital, the Bayview Mall,
    Maple Stadium, a fire station, a building site you can climb, more parks
    and gas stations.
  - **Sunset Sands**: a desert resort with the Lucky Sands Casino, the Devil's
    Canyon stunt mesas, palm trees, an oasis and the Lock & Load gun store.
  - **Frostpeak**: a snowy mountain town with a giant ski jump, a frozen lake,
    log cabins, falling snow and a Boom Bros Armory branch.
- **Island hopping**: book a flight (75 coins) at the FLIGHTS desk in any
  airport terminal, or fly your own plane or helicopter out over the open sea
  and pick a destination from the flight plan. You arrive in the air, still
  flying.
- **Map and waypoints** (M): click anywhere to drop a waypoint, then follow the
  blue GPS line on the map and minimap and the blue beacon in the world. The
  mouse wheel zooms, WASD pans, C centres on you and R resets the view. Keys
  1-6 set quick waypoints: nearest gas, nearest hospital, nearest shop, the
  airport flights desk, your mission and your car.
- **Cars**: borrow any car on the street or call your own with **C**. Ramps,
  drifting, damage and wrecks. Cars burn **fuel**, so stop at a gas station
  (F on the map) to refuel for a few coins.
- **Planes and helicopters**: the airport has a Sky Hopper, a Stunt Biplane and
  a Heli Scout that anyone can fly. The Jet Fighter and Gunship can be bought at
  Skyline Hangar. Planes need speed down the runway before they pull up.
  Helicopters climb straight up and hover. Bail out in mid-air and press Jump
  to open your parachute. Aircraft have nose guns, and missile pods can be
  fitted.
- **Weapons**: pistol, SMG, shotgun, sniper rifle (with scope), rocket
  launcher, and the new assault rifle and grenade launcher. Each one can be upgraded in four ways: power, range, optics and
  magazine. You also have health, body armor and max-health training. If you
  run out of health you wake up at Maple Medical with a small bill.
- **Gang wars**: the Rust Rats and the Neon Vipers hold turf north of the clock
  tower and shoot it out where their territory meets. Take down 8 members in a
  turf block to claim it, and claimed turf pays you every minute.
- **Police**: patrol cars drive around town and respond to gunfire. They go
  after gang members, and after you if you hurt people or damage property in
  front of them. Your wanted level (1-5 stars) brings more cars. At low levels
  officers try to arrest you ("BUSTED", with a fine). At high levels they open
  fire. Run far away, or lie low and stay out of trouble, and the stars fade.
- **16 missions**: one new mission on each new island (Desert Dash and
  Summit Scramble), plus three story lines on Maple Bay:
  - The town story: checkpoints, pizza delivery, a race and two chases.
  - The airport: a flight-school ring course and rooftop helicopter landings.
  - The armory and gangs: a shooting range, a turf war, shooting down rogue
    drones, and a final battle with the Viper gunship.
- **Shops**:
  - **Bolt & Bargain** ($): cars, outfits and perks.
  - **Boom Bros Armory** (G): weapons, ammo, weapon upgrades and gear.
  - **Skyline Hangar** (A): aircraft, aircraft upgrades (engine, power, armor,
    guns, missiles, fuel tank) and paint jobs.
  - **Turbo Tuners** (T): car engine, turbo, armor, fuel tank and paint.
  - **Lock & Load Weapon Exchange** (W), in the Bayview Mall and on Sunset
    Sands: buy every weapon (including the two exclusives), **sell** weapons
    back for half of what you paid for the gun, its upgrades and its ammo,
    and upgrade them.
- Coins, crates, 20 hidden Golden Gizmos (10 on Maple Bay, 5 on each other island), autosave after missions and
  purchases (`savegame.dat`), and manual save from the pause menu.

## Tests

```sh
make test
```

This runs 39 scripted gameplay tests by injecting real input: driving, jumping,
the tower climb, taking off and flying a plane, flying and landing a
helicopter, shooting a gang member, getting wanted and losing the police, a
police chase, police fighting a gang member, getting busted, running out of
fuel and refuelling, the shops, buying and selling at the weapon store, the
grenade launcher, waypoints and GPS, loading every island, flying between
islands by airline and by plane, key rebinding, and every mission. The tests render
offscreen, so no window opens, and they never touch your save file.

## Source layout

| File | Contents |
|---|---|
| `src/main.c` | X11 window, GLX context, input, config and save files |
| `src/audio.c` | ALSA output, synthesized sound effects and music |
| `src/gfx.c` | Drawing primitives, bitmap font, character, weapon, car and aircraft models |
| `src/world.c` | Island generation (city, desert, snow), collision grid, GPS routing, static scenery |
| `src/sim.c` | Player, car physics, traffic and police driving, pedestrians, fuel, camera, scene rendering |
| `src/air.c` | Plane and helicopter flight model, enemy gunship |
| `src/combat.c` | Weapons, bullets and rockets, gangs and turf, wanted level, health |
| `src/shop.c` | Shop catalogues and purchases |
| `src/mission.c` | Story missions |
| `src/ui.c` | Menus, HUD, shop screen, options, key rebinding, map, test harness |
