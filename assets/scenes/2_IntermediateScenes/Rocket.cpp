/*
 * ECE/CS 285 – HW1 Intermediate Scene: Rocket
 * A rocket built from quadrics, three of its parts displaced with BUMPY.
 * Compare with reference_renders/2_IntermediateScenes/Rocket.png.
 *
 * Output: renders/2_IntermediateScenes/Rocket.png
 */

#include "Ri.h"
#include "shaders.h"

void Rocket()
{
	RiBegin(RI_NULL);
		//RiFormat(960, 720);
		RiFormat(400, 300);
		//RiFormat(200, 150);
		RiDisplay("2_IntermediateScenes/Rocket.png", "", "");
		RiPixelSamples(2,2);
		
		RiFrameBegin(0);
			/* set the perspective transformation */
			float fov = 45.0;
			RiProjection(RI_PERSPECTIVE, "fov", &fov);
			//RiProjection(RI_ORTHOGRAPHIC);

			RiWorldBegin();
				RiTransformBegin();
				// water
					RtColor color = {0,0,1};
					RtColor opacity = {.6,.6,.6};
					RiOpacity(opacity);
					RiColor(color);
					BumpParams bp1 = { 0.05f, 2, 6 };
					RiDisplacement(BUMPY, &bp1, sizeof bp1);
					RiScale(7, 1.15, 1);
					RiTranslate(0, -0.75, 4);
					RiRotate(180, 0, 1, 0);
					RiRotate(110, 1, 0, 0);
					RiTranslate(0, 0, 1);
					RiRotate(180, 1, 0, 0);
					RiSphere(1, .6, 1, 360);
				// mountain
					color[0] = 0.6; color[1] = 0.3; color[2] = 0;
					opacity[0] = 1; opacity[1] = 1; opacity[2] = 1;
					RiColor(color);
					RiOpacity(opacity);
					RiDisplacement(NULL, NULL, 0);
					BumpParams bp2 = { 2, 3, 9 };
					RiDisplacement(BUMPY, &bp2, sizeof bp2);
					RiTranslate(0, 4, -6);
					RiRotate(15, 1, 0, 0);
					RiSphere(7, 6.8, 7, 360);
				RiTransformEnd();

				RiTransformBegin();
				// rocket
					RiDisplacement(NULL, NULL, 0);
					color[0] = .8; color[1] = 0.8; color[2] = .8;
					RiColor(color);
					RiTranslate(-.25, .25, 2);
					RiRotate(40, 0, 0, 1);
					RiScale(1, 2, 1);
					RiRotate(150, 0, 1, 0);
					RiRotate(-90, 1, 0, 0);
					RiSphere(.1, -.1, .1, 360);
					
					RiTranslate(0, 0, .07);
					RiCone(.075, .075, 360);
					
					RiSurface(NULL, NULL, 0);
					BumpParams bp3 = { 0.1f, 5, 7 };
					RiDisplacement(BUMPY, &bp3, sizeof bp3);
					color[0] = 1; color[1] = 0.27; color[2] = 0;
					opacity[0] = .4; opacity[1] = .4; opacity[2] = .4;
					RiOpacity(opacity);
					RiColor(color);
					RiTranslate(0, 0, -.45);
					RiCone(.3, .1, 360);

					color[1] = .1;
					RiColor(color);
					opacity[0] = 1; opacity[1] = 1; opacity[2] = 1;
					RiOpacity(opacity);
					RiDisplacement(NULL, NULL, 0);
					RiScale(20, .5, 1);
					RiTranslate(0, 0, .375);
					RiSphere(.025, -.025, .025, 360);

					RiScale(1.0f / 40.0f, 16.0f, 1.0f);
					RiTranslate(0, -0.015, -.075);
					RiSphere(.025, -.025, .025, 360);
				RiTransformEnd();
			RiWorldEnd();

		RiFrameEnd();

	RiEnd();
}
