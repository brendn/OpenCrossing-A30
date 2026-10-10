## OpenCrossing-A30

This is a fork of [OpenCrossing-Anbernic](https://github.com/TechProGabe/OpenCrossing-Anbernic), a port of Animal Crossing for Anbernic devices. This originally did not work on the Miyoo A30 due to the device only supporting GLES 2.0, so I had Claude whip up a GLES 2.0 renderer for it, as well as throw some extra optimizations on top.  As a result, the game runs at full resolution at (mostly) 60fps on this cheap little device! By default, I have it set to 75% render scale to save some more battery, but you can bump it up in the settings (`L2`). Shadows and particles have been slightly reduced to improve performance, but only to a degree where it's hard to really notice any visual difference. 

The primary goal of this was to see if it was even possible, but seeing how it's fully playable I felt like sharing it with the world since I'm sure there's a handful of AC fans out there that have this device collecting dust on a shelf somewhere. 

## Installation

Download `AnimalCrossing-ac-gc-a30` from Releases, extract to the root of your SD card and ensure you have your Animal Crossing ROM (**Only the USA disc is supported** - `GAFE01`, Rev 0) in `Roms/A30PORTS/ac-gc/rom`.  Once you're in, `L2` will open the settings menu, and pressing menu twice quits the game.

## Credits

This project would not be possible without these awesome people:

- **[ACreTeam / ac-decomp](https://github.com/ACreTeam/ac-decomp)** — the complete C decompilation of Animal Crossing that makes any of this possible.
- **[flyngmt/ACGC-PC-Port](https://github.com/flyngmt/ACGC-PC-Port)** — the original PC port of the decompilation (GX→OpenGL translation layer).
- **[Dia2809's fork](https://github.com/Dia2809/ACGC-PC-Port)** — Linux support, the OpenGL ES 3 renderer, and the ARM branches OpenCrossing-Anbernic was bootstrapped from.
- **[TechProGabe's fork](https://github.com/TechProGabe/OpenCrossing-Anbernic)** - The fork this port originated from

Animal Crossing is © Nintendo. This project is not affiliated with or endorsed by Nintendo, and distributes none of its assets.

AI tools (Claude) were used in the porting and optimization work (platform code only, **not the decompiled game code**!).