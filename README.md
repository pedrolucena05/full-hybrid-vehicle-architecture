# Full Hybrid Architecture (Stellantis especialization at UFPE (Final Project))

This is a virtual simulation of a **full hybrid vehicle**, developed using the **C POSIX library** on Linux. It incorporates embedded system-level programming concepts such as **queues**, **processes**, and **threads**.

## Behavior

- The vehicle **starts using the electric engine**.
- When the vehicle speed **surpasses 45 km/h**, the system begins to **split power** between the **electric** and **combustion** engines.
- Both engines work **together whenever possible**.

## Some System Requirements

- If **battery charge drops below 10%**, the system transitions to **combustion-only mode**.
- If running in combustion-only mode and the **battery charge reaches 100%**, the system transitions back to:
  - **Electric-only mode**, if speed is below 45 km/h.
  - **Hybrid mode**, if speed is above 45 km/h.
- Each **processing cycle** should be approximately **100ms**.

## Manipulate variables

- Its needed change the values before execute the project if you want to change it faster to test other scenarios;

## How to Run the Project (with makefile)
- Its needed that you have installed gcov 2.3 and gcc 14, (it's better to use dockerfile if you don't have these installed)

1. Open a terminal in the project's **main folder** (where the `makefile` is located) and type:
   ```bash
   make
2. Navigate to the bin folder and open three terminals. In each one, start the following executables:

   - Terminal 1:
         ./vmu
        
   - Terminal 2:
         ./ev

   - Terminal 3:
         ./iec

3. Execute the files at this order: ./vmu, /ev, ./iec;

4. Use the **VMU interface** to manipulate vehicle speed by following its on-screen commands.

## Other Makefile Commands

    make test
    Generates test executables.

    make coverage
    Generates code coverage reports using lcov and gcov.

## How to Run the Project (with dockerfile)

1. First install docker, then open a terminal in the project's **main folder** (where the `dockerfile` is located) and type:
   ```bash
   sudo docker build -t vmu_controller . // create a docker imagem with all requirements

2. After succed the first step execute the command below
   ```bash
   sudo docker run --rm -v "$(pwd)":/app -w /app vmu_controller // generate binary folder with executables and coverage test folder

3. Folow steps 2, 3 and 4 from makefile run

## System running video
https://drive.google.com/file/d/1fepKYzD_nttRtr1aOOZvuOsO1eUPuR9o/view?usp=sharing


