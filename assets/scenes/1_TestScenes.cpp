/*
 * ECE/CS 285 – HW1 Test Scenes
 * Every small test scene, in the order the Suggested Workflow needs them.
 * Each one's comment says what a correct image looks like, and
 * reference_renders/1_TestScenes/ has them all as the reference renderer
 * draws them. Objects sit in camera space: the eye at the origin looking
 * down +z, +y up, angles in degrees.
 *
 *    1. FirstPrimitive      one flat-colored quadric, your first picture
 *    2. OrthoVsPersp        the projection
 *    3. SimpleAnimation     a first animation (one frame, or a movie)
 *    4. AliasingTest        jittered samples
 *    5. DiceRateTest        dicing
 *    6. QuadricZoo          all four quadrics, partial
 *    7. Tunnel              the transform stack
 *    8. ShaderTest          the shaders
 *    9. UVTest              texture lookups
 *   10. Earth               a texture on a sphere
 *   11. DisplacementTest    displacement
 *   12. BumpVsDisplacement  bump mapping against displacement
 *   13. Tornado             heavy displacement
 *   14. TransparencyTest    opacity
 *   15. TransparencyOrder   opacity, in the right order
 *   16. PolygonTest         RiPointsPolygons
 *   17. ShadowTest          depth output, for shadow maps
 *
 * Output: renders/1_TestScenes/
 */

#include "Ri.h"
#include "sceneutil.h"   /* BumpVsDisplacement, ShadowTest */
#include "shadowmap.h"   /* BumpVsDisplacement, ShadowTest */

#include <cmath>
#include <stdio.h>       /* frame and file names */
#include <string.h>      /* FirstPrimitive's choice */
#include <filesystem>    /* SimpleAnimation's folder */

/* ------------------------------------------------------------------------
 * Test Scene 1: FirstPrimitive
 * One whole quadric, flat colored by RiColor through CONSTANT, for your
 * first picture (Workflow steps 4 to 7). Pick it by name: reyes
 * FirstPrimitive_sphere (start here: it needs only RiTranslate),
 * _cylinder, _cone or _torus. Correct image: one solid shape on black,
 * jagged at one sample per pixel: a red disk, a green upright cylinder, a
 * blue cone with its tip up, or a yellow ring tilted to show its hole. An
 * oval disk means the screen window ignores the aspect ratio; a cone tip
 * down means raster y runs up.
 *
 * Output: renders/1_TestScenes/FirstPrimitive_sphere.png (or _cylinder,
 *         _cone, _torus)
 * ---------------------------------------------------------------------- */

