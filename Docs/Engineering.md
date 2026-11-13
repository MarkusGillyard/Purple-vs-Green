# Engineering decisions

- Target Linux x86-64 and Vulkan on FLOWBOX (Ryzen 7 5700G, RTX 2060 6 GiB). The reference is 1920 × 1080 at 60 FPS, with frame-time measurement from the standalone build.
- Use the existing Epic UE 5.8.4 installer (CL 58902052). Keep project caches, intermediate builds and staging on the verified Cinqic Storage device.
- Use local C++ gameplay authority and one soldier class. Spell data and damage are shared by human and AI. Ground-route links are collision-checked when the arena starts. Bots use sight traces, short-lived last sightings and blocked-route recovery.
- Both teams share the same armor, skeleton and casting logic. A time-based cast envelope blends a small KayKit hand trajectory into the articulated viewmodel and third-person arm pose. The procedural feet use world geometry as the grounding reference. The camera and spell aim remain independent of the casting pose.
- Accepted casts sample the hand for release visuals. Damage and projectile collision use the original eye trace or swept projectile. The comet's visible offset settles into its collision path after launch. Impacts use the trace or sweep result. Charge motes, ward facets, and the Niagara burst have bounded lifetimes and visual randomness independent of combat.
- Ebony Hearts uses separate Core CI and standalone run evidence tied to a revision and package hash. PvG will likewise distinguish engine-independent checks from engine and package checks. Slopperfall’s Linux launcher and asset notices inform handoff. The other Cinqic projects reinforce test isolation, permission boundaries and artifact provenance. All PvG gameplay, art and tools are new work.
- The GitHub repository is public. Project-owned material remains all rights reserved.
