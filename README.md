# UCFS Circuits Repository

Welcome to the University of California Fusion Studies (UCFS) KiCad circuits repository! This is the central location for all our electronic circuit designs, schematics, and PCB layouts.

## About

This repository contains KiCad project files for various electronic circuits used in our fusion research and experiments. All designs are created using KiCad, an open-source electronic design automation (EDA) suite.

## Current Projects

### Teensy 4.1 Breakout Board
A custom breakout board design for the Teensy 4.1 microcontroller development board.

**Location:** `Teensy 4.1 Breakout Board/`
**Files:**
- `Breakout.kicad_pro` - Project configuration
- `Breakout.kicad_sch` - Schematic design
- `Breakout.kicad_pcb` - PCB layout

## Requirements

To work with these circuit designs, you'll need:

- **KiCad EDA Suite** (version 6.0 or later recommended)
  - Download from: https://www.kicad.org/download/
  - Free and open-source
  - Available for Windows, macOS, and Linux

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
5. **Test your designs** before submitting
6. **Document your work:** Add project details to this README when adding new circuits

### File Management

- KiCad backup files and temporary files are automatically ignored via `.gitignore`
- Only commit the main project files (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`)
- Avoid committing generated files like Gerber files unless specifically needed

## Support

For questions about these circuits or KiCad usage:

- Check the [KiCad documentation](https://docs.kicad.org/)
- Visit the [KiCad community forum](https://forum.kicad.info/)
- Contact the UCFS engineering team

## License

Circuit designs in this repository are created for research and educational purposes at UCFS. Please contact the repository maintainers for usage permissions and licensing information.