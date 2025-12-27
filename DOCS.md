# Documentation Index

Complete guide to understanding FizixLabWebGPU codebase. Start with your use case below.

---

## 🎯 Choose Your Path

### "I'm New to This Project"
**Start here:** Read in this order
1. [README.md](./README.md) - Project overview (5 min)
2. [CODEBASE_OVERVIEW.md](./CODEBASE_OVERVIEW.md) - High-level architecture (20 min)
3. Look at `src/app/main.cpp` - See the entry point (5 min)
4. Read about a system you care about in CODEBASE_OVERVIEW (20 min)

**Then:** Try modifying a value and rebuilding to see the effect

---

### "I Need to Add a Feature"
**Best reference:**
1. [CONTRIBUTING.md](./CONTRIBUTING.md) - Development workflow (10 min)
2. Find the relevant "Task" section (e.g., "Task 1: Add New Shape")
3. Use [QUICK_REFERENCE.md](./QUICK_REFERENCE.md) to find file locations
4. Review [ARCHITECTURE.md](./ARCHITECTURE.md) for design patterns

---

### "I'm Debugging Something"
**Quick lookup:**
1. [QUICK_REFERENCE.md](./QUICK_REFERENCE.md) - "Debugging Checklist" section
2. Check "Common Issues & Fixes" table
3. Find relevant system in [CODEBASE_OVERVIEW.md](./CODEBASE_OVERVIEW.md)
4. Add debug output following patterns in [CONTRIBUTING.md](./CONTRIBUTING.md)

---

### "I Want to Understand the Design"
**Read in order:**
1. [CODEBASE_OVERVIEW.md](./CODEBASE_OVERVIEW.md) - "High-Level Architecture" section
2. [ARCHITECTURE.md](./ARCHITECTURE.md) - Why decisions were made
3. [QUICK_REFERENCE.md](./QUICK_REFERENCE.md) - "Data Structures" section
4. Look at actual code with diagrams in mind

---

### "I'm Optimizing Performance"
**Check:**
1. [CONTRIBUTING.md](./CONTRIBUTING.md) - "Performance Tips" section
2. [ARCHITECTURE.md](./ARCHITECTURE.md) - ADR-002 through ADR-007 for trade-offs
3. Profile the code to find bottlenecks
4. Review relevant ADRs before making changes

---

### "I'm Reviewing Someone's PR"
**Use:**
1. [CONTRIBUTING.md](./CONTRIBUTING.md) - "Code Review Checklist"
2. [QUICK_REFERENCE.md](./QUICK_REFERENCE.md) - Code patterns and style
3. [ARCHITECTURE.md](./ARCHITECTURE.md) - Check decisions align with ADRs

---

## 📚 Document Overview

| Document | Length | Purpose | Best For |
|----------|--------|---------|----------|
| **README.md** | 2 min | Project summary & quick start | First-time visitors |
| **CODEBASE_OVERVIEW.md** | 30 min | Complete architecture guide | Understanding systems |
| **QUICK_REFERENCE.md** | 20 min | Fast lookup of files & patterns | Daily development |
| **CONTRIBUTING.md** | 40 min | Development workflow & guidelines | Making changes |
| **ARCHITECTURE.md** | 20 min | Design decisions & rationale | Understanding "why" |
| **DOCS/INDEX.md** | (this file) | Navigation guide | Finding what you need |

---

## 🗂️ Organization

```
FizixLabWebGPU/
├── README.md                 ← START HERE
├── CODEBASE_OVERVIEW.md      ← Learn architecture
├── QUICK_REFERENCE.md        ← Find things fast
├── CONTRIBUTING.md           ← Make changes
├── ARCHITECTURE.md           ← Understand decisions
└── DOCS/
    └── INDEX.md              ← You are here
```

---

## 🔍 System Overview

### Quick System Diagram

```
┌──────────────────────────────────────────┐
│         Application (main.cpp)           │
└──────────────────┬───────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
   ┌────▼──────┐      ┌──────▼────┐
   │   Engine  │      │  (Main    │
   │           │      │   Loop)   │
   └────┬──────┘      └──────┬────┘
        │                    │
   ┌────┴────┬──────────┬────┴────┐
   │          │          │         │
┌──▼──┐   ┌──▼──┐   ┌──▼──┐   ┌──▼────────┐
│World│   │Rend-│   │Utils│   │Event      │
│     │   │erer │   │     │   │Handling   │
└──┬──┘   └──┬──┘   └─────┘   └───────────┘
   │         │
   └─────────┴─────────────────────┐
                                    │
                    ┌───────────────▼──────────────┐
                    │  Graphics/Physics Pipeline   │
                    │                              │
                    │  Shapes → Physics → Render   │
                    │  Ball, Box                   │
                    │  Rigidbody                   │
                    │  Collision Detection        │
                    │  WebGPU Device               │
                    └──────────────────────────────┘
```

---

## 🎓 Learning Path

### Level 1: Basics (1-2 hours)
- Read: README.md, CODEBASE_OVERVIEW.md (High-Level section)
- Do: Build and run the project
- Understand: The 3 main systems (Engine, World, Renderer)

### Level 2: Components (2-4 hours)
- Read: CODEBASE_OVERVIEW.md (Core Systems section)
- Do: Trace execution of `Engine::RunFrame()`
- Understand: How each system works internally

### Level 3: Implementation (4-8 hours)
- Read: CONTRIBUTING.md (Task 1: Add New Shape)
- Do: Add a Triangle shape
- Understand: How to extend the engine

### Level 4: Deep Dive (8+ hours)
- Read: ARCHITECTURE.md (all ADRs)
- Do: Implement a feature from scratch
- Understand: Design trade-offs and rationale

---

## 🚀 Common Tasks

### I want to...

