
![](https://mchpgfx.github.io/legato.docs/docs/legato/images/mgs.svg)

blank_qs_9x75_cu_ac69t88a.X

Defining the Architecture
-------------------------

![](../../../images/legato_sam9x60_single_buffer_arch.png)

This application tests the display.

User touch input on the display panel is received through the PCAP capacitive touch controller, which sends a notification to the Touch Input Driver. The Touch Input Driver reads the touch information over I2C and sends the touch event to the Input System Service. Points at the corners of the display can be used to test touch functionality.

### Demonstration Features

-   Test application for the SAM9X75-Curiosity Evaluation Board
-   Test pattern that validates display and touch functionality
-   24-bit color, multi-layer, WVGA (800x480)
-   Time system service, RTT peripheral library and driver
-   Graphics Acceleration using integrated display controller (XLCDC)
-   I2C and maXTouch® controller driver

Creating the Project Graph
--------------------------

![](../../../images/sam_9x75_cu_project_graph.png)

The Project Graph diagram shows the Harmony 3 components that are included in this application. Lines between components are drawn to satisfy components that depend on a capability that another component provides.

Adding the **SAM 9X75 Curiosity BSP** and **Legato Graphics w/ AC69T88A Display** Graphics Template component into the project graph will automatically add the components needed for a graphics project and resolve their dependencies. It will also configure the pins needed to drive the external peripherals like the display and the touch controller. In this case, the Legato graphics library is disconnected and removed to test the display driver without any library.

Building the Application
------------------------

The parent directory for this application is gfx/apps/legato\_quickstart. To build this application, use MPLAB X IDE to open the gfx/apps/blank\_quickstart/blank\_qs\_9x75\_cu\_ac69t88a.X project folder and press F11.
A successful build will generate a harmony.bin file in dist/lvds_wvga_mxt/production in the project folder.

The following table lists configuration properties:

| Project Name                    | BSP Used                                                                                        | Graphics Template Used                        | Description                                                                                                                                                                                                |
| :------------------------------ | :---------------------------------------------------------------------------------------------- | :-------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| blank\_qs\_9x75\_cu\_ac69t88a.X | [SAM9X75 Curiosity BSP](https://www.microchip.com/en-us/development-tool/EV31H43A) | Legato Graphics w/ Microchip AC69T88A Display | [SAM9X75 Curiosity](https://www.microchip.com/en-us/development-tool/EV31H43A) with LVDS Interface and [AC69T88A MPU32 LVDS Display WVGA](https://www.microchip.com/en-us/development-tool/AC69T88A) |

Configuring the Hardware
------------------------

Configure the hardware as follows:
-   Connect the [AC69T88A MPU32 LVDS Display WVGA](https://www.microchip.com/en-us/development-tool/AC69T88A) to the SAM 9X75 Curiosity Board using a Type-D (contacts on opposite sides) 30-pin 0.5mm pitch FFC ribbon.

-	Take an SD Card formatted with the FAT32 file system and copy the boot.bin binary file from this [location](./binaries/boot.bin). Also copy the harmony.bin file you generated from the "Building the Application" section.

-   Insert the SD card into the SD BOOT slot (J12) and power up the board by connecting a USB cable (5V/1A recommended) to the USB port or an adapter to the 5V DC-IN jack (5V/1A recommended, Center Positive).

Running the Demonstration
-------------------------
The LCD should display a test pattern on successful power-on. See reference image. Click the points at the corners to test touch functionality.

![](../../../images/sam_9x75_cu_blank_qs.jpg)

* * * * *
