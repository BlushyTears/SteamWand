# SteamWand
- A modular, flexible, performant engine based on axiomatic principles

- Everything is modularized down to the bit. A character might have some set of fields which points to a huge region where the data is stored. If the user wants, he may opt to cache that data for the price of extra storage, with reduced time complexity. You can attatch or detatch data in any way, shape or form in a data-driven inspired approach. You might have 100 zombies that are alike, and then you might decide you want a boss zombie which can take all the normal zombie fields, eject some unused fields and add some new specialized ones all at your control. Additionally, it should be possible (if the user allows) to: times combine and concatenate modules.

- Pay for what you use (Plug n play): All systems are decoupled so that if you only want to use some parts, you only use those.

- Realtime-scripting (lua). Ability to change game-related data without having to recompile or even build the engine (You should only need to compile if you import a new library/module).

- Built-in optimization: If a user decides that the pc isn't overworked: Processes such as defragmentation can be requested to occur.

- Job-based architecture: Everything is a job, but the syntax changes should be minimal (One current consideration is if jobs should be something the user decides to opt for or not).

