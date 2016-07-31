#ifndef SANDMAN_H_
# define SANDMAN_H_

#define MAJOR_NUM 0
#define DEVICE_NAME "sandman_dev"
#define CLASS_NAME "sandman"
#define NDEVICES 1

#define IOCTL_SANDMAN_SUSPEND _IOW(MAJOR_NUM, 0, char *)
#define IOCTL_SANDMAN_RESUME _IOW(MAJOR_NUM, 1, char *)

#endif /* !SANDMAN_H_ */
