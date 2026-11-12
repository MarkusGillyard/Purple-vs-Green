# Engineering decisions

- Target Linux x86-64 and Vulkan on FLOWBOX (Ryzen 7 5700G, RTX 2060 6 GiB). The reference is 1920 × 1080 at 60 FPS, with frame-time measurement from the standalone build.
- Use the existing Epic UE 5.8.4 installer (CL 58902052). Keep project caches, intermediate builds and staging on the verified Cinqic Storage device.
- Use local C++ gameplay authority and one soldier class. Spell data and damage are shared by human and AI. Ground-route links are collision-checked when the arena starts. Bots use sight traces, short-lived last sightings and blocked-route recovery.
- Both sides use the same skeletal battlemage, animated in place from movement, damage and casting state. The first-person rig has separate hands and runs off the same state. Locomotion uses procedural two-bone foot placement against world geometry. Visual casts use the existing release time, and movement and hit reactions animate the viewmodel independently of the aim camera. Spell particles use bounded instance pools and a separate visual random stream. Presentation never writes to combat collision or health.
- Ebony Hearts uses separate Core CI and standalone run evidence tied to a revision and package hash. PvG will likewise distinguish engine-independent checks from engine and package checks. Slopperfall’s Linux launcher and asset notices inform handoff. The other Cinqic projects reinforce test isolation, permission boundaries and artifact provenance. All PvG gameplay, art and tools are new work.
- The repository is private. The project carries no public license.
