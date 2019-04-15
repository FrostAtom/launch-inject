# launch-inject

The purpose of program - reduce the number of additional clicks for injection dll in the program to 0

Advantages:

Inject multiple files.

Inject libraries in the folder by folder name.

Run the program with passing cmdline to it.


Usage (with cmdline):

launch.exe appname libname1 libname2 libname3 cmdline cmdline

launch.exe argname foldername appname libname

launch.exe libname appname foldername


Usage (with renaming injector): (cmdline fully pass to app)

appname_libname1_libname2_libname3_cmdline_cmdline.exe

argname_foldername_appname_libname.exe

libname_appname_foldername.exe
