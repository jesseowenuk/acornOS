# acornOS Engineering Principles

These principles exist because we learned the hard way
that "just enough to work" causes more pain than doing
it properly the first time.

---

## 1. Do It Properly The First Time

**Never implement "just enough to work".**

Every time we've cut corners we've paid for it:
- Floppy loading → had to rewrite for LBA
- 4MB identity map → page faults above 4MB  
- 256KB heap → ran out immediately
- Hardcoded sector counts → constant revisiting
- 32-bit when we need 64-bit → full rewrite

**The rule:** Before implementing anything, ask:
"Will this need rewriting when the project grows?"
If yes — do it properly now.

---

## 2. Map All The RAM

**Always map all available physical RAM.**

Never map "just enough for now".
- Detect RAM via E820
- Map every usable page
- No artificial limits
- No "we'll extend this later"

---

## 3. Load The Entire Kernel

**The bootloader must handle kernels of any size.**

Never hardcode sector counts.
- Read kernel size from disk
- Load dynamically based on actual size
- Never need to revisit the bootloader
  because the kernel grew

Implementation:
```asm
; Store kernel sector count in a known location
; Stage 2 reads it and loads exactly that many
; sectors — no more, no less, no hardcoding
```

---

## 4. Reserve Memory Properly

**The PMM must know about ALL reserved regions
before allocating a single page.**

Never discover a conflict at runtime.
- Reserve bootloader pages
- Reserve kernel pages  
- Reserve heap pages
- Reserve bitmap pages
- Reserve VGA/BIOS pages
- Then and only then start allocating

---

## 5. Handle All Error Cases

**Every operation must handle failure.**

No silent failures.
No returning NULL and hoping.
- kmalloc failure → kpanic
- pmm_alloc failure → kpanic
- Critical init failure → kpanic
- Log everything to serial

---

## 6. Design For The Future

**Every subsystem must be designed to last
the lifetime of the project.**

Ask before implementing:
- Will this work with 64-bit?
- Will this work with multiple cores?
- Will this work with 1TB of RAM?
- Will this work with 1000 processes?

If the answer to any is "no" — redesign now.

---

## 7. Document Everything

**Every design decision must be documented.**

- Why we chose this approach
- What alternatives we considered
- What the tradeoffs are
- What will need changing for future milestones

Key documents:
- docs/memory_map.md — memory layout
- docs/roadmap.md — project roadmap
- docs/principles.md — this document
- docs/subsystems/ — per-subsystem docs

---

## 8. Test Before Moving On

**Every subsystem must be verified working
before building on top of it.**

Never assume something works.
Never move on with "it probably works".
Write a test, run it, verify the output.

---

## 9. Clean Commits

**Every commit must leave the tree in a
working, buildable state.**

No "WIP" commits.
No broken builds.
No debug prints left in.
No commented-out code without explanation.

---

## 10. No Magic Numbers

**Every constant must be named and documented.**

Never:
```c
map_pages(0x100000, 256);   // What is this??
```

Always:
```c
// Map kernel heap — see docs/memory_map.md
map_pages(HEAP_PHYS_BASE, HEAP_SIZE / PAGE_SIZE);
```

---

## The Core Rule

> If you're thinking "this will do for now"
> you're about to create future pain.
> Stop. Do it properly. Move on once.

🌱