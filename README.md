# Sandman
A Linux kernel module used to suspend and resume your storage devices when you want.

## Usage

Just compile and load the kernel module.

Then, you can communicate with the module from the user-space by sending IOCTL commands defined in `sandman.h`.

You can also compile and launch the basic program written in the `main.c` source file.
