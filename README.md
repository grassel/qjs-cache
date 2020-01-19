# qjs-cache: PoC for file cache for QuickJS bytecode.

QuicJS operates internally on a bytecode. Before QJS can execute JS it converts it internally into bytecode.
The tool '''qjsc''', which is included in in QJS releases, can dump this bytecode to a C buffer.
It's been shown already that initial JS execution speed can be increased by using such a bytecode buffer (compared to a buffer holding textual JS).

```qjs-debug.c``` shows how to write the bytecode to a file, how to read it back into memory and how to start JS execution thereafter.  A way how to use this code is the following. Step 1: Attempt to read bytecode from cache first using ```read_js_from_cache```. If not cache file exists (cold load) this function return ```JS_UNDEFINED```. In this case, read the JS source file with ```read_js_compile_and_cache_file```. This function will return the JS Code as a ```JSValue```. Step 2: execute the JS. See ```main()```.
