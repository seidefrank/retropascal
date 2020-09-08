// lib wrapper around p-code interpreter
// Copyright (C) 2013 Frank Seide

int/*bool*/ InitPCode(size_t numDisks, char diskModes[], const char * diskPaths[], const char * batchInput);
void InterruptPCode();
int RunPCodeUntilInterrupt();
void FinishPCode();

int SuspendCPU(void * buf, size_t bufsize);
int RestoreCPU(const void * buf, size_t savedsize);
