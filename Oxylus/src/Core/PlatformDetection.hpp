// Platform detection using predefined macros
#pragma once
#ifdef _WIN32
/* Windows x64/x86 */
#ifdef _WIN64
/* Windows x64  */
#define OX_PLATFORM_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#else
		/* Windows x86 */
		#error "x86 Builds are not supported!"
#endif
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	/* TARGET_OS_MAC exists on all the platforms
	 * so we must check all of them (in this order)
	 * to ensure that we're running on MAC
	 * and not some other Apple platform */
#if TARGET_IPHONE_SIMULATOR == 1
		#error "IOS simulator is not supported!"
#elif TARGET_OS_IPHONE == 1
		#define OX_PLATFORM_IOS
		#define VK_USE_PLATFORM_IOS_MVK
		#error "IOS is not supported!"
#elif TARGET_OS_MAC == 1
		#define OX_PLATFORM_MACOS
		#define VK_USE_PLATFORM_MACOS_MVK
#else
		#error "Unknown Apple platform!"
#endif
/* We also have to check __ANDROID__ before __linux__
 * since android is based on the linux kernel
 * it has __linux__ defined */
#elif defined(__ANDROID__)
	#define OX_PLATFORM_ANDROID
	#define VK_USE_PLATFORM_ANDROID_KHR
	#error "Android is not supported!"
#elif defined(__linux__)
	#define OX_PLATFORM_LINUX
	#define VK_USE_PLATFORM_XCB_KHR
#else
	/* Unknown compiler/platform */
	#error "Unknown platform!"
#endif // End of platform detection
