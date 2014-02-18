#pragma once

//This file should not be included in a location that will make its contents visible to user code.

#include "ofUtils.h"

#include "GLFW\glfw3.h"
#include <sstream>
#include <string>

#include "CX_GLFWWindow_Compat.h"

namespace CX {
	namespace Private {
		extern GLFWwindow *glfwContext;

		extern ofPtr<ofAppGLFWCompatibilityWindow> window;

		struct CX_GLVersion {
			CX_GLVersion(void) :
				major(0),
				minor(0),
				release(0)
			{}

			CX_GLVersion(int maj, int min, int rel) :
				major(maj),
				minor(min),
				release(rel)
			{}

			int major;
			int minor;
			int release;

		};

		CX_GLVersion getOpenGLVersion(void);
		CX_GLVersion getGLSLVersion(void);
		bool glFenceSyncSupported(void);
		bool glVersionAtLeast(int desiredMajor, int desiredMinor, int desiredRelease = 0);
		int glCompareVersions(CX_GLVersion a, CX_GLVersion b);

		CX_GLVersion getGLSLVersionFromGLVersion(CX_GLVersion glVersion);

	}
}