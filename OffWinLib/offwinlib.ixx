/*
* Uses Windows Implementation Libraries (WIL).
*	In release configuration, "RESULT_DIAGNOSTICS_LEVEL" is set to 0 to avoid embedded debug information in the compiled library.
*/

export module offwinlib;

export import :data_conversion;
export import :injection;
export import :junction;
export import :memory;
export import :misc;
export import :object_manager;
export import :oplock;
export import :registry;
export import :syscall;
