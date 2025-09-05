# Thermo Forge Unreal Plugin

[![Plugin version number](https://img.shields.io/github/v/release/cem-akkaya/ThermoForge?label=Version)](https://github.com/cem-akkaya/ThermoForge/releases/latest)
[![Unreal Engine Supported Versions](https://img.shields.io/badge/Unreal_Engine-5.6-9455CE?logo=unrealengine)](https://github.com/cem-akkaya/ThermoForge/releases)
[![License](https://img.shields.io/github/license/cem-akkaya/ThermoForge)](LICENSE)
![Download count](https://img.shields.io/github/downloads/cem-akkaya/ThermoForge/total)
[![Actively Maintained](https://img.shields.io/badge/Maintenance%20Level-Actively%20Maintained-green.svg)](https://gist.github.com/cheerfulstoic/d107229326a01ff0f333a1d3476e068d)

<img src="Resources/Splash.jpeg" alt="plugin-thermo-forge" width="100%"/>

## Overview

Thermo Forge is an Unreal Engine plugin for simulating and querying heat, climate, and thermal conditions in your levels.
It processes baked geometry fields such as sky openness, wall permeability, and indoor factors, then blends them with dynamic weather, altitude, and heat sources to produce realistic temperature values anywhere in the world.

The system is built around Thermo Forge Volumes and Heat Source Components, with a World Subsystem handling queries and composition. You can work directly with baked fields, introduce runtime sources, or retrieve composed temperatures through both Blueprints and C++.

Thermo Forge is designed for seamless gameplay integration, from AI “heat sense” behaviors to environment-driven mechanics. It is efficient, editor-friendly, and requires no engine modifications or physics assets. Instead, it models environmental interaction through the concept of porosity, the relationship between material volume and surrounding space. This is used to approximate permeability and how heat dissipates or propagates in real-world conditions.

The system is baked and lightweight, relying on precomputed fields for efficiency while applying runtime adjustments for climate, diurnal cycles, elevation, and occlusion. Dynamic heat sources remain fully interactive, enabling believable changes such as a fire cooling its surroundings when extinguished.

Thermo Forge is still under development but has been proven stable in both open-world and smaller test environments. It has been validated with multiple overlapping volumes, dense grids, and complex level layouts. While experimental in nature, it is carefully designed, tested, and refined with the aid of AI supervision to ensure clarity, code quality, and reliability.

If you have any bug or crash, please open an issue in the Github repo.
If you have suggestions, questions or need help to use you can always contact [me](https://github.com/cem-akkaya)<br>

If you want to contribute, feel free to create a pull request.

## Features

- Volume-based thermal field baking (sky view, wall permeability, indoor proxy).
- Climate simulation with diurnal cycles, seasons, altitude lapse rate, and weather scaling.
- Heat Source components (point or box) with configurable intensity, falloff, and attenuation.
- Query functions for current temperature at any world position with given UTC.
- Grid previews and heat visualization in-editor.
- Subsystem for managing sources and accessing thermal data globally.
- Blueprint and C++ integration for gameplay-driven heat logic.

## Examples

Some examples of Thermo Forge plugin in action:

  <figure style="display:inline-block; margin:10px; width:370px;">
    <img src="Resources/Demo1.gif" alt="Room with spotlight accuracy" width="370"/>
    <figcaption align="center">
     Grid previews and heat visualization in-editor with default density settings.Multiple sources, blocking geometry, and occulusion is used to simulate heat distribution.e
    </figcaption>
  </figure>
  <figure style="display:inline-block; margin:10px; width:370px;">
    <img src="Resources/Demo2.gif" alt="Activate in Plugins menu" width="370"/>
    <figcaption align="center">
      Top down view of heat distribution and in a level with multiple sources, blocking geometry, influenced from season, occulusion, and weather.
    </figcaption>
  </figure>

  <figure style="display:inline-block; margin:10px; width:370px;">
    <img src="Resources/Demo3.gif" alt="Heat distribution" width="370"/>
    <figcaption align="center">
     Multiple heat sources and multiple volumes with gradual dispersion and constant temperature options.
    </figcaption>
  </figure>
  <figure style="display:inline-block; margin:10px; width:370px;">
    <img src="Resources/Demo4.gif" alt="Editor preview tools" width="370"/>
    <figcaption align="center">
      Day and night temperatures are automatically adjusted based on the time of day and weather.
    </figcaption>
  </figure>

  <figure style="display:inline-block; margin:10px; width:370px;">
    <img src="Resources/Demo5.gif" alt="Heat distribution" width="370"/>
    <figcaption align="center">
      Seasons and weather affect the temperature of the level with the defined location and season shift settings solely on UTC.
    </figcaption>
  </figure>
  <figure style="display:inline-block; margin:10px; width:370px;">
    <img src="Resources/Demo6.gif" alt="Editor preview tools" width="370"/>
    <figcaption align="center">
      Closed areas are automatically occluded giving brief temporal dispersion making cooler inside or dissipating heat less.
    </figcaption>
  </figure>



## Installation
<img src="Resources/SS1.jpeg" alt="plugin-thermo-forge" width="830"/>

Install it like any other Unreal Engine plugin.
- Place ThermoForge under `Drive:\YOURPROJECTFOLDER\Plugins\ThermoForge`
- Activate it in your project’s Plugins menu
- Open the **Thermo Forge Tab** from `Tools > Thermo Forge`
- Use **Spawn Thermal Volume** to add a volume into the level
- Resize the volume like a normal Brush/Box actor
- Use the **preview buttons** to rebuild or visualize the baked grid
- Click **Kickstart Sampling** in the tab to generate baked data for all volumes
- Select any actor and press **Add Heat Source** to attach a ThermoForgeSource component
- Configure the source with shape, radius, intensity, and falloff in the Details panel
- In Blueprints or C++, use the **Thermo Forge Subsystem** to query temperature at any world position

## Detailed Usage
<img src="Resources/SS2.jpeg" alt="plugin-thermo-forge" width="830"/>

- **Project Settings**
    - Open **Edit > Project Settings > Thermo Forge**  
      -- Set climate defaults (winter/summer averages, day-night deltas, solar gain)  
      -- Configure altitude lapse and sea level if needed  
      -- Adjust permeability rules (air density, max solid density, absorption, trace channel)  
      -- Define default grid cell size and guard cells for volumes  
      -- Choose preview defaults (time of day, season, weather)

- **Thermo Forge Tab**
    - Found under **Tools > Thermo Forge**  
      -- **Spawn Thermal Volume**: Adds a new Thermo Forge Volume into the level  
      -- **Add Heat Source to Selection**: Adds a ThermoForgeSource component to selected actor(s)  
      -- **Kickstart Sampling**: Bakes geometry fields for all volumes in the level  
      -- **Show All Previews**: Makes all grid previews visible  
      -- **Hide All Previews**: Hides all grid previews  
      -- **Set Mesh Insulated**: Applies the Thermo Forge insulator physical material to selected meshes

- **Thermo Forge Volume**
    - Place a volume in your level using the tab or manually from the class list  
      -- Adjust **BoxExtent** to fit the area you want sampled  
      -- Toggle **bUnbounded** to let the volume cover the entire level  
      -- Choose grid settings (use global grid, custom cell size, grid origin mode, orientation)  
      -- Preview options (gap size, auto-rebuild, max instances, preview material)  
      -- Assign or inspect baked field assets (automatically generated during sampling)  
      -- Use **Rebuild Preview Grid**, **Build Heat Preview**, or **Hide Preview** buttons in Details

- **Heat Source Component**
    - Add to any actor using the Thermo Forge Tab or manually in the Details panel  
      -- Toggle **Enabled** to activate/deactivate the source  
      -- Set **Intensity (°C)** to define temperature offset at the center  
      -- Choose **Shape**: Point (sphere) or Box (oriented)  
      -- Configure size: **RadiusCm** for point, **BoxExtent** for box  
      -- Pick a **Falloff** method (None, Linear, Inverse Square) for point sources  ~~~~
      -- Toggle **AffectByOwnerScale** to scale source with the actor’s transform  
      -- Sources are registered automatically in the World Subsystem.
- **Blueprint / C++ Integration**
    - Blueprint: Drag from Thermo Forge Subsystem to call query nodes
    - C++: Include `ThermoForgeSubsystem.h` and use subsystem functions
    - Use `OnSourcesChanged` delegate to react when new heat sources are added/removed
### Thermo Forge Subsystem
<img src="Resources/SS3.jpeg" alt="plugin-thermo-forge" width="830"/>

- **Subsystem & Queries**
    - Access via **Thermo Forge Subsystem** (World Subsystem)  
      -- `ComputeCurrentTemperatureAt(WorldPosition, bWinter, TimeHours, WeatherAlpha)` to calculate exact temperature  
      -- `QueryNearestBakedGridPoint(WorldPosition, QueryTimeUTC)` to get nearest baked cell info  
      -- `QueryNearestBakedGridPointNow(WorldPosition)` for real-time queries  
      -- Subsystem also provides helper functions for occlusion, ambient rays, and data dumping


## What can I do with this plugin?

- **Environmental VFX** : Show shimmer particles above hot spots, frosty breath in cold pockets, or add post-process effects that lerp emissive/outline by (Temp − Ambient).
- **Status effects** : Trigger “Overheat” in >X °C areas (stagger, bloom, accuracy penalty) or “Comfort” near heat sources (stamina regen). Cold zones can slow reload but quiet footsteps for stealth.
- **Environmental puzzles & roguelite hooks** : Heat locks that open only above a threshold, ice bridges by chilling pipes, or time-of-day rooms flipping hot/cold for route planning choices.
- **Source/field synergies** : Use baked permeability for insulation gameplay: reduced heat damage behind thick walls, or explosive blasts venting down leaky corridors.
- **Elemental weapons** : Fire weapons spawn short-lived +°C bubbles, cryo weapons spawn −°C fields that suppress enemy thermal sense and fire-rate. Mods can alter falloff (Linear / None / InverseSquare) to reshape crowd control.
- **Faction and creature traits** : “Heat predators” track targets through smoke but struggle in uniformly hot rooms, while other AI can use thermal cues as senses you can disrupt.

## FAQ

<details>
<summary><b>From which gameplay objects can I query temperature?</b></summary>

> From any world position. You don’t need to attach a component to the object.  
> You can query directly from the Thermo Forge Subsystem or spawn a helper actor with a Heat Source component if you want to contribute new thermal data.
</details>

<details>
<summary><b>How accurate are the thermal calculations?</b></summary>

> Accuracy depends on the grid cell size of your Thermo Forge Volumes and the preview/bake settings.  
> For most gameplay purposes (AI senses, survival mechanics, ambient temperature), the results are stable and believable.  
> The system is designed for performance and consistency, not exact thermodynamics simulation.
</details>

<details>
<summary><b>Does Thermo Forge simulate full thermodynamics?</b></summary>

> No. Thermo Forge is not a scientific thermodynamics solver.  
> It is a hybrid approach: baked geometry fields plus runtime adjustments (climate, altitude, weather, and heat sources).  
> This makes it extremely fast and reliable for gameplay without heavy CPU/GPU load.
</details>

<details>
<summary><b>Can I add or remove heat sources at runtime?</b></summary>

> Yes. Heat Source Components can be attached or destroyed at runtime.  
> They automatically register with the Subsystem and immediately affect queries.
</details>

<details>
<summary><b>How efficient is it in large open-world levels?</b></summary>

> Very efficient.  
> Baking is done once per volume and stored in lightweight field assets.  
> Runtime queries are O(1) lookups into the nearest grid cell with minor adjustments.  
> It scales well with multiple volumes, even in large environments.
</details>

<details>
<summary><b>Does it require physics assets or engine modifications?</b></summary>

> No. Thermo Forge uses standard Unreal tracing and materials.  
> It does not depend on physics assets or engine modifications.  
> It can be dropped into any project as a self-contained plugin.
</details>

<details>
<summary><b>Can AI use Thermo Forge for “heat sense” behaviors?</b></summary>

> Yes. You can query the subsystem in AI controllers or behavior trees.  
> This enables AI to detect players or objects by heat signature instead of only vision.
</details>

<details>
<summary><b>How do altitude and weather affect results?</b></summary>

> Altitude reduces temperature using the environmental lapse rate.  
> Weather controls solar gain and affects how much heat from the sky reaches a location.  
> Both are configurable in Project Settings.
</details>

<details>
<summary><b>Can I preview heat values in the editor?</b></summary>

> Yes. Thermo Forge Volumes support grid previews and colored heat visualization.  
> Use the tab menu or volume details panel to rebuild, show, or hide previews.
</details>

<details>
<summary><b>Is the system multiplayer/replication ready?</b></summary>

> Thermo Forge itself runs on the server side by default.  
> Query results can be passed to clients like any other gameplay data.  
> Heat Sources are just components, so their replication depends on the owning actor.
</details>

## Planned  Upcoming Features

- AI Thermal Sense (Perception).
- Per-volume bake queues with progress bars.
- Time scrubber on the Heat Previews.
- Thermal vision mode.

## License

This plugin is under [MIT license](LICENSE).

MIT License does allow commercial use. You can use, modify, and distribute the software in a commercial product without any restrictions.

However, you must include the original copyright notice and disclaimers.

## *Support Me*

If you like my plugin, please do support me with a coffee.

<a href="https://www.buymeacoffee.com/akkayaceq" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-yellow.png" alt="Buy Me A Coffee" height="41" width="174"></a>