**...change physics behavior**
→ Read CODEBASE_OVERVIEW.md section "5. Physics System"
→ Check ARCHITECTURE.md ADR-007 (time integration)
→ Modify `src/physics/` files

**...add a new shape**
→ Read CONTRIBUTING.md section "Task 1: Add a New Shape Type"
→ Use QUICK_REFERENCE.md to find related code
→ Follow the step-by-step example

**...improve rendering**
→ Read CODEBASE_OVERVIEW.md section "3. Renderer System"
→ Check ARCHITECTURE.md ADR-002 through ADR-005
→ Modify `src/shaders/` or `src/core/Renderer.cpp`

**...understand a system**
→ Use CODEBASE_OVERVIEW.md "Core Systems" section
→ Cross-reference with QUICK_REFERENCE.md
→ Read the actual code in `src/`

**...debug an issue**
→ Use QUICK_REFERENCE.md "Debugging Checklist"
→ Check "Common Issues & Fixes" table
→ Add debug output (pattern in CONTRIBUTING.md)

---

## 💡 Key Concepts to Understand

Before you go too deep, make sure you understand:

### 1. **Data Flow (Per Frame)**
- See: CODEBASE_OVERVIEW.md "Data Flow" section
- Understanding: World updates physics, then tells Renderer what to draw

### 2. **Local vs World Coordinates**
- See: CODEBASE_OVERVIEW.md "Important Concepts" section 2
- Understanding: Shapes store local vertices, transformed by position

### 3. **GPU Memory Layout**
- See: CODEBASE_OVERVIEW.md "Important Concepts" section 3
- Understanding: Uniforms batched in single buffer with dynamic offsets

### 4. **Vertex Representation**
- See: CODEBASE_OVERVIEW.md "Important Concepts" section 1
- Understanding: Vertices are flat float arrays [x0,y0, x1,y1, ...]

### 5. **Physics Integration**
- See: CODEBASE_OVERVIEW.md "Important Concepts" section 5
- Understanding: Explicit Euler with multiple iterations for stability

---

## 🔗 Cross-References

### By Topic

**Physics**
- CODEBASE_OVERVIEW.md: "5. Physics System"
- ARCHITECTURE.md: ADR-006, ADR-007, ADR-008
- CONTRIBUTING.md: "Task 2: Modify Physics Behavior"
- Code: `src/physics/`, `src/collision/`

**Graphics**
- CODEBASE_OVERVIEW.md: "3. Renderer System"
- ARCHITECTURE.md: ADR-002, ADR-003, ADR-004, ADR-005
- CONTRIBUTING.md: "Task 3: Modify Rendering"
- Code: `src/core/Renderer.cpp`, `src/shaders/`

**Shapes**
- CODEBASE_OVERVIEW.md: "4. Shape System"
- ARCHITECTURE.md: ADR-009
- CONTRIBUTING.md: "Task 1: Add New Shape Type"
- Code: `src/shape/`

**Design**
- CODEBASE_OVERVIEW.md: "Architecture Lessons" section
- ARCHITECTURE.md: All ADRs
- CONTRIBUTING.md: "Design Patterns Used"

---

## ❓ FAQ

**Q: Where should I start?**
A: Read README.md, then CODEBASE_OVERVIEW.md "High-Level Architecture" section.

**Q: How do I add a new shape?**
A: Follow the step-by-step example in CONTRIBUTING.md "Task 1".

**Q: Why is it designed this way?**
A: Check ARCHITECTURE.md for the ADR (Architecture Decision Record).

**Q: Where do I find [component]?**
A: Use QUICK_REFERENCE.md "Find It Fast" section.

**Q: What's the best way to debug?**
A: See QUICK_REFERENCE.md "Debugging Checklist".

**Q: How can I improve performance?**
A: See CONTRIBUTING.md "Performance Tips" section.

**Q: Should I refactor this code?**
A: Check ARCHITECTURE.md to see if there's an ADR about it first.

**Q: Is there a style guide?**
A: Yes, see CONTRIBUTING.md "Code Style Guidelines" section.

---

## 📞 Getting Help

1. **Code location?** → QUICK_REFERENCE.md
2. **Architecture question?** → ARCHITECTURE.md
3. **How to add feature?** → CONTRIBUTING.md
4. **System overview?** → CODEBASE_OVERVIEW.md
5. **Getting started?** → README.md

---

## 🏁 Suggested Reading Order

### For New Contributors
1. README.md (5 min)
2. CODEBASE_OVERVIEW.md - "High-Level Architecture" (10 min)
3. src/app/main.cpp (5 min)
4. CODEBASE_OVERVIEW.md - "Core Systems" relevant section (15 min)
5. Start with simple task from CONTRIBUTING.md

### For Code Review
1. CONTRIBUTING.md - "Code Review Checklist"
2. QUICK_REFERENCE.md - "Code patterns"
3. ARCHITECTURE.md - relevant ADRs
4. Then review the code

### For Architecture Questions
1. CODEBASE_OVERVIEW.md - relevant section
2. ARCHITECTURE.md - relevant ADRs
3. Source code with diagrams in mind

---

## 📊 Document Statistics

- **Total documentation**: ~8,000 words
- **Code examples**: 50+
- **Diagrams**: 10+
- **Tasks covered**: 3 major development tasks
- **Design decisions recorded**: 10 ADRs

---

## 🔄 Keeping Documentation Updated

When you make changes:

1. **Update the relevant document** if the architecture changes
2. **Create a new ADR** if making a significant decision
3. **Update QUICK_REFERENCE.md** if adding new systems
4. **Add code examples** to CONTRIBUTING.md if introducing new patterns

---

**Last Updated**: December 27, 2025
**Maintained by**: FizixLabWebGPU Contributors

