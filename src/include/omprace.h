
extern void omprace_init();
extern void omprace_fini();
extern void omprace_section_dispatch_begin();
extern void omprace_section_dispatch_end();
extern "C" void __omprace_instrument_access__(void*, int);
