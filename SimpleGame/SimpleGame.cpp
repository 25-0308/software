/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Camera.h"
#include "GameObject.h"
#include "PostProcess.h"
#include "Renderer.h"

Renderer *g_Renderer = NULL;
Camera *g_Camera = NULL;
PostProcess *g_PostProcess = NULL;
std::vector<GameObject> g_Objects;
std::chrono::steady_clock::time_point g_LastFrameTime;
float g_ElapsedSeconds = 0.f;

// Mood tuning for the post-process pass: exposure controls how quickly
// highlights roll off (tone mapping), vignetteStrength controls how much
// the screen edges darken, bloomThreshold is the minimum luminance that
// starts glowing, bloomIntensity controls how strong that glow looks.
float g_Exposure = 1.0f;
float g_VignetteStrength = 0.6f;
float g_BloomThreshold = 0.9f;
float g_BloomIntensity = 0.8f;

void RenderScene(void)
{
	g_PostProcess->BeginCapture();

	glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// This is a 2.5D scene, not a true 3D depth buffer: sort back-to-front by
	// world depth (y + z) each frame and draw in that order (painter's algorithm).
	std::vector<GameObject> drawOrder = g_Objects;
	std::sort(drawOrder.begin(), drawOrder.end(), [](const GameObject& lhs, const GameObject& rhs)
	{
		return (lhs.y + lhs.z) < (rhs.y + rhs.z);
	});

	Mat4 viewProjection = g_Camera->GetViewProjection();

	for (const GameObject& obj : drawOrder)
	{
		Mat4 model = Mat4::Translate(obj.x, obj.y, obj.z) * Mat4::Scale(obj.size, obj.size, obj.size);
		Mat4 mvp = viewProjection * model;
		g_Renderer->DrawObject(mvp, obj.r, obj.g, obj.b, obj.a);
	}

	g_PostProcess->EndCaptureAndPresent(g_Exposure, g_VignetteStrength, g_BloomThreshold, g_BloomIntensity);

	glutSwapBuffers();
}

void Update(float deltaSeconds)
{
	g_ElapsedSeconds += deltaSeconds;

	// Prototype proof that the loop is driven by real elapsed time: bob the
	// first object up and down instead of leaving the scene static.
	if (!g_Objects.empty())
	{
		g_Objects[0].z = sinf(g_ElapsedSeconds * 2.f) * 0.5f;
	}
}

void Idle(void)
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float deltaSeconds = std::chrono::duration<float>(now - g_LastFrameTime).count();
	g_LastFrameTime = now;

	Update(deltaSeconds);
	RenderScene();
}

void MouseInput(int button, int state, int x, int y)
{
}

void MouseWheel(int wheel, int direction, int x, int y)
{
	// direction is +1 for wheel-up (zoom in), -1 for wheel-down (zoom out).
	const float kZoomStep = 1.1f;
	g_Camera->AdjustZoom(direction > 0 ? kZoomStep : 1.f / kZoomStep);
}

void KeyInput(unsigned char key, int x, int y)
{
}

void SpecialKeyInput(int key, int x, int y)
{
}

int main(int argc, char **argv)
{
	// Initialize GL things
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(800, 600);
	glutCreateWindow("Game Software Engineering KPU");

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n ";
	}

	// Initialize Renderer
	g_Renderer = new Renderer(800, 600);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.. \n";
	}

	g_Camera = new Camera(8.f, 6.f, 1.f);
	g_PostProcess = new PostProcess(800, 600);

	// Prototype scene: a few objects spread across world X/Y/Z so the
	// quarter-view projection and back-to-front depth sort are both visible.
	// The first object's color/luminance sits above g_BloomThreshold so it
	// reads as a glowing, self-lit object rather than a flat bright color.
	g_Objects.push_back({ 0.f, 0.f, 0.f, 1.f, 4.f, 0.8f, 0.5f, 1.f });
	g_Objects.push_back({ 1.5f, 1.f, 0.f, 1.f, 0.3f, 1.f, 0.3f, 1.f });
	g_Objects.push_back({ -1.5f, 1.f, 0.f, 1.f, 0.3f, 0.3f, 1.f, 1.f });
	g_Objects.push_back({ 0.f, 2.f, 0.f, 2.f, 0.6f, 0.6f, 0.6f, 1.f });

	g_LastFrameTime = std::chrono::steady_clock::now();

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutMouseFunc(MouseInput);
	glutMouseWheelFunc(MouseWheel);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

	delete g_PostProcess;
	delete g_Camera;
	delete g_Renderer;

    return 0;
}
