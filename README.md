# Blemish Remover

In this project I create a GUI application which lets a user remove blemishes from an input image by means of a mouse.

## Set Up

It is assumed that OpenCV 4.x, C++11 compiler, and cmake 2.18.12 or newer are installed on the system.

### Specify OpenCV_DIR in CMakeLists

Open CMakeLists.txt and set the correct OpenCV directory in the following line:

```
SET(OpenCV_DIR /home/hp/workfolder/OpenCV-Installation/installation/OpenCV-master/lib/cmake/opencv4)
```

Depending on the platform and the way OpenCV was installed, it may be needed to provide the path to cmake files explicitly. On my KUbuntu 20.04 after building OpenCV 4.4.0 from sources the working `OpenCV_DIR` looks like <OpenCV installation path>/lib/cmake/opencv4. On Windows 8.1 after installing a binary distribution of OpenCV 4.2.0 it is C:\OpenCV\build.


### Build the Project

In the root of the project folder create the `build` directory unless it already exists. Then from the terminal run the following:

```
cd build
cmake ..
```

This should generate the build files. When it's done, compile the code:

```
cmake --build . --config Release
```

## Usage

The program has to be run from the command line. It takes one argument which is the path where the input image should be read from: 

```
reblem <input file path> 
```

The project folder contains a sample image which can be used as an input file for the program.

Sample usage (linux):
```
./reblem ../blemish.png
```

Blemish Remover is self-explanatory. It starts from the user guide which briefly explains how to use the software. In order to start editing the image, press **Space**. 

Select a blemish by **left clicking** on it. Once the mouse button is released, the program will remove the blemish.

To undo a previous action, press **U** on your keyboard. The program maintains the history of 10 last changes.

Depending on a blemish size, you may want to increase or decrease the size of the healing brush. It can be done using a **mouse wheel**. 

In case you want to discard all changes and start afresh, press **R**. This will reset the image to its original state. This action is not reversible.

In order to quit the app, press **Escape**. 