/* primitive is "sphere", "cylinder", "cone" or "torus". */
void FirstPrimitive(const char *primitive)
{
    float fov = 40.0f; /* fov will be 40 degrees */
    char name[64];
    snprintf(name, sizeof name, "1_TestScenes/FirstPrimitive_%s.png", primitive);

    RiBegin(RI_NULL);
        RiFormat(320, 240);  /* set the image resolution */
        RiDisplay(name, "", ""); /* set output filename/type */
        RiPixelSamples(1, 1); /* 1 x 1 samples/pixel */
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiSurface(CONSTANT, NULL, 0); /* flat, no shading at all */
                RiTranslate(0, 0, 5); /* 5 units in front of the eye */
                if (strcmp(primitive, "sphere") == 0) {
                    RtColor red = {0.9f, 0.2f, 0.2f};
                    RiColor(red);
                    RiSphere(1, -1, 1, 360);
                } else if (strcmp(primitive, "cylinder") == 0) {
                    RtColor green = {0.2f, 0.8f, 0.3f};
                    RiColor(green);
                    RiRotate(-90, 1, 0, 0); /* its axis, z, now points up */
                    RiCylinder(0.6f, -1, 1, 360);
                } else if (strcmp(primitive, "cone") == 0) {
                    RtColor blue = {0.25f, 0.45f, 1.0f};
                    RiColor(blue);
                    RiTranslate(0, -0.8f, 0);
                    RiRotate(-90, 1, 0, 0); /* base at the bottom, tip up */
                    RiCone(1.6f, 0.8f, 360);
                } else {
                    RtColor yellow = {1.0f, 0.8f, 0.2f};
                    RiColor(yellow);
                    RiRotate(-50, 1, 0, 0); /* tilted, so the hole shows */
                    RiTorus(0.9f, 0.3f, 0, 360, 360);
                }
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 2: OrthoVsPersp
 * The same three objects under both projections. Correct image: in
 * perspective the far (blue) sphere is much smaller than the near (red) one
 * and mostly hidden by the cylinder; in orthographic both spheres are the
 * same size and the cylinder runs off the top and bottom.
 *
 * Output: renders/1_TestScenes/Persp.png and Ortho.png
 * ---------------------------------------------------------------------- */
void OrthoVsPersp(int perspective)
{
    float fov = 40.0f;
    RtColor red = {0.9f, 0.15f, 0.15f}, green = {0.15f, 0.8f, 0.2f};
    RtColor blue = {0.2f, 0.3f, 0.95f}, opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay(perspective ? "1_TestScenes/Persp.png" : "1_TestScenes/Ortho.png", "", "");
        RiPixelSamples(2, 2);
        RiFrameBegin(0);
            if (perspective) RiProjection(RI_PERSPECTIVE, "fov", &fov);
            else             RiProjection(RI_ORTHOGRAPHIC);
            RiWorldBegin();
                RiSurface(NULL, NULL, 0); RiOpacity(opaque);
                RiTransformBegin(); RiColor(red);   RiTranslate(-0.8, 0.0,  6.0); RiSphere(0.4, -0.4, 0.4, 360); RiTransformEnd();
                RiTransformBegin(); RiColor(blue);  RiTranslate( 0.8, 0.0, 16.0); RiSphere(0.4, -0.4, 0.4, 360); RiTransformEnd();
                RiTransformBegin(); RiColor(green); RiTranslate( 0.0, 0.0, 10.0); RiRotate(-90, 1, 0, 0); RiCylinder(0.5, -2.0, 2.0, 360); RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 3: SimpleAnimation
 * A first animation: an orange ball moving over colored axes in front of
 * a white background, 100 frames at 320 x 240. SimpleAnimation(frame)
 * renders one frame, or all 100 when frame is negative: `reyes
 * SimpleAnimation` renders frame 75, and `reyes SimpleAnimationMovie`
 * renders all of them. Make the movie with
 *   ffmpeg -framerate 24 -i renders/1_TestScenes/SimpleAnimation/SimpleAnimation_%02d.png
 *          -c:v libx264 -pix_fmt yuv420p -crf 18
 *          renders/1_TestScenes/SimpleAnimation.mp4
 *
 * Output: renders/1_TestScenes/SimpleAnimation/SimpleAnimation_00.png
 *         to SimpleAnimation_99.png
 * ---------------------------------------------------------------------- */

/* SimpleAnimation: 100 frames, renders/1_TestScenes/SimpleAnimation/
 * SimpleAnimation_00.png to SimpleAnimation_99.png (the scene makes the
 * folder); only the given frame, unless it is negative. */
void SimpleAnimation(int frame) {
	int i;
	int nf;
	float slopex,slopey,slopez;
	char name[64]; /* the frame's path under renders/ */

	RtColor red={1,0,0};
	RtColor green={0,1,0};
	RtColor blue={0,0,1};
	RtColor white={1,1,1};
	RtColor orange={1.0f,0.55f,0.1f};


	RtPoint p1={30,0,10}; /* ball's initial position */
	RtPoint p2={0,20,10}; /* ball's final position  */


	RtFloat fov = 45;


	nf = 100; /* number of frames to output */
	slopex = (p2[0]-p1[0])/nf;
	slopey = (p2[1]-p1[1])/nf;
	slopez = (p2[2]-p1[2])/nf;
	
	std::error_code ignored;
	std::filesystem::create_directories("renders/1_TestScenes/SimpleAnimation", ignored);

	RiBegin(RI_NULL);
		RiFormat(320, 240);
		RiPixelSamples(2,2);

		/* loop through all the frames */
		for (i = 1; i <= nf; i++) {
			if (frame >= 0 && i - 1 != frame)
				continue;
			RiFrameBegin(i);
				snprintf(name, sizeof name, "1_TestScenes/SimpleAnimation/SimpleAnimation_%02d.png", i-1);
				RiDisplay(name, "file", "rgb");

				RiProjection("perspective", "fov", &fov);
				RiTranslate(0,-5,60);
				RiRotate(-120,1,0,0);
				RiRotate(25,0,0,1);

				RiWorldBegin();
					/* the background: a white disk far behind everything,
					 * facing the camera, unlit (CONSTANT). Undoing the camera
					 * transform (its calls inverted, in reverse order) makes
					 * what follows camera space, where the disk is easy to
					 * place: on the view axis, 200 units away, and big
					 * enough that even its elliptical outline covers the
					 * frame's corners. It is a sphere flattened, not
					 * collapsed: a zero scale would leave the matrix with no
					 * inverse, and so the normals with none either. */
					RiTransformBegin();
						RiRotate(-25,0,0,1);
						RiRotate(120,1,0,0);
						RiTranslate(0,5,-60);
						RiTranslate(0,0,200);
						RiScale(170,130,1);
						RiSurface(CONSTANT, NULL, 0);
						RiColor(white);
						RiSphere(1, -1, 1, 360);
					RiTransformEnd();
					RiSurface(NULL, NULL, 0); /* STANDARD again, for the rest */

					RiColor(blue);
					RiTransformBegin();
						RiCylinder(1, 0, 20, 360);
						RiTranslate(0, 0, 20);
						RiCone(2, 2, 360);
					RiTransformEnd();

					RiColor(green);
					RiTransformBegin();
						RiRotate(-90, 1, 0, 0);
						RiCylinder(1, 0, 20, 360);
						RiTranslate(0, 0, 20);
						RiCone(2, 2, 360);
					RiTransformEnd();

					RiColor(red);
					RiTransformBegin();
						RiRotate(90, 0, 1, 0);
						RiCylinder(1, 0, 20, 360);
						RiTranslate(0, 0, 20);
						RiCone(2, 2, 360);
					RiTransformEnd();


					RiColor(orange);
					RiTransformBegin();
						RiTranslate(p1[0] + slopex * (i-1), p1[1] + slopey * (i-1), p1[2] + slopez * (i-1));
						RiSphere(5, -5, 5, 360);
					RiTransformEnd();
				RiWorldEnd();

			/* RiWorldEnd above rendered this frame and wrote its image;
			 * RiFrameEnd only closes the frame */
			RiFrameEnd();
		}
	RiEnd();
};

/* ------------------------------------------------------------------------
 * Test Scene 4: AliasingTest
 * A fine checkerboard on a sphere and a thin torus nearly edge-on, at 1 x 1
 * and 4 x 4 samples. Correct image: at 1 x 1 the checks break into noise
 * near the sphere's edge; at 4 x 4 they fade to smooth grey. A tidy,
 * regular pattern at 1 x 1 means the samples aren't jittered independently
 * in every pixel.
 *
 * Output: renders/1_TestScenes/Aliasing_1x1.png and Aliasing_4x4.png
 * ---------------------------------------------------------------------- */
void AliasingTest(int samples)
{
    float fov = 40.0f;
    RtColor white = {1, 1, 1}, yellow = {0.95f, 0.85f, 0.2f}, opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay(samples == 1 ? "1_TestScenes/Aliasing_1x1.png" : "1_TestScenes/Aliasing_4x4.png", "", "");
        RiPixelSamples(samples, samples);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiOpacity(opaque);

                RiTransformBegin();
                    RiColor(white);
                    CheckerParams cp = { 256, 128 }; /* about 4 px per check at the center, under a pixel near the limb */
                    RiSurface(CHECKERBOARD, &cp, sizeof cp);
                    RiTranslate(-1.6, 0.6, 9.0);
                    RiRotate(-90, 1, 0, 0);
                    RiSphere(1.8, -1.8, 1.8, 360);
                RiTransformEnd();

                RiTransformBegin();
                    RiColor(yellow);
                    RiSurface(NULL, NULL, 0);
                    RiTranslate(2.0, -0.2, 9.0);
                    RiRotate(-84, 1, 0, 0);          /* nearly edge-on */
                    RiTorus(1.8, 0.05, 0, 360, 360);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 5: DiceRateTest
 * A sphere that nearly fills the frame, one about three pixels across, and
 * a torus between. Correct image: no holes or cracks in the big sphere, and
 * the tiny one is still a dot. Holes mean the micropolygons are too big; a
 * missing dot means a grid can go below 2 x 2.
 *
 * Output: renders/1_TestScenes/DiceRateTest.png
 * ---------------------------------------------------------------------- */
void DiceRateTest(void)
{
    float fov = 40.0f;
    RtColor red = {0.9f, 0.15f, 0.15f}, blue = {0.2f, 0.3f, 0.95f};
    RtColor yellow = {0.95f, 0.85f, 0.2f}, opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay("1_TestScenes/DiceRateTest.png", "", "");
        RiPixelSamples(4, 4);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiSurface(NULL, NULL, 0); RiOpacity(opaque);

                RiTransformBegin();                  /* huge on screen */
                    RiColor(blue);
                    RiTranslate(-0.6, 0.0, 4.0);
                    RiSphere(1.4, -1.4, 1.4, 360);
                RiTransformEnd();

                RiTransformBegin();                  /* a few pixels across */
                    RiColor(red);
                    RiTranslate(10.0, 5.0, 34.0);   /* upper right, clear of the big sphere */
                    RiSphere(0.05, -0.05, 0.05, 360);
                RiTransformEnd();

                RiTransformBegin();
                    RiColor(yellow);
                    RiTranslate(1.4, -0.3, 10.0);
                    RiRotate(-60, 1, 0, 0);
                    RiTorus(1.0, 0.3, 0, 360, 360);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 6: QuadricZoo
 * All four quadrics, each partial and semi-transparent. Correct image: four 
 * open shells with their insides lit. Black insides mean N isn't turned 
 * toward the eye; missing cut-outs mean thetamax is read in radians.
 *
 * Output: renders/1_TestScenes/QuadricZoo.png
 * ---------------------------------------------------------------------- */
void QuadricZoo(void)
{
    float fov = 40.0f;
    RtColor red = {0.9f, 0.15f, 0.15f}, green = {0.15f, 0.8f, 0.2f};
    RtColor blue = {0.2f, 0.3f, 0.95f}, yellow = {0.95f, 0.85f, 0.2f};
    RtColor half = {0.5f, 0.5f, 0.5f};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay("1_TestScenes/QuadricZoo.png", "", "");
        RiPixelSamples(4, 4);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiSurface(NULL, NULL, 0);
                RiOpacity(half);

                /* sphere: top cap removed, 90-degree wedge missing */
                RiTransformBegin();
                    RiColor(red);
                    RiTranslate(-3.0, 1.0, 9.0);
                    RiRotate(-90, 1, 0, 0);          /* quadric z axis points up */
                    RiRotate(30, 0, 0, 1);
                    RiSphere(1.2, -1.2, 0.7, 270);
                RiTransformEnd();

                /* cylinder: open tube, 300 degrees around */
                RiTransformBegin();
                    RiColor(green);
                    RiTranslate(-1.0, 1.0, 9.0);
                    RiRotate(-90, 1, 0, 0);
                    RiRotate(45, 0, 0, 1);
                    RiCylinder(0.9, -1.2, 1.2, 300);
                RiTransformEnd();

                /* cone: three quarters of a cone, apex up */
                RiTransformBegin();
                    RiColor(blue);
                    RiTranslate(1.2, -0.2, 9.0);
                    RiRotate(-90, 1, 0, 0);
                    RiRotate(-60, 0, 0, 1);
                    RiCone(2.4, 1.1, 270);
                RiTransformEnd();

                /* torus: 270 degrees, the tube cut open with phimin/phimax */
                RiTransformBegin();
                    RiColor(yellow);
                    RiTranslate(3.4, 1.0, 9.0);
                    RiRotate(-70, 1, 0, 0);
                    RiTorus(1.1, 0.45, 0, 270, 240);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 7: Tunnel
 * A checkered tunnel with a sphere, a cone, a cylinder and a torus inside,
 * each placed by its own transforms. Compare with
 * reference_renders/1_TestScenes/Tunnel.png.
 *
 * Output: renders/1_TestScenes/Tunnel.png
 * ---------------------------------------------------------------------- */

void Tunnel()
{
	RiBegin(RI_NULL);
		//RiFormat(960, 720);
		RiFormat(400, 300);
		RiDisplay("1_TestScenes/Tunnel.png", "", "");
		RiPixelSamples(2,2);
		
		RiFrameBegin(0);
			/* set the perspective transformation */
			float fov = 45.0;
			RiProjection(RI_PERSPECTIVE, "fov", &fov);
			//RiProjection(RI_ORTHOGRAPHIC);

			RiWorldBegin();
				RiTransformBegin();
					RtColor color = {1,0,0};
					RiColor(color);
					RiTranslate(0, 0.5, 7.0);
					RiRotate(60, 1, 0, 0);
					RiTorus(1, .25, 0, 360, 360);
				RiTransformEnd();
				RiTransformBegin();
					color[0] = 0; color[1] = 1;
					RiColor(color);
					RiTranslate(0, 0, 8.0);
					RiRotate(60, 1, 0, 0);
					RiRotate(30, 0, 1, 0);
					RiCylinder(1, -1, 1, 360);
				RiTransformEnd();
				RiTransformBegin();
					color[1] = 0; color[2] = 1;
					RiColor(color);
					RiTranslate(0, 1, 9.0);
					RiRotate(60, 1, 0, 0);
					RiSphere(1.0, -1.0, 1.0, 360);
				RiTransformEnd();
				RiTransformBegin();
					color[0] = 1; color[1] = .4; color[2] = .4;
					RiColor(color);
					CheckerParams cp = { 40, 40 };
					RiSurface(CHECKERBOARD, &cp, sizeof cp);
					RiTranslate(0, -1, 8.5);
					RiRotate(-160, 1, 0, 0);
					RiRotate(30, 0, 1, 0);
					RiCone(2, 1, 360);
				RiTransformEnd();
				RiTransformBegin();
					RiTranslate(0, 0, 7.0);
					RiCylinder(3, 0, 10, 360);
				RiTransformEnd();
			RiWorldEnd();

		RiFrameEnd();

	RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 8: ShaderTest
 * The shaders together: a textured globe (TextureMap0, a scene shader that
 * calls STANDARD), BUMPY and CHECKERBOARD. Compare with
 * reference_renders/1_TestScenes/ShaderTest.png.
 *
 * Output: renders/1_TestScenes/ShaderTest.png
 * ---------------------------------------------------------------------- */

// Texture lookup: the texture replaces Cs, then the surface is lit as usual
/* The simplest scene shader there is, and the pattern for all the others:
 * read what you need from the context, set the inputs STANDARD uses (here
 * Cs, from the texture), call STANDARD. (sceneutil.h's TexturedStandard is
 * this with the slot as a parameter.) */
void TextureMap0(RtShaderContext *c)
{
	RiTexture(0, c->u, c->v, c->Cs);
	STANDARD(c);
}

void ShaderTest()
{
	RiBegin(RI_NULL);
		//RiFormat(960, 720);
		RiFormat(400, 300);
		//RiFormat(200, 150);
		RiDisplay("1_TestScenes/ShaderTest.png", "", "");
		RiPixelSamples(2,2);
		
		RiFrameBegin(0);
			/* set the perspective transformation */
			float fov = 45.0;
			RiProjection(RI_PERSPECTIVE, "fov", &fov);
			//RiProjection(RI_ORTHOGRAPHIC);

			RiWorldBegin();
				RiTransformBegin();
					RtColor color = {1,0,0};
					RtColor opacity = {.4,.4,.4};
					RiOpacity(opacity);
					RiColor(color);
					CheckerParams cp = { 20, 10 };
					RiSurface(CHECKERBOARD, &cp, sizeof cp);
					RiTranslate(0, 0.5, 11);
					RiRotate(70, 0, 1, 0);
					RiTorus(2.5, .5, 0, 360, 360);
				RiTransformEnd();
				RiTransformBegin();
					color[0] = 0; color[1] = 1; color[2] = 0;
					opacity[0] = 1; opacity[1] = 1; opacity[2] = 1;
					RiOpacity(opacity);
					RiSurface(NULL, NULL, 0);
					RiColor(color);
					RiTranslate(3, -1.5, 11);
					RiRotate(90, 1, 0, 0);
					RiCylinder(1, -1, 1, 360);
				RiTransformEnd();
				RiTransformBegin();
					opacity[0] = 1; opacity[1] = 1; opacity[2] = 1;
					RiOpacity(opacity);
					RiMakeTexture("assets/textures/earth.jpg", 0);
					RtShaderFn earthShader = TextureMap0;
					RiSurface(earthShader, NULL, 0);
					BumpParams bp1 = { 0.3f, 2, 6 };
					RiDisplacement(BUMPY, &bp1, sizeof bp1);
					RiTranslate(3, 1, 11.0);
					RiRotate(-40, 0, 0, 1);
					RiRotate(-100, 0, 1, 0);
					RiRotate(110, 1, 0, 0);
					RiSphere(1, -1.5, 1.5, 360);
				RiTransformEnd();
				RiTransformBegin();
					opacity[0] = .8; opacity[1] = .8; opacity[2] = .8;
					RiOpacity(opacity);
					RiSurface(earthShader, NULL, 0);
					RiDisplacement(NULL, NULL, 0);
					RiTranslate(-2.5, 1, 11.0);
					RiRotate(-175, 0, 1, 0);
					RiRotate(110, 1, 0, 0);
					RiSphere(1.5, -1.5, 1.5, 360);
				RiTransformEnd();
				RiTransformBegin();
					color[0] = 0.4; color[1] = 0.2; color[2] = 0;
					RiColor(color);
					opacity[0] = 1; opacity[1] = 1; opacity[2] = 1;
					RiOpacity(opacity);
					RiSurface(NULL, NULL, 0);
					BumpParams bp2 = { 0.15f, 2, 8 };
					RiDisplacement(BUMPY, &bp2, sizeof bp2);
					RiTranslate(-2.5, -2.5, 11);
					RiRotate(-90, 1, 0, 0);
					RiRotate(90, 0, 0, 1);
					RiCone(2.5, 1, 360);
				RiTransformEnd();
			RiWorldEnd();

		RiFrameEnd();

	RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 9: UVTest
 * The labeled uvtest.png on a sphere and a cylinder. Correct image: the big
 * R reads correctly on both, with s running left to right across the top (t
 * = 0 is the top row). A mirrored R means s or t is flipped; an upside-down
 * one, both.
 *
 * Output: renders/1_TestScenes/UVTest.png
 * ---------------------------------------------------------------------- */
void UVTest(void)
{
    float fov = 40.0f;
    RtColor opaque = {1, 1, 1};
    TextureParams label = {0}; /* unlit, so the labels read exactly as in the file */

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay("1_TestScenes/UVTest.png", "", "");
        RiPixelSamples(4, 4);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiMakeTexture("assets/textures/uvtest.png", 0);
                RiSurface(UnlitTexture, &label, sizeof label);
                RiOpacity(opaque);

                RiTransformBegin();
                    RiTranslate(-1.8, 0.4, 9.0);
                    RiRotate(90, 1, 0, 0);
                    RiRotate(70, 0, 0, 1);           /* bring the R around to face the camera */
                    RiSphere(1.6, -1.6, 1.6, 360);
                RiTransformEnd();

                RiTransformBegin();
                    RiTranslate(1.9, 0.4, 9.0);
                    RiRotate(90, 1, 0, 0);
                    RiRotate(70, 0, 0, 1);
                    RiCylinder(1.2, -1.6, 1.6, 360);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 10: Earth
 * The Earth texture on a sphere, with a little BUMPY. Compare with
 * reference_renders/1_TestScenes/Earth.png.
 *
 * Output: renders/1_TestScenes/Earth.png
 * ---------------------------------------------------------------------- */

void Earth()
{
	RiBegin(RI_NULL);
		//RiFormat(960, 720);
		RiFormat(400, 300);
		//RiFormat(200, 150);
		RiDisplay("1_TestScenes/Earth.png", "", "");
		RiPixelSamples(2,2);
		
		RiFrameBegin(0);
			/* set the perspective transformation */
			float fov = 45.0;
			RiProjection(RI_PERSPECTIVE, "fov", &fov);
			//RiProjection(RI_ORTHOGRAPHIC);

			RiWorldBegin();
				RiTransformBegin();
					RiMakeTexture("assets/textures/earth.jpg", 0);
					RtShaderFn earthShader = TextureMap0;
					RiSurface(earthShader, NULL, 0);
					RtColor blue = { 0, 0, 1 };
					RtColor opacity = { .9, .9, .9 };
					RiColor(blue);
					RiOpacity(opacity);
					BumpParams bp = { 0.01f, 3, 6 };
					RiDisplacement(BUMPY, &bp, sizeof bp);
					RiTranslate(0, 0, 5.0);
					RiRotate(-175, 0, 1, 0);
					RiRotate(110, 1, 0, 0);
					RiSphere(1, -1, 1, 360);
				RiTransformEnd();
			RiWorldEnd();

		RiFrameEnd();

	RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 11: DisplacementTest
 * The same grey sphere smooth and bumpy, and a bumpy sphere under a
 * non-uniform scale. Correct image: the bumps show in the shading, not just
 * the silhouette (otherwise N wasn't recomputed after displacement), and
 * the scaled sphere is lit from the right side.
 *
 * Output: renders/1_TestScenes/DisplacementTest.png
 * ---------------------------------------------------------------------- */
void DisplacementTest(void)
{
    float fov = 40.0f;
    RtColor grey = {0.6f, 0.6f, 0.6f}, opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay("1_TestScenes/DisplacementTest.png", "", "");
        RiPixelSamples(4, 4);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiColor(grey); RiOpacity(opaque);
                RiSurface(NULL, NULL, 0);

                RiTransformBegin();                  /* smooth */
                    RiDisplacement(NULL, NULL, 0);
                    RiTranslate(-3.0, 0.3, 9.0);
                    RiSphere(1.3, -1.3, 1.3, 360);
                RiTransformEnd();

                RiTransformBegin();                  /* bumpy */
                    BumpParams bp = { 0.25f, 2, 6 };
                    RiDisplacement(BUMPY, &bp, sizeof bp);
                    RiTranslate(0.0, 0.3, 9.0);
                    RiSphere(1.3, -1.3, 1.3, 360);
                RiTransformEnd();

                RiTransformBegin();                  /* bumpy under non-uniform scale */
                    RiDisplacement(BUMPY, &bp, sizeof bp);
                    RiTranslate(3.0, 0.3, 9.0);
                    RiScale(0.6, 1.6, 1.0);
                    RiSphere(1.3, -1.3, 1.3, 360);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 12: BumpVsDisplacement
 * The same bubbly pattern on two orange plastic spheres. Left: a bump map,
 * a surface shader that only tilts N by the pattern's slope, so the
 * surface stays a sphere. Right: a displacement shader that moves P by
 * the same pattern, and the renderer recomputes N. Correct image: the two
 * spheres shade alike, but the left silhouette and shadow are smooth
 * circles and the right ones are lumpy. The shadows come from a depth
 * pass (see ShadowTest); until your depth output works the floor is
 * plain white.
 *
 * Output: renders/1_TestScenes/BumpVsDisplacement.png
 *         renders/1_TestScenes/BumpVsDisplacement_key.z (the depth file)
 * ---------------------------------------------------------------------- */

static void Tiles(const float o[3], const float a[3], const float b[3], int n); /* ShadowTest's */

/* A repeatable pseudo-random number in [0, 1) for a cell and a seed. */
static double Hash3(int x, int y, int z, int seed)
{
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)z * 83492791u ^
                 (unsigned)seed * 2654435761u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 0xffffff) / 16777216.0;
}

/* The height of a field of bubbles at a point, from -0.5 to 0.5: one
 * round bump of radius R sits in every unit cell of space, at a jittered
 * spot near the cell's center so that neighbors stay apart, and the
 * nearest one counts (Worley's F1). 0.5 on top of a bump, -0.5 on the
 * flat ground between bumps. Not noise: a distance field, which is why
 * the bumps are round. */
static double Bubbles(double x, double y, double z)
{
    const double R = 0.75;   /* bump radius: neighbors touch, and now and then overlap */
    int ix = (int)std::floor(x), iy = (int)std::floor(y), iz = (int)std::floor(z);
    double nearest = R;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                int cx = ix + dx, cy = iy + dy, cz = iz + dz;
                double px = cx + 0.25 + 0.5 * Hash3(cx, cy, cz, 0) - x;
                double py = cy + 0.25 + 0.5 * Hash3(cx, cy, cz, 1) - y;
                double pz = cz + 0.25 + 0.5 * Hash3(cx, cy, cz, 2) - z;
                nearest = std::min(nearest, std::sqrt(px * px + py * py + pz * pz));
            }
    double t = nearest / R;
    return 0.5 - t * t; /* a rounded bump that meets the ground at R */
}

/* Both bubble shaders take these: the pattern's scale (bubbles per unit
 * of object space) and its height, from the ground between bumps to a
 * bump's top (in camera-space units). The bumps rise above the sphere by
 * half of it and the ground sinks below it by the other half, so the
 * displaced sphere keeps the undisplaced one's size. */
typedef struct {
    RtFloat scale, amplitude;
} BubbleParams;

/* The plastic: saturated orange with a broad white highlight, and a faint
 * white sheen where the surface turns away from the eye (Fresnel), which
 * is what makes a shiny plastic look coated. The same shader lights both
 * spheres; only where its N comes from differs. */
static void OrangePlastic(RtShaderContext *c)
{
    const double color[3] = {0.95, 0.47, 0.12};
    const double ka = 0.25, kd = 0.75, ks = 0.45, exponent = 28, sheen = 0.35;
    double diffuse[3], specular[3];
    StandardLighting(c, exponent, diffuse, specular);
    double rim = sheen * Fresnel(c, 0.02);
    for (int i = 0; i < 3; ++i) {
        c->Ci[i] = (RtFloat)(color[i] * (ka + kd * diffuse[i]) * (1 - rim) + rim + ks * specular[i]);
        c->Oi[i] = c->Os[i];
    }
}

/* Displacement: P moves out along N by amplitude * Bubbles(scale * Po). */
static void BubbleDisplace(RtShaderContext *c)
{
    const BubbleParams *p = (const BubbleParams *)c->params;
    double h = p->amplitude * Bubbles(p->scale * c->Po[0], p->scale * c->Po[1], p->scale * c->Po[2]);
    double len = std::sqrt(c->N[0] * c->N[0] + c->N[1] * c->N[1] + c->N[2] * c->N[2]);
    for (int i = 0; i < 3; ++i)
        c->P[i] += (RtFloat)(h * c->N[i] / len);
}

/* Bump map: the normal the displaced surface would have, without moving P.
 * A surface pushed out by a height h along N has normal N - grad h (the
 * part of the gradient along the surface), so the gradient of
 * amplitude * Bubbles(scale * Po) is taken by central differences and N is
 * tilted against it. The gradient is in object space; the spheres here
 * are only translated, so it is the camera-space gradient too. Then the
 * same plastic as the displaced sphere. */
static void BubblePlastic(RtShaderContext *c)
{
    const BubbleParams *p = (const BubbleParams *)c->params;
    const double e = 1e-3; /* a small step in object space */
    double g[3];
    for (int a = 0; a < 3; ++a) {
        double q[3] = {c->Po[0], c->Po[1], c->Po[2]};
        q[a] += e;
        double hi = Bubbles(p->scale * q[0], p->scale * q[1], p->scale * q[2]);
        q[a] -= 2 * e;
        double lo = Bubbles(p->scale * q[0], p->scale * q[1], p->scale * q[2]);
        g[a] = p->amplitude * (hi - lo) / (2 * e);
    }
    double n[3] = {c->N[0], c->N[1], c->N[2]};
    double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (int i = 0; i < 3; ++i)
        n[i] /= len;
    double gn = g[0] * n[0] + g[1] * n[1] + g[2] * n[2];
    for (int i = 0; i < 3; ++i)
        n[i] -= g[i] - gn * n[i];
    len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    for (int i = 0; i < 3; ++i)
        c->N[i] = (RtFloat)(n[i] / len);

    OrangePlastic(c);
}

/* The floor: white, darkened where the key light's depth map says the
 * light is blocked, and nothing else (no lighting), so that the floor and
 * the backdrop merge into one white field with just the shadows on it. */
static void ShadowedWhite(RtShaderContext *c)
{
    const ShadowParams *s = (const ShadowParams *)c->params;
    double lit = 1;
    if (s->map) {
        double Pw[3];
        for (int i = 0; i < 3; ++i)
            Pw[i] = s->cameraToWorld[i][0] * c->P[0] + s->cameraToWorld[i][1] * c->P[1] +
                    s->cameraToWorld[i][2] * c->P[2] + s->cameraToWorld[i][3];
        lit = zfile_visibility(s->map, Pw);
    }
    c->Ci[0] = c->Ci[1] = c->Ci[2] = (RtFloat)(0.62 + 0.38 * lit);
    for (int i = 0; i < 3; ++i)
        c->Oi[i] = c->Os[i];
}

/* The spheres sit at depth CZ on a floor that tilts up and away from the
 * camera by PITCH degrees, as if the camera looked down at them. */
static void BumpVsDisplacementObjects(const ShadowParams *sp)
{
    const float R = 1.3f, CZ = 9.5f, PITCH = 14;
    const float sn = std::sin(PITCH * 3.14159265f / 180), cs = std::cos(PITCH * 3.14159265f / 180);
    RtColor white = {1, 1, 1}, opaque = {1, 1, 1};
    RiOpacity(opaque);
    RiColor(white);

    /* the floor: up the tilted plane from just in front of the camera to
     * far behind the spheres, as tiles; then a white backdrop beyond it */
    const float up[3] = {0, cs, -sn};                /* the floor's normal */
    const float along[3] = {0, sn, cs};              /* up the floor, away from the camera */
    const float foot[3] = {0, -R * up[1], CZ - R * up[2]}; /* under the spheres' centers */
    const float t0 = -(foot[2] - 1.0f) / along[2], t1 = 60; /* from z = 1 to far away */
    float o[3], a[3] = {90, 0, 0}, b[3];
    for (int i = 0; i < 3; ++i) {
        o[i] = foot[i] + t0 * along[i] - a[i] / 2;
        b[i] = (t1 - t0) * along[i];
    }
    RiSurface(ShadowedWhite, sp, sizeof *sp);
    Tiles(o, a, b, 8);
    RiSurface(CONSTANT, NULL, 0);
    {
        RtPoint P[4] = {{-200, -100, 80}, {200, -100, 80}, {200, 200, 80}, {-200, 200, 80}};
        RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
        RiPointsPolygons(1, nverts, verts, P, NULL, NULL);
    }

    BubbleParams bp = {2.3f, 0.18f};
    RiSurface(BubblePlastic, &bp, sizeof bp);  /* left: the bump map */
    RiDisplacement(NULL, NULL, 0);
    RiTransformBegin();
        RiTranslate(-1.65f, 0, CZ);
        RiSphere(R, -R, R, 360);
    RiTransformEnd();

    RiSurface(OrangePlastic, NULL, 0);          /* right: displaced */
    RiDisplacement(BubbleDisplace, &bp, sizeof bp);
    RiTransformBegin();
        RiTranslate(1.65f, 0, CZ);
        RiSphere(R, -R, R, 360);
    RiTransformEnd();
    RiDisplacement(NULL, NULL, 0);
}

void BumpVsDisplacement(void)
{
    float lightFov = 32.0f, fov = 36.0f;
    const double key[3] = {-6, 8, -4}, target[3] = {0, -0.5, 9.5};
    RtMatrix lightCamera;
    LookAt(key, target, lightCamera);

    /* the picture's camera is the world: no transform */
    ShadowParams sp;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            sp.cameraToWorld[r][c] = r == c ? 1 : 0;
    sp.light = 0;
    sp.map = NULL;

    RiBegin(RI_NULL);
        /* pass 1: depth seen from the key light */
        RiFrameBegin(0);
            RiFormat(400, 400);  /* coarse on purpose: the PCF makes soft edges of it */
            RiPixelSamples(1, 1);
            RiDisplay("1_TestScenes/BumpVsDisplacement_key.z", "zfile", "z");
            RiProjection(RI_PERSPECTIVE, "fov", &lightFov);
            RiConcatTransform(lightCamera);
            RiWorldBegin();
                BumpVsDisplacementObjects(&sp);
            RiWorldEnd();
        RiFrameEnd();

        /* pass 2: the picture, shadowed by what pass 1 wrote */
        ZFile *map = zfile_load("1_TestScenes/BumpVsDisplacement_key.z", lightCamera, lightFov);
        sp.map = map;
        RiFrameBegin(1);
            RiFormat(800, 450);
            RiPixelSamples(4, 4);
            RiDisplay("1_TestScenes/BumpVsDisplacement.png", "", "");
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                BumpVsDisplacementObjects(&sp);
            RiWorldEnd();
        RiFrameEnd();
        zfile_free(map);
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 13: Tornado
 * A cone under heavy BUMPY displacement. Compare with
 * reference_renders/1_TestScenes/Tornado.png.
 *
 * Output: renders/1_TestScenes/Tornado.png
 * ---------------------------------------------------------------------- */

void Tornado()
{
	RiBegin(RI_NULL);
		//RiFormat(960, 720);
		RiFormat(400, 300);
		//RiFormat(200, 150);
		RiDisplay("1_TestScenes/Tornado.png", "", "");
		RiPixelSamples(2,2);
		
		RiFrameBegin(0);
			/* set the perspective transformation */
			float fov = 45.0;
			RiProjection(RI_PERSPECTIVE, "fov", &fov);
			//RiProjection(RI_ORTHOGRAPHIC);

			RiWorldBegin();
				RiTransformBegin();
					RtColor gray = { .4, .4, .4 };
					RtColor opacity = { 0.5, 0.5, 0.5 };
					RiColor(gray);
					RiOpacity(opacity);
					BumpParams bp = { 1, 3, 10 };
					RiDisplacement(BUMPY, &bp, sizeof bp);
					RiTranslate(0, 0, 5.0);
					RiRotate(90, 0, 1, 0);
					RiRotate(80, 1, 0, 0);
					RiTranslate(0, 0, -1.5);
					RiCone(3, 1, 360);					
				RiTransformEnd();
			RiWorldEnd();

		RiFrameEnd();

	RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 14: TransparencyTest
 * Four partial quadrics, semi-transparent and overlapping. Compare with
 * reference_renders/1_TestScenes/TransparencyTest.png.
 *
 * Output: renders/1_TestScenes/TransparencyTest.png
 * ---------------------------------------------------------------------- */

void TransparencyTest()
{
	RiBegin(RI_NULL);
		//RiFormat(960, 720);
		RiFormat(400, 300);
		//RiFormat(200, 150);
		RiDisplay("1_TestScenes/TransparencyTest.png", "", "");
		RiPixelSamples(2,2);
		
		RiFrameBegin(0);
			/* set the perspective transformation */
			float fov = 45.0;
			RiProjection(RI_PERSPECTIVE, "fov", &fov);
			//RiProjection(RI_ORTHOGRAPHIC);

			RiWorldBegin();
				RiTransformBegin();
					RtColor color = {1,0,0};
					RtColor opacity = {.4,.4,.4};
					RiOpacity(opacity);
					RiColor(color);
					RiTranslate(0, 0.5, 7.0);
					RiRotate(60, 1, 0, 0);
					RiTorus(1, .25, 0, 250, 270);
				RiTransformEnd();
				RiTransformBegin();
					color[0] = 0; color[1] = 1;
					opacity[0] = 0.4; opacity[1] = 0.4; opacity[2] = 0.4;
					RiOpacity(opacity);
					RiColor(color);
					RiTranslate(0, 0, 8.0);
					RiRotate(60, 1, 0, 0);
					RiRotate(30, 0, 1, 0);
					RiCylinder(1, -1, 1, 300);
				RiTransformEnd();
				RiTransformBegin();
					color[1] = 0; color[2] = 1;
					opacity[0] = .7; opacity[1] = .7; opacity[2] = .7;
					RiOpacity(opacity);
					RiColor(color);
					RiTranslate(0, 1, 9.0);
					RiRotate(60, 1, 0, 0);
					RiSphere(1.8, -1.0, 1.0, 250);
				RiTransformEnd();
				RiTransformBegin();
					color[0] = 1; color[1] = .6; color[2] = .6;
					RiColor(color);
					opacity[0] = .6; opacity[1] = .6; opacity[2] = .6;
					RiOpacity(opacity);
					RiTranslate(0, -1, 8.5);
					RiRotate(-160, 1, 0, 0);
					RiRotate(30, 0, 1, 0);
					RiRotate(140, 0, 0, 1);
					RiCone(2.5, 1, 270);
				RiTransformEnd();
			RiWorldEnd();

		RiFrameEnd();

	RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 15: TransparencyOrder
 * Three nested semi-transparent hemispheres around an opaque core, drawn in
 * the wrong order on purpose. Correct image: the same as drawing them in
 * order, the white core showing through red over green over blue. A
 * different picture means the hits in each sample aren't sorted by depth.
 *
 * Output: renders/1_TestScenes/TransparencyOrder.png
 * ---------------------------------------------------------------------- */
void TransparencyOrder(void)
{
    float fov = 40.0f;
    RtColor red = {0.9f, 0.15f, 0.15f}, green = {0.15f, 0.8f, 0.2f};
    RtColor blue = {0.2f, 0.3f, 0.95f}, white = {1, 1, 1};
    RtColor o30 = {0.3f, 0.3f, 0.3f}, opaque = {1, 1, 1};

    RiBegin(RI_NULL);
        RiFormat(640, 480);
        RiDisplay("1_TestScenes/TransparencyOrder.png", "", "");
        RiPixelSamples(4, 4);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiSurface(NULL, NULL, 0);

                RiTransformBegin();                  /* middle shell first (green) */
                    RiColor(green); RiOpacity(o30);
                    RiTranslate(0, -0.5, 9.0);
                    RiRotate(-110, 1, 0, 0);
                    RiSphere(2.0, 0.0, 2.0, 360);    /* upper hemisphere only */
                RiTransformEnd();

                RiTransformBegin();                  /* outer shell (red) */
                    RiColor(red); RiOpacity(o30);
                    RiTranslate(0, -0.5, 9.0);
                    RiRotate(-110, 1, 0, 0);
                    RiSphere(2.8, 0.0, 2.8, 360);
                RiTransformEnd();

                RiTransformBegin();                  /* opaque core, drawn third */
                    RiColor(white); RiOpacity(opaque);
                    RiTranslate(0, -0.5, 9.0);
                    RiSphere(0.6, -0.6, 0.6, 360);
                RiTransformEnd();

                RiTransformBegin();                  /* inner shell last (blue) */
                    RiColor(blue); RiOpacity(o30);
                    RiTranslate(0, -0.5, 9.0);
                    RiRotate(-110, 1, 0, 0);
                    RiSphere(1.3, 0.0, 1.3, 360);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 16: PolygonTest
 * Five checks of RiPointsPolygons. Top left: uvtest.png on a quad and on
 * the same quad as two triangles, which must look the same. Bottom left: a
 * cube with no N, shaded flat, and a sphere of 80 polygons with vertex
 * normals, shaded smooth. Bottom right: a checkered triangle with no
 * cracks, and a far one that must still be a dot. Top right: a tilted quad
 * with BUMPY.
 *
 * Output: renders/1_TestScenes/PolygonTest.png
 * ---------------------------------------------------------------------- */

static const double PI = 3.14159265358979323846;

static void Quad(RtFloat x0, RtFloat y0, RtFloat x1, RtFloat y1, RtFloat z, int asTriangles)
{
    /* corners in cycle order from the top left; st with t = 0 at the top */
    RtPoint P[4] = {{x0, y1, z}, {x1, y1, z}, {x1, y0, z}, {x0, y0, z}};
    RtFloat st[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    if (asTriangles) {
        RtInt nverts[2] = {3, 3}, verts[6] = {0, 1, 2, 0, 2, 3};
        RiPointsPolygons(2, nverts, verts, P, NULL, st);
    } else {
        RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
        RiPointsPolygons(1, nverts, verts, P, NULL, st);
    }
}

/* A unit cube centered at the origin, one quad per face, no normals. */
static void Cube(void)
{
    RtPoint P[8] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
                    {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
    RtInt nverts[6] = {4, 4, 4, 4, 4, 4};
    RtInt verts[24] = {0, 1, 2, 3, 5, 4, 7, 6, 4, 0, 3, 7, 1, 5, 6, 2, 3, 2, 6, 7, 4, 5, 1, 0};
    RiPointsPolygons(6, nverts, verts, P, NULL, NULL);
}

/* A unit sphere of NLON x NLAT faces: triangles around the poles, quads in
 * between, with the exact sphere normal at every vertex. */
static void LatLongSphere(void)
{
    enum { NLON = 10, NLAT = 8, NV = 2 + NLON * (NLAT - 1) };
    RtPoint P[NV];
    RtNormal N[NV];
    RtInt nverts[NLON * NLAT], verts[4 * NLON * NLAT];

    /* vertex 0 the south pole, 1 the north pole, then the rings */
    for (int c = 0; c < 3; ++c)
        P[0][c] = P[1][c] = 0;
    P[0][1] = -1;
    P[1][1] = 1;
    for (int j = 1; j < NLAT; ++j)
        for (int i = 0; i < NLON; ++i) {
            double phi = PI * j / NLAT - PI / 2, theta = 2 * PI * i / NLON;
            RtFloat *p = P[2 + (j - 1) * NLON + i];
            p[0] = (RtFloat)(std::cos(phi) * std::cos(theta));
            p[1] = (RtFloat)std::sin(phi);
            p[2] = (RtFloat)(std::cos(phi) * std::sin(theta));
        }
    for (int k = 0; k < NV; ++k)
        for (int c = 0; c < 3; ++c)
            N[k][c] = P[k][c]; /* a unit sphere's normal is its position */

    int npolys = 0, nindices = 0;
    for (int j = 0; j < NLAT; ++j)
        for (int i = 0; i < NLON; ++i) {
            int i1 = (i + 1) % NLON;
            int lo = 2 + (j - 1) * NLON, hi = 2 + j * NLON; /* rings below and above */
            int face[4], n = 0;
            if (j > 0) {
                face[n++] = lo + i;
                face[n++] = lo + i1;
            } else {
                face[n++] = 0; /* south pole */
            }
            if (j < NLAT - 1) {
                face[n++] = hi + i1;
                face[n++] = hi + i;
            } else {
                face[n++] = 1; /* north pole */
            }
            nverts[npolys++] = n;
            for (int k = 0; k < n; ++k)
                verts[nindices++] = face[k];
        }
    RiPointsPolygons(npolys, nverts, verts, P, N, NULL);
}

void PolygonTest(void)
{
    float fov = 40.0f;
    TextureParams label = {0};
    RtColor opaque = {1, 1, 1}, orange = {0.95f, 0.55f, 0.15f}, sky = {0.45f, 0.65f, 0.95f};
    RtColor yellow = {0.95f, 0.85f, 0.2f}, white = {1, 1, 1}, grey = {0.6f, 0.6f, 0.6f};

    RiBegin(RI_NULL);
        RiFormat(500, 500);
        RiDisplay("1_TestScenes/PolygonTest.png", "", "");
        RiPixelSamples(2, 2);
        RiFrameBegin(0);
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                RiOpacity(opaque);

                /* 1 and 2: one quad, and the same quad as two triangles */
                RiMakeTexture("assets/textures/uvtest.png", 0);
                RiSurface(UnlitTexture, &label, sizeof label);
                Quad(-3.4f, 1.6f, -1.9f, 3.1f, 10, 0);
                Quad(-1.7f, 1.6f, -0.2f, 3.1f, 10, 1);

                /* 3: a flat-shaded cube and a smooth, stretched sphere */
                RiSurface(NULL, NULL, 0);
                RiTransformBegin();
                    RiColor(orange);
                    RiTranslate(-2.55f, -1.75f, 10);
                    RiRotate(25, 1, 0, 0);
                    RiRotate(35, 0, 1, 0);
                    RiScale(1.2f, 1.2f, 1.2f);
                    Cube();
                RiTransformEnd();
                RiTransformBegin();
                    RiColor(sky);
                    RiTranslate(-0.85f, -1.75f, 10);
                    RiScale(0.55f, 0.55f, 0.55f);
                    RiScale(1, 2, 1);
                    LatLongSphere();
                RiTransformEnd();

                /* 4: a large checkered triangle and a tiny one far away */
                {
                    CheckerParams cp = { 8, 8 };
                    RiColor(yellow);
                    RiSurface(CHECKERBOARD, &cp, sizeof cp);
                    RtPoint P[3] = {{0.3f, -0.3f, 10}, {3.4f, -0.3f, 10}, {0.3f, -3.4f, 10}};
                    RtFloat st[6] = {0, 0, 1, 0, 0, 1};
                    RtInt nverts[1] = {3}, verts[3] = {0, 1, 2};
                    RiPointsPolygons(1, nverts, verts, P, NULL, st);

                    RiColor(white);
                    RiSurface(NULL, NULL, 0);
                    RtPoint tiny[3] = {{60, -60, 200}, {60.6f, -60, 200}, {60.3f, -59.4f, 200}};
                    RiPointsPolygons(1, nverts, verts, tiny, NULL, NULL);
                }

                /* 5: a bumpy quad, tilted back */
                RiTransformBegin();
                    BumpParams bp = { 0.12f, 1, 4 };
                    RiColor(grey);
                    RiDisplacement(BUMPY, &bp, sizeof bp);
                    RiTranslate(1.9f, 1.7f, 10);
                    RiRotate(-55, 1, 0, 0);
                    Quad(-1.3f, -1.3f, 1.3f, 1.3f, 0, 0);
                    RiDisplacement(NULL, NULL, 0);
                RiTransformEnd();
            RiWorldEnd();
        RiFrameEnd();
    RiEnd();
}

/* ------------------------------------------------------------------------
 * Test Scene 17: ShadowTest
 * Depth output for a shadow map: the scene renders depth from the key light
 * (RiDisplay type "zfile"), then the picture through shadowmap.h. Correct
 * image: the sphere, the pillar and the ring cast soft shadows down and to
 * the right, and the glass sphere casts none. No shadows means the depth
 * file wasn't written; shadows mirrored top to bottom, its rows were
 * written bottom first; shadows shrunk away from the center, distance from
 * the eye was written instead of camera-space z.
 *
 * Output: renders/1_TestScenes/ShadowTest.png
 *         renders/1_TestScenes/ShadowTest_key.z (the depth file)
 * ---------------------------------------------------------------------- */

static void ShadowLit(RtShaderContext *c)
{
    ShadowedStandard(c, (const ShadowParams *)c->params);
}

/* A quad from corner o along edges a and b, as n x n tiles. */
static void Tiles(const float o[3], const float a[3], const float b[3], int n)
{
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i) {
            RtPoint P[4];
            const float s[4] = {(float)i / n, (float)(i + 1) / n, (float)(i + 1) / n, (float)i / n};
            const float t[4] = {(float)j / n, (float)j / n, (float)(j + 1) / n, (float)(j + 1) / n};
            for (int k = 0; k < 4; ++k)
                for (int c = 0; c < 3; ++c)
                    P[k][c] = o[c] + s[k] * a[c] + t[k] * b[c];
            RtInt nverts[1] = {4}, verts[4] = {0, 1, 2, 3};
            RiPointsPolygons(1, nverts, verts, P, NULL, NULL);
        }
}

static void ShadowTestObjects(const ShadowParams *sp)
{
    RtColor grey = {0.7f, 0.7f, 0.7f}, red = {0.9f, 0.15f, 0.15f}, green = {0.15f, 0.7f, 0.25f};
    RtColor yellow = {0.95f, 0.8f, 0.2f}, glass = {0.6f, 0.75f, 0.95f};
    RtColor opaque = {1, 1, 1}, clear = {0.3f, 0.3f, 0.3f};

    RiSurface(ShadowLit, sp, sizeof *sp);
    RiOpacity(opaque);
    RiColor(grey);
    const float fo[3] = {-9, -1.5f, 3}, fa[3] = {18, 0, 0}, fb[3] = {0, 0, 12};
    Tiles(fo, fa, fb, 4); /* floor */
    const float wo[3] = {-9, -1.5f, 15}, wa[3] = {18, 0, 0}, wb[3] = {0, 7.5f, 0};
    Tiles(wo, wa, wb, 4); /* back wall */

    RiColor(red);
    RiTransformBegin();
        RiTranslate(-0.7f, -0.5f, 10.0f);
        RiSphere(1.0f, -1.0f, 1.0f, 360);
    RiTransformEnd();
    RiColor(green);
    RiTransformBegin();
        RiTranslate(2.0f, 0, 12.0f);
        RiRotate(-90, 1, 0, 0);
        RiCylinder(0.45f, -1.5f, 2.2f, 360);
    RiTransformEnd();
    RiColor(yellow);
    RiTransformBegin();
        RiTranslate(2.4f, -0.6f, 8.0f);
        RiRotate(20, 0, 1, 0);
        RiTorus(0.7f, 0.18f, 0, 360, 360);
    RiTransformEnd();
    RiColor(glass);
    RiOpacity(clear);
    RiTransformBegin();
        RiTranslate(-2.6f, -0.8f, 7.0f);
        RiSphere(0.7f, -0.7f, 0.7f, 360);
    RiTransformEnd();
}

void ShadowTest(void)
{
    float lightFov = 60.0f, fov = 40.0f;
    const double key[3] = {-6, 8, -4}, target[3] = {0.5, -0.5, 10};
    RtMatrix lightCamera;
    LookAt(key, target, lightCamera);

    /* the picture's camera is the world: no transform */
    ShadowParams sp;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            sp.cameraToWorld[r][c] = r == c ? 1 : 0;
    sp.light = 0;
    sp.map = NULL;

    RiBegin(RI_NULL);
        /* pass 1: depth seen from the key light */
        RiFrameBegin(0);
            RiFormat(1024, 1024);
            RiPixelSamples(1, 1);
            RiDisplay("1_TestScenes/ShadowTest_key.z", "zfile", "z");
            RiProjection(RI_PERSPECTIVE, "fov", &lightFov);
            RiConcatTransform(lightCamera);
            RiWorldBegin();
                ShadowTestObjects(&sp);
            RiWorldEnd();
        RiFrameEnd();

        /* pass 2: the picture, shadowed by what pass 1 wrote */
        ZFile *map = zfile_load("1_TestScenes/ShadowTest_key.z", lightCamera, lightFov);
        sp.map = map;
        RiFrameBegin(1);
            RiFormat(640, 480);
            RiPixelSamples(4, 4);
            RiDisplay("1_TestScenes/ShadowTest.png", "", "");
            RiProjection(RI_PERSPECTIVE, "fov", &fov);
            RiWorldBegin();
                ShadowTestObjects(&sp);
            RiWorldEnd();
        RiFrameEnd();
        zfile_free(map);
    RiEnd();
}
