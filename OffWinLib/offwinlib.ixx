/*
* Uses Windows Implementation Libraries (WIL).
*	WIL has a dependency on "strsafe.h", which has compatability issues with C++ modules due to the use of static inline functions.
*		As a workaround, "STRSAFE_LIB" is defined for the project, which causes "strsafe.h" to link to "strsafe.lib" instead of using the inlined functions.
*		The project links to "legacy_stdio_definitions.lib", which is required by "strsafe.lib".
*	In release configuration, "RESULT_DIAGNOSTICS_LEVEL" is set to 0 to avoid embedded debug information in the compiled library.
*/

export module offwinlib;

export import :data_conversion;
export import :dll_injection;
export import :junction;
export import :misc;
export import :object_manager;
export import :oplock;
export import :registry;
