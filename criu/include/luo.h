#ifndef LUO_H
#define LUO_H

extern int luo_session_init(char *session);
extern int luo_session_open(char *image, int flags);
extern int luo_session_fini(void);

#endif /* LUO_H */
