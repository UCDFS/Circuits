# UCDFS Circuits Repository

Welcome to the University College Dublin Formula Student (UCDFS) KiCad circuits repository! This is the central location for all our electronic circuit designs, schematics, and PCB layouts.

## About

This repository contains KiCad project files for various electronic circuits used in our Formula Student vehicle and projects. All designs are created using KiCad, an open-source electronic design automation (EDA) suite.

## Getting Started

1. **Clone this repository:**
   ```bash
   git clone https://github.com/UCDFS/Circuits.git
   cd Circuits
   ```

2. **Open a project in KiCad:**
   - Launch KiCad
   - Open the `.kicad_pro` file for the project you want to work on
   - The schematic (`.kicad_sch`) and PCB layout (`.kicad_pcb`) will be accessible from the project manager

3. **View/Edit Schematics:**
   - Click "Schematic Editor" in KiCad's main window
   - Or directly open the `.kicad_sch` file

4. **View/Edit PCB Layout:**
   - Click "PCB Editor" in KiCad's main window
   - Or directly open the `.kicad_pcb` file

## Repository Structure

```
├── README.md                     # This file
├── .gitignore                    # Git ignore rules for KiCad projects
└── [Project Name]/               # Individual circuit projects
    ├── [Project].kicad_pro       # KiCad project file
    ├── [Project].kicad_sch       # Schematic file
    └── [Project].kicad_pcb       # PCB layout file
```

## Contributing

When contributing new circuits or modifications:

1. **Create a new branch** for your work
2. **Organize projects** in clearly named directories
3. **Follow naming conventions:** Use descriptive names for projects and files
4. **Include all necessary files:** Ensure `.kicad_pro`, `.kicad_sch`, and `.kicad_pcb` files are included
5. **Document your work:** Add project details to a README in each circuit subfolder
