/*
 * ECE/CS 285 – HW1 Intermediate Scene: Explorer
 * A cartoon robot on a small moon under a ringed gas giant: quadrics,
 * textures and displacement together.
 *
 * Output: renders/2_IntermediateScenes/Explorer.png
 */
#include "Ri.h"
#include "sceneutil.h"

#include <cmath>

/* RGB() builds an RtColor argument in place (RiColor and RiOpacity copy it
 * at once) */
static RtFloat *RGB(double r, double g, double b) { static RtColor c; c[0] = (RtFloat)r; c[1] = (RtFloat)g; c[2] = (RtFloat)b; return c; }

/* The sky's stars (texture 0, 2048 x 1024), looked up by the direction from
 * the eye (camera space: x right, y up, +z ahead) instead of by the
 * surface's u, v: about 1.5 pixels to a texel at 960 x 720 with fov 45, so
 * the stars come out as sharp points, and continuous across the pieces of
 * the sky. Unlit. */
static void SkyStars(RtShaderContext *c)
{
    const double TEXELS_PER_RADIAN = 580;
    double x = c->P[0], y = c->P[1], z = c->P[2];
    double azimuth = std::atan2(x, z), elevation = std::atan2(y, std::sqrt(x * x + z * z));
    RiTexture(0, (RtFloat)(0.5 + azimuth * TEXELS_PER_RADIAN / 2048),
              (RtFloat)(0.5 - elevation * TEXELS_PER_RADIAN / 1024), c->Ci);
    c->Oi[0] = c->Os[0];
    c->Oi[1] = c->Os[1];
    c->Oi[2] = c->Os[2];
}

