#ifndef LUO_H
#define LUO_H

#include "image.h"

#define LUO_DEVICE "/dev/liveupdate"

extern int luo_session_init(const char *session);
extern int luo_session_open(const char *session);
extern int luo_image_open(const struct cr_img *image, int flags);
extern int luo_image_close(const struct cr_img *image);
extern void luo_session_hang();
extern int luo_session_finish(void);

#endif /* LUO_H */
