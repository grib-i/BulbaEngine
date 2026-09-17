<div align="center">

<img src="git-assets/logo.png" width="220" alt="BulbaEngine Logo">

<h1>BulbaEngine</h1>

<p>
<b>Lightweight and experimental game engine written in C</b>
</p>

<p>
Low-level. Modular. Experimental.
</p>

<br>

<a href="https://github.com/grib-i/BulbaEngine">
<img src="https://img.shields.io/github/stars/grib-i/BulbaEngine?style=for-the-badge&logo=github&label=Stars">
</a>
<a href="https://github.com/grib-i/BulbaEngine/releases">
<img src="https://img.shields.io/github/v/release/grib-i/BulbaEngine?style=for-the-badge&label=Release">
</a>
<a href="https://github.com/grib-i/BulbaEngine">
<img src="https://img.shields.io/github/commit-activity/y/grib-i/BulbaEngine?style=for-the-badge&label=Activity">
</a>
<a href="LICENSE">
<img src="https://img.shields.io/github/license/grib-i/BulbaEngine?style=for-the-badge&label=License">
</a>

<br><br>

<img src="https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c">
<img src="https://img.shields.io/badge/CMake-064F8C?style=for-the-badge&logo=cmake">
<img src="https://img.shields.io/badge/Linux-000000?style=for-the-badge&logo=linux">
<img src="https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows">

<br><br>

<a href="#documentation">
<img src="https://img.shields.io/badge/📚%20Documentation-6C63FF?style=for-the-badge">
</a>
<a href="#getting-started">
<img src="https://img.shields.io/badge/🚀%20Getting%20Started-2EA043?style=for-the-badge">
</a>
<a href="#demo">
<img src="https://img.shields.io/badge/🎮%20Play%20Demo-FF8C00?style=for-the-badge">
</a>
<a href="#roadmap">
<img src="https://img.shields.io/badge/🛣️%20Roadmap-9B59B6?style=for-the-badge">
</a>

</div>

---

## ✦ About

**BulbaEngine** is a lightweight game and graphics engine written in **C**.

The project focuses on:

- low-level control
- modular architecture
- simple and understandable APIs
- rendering experiments
- small graphical applications
- learning how game engines work internally

> **Keep the engine small. Keep the API understandable. Keep control in the developer's hands.**

---

## 📌 Project Status

| Property         | Value           |
| ---------------- | --------------- |
| **Version**      | `0.54.13`       |
| **Status**       | 🧪 Experimental |
| **Language**     | C               |
| **Build System** | CMake           |
| **Platforms**    | Linux · Windows |
| **Architecture** | Modular         |

---

## 📸 Showcase

> More screenshots and demos will be added here.

<div align="center">

<img src="git-assets/showcase/scene.png" width="400" alt="BulbaEngine Scene">
<img src="git-assets/showcase/renderer.png" width="400" alt="BulbaEngine Renderer">

</div>

---

## 🧩 Features

- 🖥️ Rendering
- 🎨 Materials and lighting
- 🧊 3D objects
- 📐 Math utilities
- 🧱 Modular engine architecture
- ⚙️ CMake-based build system
- 🧪 Experimental systems
- 🔧 Low-level API

---

## 🚀 Getting Started

### Requirements

- C compiler
- CMake
- Git

### Clone

```bash
git clone https://github.com/grib-i/BulbaEngine.git
cd BulbaEngine
```

### Build

```bash
cmake -B build
cmake --build build
```

### Run

```bash
./build/BulbaEngine
```

---

## 🌿 Development

BulbaEngine uses a simple branch structure:

```text
main
└── Stable releases

develop
└── Active development

feature/*
└── Individual features and experiments
```

Create a feature branch:

```bash
git switch develop
git switch -c feature/physics
```

Push it:

```bash
git push -u origin feature/physics
```

Merge it back into `develop`:

```bash
git switch develop
git merge feature/physics
git push
```

Stable releases are merged into `main`.

---

## 📚 Documentation

<div align="center">

<a href="docs/">
<img src="https://img.shields.io/badge/📖%20Documentation-6C63FF?style=for-the-badge">
</a>

<a href="https://github.com/grib-i/BulbaEngine/wiki">
<img src="https://img.shields.io/badge/🌐%20Wiki-24292F?style=for-the-badge&logo=github">
</a>

</div>

---

## 🎮 Demo

<div align="center">

<!-- Demo GIF will be added here -->

<img src="git-assets/showcase/demo.gif" width="820" alt="BulbaEngine Demo">

<br><br>

<a href="https://grib-i.github.io/BulbaEngine-demo/">
<img src="https://img.shields.io/badge/▶%20Play%20Demo-FF8C00?style=for-the-badge">
</a>

</div>

---

## 🛣️ Roadmap

| System          | Status        |
| --------------- | ------------- |
| **Core**        | 🟢 Active     |
| **Math**        | 🟢 Active     |
| **Renderer**    | 🟡 Developing |
| **Physics**     | 🟡 Planned    |
| **Audio**       | 🟡 Planned    |
| **Scripting**   | 🟡 Developing |
| **WebAssembly** | 🟡 Planned    |

---

## 🤝 Contributing

Contributions, experiments and ideas are welcome.

See the project documentation for development guidelines.

---

## ⭐ Support

<div align="center">

<a href="https://github.com/grib-i/BulbaEngine">
<img src="https://img.shields.io/badge/⭐%20Star%20the%20repository-181717?style=for-the-badge&logo=github">
</a>

</div>

---

## 📄 License

BulbaEngine is licensed under the **MIT License**.

<a href="LICENSE">
<img src="https://img.shields.io/badge/View%20License-MIT-green?style=for-the-badge">
</a>

Copyright © 2026 **grib_i**

---

<div align="center">

### BulbaEngine

<sub>Built with C • Experiment • Break things • Build again</sub>

</div>
