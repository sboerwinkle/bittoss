
#define fontSizePx 16
// TODO This constant is waaaay outdated, from back when this was a 2D game (!)
#define PTS_PER_PX 32

// Person cube radius is 16, and with a 90 deg FOV the closest
// something touching our surface could be while still being onscreen
// is 16/sqrt(2) = approx 11.3
#define nearPlane (11*PTS_PER_PX)
// Pretty arbitrary; remember that increasing the ratio of far/near decreases z-buffer accuracy
#define farPlane (1500000*PTS_PER_PX)

extern int32_t frameOffset[3];

extern void initGraphics();

extern void setDisplaySize(int width, int height);

// These functions affect GL state,
// and are expected to be called in the listed order.
// (Some could be skipped, but it's moot because we don't)
// They could be made more robust, but not worth the effort since this isn't
// a library or anything, just a standalone application.
extern void setupFrame(float pitch, float yaw, float up, float forward);
extern void setupStipple();
extern void setupTags();
extern void setupText();

extern void rect(int32_t *p, int32_t *radius, float r, float g, float b);
extern void drawHudText(const char* str, double xBase, double yBase, double x, double y, double scale, float* color);
extern void drawHudRect(double x, double y, double w, double h, float *color);
