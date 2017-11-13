
How to run linux-demo
1. update Makefile, set CUSTOMER to linux
2. make linux-demo
3. run the demo,
   out/linux/modules/linux-demo/linux-demo -h
   out/linux/modules/linux-demo/linux-demo -p profile -r record_file

How to compile for android platform
  1. install android ndk first
  2. make sure ndk-build work, then run below command in the root dir of this project
    ndk-build NDK_PROJECT_PATH=.  APP_PLATFORM=android-14 APP_BUILD_SCRIPT=./Android.mk TARGET_OUT='out/android/$(TARGET_ARCH_ABI)' $@
jni : used for andriod
