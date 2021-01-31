void printDrmModes(int fd);
void *setupFrameBuffer(int fd, int crtc_id, uint32_t conn_id, uint32_t *width, uint32_t *heigth, uint32_t *pitch);
void getDefaultFramebufferSize(int fd, uint32_t crtc_id, uint32_t *width, uint32_t* heigth);
void *createFrameBuffer(int fd, uint32_t width, uint32_t heigth, uint32_t *fb_id, uint32_t *pitch);
void showFirstFrame(int fd, uint32_t crtc_id, uint32_t conn_id, uint32_t fb_id);
