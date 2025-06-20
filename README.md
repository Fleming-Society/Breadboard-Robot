# Breadboard Robot

A simple, modular robot built on a breadboard platform. This project contains all the design files, CAD models, and source code needed to assemble and program your own breadboard robot.

## Repository Structure
```
├── CAD/ # 3D models and design files
├── code/ # Source code for microcontroller and scripts
└── README.md # This file
```




## 📁 CAD

All CAD files, including 3D printable parts, STEP/IGES files, and assembly drawings, are stored here.

- `CAD/`
  - `parts/` – Individual component models (e.g., sensor mounts, chassis pieces).
  - `assemblies/` – Full robot assembly drawings.
  - `exports/` – Exported STEP/IGES/STL files for sharing and fabrication.

## 📁 code

Contains firmware and scripts needed to run your robot.

- `code/`
  - `firmware/` – Microcontroller source code (Arduino, ESP32, etc.).
    - `src/` – `.ino` or `.cpp` and `.h` files.
    - `lib/` – Any external libraries or modules.
  - `scripts/` – Helper scripts (Python, shell) for uploading code, data logging, or calibration.
  - `README.md` – Code-specific instructions and dependencies.


## 📝 License

This project is licensed under the MIT License. See LICENSE for details.