void Explorer(void)
{
    float fov = 45.0f;
    RiBegin(RI_NULL);
    RiFormat(960, 720);
    RiPixelSamples(4, 4);
    RiDisplay("2_IntermediateScenes/Explorer.png", "", "");
    RiFrameBegin(0);
        RiProjection(RI_PERSPECTIVE, "fov", &fov);
        RiMakeTexture("stars.png", 0);
        RiMakeTexture("gasgiant.png", 1);
        RiMakeTexture("moon.png", 2);
        RiWorldBegin();
            /* Sky: a curved backdrop behind everything, made of six arcs of a
             * cylinder 600 m in radius, its axis upright 750 m ahead, so its
             * near face is 150 m away. Nothing surrounds the eye: a sky
             * sphere around it crosses the eye plane, so it gets the maximum
             * dice rate however big it is, and its micropolygons came out
             * six pixels wide and smeared the stars. Each arc (7 degrees of
             * the cylinder, half its height) is diced on its own, into
             * micropolygons under half a pixel. Together they cover the
             * frame, which spans 29 degrees each side and 22.5 up and down. */
            RiTransformBegin();
                RiSurface(SkyStars, NULL, 0);
                RiOpacity(RGB(1, 1, 1));
                RiColor(RGB(1, 1, 1));
                RiTranslate(0, 0, 750);
                RiRotate(-90, 1, 0, 0); /* the cylinder's axis up; its theta = 90 faces the eye */
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 2; ++j) {
                        RiTransformBegin();
                            RiRotate((RtFloat)(79.5 + 7 * i), 0, 0, 1);
                            RiCylinder(600, (RtFloat)(-80 + 80 * j), (RtFloat)(80 * j), 7);
                        RiTransformEnd();
                    }
            RiTransformEnd();

            /* ground: a small moon, so the horizon curves away */
            RiTransformBegin();
                RiSurface(NULL, NULL, 0);
                RiColor(RGB(0.55, 0.5, 0.44));
                BumpParams bp1 = { 0.12f, 2, 6 };  RiDisplacement(BUMPY, &bp1, sizeof bp1);
                RiTranslate(0, -43, 15);
                RiRotate(-90, 1, 0, 0);
                RiSphere(40, -40, 40, 360);
                RiDisplacement(NULL, NULL, 0);
            RiTransformEnd();

            /* gas giant with a flattened torus as its ring */
            RiTransformBegin();
                RiTranslate(13.5, 8.2, 46);
                RiRotate(-65, 1, 0, 0);
                RiRotate(20, 0, 1, 0);
                TextureParams planet = {1};
                RiSurface(TexturedStandard, &planet, sizeof planet);
                RiColor(RGB(1, 1, 1));
                RiSphere(4.2, -4.2, 4.2, 360);
                RiSurface(NULL, NULL, 0);
                RiColor(RGB(0.85, 0.78, 0.62));
                RiOpacity(RGB(0.85, 0.85, 0.85));
                RiScale(1, 1, 0.05);
                RiTorus(6.6, 1.5, 0, 360, 360);
                RiOpacity(RGB(1, 1, 1));
            RiTransformEnd();

            /* a second, cratered moon */
            RiTransformBegin();
                RiTranslate(-10, 6.4, 36);
                RiRotate(-90, 1, 0, 0);
                RiRotate(140, 0, 0, 1);
                TextureParams moon = {2};
                RiSurface(TexturedStandard, &moon, sizeof moon);
                RiColor(RGB(1, 1, 1));
                RiSphere(1.3, -1.3, 1.3, 360);
            RiTransformEnd();

            /* rocks */
            RiSurface(NULL, NULL, 0);
            RiColor(RGB(0.42, 0.38, 0.34));
            BumpParams bp2 = { 0.22f, 1, 4 };  RiDisplacement(BUMPY, &bp2, sizeof bp2);
            RiTransformBegin();
                RiTranslate(-3.6, -2.505, 14.5);
                RiScale(1.2, 0.8, 1);
                RiSphere(0.9, -0.9, 0.9, 360);
            RiTransformEnd();
            RiTransformBegin();
                RiTranslate(3.4, -2.5875, 16);
                RiScale(1.2, 0.8, 1);
                RiSphere(0.75, -0.75, 0.75, 360);
            RiTransformEnd();
            RiTransformBegin();
                RiTranslate(-1.9, -2.7525, 9.5);
                RiScale(1.2, 0.8, 1);
                RiSphere(0.45, -0.45, 0.45, 360);
            RiTransformEnd();
            RiTransformBegin();
                RiTranslate(4.6, -2.725, 13);
                RiScale(1.2, 0.8, 1);
                RiSphere(0.5, -0.5, 0.5, 360);
            RiTransformEnd();
            RiDisplacement(NULL, NULL, 0);

            /* robot: body */
            RiSurface(NULL, NULL, 0);
            RiOpacity(RGB(1, 1, 1));
            RiTransformBegin();
                RiTranslate(0, -3, 12);
                RiColor(RGB(0.4, 0.42, 0.46));
                RiTransformBegin();
                    RiTranslate(-0.45, 0, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiCylinder(0.26, 0, 1.9, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(-0.45, 0.02, -0.12);
                    RiScale(1, 0.55, 1.25);
                    RiRotate(-90, 1, 0, 0);
                    RiSphere(0.46, 0, 0.46, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0.45, 0, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiCylinder(0.26, 0, 1.9, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0.45, 0.02, -0.12);
                    RiScale(1, 0.55, 1.25);
                    RiRotate(-90, 1, 0, 0);
                    RiSphere(0.46, 0, 0.46, 360);
                RiTransformEnd();
                RiColor(RGB(0.74, 0.8, 0.9));
                RiTransformBegin();
                    RiTranslate(0, 1.8, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiCylinder(1, 0, 2.2, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0, 1.8, 0);
                    RiScale(1, 0.35, 1);
                    RiRotate(-90, 1, 0, 0);
                    RiSphere(1, -1, 0, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0, 4, 0);
                    RiScale(1, 0.35, 1);
                    RiRotate(-90, 1, 0, 0);
                    RiSphere(1, 0, 1, 360);
                RiTransformEnd();
                RiColor(RGB(0.95, 0.7, 0.15));
                RiTransformBegin();
                    RiTranslate(0, 2.4, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiTorus(1, 0.11, 0, 360, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0, 3.2, -0.98);
                    RiScale(0.55, 0.4, 0.12);
                    CheckerParams cp3 = { 12, 6 };  RiSurface(CHECKERBOARD, &cp3, sizeof cp3);
                    RiColor(RGB(0.3, 0.95, 0.45));
                    RiSphere(1, -1, 1, 360);
                    RiSurface(NULL, NULL, 0);
                RiTransformEnd();
                /* arms */
                RiTransformBegin();
                    RiTranslate(-1.05, 3.75, 0);
                    RiColor(RGB(0.95, 0.7, 0.15));
                    RiSphere(0.3, -0.3, 0.3, 360);
                    RiRotate(-35, 0, 0, 1);
                    RiRotate(90, 1, 0, 0);
                    RiColor(RGB(0.4, 0.42, 0.46));
                    RiCylinder(0.2, 0, 1.55, 360);
                    RiTranslate(0, 0, 1.7);
                    RiColor(RGB(0.74, 0.8, 0.9));
                    RiSphere(0.32, -0.32, 0.32, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(1.05, 3.75, 0);
                    RiColor(RGB(0.95, 0.7, 0.15));
                    RiSphere(0.3, -0.3, 0.3, 360);
                    RiRotate(150, 0, 0, 1);
                    RiRotate(90, 1, 0, 0);
                    RiColor(RGB(0.4, 0.42, 0.46));
                    RiCylinder(0.2, 0, 1.55, 360);
                    RiTranslate(0, 0, 1.7);
                    RiColor(RGB(0.74, 0.8, 0.9));
                    RiSphere(0.32, -0.32, 0.32, 360);
                RiTransformEnd();
                /* head */
                RiColor(RGB(0.4, 0.42, 0.46));
                RiTransformBegin();
                    RiTranslate(0, 4.3, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiCylinder(0.3, 0, 0.45, 360);
                RiTransformEnd();
                RiColor(RGB(0.74, 0.8, 0.9));
                RiTransformBegin();
                    RiTranslate(0, 5.5, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiSphere(0.85, -0.85, 0.85, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(-0.33, 5.6, -0.7);
                    RiColor(RGB(0.98, 0.98, 1));
                    RiSphere(0.21, -0.21, 0.21, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(-0.33, 5.6, -0.86);
                    RiColor(RGB(0.05, 0.05, 0.08));
                    RiSphere(0.09, -0.09, 0.09, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0.33, 5.6, -0.7);
                    RiColor(RGB(0.98, 0.98, 1));
                    RiSphere(0.21, -0.21, 0.21, 360);
                RiTransformEnd();
                RiTransformBegin();
                    RiTranslate(0.33, 5.6, -0.86);
                    RiColor(RGB(0.05, 0.05, 0.08));
                    RiSphere(0.09, -0.09, 0.09, 360);
                RiTransformEnd();
                RiColor(RGB(0.4, 0.42, 0.46));
                RiTransformBegin();
                    RiTranslate(0, 6.3, 0);
                    RiRotate(-90, 1, 0, 0);
                    RiCylinder(0.05, 0, 0.75, 360);
                RiTransformEnd();
                RiColor(RGB(0.95, 0.15, 0.12));
                RiTransformBegin();
                    RiTranslate(0, 7.15, 0);
                    RiSphere(0.16, -0.16, 0.16, 360);
                RiTransformEnd();
            RiTransformEnd();
        RiWorldEnd();
    RiFrameEnd();
    RiEnd();
}
